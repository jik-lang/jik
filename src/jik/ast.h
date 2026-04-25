#ifndef JIK_AST_H
#define JIK_AST_H

#include <assert.h>

#include "alloc.h"
#include "common.h"
#include "diag.h"
#include "htab.h"
#include "scope.h"
#include "token.h"
#include "types.h"
#include "utils.h"
#include "vec.h"

typedef struct TabVarInfo TabVarInfo;

#define NODE_NAMES                                                                                 \
    X(NODE_PROGRAM)                                                                                \
    X(NODE_FUNCTION)                                                                               \
    X(NODE_EXTERN_FUNCTION)                                                                        \
    X(NODE_BUILTIN_FUNCTION)                                                                       \
    X(NODE_BLOCK)                                                                                  \
    X(NODE_STMNT_RETURN)                                                                           \
    X(NODE_STMNT_DECLARE)                                                                          \
    X(NODE_STMNT_ASSIGN)                                                                           \
    X(NODE_STMNT_MEMBER_SET)                                                                       \
    X(NODE_CATCH)                                                                                  \
    X(NODE_EXPR_SUBSCRIPT_GET)                                                                     \
    X(NODE_STMNT_SUBSCRIPT_SET)                                                                    \
    X(NODE_COND_IF)                                                                                \
    X(NODE_COND_IFELIF)                                                                            \
    X(NODE_COND_IFELSE)                                                                            \
    X(NODE_LOOP_WHILE)                                                                             \
    X(NODE_LOOP_FOR)                                                                               \
    X(NODE_LOOP_FOR_IN)                                                                            \
    X(NODE_LOOP_FOR_IN_DICT)                                                                       \
    X(NODE_STMNT_BREAK)                                                                            \
    X(NODE_STMNT_CONTINUE)                                                                         \
    X(NODE_EXPR_CALL)                                                                              \
    X(NODE_EXPR_TERNARY)                                                                           \
    X(NODE_EXPR_BINOP)                                                                             \
    X(NODE_EXPR_UNOP)                                                                              \
    X(NODE_EXPR_INTEGER)                                                                           \
    X(NODE_EXPR_FLOAT)                                                                             \
    X(NODE_EXPR_CHAR)                                                                              \
    X(NODE_EXPR_STRING)                                                                            \
    X(NODE_EXPR_BOOL)                                                                              \
    X(NODE_EXPR_IDENTIFIER)                                                                        \
    X(NODE_EXPR_GROUPING)                                                                          \
    X(NODE_EXPR_MEMBER_ACCESS)                                                                     \
    X(NODE_EXPR_REGIONOF)                                                                          \
    X(NODE_EXPR_LOCAL_REGION)                                                                      \
    X(NODE_EXPR_MUST)                                                                              \
    X(NODE_EXPR_OPTION_SOME)                                                                       \
    X(NODE_EXPR_OPTION_NONE)                                                                       \
    X(NODE_EXPR_OPTION_UNWRAP)                                                                     \
    X(NODE_EXPR_OPTION_IS)                                                                         \
    X(NODE_STRUCT)                                                                                 \
    X(NODE_ENUM)                                                                                   \
    X(NODE_EXPR_ENUM_NEW)                                                                          \
    X(NODE_EXPR_STRUCT_NEW)                                                                        \
    X(NODE_EXPR_VARIANT_NEW)                                                                       \
    X(NODE_EXPR_VARIANT_TAG_CHECK)                                                                 \
    X(NODE_VARIANT)                                                                                \
    X(NODE_EXPR_VECTOR)                                                                            \
    X(NODE_EXPR_DICT)                                                                              \
    X(NODE_STMNT_MATCH)                                                                            \
    X(NODE_CASE)                                                                                   \
    X(NODE_VARIANT_TAG)                                                                            \
    X(NODE_HINT)                                                                                   \
    X(NODE_TYPE_DESC)                                                                              \
    X(NODE_EMBEDDED_C)                                                                             \
    X(NODE_PLACEHOLDER)                                                                            \
    X(NODE_BINOP_PLUS)

typedef enum JikNodeType {
#define X(name) name,
    NODE_NAMES
#undef X
        NUM_NODES
} JikNodeType;

//  TODO: do this for type list also in function type
JIK_VEC_DECLARE(VecJikNode, struct JikNode *);

typedef enum JikAllocKind {
    JIK_ALLOC_UNKNOWN,
    JIK_ALLOC_LOCAL,
    JIK_ALLOC_GLOBAL,
    JIK_ALLOC_CONTAINER,
    JIK_ALLOC_NAMED_REGION,
} JikAllocKind;

