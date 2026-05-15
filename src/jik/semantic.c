#include "semantic.h"

#include "charbuf.h"
#include "diag.h"
#include "parser.h"
#include "regcheck.h"
#include "scope.h"
#include "typecheck.h"
#include "types.h"
#include "utils.h"

#define JIK_DEBUG(num) printf("------------------------%d\n", (num));

static JikType *
jik_semantic_resolve_type_or_error(JikNode *nd);
static JikNode *
jik_make_init_call_for_extern_struct(JikType  *t,
                                     char     *mod_alias,
                                     JikScope *context,
                                     JikToken *token);

static bool
jik_type_is_extern_struct(JikType *t)
{
    return t && t->name == TYPE_STRUCT && t->is_extern;
}

static bool
jik_tab_jik_node_is_empty(TabJikNode *tab)
{
    TabJikNode_iter it = TabJikNode_iter_new(tab);
    TabJikNode_item item;
    return !TabJikNode_iter_next(&it, &item);
}

void
jik_semantic_init(JikSemanticAnalyzer *sa, JikContext *ctx)
{
    sa->ctx                   = ctx;
    sa->nodes                 = ctx->nodes;
    sa->uninferred            = VecJikNode_new_empty();
    sa->main_defined          = false;
    sa->function_nodes        = TabJikNode_new();
    sa->struct_nodes          = TabJikNode_new();
    sa->variant_nodes         = TabJikNode_new();
    sa->extern_function_nodes = TabJikNode_new();
    sa->extern_struct_nodes   = TabJikNode_new();
    sa->enum_nodes            = TabJikNode_new();
    sa->funs_with_returns     = TabBool_new();
    sa->allocated_symbols     = TabJikNode_new();
    sa->current_struct        = NULL;
    sa->needs_recollect       = false;
}

bool
is_main_function(JikNode *nd)
{
    assert(nd->type == NODE_FUNCTION);
    return strcmp(nd->val_function.name, "main") == 0 && strcmp(nd->token->mod_alias, "main") == 0;
}

static bool
jik_node_is_fail_statement(JikNode *nd)
{
    return nd && nd->type == NODE_EXPR_CALL && strcmp(nd->val_call.name->val_id.name, "fail") == 0;
}

static bool
jik_identifier_is_builtin_name(JikSemanticAnalyzer *sa, char *name)
{
    assert(sa->ctx->ast);
    assert(sa->ctx->ast->type == NODE_PROGRAM);
    size_t n = VecJikNode_size(sa->ctx->ast->val_program.builtin_functions);
    for (size_t i = 0; i < n; i++) {
        JikNode *builtin = VecJikNode_get(sa->ctx->ast->val_program.builtin_functions, i);
        if (strcmp(name, builtin->val_builtin_function.name) == 0) {
            return true;
        }
    }
    return false;
}

static void
jik_semantic_reject_reserved_prefix(char *name, JikToken *tok)
{
    jik_diag_fatal_error_if(
        jik_identifier_has_reserved_prefix(name),
        JIK_STRING_NCAT("identifier \"", name, "\" uses reserved prefix \"jik_\""),
        jik_token_to_text(tok));
}

static void
jik_semantic_reject_builtin_name_collision(JikSemanticAnalyzer *sa, char *name, JikToken *tok)
{
    jik_diag_fatal_error_if(jik_identifier_is_builtin_name(sa, name),
                            JIK_STRING_NCAT("identifier \"", name, "\" collides with builtin name"),
                            jik_token_to_text(tok));
}

void
jik_semantic_ensure_module_used(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_IDENTIFIER);
    if (!nd->val_id.mod_alias || strcmp(nd->val_id.mod_alias, nd->token->mod_alias) == 0) {
        return;
    }
    bool *res = TabBool_get(nd->token->used_aliases, nd->val_id.mod_alias);
    jik_diag_fatal_error_if(!res,
                            JIK_STRING_NCAT("unknown module: ", nd->val_id.mod_alias),
                            jik_token_to_text(nd->token));
}

void
jik_ensure_valid_variant_tag(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_VARIANT_NEW);
    JikNode *s = jik_scope_get_symbol(nd->context,
                                      nd->val_variant_new.name->val_id.name,
                                      nd->val_variant_new.name->val_id.mod_alias,
                                      nd->token->mod_alias);
    jik_semantic_ensure_module_used(nd->val_variant_new.name);
    jik_diag_fatal_error_if(
        !s,
        JIK_STRING_NCAT("variant \"", nd->val_variant_new.name->val_id.name, "\" not defined"),
        jik_token_to_text(nd->token));
    nd->val_variant_new.variant_node = s;
    JikNode **res = TabJikNode_get(s->val_variant.type_descs, nd->val_variant_new.tag);
    jik_diag_fatal_error_if(
        !res,
        JIK_STRING_NCAT("unknown variant tag \"", nd->val_variant_new.tag, "\""),
        jik_token_to_text(nd->token));
}

static void
jik_semantic_reject_invalid_value_expr(JikNode *nd)
{
    if (!nd) {
        return;
    }
    if (nd->type == NODE_EXPR_IDENTIFIER) {
        JikNode *sym = jik_scope_get_symbol(
            nd->context, nd->val_id.name, nd->val_id.mod_alias, nd->token->mod_alias);
        if (!sym) {
            return;
        }
        if (sym->type == NODE_STRUCT) {
            jik_diag_fatal_error(JIK_STRING_NCAT("struct name cannot be used as a value; use ",
                                                 nd->val_id.name,
                                                 "{}"),
                                 jik_token_to_text(nd->token));
        }
        if (sym->type == NODE_ENUM) {
            jik_diag_fatal_error(JIK_STRING_NCAT("enum name cannot be used as a value; use ",
                                                 nd->val_id.name,
                                                 ".",
                                                 sym->val_enum.first_enumerator),
                                 jik_token_to_text(nd->token));
        }
        if (sym->type == NODE_VARIANT) {
            jik_diag_fatal_error(JIK_STRING_NCAT("variant name cannot be used as a value; use ",
                                                 nd->val_id.name,
                                                 ".",
                                                 sym->val_variant.first_member,
                                                 "{}"),
                                 jik_token_to_text(nd->token));
        }
    }
    if (nd->type == NODE_VARIANT_TAG) {
        jik_diag_fatal_error(JIK_STRING_NCAT("variant tag cannot be used as a value; use ",
                                             nd->val_variant_tag.name->val_id.name,
                                             ".",
                                             nd->val_variant_tag.tag,
                                             "{}"),
                             jik_token_to_text(nd->token));
    }
    if (nd->type == NODE_EXPR_MEMBER_ACCESS &&
        nd->val_member_access.node->type == NODE_EXPR_IDENTIFIER) {
        JikNode *sym = jik_scope_get_symbol(nd->context,
                                            nd->val_member_access.node->val_id.name,
                                            nd->val_member_access.node->val_id.mod_alias,
                                            nd->token->mod_alias);
        if (sym && sym->type == NODE_VARIANT) {
            jik_diag_fatal_error(JIK_STRING_NCAT("variant tag cannot be used as a value; use ",
                                                 nd->val_member_access.node->val_id.name,
                                                 ".",
                                                 nd->val_member_access.member_name,
                                                 "{}"),
                                 jik_token_to_text(nd->token));
        }
    }
}

static void
jik_semantic_reject_invalid_value_exprs_in_expr(JikNode *nd)
{
    if (!nd) {
        return;
    }
    jik_semantic_reject_invalid_value_expr(nd);
    if (nd->type == NODE_EXPR_CALL) {
        size_t n = VecJikNode_size(nd->val_call.args);
        for (size_t i = 0; i < n; i++) {
            jik_semantic_reject_invalid_value_expr(VecJikNode_get(nd->val_call.args, i));
        }
    }
    else if (nd->type == NODE_EXPR_VECTOR && nd->val_vector.init_elems) {
        size_t n = VecJikNode_size(nd->val_vector.init_elems);
        for (size_t i = 0; i < n; i++) {
            jik_semantic_reject_invalid_value_expr(VecJikNode_get(nd->val_vector.init_elems, i));
        }
    }
    else if (nd->type == NODE_EXPR_DICT && nd->val_dict.init_values) {
        size_t n = VecJikNode_size(nd->val_dict.init_values);
        for (size_t i = 0; i < n; i++) {
            jik_semantic_reject_invalid_value_expr(VecJikNode_get(nd->val_dict.init_values, i));
        }
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        TabJikNode_iter it = TabJikNode_iter_new(nd->val_struct_new.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            jik_semantic_reject_invalid_value_expr(item.value);
        }
    }
}

static void
jik_semantic_check_invalid_value_exprs(JikSemanticAnalyzer *sa)
{
    JikNode        *nd;
    VecJikNode_iter it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_STMNT_ASSIGN) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd->val_assign.expr);
        }
        else if (nd->type == NODE_STMNT_DECLARE) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd->val_declare.expr);
        }
        else if (nd->type == NODE_STMNT_RETURN) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd->val_return.expr);
        }
        else if (nd->type == NODE_EXPR_CALL) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd);
        }
        else if (nd->type == NODE_EXPR_VECTOR) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd);
        }
        else if (nd->type == NODE_EXPR_DICT) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd);
        }
        else if (nd->type == NODE_EXPR_STRUCT_NEW) {
            jik_semantic_reject_invalid_value_exprs_in_expr(nd);
        }
    }
}

