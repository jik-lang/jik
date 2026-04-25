#include "regcheck.h"

#include "ast.h"
#include "scope.h"
#include "semantic.h"
#include "types.h"

JIK_HTAB_DEFINE(TabJikAllocSpec, JikAllocSpec);
JIK_HTAB_DEFINE(TabInt, int);

char *
jik_make_alloc_src_mismatch_msg(JikAllocSpec lhs, JikAllocSpec rhs)
{
    char *msg = "region mismatch: ";
    if (lhs.src == JIK_ALLOC_SRC_CROSS || rhs.src == JIK_ALLOC_SRC_CROSS) {
        return JIK_STRING_NCAT(msg, "illegal foreign parameter in store operation");
    }
    else if (lhs.src == JIK_ALLOC_LOCAL || rhs.src == JIK_ALLOC_LOCAL) {
        return JIK_STRING_NCAT(msg, "illegal store operation between local and non-local value");
    }
    return JIK_STRING_NCAT(msg, "illegal store operation between incompatible allocation sources");
}

static void
jik_check_region_semantics(JikNode *ast)
{
    JikNode *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd                    = VecJikNode_get(ast->val_program.functions, i);
        VecJikNode     *func_nodes = func_nd->val_function.subnodes;
        JikNode        *nd;
        VecJikNode_iter it = VecJikNode_iter_new(func_nodes);
        while (VecJikNode_iter_next(&it, &nd)) {
            if (nd->type == NODE_STMNT_RETURN && nd->val_return.expr) {
                jik_diag_fatal_error_if(nd->val_return.expr->jik_type == &JIK_TYPE_REGION,
                                        "region cannot be a return value",
                                        jik_token_to_text(nd->token));
            }
            else if (nd->type == NODE_STMNT_ASSIGN) {
                JikNode *rhs = nd->val_assign.expr;
                if (rhs->jik_type != &JIK_TYPE_REGION) {
                    continue;
                }
                jik_diag_fatal_error("region cannot be aliased", jik_token_to_text(nd->token));
            }
            else if (nd->type == NODE_STMNT_DECLARE) {
                JikNode *rhs = nd->val_declare.expr;
                if (rhs->jik_type != &JIK_TYPE_REGION) {
                    continue;
                }
                jik_diag_fatal_error("region cannot be declared", jik_token_to_text(nd->token));
            }
            else if (jik_node_is_allocated_literal(nd)) {
                JikAllocSpec aspec = jik_get_alloc_spec(nd);
                if (aspec.kind == JIK_ALLOC_LOCAL) {
                    continue;
                }
                JikNode *s = jik_scope_get_local_symbol(nd->context, aspec.region_name);
                jik_diag_fatal_error_if(
                    !s,
                    JIK_STRING_NCAT("allocation in region of unknown or global symbol: ",
                                    aspec.region_name),
                    jik_token_to_text(nd->token));
                if (aspec.kind == JIK_ALLOC_NAMED_REGION) {
                    jik_diag_fatal_error_if(
                        s->jik_type != &JIK_TYPE_REGION,
                        JIK_STRING_NCAT(
                            "allocation to unknown region: ", "\"", aspec.region_name, "\""),
                        jik_token_to_text(nd->token));
                }
                else if (aspec.kind == JIK_ALLOC_CONTAINER) {
                    jik_diag_fatal_error_if(!jik_type_is_allocated(s->jik_type),
                                            "invalid allocation destination",
                                            jik_token_to_text(nd->token));
                }
            }
        }
    }
}

static TabInt *
get_function_param_names(JikNode *nd)
{
    assert(nd->type == NODE_FUNCTION);
    TabInt *param_names = TabInt_new();
    size_t  n           = VecJikNode_size(nd->val_function.params);
    for (size_t i = 0; i < n; i++) {
        JikNode *param = VecJikNode_get(nd->val_function.params, i);
        TabInt_set(param_names, param->val_id.name, 1);
    }
    return param_names;
}

static bool
is_func_param_name_foreign(JikNode *func_nd, char *name)
{
    for (size_t i = 0; i < VecJikNode_size(func_nd->val_function.params); i++) {
        JikNode *param = VecJikNode_get(func_nd->val_function.params, i);
        if (strcmp(name, param->val_id.name) == 0) {
            return param->val_id.is_foreign;
        }
    }
    jik_diag_fatal_error("internal error: unknown function parameter in region check", "");
}

