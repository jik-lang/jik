#include "types.h"

#include <assert.h>

#include "alloc.h"
#include "common.h"
#include "utils.h"

JikType JIK_TYPE_INT    = {.name = TYPE_INTEGER, .C_name = C_TYPE_NAME_INTEGER};
JikType JIK_TYPE_FLOAT  = {.name = TYPE_FLOAT, .C_name = C_TYPE_NAME_FLOAT};
JikType JIK_TYPE_BOOL   = {.name = TYPE_BOOL, .C_name = C_TYPE_NAME_BOOL};
JikType JIK_TYPE_STRING = {.name = TYPE_STRING, .C_name = C_TYPE_NAME_STRING};
JikType JIK_TYPE_VOID   = {.name = TYPE_VOID, .C_name = C_TYPE_NAME_VOID};
JikType JIK_TYPE_CHAR   = {.name = TYPE_CHAR, .C_name = C_TYPE_NAME_CHAR};
JikType JIK_TYPE_REGION = {.name = TYPE_REGION, .C_name = C_TYPE_NAME_REGION};
JikType JIK_TYPE_SITE   = {.name = TYPE_SITE, .C_name = C_TYPE_NAME_SITE};

JIK_HTAB_DEFINE(TabJikType, struct JikType *);
JIK_VEC_DEFINE(VecJikType, struct JikType *);

char *
jik_type_pretty_name(JikType *t)
{
    if (t->name == TYPE_INTEGER)
        return "int";
    else if (t->name == TYPE_FLOAT)
        return "double";
    else if (t->name == TYPE_BOOL)
        return "bool";
    else if (t->name == TYPE_VOID)
        return "void";
    else if (t->name == TYPE_STRING)
        return "String";
    else if (t->name == TYPE_CHAR)
        return "char";
    else if (t->name == TYPE_STRUCT) {
        return t->val_struct.name;
    }
    else if (t->name == TYPE_VECTOR) {
        return JIK_STRING_NCAT("Vec[", jik_type_pretty_name(t->val_vec.elem_type), "]");
    }
    else if (t->name == TYPE_DICT) {
        return JIK_STRING_NCAT("Dict[", jik_type_pretty_name(t->val_dict.elem_type), "]");
    }
    else if (t->name == TYPE_OPTION) {
        return JIK_STRING_NCAT("Option[", jik_type_pretty_name(t->val_option.elem_type), "]");
    }
    else if (t->name == TYPE_VARIANT) {
        return t->val_variant.name;
    }
    else if (t->name == TYPE_ENUM) {
        return t->val_enum.name;
    }
    else if (t->name == TYPE_REGION) {
        return "Region";
    }
    else if (t->name == TYPE_SITE) {
        return "Site";
    }
    else {
        return "???";
    }
}

JikType *
jik_type_new(JikTypeName name)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    t->name    = name;
    if (name == TYPE_INTEGER)
        t->C_name = C_TYPE_NAME_INTEGER;
    else if (name == TYPE_BOOL)
        t->C_name = C_TYPE_NAME_BOOL;
    else if (name == TYPE_VOID)
        t->C_name = C_TYPE_NAME_VOID;
    else if (name == TYPE_STRING)
        t->C_name = C_TYPE_NAME_STRING;
    else if (name == TYPE_CHAR)
        t->C_name = C_TYPE_NAME_CHAR;
    return t;
}

VecJikType *
jik_type_new_param_types(int n)
{
    if (n < 0) {
        return VecJikType_new(0);
    }
    VecJikType *v = VecJikType_new(n);
    for (int i = 0; i < n; i++) {
        VecJikType_set(v, i, jik_type_new(TYPE_UNKNOWN));
    }
    return v;
}

JikType *
jik_type_new_function(int num_params)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){.name                 = TYPE_FUNCTION,
                           .val_func.num_params  = num_params,
                           .val_func.ret_type    = jik_type_new(TYPE_UNKNOWN),
                           .val_func.param_types = jik_type_new_param_types(num_params),
                           .val_func.builtin     = false};
    return t;
}