static void
jik_semantic_resolve_symbols(JikSemanticAnalyzer *sa)
{
    JikNode *nd;
    // 1. Add symbols to scopes
    VecJikNode_iter it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        // TODO: this is badly written, and may not be needed at all
        if (nd->type != NODE_PROGRAM && nd->type != NODE_FUNCTION && nd->type != NODE_STRUCT &&
            nd->type != NODE_ENUM && nd->type != NODE_BUILTIN_FUNCTION &&
            nd->type != NODE_EXTERN_FUNCTION && nd->type != NODE_VARIANT) {
            assert(nd->context != NULL);
        }
        if (nd->type == NODE_ENUM) {
            assert(nd->context->parent == NULL);
            jik_semantic_reject_reserved_prefix(nd->val_enum.name, nd->token);
            jik_semantic_reject_builtin_name_collision(sa, nd->val_enum.name, nd->token);
            bool res = jik_scope_add_global_symbol(nd->val_enum.name, nd->token->mod_alias, nd);
            jik_diag_fatal_error_if(!res,
                                    JIK_STRING_NCAT("symbol \"",
                                                    nd->val_enum.name,
                                                    "\" already defined in module \"",
                                                    nd->token->mod_alias,
                                                    "\""),
                                    "");
        }
        else if (nd->type == NODE_STRUCT) {
            assert(nd->context->parent == NULL);
            jik_semantic_reject_reserved_prefix(nd->val_struct.name, nd->token);
            jik_semantic_reject_builtin_name_collision(sa, nd->val_struct.name, nd->token);
            TabJikNode_iter it_fields = TabJikNode_iter_new(nd->val_struct.type_descs);
            TabJikNode_item item_field;
            while (TabJikNode_iter_next(&it_fields, &item_field)) {
                jik_semantic_reject_reserved_prefix(item_field.key, item_field.value->token);
            }
            bool res = jik_scope_add_global_symbol(nd->val_struct.name, nd->token->mod_alias, nd);
            jik_diag_fatal_error_if(!res,
                                    JIK_STRING_NCAT("symbol \"",
                                                    nd->val_struct.name,
                                                    "\" already defined in module \"",
                                                    nd->token->mod_alias,
                                                    "\""),
                                    "");
        }
        else if (nd->type == NODE_VARIANT) {
            assert(nd->context->parent == NULL);
            jik_semantic_reject_reserved_prefix(nd->val_variant.name, nd->token);
            jik_semantic_reject_builtin_name_collision(sa, nd->val_variant.name, nd->token);
            TabJikNode_iter it_fields = TabJikNode_iter_new(nd->val_variant.type_descs);
            TabJikNode_item item_field;
            while (TabJikNode_iter_next(&it_fields, &item_field)) {
                jik_semantic_reject_reserved_prefix(item_field.key, item_field.value->token);
            }
            bool res = jik_scope_add_global_symbol(nd->val_variant.name, nd->token->mod_alias, nd);
            jik_diag_fatal_error_if(!res,
                                    JIK_STRING_NCAT("symbol \"",
                                                    nd->val_variant.name,
                                                    "\" already defined in module \"",
                                                    nd->token->mod_alias,
                                                    "\""),
                                    "");
        }
        else if (nd->type == NODE_FUNCTION) {
            jik_semantic_reject_reserved_prefix(nd->val_function.name, nd->token);
            size_t num_params = VecJikNode_size(nd->val_function.params);
            for (size_t i = 0; i < num_params; i++) {
                JikNode *param = VecJikNode_get(nd->val_function.params, i);
                jik_semantic_reject_reserved_prefix(param->val_id.name, param->token);
                jik_semantic_reject_builtin_name_collision(sa, param->val_id.name, param->token);
                param->val_id.is_func_param = true;
                jik_scope_add_local_symbol(
                    nd->val_function.body->context, param->val_id.name, param);
            }

            // assert(nd->context->parent == NULL);
            if (is_main_function(nd)) {
                sa->main_defined = true;
                jik_diag_fatal_error_if(num_params > 1,
                                        "main function takes at most one argument",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(nd->val_function.throws,
                                        "main function cannot throw",
                                        jik_token_to_text(nd->token));
            }
            bool res = jik_scope_add_global_symbol(nd->val_function.name, nd->token->mod_alias, nd);
            jik_diag_fatal_error_if(!res,
                                    JIK_STRING_NCAT("symbol \"",
                                                    nd->val_function.name,
                                                    "\" already defined in module \"",
                                                    nd->token->mod_alias,
                                                    "\""),
                                    "");
        }
        else if (nd->type == NODE_BUILTIN_FUNCTION) {
            bool res = jik_scope_add_builtin_symbol(nd->val_builtin_function.name, nd);
            assert(res);
        }
        else if (nd->type == NODE_EXTERN_FUNCTION) {
            jik_semantic_reject_reserved_prefix(nd->val_extern_function.name, nd->token);
            bool res =
                jik_scope_add_global_symbol(nd->val_extern_function.name, nd->token->mod_alias, nd);
            jik_diag_fatal_error_if(!res,
                                    JIK_STRING_NCAT("symbol \"",
                                                    nd->val_function.name,
                                                    "\" already defined in module \"",
                                                    nd->token->mod_alias,
                                                    "\""),
                                    "");
        }
        else if (nd->type == NODE_STMNT_DECLARE) {
            jik_semantic_reject_reserved_prefix(nd->val_declare.id->val_id.name,
                                                nd->val_declare.id->token);
            jik_semantic_reject_builtin_name_collision(
                sa, nd->val_declare.id->val_id.name, nd->val_declare.id->token);
            if (nd->val_declare.global) {
                bool res = jik_scope_add_global_symbol(
                    nd->val_declare.id->val_id.name, nd->token->mod_alias, nd->val_declare.expr);
                jik_diag_fatal_error_if(!res,
                                        JIK_STRING_NCAT("symbol \"",
                                                        nd->val_declare.id->val_id.name,
                                                        "\" already defined in module \"",
                                                        nd->token->mod_alias,
                                                        "\""),
                                        "");
            }
            else {
                // No shadowing of globals
                JikNode *gs = jik_scope_get_global_symbol(nd->val_declare.id->val_id.name,
                                                          nd->token->mod_alias);
                jik_diag_fatal_error_if(gs,
                                        "shadowing of global symbols not allowed",
                                        jik_token_to_text(nd->val_declare.id->token));

                JikNode *ls =
                    jik_scope_get_local_symbol(nd->context, nd->val_declare.id->val_id.name);
                jik_diag_fatal_error_if(
                    ls,
                    JIK_STRING_NCAT("symbol already declared: ", nd->val_declare.id->val_id.name),
                    jik_token_to_text(nd->token));
                jik_scope_add_local_symbol(
                    nd->context, nd->val_declare.id->val_id.name, nd->val_declare.id);
            }
        }
        else if (nd->type == NODE_STMNT_ASSIGN) {
            JikNode *gs =
                jik_scope_get_global_symbol(nd->val_assign.id->val_id.name, nd->token->mod_alias);
            jik_diag_fatal_error_if(
                gs, "global symbols are immutable", jik_token_to_text(nd->val_assign.id->token));
            JikNode *ls = jik_scope_get_local_symbol(nd->context, nd->val_assign.id->val_id.name);
            jik_diag_fatal_error_if(
                !ls,
                JIK_STRING_NCAT("undefined symbol: ", nd->val_assign.id->val_id.name),
                jik_token_to_text(nd->token));
        }
    }

    // Add iteration symbols
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_LOOP_FOR) {
            jik_semantic_reject_reserved_prefix(nd->val_for.var_name->val_id.name,
                                                nd->val_for.var_name->token);
            jik_semantic_reject_builtin_name_collision(
                sa, nd->val_for.var_name->val_id.name, nd->val_for.var_name->token);
            JikNode *ls =
                jik_scope_get_local_symbol(nd->context, nd->val_for.var_name->val_id.name);
            jik_diag_fatal_error_if(
                ls,
                JIK_STRING_NCAT("symbol already declared: ", nd->val_for.var_name->val_id.name),
                jik_token_to_text(nd->val_for.var_name->token));
            jik_scope_add_local_symbol(
                nd->val_for.body->context, nd->val_for.var_name->val_id.name, nd->val_for.var_name);
        }
        if (nd->type == NODE_LOOP_FOR_IN) {
            jik_semantic_reject_reserved_prefix(nd->val_for_in.var_name->val_id.name,
                                                nd->val_for_in.var_name->token);
            jik_semantic_reject_builtin_name_collision(
                sa, nd->val_for_in.var_name->val_id.name, nd->val_for_in.var_name->token);
            JikNode *ls =
                jik_scope_get_local_symbol(nd->context, nd->val_for_in.var_name->val_id.name);
            jik_diag_fatal_error_if(
                ls,
                JIK_STRING_NCAT("symbol already declared: ", nd->val_for_in.var_name->val_id.name),
                jik_token_to_text(nd->val_for_in.var_name->token));
            jik_scope_add_local_symbol(nd->val_for_in.body->context,
                                       nd->val_for_in.var_name->val_id.name,
                                       nd->val_for_in.var_name);
        }
        else if (nd->type == NODE_LOOP_FOR_IN_DICT) {
            jik_semantic_reject_reserved_prefix(nd->val_for_in_dict.key_name->val_id.name,
                                                nd->val_for_in_dict.key_name->token);
            jik_semantic_reject_reserved_prefix(nd->val_for_in_dict.val_name->val_id.name,
                                                nd->val_for_in_dict.val_name->token);
            jik_semantic_reject_builtin_name_collision(
                sa, nd->val_for_in_dict.key_name->val_id.name, nd->val_for_in_dict.key_name->token);
            jik_semantic_reject_builtin_name_collision(
                sa, nd->val_for_in_dict.val_name->val_id.name, nd->val_for_in_dict.val_name->token);
            JikNode *ls =
                jik_scope_get_local_symbol(nd->context, nd->val_for_in_dict.key_name->val_id.name);
            jik_diag_fatal_error_if(ls,
                                    JIK_STRING_NCAT("symbol already declared: ",
                                                    nd->val_for_in_dict.key_name->val_id.name),
                                    jik_token_to_text(nd->val_for_in_dict.key_name->token));
            jik_scope_add_local_symbol(nd->val_for_in_dict.body->context,
                                       nd->val_for_in_dict.key_name->val_id.name,
                                       nd->val_for_in_dict.key_name);
            ls = jik_scope_get_local_symbol(nd->context, nd->val_for_in_dict.val_name->val_id.name);
            jik_diag_fatal_error_if(ls,
                                    JIK_STRING_NCAT("symbol already declared: ",
                                                    nd->val_for_in_dict.val_name->val_id.name),
                                    jik_token_to_text(nd->val_for_in_dict.val_name->token));
            jik_scope_add_local_symbol(nd->val_for_in_dict.body->context,
                                       nd->val_for_in_dict.val_name->val_id.name,
                                       nd->val_for_in_dict.val_name);
        }
        else if (nd->type == NODE_CASE) {
            if (!nd->val_case.variant->val_variant_new.init_expr ||
                nd->val_case.variant->val_variant_new.init_expr->type != NODE_EXPR_IDENTIFIER) {
                continue;
            }
            char *id = nd->val_case.variant->val_variant_new.init_expr->val_id.name;
            jik_semantic_reject_reserved_prefix(
                id, nd->val_case.variant->val_variant_new.init_expr->token);
            jik_semantic_reject_builtin_name_collision(
                sa, id, nd->val_case.variant->val_variant_new.init_expr->token);
            JikNode *ls = jik_scope_get_local_symbol(nd->context, id);
            jik_diag_fatal_error_if(ls,
                                    JIK_STRING_NCAT("symbol already declared: ",
                                                    nd->val_for_in_dict.key_name->val_id.name),
                                    jik_token_to_text(nd->val_for_in_dict.key_name->token));
            jik_scope_add_local_symbol(
                nd->val_case.body->context, id, nd->val_case.variant->val_variant_new.init_expr);
        }
    }

    // 2. Check symbol usages
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_IDENTIFIER) {
            // TODO: here, also identifier Node from Node.FOO will come in, may be a problem or at
            // best redundant
            JikNode *s = jik_scope_get_symbol(
                nd->context, nd->val_id.name, nd->val_id.mod_alias, nd->token->mod_alias);
            if (s == NULL) {
                jik_diag_fatal_error(jik_string_cat("undefined symbol: ", nd->val_id.name),
                                     jik_token_to_text(nd->token));
            }
            jik_semantic_ensure_module_used(nd);
        }
        else if (nd->type == NODE_EXPR_CALL) {
            JikNode *s = jik_scope_get_symbol(nd->context,
                                              nd->val_call.name->val_id.name,
                                              nd->val_call.name->val_id.mod_alias,
                                              nd->token->mod_alias);
            if (!s) {
                s = jik_scope_get_builtin_symbol(nd->val_call.name->val_id.name);
                jik_diag_fatal_error_if(!s,
                                        JIK_STRING_NCAT("function \"",
                                                        nd->val_call.name->val_id.name,
                                                        "\" not defined"),
                                        jik_token_to_text(nd->token));
                nd->val_call.builtin = true;
            }
            else {
                jik_semantic_ensure_module_used(nd->val_call.name);
                if (s->type == NODE_EXTERN_FUNCTION) {
                    nd->val_call.extern_name = s->val_extern_function.C_func_name;
                }
            }
        }
        else if (nd->type == NODE_EXPR_STRUCT_NEW) {
            JikNode *s = jik_scope_get_symbol(nd->context,
                                              nd->val_struct_new.name->val_id.name,
                                              nd->val_struct_new.name->val_id.mod_alias,
                                              nd->token->mod_alias);
            jik_diag_fatal_error_if(!s,
                                    JIK_STRING_NCAT("struct \"",
                                                    nd->val_struct_new.name->val_id.name,
                                                    "\" not defined"),
                                    jik_token_to_text(nd->token));
            jik_diag_fatal_error_if(s->type != NODE_STRUCT,
                                    JIK_STRING_NCAT("struct \"",
                                                    nd->val_struct_new.name->val_id.name,
                                                    "\" not defined"),
                                    jik_token_to_text(nd->token));
            jik_semantic_ensure_module_used(nd->val_struct_new.name);
            if (s->val_struct.is_extern) {
                jik_diag_fatal_error_if(!jik_tab_jik_node_is_empty(nd->val_struct_new.init_vals),
                                        "external structs cannot be instantiated with fields",
                                        jik_token_to_text(nd->token));
            }
            nd->val_struct_new.struct_node = s;
        }
        else if (nd->type == NODE_EXPR_VARIANT_NEW) {
            JikNode *s = jik_scope_get_symbol(nd->context,
                                              nd->val_variant_new.name->val_id.name,
                                              nd->val_variant_new.name->val_id.mod_alias,
                                              nd->token->mod_alias);
            jik_diag_fatal_error_if(!s,
                                    JIK_STRING_NCAT("variant \"",
                                                    nd->val_variant_new.name->val_id.name,
                                                    "\" not defined"),
                                    jik_token_to_text(nd->token));
            jik_diag_fatal_error_if(s->type != NODE_VARIANT,
                                    JIK_STRING_NCAT("variant \"",
                                                    nd->val_variant_new.name->val_id.name,
                                                    "\" not defined"),
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
            if (nd->val_member_access.node->type == NODE_EXPR_IDENTIFIER) {
                char    *mod_alias = nd->val_member_access.node->val_id.mod_alias
                                         ? nd->val_member_access.node->val_id.mod_alias
                                         : nd->val_member_access.node->token->mod_alias;
                JikNode *enum_node =
                    jik_scope_get_global_symbol(nd->val_member_access.node->val_id.name, mod_alias);
                // TODO: this is not so nice, needing to explicitly copy out this stuff
                if (enum_node && enum_node->type == NODE_ENUM) {
                    JikNode *enum_new_nd = jik_node_new_enum_new(
                        nd->val_member_access.member_name, nd->context, nd->token);
                    nd->type                    = enum_new_nd->type;
                    nd->jik_type                = enum_node->jik_type;
                    nd->val_enum_new.enumerator = enum_new_nd->val_enum_new.enumerator;
                    jik_diag_fatal_error_if(
                        !TabBool_get(enum_node->val_enum.enumerators,
                                     nd->val_member_access.member_name),
                        JIK_STRING_NCAT("unknown enumerator: ", nd->val_member_access.member_name),
                        jik_token_to_text(nd->token));
                }
                else if (enum_node && enum_node->type == NODE_VARIANT) {
                    JikNode *var_new_nd =
                        jik_node_new_variant_tag(nd->val_member_access.node,
                                                 nd->val_member_access.member_name,
                                                 nd->context,
                                                 nd->token);
                    nd->type                 = var_new_nd->type;
                    nd->jik_type             = var_new_nd->jik_type;
                    nd->val_variant_tag.name = var_new_nd->val_variant_tag.name;
                    nd->val_variant_tag.tag  = var_new_nd->val_variant_tag.tag;
                }
            }
        }
        else if (nd->type == NODE_EXPR_LOCAL_REGION) {
            jik_diag_fatal_error_if(!nd->context->parent,
                                    "local region referenced from outside function",
                                    jik_token_to_text(nd->token));
        }
    }
}