typedef enum JikAllocSource {
    JIK_ALLOC_SRC_UNKNOWN,
    JIK_ALLOC_SRC_LOCAL,
    JIK_ALLOC_SRC_FOREIGN,
    JIK_ALLOC_SRC_CROSS,
} JikAllocSource;

typedef struct JikAllocSpec {
    JikAllocKind   kind;
    JikAllocSource src;
    char          *region_name;
} JikAllocSpec;

typedef struct JikNode {
    JikNodeType type;
    JikType    *jik_type;
    JikScope   *context;
    JikToken   *token;
    bool        skip;
    union {
        // Expressions
        char           *val_code;
        bool            val_bool;
        char            val_char;
        struct JikNode *val_grouping;

        // String
        struct {
            char        *val;
            bool         literal;
            bool         multiline;
            JikAllocSpec alloc_spec;
        } val_str;

        // Identifier
        struct {
            char *name;
            char *mod_alias;
            char *mangled_name;
            // TODO: is this needed ?
            bool            is_global;
            bool            is_func_param; // TODO: wrong, can be overwritten
            struct JikNode *type_annot;
            bool            is_foreign;
        } val_id;

        struct {
            char           *val;
            struct JikNode *left;
            struct JikNode *right;
        } val_binop;

        struct {
            char           *val;
            struct JikNode *expr;
        } val_unop;

        struct {
            struct JikNode *expr;
        } val_must;

        struct {
            struct JikNode *expr;
            JikAllocSpec    alloc_spec;
        } val_option_some;

        struct {
            JikAllocSpec alloc_spec;
        } val_option_none;

        struct {
            struct JikNode *expr;
        } val_option_unwrap;

        struct {
            struct JikNode *expr;
            bool            is_some;
        } val_option_is;

        struct {
            struct JikNode *name;
            VecJikNode     *args;
            bool            builtin;
            char           *extern_name;
            JikAllocSpec    alloc_spec;
            bool            must;
            struct JikNode *parent_func;
            bool            auto_region;
        } val_call;

        struct {
            struct JikNode *condition;
            struct JikNode *expr_if;
            struct JikNode *expr_else;
        } val_ternary;

        // Member access and set
        struct {
            struct JikNode *node;
            char           *member_name;
            char           *variant_tag;
        } val_member_access;

        struct {
            struct JikNode *node;
            char           *member_name;
            struct JikNode *expr;
            char           *variant_tag;
        } val_member_set;

        // Types

        struct {
            struct JikNode *name; // this is type id for primitives and structs
            struct JikNode *desc; // this is for nested types
            JikTypeName     kind; // type name, from types.h
            JikAllocSpec    alloc_spec;
        } val_type_desc;

        // Hint
        struct {
            VecJikNode     *param_types; // vec of type descs
            struct JikNode *ret_type;    // type desc
        } val_hint;

        // Enum
        struct {
            char      *name;
            TabBool   *enumerators;
            VecString *enumerator_order;
            char      *first_enumerator;
        } val_enum;

        struct {
            char *enumerator;
        } val_enum_new;

        // Struct
        struct {
            char       *name;
            TabJikNode *type_descs;
            VecString  *field_order;
            TabJikNode *init_vals;
            bool        is_extern;
            bool        inferring;
        } val_struct;

        struct {
            struct JikNode *name;
            struct JikNode *struct_node;
            TabJikNode     *init_vals;
            JikAllocSpec    alloc_spec;
        } val_struct_new;

        // Variant
        struct {
            char           *name;
            char           *first_member;
            TabJikNode     *type_descs;
            VecString      *member_order;
            TabJikNode     *init_vals;
            struct JikNode *enum_nd;
            bool            inferring;
        } val_variant;

        struct {
            struct JikNode *name;
            struct JikNode *variant_node;
            struct JikNode *init_expr;
            char           *tag;
            JikAllocSpec    alloc_spec;
        } val_variant_new;

        struct {
            char           *tag;
            struct JikNode *inst_node;
            struct JikNode *id_node;
            // metadata
            struct JikNode *variant_node;
        } val_variant_tag_check;

        // Vector

        struct {
            struct JikNode *size_expr;
            struct JikNode *elem_expr;
            VecJikNode     *init_elems;
            JikAllocSpec    alloc_spec;
        } val_vector;

        struct {
            struct JikNode *node;
            struct JikNode *expr;
        } val_subscript_get;

        struct {
            struct JikNode *node;
            struct JikNode *sub_expr;
            struct JikNode *expr;
        } val_subscript_set;

        // Dict

        struct {
            struct JikNode *elem_expr;
            VecJikNode     *init_keys;
            VecJikNode     *init_values;
            JikAllocSpec    alloc_spec;
        } val_dict;

        // Statements
        struct {
            struct JikNode *expr;
            struct JikNode *parent_function;
        } val_return;

        struct {
            struct JikNode *id;
            struct JikNode *expr;
            struct JikNode *type_desc;
            bool            global;
        } val_declare;

        struct {
            struct JikNode *id;
            struct JikNode *expr;
            bool            declaration;
        } val_assign;

        // Block
        VecJikNode *val_block;

        // Functions
        struct {
            char *name;
            char *mangled_name;
            // VecString      *param_names;
            VecJikNode      *params;
            struct JikNode  *body;
            bool             has_return;
            struct JikNode  *hint;
            struct JikNode  *ret_type_annot;
            bool             has_allocs;
            TabVarInfo      *var_info;
            VecJikNode      *subnodes;
            struct FuncInfo *info;
            bool             throws;
        } val_function;

        struct {
            char *name;
            char *mangled_name;
        } val_builtin_function;

        // Externs
        struct {
            char           *name;
            char           *C_func_name;
            VecJikNode     *params;
            struct JikNode *ret_node;
            bool            throws;
        } val_extern_function;

        // Match
        struct {
            struct JikNode *name;
            char           *tag;
        } val_variant_tag;

        struct {
            struct JikNode *variant; // variant tag
            struct JikNode *body;
            struct JikNode *match;
        } val_case;

        struct {
            struct JikNode *expr;
            VecJikNode     *cases;
        } val_match;

        // Conditionals
        struct {
            struct JikNode *expr;
            struct JikNode *body;
        } val_if;

        struct {
            struct JikNode *expr;
            struct JikNode *body_if;
            struct JikNode *body_else;
        } val_ifelse;

        struct {
            struct JikNode *expr;
            struct JikNode *body_if;
            VecJikNode     *elifs;
            struct JikNode *body_else;
        } val_ifelif;

        // Catch
        struct {
            struct JikNode *stmnt;
            struct JikNode *body_err;
            struct JikNode *body_pass;
        } val_catch;

        // Loops
        struct {
            struct JikNode *expr;
            struct JikNode *body;
        } val_while;

        struct {
            struct JikNode *var_name;
            struct JikNode *start_expr;
            struct JikNode *end_expr;
            struct JikNode *body;
        } val_for;

        struct {
            struct JikNode *var_name;
            struct JikNode *container_expr;
            struct JikNode *body;
        } val_for_in;

        struct {
            struct JikNode *key_name;
            struct JikNode *val_name;
            struct JikNode *dict_expr;
            struct JikNode *body;
        } val_for_in_dict;

        // Program
        struct {
            VecJikNode *globals;
            VecJikNode *functions;
            VecJikNode *embedded_C;
            VecJikNode *extern_functions;
            VecJikNode *builtin_functions;
            VecJikNode *structs;
            VecJikNode *extern_structs;
            VecJikNode *enums;
            VecJikNode *variants;
            VecJikNode *hints;
        } val_program;
    };
} JikNode;

