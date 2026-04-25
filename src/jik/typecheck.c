#include "typecheck.h"

#include "diag.h"
#include "semantic.h"
#include "types.h"

static void
jik_check_builtin_push(JikNode *nd)
{
    size_t n = VecJikNode_size(nd->val_call.args);
    jik_diag_fatal_error_if(
        n != 2, "expected two arguments", jik_token_to_text(nd->val_call.name->token));
    JikNode *vec  = VecJikNode_get(nd->val_call.args, 0);
    JikNode *expr = VecJikNode_get(nd->val_call.args, 1);
    jik_diag_fatal_error_if(
        vec->jik_type->name != TYPE_VECTOR, "expected vector", jik_token_to_text(vec->token));
    jik_diag_fatal_error_if(!(jik_type_equal(vec->jik_type->val_vec.elem_type, expr->jik_type)),
                            JIK_STRING_NCAT("type mismatch: expected ",
                                            jik_type_pretty_name(vec->jik_type->val_vec.elem_type),
                                            ", got ",
                                            jik_type_pretty_name(expr->jik_type)),
                            jik_token_to_text(expr->token));
}

static void
jik_check_builtin_pop(JikNode *nd)
{
    size_t n = VecJikNode_size(nd->val_call.args);
    jik_diag_fatal_error_if(
        n != 1, "expected one argument", jik_token_to_text(nd->val_call.name->token));
}

static void
jik_check_builtin_clear(JikNode *nd)
{
    size_t n = VecJikNode_size(nd->val_call.args);
    jik_diag_fatal_error_if(
        n != 1, "expected one argument", jik_token_to_text(nd->val_call.name->token));
    JikNode *obj = VecJikNode_get(nd->val_call.args, 0);
    jik_diag_fatal_error_if(
        !jik_type_is_one_of(obj->jik_type, (JikTypeName[]){TYPE_VECTOR, TYPE_DICT, TYPE_NOTYPE}),
        "expected Vec or Dict",
        jik_token_to_text(obj->token));
}

static void
jik_check_builtin_len(JikNode *nd)
{
    size_t n = VecJikNode_size(nd->val_call.args);
    jik_diag_fatal_error_if(
        n != 1, "expected one argument", jik_token_to_text(nd->val_call.name->token));
    JikNode *obj = VecJikNode_get(nd->val_call.args, 0);
    jik_diag_fatal_error_if(
        !jik_type_is_one_of(obj->jik_type,
                            (JikTypeName[]){TYPE_VECTOR, TYPE_STRING, TYPE_DICT, TYPE_NOTYPE}),
        "expected Vec[T], Dict[T] or String",
        jik_token_to_text(obj->token));
}

static void
jik_check_builtin_concat(JikNode *nd)
{
    size_t n = VecJikNode_size(nd->val_call.args);
    jik_diag_fatal_error_if(n < 2,
                            "concat expects one or more String arguments followed by Region",
                            jik_token_to_text(nd->val_call.name->token));

    for (size_t i = 0; i + 1 < n; i++) {
        JikNode *arg = VecJikNode_get(nd->val_call.args, i);
        jik_diag_fatal_error_if(!jik_type_equal(arg->jik_type, &JIK_TYPE_STRING),
                                JIK_STRING_NCAT("type mismatch: required ",
                                                jik_type_pretty_name(&JIK_TYPE_STRING),
                                                ", got ",
                                                jik_type_pretty_name(arg->jik_type)),
                                jik_token_to_text(arg->token));
    }

    JikNode *reg = VecJikNode_get(nd->val_call.args, n - 1);
    jik_diag_fatal_error_if(!jik_type_equal(reg->jik_type, &JIK_TYPE_REGION),
                            JIK_STRING_NCAT("type mismatch: required ",
                                            jik_type_pretty_name(&JIK_TYPE_REGION),
                                            ", got ",
                                            jik_type_pretty_name(reg->jik_type)),
                            jik_token_to_text(reg->token));
}