static void
populate_spec_tab_from_scope(JikScope        *s,
                             TabInt          *param_names,
                             TabJikAllocSpec *spec_tab,
                             JikNode         *func_nd)
{
    TabJikNode_iter it = TabJikNode_iter_new(s->symbols);
    TabJikNode_item item;
    while (TabJikNode_iter_next(&it, &item)) {
        if (!jik_type_is_allocated(item.value->jik_type) &&
            item.value->jik_type != &JIK_TYPE_REGION) {
            continue;
        }
        int *res = TabInt_get(param_names, item.key);
        if (!res) {
            TabJikAllocSpec_set(
                spec_tab,
                item.key,
                (JikAllocSpec){.kind = JIK_ALLOC_UNKNOWN, .src = JIK_ALLOC_SRC_UNKNOWN});
        }
        else if (item.value->jik_type != &JIK_TYPE_REGION) {
            JikAllocSource s = is_func_param_name_foreign(func_nd, item.key)
                                   ? JIK_ALLOC_SRC_CROSS
                                   : JIK_ALLOC_SRC_FOREIGN;
            TabJikAllocSpec_set(
                spec_tab,
                item.key,
                (JikAllocSpec){.kind = JIK_ALLOC_CONTAINER, .src = s, .region_name = item.key});
        }
        else {
            TabJikAllocSpec_set(spec_tab,
                                item.key,
                                (JikAllocSpec){.kind        = JIK_ALLOC_NAMED_REGION,
                                               .src         = JIK_ALLOC_SRC_FOREIGN,
                                               .region_name = item.key});
        }
    }
}

static void
create_alloc_spec_tab_for_function(JikNode *func_nd)
{
    func_nd->val_function.info->spec_tab = TabJikAllocSpec_new();
    TabJikAllocSpec *spec_tab            = func_nd->val_function.info->spec_tab;
    TabInt          *param_names         = get_function_param_names(func_nd);
    size_t           n                   = VecJikNode_size(func_nd->val_function.subnodes);
    JikNode         *nd;
    for (size_t i = 0; i < n; i++) {
        nd = VecJikNode_get(func_nd->val_function.subnodes, i);
        if (nd->type == NODE_BLOCK) {
            populate_spec_tab_from_scope(nd->context, param_names, spec_tab, func_nd);
        }
    }
}

static void
jik_prepare_functions(JikNode *ast)
{
    JikNode *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd              = VecJikNode_get(ast->val_program.functions, i);
        VecJikNode *subnodes = VecJikNode_new_empty();
        jik_collect_nodes(func_nd, subnodes);
        func_nd->val_function.subnodes         = subnodes;
        func_nd->val_function.info             = (FuncInfo *)jik_alloc(sizeof(FuncInfo));
        func_nd->val_function.info->has_allocs = false;
        create_alloc_spec_tab_for_function(func_nd);
    }
}

static void
jik_mark_function_needs_local_region(JikNode *func_nd)
{
    func_nd->val_function.info->has_allocs = true;
}

static bool
node_needs_local_region(JikNode *nd)
{
    if (jik_node_is_allocated_literal(nd)) {
        JikAllocSpec spec = jik_get_alloc_spec(nd);
        return spec.kind == JIK_ALLOC_LOCAL;
    }

    if (nd->type == NODE_EXPR_LOCAL_REGION) {
        return true;
    }

    if (nd->type != NODE_EXPR_CALL) {
        return false;
    }

    if (nd->val_call.auto_region) {
        return true;
    }

    if (!nd->val_call.builtin) {
        return false;
    }

    char *name = nd->val_call.name->val_id.name;
    if (strcmp(name, "print") == 0 || strcmp(name, "println") == 0) {
        return true;
    }

    if (strcmp(name, "error_msg") == 0 && VecJikNode_size(nd->val_call.args) == 0) {
        return true;
    }

    return false;
}

static void
jik_classify_functions(JikNode *ast)
{
    JikNode *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd                                = VecJikNode_get(ast->val_program.functions, i);
        func_nd->val_function.info->has_allocs = false;
        VecJikNode     *subnodes               = func_nd->val_function.subnodes;
        JikNode        *nd;
        VecJikNode_iter it = VecJikNode_iter_new(subnodes);
        while (VecJikNode_iter_next(&it, &nd)) {
            if (node_needs_local_region(nd)) {
                jik_mark_function_needs_local_region(func_nd);
                break;
            }
        }
    }
}