static TabJikType *
get_field_types(TabJikNode *init_vals, JikType *obj_type)
{
    TabJikType     *field_types = TabJikType_new();
    TabJikNode_iter it          = TabJikNode_iter_new(init_vals);
    TabJikNode_item item;
    while (TabJikNode_iter_next(&it, &item)) {
        if (item.value->jik_type->name == TYPE_UNKNOWN) {
            return NULL;
        }
        TabJikType_set(field_types, item.key, item.value->jik_type);
    }
    return field_types;
}

static void
jik_semantic_infer_type(JikSemanticAnalyzer *sa, JikNode *nd);

static void
infer_type_builtin_pop(JikSemanticAnalyzer *sa, JikNode *nd)
{
    // TODO: assert len args is 1
    JikNode *obj = VecJikNode_get(nd->val_call.args, 0);
    // jik_semantic_infer_type(sa, obj);
    if (jik_type_is_inferred(obj->jik_type)) {
        if (obj->jik_type->name == TYPE_VECTOR) {
            nd->jik_type = obj->jik_type->val_vec.elem_type;
        }
        else {
            jik_diag_fatal_error_if(obj->jik_type->name != TYPE_VECTOR,
                                    "expected vector",
                                    jik_token_to_text(obj->token));
        }
    }
}

static void
jik_semantic_apply_option_context(JikNode *expr, JikType *expected_type)
{
    if (!expr || !expected_type) {
        return;
    }
    if (expected_type->name == TYPE_OPTION && expr->type == NODE_EXPR_OPTION_NONE) {
        expr->jik_type = expected_type;
    }
    else if (expr->type == NODE_EXPR_TERNARY) {
        jik_semantic_apply_option_context(expr->val_ternary.expr_if, expected_type);
        jik_semantic_apply_option_context(expr->val_ternary.expr_else, expected_type);
    }
    else if (expected_type->name == TYPE_VECTOR && expr->type == NODE_EXPR_VECTOR) {
        if (expr->val_vector.init_elems) {
            size_t n = VecJikNode_size(expr->val_vector.init_elems);
            for (size_t i = 0; i < n; i++) {
                jik_semantic_apply_option_context(VecJikNode_get(expr->val_vector.init_elems, i),
                                                  expected_type->val_vec.elem_type);
            }
        }
        else if (expr->val_vector.elem_expr) {
            jik_semantic_apply_option_context(expr->val_vector.elem_expr,
                                              expected_type->val_vec.elem_type);
        }
    }
    else if (expected_type->name == TYPE_DICT && expr->type == NODE_EXPR_DICT) {
        if (expr->val_dict.init_values) {
            size_t n = VecJikNode_size(expr->val_dict.init_values);
            for (size_t i = 0; i < n; i++) {
                jik_semantic_apply_option_context(VecJikNode_get(expr->val_dict.init_values, i),
                                                  expected_type->val_dict.elem_type);
            }
        }
        else if (expr->val_dict.elem_expr) {
            jik_semantic_apply_option_context(expr->val_dict.elem_expr,
                                              expected_type->val_dict.elem_type);
        }
    }
}

static JikNode *
jik_get_default_initializer_for_type_desc(JikSemanticAnalyzer *sa, JikNode *nd);

static char *
jik_get_composite_name(JikNode *nd)
{
    assert(nd->type == NODE_STRUCT || nd->type == NODE_VARIANT);
    return nd->type == NODE_STRUCT ? nd->val_struct.name : nd->val_variant.name;
}

static char *
jik_get_composite_kind(JikNode *nd)
{
    assert(nd->type == NODE_STRUCT || nd->type == NODE_VARIANT);
    return nd->type == NODE_STRUCT ? "struct" : "variant";
}

static char *
jik_get_composite_key(JikNode *nd)
{
    return JIK_STRING_NCAT(nd->token->mod_alias, "::", jik_get_composite_name(nd));
}

static JikNode *
jik_resolve_type_desc_composite(JikNode *td)
{
    assert(td->type == NODE_TYPE_DESC);
    if (td->val_type_desc.kind != TYPE_UNKNOWN || !td->val_type_desc.name) {
        return NULL;
    }
    char    *mod_alias = td->val_type_desc.name->val_id.mod_alias
                             ? td->val_type_desc.name->val_id.mod_alias
                             : td->token->mod_alias;
    JikNode *sym = jik_scope_get_global_symbol(td->val_type_desc.name->val_id.name, mod_alias);
    if (!sym) {
        return NULL;
    }
    if (sym->type == NODE_STRUCT || sym->type == NODE_VARIANT) {
        return sym;
    }
    return NULL;
}

static bool
jik_type_desc_has_non_option_cycle_to(JikNode *td, JikNode *target, TabBool *visiting)
{
    assert(td->type == NODE_TYPE_DESC);
    if (td->val_type_desc.kind == TYPE_OPTION) {
        return false;
    }
    if (td->val_type_desc.kind == TYPE_VECTOR || td->val_type_desc.kind == TYPE_DICT) {
        return jik_type_desc_has_non_option_cycle_to(td->val_type_desc.desc, target, visiting);
    }

    JikNode *next = jik_resolve_type_desc_composite(td);
    if (!next) {
        return false;
    }
    if (next == target) {
        return true;
    }

    char *key = jik_get_composite_key(next);
    if (TabBool_get(visiting, key)) {
        return false;
    }
    TabBool_set(visiting, key, true);

    TabJikNode *type_descs =
        next->type == NODE_STRUCT ? next->val_struct.type_descs : next->val_variant.type_descs;
    TabJikNode_iter it = TabJikNode_iter_new(type_descs);
    TabJikNode_item item;
    while (TabJikNode_iter_next(&it, &item)) {
        if (jik_type_desc_has_non_option_cycle_to(item.value, target, visiting)) {
            return true;
        }
    }
    return false;
}

static void
jik_semantic_check_recursive_composites(JikSemanticAnalyzer *sa)
{
    VecJikNode_iter it = VecJikNode_iter_new(sa->nodes);
    JikNode        *nd;
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type != NODE_STRUCT && nd->type != NODE_VARIANT) {
            continue;
        }

        TabJikNode *type_descs =
            nd->type == NODE_STRUCT ? nd->val_struct.type_descs : nd->val_variant.type_descs;
        TabJikNode_iter fields = TabJikNode_iter_new(type_descs);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&fields, &item)) {
            TabBool *visiting = TabBool_new();
            TabBool_set(visiting, jik_get_composite_key(nd), true);
            if (!jik_type_desc_has_non_option_cycle_to(item.value, nd, visiting)) {
                continue;
            }
            jik_diag_fatal_error(
                JIK_STRING_NCAT("illegal recursive ",
                                jik_get_composite_kind(nd),
                                " definition: member \"",
                                item.key,
                                "\" creates a recursive cycle without Option; use Option[...]."),
                jik_token_to_text(item.value->token));
        }
    }
}