static void
jik_check_builtin_fail(JikNode *nd)
{
    size_t n = VecJikNode_size(nd->val_call.args);
    jik_diag_fatal_error_if(n != 1 && n != 2,
                            JIK_STRING_NCAT("expected 1 or 2 arguments, got ", size_t_to_string(n)),
                            jik_token_to_text(nd->val_call.name->token));

    JikNode *msg = VecJikNode_get(nd->val_call.args, 0);
    jik_diag_fatal_error_if(!jik_type_equal(msg->jik_type, &JIK_TYPE_STRING),
                            JIK_STRING_NCAT("type mismatch: required ",
                                            jik_type_pretty_name(&JIK_TYPE_STRING),
                                            ", got ",
                                            jik_type_pretty_name(msg->jik_type)),
                            jik_token_to_text(msg->token));

    if (n == 2) {
        JikNode *code = VecJikNode_get(nd->val_call.args, 1);
        jik_diag_fatal_error_if(!jik_type_equal(code->jik_type, &JIK_TYPE_INT),
                                JIK_STRING_NCAT("type mismatch: required ",
                                                jik_type_pretty_name(&JIK_TYPE_INT),
                                                ", got ",
                                                jik_type_pretty_name(code->jik_type)),
                                jik_token_to_text(code->token));
    }
}

static bool
call_requires_region_injection(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_CALL);
    JikNode *func = jik_scope_get_function(nd->context,
                                           nd->val_call.name->val_id.name,
                                           nd->val_call.name->val_id.mod_alias,
                                           nd->token->mod_alias);
    assert(func);
    JikType *func_type = func->jik_type;
    if (func_type->val_func.num_params <= 0) {
        return false;
    }
    size_t n_req = func_type->val_func.num_params;
    size_t n_arg = VecJikNode_size(nd->val_call.args);
    if (n_arg == n_req - 1) {
        JikType *last_req = VecJikType_get(func_type->val_func.param_types, n_req - 1);
        return last_req == &JIK_TYPE_REGION;
    }
    return false;
}