static void
jik_check_orphaned_allocations(JikNode *ast)
{
    JikNode *base;
    JikNode *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd                    = VecJikNode_get(ast->val_program.functions, i);
        VecJikNode     *func_nodes = func_nd->val_function.subnodes;
        JikNode        *nd;
        VecJikNode_iter it = VecJikNode_iter_new(func_nodes);
        while (VecJikNode_iter_next(&it, &nd)) {
            if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
                base = nd->val_member_access.node;
                while (base->type == NODE_EXPR_GROUPING) {
                    base = base->val_grouping;
                }
                jik_diag_fatal_error_if(base->type == NODE_EXPR_STRUCT_NEW,
                                        "composite value must be bound before member access",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(base->type == NODE_EXPR_CALL,
                                        "cannot access member on function call",
                                        jik_token_to_text(nd->token));
            }
            else if (nd->type == NODE_STMNT_MEMBER_SET) {
                base = nd->val_member_set.node;
                while (base->type == NODE_EXPR_GROUPING) {
                    base = base->val_grouping;
                }
                jik_diag_fatal_error_if(base->type == NODE_EXPR_STRUCT_NEW,
                                        "composite value must be bound before mutation",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(base->type == NODE_EXPR_CALL,
                                        "cannot set member on function call",
                                        jik_token_to_text(nd->token));
            }
            else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
                base = nd->val_subscript_get.node;
                while (base->type == NODE_EXPR_GROUPING) {
                    base = base->val_grouping;
                }
                jik_diag_fatal_error_if(base->type == NODE_EXPR_VECTOR,
                                        "composite value must be bound before subscripting",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(base->type == NODE_EXPR_CALL,
                                        "cannot set subscript on function call",
                                        jik_token_to_text(nd->token));
            }
            else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
                base = nd->val_subscript_set.node;
                while (base->type == NODE_EXPR_GROUPING) {
                    base = base->val_grouping;
                }
                jik_diag_fatal_error_if(base->type == NODE_EXPR_VECTOR,
                                        "composite value must be bound before mutation",
                                        jik_token_to_text(nd->token));
                jik_diag_fatal_error_if(base->type == NODE_EXPR_CALL,
                                        "cannot set member on function call",
                                        jik_token_to_text(nd->token));
            }
            else if (nd->type == NODE_LOOP_FOR_IN) {
                base = nd->val_for_in.container_expr;
                while (base->type == NODE_EXPR_GROUPING) {
                    base = base->val_grouping;
                }
                jik_diag_fatal_error_if(jik_node_is_allocated_literal(base),
                                        "composite value must be bound before iteration",
                                        jik_token_to_text(nd->val_for_in.container_expr->token));
            }
            else if (nd->type == NODE_LOOP_FOR_IN_DICT) {
                base = nd->val_for_in_dict.dict_expr;
                while (base->type == NODE_EXPR_GROUPING) {
                    base = base->val_grouping;
                }
                jik_diag_fatal_error_if(jik_node_is_allocated_literal(base),
                                        "composite value must be bound before iteration",
                                        jik_token_to_text(nd->val_for_in.container_expr->token));
            }
        }
    }
}

static bool
is_func_param_foreign(JikNode *func_nd, size_t idx)
{
    if (func_nd->type == NODE_BUILTIN_FUNCTION) {
        return false;
    }
    if (func_nd->type == NODE_FUNCTION) {
        JikNode *param = VecJikNode_get(func_nd->val_function.params, idx);
        return param->val_id.is_foreign;
    }
    if (func_nd->type == NODE_EXTERN_FUNCTION) {
        JikNode *param = VecJikNode_get(func_nd->val_extern_function.params, idx);
        return param->val_id.is_foreign;
    }
    jik_diag_fatal_error("internal error: invalid function node in is_func_param_foreign", "");
}

VecJikNode *
get_allocated_args(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_CALL);

    JikNode *func = jik_scope_get_function(nd->context,
                                           nd->val_call.name->val_id.name,
                                           nd->val_call.name->val_id.mod_alias,
                                           nd->token->mod_alias);
    assert(func);

    size_t      n           = VecJikNode_size(nd->val_call.args);
    VecJikNode *allocd_args = VecJikNode_new_empty();
    for (size_t i = 0; i < n; i++) {
        if (is_func_param_foreign(func, i)) {
            continue;
        }
        JikNode *arg = VecJikNode_get(nd->val_call.args, i);
        if (!jik_type_is_allocated(arg->jik_type) && arg->jik_type != &JIK_TYPE_REGION) {
            continue;
        }
        VecJikNode_push(allocd_args, arg);
    }
    if (nd->val_call.auto_region) {
        VecJikNode_push(allocd_args, jik_node_new_local_region(nd->context, nd->token));
    }
    return allocd_args;
}

JikNode *
get_first_non_literal_allocd_arg(VecJikNode *args)
{
    size_t   n   = VecJikNode_size(args);
    JikNode *arg = NULL;
    for (size_t i = 0; i < n; i++) {
        JikNode *nd = VecJikNode_get(args, i);
        if (!jik_node_is_allocated_literal(nd)) {
            arg = nd;
            break;
        }
    }
    return arg;
}

