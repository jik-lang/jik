#include "ast.h"

const char *NODE_STRINGS[NUM_NODES] = {
#define X(name) [name] = #name,
    NODE_NAMES
#undef X
};

//  TODO: do this for type list also in function type
JIK_VEC_DEFINE(VecJikNode, struct JikNode *);

bool
jik_node_is_type_inferred(JikNode *nd)
{
    return jik_type_is_inferred(nd->jik_type);
}

bool
jik_node_types_equal(JikNode *n1, JikNode *n2)
{
    assert(jik_node_is_type_inferred(n1));
    assert(jik_node_is_type_inferred(n2));
    return jik_type_equal(n1->jik_type, n2->jik_type);
}

bool
jik_node_is_type_one_of(JikNode *nd, JikTypeName names[])
{
    assert(jik_node_is_type_inferred(nd));
    return jik_type_is_one_of(nd->jik_type, names);
}

JikNode *
jik_node_new_block(void)
{
    JikNode *nd   = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type  = jik_type_new(TYPE_NOTYPE);
    nd->type      = NODE_BLOCK;
    nd->val_block = VecJikNode_new_empty();
    nd->context   = NULL;
    nd->token     = NULL;
    return nd;
}

JikNode *
jik_node_new_program(void)
{
    JikNode *nd                       = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                      = jik_type_new(TYPE_NOTYPE);
    nd->type                          = NODE_PROGRAM;
    nd->val_program.globals           = VecJikNode_new_empty();
    nd->val_program.functions         = VecJikNode_new_empty();
    nd->val_program.structs           = VecJikNode_new_empty();
    nd->val_program.extern_functions  = VecJikNode_new_empty();
    nd->val_program.builtin_functions = VecJikNode_new_empty();
    nd->val_program.extern_structs    = VecJikNode_new_empty();
    nd->val_program.embedded_C        = VecJikNode_new_empty();
    nd->val_program.hints             = VecJikNode_new_empty();
    nd->val_program.enums             = VecJikNode_new_empty();
    nd->val_program.variants          = VecJikNode_new_empty();
    nd->context                       = NULL;
    nd->token                         = NULL;
    return nd;
}

JikNode *
jik_node_new_hint(VecJikNode *param_types, JikNode *ret_type, JikScope *ctx, JikToken *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_NOTYPE);
    nd->type                 = NODE_HINT;
    nd->val_hint.param_types = param_types;
    nd->val_hint.ret_type    = ret_type;
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_function(char       *name,
                      VecJikNode *params,
                      JikNode    *body,
                      JikNode    *hint,
                      bool        throws,
                      JikScope   *ctx,
                      JikToken   *tok)
{
    JikNode *nd                     = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->type                        = NODE_FUNCTION;
    nd->val_function.name           = name;
    nd->val_function.params         = params;
    nd->val_function.body           = body;
    nd->val_function.has_return     = false;
    nd->val_function.mangled_name   = NULL;
    nd->val_function.hint           = hint;
    nd->val_function.has_allocs     = false;
    nd->val_function.var_info       = NULL;
    nd->val_function.info           = NULL;
    nd->val_function.ret_type_annot = NULL;
    nd->val_function.throws         = throws;
    nd->context                     = ctx;
    nd->token                       = tok;
    return nd;
}

JikNode *
jik_node_new_extern_function(char     *name,
                             char     *C_func_name,
                             bool      throws,
                             JikScope *ctx,
                             JikToken *tok)
{
    JikNode *nd                         = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                        = jik_type_new(TYPE_UNKNOWN);
    nd->type                            = NODE_EXTERN_FUNCTION;
    nd->val_extern_function.name        = name;
    nd->val_extern_function.C_func_name = C_func_name;
    nd->val_extern_function.params      = NULL;
    nd->val_extern_function.ret_node    = NULL;
    nd->val_extern_function.throws      = throws;
    nd->context                         = ctx;
    nd->token                           = tok;
    return nd;
}

JikNode *
jik_node_new_builtin_function(char *name, JikType *type, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                           = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                          = type;
    nd->type                              = NODE_BUILTIN_FUNCTION;
    nd->val_builtin_function.name         = name;
    nd->val_builtin_function.mangled_name = NULL;
    nd->context                           = ctx;
    nd->token                             = tok;
    return nd;
}

JikNode *
jik_node_new_cond_if(JikNode *expr, JikNode *body, JikScope *ctx, JikToken *tok)
{
    JikNode *nd     = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type    = jik_type_new(TYPE_NOTYPE);
    nd->type        = NODE_COND_IF;
    nd->val_if.expr = expr;
    nd->val_if.body = body;
    nd->context     = ctx;
    nd->token       = tok;
    return nd;
}

JikNode *
jik_node_new_cond_ifelse(JikNode  *expr,
                         JikNode  *body_if,
                         JikNode  *body_else,
                         JikScope *ctx,
                         JikToken *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_NOTYPE);
    nd->type                 = NODE_COND_IFELSE;
    nd->val_ifelse.expr      = expr;
    nd->val_ifelse.body_if   = body_if;
    nd->val_ifelse.body_else = body_else;
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_cond_ifelif(JikNode    *expr,
                         JikNode    *body_if,
                         VecJikNode *elifs,
                         JikNode    *body_else,
                         JikScope   *ctx,
                         JikToken   *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_NOTYPE);
    nd->type                 = NODE_COND_IFELIF;
    nd->val_ifelif.expr      = expr;
    nd->val_ifelif.body_if   = body_if;
    nd->val_ifelif.elifs     = elifs;
    nd->val_ifelif.body_else = body_else;
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_catch(JikNode  *stmnt,
                   JikNode  *body_err,
                   JikNode  *body_pass,
                   JikScope *ctx,
                   JikToken *tok)
{
    JikNode *nd             = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type            = jik_type_new(TYPE_NOTYPE);
    nd->type                = NODE_CATCH;
    nd->val_catch.stmnt     = stmnt;
    nd->val_catch.body_err  = body_err;
    nd->val_catch.body_pass = body_pass;
    nd->context             = ctx;
    nd->token               = tok;
    return nd;
}

JikNode *
jik_node_new_loop_while(JikNode *expr, JikNode *body, JikScope *ctx, JikToken *tok)
{
    JikNode *nd        = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type       = jik_type_new(TYPE_NOTYPE);
    nd->type           = NODE_LOOP_WHILE;
    nd->val_while.expr = expr;
    nd->val_while.body = body;
    nd->context        = ctx;
    nd->token          = tok;
    return nd;
}

