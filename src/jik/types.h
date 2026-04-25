#ifndef JIK_TYPES_H
#define JIK_TYPES_H

#include "common.h"
#include "htab.h"

#define C_TYPE_NAME_VOID    "void"
#define C_TYPE_NAME_INTEGER "int32_t"
#define C_TYPE_NAME_FLOAT   "double"
#define C_TYPE_NAME_BOOL    "bool"
#define C_TYPE_NAME_CHAR    "uint8_t"
#define C_TYPE_NAME_STRING  "JikString *"
#define C_TYPE_NAME_REGION  "JikRegion *"
#define C_TYPE_NAME_SITE    "JikSite"

#define TYPE_NAMES                                                                                 \
    X(TYPE_NOTYPE)                                                                                 \
    X(TYPE_UNKNOWN)                                                                                \
    X(TYPE_INTEGER)                                                                                \
    X(TYPE_FLOAT)                                                                                  \
    X(TYPE_STRING)                                                                                 \
    X(TYPE_CHAR)                                                                                   \
    X(TYPE_BOOL)                                                                                   \
    X(TYPE_VOID)                                                                                   \
    X(TYPE_FUNCTION)                                                                               \
    X(TYPE_STRUCT)                                                                                 \
    X(TYPE_VECTOR)                                                                                 \
    X(TYPE_DICT)                                                                                   \
    X(TYPE_OPTION)                                                                                 \
    X(TYPE_VARIANT)                                                                                \
    X(TYPE_REGION)                                                                                 \
    X(TYPE_SITE)                                                                                   \
    X(TYPE_ENUM)

typedef enum JikTypeName {
#define X(name) name,
    TYPE_NAMES
#undef X
        NUM_TYPES
} JikTypeName;

static const char *TYPE_STRINGS[NUM_TYPES] = {
#define X(name) [name] = #name,
    TYPE_NAMES
#undef X
};

JIK_HTAB_DECLARE(TabJikType, struct JikType *);
JIK_VEC_DECLARE(VecJikType, struct JikType *);

typedef struct JikType {
    JikTypeName name;
    char       *C_name;
    // TODO: this also is used for struct name, so a bit misleading
    char *mangled_name;
    union {
        // Function
        struct {
            int             num_params;
            struct JikType *ret_type;
            VecJikType     *param_types;
            bool            builtin;
        } val_func;

        struct {
            char       *name;
            TabJikType *field_types;
            // TODO: this stuff with metadata for proxy structs for dicts and variants is not so
            // nice
            struct JikType *variant_enum_type;
            bool            visiting;
        } val_struct;

        struct {
            struct JikType *struct_type;
        } val_this;

        struct {
            struct JikType *elem_type;
        } val_vec;

        struct {
            struct JikType *elem_type;
        } val_dict;

        struct {
            struct JikType *elem_type;
        } val_option;

        struct {
            char *name;
            // TabBool *variants;
        } val_enum;

        struct {
            char           *name;
            TabJikType     *variant_types;
            struct JikType *enum_type;
            bool            visiting;
        } val_variant;
    };
} JikType;

extern JikType JIK_TYPE_INT;
extern JikType JIK_TYPE_FLOAT;
extern JikType JIK_TYPE_BOOL;
extern JikType JIK_TYPE_STRING;
extern JikType JIK_TYPE_VOID;
extern JikType JIK_TYPE_CHAR;
extern JikType JIK_TYPE_REGION;
extern JikType JIK_TYPE_SITE;

char *
jik_type_pretty_name(JikType *t);
JikType *
jik_type_new(JikTypeName name);
VecJikType *
jik_type_new_param_types(int n);
JikType *
jik_type_new_function(int num_params);
JikType *
jik_type_new_struct(char *name, TabJikType *field_types);
JikType *
jik_type_new_vector(JikType *elem_type);
JikType *
jik_type_new_enum(char *full_name);
JikType *
jik_type_new_variant(char *name, TabJikType *variant_types);
JikType *
jik_type_new_dict(JikType *elem_type);
JikType *
jik_type_new_option(JikType *elem_type);
bool
jik_type_is_inferred(JikType *t);
bool
jik_type_equal(JikType *t1, JikType *t2);
bool
jik_type_is_one_of(JikType *t, JikTypeName names[]);
bool
jik_type_is_numeric(JikType *t);
bool
jik_type_is_primitive(JikType *t);
bool
jik_type_is_allocated(JikType *t);
bool
jik_type_is_subscriptable(JikType *t);
bool
jik_type_is_accessible(JikType *t);
char *
jik_type_get_full_name(JikType *t);
void
jik_type_print(JikType *t);
bool
jik_type_is_iterable(JikType *t);
JikType *
jik_type_get_iterable_elem_type(JikType *t);

#endif