static void
infer_unop(JikNode *nd)
{
    // TODO: entire function can be refactored and simplified
    if (!jik_node_is_type_inferred(nd->val_unop.expr))
        return;
    if (strcmp(nd->val_unop.val, "not") == 0) {
        if (!jik_node_is_type_one_of(nd->val_unop.expr, (JikTypeName[]){TYPE_BOOL, TYPE_NOTYPE}))
            jik_diag_fatal_error("unsupported type for unop", jik_token_to_text(nd->token));
        nd->jik_type = &JIK_TYPE_BOOL;
    }
    else if (strcmp(nd->val_unop.val, "-") == 0) {
        if (!jik_node_is_type_one_of(nd->val_unop.expr,
                                     (JikTypeName[]){TYPE_INTEGER, TYPE_FLOAT, TYPE_NOTYPE}))
            jik_diag_fatal_error("unsupported type for unop", jik_token_to_text(nd->token));
        if (nd->val_unop.expr->jik_type->name == TYPE_INTEGER) {
            nd->jik_type = &JIK_TYPE_INT;
        }
        else if (nd->val_unop.expr->jik_type->name == TYPE_FLOAT) {
            nd->jik_type = &JIK_TYPE_FLOAT;
        }
    }
    else
        jik_diag_fatal_error("dont know how to infer unop type", jik_token_to_text(nd->token));
}

static bool
binop_operands_are_of_type(JikNode *nd, JikTypeName names[])
{
    return jik_node_is_type_one_of(nd->val_binop.left, names) &&
           jik_node_is_type_one_of(nd->val_binop.right, names);
}

static bool
jik_types_are_numeric_pair(JikType *t1, JikType *t2)
{
    bool t1_numeric = t1->name == TYPE_INTEGER || t1->name == TYPE_FLOAT;
    bool t2_numeric = t2->name == TYPE_INTEGER || t2->name == TYPE_FLOAT;
    return t1_numeric && t2_numeric;
}

static void
infer_ternary(JikNode *nd)
{
    JikNode *expr_if   = nd->val_ternary.expr_if;
    JikNode *expr_else = nd->val_ternary.expr_else;

    if (!jik_node_is_type_inferred(expr_if) || !jik_node_is_type_inferred(expr_else)) {
        return;
    }

    if (expr_if->type == NODE_EXPR_OPTION_NONE && expr_else->jik_type->name == TYPE_OPTION) {
        expr_if->jik_type = expr_else->jik_type;
        nd->jik_type      = expr_else->jik_type;
        return;
    }
    if (expr_else->type == NODE_EXPR_OPTION_NONE && expr_if->jik_type->name == TYPE_OPTION) {
        expr_else->jik_type = expr_if->jik_type;
        nd->jik_type        = expr_if->jik_type;
        return;
    }
    if (jik_type_equal(expr_if->jik_type, expr_else->jik_type)) {
        nd->jik_type = expr_if->jik_type;
        return;
    }
    if (jik_types_are_numeric_pair(expr_if->jik_type, expr_else->jik_type)) {
        nd->jik_type =
            expr_if->jik_type->name == TYPE_FLOAT || expr_else->jik_type->name == TYPE_FLOAT
                ? &JIK_TYPE_FLOAT
                : &JIK_TYPE_INT;
        return;
    }

    jik_diag_fatal_error("ternary branches have incompatible types", jik_token_to_text(nd->token));
}

static void
infer_binop(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_BINOP);
    // TODO: entire function can be refactored and simplified
    if (!jik_node_is_type_inferred(nd->val_binop.left) ||
        !jik_node_is_type_inferred(nd->val_binop.right))
        return;
    if (strcmp(nd->val_binop.val, "+") == 0 || strcmp(nd->val_binop.val, "-") == 0 ||
        strcmp(nd->val_binop.val, "*") == 0 || strcmp(nd->val_binop.val, "/") == 0) {
        // TODO: simpligy this to numeric types or similar
        if (!binop_operands_are_of_type(nd,
                                        (JikTypeName[]){TYPE_INTEGER, TYPE_FLOAT, TYPE_NOTYPE})) {
            jik_diag_fatal_error("unsupported type for binop", jik_token_to_text(nd->token));
        }
        if (nd->val_binop.left->jik_type == &JIK_TYPE_FLOAT ||
            nd->val_binop.right->jik_type == &JIK_TYPE_FLOAT) {
            nd->jik_type = &JIK_TYPE_FLOAT;
        }
        else {
            nd->jik_type = &JIK_TYPE_INT;
        }
    }
    else if (strcmp(nd->val_binop.val, "%") == 0) {
        if (!binop_operands_are_of_type(nd, (JikTypeName[]){TYPE_INTEGER, TYPE_NOTYPE})) {
            jik_diag_fatal_error("unsupported type for binop", jik_token_to_text(nd->token));
        }
        nd->jik_type = &JIK_TYPE_INT;
    }
    else if (strcmp(nd->val_binop.val, "<") == 0 || strcmp(nd->val_binop.val, ">") == 0 ||
             strcmp(nd->val_binop.val, ">=") == 0 || strcmp(nd->val_binop.val, "<=") == 0) {
        if (!binop_operands_are_of_type(nd,
                                        (JikTypeName[]){TYPE_INTEGER, TYPE_FLOAT, TYPE_NOTYPE})) {
            jik_diag_fatal_error("unsupported type for binop", jik_token_to_text(nd->token));
        }
        nd->jik_type = &JIK_TYPE_BOOL;
    }
    else if (strcmp(nd->val_binop.val, "==") == 0 || strcmp(nd->val_binop.val, "!=") == 0) {
        if (!binop_operands_are_of_type(nd,
                                        (JikTypeName[]){TYPE_INTEGER,
                                                        TYPE_FLOAT,
                                                        TYPE_STRING,
                                                        TYPE_BOOL,
                                                        TYPE_CHAR,
                                                        TYPE_ENUM,
                                                        TYPE_NOTYPE}))
            jik_diag_fatal_error("unsupported type for binop", jik_token_to_text(nd->token));
        nd->jik_type = &JIK_TYPE_BOOL;
        if (binop_operands_are_of_type(nd, (JikTypeName[]){TYPE_INTEGER, TYPE_FLOAT, TYPE_NOTYPE}))
            return;
        if (!jik_node_types_equal(nd->val_binop.left, nd->val_binop.right)) {
            jik_diag_fatal_error("binary operator between different types",
                                 jik_token_to_text(nd->token));
        }
    }
    else if (strcmp(nd->val_binop.val, "and") == 0 || strcmp(nd->val_binop.val, "or") == 0) {
        if (!binop_operands_are_of_type(nd, (JikTypeName[]){TYPE_BOOL, TYPE_NOTYPE})) {
            jik_diag_fatal_error("unsupported type for binop", jik_token_to_text(nd->token));
        }
        nd->jik_type = &JIK_TYPE_BOOL;
    }
    else
        jik_diag_fatal_error("dont know how to infer type", jik_token_to_text(nd->token));
}