bool
jik_node_is_type_inferred(JikNode *nd);
bool
jik_node_types_equal(JikNode *n1, JikNode *n2);
bool
jik_node_is_type_one_of(JikNode *nd, JikTypeName names[]);

JikNode *
jik_node_new_block(void);
JikNode *
jik_node_new_program(void);
JikNode *
jik_node_new_hint(VecJikNode *param_types, JikNode *ret_type, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_function(char       *name,
                      VecJikNode *params,
                      JikNode    *body,
                      JikNode    *hint,
                      bool        throws,
                      JikScope   *ctx,
                      JikToken   *tok);
JikNode *
jik_node_new_extern_function(char     *name,
                             char     *C_func_name,
                             bool      throws,
                             JikScope *ctx,
                             JikToken *tok);
JikNode *
jik_node_new_builtin_function(char *name, JikType *type, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_cond_if(JikNode *expr, JikNode *body, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_cond_ifelse(JikNode  *expr,
                         JikNode  *body_if,
                         JikNode  *body_else,
                         JikScope *ctx,
                         JikToken *tok);
JikNode *
jik_node_new_cond_ifelif(JikNode    *expr,
                         JikNode    *body_if,
                         VecJikNode *elifs,
                         JikNode    *body_else,
                         JikScope   *ctx,
                         JikToken   *tok);
JikNode *
jik_node_new_catch(JikNode  *stmnt,
                   JikNode  *body_err,
                   JikNode  *body_pass,
                   JikScope *ctx,
                   JikToken *tok);
JikNode *
jik_node_new_loop_while(JikNode *expr, JikNode *body, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_loop_for(JikNode  *var_name,
                      JikNode  *start_expr,
                      JikNode  *end_expr,
                      JikNode  *body,
                      JikScope *ctx,
                      JikToken *tok);
JikNode *
jik_node_new_loop_for_in(JikNode  *var_name,
                         JikNode  *container_expr,
                         JikNode  *body,
                         JikScope *ctx,
                         JikToken *tok);
JikNode *
jik_node_new_loop_for_in_dict(JikNode  *key_name,
                              JikNode  *val_name,
                              JikNode  *dict_expr,
                              JikNode  *body,
                              JikScope *ctx,
                              JikToken *tok);

JikNode *
jik_node_new_break(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_continue(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_call(struct JikNode *name, VecJikNode *args, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_ternary(JikNode  *condition,
                     JikNode  *expr_if,
                     JikNode  *expr_else,
                     JikScope *ctx,
                     JikToken *tok);
JikNode *
jik_node_new_return(JikNode *expr, JikNode *parent_function, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_assign(JikNode *id, JikNode *expr, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_declare(JikNode *id, JikNode *expr, JikNode *type_desc, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_binop(JikNode *left, JikNode *right, char *val, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_unop(JikNode *expr, char *val, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_must(JikNode *expr, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_option_some(JikNode *expr, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_option_none(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_option_unwrap(JikNode *expr, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_option_is(JikNode *expr, bool is_some, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_integer(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_float(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_char(char ch, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_boolean(bool val, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_string(char *val, bool literal, bool multiline, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_identifier(char *name, char *mod_alias, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_grouping(JikNode *expr, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_enum(char     *name,
                  TabBool  *enumerators,
                  char     *first_enumerator,
                  JikScope *ctx,
                  JikToken *tok);
JikNode *
jik_node_new_enum_new(char *enumerator, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_struct(char       *name,
                    TabJikNode *type_descs,
                    bool        is_extern,
                    JikScope   *ctx,
                    JikToken   *tok);

JikNode *
jik_node_new_variant(char *name, TabJikNode *type_descs, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_variant_new(JikNode  *name,
                         JikNode  *init_expr,
                         char     *tag,
                         JikScope *ctx,
                         JikToken *tok);
JikNode *
jik_node_new_variant_tag_check(JikNode  *inst_node,
                               JikNode  *id_node,
                               char     *tag,
                               JikScope *ctx,
                               JikToken *tok);
JikNode *
jik_node_new_case(JikNode *variant, JikNode *body, JikNode *match, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_match(JikNode *expr, VecJikNode *cases, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_variant_tag(JikNode *name, char *tag, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_struct_new(JikNode *name, TabJikNode *init_vals, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_vector(JikNode    *size_expr,
                    JikNode    *elem_expr,
                    VecJikNode *init_elems,
                    JikScope   *ctx,
                    JikToken   *tok);
JikNode *
jik_node_new_member_access(JikNode *node, char *member_name, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_member_set(JikNode  *node,
                        char     *member_name,
                        JikNode  *expr,
                        JikScope *ctx,
                        JikToken *tok);
JikNode *
jik_node_new_subscript_get(JikNode *node, JikNode *expr, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_subscript_set(JikNode  *node,
                           JikNode  *sub_expr,
                           JikNode  *expr,
                           JikScope *ctx,
                           JikToken *tok);
JikNode *
jik_node_new_embedded_C(char *val, JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_type_desc(JikNode     *type_name,
                       JikNode     *type_desc,
                       JikTypeName  type_kind,
                       JikAllocSpec alloc_spec,
                       JikScope    *ctx,
                       JikToken    *tok);
void
jik_node_print(JikNode *nd, size_t level);
void
VecJikNode_print(VecJikNode *lst);
void
jik_collect_nodes(JikNode *nd, VecJikNode *nodes);

bool
jik_node_is_allocated_literal(JikNode *nd);
bool
jik_node_has_alloc_spec(JikNode *nd);
JikNode *
jik_node_get_vector_elem_expr(JikNode *nd);
JikNode *
jik_node_new_dict_literal(JikNode    *elem_expr,
                          VecJikNode *init_keys,
                          VecJikNode *init_values,
                          JikScope   *ctx,
                          JikToken   *tok);
void
jik_set_alloc_spec(JikNode *nd, JikAllocSpec spec);
JikAllocSpec
jik_get_alloc_spec(JikNode *nd);
JikNode *
jik_node_new_regionof(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_local_region(JikScope *ctx, JikToken *tok);
JikNode *
jik_node_new_placeholder(JikScope *ctx, JikToken *tok);
bool
jik_function_throws(JikNode *func_nd);

#endif