void
jik_check_types(VecJikNode *nodes)
{
    // TODO:  track variant types per symbol, and then write to variant_tag on each node after type
    // check!

    JikNode        *nd;
    VecJikNode_iter it_nodes = VecJikNode_iter_new(nodes);
    while (VecJikNode_iter_next(&it_nodes, &nd)) {
        if (nd->type == NODE_STMNT_ASSIGN) {
            jik_diag_fatal_error_if(nd->val_assign.expr->jik_type->name == TYPE_NOTYPE ||
                                        nd->val_assign.expr->jik_type->name == TYPE_UNKNOWN,
                                    "cannot assign expression with no value type",
                                    jik_token_to_text(nd->val_assign.expr->token));
            jik_diag_fatal_error_if(
                strcmp(jik_type_pretty_name(nd->val_assign.expr->jik_type), "???") == 0,
                "cannot assign expression with no value type",
                jik_token_to_text(nd->val_assign.expr->token));
            if (jik_type_equal(nd->val_assign.expr->jik_type, &JIK_TYPE_VOID)) {
                jik_diag_fatal_error("cannot assign from void", jik_token_to_text(nd->token));
            }
            JikNode *n = jik_scope_get_symbol(nd->context,
                                              nd->val_assign.id->val_id.name,
                                              nd->val_assign.id->val_id.mod_alias,
                                              nd->val_assign.id->token->mod_alias);
            if (jik_node_is_type_inferred(n) &&
                !jik_type_equal(n->jik_type, nd->val_assign.expr->jik_type)) {
                char *details = JIK_STRING_NCAT("cannot assign ",
                                                jik_type_pretty_name(nd->val_assign.expr->jik_type),
                                                " to ",
                                                jik_type_pretty_name(n->jik_type),
                                                jik_token_to_text(nd->token));
                jik_diag_fatal_error("type mismatch", details);
            }
        }
        else if (nd->type == NODE_STMNT_DECLARE) {
            jik_diag_fatal_error_if(nd->val_declare.expr &&
                                        (nd->val_declare.expr->jik_type->name == TYPE_NOTYPE ||
                                         nd->val_declare.expr->jik_type->name == TYPE_UNKNOWN),
                                    "cannot declare from expression with no value type",
                                    jik_token_to_text(nd->val_declare.expr->token));
            jik_diag_fatal_error_if(
                nd->val_declare.expr &&
                    strcmp(jik_type_pretty_name(nd->val_declare.expr->jik_type), "???") == 0,
                "cannot declare from expression with no value type",
                jik_token_to_text(nd->val_declare.expr->token));
            if (nd->val_declare.expr &&
                jik_type_equal(nd->val_declare.expr->jik_type, &JIK_TYPE_VOID)) {
                jik_diag_fatal_error("cannot declare from void", jik_token_to_text(nd->token));
            }
        }
        else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
            jik_diag_fatal_error_if(!(jik_type_is_accessible(nd->val_member_access.node->jik_type)),
                                    "type is not accessible",
                                    jik_token_to_text(nd->val_member_access.node->token));
        }
        else if (nd->type == NODE_EXPR_OPTION_SOME) {
            jik_diag_fatal_error_if(nd->jik_type->name != TYPE_OPTION,
                                    "expected option type",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_EXPR_OPTION_NONE) {
            jik_diag_fatal_error_if(nd->jik_type->name != TYPE_OPTION,
                                    "cannot infer Option payload type",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_EXPR_OPTION_IS) {
            jik_diag_fatal_error_if(nd->val_option_is.expr->jik_type->name != TYPE_OPTION,
                                    "expected Option value",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_EXPR_OPTION_UNWRAP) {
            jik_diag_fatal_error_if(nd->val_option_unwrap.expr->jik_type->name != TYPE_OPTION,
                                    "expected Option value",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_STMNT_MEMBER_SET) {
            jik_diag_fatal_error_if(!(jik_type_is_accessible(nd->val_member_set.node->jik_type)),
                                    "type is not accessible",
                                    jik_token_to_text(nd->val_member_set.node->token));
            char    *member_name = nd->val_member_set.member_name;
            JikType *node_type   = nd->val_member_set.node->jik_type;
            if (node_type->name == TYPE_STRUCT) {
                JikType **req_type = TabJikType_get(node_type->val_struct.field_types, member_name);
                jik_diag_fatal_error_if(
                    req_type == NULL, "unknown field name", jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(
                    !jik_type_equal(*req_type, nd->val_member_set.expr->jik_type),
                    JIK_STRING_NCAT("type mismatch: required ",
                                    jik_type_pretty_name(*req_type),
                                    ", got ",
                                    jik_type_pretty_name(nd->val_member_set.expr->jik_type)),
                    jik_token_to_text(nd->token));
            }
        }
        else if (nd->type == NODE_EXPR_CALL) {
            bool reg_inject          = call_requires_region_injection(nd);
            nd->val_call.auto_region = reg_inject;
            // Check builtins
            if (nd->val_call.builtin) {
                // Specific builtins - variadic and polymorphic
                if (strcmp(nd->val_call.name->val_id.name, "push") == 0) {
                    jik_check_builtin_push(nd);
                }
                else if (strcmp(nd->val_call.name->val_id.name, "pop") == 0) {
                    jik_check_builtin_pop(nd);
                }
                else if (strcmp(nd->val_call.name->val_id.name, "clear") == 0) {
                    jik_check_builtin_clear(nd);
                }
                else if (strcmp(nd->val_call.name->val_id.name, "len") == 0) {
                    jik_check_builtin_len(nd);
                }
                else if (strcmp(nd->val_call.name->val_id.name, "concat") == 0) {
                    jik_check_builtin_concat(nd);
                }
                else if (strcmp(nd->val_call.name->val_id.name, "fail") == 0) {
                    jik_check_builtin_fail(nd);
                }
                // Static builtins - fixed argument number and types
                else {
                    JikNode *builtin_func =
                        jik_scope_get_function(nd->context,
                                               nd->val_call.name->val_id.name,
                                               nd->val_call.name->val_id.mod_alias,
                                               nd->token->mod_alias);
                    assert(builtin_func);
                    JikType *func_type = builtin_func->jik_type;
                    if (func_type->val_func.num_params == -1) {
                        continue;
                    }
                    size_t n_req = func_type->val_func.num_params;
                    size_t n_arg = VecJikNode_size(nd->val_call.args);
                    jik_diag_fatal_error_if(n_req != n_arg && !reg_inject,
                                            JIK_STRING_NCAT("expected ",
                                                            size_t_to_string(n_req),
                                                            " arguments, got ",
                                                            size_t_to_string(n_arg)),
                                            jik_token_to_text(nd->val_call.name->token));
                    for (size_t i = 0; i < n_arg; i++) {
                        JikType *req_type =
                            VecJikType_get(builtin_func->jik_type->val_func.param_types, i);
                        JikNode *arg_node = VecJikNode_get(nd->val_call.args, i);
                        JikType *arg_type = arg_node->jik_type;
                        jik_diag_fatal_error_if(!jik_type_equal(arg_type, req_type),
                                                JIK_STRING_NCAT("type mismatch: required ",
                                                                jik_type_pretty_name(req_type),
                                                                ", got ",
                                                                jik_type_pretty_name(arg_type)),
                                                jik_token_to_text(arg_node->token));
                    }
                }
            }
            // Check externs
            else if (nd->val_call.extern_name) {
                JikNode *ext_func = jik_scope_get_function(nd->context,
                                                           nd->val_call.name->val_id.name,
                                                           nd->val_call.name->val_id.mod_alias,
                                                           nd->token->mod_alias);
                assert(ext_func && ext_func->type == NODE_EXTERN_FUNCTION);
                size_t n_req = VecJikNode_size(ext_func->val_extern_function.params);
                size_t n_arg = VecJikNode_size(nd->val_call.args);

                jik_diag_fatal_error_if(n_req != n_arg && !reg_inject,
                                        JIK_STRING_NCAT("expected ",
                                                        size_t_to_string(n_req),
                                                        " arguments, got ",
                                                        size_t_to_string(n_arg)),
                                        jik_token_to_text(nd->val_call.name->token));
                for (size_t i = 0; i < n_arg; i++) {
                    JikType *req_type = VecJikType_get(ext_func->jik_type->val_func.param_types, i);
                    JikNode *arg_node = VecJikNode_get(nd->val_call.args, i);
                    JikType *arg_type = arg_node->jik_type;
                    jik_diag_fatal_error_if(!jik_type_equal(arg_type, req_type),
                                            JIK_STRING_NCAT("type mismatch: required ",
                                                            jik_type_pretty_name(req_type),
                                                            ", got ",
                                                            jik_type_pretty_name(arg_type)),
                                            jik_token_to_text(arg_node->token));
                }
            }
            else {
                JikNode *func = jik_scope_get_function(nd->context,
                                                       nd->val_call.name->val_id.name,
                                                       nd->val_call.name->val_id.mod_alias,
                                                       nd->token->mod_alias);
                assert(func);
                size_t n_req = VecJikType_size(func->jik_type->val_func.param_types);
                size_t n_arg = VecJikNode_size(nd->val_call.args);

                jik_diag_fatal_error_if(n_req != n_arg && !reg_inject,
                                        JIK_STRING_NCAT("expected ",
                                                        size_t_to_string(n_req),
                                                        " arguments, got ",
                                                        size_t_to_string(n_arg)),
                                        jik_token_to_text(nd->val_call.name->token));
            }
        }
        else if (nd->type == NODE_EXPR_TERNARY) {
            JikNode *condition = nd->val_ternary.condition;
            JikNode *expr_if   = nd->val_ternary.expr_if;
            JikNode *expr_else = nd->val_ternary.expr_else;
            jik_diag_fatal_error_if(!jik_type_equal(condition->jik_type, &JIK_TYPE_BOOL),
                                    "expected expression of bool type",
                                    jik_token_to_text(condition->token));
            jik_diag_fatal_error_if(expr_if->jik_type->name == TYPE_VOID ||
                                        expr_else->jik_type->name == TYPE_VOID,
                                    "ternary branches cannot be void",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_EXPR_VECTOR) {
            if (nd->val_vector.size_expr) {
                jik_diag_fatal_error_if(
                    !jik_type_equal(nd->val_vector.size_expr->jik_type, &JIK_TYPE_INT),
                    "expected expression of integer type",
                    jik_token_to_text(nd->val_vector.size_expr->token));
            }
            else if (nd->val_vector.init_elems) {
                size_t   n     = VecJikNode_size(nd->val_vector.init_elems);
                JikNode *first = VecJikNode_get(nd->val_vector.init_elems, 0);
                for (size_t i = 0; i < n; i++) {
                    JikNode *v = VecJikNode_get(nd->val_vector.init_elems, i);
                    jik_diag_fatal_error_if(!jik_type_equal(v->jik_type, first->jik_type),
                                            JIK_STRING_NCAT("type mismatch: required ",
                                                            jik_type_pretty_name(first->jik_type),
                                                            ", got ",
                                                            jik_type_pretty_name(v->jik_type)),
                                            jik_token_to_text(v->token));
                }
            }
        }
        else if (nd->type == NODE_EXPR_DICT) {
            if (!nd->val_dict.init_values) {
                continue;
            }
            size_t n = VecJikNode_size(nd->val_dict.init_values);
            assert(n > 0);
            JikNode *first = VecJikNode_get(nd->val_dict.init_values, 0);
            JikNode *k, *v;
            for (size_t i = 0; i < n; i++) {
                k = VecJikNode_get(nd->val_dict.init_keys, i);
                v = VecJikNode_get(nd->val_dict.init_values, i);
                jik_diag_fatal_error_if(!jik_type_equal(k->jik_type, &JIK_TYPE_STRING),
                                        "expected expression of string type",
                                        jik_token_to_text(k->token));
                jik_diag_fatal_error_if(!jik_type_equal(v->jik_type, first->jik_type),
                                        JIK_STRING_NCAT("type mismatch: required ",
                                                        jik_type_pretty_name(v->jik_type),
                                                        ", got ",
                                                        jik_type_pretty_name(first->jik_type)),
                                        jik_token_to_text(nd->token));
            }
        }
        else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
            if (nd->val_subscript_get.node->jik_type->name == TYPE_VECTOR) {
                jik_diag_fatal_error_if(
                    !jik_type_equal(nd->val_subscript_get.expr->jik_type, &JIK_TYPE_INT),
                    "expected expression of integer type",
                    jik_token_to_text(nd->val_subscript_get.expr->token));
            }
            else if (nd->val_subscript_get.node->jik_type->name == TYPE_DICT) {
                jik_diag_fatal_error_if(
                    !jik_type_equal(nd->val_subscript_get.expr->jik_type, &JIK_TYPE_STRING),
                    "expected expression of integer type",
                    jik_token_to_text(nd->val_subscript_get.expr->token));
            }
        }
        else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
            JikNode *node = nd->val_subscript_set.node;
            JikNode *sub  = nd->val_subscript_set.sub_expr;
            JikNode *expr = nd->val_subscript_set.expr;
            if (node->jik_type->name == TYPE_VECTOR) {
                jik_diag_fatal_error_if(!jik_type_equal(sub->jik_type, &JIK_TYPE_INT),
                                        "expected expression of integer type",
                                        jik_token_to_text(sub->token));
                jik_diag_fatal_error_if(
                    !jik_type_equal(node->jik_type->val_vec.elem_type, expr->jik_type),
                    JIK_STRING_NCAT("type mismatch: required ",
                                    jik_type_pretty_name(node->jik_type->val_vec.elem_type),
                                    ", got ",
                                    jik_type_pretty_name(expr->jik_type)),
                    jik_token_to_text(nd->token));
            }
            else if (node->jik_type->name == TYPE_DICT) {
                jik_diag_fatal_error_if(!jik_type_equal(sub->jik_type, &JIK_TYPE_STRING),
                                        "expected expression of string type",
                                        jik_token_to_text(sub->token));
                jik_diag_fatal_error_if(
                    !jik_type_equal(node->jik_type->val_dict.elem_type, expr->jik_type),
                    JIK_STRING_NCAT("type mismatch: required ",
                                    jik_type_pretty_name(node->jik_type->val_dict.elem_type),
                                    ", got ",
                                    jik_type_pretty_name(expr->jik_type)),
                    jik_token_to_text(nd->token));
            }
            else if (node->jik_type->name == TYPE_VARIANT) {
                JikType **active_type = TabJikType_get(node->jik_type->val_variant.variant_types,
                                                       sub->val_variant_tag.tag);
                assert(active_type);
                JikType *req_type = *active_type;
                jik_diag_fatal_error_if(
                    !jik_type_equal(req_type, expr->jik_type),
                    JIK_STRING_NCAT("type mismatch: required ",
                                    jik_type_pretty_name(req_type),
                                    ", got ",
                                    jik_type_pretty_name(nd->val_member_set.expr->jik_type)),
                    jik_token_to_text(nd->token));
            }
            jik_diag_fatal_error_if(node->jik_type->name == TYPE_STRING,
                                    "strings are immutable",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_LOOP_FOR) {
            JikNode *start = nd->val_for.start_expr;
            JikNode *end   = nd->val_for.end_expr;
            jik_diag_fatal_error_if(!jik_type_equal(start->jik_type, &JIK_TYPE_INT),
                                    "expected expression of integer type",
                                    jik_token_to_text(start->token));
            jik_diag_fatal_error_if(!jik_type_equal(end->jik_type, &JIK_TYPE_INT),
                                    "expected expression of integer type",
                                    jik_token_to_text(end->token));
        }
        else if (nd->type == NODE_EXPR_STRUCT_NEW) {
            TabJikNode_iter it = TabJikNode_iter_new(nd->val_struct_new.init_vals);
            TabJikNode_item item;
            while (TabJikNode_iter_next(&it, &item)) {
                JikType **req_type = TabJikType_get(nd->jik_type->val_struct.field_types, item.key);
                jik_diag_fatal_error_if(!req_type,
                                        JIK_STRING_NCAT("unknown field name: \"", item.key, "\""),
                                        jik_token_to_text(item.value->token));
                jik_diag_fatal_error_if(!jik_type_equal(*req_type, item.value->jik_type),
                                        JIK_STRING_NCAT("type mismatch: required ",
                                                        jik_type_pretty_name(*req_type),
                                                        ", got ",
                                                        jik_type_pretty_name(item.value->jik_type)),
                                        jik_token_to_text(item.value->token));
            }
        }
        else if (nd->type == NODE_STMNT_MATCH) {
            jik_diag_fatal_error_if(nd->val_match.expr->jik_type->name != TYPE_VARIANT,
                                    "expected variant instance",
                                    jik_token_to_text(nd->val_match.expr->token));
            JikType *variant_type = nd->val_match.expr->jik_type;
            size_t   num_tags     = TabJikType_size(variant_type->val_variant.variant_types);
            size_t   n            = VecJikNode_size(nd->val_match.cases);
            jik_diag_fatal_error_if(n == 0, "no cases provided", jik_token_to_text(nd->token));
            TabBool *seen_tags = TabBool_new();
            size_t   cnt       = 0;
            for (size_t i = 0; i < n; i++) {
                JikNode *case_nd = VecJikNode_get(nd->val_match.cases, i);
                bool *res = TabBool_get(seen_tags, case_nd->val_case.variant->val_variant_new.tag);
                if (!res) {
                    TabBool_set(seen_tags, case_nd->val_case.variant->val_variant_new.tag, true);
                    cnt++;
                }
            }
            jik_diag_fatal_error_if(cnt != num_tags,
                                    "match doesn't exhaust all variant tags",
                                    jik_token_to_text(nd->token));
        }
        else if (nd->type == NODE_CASE) {
            jik_ensure_valid_variant_tag(nd->val_case.variant);
        }
        else if (nd->type == NODE_EXPR_VARIANT_TAG_CHECK) {
            JikNode *s = jik_scope_get_symbol(nd->context,
                                              nd->val_variant_tag_check.id_node->val_id.name,
                                              nd->val_variant_tag_check.id_node->val_id.mod_alias,
                                              nd->token->mod_alias);
            jik_diag_fatal_error_if(s->type != NODE_VARIANT,
                                    JIK_STRING_NCAT("expected variant"),
                                    jik_token_to_text(nd->token));
            jik_semantic_ensure_module_used(nd->val_variant_tag_check.id_node);
            jik_diag_fatal_error_if(!s,
                                    JIK_STRING_NCAT("variant \"",
                                                    nd->val_variant_tag_check.id_node->val_id.name,
                                                    "\" not defined"),
                                    jik_token_to_text(nd->val_variant_tag_check.id_node->token));
            nd->val_variant_tag_check.variant_node = s;
            JikNode **res = TabJikNode_get(s->val_variant.init_vals, nd->val_variant_tag_check.tag);
            jik_diag_fatal_error_if(
                !res,
                JIK_STRING_NCAT("unknown variant tag \"", nd->val_variant_tag_check.tag, "\""),
                jik_token_to_text(nd->val_variant_tag_check.id_node->token));
        }
        else if (nd->type == NODE_STMNT_RETURN && nd->val_return.expr) {
            jik_diag_fatal_error_if(nd->val_return.expr->jik_type == &JIK_TYPE_VOID,
                                    "cannot return void",
                                    jik_token_to_text(nd->val_return.expr->token));
        }
    }
}