JikNode *
jik_node_new_loop_for(JikNode  *var_name,
                      JikNode  *start_expr,
                      JikNode  *end_expr,
                      JikNode  *body,
                      JikScope *ctx,
                      JikToken *tok)
{
    JikNode *nd            = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type           = jik_type_new(TYPE_NOTYPE);
    nd->type               = NODE_LOOP_FOR;
    nd->val_for.var_name   = var_name;
    nd->val_for.start_expr = start_expr;
    nd->val_for.end_expr   = end_expr;
    nd->val_for.body       = body;
    nd->context            = ctx;
    nd->token              = tok;
    return nd;
}

JikNode *
jik_node_new_loop_for_in(JikNode  *var_name,
                         JikNode  *container_expr,
                         JikNode  *body,
                         JikScope *ctx,
                         JikToken *tok)
{
    JikNode *nd                   = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                  = jik_type_new(TYPE_NOTYPE);
    nd->type                      = NODE_LOOP_FOR_IN;
    nd->val_for_in.var_name       = var_name;
    nd->val_for_in.container_expr = container_expr;
    nd->val_for_in.body           = body;
    nd->context                   = ctx;
    nd->token                     = tok;
    return nd;
}

JikNode *
jik_node_new_loop_for_in_dict(JikNode  *key_name,
                              JikNode  *val_name,
                              JikNode  *dict_expr,
                              JikNode  *body,
                              JikScope *ctx,
                              JikToken *tok)
{
    JikNode *nd                   = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                  = jik_type_new(TYPE_NOTYPE);
    nd->type                      = NODE_LOOP_FOR_IN_DICT;
    nd->val_for_in_dict.key_name  = key_name;
    nd->val_for_in_dict.val_name  = val_name;
    nd->val_for_in_dict.dict_expr = dict_expr;
    nd->val_for_in_dict.body      = body;
    nd->context                   = ctx;
    nd->token                     = tok;
    return nd;
}