static bool
jik_both_alloc_sources_known(JikAllocSpec s1, JikAllocSpec s2);
static bool
jik_alloc_source_known(JikAllocSpec s);
static bool
jik_alloc_kind_known(JikAllocSpec s);
static bool
jik_alloc_spec_complete(JikAllocSpec s);
static bool
jik_alloc_sources_match(JikAllocSpec s1, JikAllocSpec s2);

static JikAllocSpec
get_expression_alloc_spec(JikNode *nd, TabJikAllocSpec *tvs)
{
    if (jik_node_is_allocated_literal(nd)) {
        return jik_get_alloc_spec(nd);
    }
    else if (nd->type == NODE_EXPR_CALL) {
        return nd->val_call.alloc_spec;
    }
    else if (nd->type == NODE_EXPR_MUST) {
        return get_expression_alloc_spec(nd->val_must.expr, tvs);
    }
    else if (nd->type == NODE_EXPR_OPTION_UNWRAP) {
        return get_expression_alloc_spec(nd->val_option_unwrap.expr, tvs);
    }
    else if (nd->type == NODE_EXPR_IDENTIFIER) {
        JikAllocSpec *res = TabJikAllocSpec_get(tvs, nd->val_id.name);
        if (res) {
            return *res;
        }
        JikNode *gs = jik_scope_get_global_symbol(nd->val_id.name, nd->token->mod_alias);
        if (!gs) {
            jik_diag_fatal_error("internal error: unresolved global symbol in region check", "");
        }
        return get_expression_alloc_spec(gs, tvs);
    }
    else if (nd->type == NODE_EXPR_LOCAL_REGION) {
        return (JikAllocSpec){.kind = JIK_ALLOC_LOCAL, .src = JIK_ALLOC_SRC_LOCAL};
    }
    else if (nd->type == NODE_EXPR_REGIONOF) {
        JikAllocSpec *res = TabJikAllocSpec_get(tvs, nd->token->lexeme);
        assert(res);
        return *res;
    }
    else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
        return get_expression_alloc_spec(nd->val_subscript_get.node, tvs);
    }
    else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
        return get_expression_alloc_spec(nd->val_member_access.node, tvs);
    }
    else if (nd->type == NODE_EXPR_TERNARY) {
        JikNode     *expr_if   = nd->val_ternary.expr_if;
        JikNode     *expr_else = nd->val_ternary.expr_else;
        JikAllocSpec spec_if   = get_expression_alloc_spec(expr_if, tvs);
        JikAllocSpec spec_else = get_expression_alloc_spec(expr_else, tvs);

        if (jik_alloc_spec_complete(spec_if) && jik_node_is_allocated_literal(expr_else) &&
            spec_else.src == JIK_ALLOC_SRC_LOCAL) {
            jik_set_alloc_spec(expr_else, spec_if);
            return spec_if;
        }
        if (jik_alloc_spec_complete(spec_else) && jik_node_is_allocated_literal(expr_if) &&
            spec_if.src == JIK_ALLOC_SRC_LOCAL) {
            jik_set_alloc_spec(expr_if, spec_else);
            return spec_else;
        }
        if (jik_both_alloc_sources_known(spec_if, spec_else)) {
            jik_diag_fatal_error_if(!jik_alloc_sources_match(spec_if, spec_else),
                                    "ternary branches should belong to same region",
                                    jik_token_to_text(nd->token));
        }
        if (jik_alloc_spec_complete(spec_if) && jik_alloc_spec_complete(spec_else)) {
            return spec_if;
        }
        return (JikAllocSpec){.kind = JIK_ALLOC_UNKNOWN, .src = JIK_ALLOC_SRC_UNKNOWN};
    }
    else {
        jik_diag_fatal_error("internal error: no region rule for expression",
                             jik_token_to_text(nd->token));
    }
}

static bool
jik_both_alloc_sources_known(JikAllocSpec s1, JikAllocSpec s2)
{
    return s1.src != JIK_ALLOC_SRC_UNKNOWN && s2.src != JIK_ALLOC_SRC_UNKNOWN;
}

static bool
jik_alloc_source_known(JikAllocSpec s)
{
    return s.src != JIK_ALLOC_SRC_UNKNOWN;
}

static bool
jik_alloc_kind_known(JikAllocSpec s)
{
    return s.kind != JIK_ALLOC_UNKNOWN;
}

static bool
jik_alloc_spec_complete(JikAllocSpec s)
{
    return s.kind != JIK_ALLOC_UNKNOWN && s.src != JIK_ALLOC_SRC_UNKNOWN;
}

static bool
jik_alloc_sources_match(JikAllocSpec s1, JikAllocSpec s2)
{
    assert(s1.src != JIK_ALLOC_SRC_UNKNOWN && s2.src != JIK_ALLOC_SRC_UNKNOWN);
    if (s1.src == JIK_ALLOC_SRC_CROSS || s2.src == JIK_ALLOC_SRC_CROSS) {
        return false;
    }
    return s1.src == s2.src;
}

