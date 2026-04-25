#include <assert.h>
#include <ctype.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// ---------------------------------------------------------------------------------------------------
//      SECT ERRORS
// ---------------------------------------------------------------------------------------------------

#define JIK_ERR_PREFIX              "Jik runtime error: "

#ifdef JIK_UNSAFE_NO_BOUNDS_CHECKS
#define JIK_VEC_BOUNDS_CHECK(v, idx, dbg_info) ((void)0)
#else
#define JIK_VEC_BOUNDS_CHECK(v, idx, dbg_info)                                                   \
    jik_die_if((idx) >= (v)->size, "vector index out of bounds\n  --> %s", (dbg_info))
#endif


void
jik_die(char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    fprintf(stderr, "%s", JIK_ERR_PREFIX);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

void
jik_die_if(bool cond, char *msg, ...)
{
    if (cond) {
        va_list args;
        va_start(args, msg);
        fprintf(stderr, "%s", JIK_ERR_PREFIX);
        vfprintf(stderr, msg, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(EXIT_FAILURE);
    }
}

// ---------------------------------------------------------------------------------------------------
//      SECT ASSERT
// ---------------------------------------------------------------------------------------------------

void
jik_assert(bool cond, const char *filepath, int32_t line)
{
    if (cond) {
        return;
    }
    if (filepath == NULL) {
        filepath = "<unknown>";
    }
    fprintf(stderr, "jik: assertion failed at %s:%" PRId32 "\n", filepath, line);
    fflush(stderr);
    abort();
}


// ---------------------------------------------------------------------------------------------------
//      SECT MEMORY ALLOCATION
// ---------------------------------------------------------------------------------------------------

void *
jik_malloc(size_t n)
{
    void *ptr = malloc(n);
    jik_die_if(ptr == NULL, "jik_malloc");
    return ptr;
}

void *
jik_calloc(size_t n, size_t size)
{
    void *ptr = calloc(n, size);
    jik_die_if(ptr == NULL, "jik_calloc");
    return ptr;
}

void *
jik_realloc(void *ptr, size_t n)
{
    if (n == 0) {
        free(ptr);
        return NULL;
    }
    void *p = realloc(ptr, n);
    jik_die_if(p == NULL, "jik_realloc");
    return p;
}

// ---------------------------------------------------------------------------------------------------
//      REGION ALLOCATOR
// ---------------------------------------------------------------------------------------------------

// -------------------------------------------------------------
// Configuration
// -------------------------------------------------------------

#ifndef REGION_DEFAULT_BLOCK_SIZE
#define REGION_DEFAULT_BLOCK_SIZE (64 * 1024) // 64KB default block size
#endif

// -------------------------------------------------------------
// Internal block structure
// -------------------------------------------------------------

typedef struct JikRegionBlock {
    struct JikRegionBlock *next;
    size_t                capacity; // total bytes in data[]
    size_t                used;     // bytes already allocated
    unsigned char         data[];   // flexible array of raw bytes
} JikRegionBlock;

// -------------------------------------------------------------
// JikRegion handle
// -------------------------------------------------------------

typedef struct JikRegion {
    JikRegionBlock *blocks;     // head of block list (for freeing)
    JikRegionBlock *current;    // current block for bump allocation
    size_t         block_size; // default block size for new blocks
    size_t         total_mem;  // total capacity of all allocated blocks
} JikRegion;

// -------------------------------------------------------------
// Helpers
// -------------------------------------------------------------

static inline size_t
jik_region_align_up(size_t n, size_t align)
{
    return (n + align - 1) & ~(align - 1);
}

static JikRegionBlock *
jik_region_new_block(size_t capacity)
{
    size_t         total = sizeof(JikRegionBlock) + capacity;
    JikRegionBlock *blk   = (JikRegionBlock *)jik_malloc(total);
    blk->next            = NULL;
    blk->capacity        = capacity;
    blk->used            = 0;
    return blk;
}

// -------------------------------------------------------------
// Public API
// -------------------------------------------------------------

// Allocate a new jik region on the heap.
// If block_size == 0, REGION_DEFAULT_BLOCK_SIZE is used.
JikRegion *
jik_region_new(size_t block_size)
{
    JikRegion *a   = jik_malloc(sizeof(JikRegion));
    a->blocks     = NULL;
    a->current    = NULL;
    a->block_size = (block_size == 0) ? REGION_DEFAULT_BLOCK_SIZE : block_size;
    a->total_mem  = 0;
    return a;
}

// Destroy a heap-allocated jik region and all of its blocks.
static inline void
jik_region_free(JikRegion *r)
{
    JikRegionBlock *blk = r->blocks;
    while (blk) {
        JikRegionBlock *next = blk->next;
        free(blk);
        blk = next;
    }

    r->blocks    = NULL;
    r->current   = NULL;
    r->total_mem = 0;
    free(r);
}

// Core allocator: allocate `size` bytes from jik_region.
// Returns memory suitable for current Jik runtime objects on the default
// supported targets. Full max_align_t portability for all host ABIs and
// FFI-defined C types is not yet guaranteed.
static inline void *
jik_region_alloc(JikRegion *r, size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size_t align = alignof(max_align_t);
    size         = jik_region_align_up(size, align);

    JikRegionBlock *blk = r->current;

    // Need a new block?
    if (!blk || blk->used + size > blk->capacity) {
        size_t cap = r->block_size;
        // For very large allocations, make a dedicated big block.
        if (size > cap) {
            cap = size;
        }

        JikRegionBlock *new_blk = jik_region_new_block(cap);
        r->total_mem += cap;
        // Push onto list head
        new_blk->next = r->blocks;
        r->blocks     = new_blk;
        r->current    = new_blk;
        blk           = new_blk;
    }

    void *ptr = blk->data + blk->used;
    blk->used += size;
    return ptr;
}

// Allocate and zero-initialize `count * size` bytes from jik_region.
static inline void *
jik_region_calloc(JikRegion *r, size_t count, size_t size)
{
    size_t total = count * size;
    void  *ptr   = jik_region_alloc(r, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

// Convenience "realloc" for jik_regions.
// NOTE:
//   - This NEVER frees `old_ptr`; the old memory remains as garbage in the jik_region.
//   - Caller must pass the old_size; jik_regions do not track per-allocation sizes.
//   - Semantics are:
//       * if new_size <= old_size: returns old_ptr (no move).
//       * else: allocate new block, copy old_size bytes, return new pointer.
static inline void *
jik_region_realloc(JikRegion *r, void *old_ptr, size_t old_size, size_t new_size)
{
    if (new_size == 0) {
        // In jik_region semantics, "free" doesn't exist; the old allocation remains dead data.
        return NULL;
    }

    if (!old_ptr) {
        // Behave like malloc
        return jik_region_alloc(r, new_size);
    }

    if (new_size <= old_size) {
        // Shrinking: keep old ptr, do nothing (optional behavior).
        return old_ptr;
    }

    // Growing: allocate new space, copy old_size bytes, leave old in place.
    void *new_ptr = jik_region_alloc(r, new_size);
    memcpy(new_ptr, old_ptr, old_size);
    return new_ptr;
}

// ---------------------------------------------------------------------------------------------------
//      STRING
// ---------------------------------------------------------------------------------------------------

#define JIK_STRING_TYPE_NAME JikString

typedef struct JIK_STRING_TYPE_NAME {
    char     *data;
    size_t    size;
    size_t    capacity;
    JikRegion *region;
} JIK_STRING_TYPE_NAME;

JikString *
jik_string_new(char *from, JikRegion *a)
{
    size_t     n = strlen(from);
    JikString *s = jik_region_alloc(a, sizeof(JikString));
    s->data      = jik_region_alloc(a, n + 1);
    memcpy(s->data, from, n);
    s->data[n]  = '\0';
    s->size     = n;
    s->capacity = n;
    s->region    = a;
    return s;
}

int
jik_string_cmp(JikString *s1, JikString *s2)
{
    return strcmp(s1->data, s2->data);
}

char
jik_string_get(JikString *s, size_t idx, char *dbg_info)
{
    jik_die_if(idx >= s->size, "string index out of bounds\n  --> %s", dbg_info);
    return s->data[idx];
}


// ---------------------------------------------------------------------------------------------------
//      BUILTIN ERRORS
// ---------------------------------------------------------------------------------------------------

#define MAX_ERR_MSG 256

typedef struct JikError {
    int        code;
    char msg[MAX_ERR_MSG];
} JikError;

bool
jik_error_failed(JikError *e)
{
    return e->code != 0;
}

void
jik_error_set(JikError *e, int code, char *msg)
{
    assert(code > 0);
    e->code = code;
    size_t n = strlen(msg);
    if (n >= MAX_ERR_MSG) {
        n = MAX_ERR_MSG - 1;
    }
    memcpy(e->msg, msg, n);
    e->msg[n] = '\0';
}

void
jik_error_clear(JikError *e)
{
    e->code = 0;
    e->msg[0] = '\0';
}

JikString *
jik_error_msg(JikError *e, JikRegion *r)
{
    return jik_string_new(e->msg, r);
}

int32_t
jik_error_code(JikError *e)
{
    return (int32_t)e->code;
}

void
jik_panic_on_error(JikError *e, char *msg)
{
    jik_die_if(jik_error_failed(e), "%s\n%s", msg, e->msg);
}


// ---------------------------------------------------------------------------------------------------
//      BUILTIN DEBUG UTIL
// ---------------------------------------------------------------------------------------------------

typedef struct JikSite {
    char *filepath;
    char *codeline;
    int lineno;
} JikSite;

JikSite
jik_site_new(char *filepath, char *codeline, int lineno)
{
    JikSite s = {0};
    s.filepath = filepath;
    s.codeline = codeline;
    s.lineno   = lineno;
    return s;
}

JikString *
jik_site_file(JikSite s, JikRegion *r)
{
    return jik_string_new(s.filepath, r);
}

int
jik_site_line(JikSite s)
{
    return s.lineno;
}

JikString *
jik_site_code(JikSite s, JikRegion *r)
{
    return jik_string_new(s.codeline, r);
}


// ---------------------------------------------------------------------------------------------------
//      STRING PRINTING UTILS
// ---------------------------------------------------------------------------------------------------

typedef struct JikCharBuffer {
    char  *data;
    size_t size;
    size_t capacity;
} JikCharBuffer;

JikCharBuffer *
jik_char_buffer_new(const char *from, JikRegion *a)
{
    JikCharBuffer *b = jik_region_alloc(a, sizeof(JikCharBuffer));
    b->size          = strlen(from);
    b->capacity      = b->size;
    b->data          = jik_region_alloc(a, b->size + 1);
    memcpy(b->data, from, b->size + 1);
    b->data[b->size] = '\0';
    return b;
}

void
jik_char_buffer_append(JikCharBuffer *b, const char *str, JikRegion *a)
{
    size_t n       = strlen(str);
    size_t req_cap = b->size + n + 1;
    if (req_cap > b->capacity) {
        size_t old_cap = b->capacity;
        while (req_cap > b->capacity) {
            b->capacity = 2 * b->capacity + 1;
        }
        b->data = jik_region_realloc(a, b->data, old_cap, b->capacity);
    }
    memcpy(b->data + b->size, str, n);
    b->size += n;
    b->data[b->size] = '\0';
}

JikString *
jik_int_tostr(int value, JikRegion *a)
{
    char buf[32];
    int  len = snprintf(buf, sizeof(buf), "%d", value);
    jik_die_if(len < 0, "str repr error");
    return jik_string_new(buf, a);
}

JikString *
jik_double_tostr(double value, JikRegion *a)
{
    char buf[64];
    int  len = snprintf(buf, sizeof(buf), "%.17g", value);
    jik_die_if(len < 0, "dbl repr error");
    return jik_string_new(buf, a);
}

JikString *
jik_bool_tostr(bool value, JikRegion *a)
{
    char *src = value ? "true" : "false";
    return jik_string_new(src, a);
}

JikString *
jik_string_tostr(JikString *value, JikRegion *a)
{
    return value;
}

JikString *
jik_char_tostr(char value, JikRegion *a)
{
    char buf[2] = {value, '\0'};
    return jik_string_new(buf, a);
}

void
jik_print(JikString **strings, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        printf("%s", strings[i]->data);
    }
}

void
jik_println(JikString **strings, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        printf("%s", strings[i]->data);
    }
    printf("\n");
}

JikString *
jik_concat(JikString **strings, size_t n, JikRegion *a)
{
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += strings[i]->size;
    }

    JikString *res = jik_region_alloc(a, sizeof(JikString));
    res->data      = jik_region_alloc(a, total + 1);
    res->size      = total;
    res->capacity  = total;
    res->region    = a;

    size_t off = 0;
    for (size_t i = 0; i < n; i++) {
        memcpy(res->data + off, strings[i]->data, strings[i]->size);
        off += strings[i]->size;
    }
    res->data[total] = '\0';
    return res;
}


// ---------------------------------------------------------------------------------------------------
//      SECT STRUCT CREATOR
// ---------------------------------------------------------------------------------------------------

#define JIK_DECLARE_STRUCT_NEW(struct_name)                                                        \
    struct struct_name *struct_name##_new(JikRegion *a, struct struct_name *init)

#define JIK_DECLARE_STRUCT_TOSTR(struct_name)                                                      \
    JikString *struct_name##_tostr(struct struct_name *s, JikRegion *a)

#define JIK_DEFINE_STRUCT_NEW(struct_name)                                                    \
    struct struct_name *struct_name##_new(JikRegion *a, struct struct_name *init)  \
    {                                                                                              \
        struct struct_name *p =                                                                    \
            (struct struct_name *)jik_region_alloc(a, sizeof(struct struct_name));                  \
        memcpy(p, init, sizeof(struct struct_name));                                               \
        p->region = a;                                                                              \
        return p;                                                                                  \
    }

// for variants
#define JIK_DEFINE_VARIANT_ACCESS(struct_name, tag_name)                                           \
    static inline struct struct_name *struct_name##_access(                                        \
        struct struct_name *s, enum tag_name t, char *err_msg)                                     \
    {                                                                                              \
        jik_die_if(s->tag != t, "%s", err_msg);                                                    \
        return s;                                                                                  \
    }