JikType *
jik_type_new_struct(char *name, TabJikType *field_types)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){.name                   = TYPE_STRUCT,
                           .C_name                 = NULL,
                           .val_struct.name        = name,
                           .val_struct.field_types = field_types,
                           .val_struct.visiting    = false};
    return t;
}

JikType *
jik_type_new_vector(JikType *elem_type)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){.name = TYPE_VECTOR, .val_vec.elem_type = elem_type};
    return t;
}

JikType *
jik_type_new_dict(JikType *elem_type)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){.name = TYPE_DICT, .val_dict.elem_type = elem_type};
    return t;
}

JikType *
jik_type_new_option(JikType *elem_type)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){.name = TYPE_OPTION, .val_option.elem_type = elem_type};
    return t;
}

JikType *
jik_type_new_enum(char *name)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){
                .name          = TYPE_ENUM,
                .C_name        = NULL,
                .val_enum.name = name,
    };
    return t;
}

JikType *
jik_type_new_variant(char *name, TabJikType *variant_types)
{
    JikType *t = (JikType *)jik_alloc(sizeof(JikType));
    *t         = (JikType){.name                      = TYPE_VARIANT,
                           .C_name                    = NULL,
                           .val_variant.name          = name,
                           .val_variant.variant_types = variant_types,
                           .val_variant.visiting      = false,
                           .val_variant.enum_type     = NULL};
    return t;
}

bool
jik_type_is_inferred(JikType *t)
{
    assert(t);
    if (t->name == TYPE_NOTYPE) {
        return true;
    }
    if (t->name == TYPE_UNKNOWN) {
        return false;
    }
    if (t->name == TYPE_FUNCTION) {
        if (!jik_type_is_inferred(t->val_func.ret_type)) {
            return false;
        }
        if (t->val_func.num_params < 0) {
            return true;
        }
        for (int i = 0; i < t->val_func.num_params; i++) {
            if (!jik_type_is_inferred(VecJikType_get(t->val_func.param_types, i))) {
                return false;
            }
        }
    }
    if (t->name == TYPE_STRUCT) {
        if (t->val_struct.field_types == NULL) {
            return false;
        }

        if (t->val_struct.visiting) {
            return true;
        }
        t->val_struct.visiting = true;

        bool            inferred = true;
        TabJikType_iter it       = TabJikType_iter_new(t->val_struct.field_types);
        TabJikType_item item;
        while (TabJikType_iter_next(&it, &item)) {
            if (!jik_type_is_inferred(item.value)) {
                inferred = false;
            }
        }
        t->val_struct.visiting = false;
        return inferred;
    }
    if (t->name == TYPE_VARIANT) {
        if (t->val_variant.variant_types == NULL) {
            return false;
        }
        if (t->val_variant.visiting) {
            return true;
        }
        t->val_variant.visiting  = true;
        bool            inferred = true;
        TabJikType_iter it       = TabJikType_iter_new(t->val_variant.variant_types);
        TabJikType_item item;
        while (TabJikType_iter_next(&it, &item)) {
            if (!jik_type_is_inferred(item.value)) {
                inferred = false;
            }
        }
        t->val_variant.visiting = false;
        return inferred;
    }
    if (t->name == TYPE_VECTOR) {
        return jik_type_is_inferred(t->val_vec.elem_type);
    }
    if (t->name == TYPE_DICT) {
        return jik_type_is_inferred(t->val_dict.elem_type);
    }
    if (t->name == TYPE_OPTION) {
        return jik_type_is_inferred(t->val_option.elem_type);
    }
    return true;
}