static VecJikNode *
jik_collect_unknown_allocs(JikNode *ast)
{
    VecJikNode *unknown = VecJikNode_new_empty();
    JikNode    *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd                   = VecJikNode_get(ast->val_program.functions, i);
        VecJikNode      *subnodes = func_nd->val_function.subnodes;
        TabJikAllocSpec *spec_tab = func_nd->val_function.info->spec_tab;
        VecJikNode_iter  it       = VecJikNode_iter_new(subnodes);
        JikNode         *nd;
        while (VecJikNode_iter_next(&it, &nd)) {
            if (!jik_node_has_alloc_spec(nd)) {
                continue;
            }
            JikAllocSpec alloc_spec = get_expression_alloc_spec(nd, spec_tab);
            if (!jik_alloc_source_known(alloc_spec) || !jik_alloc_kind_known(alloc_spec)) {
                VecJikNode_push(unknown, nd);
            }
        }
    }
    return unknown;
}

static void
jik_diag_uninferred_alloc(JikNode *nd)
{
    jik_diag_fatal_error(
        "could not infer region for expression",
        JIK_STRING_NCAT("the compiler could not determine whether this value belongs to the "
                        "local, argument-region, or foreign source class\n",
                        jik_token_to_text(nd->token)));
}

// A function is region safe if it does not write anything
// from one region to another or similar. Such functions are
// builtins like print, etc. Checks for user-defined functions
// should be added later.
static bool
is_region_safe_function_call(JikNode *nd)
{
    assert(nd->type == NODE_EXPR_CALL);
    if (nd->val_call.builtin && strcmp(nd->val_call.name->val_id.name, "print") == 0) {
        return true;
    }
    else if (nd->val_call.builtin && strcmp(nd->val_call.name->val_id.name, "println") == 0) {
        return true;
    }
    return false;
}