// ---------------------------------------------------------------------------------------------------
//      SECT VECTOR
// ---------------------------------------------------------------------------------------------------

#define JIK_DECLARE_VEC(vec_name, vec_elem_type)                                                   \
    struct vec_name;                                                                               \
    struct vec_name *vec_name##_new(JikRegion *a, size_t n);                       \
    struct vec_name *vec_name##_new_from(                                                          \
        JikRegion *a, size_t n, vec_elem_type init_vals[]);                        \
    void          vec_name##_clear(struct vec_name *v);                                            \
    vec_elem_type vec_name##_get(struct vec_name *v, size_t idx, char *dbg_info);                  \
    void vec_name##_set(struct vec_name *v, size_t idx, vec_elem_type item, char *dbg_info);       \
    void vec_name##_push(struct vec_name *v, vec_elem_type item, char *dbg_info); \
    void vec_name##_extend(                                                                        \
        struct vec_name *v, struct vec_name *b, char *dbg_info);                  \
    vec_elem_type vec_name##_pop(struct vec_name *v, char *dbg_info);                              \
    JikString    *vec_name##_tostr(struct vec_name *v, JikRegion *a);                               \
    size_t        vec_name##_size(struct vec_name *v, char *dbg_info);

#define JIK_DEFINE_VEC(vec_name, vec_elem_type)                                                    \
    struct vec_name {                                                                              \
        vec_elem_type *data;                                                                       \
        size_t         size;                                                                       \
        size_t         capacity;                                                                   \
        JikRegion      *region;                                                                      \
    };                                                                                             \
                                                                                                   \
    struct vec_name *vec_name##_new(JikRegion *a, size_t n)                        \
    {                                                                                              \
        struct vec_name *v = jik_region_alloc(a, sizeof(struct vec_name));                          \
        v->data            = jik_region_alloc(a, n * sizeof(vec_elem_type));                        \
        v->size            = n;                                                                    \
        v->capacity        = n;                                                                    \
        v->region           = a;                                                                    \
        return v;                                                                                  \
    }                                                                                              \
                                                                                                   \
    struct vec_name *vec_name##_new_from(                                                          \
        JikRegion *a, size_t n, vec_elem_type init_vals[])                         \
    {                                                                                              \
        struct vec_name *v = jik_region_alloc(a, sizeof(struct vec_name));                          \
        v->data            = jik_region_alloc(a, n * sizeof(vec_elem_type));                        \
        v->size            = n;                                                                    \
        v->capacity        = n;                                                                    \
        for (size_t i = 0; i < n; i++) {                                                           \
            v->data[i] = init_vals[i];                                                             \
        }                                                                                          \
        v->region = a;                                                                              \
        return v;                                                                                  \
    }                                                                                              \
                                                                                                   \
    void vec_name##_clear(struct vec_name *v) { v->size = 0; }                                     \
                                                                                                   \
    vec_elem_type vec_name##_get(struct vec_name *v, size_t idx, char *dbg_info)                   \
    {                                                                                              \
        JIK_VEC_BOUNDS_CHECK(v, idx, dbg_info);                                                    \
        return v->data[idx];                                                                       \
    }                                                                                              \
                                                                                                   \
    void vec_name##_set(struct vec_name *v, size_t idx, vec_elem_type item, char *dbg_info)        \
    {                                                                                              \
        JIK_VEC_BOUNDS_CHECK(v, idx, dbg_info);                                                    \
        v->data[idx] = item;                                                                       \
    }                                                                                              \
                                                                                                   \
    void vec_name##_push(struct vec_name *v, vec_elem_type item, char *dbg_info)  \
    {                                                                                              \
        if (v->size == v->capacity) {                                                              \
            size_t new_cap = 2 * v->capacity + 1;                                                  \
            v->data        = jik_region_realloc(v->region,                                           \
                                        v->data,                                            \
                                        v->capacity * sizeof(vec_elem_type),                \
                                        new_cap * sizeof(vec_elem_type));                   \
            v->capacity    = new_cap;                                                              \
        }                                                                                          \
        v->data[v->size] = item;                                                                   \
        v->size++;                                                                                 \
    }                                                                                              \
                                                                                                   \
    void vec_name##_extend(                                                                        \
        struct vec_name *v, struct vec_name *b, char *dbg_info)                   \
    {                                                                                              \
        if (b->size == 0) {                                                                        \
            return;                                                                                \
        }                                                                                          \
        size_t new_size = v->size + b->size;                                                       \
        if (new_size > v->capacity) {                                                              \
            size_t new_cap = v->capacity;                                                          \
            while (new_cap < new_size) {                                                           \
                new_cap = 2 * new_cap + 1;                                                         \
            }                                                                                      \
            v->data     = jik_region_realloc(v->region,                                              \
                                        v->data,                                               \
                                        v->capacity * sizeof(vec_elem_type),                   \
                                        new_cap * sizeof(vec_elem_type));                      \
            v->capacity = new_cap;                                                                 \
        }                                                                                          \
        for (size_t i = 0; i < b->size; i++)                                                       \
            v->data[v->size + i] = b->data[i];                                                     \
        v->size = new_size;                                                                        \
    }                                                                                              \
                                                                                                   \
    vec_elem_type vec_name##_pop(struct vec_name *v, char *dbg_info)                               \
    {                                                                                              \
        jik_die_if(v->size == 0, "cannot pop from empty vector\n  --> %s", dbg_info);              \
        v->size--;                                                                                 \
        return v->data[v->size];                                                                   \
    }                                                                                              \
                                                                                                   \
    size_t vec_name##_size(struct vec_name *v, char *dbg_info) { return v->size; }

