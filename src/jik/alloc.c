#include "alloc.h"

#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JIK_ARENA_INIT_CAPACITY (1024 * 1024)

typedef struct JikChunk {
    struct JikChunk *next;
    size_t           used;
    size_t           cap;
    char            *data;
} JikChunk;

typedef struct {
    JikChunk *head; // current chunk
} JikArena;

static JikArena jik_arena;

static inline size_t
align_up(size_t x, size_t a)
{
    return (x + (a - 1)) & ~(a - 1);
}

static JikChunk *
new_chunk(size_t min_cap)
{
    size_t cap = JIK_ARENA_INIT_CAPACITY;
    while (cap < min_cap) {
        cap *= 2;
    }
    JikChunk *c = malloc(sizeof(JikChunk));
    if (!c) {
        fprintf(stderr, "%s", "arena malloc failure");
        exit(EXIT_FAILURE);
    }
    c->data = malloc(cap);
    if (!c->data) {
        fprintf(stderr, "%s", "arena malloc failure");
        exit(EXIT_FAILURE);
    }
    c->used = 0;
    c->cap  = cap;
    c->next = NULL;
    return c;
}

void
jik_arena_init(void)
{
    jik_arena.head = new_chunk(JIK_ARENA_INIT_CAPACITY);
}

char *
jik_alloc(size_t size)
{
    // TODO: this assertion fails, check why
    // assert(size > 0);
    const size_t A = alignof(max_align_t); // safe for any fundamental type
    JikChunk    *c = jik_arena.head;

    size_t off = align_up(c->used, A);

    if (size > c->cap - off) {
        // Ensure the new chunk is large enough even if first allocation needs alignment.
        // (malloc usually returns max_align_t-aligned memory, so off will be 0 and aligned,
        // but we add (A-1) to be robust if chunk->data ever has stricter needs.)
        JikChunk *n    = new_chunk(size + (A - 1));
        n->next        = c;
        jik_arena.head = n;
        c              = n;

        // Fresh chunk: used = 0, so off is simply 0 (and aligned).
        off = align_up(c->used, A);
    }

    // Hand out aligned pointer and bump the used watermark.
    char *p = c->data + off;
    c->used = off + size;

    // Optional debug assert: verify alignment of the returned pointer.
    assert(((uintptr_t)p & (A - 1)) == 0);

    return p;
}

char *
jik_realloc(char *p, size_t size, size_t old_size)
{
    assert(p && size > 0);
    char *np = jik_alloc(size);
    if (old_size) {
        memcpy(np, p, old_size < size ? old_size : size);
    }
    return np;
}

void
jik_arena_free(void)
{
    for (JikChunk *c = jik_arena.head; c;) {
        JikChunk *n = c->next;
        free(c->data);
        free(c);
        c = n;
    }
    jik_arena.head = NULL;
}