static void
jik_semantic_infer_type(JikSemanticAnalyzer *sa, JikNode *nd)
{
    if (jik_node_is_type_inferred(nd))
        return;
    if (nd->type == NODE_EXPR_GROUPING) {
        jik_semantic_infer_type(sa, nd->val_grouping);
        if (jik_type_is_inferred(nd->val_grouping->jik_type)) {
            nd->jik_type = nd->val_grouping->jik_type;
        }
    }
    else if (nd->type == NODE_EXPR_BINOP) {
        jik_semantic_infer_type(sa, nd->val_binop.left);
        jik_semantic_infer_type(sa, nd->val_binop.right);
        infer_binop(nd);
    }
    else if (nd->type == NODE_EXPR_UNOP) {
        jik_semantic_infer_type(sa, nd->val_unop.expr);
        infer_unop(nd);
    }
    else if (nd->type == NODE_EXPR_OPTION_SOME) {
        jik_semantic_infer_type(sa, nd->val_option_some.expr);
        if (jik_node_is_type_inferred(nd->val_option_some.expr)) {
            nd->jik_type = jik_type_new_option(nd->val_option_some.expr->jik_type);
        }
    }
    else if (nd->type == NODE_EXPR_OPTION_NONE) {
        return;
    }
    else if (nd->type == NODE_EXPR_OPTION_IS) {
        jik_semantic_infer_type(sa, nd->val_option_is.expr);
        nd->jik_type = &JIK_TYPE_BOOL;
    }
    else if (nd->type == NODE_EXPR_OPTION_UNWRAP) {
        jik_semantic_infer_type(sa, nd->val_option_unwrap.expr);
        if (jik_node_is_type_inferred(nd->val_option_unwrap.expr) &&
            nd->val_option_unwrap.expr->jik_type->name == TYPE_OPTION) {
            nd->jik_type = nd->val_option_unwrap.expr->jik_type->val_option.elem_type;
        }
    }
    else if (nd->type == NODE_EXPR_IDENTIFIER) {
        JikNode *n = jik_scope_get_symbol(
            nd->context, nd->val_id.name, nd->val_id.mod_alias, nd->token->mod_alias);
        assert(n);
        if (jik_node_is_type_inferred(n)) {
            nd->jik_type = n->jik_type;
        }
    }
    else if (nd->type == NODE_EXPR_CALL) {
        JikNode *func = jik_scope_get_function(nd->context,
                                               nd->val_call.name->val_id.name,
                                               nd->val_call.name->val_id.mod_alias,
                                               nd->token->mod_alias);
        if (nd->val_call.builtin) {
            if (jik_type_is_inferred(func->jik_type->val_func.ret_type)) {
                nd->jik_type = func->jik_type->val_func.ret_type;
            }
            else if (strcmp(nd->val_call.name->val_id.name, "pop") == 0) {
                infer_type_builtin_pop(sa, nd);
            }
            else {
                return;
            }
        }
        else if (jik_type_is_inferred(func->jik_type->val_func.ret_type)) {
            nd->jik_type = func->jik_type->val_func.ret_type;
        }
    }
    else if (nd->type == NODE_EXPR_TERNARY) {
        jik_semantic_infer_type(sa, nd->val_ternary.condition);
        jik_semantic_infer_type(sa, nd->val_ternary.expr_if);
        jik_semantic_infer_type(sa, nd->val_ternary.expr_else);
        infer_ternary(nd);
    }
    else if (nd->type == NODE_FUNCTION || nd->type == NODE_BUILTIN_FUNCTION ||
             nd->type == NODE_EXTERN_FUNCTION) {
        return;
    }
    else if (nd->type == NODE_STRUCT) {
        if (nd->val_struct.inferring) {
            return;
        }
        nd->val_struct.inferring = true;
        for (size_t i = 0; i < VecString_size(nd->val_struct.field_order); i++) {
            char     *field_name = VecString_get(nd->val_struct.field_order, i);
            JikNode **type_desc  = TabJikNode_get(nd->val_struct.type_descs, field_name);
            assert(type_desc);
            if (TabJikNode_get(nd->val_struct.init_vals, field_name)) {
                continue;
            }
            JikNode *expr = jik_get_default_initializer_for_type_desc(sa, *type_desc);
            assert(expr);
            jik_semantic_infer_type(sa, expr);
            TabJikNode_set(nd->val_struct.init_vals, field_name, expr);
        }
        nd->jik_type->val_struct.field_types =
            get_field_types(nd->val_struct.init_vals, nd->jik_type);
        sa->needs_recollect      = true;
        nd->val_struct.inferring = false;
    }
    else if (nd->type == NODE_VARIANT) {
        if (nd->val_variant.inferring) {
            return;
        }
        nd->val_variant.inferring = true;
        for (size_t i = 0; i < VecString_size(nd->val_variant.member_order); i++) {
            char     *field_name = VecString_get(nd->val_variant.member_order, i);
            JikNode **type_desc  = TabJikNode_get(nd->val_variant.type_descs, field_name);
            assert(type_desc);
            if (TabJikNode_get(nd->val_variant.init_vals, field_name)) {
                continue;
            }
            JikNode *expr = jik_get_default_initializer_for_type_desc(sa, *type_desc);
            assert(expr);
            jik_semantic_infer_type(sa, expr);
            TabJikNode_set(nd->val_variant.init_vals, field_name, expr);
        }

        nd->jik_type->val_variant.variant_types =
            get_field_types(nd->val_variant.init_vals, nd->jik_type);
        sa->needs_recollect       = true;
        nd->val_variant.inferring = false;
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        JikNode *st  = jik_scope_get_symbol(nd->context,
                                           nd->val_struct_new.name->val_id.name,
                                           nd->val_struct_new.name->val_id.mod_alias,
                                           nd->token->mod_alias);
        if (st->val_struct.is_extern) {
            JikNode *call = jik_make_init_call_for_extern_struct(
                st->jik_type,
                nd->val_struct_new.name->val_id.mod_alias,
                nd->context,
                nd->token);
            *nd                 = *call;
            sa->needs_recollect = true;
            return;
        }
        nd->jik_type = st->jik_type;
    }
    else if (nd->type == NODE_EXPR_VARIANT_NEW) {
        JikNode *st  = jik_scope_get_symbol(nd->context,
                                           nd->val_variant_new.name->val_id.name,
                                           nd->val_variant_new.name->val_id.mod_alias,
                                           nd->token->mod_alias);
        nd->jik_type = st->jik_type;
        if (st->jik_type->name == TYPE_VARIANT) {
            jik_ensure_valid_variant_tag(nd);
            JikNode **init_expr = TabJikNode_get(
                nd->val_variant_new.variant_node->val_variant.init_vals, nd->val_variant_new.tag);
            if (!init_expr) {
                return;
            }

            if (!nd->val_variant_new.init_expr) {
                nd->val_variant_new.init_expr = *init_expr;
                sa->needs_recollect           = true;
                if (jik_node_is_allocated_literal(nd->val_variant_new.init_expr)) {
                    // TODO: we set alloc specs in different places, this is messy
                    jik_set_alloc_spec(nd->val_variant_new.init_expr,
                                       nd->val_variant_new.alloc_spec);
                }
            }
        }
    }
    else if (nd->type == NODE_EXPR_VARIANT_TAG_CHECK) {
        nd->jik_type = &JIK_TYPE_BOOL;
    }
    else if (nd->type == NODE_VARIANT_TAG) {
        return;
    }
    else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
        jik_semantic_infer_type(sa, nd->val_member_access.node);
        if (jik_node_is_type_inferred(nd->val_member_access.node)) {
            if (nd->val_member_access.node->jik_type->name == TYPE_STRUCT) {
                JikType **member_type =
                    TabJikType_get(nd->val_member_access.node->jik_type->val_struct.field_types,
                                   nd->val_member_access.member_name);

                jik_diag_fatal_error_if(
                    member_type == NULL, "unknown field name", jik_token_to_text(nd->token));
                // if (!member_type) {
                //     return;
                // }
                nd->jik_type = *member_type;
            }
            else {
                jik_diag_fatal_error("member access not supported on this type",
                                     jik_token_to_text(nd->token));
            }
        }
    }
    else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
        jik_semantic_infer_type(sa, nd->val_subscript_get.node);
        if (jik_node_is_type_inferred(nd->val_subscript_get.node)) {
            jik_diag_fatal_error_if(
                !jik_type_is_subscriptable(nd->val_subscript_get.node->jik_type),
                "unsubscriptable type",
                jik_token_to_text(nd->val_subscript_get.node->token));
            if (nd->val_subscript_get.node->jik_type->name == TYPE_VECTOR) {
                nd->jik_type = nd->val_subscript_get.node->jik_type->val_vec.elem_type;
            }
            else if (nd->val_subscript_get.node->jik_type->name == TYPE_STRING) {
                nd->jik_type = &JIK_TYPE_CHAR;
            }
            else if (nd->val_subscript_get.node->jik_type->name == TYPE_DICT) {
                nd->jik_type =
                    jik_type_new_option(nd->val_subscript_get.node->jik_type->val_dict.elem_type);
            }
            else if (nd->val_subscript_get.expr->type == NODE_VARIANT_TAG) {
                assert(nd->val_subscript_get.node->jik_type->name == TYPE_VARIANT);
                JikNode *variant_nd = jik_scope_get_symbol(
                    nd->context,
                    nd->val_subscript_get.expr->val_variant_tag.name->val_id.name,
                    nd->val_subscript_get.expr->val_variant_tag.name->val_id.mod_alias,
                    nd->token->mod_alias);
                jik_semantic_ensure_module_used(nd->val_subscript_get.expr->val_variant_tag.name);
                jik_diag_fatal_error_if(
                    !variant_nd,
                    JIK_STRING_NCAT("variant \"",
                                    nd->val_subscript_get.expr->val_variant_tag.name->val_id.name,
                                    "\" not defined"),
                    jik_token_to_text(nd->val_subscript_get.expr->token));
                assert(variant_nd->jik_type->val_variant.variant_types);
                JikType **t = TabJikType_get(variant_nd->jik_type->val_variant.variant_types,
                                             nd->val_subscript_get.expr->val_variant_tag.tag);
                jik_diag_fatal_error_if(
                    !t,
                    JIK_STRING_NCAT("unknown variant tag \"",
                                    nd->val_subscript_get.expr->val_variant_tag.tag,
                                    "\""),
                    jik_token_to_text(nd->val_subscript_get.expr->token));
                nd->jik_type = *t;
            }
            else {
                jik_diag_fatal_error("internal error: unhandled subscriptable type", "");
            }
        }
    }
    else if (nd->type == NODE_EXPR_VECTOR) {
        if (nd->val_vector.init_elems) {
            size_t n = VecJikNode_size(nd->val_vector.init_elems);
            jik_diag_fatal_error_if(
                n == 0, "internal error: empty vector literal escaped parser rejection", "");
            for (size_t i = 0; i < n; i++) {
                jik_semantic_infer_type(sa, VecJikNode_get(nd->val_vector.init_elems, i));
            }
            JikNode *first = VecJikNode_get(nd->val_vector.init_elems, 0);
            if (jik_node_is_type_inferred(first)) {
                nd->val_vector.elem_expr = first;
                nd->jik_type             = jik_type_new_vector(nd->val_vector.elem_expr->jik_type);
            }
        }
        else {
            jik_semantic_infer_type(sa, nd->val_vector.size_expr);
            jik_semantic_infer_type(sa, nd->val_vector.elem_expr);
            if (jik_node_is_type_inferred(nd->val_vector.elem_expr)) {
                nd->jik_type = jik_type_new_vector(nd->val_vector.elem_expr->jik_type);
            }
        }
    }
    else if (nd->type == NODE_EXPR_DICT) {
        if (nd->val_dict.elem_expr) {
            jik_semantic_infer_type(sa, nd->val_dict.elem_expr);
            if (jik_node_is_type_inferred(nd->val_dict.elem_expr)) {
                nd->jik_type = jik_type_new_dict(nd->val_dict.elem_expr->jik_type);
            }
        }
        else {
            jik_diag_fatal_error_if(
                VecJikNode_size(nd->val_dict.init_values) == 0,
                "internal error: empty dictionary literal escaped parser rejection",
                "");
            JikNode *first = VecJikNode_get(nd->val_dict.init_values, 0);
            jik_semantic_infer_type(sa, first);
            if (jik_node_is_type_inferred(first)) {
                nd->jik_type           = jik_type_new_dict(first->jik_type);
                nd->val_dict.elem_expr = first;
            }
        }
    }
    else if (nd->type == NODE_EXPR_MUST) {
        jik_semantic_infer_type(sa, nd->val_must.expr);
        if (jik_node_is_type_inferred(nd->val_must.expr)) {
            nd->jik_type = nd->val_must.expr->jik_type;
        }
    }
    else {
        jik_node_print(nd, 0);
        jik_diag_fatal_error("no idea how to infer type", "");
    }
}