bool
jik_type_equal(JikType *t1, JikType *t2)
{
    // NOTE: we rely on pointer uniqueness per primitive and struct types here.
    assert(jik_type_is_inferred(t1) && jik_type_is_inferred(t2));
    if (t1->name == TYPE_VECTOR && t2->name == TYPE_VECTOR) {
        if (t1->val_vec.elem_type->name != t2->val_vec.elem_type->name) {
            return false;
        }
        return jik_type_equal(t1->val_vec.elem_type, t2->val_vec.elem_type);
    }
    else if (t1->name == TYPE_DICT && t2->name == TYPE_DICT) {
        if (t1->val_dict.elem_type->name != t2->val_dict.elem_type->name) {
            return false;
        }
        return jik_type_equal(t1->val_dict.elem_type, t2->val_dict.elem_type);
    }
    else if (t1->name == TYPE_OPTION && t2->name == TYPE_OPTION) {
        if (t1->val_option.elem_type->name != t2->val_option.elem_type->name) {
            return false;
        }
        return jik_type_equal(t1->val_option.elem_type, t2->val_option.elem_type);
    }
    return t1 == t2;
}

// we use TYPE_NOTYPE as sentinel in passed names array.
bool
jik_type_is_one_of(JikType *t, JikTypeName names[])
{
    for (size_t i = 0; names[i] != TYPE_NOTYPE; i++) {
        if (names[i] == t->name)
            return true;
    }
    return false;
}

bool
jik_type_is_numeric(JikType *t)
{
    return jik_type_is_one_of(t, (JikTypeName[]){TYPE_INTEGER, TYPE_FLOAT, TYPE_NOTYPE});
}

bool
jik_type_is_primitive(JikType *t)
{
    return jik_type_is_one_of(
        t, (JikTypeName[]){TYPE_INTEGER, TYPE_FLOAT, TYPE_BOOL, TYPE_CHAR, TYPE_ENUM, TYPE_NOTYPE});
}

bool
jik_type_is_allocated(JikType *t)
{
    return jik_type_is_one_of(t,
                              (JikTypeName[]){TYPE_STRUCT,
                                              TYPE_STRING,
                                              TYPE_VECTOR,
                                              TYPE_VARIANT,
                                              TYPE_DICT,
                                              TYPE_OPTION,
                                              TYPE_NOTYPE});
}

bool
jik_type_is_subscriptable(JikType *t)
{
    return jik_type_is_one_of(
        t, (JikTypeName[]){TYPE_VECTOR, TYPE_STRING, TYPE_DICT, TYPE_VARIANT, TYPE_NOTYPE});
}

bool
jik_type_is_accessible(JikType *t)
{
    return jik_type_is_one_of(t, (JikTypeName[]){TYPE_STRUCT, TYPE_VARIANT, TYPE_NOTYPE});
}

char *
jik_type_get_full_name(JikType *t)
{
    if (jik_type_is_primitive(t)) {
        return t->C_name;
    }
    return NULL;
}

void
jik_type_print(JikType *t)
{
    if (t->name == TYPE_FUNCTION) {
        printf("(%s, ", TYPE_STRINGS[t->name]);
        jik_type_print(t->val_func.ret_type);
        for (int i = 0; i < t->val_func.num_params; i++) {
            jik_type_print(VecJikType_get(t->val_func.param_types, i));
            printf(", ");
        }
        printf(")");
    }
    else {
        // printf("(%s, %s)", TYPE_STRINGS[t->name], t->C_name);
        printf("(%s, %s)", TYPE_STRINGS[t->name], "");
    }
}

bool
jik_type_is_iterable(JikType *t)
{
    return t->name == TYPE_STRING || t->name == TYPE_VECTOR || t->name == TYPE_DICT;
}

JikType *
jik_type_get_iterable_elem_type(JikType *t)
{
    assert(jik_type_is_iterable(t));
    if (t->name == TYPE_STRING) {
        return &JIK_TYPE_CHAR;
    }
    else if (t->name == TYPE_VECTOR) {
        return t->val_vec.elem_type;
    }
    else if (t->name == TYPE_DICT) {
        return t->val_dict.elem_type;
    }
    else if (t->name == TYPE_OPTION) {
        return t->val_option.elem_type;
    }
    else {
        return NULL;
    }
}