JikNode *
jik_node_new_break(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = jik_type_new(TYPE_NOTYPE);
    nd->type     = NODE_STMNT_BREAK;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_continue(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = jik_type_new(TYPE_NOTYPE);
    nd->type     = NODE_STMNT_CONTINUE;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_call(struct JikNode *name, VecJikNode *args, JikScope *ctx, JikToken *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_UNKNOWN);
    nd->type                 = NODE_EXPR_CALL;
    nd->val_call.name        = name;
    nd->val_call.args        = args;
    nd->val_call.builtin     = false;
    nd->val_call.extern_name = NULL;
    nd->val_call.alloc_spec  = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->val_call.must        = false;
    nd->val_call.parent_func = NULL;
    nd->val_call.auto_region = false;
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_ternary(JikNode  *condition,
                     JikNode  *expr_if,
                     JikNode  *expr_else,
                     JikScope *ctx,
                     JikToken *tok)
{
    JikNode *nd               = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type              = jik_type_new(TYPE_UNKNOWN);
    nd->type                  = NODE_EXPR_TERNARY;
    nd->val_ternary.condition = condition;
    nd->val_ternary.expr_if   = expr_if;
    nd->val_ternary.expr_else = expr_else;
    nd->context               = ctx;
    nd->token                 = tok;
    return nd;
}

JikNode *
jik_node_new_return(JikNode *expr, JikNode *parent_function, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                    = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                   = jik_type_new(TYPE_NOTYPE);
    nd->type                       = NODE_STMNT_RETURN;
    nd->val_return.expr            = expr;
    nd->val_return.parent_function = parent_function;
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_assign(JikNode *id, JikNode *expr, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type               = jik_type_new(TYPE_NOTYPE);
    nd->type                   = NODE_STMNT_ASSIGN;
    nd->val_assign.id          = id;
    nd->val_assign.expr        = expr;
    nd->val_assign.declaration = true;
    nd->context                = ctx;
    nd->token                  = tok;
    return nd;
}

JikNode *
jik_node_new_declare(JikNode *id, JikNode *expr, JikNode *type_desc, JikScope *ctx, JikToken *tok)
{
    JikNode *nd               = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type              = jik_type_new(TYPE_NOTYPE);
    nd->type                  = NODE_STMNT_DECLARE;
    nd->val_declare.id        = id;
    nd->val_declare.expr      = expr;
    nd->val_declare.type_desc = type_desc;
    nd->val_declare.global    = false;
    nd->context               = ctx;
    nd->token                 = tok;
    return nd;
}

JikNode *
jik_node_new_binop(JikNode *left, JikNode *right, char *val, JikScope *ctx, JikToken *tok)
{
    JikNode *nd         = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type        = jik_type_new(TYPE_UNKNOWN);
    nd->type            = NODE_EXPR_BINOP;
    nd->val_binop.left  = left;
    nd->val_binop.right = right;
    nd->val_binop.val   = val;
    nd->context         = ctx;
    nd->token           = tok;
    return nd;
}

JikNode *
jik_node_new_unop(JikNode *expr, char *val, JikScope *ctx, JikToken *tok)
{
    JikNode *nd       = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type      = jik_type_new(TYPE_UNKNOWN);
    nd->type          = NODE_EXPR_UNOP;
    nd->val_unop.expr = expr;
    nd->val_unop.val  = val;
    nd->context       = ctx;
    nd->token         = tok;
    return nd;
}

JikNode *
jik_node_new_must(JikNode *expr, JikScope *ctx, JikToken *tok)
{
    JikNode *nd       = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type      = jik_type_new(TYPE_UNKNOWN);
    nd->type          = NODE_EXPR_MUST;
    nd->val_must.expr = expr;
    nd->context       = ctx;
    nd->token         = tok;
    return nd;
}

JikNode *
jik_node_new_option_some(JikNode *expr, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                    = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                   = jik_type_new(TYPE_UNKNOWN);
    nd->type                       = NODE_EXPR_OPTION_SOME;
    nd->val_option_some.expr       = expr;
    nd->val_option_some.alloc_spec = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_option_none(JikScope *ctx, JikToken *tok)
{
    JikNode *nd                    = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                   = jik_type_new(TYPE_UNKNOWN);
    nd->type                       = NODE_EXPR_OPTION_NONE;
    nd->val_option_none.alloc_spec = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_option_unwrap(JikNode *expr, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type               = jik_type_new(TYPE_UNKNOWN);
    nd->type                   = NODE_EXPR_OPTION_UNWRAP;
    nd->val_option_unwrap.expr = expr;
    nd->context                = ctx;
    nd->token                  = tok;
    return nd;
}

JikNode *
jik_node_new_option_is(JikNode *expr, bool is_some, JikScope *ctx, JikToken *tok)
{
    JikNode *nd               = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type              = jik_type_new(TYPE_UNKNOWN);
    nd->type                  = NODE_EXPR_OPTION_IS;
    nd->val_option_is.expr    = expr;
    nd->val_option_is.is_some = is_some;
    nd->context               = ctx;
    nd->token                 = tok;
    return nd;
}

JikNode *
jik_node_new_integer(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = &JIK_TYPE_INT;
    nd->type     = NODE_EXPR_INTEGER;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_float(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = &JIK_TYPE_FLOAT;
    nd->type     = NODE_EXPR_FLOAT;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_char(char ch, JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = &JIK_TYPE_CHAR;
    nd->type     = NODE_EXPR_CHAR;
    nd->val_char = ch;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_boolean(bool val, JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = &JIK_TYPE_BOOL;
    nd->type     = NODE_EXPR_BOOL;
    nd->val_bool = val;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_string(char *val, bool literal, bool multiline, JikScope *ctx, JikToken *tok)
{
    JikNode *nd            = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type           = &JIK_TYPE_STRING;
    nd->type               = NODE_EXPR_STRING;
    nd->val_str.val        = val;
    nd->val_str.literal    = literal;
    nd->val_str.multiline  = multiline;
    nd->val_str.alloc_spec = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context            = ctx;
    nd->token              = tok;
    return nd;
}

JikNode *
jik_node_new_identifier(char *name, char *mod_alias, JikScope *ctx, JikToken *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_UNKNOWN);
    nd->type                 = NODE_EXPR_IDENTIFIER;
    nd->val_id.name          = name;
    nd->val_id.mod_alias     = mod_alias;
    nd->val_id.is_global     = false;
    nd->val_id.mangled_name  = NULL;
    nd->val_id.is_func_param = false;
    nd->val_id.is_foreign    = false;
    nd->val_id.type_annot    = NULL;
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_grouping(JikNode *expr, JikScope *ctx, JikToken *tok)
{
    JikNode *nd      = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type     = jik_type_new(TYPE_UNKNOWN);
    nd->type         = NODE_EXPR_GROUPING;
    nd->val_grouping = expr;
    nd->context      = ctx;
    nd->token        = tok;
    return nd;
}

JikNode *
jik_node_new_enum(char     *name,
                  TabBool  *enumerators,
                  char     *first_enumerator,
                  JikScope *ctx,
                  JikToken *tok)
{
    JikNode *nd                   = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                  = jik_type_new_enum(name);
    nd->type                      = NODE_ENUM;
    nd->val_enum.name             = name;
    nd->val_enum.enumerators      = enumerators;
    nd->val_enum.enumerator_order = VecString_new_empty();
    nd->val_enum.first_enumerator = first_enumerator;
    nd->context                   = ctx;
    nd->token                     = tok;
    return nd;
}

JikNode *
jik_node_new_enum_new(char *enumerator, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                 = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                = jik_type_new(TYPE_UNKNOWN);
    nd->type                    = NODE_EXPR_ENUM_NEW;
    nd->val_enum_new.enumerator = enumerator;
    nd->context                 = ctx;
    nd->token                   = tok;
    return nd;
}

JikNode *
jik_node_new_struct(char       *name,
                    TabJikNode *type_descs,
                    bool        is_extern,
                    JikScope   *ctx,
                    JikToken   *tok)
{
    JikNode *nd                = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type               = jik_type_new(TYPE_UNKNOWN);
    nd->type                   = NODE_STRUCT;
    nd->val_struct.name        = name;
    nd->val_struct.type_descs  = type_descs;
    nd->val_struct.field_order = VecString_new_empty();
    nd->val_struct.init_vals   = TabJikNode_new();
    nd->val_struct.is_extern   = is_extern;
    nd->val_struct.inferring   = false;
    nd->context                = ctx;
    nd->token                  = tok;
    return nd;
}

JikNode *
jik_node_new_variant(char *name, TabJikNode *type_descs, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                 = jik_type_new(TYPE_UNKNOWN);
    nd->type                     = NODE_VARIANT;
    nd->val_variant.type_descs   = type_descs;
    nd->val_variant.name         = name;
    nd->val_variant.first_member = NULL;
    nd->val_variant.member_order = VecString_new_empty();
    nd->val_variant.init_vals    = TabJikNode_new();
    nd->val_variant.inferring    = false;
    // nd->val_variant.enum_nd = enum_nd;
    nd->context = ctx;
    nd->token   = tok;
    return nd;
}

JikNode *
jik_node_new_variant_new(JikNode *name, JikNode *init_expr, char *tag, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                      = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                     = jik_type_new(TYPE_UNKNOWN);
    nd->type                         = NODE_EXPR_VARIANT_NEW;
    nd->val_variant_new.name         = name;
    nd->val_variant_new.init_expr    = init_expr;
    nd->val_variant_new.tag          = tag;
    nd->val_variant_new.variant_node = NULL;
    nd->val_variant_new.alloc_spec = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_variant_tag_check(JikNode  *inst_node,
                               JikNode  *id_node,
                               char     *tag,
                               JikScope *ctx,
                               JikToken *tok)
{
    JikNode *nd                         = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                        = jik_type_new(TYPE_UNKNOWN);
    nd->type                            = NODE_EXPR_VARIANT_TAG_CHECK;
    nd->val_variant_tag_check.tag       = tag;
    nd->val_variant_tag_check.id_node   = id_node;
    nd->val_variant_tag_check.inst_node = inst_node;
    nd->context                         = ctx;
    nd->token                           = tok;
    return nd;
}

JikNode *
jik_node_new_case(JikNode *variant, JikNode *body, JikNode *match, JikScope *ctx, JikToken *tok)
{
    JikNode *nd          = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type         = jik_type_new(TYPE_NOTYPE);
    nd->type             = NODE_CASE;
    nd->val_case.variant = variant;
    nd->val_case.body    = body;
    nd->val_case.match   = match;
    nd->context          = ctx;
    nd->token            = tok;
    return nd;
}

JikNode *
jik_node_new_match(JikNode *expr, VecJikNode *cases, JikScope *ctx, JikToken *tok)
{
    JikNode *nd         = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type        = jik_type_new(TYPE_NOTYPE);
    nd->type            = NODE_STMNT_MATCH;
    nd->val_match.expr  = expr;
    nd->val_match.cases = cases;
    nd->context         = ctx;
    nd->token           = tok;
    return nd;
}

JikNode *
jik_node_new_variant_tag(JikNode *name, char *tag, JikScope *ctx, JikToken *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_NOTYPE);
    nd->type                 = NODE_VARIANT_TAG;
    nd->val_variant_tag.name = name;
    nd->val_variant_tag.tag  = tag;
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_struct_new(JikNode *name, TabJikNode *init_vals, JikScope *ctx, JikToken *tok)
{
    JikNode *nd = (JikNode *)jik_alloc(sizeof(JikNode));
    // nd->jik_type = jik_type_new_struct(full_name, NULL);
    nd->jik_type                   = jik_type_new(TYPE_UNKNOWN);
    nd->type                       = NODE_EXPR_STRUCT_NEW;
    nd->val_struct_new.name        = name;
    nd->val_struct_new.struct_node = NULL;
    nd->val_struct_new.init_vals   = init_vals;
    nd->val_struct_new.alloc_spec  = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_vector(JikNode    *size_expr,
                    JikNode    *elem_expr,
                    VecJikNode *init_elems,
                    JikScope   *ctx,
                    JikToken   *tok)
{
    JikNode *nd               = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type              = jik_type_new(TYPE_UNKNOWN);
    nd->type                  = NODE_EXPR_VECTOR;
    nd->val_vector.elem_expr  = elem_expr;
    nd->val_vector.size_expr  = size_expr;
    nd->val_vector.init_elems = init_elems;
    nd->val_vector.alloc_spec = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context               = ctx;
    nd->token                 = tok;
    return nd;
}

JikNode *
jik_node_new_member_access(JikNode *node, char *member_name, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                       = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                      = jik_type_new(TYPE_UNKNOWN);
    nd->type                          = NODE_EXPR_MEMBER_ACCESS;
    nd->val_member_access.node        = node;
    nd->val_member_access.member_name = member_name;
    nd->context                       = ctx;
    nd->token                         = tok;
    return nd;
}

JikNode *
jik_node_new_member_set(JikNode  *node,
                        char     *member_name,
                        JikNode  *expr,
                        JikScope *ctx,
                        JikToken *tok)
{
    JikNode *nd                    = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                   = jik_type_new(TYPE_NOTYPE);
    nd->type                       = NODE_STMNT_MEMBER_SET;
    nd->val_member_set.node        = node;
    nd->val_member_set.member_name = member_name;
    nd->val_member_set.expr        = expr;
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_subscript_get(JikNode *node, JikNode *expr, JikScope *ctx, JikToken *tok)
{
    JikNode *nd                = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type               = jik_type_new(TYPE_UNKNOWN);
    nd->type                   = NODE_EXPR_SUBSCRIPT_GET;
    nd->val_subscript_get.node = node;
    nd->val_subscript_get.expr = expr;
    nd->context                = ctx;
    nd->token                  = tok;
    return nd;
}

JikNode *
jik_node_new_subscript_set(JikNode  *node,
                           JikNode  *sub_expr,
                           JikNode  *expr,
                           JikScope *ctx,
                           JikToken *tok)
{
    JikNode *nd                    = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                   = jik_type_new(TYPE_NOTYPE);
    nd->type                       = NODE_STMNT_SUBSCRIPT_SET;
    nd->val_subscript_set.node     = node;
    nd->val_subscript_set.sub_expr = sub_expr;
    nd->val_subscript_set.expr     = expr;
    nd->context                    = ctx;
    nd->token                      = tok;
    return nd;
}

JikNode *
jik_node_new_embedded_C(char *val, JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = jik_type_new(TYPE_NOTYPE);
    nd->type     = NODE_EMBEDDED_C;
    nd->val_code = val;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_type_desc(JikNode     *type_name,
                       JikNode     *type_desc,
                       JikTypeName  type_kind,
                       JikAllocSpec alloc_spec,
                       JikScope    *ctx,
                       JikToken    *tok)
{
    JikNode *nd                  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type                 = jik_type_new(TYPE_NOTYPE);
    nd->type                     = NODE_TYPE_DESC;
    nd->val_type_desc.name       = type_name;
    nd->val_type_desc.desc       = type_desc;
    nd->val_type_desc.kind       = type_kind;
    nd->val_type_desc.alloc_spec = alloc_spec;
    nd->context                  = ctx;
    nd->token                    = tok;
    return nd;
}

JikNode *
jik_node_new_dict_literal(JikNode    *elem_expr,
                          VecJikNode *init_keys,
                          VecJikNode *init_values,
                          JikScope   *ctx,
                          JikToken   *tok)
{
    JikNode *nd              = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type             = jik_type_new(TYPE_UNKNOWN);
    nd->type                 = NODE_EXPR_DICT;
    nd->val_dict.elem_expr   = elem_expr;
    nd->val_dict.init_keys   = init_keys;
    nd->val_dict.init_values = init_values;
    nd->val_dict.alloc_spec  = (JikAllocSpec){JIK_ALLOC_UNKNOWN, JIK_ALLOC_SRC_UNKNOWN, NULL};
    nd->context              = ctx;
    nd->token                = tok;
    return nd;
}

JikNode *
jik_node_new_regionof(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = &JIK_TYPE_REGION;
    nd->type     = NODE_EXPR_REGIONOF;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_local_region(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = &JIK_TYPE_REGION;
    nd->type     = NODE_EXPR_LOCAL_REGION;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

JikNode *
jik_node_new_placeholder(JikScope *ctx, JikToken *tok)
{
    JikNode *nd  = (JikNode *)jik_alloc(sizeof(JikNode));
    nd->jik_type = jik_type_new(TYPE_NOTYPE);
    nd->type     = NODE_PLACEHOLDER;
    nd->context  = ctx;
    nd->token    = tok;
    return nd;
}

bool
jik_function_throws(JikNode *func_nd)
{
    if (func_nd->type == NODE_FUNCTION) {
        return func_nd->val_function.throws;
    }
    else if (func_nd->type == NODE_EXTERN_FUNCTION) {
        return func_nd->val_extern_function.throws;
    }
    else if (func_nd->type == NODE_BUILTIN_FUNCTION) {
        return false;
    }
    jik_diag_fatal_error("internal error: invalid function node in jik_function_throws", "");
}

void
jik_node_print(JikNode *nd, size_t level)
{
    for (size_t i = 0; i < level; i++)
        printf("    ");
    if (nd->type == NODE_PROGRAM) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.globals); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.globals, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.structs); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.structs, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.extern_functions); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.extern_functions, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.extern_structs); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.extern_structs, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.functions); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.functions, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.embedded_C); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.embedded_C, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.enums); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.enums, i), level + 1);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.variants); i++) {
            jik_node_print(VecJikNode_get(nd->val_program.variants, i), level + 1);
        }
    }
    else if (nd->type == NODE_FUNCTION) {
        printf("<%s \"%s\"", NODE_STRINGS[nd->type], nd->val_function.name);
        printf(", params = ");
        // VecString_print(nd->val_function.param_names);
        printf(", type = ");
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_function.body, level + 1);
    }
    else if (nd->type == NODE_BUILTIN_FUNCTION) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_builtin_function.name);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXTERN_FUNCTION) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_extern_function.name);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_ENUM) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_enum.name);
        jik_type_print(nd->jik_type);
        printf(">\n");
        TabBool_iter it = TabBool_iter_new(nd->val_enum.enumerators);
        TabBool_item item;
        while (TabBool_iter_next(&it, &item)) {
            printf("%s\n", item.key);
        }
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_ENUM_NEW) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_enum_new.enumerator);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_STRUCT) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_struct.name);
        jik_type_print(nd->jik_type);
        printf(">\n");
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_struct.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            printf("%s = ", item.key);
            jik_node_print(item.value, 0);
        }
        printf(">\n");
    }
    else if (nd->type == NODE_VARIANT) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_variant.name);
        jik_type_print(nd->jik_type);
        printf(">\n");
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_variant.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            printf("%s = ", item.key);
            jik_node_print(item.value, 0);
        }
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        jik_node_print(nd->val_struct_new.name, level + 1);
        printf(">\n");
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_struct_new.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            printf("%s = ", item.key);
            jik_node_print(item.value, 0);
        }
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_VARIANT_NEW) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        jik_node_print(nd->val_variant_new.name, level + 1);
        if (nd->val_variant_new.init_expr) {
            jik_node_print(nd->val_variant_new.init_expr, level + 1);
        }
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_VARIANT_TAG_CHECK) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        jik_node_print(nd->val_variant_tag_check.inst_node, level + 1);
        printf(">\n");
    }
    else if (nd->type == NODE_VARIANT_TAG) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_VECTOR) {
        printf("<%s, ", NODE_STRINGS[nd->type]);
        if (nd->val_vector.init_elems) {
            for (size_t i = 0; i < VecJikNode_size(nd->val_vector.init_elems); i++) {
                jik_node_print(VecJikNode_get(nd->val_vector.init_elems, i), level + 1);
            }
        }
        else {
            jik_node_print(nd->val_vector.size_expr, level + 1);
            jik_node_print(nd->val_vector.elem_expr, level + 1);
        }
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_member_access.member_name);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_member_access.node, level + 1);
    }
    else if (nd->type == NODE_STMNT_MEMBER_SET) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_member_set.member_name);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_member_set.node, level + 1);
        jik_node_print(nd->val_member_set.expr, level + 1);
    }
    else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
        printf("<%s", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_subscript_get.node, level + 1);
        jik_node_print(nd->val_subscript_get.expr, level + 1);
        printf(">\n");
    }
    else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
        printf("<%s", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_subscript_set.node, level + 1);
        jik_node_print(nd->val_subscript_set.sub_expr, level + 1);
        jik_node_print(nd->val_subscript_set.expr, level + 1);
        printf(">\n");
    }
    else if (nd->type == NODE_BLOCK) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        for (size_t i = 0; i < VecJikNode_size(nd->val_block); i++) {
            jik_node_print(VecJikNode_get(nd->val_block, i), level + 1);
        }
    }
    else if (nd->type == NODE_STMNT_RETURN) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        if (nd->val_return.expr) {
            jik_node_print(nd->val_return.expr, level + 1);
        }
    }
    else if (nd->type == NODE_STMNT_DECLARE) {
        printf("<%s, id=>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_declare.id, level + 1);
        if (nd->val_declare.expr) {
            jik_node_print(nd->val_declare.expr, level + 1);
        }
        else if (nd->val_declare.type_desc) {
            jik_node_print(nd->val_declare.type_desc, level + 1);
        }
    }
    else if (nd->type == NODE_STMNT_ASSIGN) {
        printf("<%s, decl=%s, id=>\n",
               NODE_STRINGS[nd->type],
               nd->val_assign.declaration ? "true" : "false");
        jik_node_print(nd->val_assign.id, level + 1);
        jik_node_print(nd->val_assign.expr, level + 1);
    }
    else if (nd->type == NODE_COND_IF) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_if.expr, level + 1);
        jik_node_print(nd->val_if.body, level + 1);
    }
    else if (nd->type == NODE_COND_IFELSE) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_ifelse.expr, level + 1);
        jik_node_print(nd->val_ifelse.body_if, level + 1);
        jik_node_print(nd->val_ifelse.body_else, level + 1);
    }
    else if (nd->type == NODE_COND_IFELIF) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_ifelif.expr, level + 1);
        jik_node_print(nd->val_ifelif.body_if, level + 1);
        VecJikNode_iter it = VecJikNode_iter_new(nd->val_ifelif.elifs);
        JikNode        *elif_nd;
        while (VecJikNode_iter_next(&it, &elif_nd)) {
            jik_node_print(elif_nd, level + 1);
        }
        if (nd->val_ifelif.body_else != NULL) {
            jik_node_print(nd->val_ifelif.body_else, level + 1);
        }
    }
    else if (nd->type == NODE_CATCH) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_catch.stmnt, level + 1);
        jik_node_print(nd->val_catch.body_err, level + 1);
        jik_node_print(nd->val_catch.body_pass, level + 1);
    }
    else if (nd->type == NODE_LOOP_WHILE) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_while.expr, level + 1);
        jik_node_print(nd->val_while.body, level + 1);
    }
    else if (nd->type == NODE_LOOP_FOR) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_for.start_expr, level + 1);
        jik_node_print(nd->val_for.end_expr, level + 1);
        jik_node_print(nd->val_for.body, level + 1);
    }
    else if (nd->type == NODE_LOOP_FOR_IN) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_for_in.var_name, level + 1);
        jik_node_print(nd->val_for_in.container_expr, level + 1);
        jik_node_print(nd->val_for_in.body, level + 1);
    }
    else if (nd->type == NODE_LOOP_FOR_IN_DICT) {
        printf("<%s>\n", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_for_in_dict.key_name, level + 1);
        jik_node_print(nd->val_for_in_dict.val_name, level + 1);
        jik_node_print(nd->val_for_in_dict.dict_expr, level + 1);
        jik_node_print(nd->val_for_in_dict.body, level + 1);
    }
    else if (nd->type == NODE_STMNT_BREAK) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        printf(">\n");
    }
    else if (nd->type == NODE_STMNT_CONTINUE) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_BINOP) {
        printf("<%s \"%s\", type=", NODE_STRINGS[nd->type], nd->val_binop.val);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_binop.left, level + 1);
        jik_node_print(nd->val_binop.right, level + 1);
    }
    else if (nd->type == NODE_EXPR_UNOP) {
        printf("<%s \"%s\", type=", NODE_STRINGS[nd->type], nd->val_unop.val);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_unop.expr, level + 1);
    }
    else if (nd->type == NODE_EXPR_MUST) {
        printf("<%s>, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_must.expr, level + 1);
    }
    else if (nd->type == NODE_EXPR_OPTION_SOME) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_option_some.expr, level + 1);
    }
    else if (nd->type == NODE_EXPR_OPTION_NONE) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_OPTION_UNWRAP) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_option_unwrap.expr, level + 1);
    }
    else if (nd->type == NODE_EXPR_OPTION_IS) {
        printf(
            "<%s, %s, type=", NODE_STRINGS[nd->type], nd->val_option_is.is_some ? "Some" : "None");
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_option_is.expr, level + 1);
    }
    else if (nd->type == NODE_EXPR_INTEGER) {
        printf("<%s, val=%s, type=", NODE_STRINGS[nd->type], nd->token->lexeme);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_PLACEHOLDER) {
        printf("<%s, val=%s, type=", NODE_STRINGS[nd->type], nd->token->lexeme);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_REGIONOF) {
        printf("<%s, val=%s, type=", NODE_STRINGS[nd->type], nd->token->lexeme);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_LOCAL_REGION) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_FLOAT) {
        printf("<%s, val=%s, type=", NODE_STRINGS[nd->type], nd->token->lexeme);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_BOOL) {
        printf("<%s, val=%d, type=", NODE_STRINGS[nd->type], nd->val_bool);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_STRING) {
        printf("<%s, val=%s, type=", NODE_STRINGS[nd->type], nd->val_str.val);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_CHAR) {
        printf("<%s, val=%c, type=", NODE_STRINGS[nd->type], nd->val_char);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_IDENTIFIER) {
        printf("<%s, \"%s\", type=", NODE_STRINGS[nd->type], nd->val_id.name);
        jik_type_print(nd->jik_type);
        printf(">\n");
    }
    else if (nd->type == NODE_EXPR_CALL) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_call.name, level + 1);
        jik_type_print(nd->jik_type);
        printf(">\n");
        for (size_t i = 0; i < VecJikNode_size(nd->val_call.args); i++) {
            jik_node_print(VecJikNode_get(nd->val_call.args, i), level + 1);
        }
    }
    else if (nd->type == NODE_EXPR_TERNARY) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        jik_type_print(nd->jik_type);
        printf(">\n");
        jik_node_print(nd->val_ternary.condition, level + 1);
        jik_node_print(nd->val_ternary.expr_if, level + 1);
        jik_node_print(nd->val_ternary.expr_else, level + 1);
    }
    else if (nd->type == NODE_EXPR_GROUPING) {
        printf("<%s", NODE_STRINGS[nd->type]);
        jik_node_print(nd->val_grouping, level + 1);
        printf(">\n");
    }
    else if (nd->type == NODE_HINT) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
    }
    else if (nd->type == NODE_TYPE_DESC) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
        printf(">\n");
    }
    else if (nd->type == NODE_STMNT_MATCH) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
    }
    else if (nd->type == NODE_CASE) {
        printf("<%s, type=", NODE_STRINGS[nd->type]);
    }
    else if (nd->type == NODE_EXPR_DICT) {
        printf("<%s, ", NODE_STRINGS[nd->type]);
        if (nd->val_dict.elem_expr) {
            jik_node_print(nd->val_dict.elem_expr, level + 1);
            printf(">\n");
        }
        else {
            assert(nd->val_dict.init_keys);
            assert(nd->val_dict.init_values);
            for (size_t i = 0; i < VecJikNode_size(nd->val_dict.init_keys); i++) {
                jik_node_print(VecJikNode_get(nd->val_dict.init_keys, i), level + 1);
            }
            for (size_t i = 0; i < VecJikNode_size(nd->val_dict.init_values); i++) {
                jik_node_print(VecJikNode_get(nd->val_dict.init_values, i), level + 1);
            }
        }
    }
    else {
        jik_diag_fatal_error("unrecognized ast node", "");
    }
}