static void
jik_semantic_traverse_ast(JikSemanticAnalyzer *sa)
{
    JikNode        *nd;
    VecJikNode_iter it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (!jik_node_is_type_inferred(nd)) {
            jik_semantic_infer_type(sa, nd);
        }
        else if (nd->type == NODE_LOOP_FOR_IN &&
                 !jik_node_is_type_inferred(nd->val_for_in.var_name)) {
            if (jik_node_is_type_inferred(nd->val_for_in.container_expr)) {
                // this check needs to go here because we need an iterable to extract the type out
                // of it
                jik_diag_fatal_error_if(
                    !jik_type_is_iterable(nd->val_for_in.container_expr->jik_type),
                    "expected iterable",
                    jik_token_to_text(nd->val_for_in.container_expr->token));
                nd->val_for_in.var_name->jik_type =
                    jik_type_get_iterable_elem_type(nd->val_for_in.container_expr->jik_type);
            }
        }
        else if (nd->type == NODE_LOOP_FOR_IN_DICT &&
                 (!jik_node_is_type_inferred(nd->val_for_in_dict.key_name) ||
                  !jik_node_is_type_inferred(nd->val_for_in_dict.val_name))) {
            if (jik_node_is_type_inferred(nd->val_for_in_dict.dict_expr)) {
                // this check needs to go here because we need a dict to extract the type out
                // of it
                jik_diag_fatal_error_if(nd->val_for_in_dict.dict_expr->jik_type->name != TYPE_DICT,
                                        "expected dictionary",
                                        jik_token_to_text(nd->val_for_in_dict.dict_expr->token));
                nd->val_for_in_dict.key_name->jik_type = &JIK_TYPE_STRING;
                nd->val_for_in_dict.val_name->jik_type =
                    jik_type_get_iterable_elem_type(nd->val_for_in_dict.dict_expr->jik_type);
            }
        }
        else if (nd->type == NODE_STMNT_MATCH && jik_node_is_type_inferred(nd->val_match.expr)) {
            jik_diag_fatal_error_if(nd->val_match.expr->jik_type->name != TYPE_VARIANT,
                                    "expected variant",
                                    jik_token_to_text(nd->val_match.expr->token));
            JikType        *variant_type = nd->val_match.expr->jik_type;
            VecJikNode_iter case_it      = VecJikNode_iter_new(nd->val_match.cases);
            JikNode        *case_nd;
            while (VecJikNode_iter_next(&case_it, &case_nd)) {
                JikNode *s = jik_scope_get_symbol(
                    nd->context,
                    case_nd->val_case.variant->val_variant_new.name->val_id.name,
                    case_nd->val_case.variant->val_variant_new.name->val_id.mod_alias,
                    nd->token->mod_alias);
                jik_diag_fatal_error_if(!jik_type_equal(s->jik_type, variant_type),
                                        JIK_STRING_NCAT("wrong variant type: expected ",
                                                        jik_type_pretty_name(variant_type)),
                                        jik_token_to_text(case_nd->val_case.variant->token));
                assert(variant_type->val_variant.variant_types);
                JikType **expr_type =
                    TabJikType_get(variant_type->val_variant.variant_types,
                                   case_nd->val_case.variant->val_variant_new.tag);
                assert(expr_type);
                // jik_diag_fatal_error_if(!expr_type, JIK_STRING_NCAT("wrong variant type: expected
                // ", jik_type_pretty_name(variant_type)),
                // jik_token_to_text(case_nd->val_case.variant->token));
                if (jik_type_is_inferred(*expr_type) &&
                    case_nd->val_case.variant->val_variant_new.init_expr &&
                    case_nd->val_case.variant->val_variant_new.init_expr->type ==
                        NODE_EXPR_IDENTIFIER) {
                    JikNode *s = jik_scope_get_local_symbol(
                        case_nd->val_case.body->context,
                        case_nd->val_case.variant->val_variant_new.init_expr->val_id.name);
                    s->jik_type = *expr_type;
                }
            }
        }
    }

    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_CALL) {
            if (nd->val_call.builtin && strcmp(nd->val_call.name->val_id.name, "push") == 0 &&
                VecJikNode_size(nd->val_call.args) == 2) {
                JikNode *vec = VecJikNode_get(nd->val_call.args, 0);
                JikNode *arg = VecJikNode_get(nd->val_call.args, 1);
                jik_semantic_infer_type(sa, vec);
                if (jik_node_is_type_inferred(vec) && vec->jik_type->name == TYPE_VECTOR) {
                    jik_semantic_apply_option_context(arg, vec->jik_type->val_vec.elem_type);
                }
            }
            JikNode *func = jik_scope_get_function(nd->context,
                                                   nd->val_call.name->val_id.name,
                                                   nd->val_call.name->val_id.mod_alias,
                                                   nd->token->mod_alias);
            assert(func);
            size_t n_args   = VecJikNode_size(nd->val_call.args);
            size_t n_params = VecJikType_size(func->jik_type->val_func.param_types);
            size_t n        = n_args < n_params ? n_args : n_params;
            for (size_t i = 0; i < n; i++) {
                JikNode *arg        = VecJikNode_get(nd->val_call.args, i);
                JikType *param_type = VecJikType_get(func->jik_type->val_func.param_types, i);
                jik_semantic_apply_option_context(arg, param_type);
            }
        }
        if (nd->type == NODE_STMNT_ASSIGN) {
            JikNode *target = jik_scope_get_symbol(nd->context,
                                                   nd->val_assign.id->val_id.name,
                                                   nd->val_assign.id->val_id.mod_alias,
                                                   nd->val_assign.id->token->mod_alias);
            if (target && jik_node_is_type_inferred(target)) {
                jik_semantic_apply_option_context(nd->val_assign.expr, target->jik_type);
            }
            jik_semantic_infer_type(sa, nd->val_assign.expr);
            if (!jik_node_is_type_inferred(nd->val_assign.expr)) {
                continue;
            }
            // We mix type checks here a bit, because we the first assigned type is the defined
            // type/ If we run the type checker afterwards, this wont be true

            // ??TODOY: this is not good, as this may not happen, depending o the number of
            // traversals for type inference. So, ALL such checks need to be moved to type check!!!
            JikNode *n = target;
            if (jik_node_is_type_inferred(n) &&
                !jik_type_equal(n->jik_type, nd->val_assign.expr->jik_type)) {
                char *details = JIK_STRING_NCAT("cannot assign ",
                                                jik_type_pretty_name(nd->val_assign.expr->jik_type),
                                                " to ",
                                                jik_type_pretty_name(n->jik_type),
                                                jik_token_to_text(nd->token));
                jik_diag_fatal_error("type mismatch", details);
            }
            // TODOY: may not be needed if we yield id node in collect nodes
            nd->val_assign.id->jik_type = nd->val_assign.expr->jik_type;
        }
        else if (nd->type == NODE_STMNT_DECLARE) {
            if (nd->val_declare.expr->type == NODE_PLACEHOLDER) {
                // This may be too early, but better than segfault for now
                jik_semantic_resolve_type_or_error(nd->val_declare.type_desc);
                JikNode *expr =
                    jik_get_default_initializer_for_type_desc(sa, nd->val_declare.type_desc);
                if (!expr) {
                    continue;
                }
                nd->val_declare.expr = expr;
                jik_scope_overwrite_local_symbol(
                    nd->context, nd->val_declare.id->val_id.name, expr);
                jik_semantic_infer_type(sa, nd->val_declare.expr);
                if (jik_node_is_type_inferred(nd->val_declare.expr)) {
                    nd->val_declare.id->jik_type = nd->val_declare.expr->jik_type;
                }
                sa->needs_recollect = true;
            }
            else if (nd->val_declare.type_desc->type == NODE_PLACEHOLDER) {
                jik_semantic_infer_type(sa, nd->val_declare.expr);
                if (jik_node_is_type_inferred(nd->val_declare.expr)) {
                    nd->val_declare.id->jik_type = nd->val_declare.expr->jik_type;
                }
            }
            else {
                JikType *decl_type = jik_semantic_resolve_type_or_error(nd->val_declare.type_desc);
                jik_semantic_apply_option_context(nd->val_declare.expr, decl_type);
                jik_semantic_infer_type(sa, nd->val_declare.expr);
                if (jik_node_is_type_inferred(nd->val_declare.expr)) {
                    nd->val_declare.id->jik_type = decl_type;
                }
            }
        }
        else if (nd->type == NODE_EXPR_STRUCT_NEW) {
            TabJikNode_iter it_fields = TabJikNode_iter_new(nd->val_struct_new.init_vals);
            TabJikNode_item item;
            while (TabJikNode_iter_next(&it_fields, &item)) {
                JikType **field_type =
                    TabJikType_get(nd->jik_type->val_struct.field_types, item.key);
                if (field_type) {
                    jik_semantic_apply_option_context(item.value, *field_type);
                }
            }
        }
        else if (nd->type == NODE_EXPR_VARIANT_NEW && nd->val_variant_new.variant_node) {
            JikType **field_type =
                TabJikType_get(nd->jik_type->val_variant.variant_types, nd->val_variant_new.tag);
            if (field_type && nd->val_variant_new.init_expr) {
                jik_semantic_apply_option_context(nd->val_variant_new.init_expr, *field_type);
            }
        }
        else if (nd->type == NODE_STMNT_MEMBER_SET) {
            JikNode *node = nd->val_member_set.node;
            if (!jik_node_is_type_inferred(node) || node->jik_type->name != TYPE_STRUCT) {
                continue;
            }
            JikType **field_type = TabJikType_get(node->jik_type->val_struct.field_types,
                                                  nd->val_member_set.member_name);
            if (field_type) {
                jik_semantic_apply_option_context(nd->val_member_set.expr, *field_type);
            }
        }
        else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
            JikNode *node = nd->val_subscript_set.node;
            if (!jik_node_is_type_inferred(node)) {
                continue;
            }
            if (node->jik_type->name == TYPE_VECTOR) {
                jik_semantic_apply_option_context(nd->val_subscript_set.expr,
                                                  node->jik_type->val_vec.elem_type);
            }
            else if (node->jik_type->name == TYPE_DICT) {
                jik_semantic_apply_option_context(nd->val_subscript_set.expr,
                                                  node->jik_type->val_dict.elem_type);
            }
            else if (node->jik_type->name == TYPE_VARIANT &&
                     nd->val_subscript_set.sub_expr->type == NODE_VARIANT_TAG) {
                JikType **active_type =
                    TabJikType_get(node->jik_type->val_variant.variant_types,
                                   nd->val_subscript_set.sub_expr->val_variant_tag.tag);
                if (active_type) {
                    jik_semantic_apply_option_context(nd->val_subscript_set.expr, *active_type);
                }
            }
        }
    }
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_STMNT_RETURN) {
            if (!nd->val_return.expr) {
                if (jik_type_is_inferred(
                        nd->val_return.parent_function->jik_type->val_func.ret_type) &&
                    !jik_type_equal(nd->val_return.parent_function->jik_type->val_func.ret_type,
                                    &JIK_TYPE_VOID)) {
                    char *details = JIK_STRING_NCAT(
                        "cannot return both ",
                        jik_type_pretty_name(&JIK_TYPE_VOID),
                        " and ",
                        jik_type_pretty_name(
                            nd->val_return.parent_function->jik_type->val_func.ret_type),
                        jik_token_to_text(nd->token));
                    jik_diag_fatal_error("type mismatch", details);
                }
                nd->val_return.parent_function->jik_type->val_func.ret_type = &JIK_TYPE_VOID;
            }
            else {
                jik_semantic_apply_option_context(
                    nd->val_return.expr,
                    nd->val_return.parent_function->jik_type->val_func.ret_type);
                jik_semantic_infer_type(sa, nd->val_return.expr);
            }
            if (nd->val_return.expr && jik_node_is_type_inferred(nd->val_return.expr)) {
                // TODO: this should be in type checking, but not clear if we can do it there, as
                // there may be multiple returns
                if (jik_node_is_type_inferred(nd->val_return.parent_function) &&
                    !jik_type_equal(nd->val_return.parent_function->jik_type->val_func.ret_type,
                                    nd->val_return.expr->jik_type)) {
                    char *details = JIK_STRING_NCAT(
                        "cannot return both ",
                        jik_type_pretty_name(nd->val_return.expr->jik_type),
                        " and ",
                        jik_type_pretty_name(
                            nd->val_return.parent_function->jik_type->val_func.ret_type),
                        jik_token_to_text(nd->token));
                    jik_diag_fatal_error("type mismatch", details);
                }
                nd->val_return.parent_function->jik_type->val_func.ret_type =
                    nd->val_return.expr->jik_type;
            }
        }
    }
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_CALL) {
            JikNode *func = jik_scope_get_function(nd->context,
                                                   nd->val_call.name->val_id.name,
                                                   nd->val_call.name->val_id.mod_alias,
                                                   nd->token->mod_alias);
            if (func->type == NODE_BUILTIN_FUNCTION || func->type == NODE_EXTERN_FUNCTION) {
                continue;
            }
            JikNode *arg;
            size_t   num_params = VecJikNode_size(func->val_function.params);
            size_t   num_args   = VecJikNode_size(nd->val_call.args);
            // if (num_params != num_args) {
            //     jik_diag_fatal_error(JIK_STRING_NCAT("function called with ",
            //                                          size_t_to_string(num_args),
            //                                          " arguments, expected ",
            //                                          size_t_to_string(num_params)),
            //                          jik_token_to_text(nd->token));
            // }
            for (size_t i = 0; i < num_params; i++) {
                if (i == num_args) {
                    break;
                }
                arg = VecJikNode_get(nd->val_call.args, i);
                if (jik_node_is_type_inferred(arg)) {
                    char    *param_name = VecJikNode_get(func->val_function.params, i)->val_id.name;
                    JikNode *arg_nd     = jik_scope_get_symbol(
                        func->val_function.body->context, param_name, NULL, func->token->mod_alias);
                    JikType *param_type = VecJikType_get(func->jik_type->val_func.param_types, i);
                    if (jik_type_is_inferred(param_type) &&
                        !jik_type_equal(param_type, arg->jik_type)) {
                        char *details = JIK_STRING_NCAT("cannot assign ",
                                                        jik_type_pretty_name(arg->jik_type),
                                                        " to ",
                                                        jik_type_pretty_name(param_type),
                                                        jik_token_to_text(arg->token));
                        jik_diag_fatal_error("type mismatch", details);
                    }
                    VecJikType_set(func->jik_type->val_func.param_types, i, arg->jik_type);
                    arg_nd->jik_type = arg->jik_type;
                }
            }
        }
    }
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_FUNCTION) {
            if (is_main_function(nd)) {
                continue;
            }
            if (!nd->val_function.has_return) {
                nd->jik_type->val_func.ret_type = &JIK_TYPE_VOID;
            }
        }
    }
}