// TODO: in the long term, we should get rid if this - its a GNU statement expression and not standard C
#define JIK_MAKE_VEC(region, vec_name, init_size, initializer)                                 \
    ({                                                                                             \
        struct vec_name *__v = vec_name##_new((region), (init_size));                        \
        for (size_t __i = 0; __i < (init_size); __i++) {                                           \
            __v->data[__i] = initializer;                                                          \
        }                                                                                          \
        __v;                                                                                       \
    })

// ---------------------------------------------------------------------------------------------------
//      SECT DICT
// ---------------------------------------------------------------------------------------------------

#define JIK_FNV_OFFSET_BASIS   2166136261u
#define JIK_FNV_PRIME          16777619u
#define JIK_HTAB_INIT_CAPACITY 16
#define JIK_HTAB_LOAD_FACT     0.75

#define JIK_DECLARE_OPTION(option_name, option_elem_type)                                           \
    struct option_name;                                                                             \
    struct option_name *option_name##_some(option_elem_type val, JikRegion *a);                     \
    struct option_name *option_name##_none(JikRegion *a);                                           \
    bool option_name##_is_some(struct option_name *opt);                                            \
    bool option_name##_is_none(struct option_name *opt);                                            \
    option_elem_type option_name##_unwrap(struct option_name *opt, char *dbg_info);                 \
    JikString *option_name##_tostr(struct option_name *opt, JikRegion *a);