void
VecJikNode_print(VecJikNode *lst)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(lst); i++) {
        nd = VecJikNode_get(lst, i);
        printf("<%s>\n", NODE_STRINGS[nd->type]);
    }
}

void
jik_collect_nodes(JikNode *nd, VecJikNode *nodes)
{
    VecJikNode_push(nodes, nd);
    if (nd->type == NODE_PROGRAM) {
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.enums); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.enums, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.structs); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.structs, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.variants); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.variants, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.extern_structs); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.extern_structs, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.globals); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.globals, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.extern_functions); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.extern_functions, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.builtin_functions); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.builtin_functions, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.functions); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.functions, i), nodes);
        }
        for (size_t i = 0; i < VecJikNode_size(nd->val_program.hints); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_program.hints, i), nodes);
        }
    }
    else if (nd->type == NODE_FUNCTION) {
        jik_collect_nodes(nd->val_function.body, nodes);
        // for (size_t i = 0; i < VecJikNode_size(nd->val_function.params); i++) {
        //     jik_collect_nodes(VecJikNode_get(nd->val_function.params, i), nodes);
        // }
    }
    else if (nd->type == NODE_STRUCT) {
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_struct.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            jik_collect_nodes(item.value, nodes);
        }
    }
    else if (nd->type == NODE_VARIANT) {
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_variant.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            jik_collect_nodes(item.value, nodes);
        }
    }
    else if (nd->type == NODE_BLOCK) {
        for (size_t i = 0; i < VecJikNode_size(nd->val_block); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_block, i), nodes);
        }
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_struct_new.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            jik_collect_nodes(item.value, nodes);
        }
    }
    else if (nd->type == NODE_EXPR_VARIANT_NEW) {
        // jik_collect_nodes(nd->val_variant_new.name, nodes);
        if (nd->val_variant_new.init_expr) {
            jik_collect_nodes(nd->val_variant_new.init_expr, nodes);
        }
    }
    else if (nd->type == NODE_EXPR_VARIANT_TAG_CHECK) {
        jik_collect_nodes(nd->val_variant_tag_check.inst_node, nodes);
    }
    else if (nd->type == NODE_STMNT_MATCH) {
        jik_collect_nodes(nd->val_match.expr, nodes);
        for (size_t i = 0; i < VecJikNode_size(nd->val_match.cases); i++) {
            jik_collect_nodes(VecJikNode_get(nd->val_match.cases, i), nodes);
        }
    }
    else if (nd->type == NODE_CASE) {
        jik_collect_nodes(nd->val_case.body, nodes);
    }
    else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
        jik_collect_nodes(nd->val_member_access.node, nodes);
    }
    else if (nd->type == NODE_STMNT_MEMBER_SET) {
        jik_collect_nodes(nd->val_member_set.node, nodes);
        jik_collect_nodes(nd->val_member_set.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
        jik_collect_nodes(nd->val_subscript_get.node, nodes);
        jik_collect_nodes(nd->val_subscript_get.expr, nodes);
    }
    else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
        jik_collect_nodes(nd->val_subscript_set.node, nodes);
        jik_collect_nodes(nd->val_subscript_set.sub_expr, nodes);
        jik_collect_nodes(nd->val_subscript_set.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_VECTOR) {
        if (nd->val_vector.init_elems) {
            for (size_t i = 0; i < VecJikNode_size(nd->val_vector.init_elems); i++) {
                jik_collect_nodes(VecJikNode_get(nd->val_vector.init_elems, i), nodes);
            }
        }
        else {
            jik_collect_nodes(nd->val_vector.size_expr, nodes);
            jik_collect_nodes(nd->val_vector.elem_expr, nodes);
        }
    }
    else if (nd->type == NODE_EXPR_DICT) {
        if (nd->val_dict.init_values) {
            assert(nd->val_dict.init_keys);
            for (size_t i = 0; i < VecJikNode_size(nd->val_dict.init_keys); i++) {
                jik_collect_nodes(VecJikNode_get(nd->val_dict.init_keys, i), nodes);
            }
            for (size_t i = 0; i < VecJikNode_size(nd->val_dict.init_values); i++) {
                jik_collect_nodes(VecJikNode_get(nd->val_dict.init_values, i), nodes);
            }
        }
        else if (nd->val_dict.elem_expr) {
            jik_collect_nodes(nd->val_dict.elem_expr, nodes);
        }
    }
    else if (nd->type == NODE_STMNT_RETURN) {
        if (nd->val_return.expr) {
            jik_collect_nodes(nd->val_return.expr, nodes);
        }
    }
    else if (nd->type == NODE_STMNT_DECLARE) {
        jik_collect_nodes(nd->val_declare.expr, nodes);
        jik_collect_nodes(nd->val_declare.type_desc, nodes);
        // TODOY: should we yield this also?
        jik_collect_nodes(nd->val_declare.id, nodes);
    }
    else if (nd->type == NODE_STMNT_ASSIGN) {
        jik_collect_nodes(nd->val_assign.expr, nodes);
        // TODOY: should we yield this also?
        jik_collect_nodes(nd->val_assign.id, nodes);
    }
    else if (nd->type == NODE_COND_IF) {
        jik_collect_nodes(nd->val_if.expr, nodes);
        jik_collect_nodes(nd->val_if.body, nodes);
    }
    else if (nd->type == NODE_COND_IFELSE) {
        jik_collect_nodes(nd->val_ifelse.expr, nodes);
        jik_collect_nodes(nd->val_ifelse.body_if, nodes);
        jik_collect_nodes(nd->val_ifelse.body_else, nodes);
    }
    else if (nd->type == NODE_COND_IFELIF) {
        jik_collect_nodes(nd->val_ifelif.expr, nodes);
        jik_collect_nodes(nd->val_ifelif.body_if, nodes);
        VecJikNode_iter it = VecJikNode_iter_new(nd->val_ifelif.elifs);
        JikNode        *elif_nd;
        while (VecJikNode_iter_next(&it, &elif_nd)) {
            jik_collect_nodes(elif_nd, nodes);
        }
        if (nd->val_ifelif.body_else != NULL) {
            jik_collect_nodes(nd->val_ifelif.body_else, nodes);
        }
    }
    else if (nd->type == NODE_CATCH) {
        jik_collect_nodes(nd->val_catch.stmnt, nodes);
        jik_collect_nodes(nd->val_catch.body_err, nodes);
        jik_collect_nodes(nd->val_catch.body_pass, nodes);
    }
    else if (nd->type == NODE_LOOP_WHILE) {
        jik_collect_nodes(nd->val_while.expr, nodes);
        jik_collect_nodes(nd->val_while.body, nodes);
    }
    else if (nd->type == NODE_LOOP_FOR) {
        jik_collect_nodes(nd->val_for.start_expr, nodes);
        jik_collect_nodes(nd->val_for.end_expr, nodes);
        jik_collect_nodes(nd->val_for.body, nodes);
    }
    else if (nd->type == NODE_LOOP_FOR_IN) {
        jik_collect_nodes(nd->val_for_in.container_expr, nodes);
        jik_collect_nodes(nd->val_for_in.body, nodes);
    }
    else if (nd->type == NODE_LOOP_FOR_IN_DICT) {
        jik_collect_nodes(nd->val_for_in_dict.dict_expr, nodes);
        jik_collect_nodes(nd->val_for_in_dict.body, nodes);
    }
    else if (nd->type == NODE_EXPR_BINOP) {
        jik_collect_nodes(nd->val_binop.left, nodes);
        jik_collect_nodes(nd->val_binop.right, nodes);
    }
    else if (nd->type == NODE_EXPR_UNOP) {
        jik_collect_nodes(nd->val_unop.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_MUST) {
        jik_collect_nodes(nd->val_must.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_OPTION_SOME) {
        jik_collect_nodes(nd->val_option_some.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_OPTION_UNWRAP) {
        jik_collect_nodes(nd->val_option_unwrap.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_OPTION_IS) {
        jik_collect_nodes(nd->val_option_is.expr, nodes);
    }
    else if (nd->type == NODE_EXPR_CALL) {
        VecJikNode_iter it = VecJikNode_iter_new(nd->val_call.args);
        JikNode        *arg;
        while (VecJikNode_iter_next(&it, &arg)) {
            jik_collect_nodes(arg, nodes);
        }
    }
    else if (nd->type == NODE_EXPR_TERNARY) {
        jik_collect_nodes(nd->val_ternary.condition, nodes);
        jik_collect_nodes(nd->val_ternary.expr_if, nodes);
        jik_collect_nodes(nd->val_ternary.expr_else, nodes);
    }
    else if (nd->type == NODE_EXPR_GROUPING) {
        jik_collect_nodes(nd->val_grouping, nodes);
    }
}

bool
jik_node_is_allocated_literal(JikNode *nd)
{
    return nd->type == NODE_EXPR_STRUCT_NEW || nd->type == NODE_EXPR_VECTOR ||
           nd->type == NODE_EXPR_DICT || nd->type == NODE_EXPR_VARIANT_NEW ||
           nd->type == NODE_EXPR_STRING || nd->type == NODE_EXPR_OPTION_SOME ||
           nd->type == NODE_EXPR_OPTION_NONE;
}

bool
jik_node_has_alloc_spec(JikNode *nd)
{
    if (jik_node_is_allocated_literal(nd)) {
        return true;
    }
    if (nd->type == NODE_EXPR_CALL && nd->val_call.builtin &&
        strcmp(nd->val_call.name->val_id.name, "concat") == 0) {
        return true;
    }
    if (nd->type == NODE_EXPR_CALL && !nd->val_call.builtin && !nd->val_call.extern_name) {
        return jik_type_is_allocated(nd->jik_type);
    }
    if (nd->type == NODE_EXPR_TERNARY) {
        return jik_type_is_allocated(nd->jik_type);
    }
    if (nd->type == NODE_EXPR_IDENTIFIER) {
        return jik_type_is_allocated(nd->jik_type);
    }
    return false;
}

JikNode *
jik_node_get_vector_elem_expr(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_VECTOR);
    JikNode *elem = nd->val_vector.elem_expr;
    while (elem->type == NODE_EXPR_VECTOR) {
        elem = elem->val_vector.elem_expr;
    }
    return elem;
}

void
jik_set_alloc_spec(JikNode *nd, JikAllocSpec spec)
{
    // TODO: also recursively set for subelements
    if (nd->type == NODE_EXPR_VECTOR) {
        nd->val_vector.alloc_spec = spec;
        if (nd->val_vector.init_elems) {
            VecJikNode_iter it = VecJikNode_iter_new(nd->val_vector.init_elems);
            JikNode        *elem_nd;
            while (VecJikNode_iter_next(&it, &elem_nd)) {
                if (jik_node_is_allocated_literal(elem_nd)) {
                    jik_set_alloc_spec(elem_nd, spec);
                }
            }
        }
        else {
            if (jik_node_is_allocated_literal(nd->val_vector.elem_expr)) {
                jik_set_alloc_spec(nd->val_vector.elem_expr, spec);
            }
        }
    }
    else if (nd->type == NODE_EXPR_STRING) {
        nd->val_str.alloc_spec = spec;
    }
    else if (nd->type == NODE_EXPR_DICT) {
        nd->val_dict.alloc_spec = spec;
        if (nd->val_dict.elem_expr) {
            if (jik_node_is_allocated_literal(nd->val_dict.elem_expr)) {
                jik_set_alloc_spec(nd->val_dict.elem_expr, spec);
            }
        }
        else {
            VecJikNode_iter it = VecJikNode_iter_new(nd->val_dict.init_values);
            JikNode        *val;
            while (VecJikNode_iter_next(&it, &val)) {
                if (!jik_node_is_allocated_literal(val)) {
                    continue;
                }
                jik_set_alloc_spec(val, spec);
            }
        }
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        nd->val_struct_new.alloc_spec = spec;
        TabJikNode_iter it            = TabJikNode_iter_new(nd->val_struct_new.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            if (!jik_node_is_allocated_literal(item.value)) {
                continue;
            }
            jik_set_alloc_spec(item.value, spec);
        }
    }
    else if (nd->type == NODE_EXPR_VARIANT_NEW) {
        nd->val_variant_new.alloc_spec = spec;
        // assert(nd->val_variant_new.init_expr);
        if (nd->val_variant_new.init_expr &&
            jik_node_is_allocated_literal(nd->val_variant_new.init_expr)) {
            jik_set_alloc_spec(nd->val_variant_new.init_expr, spec);
        }
    }
    else if (nd->type == NODE_EXPR_OPTION_SOME) {
        nd->val_option_some.alloc_spec = spec;
        if (jik_node_is_allocated_literal(nd->val_option_some.expr)) {
            jik_set_alloc_spec(nd->val_option_some.expr, spec);
        }
    }
    else if (nd->type == NODE_EXPR_OPTION_NONE) {
        nd->val_option_none.alloc_spec = spec;
    }
    else {
        jik_diag_fatal_error("internal error: invalid node in jik_set_alloc_spec", "");
    }
}

JikAllocSpec
jik_get_alloc_spec(JikNode *nd)
{
    if (nd->type == NODE_EXPR_VECTOR) {
        return nd->val_vector.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_STRING) {
        return nd->val_str.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_DICT) {
        return nd->val_dict.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        return nd->val_struct_new.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_VARIANT_NEW) {
        return nd->val_variant_new.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_OPTION_SOME) {
        return nd->val_option_some.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_OPTION_NONE) {
        return nd->val_option_none.alloc_spec;
    }
    else {
        jik_diag_fatal_error("internal error: invalid node in jik_get_alloc_spec", "");
    }
}