static void
jik_check_region_integrity(JikNode *ast)
{
    JikNode *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd                     = VecJikNode_get(ast->val_program.functions, i);
        VecJikNode      *func_nodes = func_nd->val_function.subnodes;
        JikNode         *nd;
        TabJikAllocSpec *spec_tab = func_nd->val_function.info->spec_tab;
        VecJikNode_iter  it       = VecJikNode_iter_new(func_nodes);
        while (VecJikNode_iter_next(&it, &nd)) {
            if (nd->type == NODE_STMNT_DECLARE) {
                if (!jik_type_is_allocated(nd->val_declare.expr->jik_type)) {
                    continue;
                }
                JikAllocSpec src_rhs = get_expression_alloc_spec(nd->val_declare.expr, spec_tab);
                if (jik_alloc_source_known(src_rhs)) {
                    TabJikAllocSpec_set(spec_tab, nd->val_declare.id->val_id.name, src_rhs);
                }
            }
            else if (nd->type == NODE_STMNT_ASSIGN) {
                if (!jik_type_is_allocated(nd->val_assign.expr->jik_type)) {
                    continue;
                }
                JikAllocSpec src_lhs = get_expression_alloc_spec(nd->val_assign.id, spec_tab);
                JikAllocSpec src_rhs = get_expression_alloc_spec(nd->val_assign.expr, spec_tab);
                if (jik_both_alloc_sources_known(src_lhs, src_rhs)) {
                    jik_diag_fatal_error_if(!jik_alloc_sources_match(src_lhs, src_rhs),
                                            jik_make_alloc_src_mismatch_msg(src_lhs, src_rhs),
                                            jik_token_to_text(nd->token));
                }
            }
            else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
                JikAllocSpec spec_lhs =
                    get_expression_alloc_spec(nd->val_subscript_set.node, spec_tab);
                jik_diag_fatal_error_if(spec_lhs.kind == JIK_ALLOC_GLOBAL,
                                        "global composites are immutable",
                                        jik_token_to_text(nd->token));
                if (!jik_type_is_allocated(nd->val_subscript_set.expr->jik_type)) {
                    continue;
                }
                JikAllocSpec spec_rhs =
                    get_expression_alloc_spec(nd->val_subscript_set.expr, spec_tab);
                // TODOY: probably need to require allocated LOCAL literal here, other regions will
                // not match
                if (jik_alloc_source_known(spec_lhs) &&
                    jik_node_is_allocated_literal(nd->val_subscript_set.expr)) {
                    JikAllocSpec as =
                        get_expression_alloc_spec(nd->val_subscript_set.node, spec_tab);
                    if (jik_alloc_spec_complete(as)) {
                        jik_set_alloc_spec(nd->val_subscript_set.expr, as);
                        continue;
                    }
                }
                if (jik_both_alloc_sources_known(spec_lhs, spec_rhs)) {
                    jik_diag_fatal_error_if(!jik_alloc_sources_match(spec_lhs, spec_rhs),
                                            jik_make_alloc_src_mismatch_msg(spec_lhs, spec_rhs),
                                            jik_token_to_text(nd->token));
                }
            }
            else if (nd->type == NODE_STMNT_MEMBER_SET) {
                JikAllocSpec spec_lhs =
                    get_expression_alloc_spec(nd->val_member_set.node, spec_tab);
                jik_diag_fatal_error_if(spec_lhs.kind == JIK_ALLOC_GLOBAL,
                                        "global composites are immutable",
                                        jik_token_to_text(nd->token));
                if (!jik_type_is_allocated(nd->val_member_set.expr->jik_type)) {
                    continue;
                }
                JikAllocSpec spec_rhs =
                    get_expression_alloc_spec(nd->val_member_set.expr, spec_tab);
                // TODOY: probably need to require allocated LOCAL literal here, other regions will
                // not match
                if (jik_alloc_source_known(spec_lhs) &&
                    jik_node_is_allocated_literal(nd->val_member_set.expr)) {
                    JikAllocSpec as = get_expression_alloc_spec(nd->val_member_set.node, spec_tab);
                    if (jik_alloc_spec_complete(as)) {
                        jik_set_alloc_spec(nd->val_member_set.expr, as);
                        continue;
                    }
                }
                if (jik_both_alloc_sources_known(spec_lhs, spec_rhs)) {
                    jik_diag_fatal_error_if(!jik_alloc_sources_match(spec_lhs, spec_rhs),
                                            jik_make_alloc_src_mismatch_msg(spec_lhs, spec_rhs),
                                            jik_token_to_text(nd->token));
                }
            }
            // else if (nd->type == NODE_EXPR_CALL && !nd->val_call.builtin) {
            else if (nd->type == NODE_EXPR_CALL) {
                if (nd->val_call.builtin && strcmp(nd->val_call.name->val_id.name, "concat") == 0) {
                    size_t       n       = VecJikNode_size(nd->val_call.args);
                    JikNode     *reg_arg = VecJikNode_get(nd->val_call.args, n - 1);
                    JikAllocSpec spec    = get_expression_alloc_spec(reg_arg, spec_tab);
                    if (jik_alloc_spec_complete(spec)) {
                        nd->val_call.alloc_spec = spec;
                    }
                    continue;
                }
                if (is_region_safe_function_call(nd)) {
                    continue;
                }

                VecJikNode *allocd_args = get_allocated_args(nd);
                size_t      n           = VecJikNode_size(allocd_args);
                if (VecJikNode_size(allocd_args) == 0) {
                    continue;
                }

                JikNode     *first_non_literal = get_first_non_literal_allocd_arg(allocd_args);
                JikNode     *first_arg         = VecJikNode_get(allocd_args, 0);
                JikAllocSpec req_spec          = first_non_literal
                                                     ? get_expression_alloc_spec(first_non_literal, spec_tab)
                                                     : get_expression_alloc_spec(first_arg, spec_tab);
                // jik_diag_fatal_error_if(req_spec.kind == JIK_ALLOC_GLOBAL, "composite globals
                // cannot be passed to functions", jik_token_to_text(first_arg->token));
                //  All arguments need to be in the same region
                for (size_t i = 1; i < n; i++) {
                    JikNode     *arg     = VecJikNode_get(allocd_args, i);
                    JikAllocSpec arg_src = get_expression_alloc_spec(arg, spec_tab);
                    if (jik_both_alloc_sources_known(arg_src, req_spec)) {
                        // We force local literals into same region as other args for better
                        // ergonomics
                        if (jik_node_is_allocated_literal(arg) &&
                            arg_src.src == JIK_ALLOC_SRC_LOCAL &&
                            req_spec.src != JIK_ALLOC_SRC_LOCAL) {
                            jik_set_alloc_spec(arg, req_spec);
                            continue;
                        }
                        jik_diag_fatal_error_if(!jik_alloc_sources_match(arg_src, req_spec),
                                                "all arguments should belong to same region",
                                                jik_token_to_text(arg->token));
                    }
                }
                // TODOY: might need adjustment for void returns
                if (jik_alloc_spec_complete(req_spec)) {
                    nd->val_call.alloc_spec = req_spec;
                }
            }
            else if (nd->type == NODE_STMNT_RETURN && nd->val_return.expr) {
                if (!jik_type_is_allocated(nd->val_return.expr->jik_type)) {
                    continue;
                }
                JikAllocSpec spec = get_expression_alloc_spec(nd->val_return.expr, spec_tab);
                if (jik_alloc_source_known(spec) && spec.src == JIK_ALLOC_SRC_LOCAL) {
                    jik_diag_fatal_error("cannot return local allocation",
                                         jik_token_to_text(nd->val_return.expr->token));
                }
                else if (jik_alloc_source_known(spec) && spec.src == JIK_ALLOC_SRC_CROSS) {
                    jik_diag_fatal_error("cannot return foreign composite value",
                                         jik_token_to_text(nd->val_return.expr->token));
                }
                else if (jik_alloc_source_known(spec) && spec.kind == JIK_ALLOC_GLOBAL) {
                    jik_diag_fatal_error("cannot return composite global",
                                         jik_token_to_text(nd->val_return.expr->token));
                }
            }
            else if (jik_node_is_allocated_literal(nd)) {
                JikAllocSpec as = get_expression_alloc_spec(nd, spec_tab);
                if (as.kind == JIK_ALLOC_NAMED_REGION || as.kind == JIK_ALLOC_CONTAINER) {
                    JikAllocSpec *cont_spec = TabJikAllocSpec_get(spec_tab, as.region_name);
                    assert(cont_spec);
                    if (jik_alloc_source_known(*cont_spec) && !jik_alloc_source_known(as)) {
                        as.src = cont_spec->src;
                        jik_set_alloc_spec(nd, as);
                    }
                }
            }
            // Iteration variables need to be handled separately.
            else if (nd->type == NODE_LOOP_FOR_IN) {
                if (!jik_type_is_allocated(nd->val_for_in.var_name->jik_type)) {
                    continue;
                }
                JikAllocSpec spec =
                    get_expression_alloc_spec(nd->val_for_in.container_expr, spec_tab);
                if (jik_alloc_source_known(spec)) {
                    TabJikAllocSpec_set(spec_tab, nd->val_for_in.var_name->val_id.name, spec);
                }
            }
            else if (nd->type == NODE_LOOP_FOR_IN_DICT) {
                JikAllocSpec spec_dict =
                    get_expression_alloc_spec(nd->val_for_in_dict.dict_expr, spec_tab);
                if (jik_alloc_source_known(spec_dict)) {
                    TabJikAllocSpec_set(
                        spec_tab, nd->val_for_in_dict.key_name->val_id.name, spec_dict);
                    if (jik_type_is_allocated(nd->val_for_in_dict.val_name->jik_type)) {
                        TabJikAllocSpec_set(
                            spec_tab, nd->val_for_in_dict.val_name->val_id.name, spec_dict);
                    }
                }
            }
            else if (nd->type == NODE_CASE) {
                if (!nd->val_case.variant->val_variant_new.init_expr ||
                    nd->val_case.variant->val_variant_new.init_expr->type != NODE_EXPR_IDENTIFIER) {
                    continue;
                }
                if (!jik_type_is_allocated(
                        nd->val_case.variant->val_variant_new.init_expr->jik_type)) {
                    continue;
                }
                JikAllocSpec spec =
                    get_expression_alloc_spec(nd->val_case.match->val_match.expr, spec_tab);
                if (jik_alloc_source_known(spec)) {
                    TabJikAllocSpec_set(
                        spec_tab,
                        nd->val_case.variant->val_variant_new.init_expr->val_id.name,
                        spec);
                }
            }
        }
    }
}