#define JIK_DEFINE_OPTION(option_name, option_elem_type)                                             \
    struct option_name {                                                                            \
        bool             is_some;                                                                   \
        option_elem_type val;                                                                       \
        JikRegion       *region;                                                                    \
    };                                                                                              \
                                                                                                    \
    struct option_name *option_name##_some(option_elem_type val, JikRegion *a)                      \
    {                                                                                               \
        struct option_name *opt = jik_region_alloc(a, sizeof(struct option_name));                  \
        opt->is_some            = true;                                                             \
        opt->val                = val;                                                              \
        opt->region             = a;                                                                \
        return opt;                                                                                 \
    }                                                                                               \
                                                                                                    \
    struct option_name *option_name##_none(JikRegion *a)                                            \
    {                                                                                               \
        struct option_name *opt = jik_region_alloc(a, sizeof(struct option_name));                  \
        opt->is_some            = false;                                                            \
        opt->region             = a;                                                                \
        return opt;                                                                                 \
    }                                                                                               \
                                                                                                    \
    bool option_name##_is_some(struct option_name *opt) { return opt && opt->is_some; }            \
    bool option_name##_is_none(struct option_name *opt) { return !option_name##_is_some(opt); }    \
                                                                                                    \
    option_elem_type option_name##_unwrap(struct option_name *opt, char *dbg_info)                  \
    {                                                                                               \
        jik_die_if(!option_name##_is_some(opt), "illegal unwrap of None\n  --> %s", dbg_info);      \
        return opt->val;                                                                            \
    }

#define JIK_DECLARE_DICT(dict_name, dict_elem_type, dict_option_type)                               \
    struct dict_name;                                                                              \
    struct dict_name##_item;                                                                       \
    uint32_t                 dict_name##_hash_key(char *key);                                      \
    struct dict_name##_item *dict_name##_find_entry(                                               \
        struct dict_name##_item *items, size_t capacity, char *key);                               \
    void              dict_name##_resize(struct dict_name *ht, size_t capacity);  \
    struct dict_name *dict_name##_new(JikRegion *a);                               \
    struct dict_name *dict_name##_new_from(                                                        \
        JikRegion *a, struct dict_name##_item *init, size_t n, char *dbg_info);    \
    void                     dict_name##_clear(struct dict_name *ht);                              \
    void                     dict_name##_set(                              \
                         struct dict_name *ht,                                 \
                         JikString        *key,                                \
                         dict_elem_type    val,                                \
                         char             *dbg_info);                                      \
    struct dict_option_type *dict_name##_get(                                                      \
        struct dict_name *ht, JikString *key, char *dbg_info);                                     \
    JikString     *dict_name##_tostr(struct dict_name *ht, JikRegion *a);                           \
    size_t         dict_name##_size(struct dict_name *ht, char *dbg_info);