static void
jik_semantic_collect_uninferred_nodes(JikSemanticAnalyzer *sa)
{
    VecJikNode_iter it;
    // uninferred must be empty when passed here!
    JikNode *nd;
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->jik_type->name == TYPE_NOTYPE || nd->type == NODE_BUILTIN_FUNCTION)
            continue;
        if (!jik_node_is_type_inferred(nd))
            VecJikNode_push(sa->uninferred, nd);
    }
}

JikType *
jik_semantic_resolve_type(JikNode *nd)
{
    assert(nd->type == NODE_TYPE_DESC);
    if (!nd->val_type_desc.desc) {
        JikNode *type_id = nd->val_type_desc.name;
        if (strcmp(type_id->val_id.name, "int") == 0) {
            return &JIK_TYPE_INT;
        }
        else if (strcmp(type_id->val_id.name, "double") == 0) {
            return &JIK_TYPE_FLOAT;
        }
        else if (strcmp(type_id->val_id.name, "void") == 0) {
            return &JIK_TYPE_VOID;
        }
        else if (strcmp(type_id->val_id.name, "String") == 0) {
            return &JIK_TYPE_STRING;
        }
        else if (strcmp(type_id->val_id.name, "bool") == 0) {
            return &JIK_TYPE_BOOL;
        }
        else if (strcmp(type_id->val_id.name, "char") == 0) {
            return &JIK_TYPE_CHAR;
        }
        else if (strcmp(type_id->val_id.name, "Region") == 0) {
            return &JIK_TYPE_REGION;
        }
        else if (strcmp(type_id->val_id.name, "Site") == 0) {
            return &JIK_TYPE_SITE;
        }
        else {
            JikNode *s = jik_scope_get_symbol(
                nd->context, type_id->val_id.name, type_id->val_id.mod_alias, nd->token->mod_alias);
            if (s) {
                return s->jik_type;
            }
            return NULL;
        }
    }
    else if (nd->val_type_desc.desc) {
        JikType *inner = jik_semantic_resolve_type(nd->val_type_desc.desc);
        if (!inner) {
            return NULL;
        }
        if (nd->val_type_desc.kind == TYPE_VECTOR) {
            return jik_type_new_vector(inner);
        }
        else if (nd->val_type_desc.kind == TYPE_DICT) {
            return jik_type_new_dict(inner);
        }
        else if (nd->val_type_desc.kind == TYPE_OPTION) {
            return jik_type_new_option(inner);
        }
        else {
            return NULL;
        }
    }
    else {
        return NULL;
    }
}

static char *
jik_semantic_type_desc_display_name(JikNode *nd)
{
    assert(nd->type == NODE_TYPE_DESC);
    if (nd->val_type_desc.name) {
        return nd->val_type_desc.name->val_id.name;
    }
    if (nd->val_type_desc.kind == TYPE_VECTOR) {
        return "Vec[...]";
    }
    if (nd->val_type_desc.kind == TYPE_DICT) {
        return "Dict[...]";
    }
    if (nd->val_type_desc.kind == TYPE_OPTION) {
        return "Option[...]";
    }
    return "???";
}

static JikType *
jik_semantic_resolve_type_or_error(JikNode *nd)
{
    JikType *t = jik_semantic_resolve_type(nd);
    jik_diag_fatal_error_if(
        !t,
        JIK_STRING_NCAT("could not resolve type \"", jik_semantic_type_desc_display_name(nd), "\""),
        jik_token_to_text(nd->token));
    return t;
}

static JikNode *
jik_make_init_call_for_extern_struct(JikType  *t,
                                     char     *mod_alias,
                                     JikScope *context,
                                     JikToken *token)
{
    if (!t->init_func) {
        jik_diag_fatal_error(
            JIK_STRING_NCAT("external struct type has no init function: ", jik_type_pretty_name(t)),
            jik_token_to_text(token));
    }

    JikNode *func      = t->init_func;
    mod_alias          = mod_alias ? mod_alias : func->token->mod_alias;
    JikNode *name      = jik_node_new_identifier(
        func->val_extern_function.name, mod_alias, context, token);
    VecJikNode *args = VecJikNode_new_empty();
    VecJikNode_push(args, jik_node_new_local_region(context, token));

    JikNode *call = jik_node_new_call(name, args, context, token);
    call->val_call.extern_name = func->val_extern_function.C_func_name;
    call->jik_type             = t;
    return call;
}

static JikNode *
jik_get_default_initializer_for_extern_struct(JikNode *type_desc, JikType *t)
{
    return jik_make_init_call_for_extern_struct(t,
                                               type_desc->val_type_desc.name->val_id.mod_alias,
                                               type_desc->context,
                                               type_desc->token);
}

static JikNode *
jik_get_default_initializer_for_type_desc(JikSemanticAnalyzer *sa, JikNode *nd)
{
    // TODOY: this planting of lexemes contexts and tokens is weird, check
    JikType *t = jik_semantic_resolve_type_or_error(nd);

    jik_diag_fatal_error_if(t->name == TYPE_REGION,
                            "Region type cannot be created directly",
                            jik_token_to_text(nd->token));
    jik_diag_fatal_error_if(
        t->name == TYPE_SITE, "Site type cannot be created directly", jik_token_to_text(nd->token));

    if (t->name == TYPE_INTEGER) {
        nd->token->lexeme = "0";
        return jik_node_new_integer(nd->context, nd->token);
    }
    else if (t->name == TYPE_FLOAT) {
        nd->token->lexeme = "0.";
        return jik_node_new_float(nd->context, nd->token);
    }
    else if (t->name == TYPE_BOOL) {
        return jik_node_new_boolean(false, nd->context, nd->token);
    }
    else if (t->name == TYPE_CHAR) {
        JikNode *ret = jik_node_new_char('\0', nd->context, nd->token);
        return ret;
    }
    else if (t->name == TYPE_STRING) {
        JikNode *ret = jik_node_new_string("", true, false, nd->context, nd->token);
        jik_set_alloc_spec(ret, nd->val_type_desc.alloc_spec);
        return ret;
    }
    else if (t->name == TYPE_VECTOR) {
        nd->token->lexeme  = "0";
        JikNode *size_expr = jik_node_new_integer(nd->context, nd->token);
        JikNode *elem_expr = jik_get_default_initializer_for_type_desc(sa, nd->val_type_desc.desc);
        if (!elem_expr) {
            return NULL;
        }
        JikNode *ret  = jik_node_new_vector(size_expr, elem_expr, NULL, nd->context, nd->token);
        ret->jik_type = t;
        jik_set_alloc_spec(ret, nd->val_type_desc.alloc_spec);
        return ret;
    }
    else if (t->name == TYPE_DICT) {
        JikNode *elem_expr = jik_get_default_initializer_for_type_desc(sa, nd->val_type_desc.desc);
        if (!elem_expr) {
            return NULL;
        }
        JikNode *ret  = jik_node_new_dict_literal(elem_expr, NULL, NULL, nd->context, nd->token);
        ret->jik_type = t;
        jik_set_alloc_spec(ret, nd->val_type_desc.alloc_spec);
        return ret;
    }
    else if (t->name == TYPE_OPTION) {
        JikNode *ret  = jik_node_new_option_none(nd->context, nd->token);
        ret->jik_type = t;
        jik_set_alloc_spec(ret, nd->val_type_desc.alloc_spec);
        return ret;
    }
    else if (t->name == TYPE_STRUCT) {
        if (jik_type_is_extern_struct(t)) {
            return jik_get_default_initializer_for_extern_struct(nd, t);
        }
        if (!nd->val_type_desc.name->val_id.mod_alias) {
            nd->val_type_desc.name->val_id.mod_alias = nd->token->mod_alias;
        }
        JikNode *ret = jik_node_new_struct_new(
            nd->val_type_desc.name, TabJikNode_new(), nd->context, nd->token);
        JikNode *s = jik_scope_get_symbol(ret->context,
                                          ret->val_struct_new.name->val_id.name,
                                          ret->val_struct_new.name->val_id.mod_alias,
                                          ret->token->mod_alias);
        assert(s);
        jik_semantic_infer_type(sa, s);
        ret->val_struct_new.struct_node = s;
        jik_semantic_infer_type(sa, ret);
        jik_set_alloc_spec(ret, nd->val_type_desc.alloc_spec);
        return ret;
    }
    else if (t->name == TYPE_ENUM) {
        char    *mod_alias = nd->val_type_desc.name->val_id.mod_alias
                                 ? nd->val_type_desc.name->val_id.mod_alias
                                 : nd->token->mod_alias;
        JikNode *enum_node =
            jik_scope_get_global_symbol(nd->val_member_access.node->val_id.name, mod_alias);
        assert(enum_node);
        JikNode *ret =
            jik_node_new_enum_new(enum_node->val_enum.first_enumerator, nd->context, nd->token);
        ret->jik_type = enum_node->jik_type;
        return ret;
    }
    else if (t->name == TYPE_VARIANT) {
        if (!nd->val_type_desc.name->val_id.mod_alias) {
            nd->val_type_desc.name->val_id.mod_alias = nd->token->mod_alias;
        }
        JikNode *s = jik_scope_get_symbol(nd->context,
                                          nd->val_type_desc.name->val_id.name,
                                          nd->val_type_desc.name->val_id.mod_alias,
                                          nd->token->mod_alias);
        assert(s);
        jik_semantic_infer_type(sa, s);
        JikNode *ret = jik_node_new_variant_new(
            nd->val_type_desc.name, NULL, s->val_variant.first_member, nd->context, nd->token);
        ret->val_variant_new.variant_node = s;
        jik_semantic_infer_type(sa, ret);
        jik_set_alloc_spec(ret, nd->val_type_desc.alloc_spec);
        return ret;
    }
    else {
        // jik_node_print(nd, 0);
        // jik_type_print(t);
        // assert(false);
        return NULL;
    }
}
static void
jik_semantic_apply_type_annotations(JikSemanticAnalyzer *sa)
{
    VecJikNode_iter it;
    JikNode        *nd;
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_FUNCTION) {
            size_t n = VecJikNode_size(nd->val_function.params);
            for (size_t i = 0; i < n; i++) {
                JikNode *param_node = VecJikNode_get(nd->val_function.params, i);
                JikNode *id_nd      = param_node->val_id.type_annot;
                if (!id_nd) {
                    continue;
                }
                JikType *t = jik_semantic_resolve_type_or_error(id_nd);
                jik_diag_fatal_error_if(jik_type_equal(t, &JIK_TYPE_VOID),
                                        "argument cannot be of type void",
                                        jik_token_to_text(id_nd->token));
                JikType *arg_type = VecJikType_get(nd->jik_type->val_func.param_types, i);
                jik_diag_fatal_error_if(jik_type_is_inferred(arg_type) &&
                                            !jik_type_equal(arg_type, t),
                                        JIK_STRING_NCAT("type mismatch: required ",
                                                        jik_type_pretty_name(t),
                                                        ", got ",
                                                        jik_type_pretty_name(arg_type)),
                                        jik_token_to_text(id_nd->token));
                VecJikType_set(nd->jik_type->val_func.param_types, i, t);
                char    *param_name = VecJikNode_get(nd->val_function.params, i)->val_id.name;
                JikNode *arg_nd     = jik_scope_get_symbol(
                    nd->val_function.body->context, param_name, NULL, nd->token->mod_alias);
                arg_nd->jik_type = t;
            }
            if (!nd->val_function.ret_type_annot) {
                continue;
            }
            JikType *t = jik_semantic_resolve_type_or_error(nd->val_function.ret_type_annot);
            jik_diag_fatal_error_if(
                jik_type_is_inferred(nd->jik_type->val_func.ret_type) &&
                    !jik_type_equal(nd->jik_type->val_func.ret_type, t),
                JIK_STRING_NCAT("type mismatch: required ",
                                jik_type_pretty_name(t),
                                ", got ",
                                jik_type_pretty_name(nd->jik_type->val_func.ret_type)),
                jik_token_to_text(nd->val_function.ret_type_annot->token));
            nd->jik_type->val_func.ret_type = t;
        }
    }
}