static void
check_nested_literal_consistency(JikNode *nd, TabJikAllocSpec *spec_tab)
{
    if (nd->type == NODE_EXPR_VECTOR && jik_type_is_allocated(nd->jik_type->val_vec.elem_type)) {
        if (nd->val_vector.init_elems) {
            JikNode     *first    = VecJikNode_get(nd->val_vector.init_elems, 0);
            JikAllocSpec req_spec = get_expression_alloc_spec(first, spec_tab);
            for (size_t i = 1; i < VecJikNode_size(nd->val_vector.init_elems); i++) {
                JikNode     *elem     = VecJikNode_get(nd->val_vector.init_elems, i);
                JikAllocSpec arg_spec = get_expression_alloc_spec(elem, spec_tab);
                jik_diag_fatal_error_if(!jik_alloc_sources_match(arg_spec, req_spec),
                                        "all vector elements should belong to same region",
                                        jik_token_to_text(elem->token));
            }
        }
        else {
            JikAllocSpec vec_spec  = get_expression_alloc_spec(nd, spec_tab);
            JikAllocSpec init_spec = get_expression_alloc_spec(nd->val_vector.elem_expr, spec_tab);
            jik_diag_fatal_error_if(!jik_alloc_sources_match(vec_spec, init_spec),
                                    "vector initializer should belong to same region as vector",
                                    jik_token_to_text(nd->val_vector.elem_expr->token));
        }
    }
    else if (nd->type == NODE_EXPR_DICT && nd->val_dict.init_values &&
             jik_type_is_allocated(nd->jik_type->val_dict.elem_type)) {
        JikNode *first = VecJikNode_get(nd->val_dict.init_values, 0);
        if (jik_type_is_allocated(first->jik_type)) {
            JikAllocSpec req_spec = get_expression_alloc_spec(first, spec_tab);
            for (size_t i = 1; i < VecJikNode_size(nd->val_dict.init_values); i++) {
                JikNode     *elem     = VecJikNode_get(nd->val_dict.init_values, i);
                JikAllocSpec arg_spec = get_expression_alloc_spec(elem, spec_tab);
                jik_diag_fatal_error_if(!jik_alloc_sources_match(arg_spec, req_spec),
                                        "all dictionary values should belong to same region",
                                        jik_token_to_text(elem->token));
            }
        }
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        JikAllocSpec    struct_spec = get_expression_alloc_spec(nd, spec_tab);
        TabJikNode_iter it          = TabJikNode_iter_new(nd->val_struct_new.init_vals);
        TabJikNode_item item;
        while (TabJikNode_iter_next(&it, &item)) {
            if (!jik_type_is_allocated(item.value->jik_type)) {
                continue;
            }
            JikAllocSpec val_spec = get_expression_alloc_spec(item.value, spec_tab);
            jik_diag_fatal_error_if(!jik_alloc_sources_match(struct_spec, val_spec),
                                    "all struct initializer values should belong to same region",
                                    jik_token_to_text(item.value->token));
        }
    }
}