#define JIK_DEFINE_DICT(dict_name, dict_elem_type, dict_option_type)                               \
    struct dict_name##_item {                                                                      \
        bool           ok;                                                                         \
        JikString     *key;                                                                        \
        dict_elem_type val;                                                                        \
    };                                                                                             \
                                                                                                   \
    struct dict_name {                                                                             \
        size_t                   capacity;                                                         \
        size_t                   size;                                                             \
        struct dict_name##_item *items;                                                            \
        JikRegion                *region;                                                            \
    };                                                                                             \
                                                                                                   \
    uint32_t dict_name##_hash_key(char *key)                                                       \
    {                                                                                              \
        uint32_t hash = JIK_FNV_OFFSET_BASIS;                                                      \
        for (const char *p = key; *p; p++) {                                                       \
            hash ^= (uint8_t)(unsigned char)(*p);                                                  \
            hash *= JIK_FNV_PRIME;                                                                 \
        }                                                                                          \
        return hash;                                                                               \
    }                                                                                              \
                                                                                                   \
    struct dict_name##_item *dict_name##_find_entry(                                               \
        struct dict_name##_item *items, size_t capacity, char *key)                                \
    {                                                                                              \
        size_t                   idx = (size_t)(dict_name##_hash_key(key) % capacity);             \
        struct dict_name##_item *item;                                                             \
        for (;;) {                                                                                 \
            item = &items[idx];                                                                    \
            if (item->key == NULL || strcmp(item->key->data, key) == 0)                            \
                return item;                                                                       \
            idx = (idx + 1) % capacity;                                                            \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void dict_name##_resize(struct dict_name *ht, size_t capacity)                \
    {                                                                                              \
        struct dict_name##_item *new_items =                                                       \
            jik_region_calloc(ht->region, capacity, sizeof(struct dict_name##_item));                \
        struct dict_name##_item *item;                                                             \
        for (size_t i = 0; i < ht->capacity; i++) {                                                \
            item = &(ht->items[i]);                                                                \
            if (item->key == NULL)                                                                 \
                continue;                                                                          \
            struct dict_name##_item *dest =                                                        \
                dict_name##_find_entry(new_items, capacity, item->key->data);                      \
            dest->ok  = item->ok;                                                                  \
            dest->key = item->key;                                                                 \
            dest->val = item->val;                                                                 \
        }                                                                                          \
        ht->items    = new_items;                                                                  \
        ht->capacity = capacity;                                                                   \
        return;                                                                                    \
    }                                                                                              \
                                                                                                   \
    struct dict_name *dict_name##_new(JikRegion *a)                                \
    {                                                                                              \
        struct dict_name *ht = jik_region_alloc(a, sizeof(struct dict_name));                       \
        ht->capacity         = JIK_HTAB_INIT_CAPACITY;                                             \
        ht->size             = 0;                                                                  \
        ht->items = jik_region_calloc(a, JIK_HTAB_INIT_CAPACITY, sizeof(struct dict_name##_item));  \
        ht->region = a;                                                                             \
        return ht;                                                                                 \
    }                                                                                              \
                                                                                                   \
    struct dict_name *dict_name##_new_from(                                                        \
        JikRegion *a, struct dict_name##_item *init, size_t n, char *dbg_info)     \
    {                                                                                              \
        struct dict_name *ht = dict_name##_new(a);                                            \
        for (size_t i = 0; i < n; i++) {                                                           \
            dict_name##_set(ht, init[i].key, init[i].val, dbg_info);                          \
        }                                                                                          \
        return ht;                                                                                 \
    }                                                                                              \
                                                                                                   \
    void dict_name##_clear(struct dict_name *ht)                                                   \
    {                                                                                              \
        ht->size = 0;                                                                              \
        for (size_t i = 0; i < ht->capacity; i++) {                                                \
            ht->items[i].key = NULL;                                                               \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void dict_name##_set(                                                                          \
        struct dict_name *ht, JikString *key, dict_elem_type val, char *dbg_info) \
    {                                                                                              \
        if (ht->size + 1 > ht->capacity * JIK_HTAB_LOAD_FACT) {                                    \
            size_t new_cap = ht->capacity * 2;                                                     \
            dict_name##_resize(ht, new_cap);                                                  \
        }                                                                                          \
        struct dict_name##_item *item =                                                            \
            dict_name##_find_entry(ht->items, ht->capacity, key->data);                            \
        if (item->key == NULL)                                                                     \
            ht->size++;                                                                            \
        item->ok  = true;                                                                          \
        item->key = jik_string_new(key->data, ht->region);                                          \
        item->val = val;                                                                           \
        return;                                                                                    \
    }                                                                                              \
                                                                                                   \
    struct dict_option_type *dict_name##_get(struct dict_name *ht, JikString *key, char *dbg_info) \
    {                                                                                              \
        struct dict_name##_item *item =                                                            \
            dict_name##_find_entry(ht->items, ht->capacity, key->data);                            \
        if (item && item->ok) {                                                                    \
            return dict_option_type##_some(item->val, ht->region);                                 \
        }                                                                                          \
        return dict_option_type##_none(ht->region);                                                \
    }                                                                                              \
                                                                                                   \
    size_t dict_name##_size(struct dict_name *ht, char *dbg_info) { return ht->size; }


// ---------------------------------------------------------------------------------------------------
//      SECT HELPERS
// ---------------------------------------------------------------------------------------------------

#define MAKE_ARG_VEC(argc, argv, region)                                                       \
    struct vec_JikString *args = vec_JikString_new(region, argc);                              \
    for (size_t i = 0; i < argc; i++) {                                                            \
        vec_JikString_set(args, i, jik_string_new(argv[i], region), NULL);                          \
    }