static bool
should_report_uninferred_node(JikNode *nd)
{
    return nd->type == NODE_FUNCTION || nd->type == NODE_STRUCT || nd->type == NODE_VARIANT;
}

static void
jik_semantic_ensure_types_inferred(JikSemanticAnalyzer *sa)
{
    JikNode *nd;
    if (VecJikNode_size(sa->uninferred) > 0) {
        CharBuffer     *cb            = char_buffer_new("");
        VecJikNode_iter uninferred_it = VecJikNode_iter_new(sa->uninferred);
        while (VecJikNode_iter_next(&uninferred_it, &nd)) {
            if (should_report_uninferred_node(nd)) {
                char_buffer_append(cb, JIK_STRING_NCAT("\n", jik_token_to_text(nd->token)));
            }
        }
        jik_diag_fatal_error("type inference error, could not infer:", cb->data);
    }
}

static void
jik_semantic_set_main_function_type(JikSemanticAnalyzer *sa)
{
    // TODO: main should maybe return int and then simply return value of main_main in main!
    JikNode *nd = jik_scope_get_global_symbol("main", "main");
    assert(nd);
    // nd->jik_type       = jik_type_new_function(1);
    size_t num_params = VecJikNode_size(nd->val_function.params);
    nd->jik_type      = jik_type_new_function(num_params);
    if (num_params > 0) {
        JikType *args_type = jik_type_new_vector(&JIK_TYPE_STRING);
        sa->ctx->args_type = args_type;
        VecJikType_set(nd->jik_type->val_func.param_types, 0, args_type);
        JikNode *args = jik_scope_get_local_symbol(nd->val_function.body->context, "args");
        assert(args);
        args->jik_type = args_type;
    }
    nd->jik_type->val_func.ret_type = &JIK_TYPE_INT;
}

static void
jik_semantic_set_extern_function_types(JikSemanticAnalyzer *sa)
{
    VecJikNode_iter it;
    JikNode        *nd;
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXTERN_FUNCTION) {
            size_t n     = VecJikNode_size(nd->val_extern_function.params);
            nd->jik_type = jik_type_new_function(n);
            for (size_t i = 0; i < n; i++) {
                JikNode *arg_nd = VecJikNode_get(nd->val_extern_function.params, i);
                JikType *t      = jik_semantic_resolve_type_or_error(arg_nd->val_id.type_annot);
                VecJikType_set(nd->jik_type->val_func.param_types, i, t);
            }
            JikType *t = jik_semantic_resolve_type_or_error(nd->val_extern_function.ret_node);
            nd->jik_type->val_func.ret_type = t;
            if (nd->val_extern_function.init) {
                jik_diag_fatal_error_if(nd->val_extern_function.throws,
                                        "extern init functions cannot throw",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(!jik_type_is_extern_struct(t),
                                        "extern init function must return an extern struct",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(n != 1,
                                        "extern init function must take one Region parameter",
                                        jik_token_to_text(nd->token));
                JikType *param_type = VecJikType_get(nd->jik_type->val_func.param_types, 0);
                jik_diag_fatal_error_if(!jik_type_equal(param_type, &JIK_TYPE_REGION),
                                        "extern init function must take one Region parameter",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(t->init_func,
                                        JIK_STRING_NCAT("extern struct type already has an init "
                                                        "function: ",
                                                        jik_type_pretty_name(t)),
                                        jik_token_to_text(nd->token));
                t->init_func = nd;
            }
        }
    }
}

static void
jik_recollect_nodes(JikSemanticAnalyzer *sa)
{
    sa->nodes = VecJikNode_new_empty();
    jik_collect_nodes(sa->ctx->ast, sa->nodes);
    sa->ctx->nodes = sa->nodes;
}

static void
jik_semantic_set_named_types(JikSemanticAnalyzer *sa)
{
    VecJikNode_iter it;
    JikNode        *nd;
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_STRUCT && nd->jik_type->name == TYPE_UNKNOWN) {
            nd->jik_type = jik_type_new_struct(nd->val_struct.name, NULL);
        }
        else if (nd->type == NODE_VARIANT && nd->jik_type->name == TYPE_UNKNOWN) {
            nd->jik_type = jik_type_new_variant(nd->val_variant.name, NULL);
        }
    }
}

static void
jik_semantic_infer_types(JikSemanticAnalyzer *sa)
{
    jik_semantic_set_main_function_type(sa);
    jik_semantic_set_extern_function_types(sa);
    size_t prev_len = SIZE_MAX;
    size_t n;
    bool   hints_applied = false;
    while (VecJikNode_size(sa->uninferred) < prev_len) {
        jik_semantic_traverse_ast(sa);
        if (sa->needs_recollect) {
            // jik_collect_nodes(sa->ctx->ast, sa->nodes);
            jik_recollect_nodes(sa);
            sa->needs_recollect = false;
        }
        jik_semantic_collect_uninferred_nodes(sa);
        n = VecJikNode_size(sa->uninferred);
        if (n == prev_len) {
            if (hints_applied) {
                break;
            }
            jik_semantic_apply_type_annotations(sa);
            hints_applied  = true;
            sa->uninferred = VecJikNode_new_empty();
        }
        else if (n < prev_len) {
            prev_len       = n;
            sa->uninferred = VecJikNode_new_empty();
        }
    }
    jik_semantic_apply_type_annotations(
        sa); // apply hints once more to check against inferred types
    jik_semantic_ensure_types_inferred(sa);
}

static void
jik_semantic_post_infer_actions(JikSemanticAnalyzer *sa)
{
    VecJikNode_iter it;
    // create variant nodes
    JikNode *nd;
    it = VecJikNode_iter_new(sa->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_VARIANT && !nd->val_variant.enum_nd) {
            TabBool *enumerators = TabBool_new();
            char    *first       = NULL;
            for (size_t i = 0; i < VecString_size(nd->val_variant.member_order); i++) {
                char *member_name = VecString_get(nd->val_variant.member_order, i);
                TabBool_set(enumerators, member_name, true);
                if (!first) {
                    first = member_name;
                }
            }
            nd->val_variant.enum_nd =
                jik_node_new_enum(JIK_STRING_NCAT("jik__", nd->val_variant.name),
                                  enumerators,
                                  first,
                                  nd->context,
                                  nd->token);
            for (size_t i = 0; i < VecString_size(nd->val_variant.member_order); i++) {
                VecString_push(nd->val_variant.enum_nd->val_enum.enumerator_order,
                               VecString_get(nd->val_variant.member_order, i));
            }
        }
    }
}

void
jik_check_error_handling(VecJikNode *nodes)
{
    JikNode        *nd;
    VecJikNode_iter it = VecJikNode_iter_new(nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_MUST) {
            JikNode *call_expr = nd->val_must.expr;
            JikNode *func      = jik_scope_get_function(call_expr->context,
                                                   call_expr->val_call.name->val_id.name,
                                                   call_expr->val_call.name->val_id.mod_alias,
                                                   call_expr->token->mod_alias);
            assert(func);
            jik_diag_fatal_error_if(!jik_function_throws(func),
                                    "non-throwable function cannot be handled with \"must\"",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_EXPR_CALL) {
            JikNode *func = jik_scope_get_function(nd->context,
                                                   nd->val_call.name->val_id.name,
                                                   nd->val_call.name->val_id.mod_alias,
                                                   nd->token->mod_alias);
            assert(func);
            jik_diag_fatal_error_if(jik_function_throws(func) && !nd->val_call.must,
                                    "throwable function must be handled with \"must\" or \"try\"",
                                    jik_token_to_text(nd->token));
            jik_diag_fatal_error_if(strcmp(nd->val_call.name->val_id.name, "fail") == 0 &&
                                        func->jik_type->val_func.builtin &&
                                        !jik_function_throws(nd->val_call.parent_func),
                                    "\"fail\" cannot be called from non-throwable function",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_CATCH) {
            JikNode *call_expr = nd->val_catch.stmnt->type == NODE_STMNT_DECLARE
                                     ? nd->val_catch.stmnt->val_declare.expr
                                     : nd->val_catch.stmnt;
            JikNode *func      = jik_scope_get_function(call_expr->context,
                                                   call_expr->val_call.name->val_id.name,
                                                   call_expr->val_call.name->val_id.mod_alias,
                                                   call_expr->token->mod_alias);
            assert(func);
            jik_diag_fatal_error_if(!jik_function_throws(func),
                                    "non-throwable function cannot be handled with \"try\"",
                                    jik_token_to_text(nd->token));
        }
    }
}

static void
jik_semantic_check_fail_block_placement(VecJikNode *nodes)
{
    JikNode        *nd;
    VecJikNode_iter it = VecJikNode_iter_new(nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type != NODE_BLOCK) {
            continue;
        }

        size_t n = VecJikNode_size(nd->val_block);
        for (size_t i = 0; i + 1 < n; i++) {
            JikNode *stmnt = VecJikNode_get(nd->val_block, i);
            if (!jik_node_is_fail_statement(stmnt)) {
                continue;
            }

            JikNode *next_stmnt = VecJikNode_get(nd->val_block, i + 1);
            jik_diag_fatal_error("statement after \"fail\" is unreachable",
                                 jik_token_to_text(next_stmnt->token));
        }
    }
}

void
jik_semantic_run(JikSemanticAnalyzer *sa)
{
    jik_init_namespaces();
    jik_semantic_resolve_symbols(sa);
    jik_semantic_check_invalid_value_exprs(sa);
    jik_diag_fatal_error_if(!sa->main_defined, "main function not defined", "");
    jik_semantic_set_named_types(sa);
    jik_semantic_check_recursive_composites(sa);
    jik_semantic_infer_types(sa);
    jik_semantic_post_infer_actions(sa);
    jik_check_types(sa->nodes);
    jik_check_error_handling(sa->nodes);
    jik_semantic_check_fail_block_placement(sa->nodes);
    jik_check_regions(sa->ctx->ast);
}