static void
jik_post_check_region_integrity(JikNode *ast)
{
    JikNode *func_nd;
    for (size_t i = 0; i < VecJikNode_size(ast->val_program.functions); i++) {
        func_nd                     = VecJikNode_get(ast->val_program.functions, i);
        VecJikNode      *func_nodes = func_nd->val_function.subnodes;
        JikNode         *nd;
        TabJikAllocSpec *spec_tab = func_nd->val_function.info->spec_tab;
        VecJikNode_iter  it       = VecJikNode_iter_new(func_nodes);
        while (VecJikNode_iter_next(&it, &nd)) {
            if (jik_node_is_allocated_literal(nd)) {
                check_nested_literal_consistency(nd, spec_tab);
            }
            else if (nd->type == NODE_EXPR_CALL) {
                if (nd->val_call.builtin && strcmp(nd->val_call.name->val_id.name, "concat") == 0) {
                    continue;
                }
                VecJikNode *allocd_args = get_allocated_args(nd);
                size_t      n           = VecJikNode_size(allocd_args);
                if (VecJikNode_size(allocd_args) == 0) {
                    return;
                }
                JikNode     *first             = VecJikNode_get(allocd_args, 0);
                JikNode     *first_non_literal = get_first_non_literal_allocd_arg(allocd_args);
                JikAllocSpec req_spec          = first_non_literal
                                                     ? get_expression_alloc_spec(first_non_literal, spec_tab)
                                                     : get_expression_alloc_spec(first, spec_tab);
                for (size_t i = 1; i < n; i++) {
                    JikNode     *arg      = VecJikNode_get(allocd_args, i);
                    JikAllocSpec arg_spec = get_expression_alloc_spec(arg, spec_tab);
                    jik_diag_fatal_error_if(!jik_alloc_sources_match(req_spec, arg_spec),
                                            "all arguments should belong to same region",
                                            jik_token_to_text(arg->token));
                }
            }
        }
    }
}

void
jik_check_regions(JikNode *ast)
{
    jik_prepare_functions(ast);
    jik_check_orphaned_allocations(ast);
    jik_check_region_semantics(ast);

    VecJikNode *unknown_allocs = VecJikNode_new_empty();
    size_t      prev_len       = SIZE_MAX;
    size_t      n;
    while (VecJikNode_size(unknown_allocs) < prev_len) {
        jik_check_region_integrity(ast);
        unknown_allocs = jik_collect_unknown_allocs(ast);
        n              = VecJikNode_size(unknown_allocs);
        if (n == prev_len) {
            break;
        }
        else if (n < prev_len) {
            prev_len       = n;
            unknown_allocs = VecJikNode_new_empty();
        }
    }

    n = VecJikNode_size(unknown_allocs);
    if (n > 0) {
        JikNode *nd = VecJikNode_get(unknown_allocs, 0);
        jik_diag_uninferred_alloc(nd);
    }

    // This is an additional pass which can only be done after all alloc source are known.
    jik_post_check_region_integrity(ast);
    jik_classify_functions(ast);
}
