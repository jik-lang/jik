#include "codegen.h"

#include "alloc.h"
#include "charbuf.h"
#include "diag.h"
#include "regcheck.h"
#include "semantic.h"
#include "types.h"
#include "version.h"

JIK_HTAB_DEFINE(TabString, char *);

#define MANGLE_PREFIX "jikmn_"

#define JIK_REGION_TYPE_NAME       "JikRegion *"
#define JIK_REGION_VAR_NAME        "jik_local_region"
#define JIK_GLOBAL_REGION_VAR_NAME "jik_global_region"

#define JIK_C_STRUCT_PREFIX "struct "
#define JIK_C_ENUM_PREFIX   "enum "
#define JIK_C_PTR_SUFFIX    " *"

#define JIK_NULL_PRINT_EXPR   "jik_string_new(\"NULL\", a)"
#define JIK_PRINT_FUNC_SUFFIX "_tostr"
#define JIK_LEN_FUNC_SUFFIX   "_len"
#define JIK_SUBSCRIPT_SUFFIX  "_get"

// -----------------------------------------------------------------------------
// General helpers and initialization
// -----------------------------------------------------------------------------

static char *
get_alloc_dest(JikAllocSpec spec)
{
    if (spec.kind == JIK_ALLOC_LOCAL) {
        return JIK_REGION_VAR_NAME;
    }
    else if (spec.kind == JIK_ALLOC_GLOBAL) {
        return JIK_GLOBAL_REGION_VAR_NAME;
    }
    else if (spec.kind == JIK_ALLOC_CONTAINER) {
        return JIK_STRING_NCAT(spec.region_name, "->region");
    }
    else if (spec.kind == JIK_ALLOC_NAMED_REGION) {
        return JIK_STRING_NCAT(spec.region_name);
    }
    else {
        jik_diag_fatal_error("internal error: unknown allocation destination", "");
    }
}

static JikNode *
jik_codegen_make_console_arg_vec(JikContext *ctx)
{
    JikNode *vec  = jik_node_new_vector(jik_node_new_integer(NULL, NULL),
                                       jik_node_new_string("", true, false, NULL, NULL),
                                       NULL,
                                       NULL,
                                       NULL);
    vec->jik_type = ctx->args_type;
    return vec;
}

void
jik_codegen_init(JikCodeGenerator *cg, JikContext *ctx)
{
    cg->ctx                          = ctx;
    cg->ast                          = ctx->ast;
    cg->nodes                        = ctx->nodes;
    cg->print_functions              = TabString_new();
    cg->len_functions                = TabString_new();
    cg->subscript_functions          = TabString_new();
    cg->declared_vec_types           = TabBool_new();
    cg->declared_vec_struct_types    = TabBool_new();
    cg->defined_vec_types            = TabBool_new();
    cg->declared_dict_types          = TabBool_new();
    cg->declared_dict_struct_types   = TabBool_new();
    cg->declared_option_types        = TabBool_new();
    cg->declared_option_struct_types = TabBool_new();
    cg->defined_dict_types           = TabBool_new();
    cg->defined_option_types         = TabBool_new();
    cg->arg_vec = ctx->args_type ? jik_codegen_make_console_arg_vec(ctx) : NULL;
    jik_writer_init(&cg->cw);
}

static char *
sanitize_string(const char *input)
{
    CharBuffer          *out = char_buffer_new("");
    const unsigned char *src = (const unsigned char *)input;

    while (*src != '\0') {
        char buf[5];
        switch (*src) {
        case '\n':
            char_buffer_append(out, "\\n");
            break;
        case '\t':
            char_buffer_append(out, "\\t");
            break;
        case '\r':
            char_buffer_append(out, "\\r");
            break;
        case '\"':
            char_buffer_append(out, "\\\"");
            break;
        case '\\':
            char_buffer_append(out, "\\\\");
            break;
        default:
            if (*src >= 32 && *src < 127) {
                char_buffer_push(out, (char)*src);
            }
            else {
                // Emit non-ASCII and other control bytes as fixed-width octal escapes so the
                // generated C source preserves the original byte sequence.
                snprintf(buf, sizeof(buf), "\\%03o", *src);
                char_buffer_append(out, buf);
            }
            break;
        }
        src++;
    }

    return out->data;
}

static char *
jik_codegen_format_site(JikToken *tok)
{
    char *filepath = sanitize_string(tok->filepath);
    return JIK_STRING_NCAT(
        filepath, ":", size_t_to_string(tok->lineno), ":", size_t_to_string(tok->colno));
}

static char *
jik_codegen_quote_site(JikToken *tok)
{
    return JIK_STRING_NCAT("\"", jik_codegen_format_site(tok), "\"");
}

static char *
jik_codegen_format_runtime_error(JikToken *tok, char *msg)
{
    return JIK_STRING_NCAT("\"", msg, "\\n  --> ", jik_codegen_format_site(tok), "\"");
}

#define DEBUG_ARG jik_codegen_quote_site(nd->token)

static char *
jik_codegen_mangle_name(char *mod_alias, char *name)
{
    assert(mod_alias && name);
    size_t l1 = strlen(mod_alias);
    size_t l2 = strlen(name);
    return JIK_STRING_NCAT(
        MANGLE_PREFIX, size_t_to_string(l1), "_", mod_alias, "_", size_t_to_string(l2), "_", name);
}

char *
get_type_name(JikType *t)
{
    if (t->name == TYPE_STRUCT || t->name == TYPE_VECTOR || t->name == TYPE_DICT ||
        t->name == TYPE_OPTION || t->name == TYPE_VARIANT || t->name == TYPE_ENUM) {
        assert(t->mangled_name);
        return t->mangled_name;
    }
    else if (t->name == TYPE_STRING) {
        return t->C_name;
    }
    else {
        return NULL;
    }
}

static char *
jik_codegen_get_type_id_fragment(JikType *t)
{
    assert(t);
    if (t->name == TYPE_STRING) {
        return "JikString";
    }
    else if (jik_type_is_primitive(t)) {
        return t->name == TYPE_ENUM ? t->mangled_name : t->C_name;
    }
    else {
        assert(t->mangled_name);
        return t->mangled_name;
    }
}

static void
jik_codegen_register_print_function(JikCodeGenerator *cg, char *type_name, char *custom_name)
{
    char *func_name = custom_name ? custom_name : JIK_STRING_NCAT(type_name, JIK_PRINT_FUNC_SUFFIX);
    TabString_set(cg->print_functions, type_name, func_name);
}

char *
jik_codegen_get_print_function(JikCodeGenerator *cg, char *type_name)
{
    char **entry = TabString_get(cg->print_functions, type_name);
    jik_diag_fatal_error_if(!entry, "no print function registered for type", "");
    return *entry;
}

char *
jik_codegen_emit_expression(JikCodeGenerator *cg, JikNode *nd);
static void
jik_codegen_emit_block(JikCodeGenerator *cg, JikNode *nd);

// -----------------------------------------------------------------------------
// Top-level program emission helpers
// -----------------------------------------------------------------------------

static bool
jik_any_allocated_globals_defined(JikCodeGenerator *cg)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.globals); i++) {
        nd = VecJikNode_get(cg->ast->val_program.globals, i);
        if (jik_type_is_allocated(nd->val_assign.expr->jik_type)) {
            return true;
        }
    }
    return false;
}

static void
jik_codegen_emit_main_function(JikCodeGenerator *cg)
{
    jik_writer_blank_line(&cg->cw);
    jik_writer_write_line(&cg->cw, C_TYPE_NAME_INTEGER);
    jik_writer_write_line(&cg->cw, "main(int argc, char **argv)");
    jik_writer_begin_block(&cg->cw, "{");
    // initialize globals
    bool allocd_globals_defined = jik_any_allocated_globals_defined(cg);
    if (allocd_globals_defined) {
        jik_writer_write_line(
            &cg->cw,
            JIK_STRING_NCAT(
                JIK_REGION_TYPE_NAME, " ", JIK_GLOBAL_REGION_VAR_NAME, " = jik_region_new(0);"));
    }

    jik_writer_blank_line(&cg->cw);
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.globals); i++) {
        nd = VecJikNode_get(cg->ast->val_program.globals, i);
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT(nd->val_assign.id->val_id.mangled_name,
                                              " = ",
                                              jik_codegen_emit_expression(cg, nd->val_assign.expr),
                                              ";"));
    }
    jik_writer_blank_line(&cg->cw);

    char *main_arg = "";
    if (cg->ctx->args_type) {
        jik_writer_write_line(
            &cg->cw,
            JIK_STRING_NCAT(
                JIK_REGION_TYPE_NAME, " ", JIK_REGION_VAR_NAME, " = jik_region_new(0);"));
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT("MAKE_ARG_VEC(argc, argv, ", JIK_REGION_VAR_NAME, ");"));
        JikNode *main_nd = jik_scope_get_global_symbol("main", "main");
        main_arg         = VecJikNode_get(main_nd->val_function.params, 0)->val_id.name;
    }

    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT(C_TYPE_NAME_INTEGER,
                                          " jik_ret_val = ",
                                          jik_codegen_mangle_name("main", "main"),
                                          "(",
                                          main_arg,
                                          ");"));

    if (cg->ctx->args_type) {
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("jik_region_free(", JIK_REGION_VAR_NAME, ");"));
    }

    if (allocd_globals_defined) {
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT("jik_region_free(", JIK_GLOBAL_REGION_VAR_NAME, ");"));
    }

    jik_writer_write_line(&cg->cw, "return jik_ret_val;");

    jik_writer_end_block(&cg->cw);
}

static bool
is_type_print_supported(JikType *t)
{
    (void)t;
    return true;
}

static char *
jik_codegen_get_print_type_name(JikType *t)
{
    return jik_type_is_primitive(t) && t->name != TYPE_ENUM ? t->C_name : get_type_name(t);
}

// -----------------------------------------------------------------------------
// Builtin lowering
// -----------------------------------------------------------------------------

char *
get_print_seq_trans(JikCodeGenerator *cg, JikNode *nd)
{
    size_t      n  = VecJikNode_size(nd->val_call.args);
    CharBuffer *cb = char_buffer_new("");
    JikNode    *arg;
    for (size_t i = 0; i < n; i++) {
        arg = VecJikNode_get(nd->val_call.args, i);
        jik_diag_fatal_error_if(!is_type_print_supported(arg->jik_type),
                                JIK_STRING_NCAT("printing not supported for type ",
                                                jik_type_pretty_name(arg->jik_type)),
                                jik_token_to_text(arg->token));
        char *tse       = jik_codegen_emit_expression(cg, arg);
        char *type_name = jik_codegen_get_print_type_name(arg->jik_type);
        assert(type_name);
        char **entry = TabString_get(cg->print_functions, type_name);
        char  *final_expr;
        if (entry) {
            char *pf   = *entry;
            final_expr = JIK_STRING_NCAT(pf, "(", tse, ", ", JIK_REGION_VAR_NAME, ")");
        }
        else {
            char *pretty_name = sanitize_string(jik_type_pretty_name(arg->jik_type));
            final_expr        = JIK_STRING_NCAT(
                "jik_string_new(\"<", pretty_name, ">\", ", JIK_REGION_VAR_NAME, ")");
        }
        char_buffer_append(cb, final_expr);
        char_buffer_append(cb, ", ");
    }
    return cb->data;
}

char *
get_builtin_call_print(JikCodeGenerator *cg, JikNode *nd)
{
    size_t n   = VecJikNode_size(nd->val_call.args);
    char  *seq = get_print_seq_trans(cg, nd);
    return JIK_STRING_NCAT("jik_print((JikString *[]){", seq, "}", ", ", size_t_to_string(n), ")");
}

char *
get_builtin_call_println(JikCodeGenerator *cg, JikNode *nd)
{
    size_t n   = VecJikNode_size(nd->val_call.args);
    char  *seq = get_print_seq_trans(cg, nd);
    return JIK_STRING_NCAT(
        "jik_println((JikString *[]){", seq, "}", ", ", size_t_to_string(n), ")");
}

char *
get_builtin_call_concat(JikCodeGenerator *cg, JikNode *nd)
{
    size_t      n  = VecJikNode_size(nd->val_call.args);
    size_t      ns = n - 1;
    CharBuffer *cb = char_buffer_new("");
    for (size_t i = 0; i < ns; i++) {
        JikNode *arg  = VecJikNode_get(nd->val_call.args, i);
        char    *expr = jik_codegen_emit_expression(cg, arg);
        char_buffer_append(cb, expr);
        char_buffer_append(cb, ", ");
    }

    JikNode *reg    = VecJikNode_get(nd->val_call.args, n - 1);
    char    *reg_tr = jik_codegen_emit_expression(cg, reg);
    return JIK_STRING_NCAT("jik_concat((JikString *[]){",
                           cb->data,
                           "}",
                           ", ",
                           size_t_to_string(ns),
                           ", ",
                           reg_tr,
                           ")");
}

char *
get_builtin_call_assert(JikCodeGenerator *cg, JikNode *nd)
{
    char *tse      = jik_codegen_emit_expression(cg, VecJikNode_get(nd->val_call.args, 0));
    char *filepath = sanitize_string(nd->token->filepath);
    return JIK_STRING_NCAT(
        "jik_assert(", tse, ", \"", filepath, "\", ", size_t_to_string(nd->token->lineno), ");");
}

char *
get_builtin_call_push(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *vec     = VecJikNode_get(nd->val_call.args, 0);
    char    *vec_tr  = jik_codegen_emit_expression(cg, vec);
    char    *expr_tr = jik_codegen_emit_expression(cg, VecJikNode_get(nd->val_call.args, 1));
    return JIK_STRING_NCAT(
        vec->jik_type->mangled_name, "_push(", vec_tr, ", ", expr_tr, ", ", DEBUG_ARG, ")");
}

char *
get_builtin_call_pop(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *vec    = VecJikNode_get(nd->val_call.args, 0);
    char    *vec_tr = jik_codegen_emit_expression(cg, vec);
    return JIK_STRING_NCAT(vec->jik_type->mangled_name, "_pop(", vec_tr, ", ", DEBUG_ARG, ")");
}

char *
get_builtin_call_clear(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *vec    = VecJikNode_get(nd->val_call.args, 0);
    char    *vec_tr = jik_codegen_emit_expression(cg, vec);
    return JIK_STRING_NCAT(vec->jik_type->mangled_name, "_clear(", vec_tr, ")");
}

char *
get_builtin_call_len(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *expr    = VecJikNode_get(nd->val_call.args, 0);
    char    *expr_tr = jik_codegen_emit_expression(cg, expr);
    if (expr->jik_type->name == TYPE_VECTOR) {
        char *func_name = JIK_STRING_NCAT(expr->jik_type->mangled_name, "_size");
        return JIK_STRING_NCAT(func_name, "(", expr_tr, ",", DEBUG_ARG, ")");
    }
    else if (expr->jik_type->name == TYPE_STRING) {
        return JIK_STRING_NCAT("(", expr_tr, "->size)");
    }
    else if (expr->jik_type->name == TYPE_DICT) {
        char *func_name = JIK_STRING_NCAT(expr->jik_type->mangled_name, "_size");
        return JIK_STRING_NCAT(func_name, "(", expr_tr, ",", DEBUG_ARG, ")");
    }
    else {
        jik_diag_fatal_error("internal error: unsupported len builtin operand", "");
    }
}

char *
get_builtin_call_site(JikCodeGenerator *cg, JikNode *nd)
{
    char *arg1 = JIK_STRING_NCAT("\"", sanitize_string(nd->token->filepath), "\"");
    char *arg2 = JIK_STRING_NCAT("\"", sanitize_string(nd->token->codeline), "\"");
    return JIK_STRING_NCAT(
        "jik_site_new(", arg1, ", ", arg2, ", ", size_t_to_string(nd->token->lineno), ")");
}

char *
get_builtin_call_site_file(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *s    = VecJikNode_get(nd->val_call.args, 0);
    JikNode *reg  = VecJikNode_get(nd->val_call.args, 1);
    char    *s_tr = jik_codegen_emit_expression(cg, s);
    char    *r_tr = jik_codegen_emit_expression(cg, reg);
    return JIK_STRING_NCAT("jik_site_file(", s_tr, ", ", r_tr, ")");
}

char *
get_builtin_call_site_line(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *s    = VecJikNode_get(nd->val_call.args, 0);
    char    *s_tr = jik_codegen_emit_expression(cg, s);
    return JIK_STRING_NCAT("jik_site_line(", s_tr, ")");
}

char *
get_builtin_call_site_code(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *s    = VecJikNode_get(nd->val_call.args, 0);
    JikNode *reg  = VecJikNode_get(nd->val_call.args, 1);
    char    *s_tr = jik_codegen_emit_expression(cg, s);
    char    *r_tr = jik_codegen_emit_expression(cg, reg);
    return JIK_STRING_NCAT("jik_site_code(", s_tr, ", ", r_tr, ")");
}

char *
get_builtin_call_fail(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *s       = VecJikNode_get(nd->val_call.args, 0);
    char    *s_tr    = jik_codegen_emit_expression(cg, s);
    char    *code_tr = "1";
    if (VecJikNode_size(nd->val_call.args) == 2) {
        JikNode *code = VecJikNode_get(nd->val_call.args, 1);
        code_tr       = jik_codegen_emit_expression(cg, code);
    }
    return JIK_STRING_NCAT(
        "jik_error_set(jik_err_arg, ", code_tr, ", ", s_tr, "->data); goto __jik_cleanup");
}

char *
get_builtin_call_error_msg(JikCodeGenerator *cg, JikNode *nd)
{
    (void)cg;
    char *reg_tr = JIK_REGION_VAR_NAME;
    if (VecJikNode_size(nd->val_call.args) > 0) {
        JikNode *reg = VecJikNode_get(nd->val_call.args, 0);
        reg_tr       = jik_codegen_emit_expression(cg, reg);
    }
    return JIK_STRING_NCAT("jik_error_msg(&jik_catch_err, ", reg_tr, ")");
}

char *
get_builtin_call_error_code(JikCodeGenerator *cg, JikNode *nd)
{
    (void)cg;
    (void)nd;
    return "jik_error_code(&jik_catch_err)";
}

char *
get_builtin_call(JikCodeGenerator *cg, JikNode *nd)
{
    if (strcmp(nd->val_call.name->val_id.name, "print") == 0) {
        return get_builtin_call_print(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "println") == 0) {
        return get_builtin_call_println(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "concat") == 0) {
        return get_builtin_call_concat(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "assert") == 0) {
        return get_builtin_call_assert(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "push") == 0) {
        return get_builtin_call_push(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "pop") == 0) {
        return get_builtin_call_pop(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "clear") == 0) {
        return get_builtin_call_clear(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "len") == 0) {
        return get_builtin_call_len(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "site") == 0) {
        return get_builtin_call_site(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "site_file") == 0) {
        return get_builtin_call_site_file(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "site_line") == 0) {
        return get_builtin_call_site_line(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "site_code") == 0) {
        return get_builtin_call_site_code(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "fail") == 0) {
        return get_builtin_call_fail(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "error_msg") == 0) {
        return get_builtin_call_error_msg(cg, nd);
    }
    else if (strcmp(nd->val_call.name->val_id.name, "error_code") == 0) {
        return get_builtin_call_error_code(cg, nd);
    }
    else {
        return NULL;
    }
}

char *
get_callee_name(JikCodeGenerator *cg, JikNode *nd)
{
    if (!nd->val_call.name->val_id.mangled_name) {
        nd->val_call.name->val_id.mangled_name = jik_codegen_mangle_name(
            nd->val_call.name->val_id.mod_alias, nd->val_call.name->val_id.name);
    }
    return nd->val_call.extern_name ? nd->val_call.extern_name
                                    : nd->val_call.name->val_id.mangled_name;
}

char *
get_callee_args(JikCodeGenerator *cg, JikNode *nd)
{
    CharBuffer *tr = char_buffer_new("");
    JikNode    *arg;
    size_t      n = VecJikNode_size(nd->val_call.args);
    for (size_t i = 0; i < n; i++) {
        arg = VecJikNode_get(nd->val_call.args, i);
        char_buffer_append(tr, jik_codegen_emit_expression(cg, arg));
        if (i < n - 1)
            char_buffer_append(tr, ", ");
    }
    // TODO: duplicate data and free string buffer
    if (nd->val_call.auto_region) {
        char *pfx = n == 0 ? "" : ", ";
        char_buffer_append(tr, JIK_STRING_NCAT(pfx, JIK_REGION_VAR_NAME));
    }
    return tr->data;
}

char *
get_function_call(JikCodeGenerator *cg, JikNode *nd)
{
    if (nd->val_call.builtin) {
        char *bc = get_builtin_call(cg, nd);
        jik_diag_fatal_error_if(
            !bc, "dont know how to translate builtin", nd->val_call.name->val_id.name);
        return bc;
    }
    char    *func_name = get_callee_name(cg, nd);
    char    *args      = get_callee_args(cg, nd);
    JikNode *func      = jik_scope_get_function(nd->context,
                                           nd->val_call.name->val_id.name,
                                           nd->val_call.name->val_id.mod_alias,
                                           nd->token->mod_alias);
    assert(func);
    size_t n       = VecJikNode_size(nd->val_call.args);
    char  *prefix  = n > 0 ? ", " : "";
    char  *err_arg = jik_function_throws(func) ? JIK_STRING_NCAT(prefix, "&jik_catch_err") : "";
    return JIK_STRING_NCAT(func_name, "(", args, err_arg, ")");
}

char *
jik_codegen_emit_expr_call(JikCodeGenerator *cg, JikNode *nd)
{
    return get_function_call(cg, nd);
}

// -----------------------------------------------------------------------------
// Expression emission helpers
// -----------------------------------------------------------------------------

char *
get_struct_initializer_values(JikCodeGenerator *cg, JikNode *nd)
{
    // TODO: here, we need to know what the parent arena is
    TabJikNode  *init_vals   = nd->val_struct_new.init_vals;
    CharBuffer  *tr          = char_buffer_new("");
    JikNode     *struct_node = nd->val_struct_new.struct_node;
    JikAllocSpec as          = nd->val_struct_new.alloc_spec;
    for (size_t i = 0; i < VecString_size(struct_node->val_struct.field_order); i++) {
        char     *field_name    = VecString_get(struct_node->val_struct.field_order, i);
        JikNode **default_node  = TabJikNode_get(struct_node->val_struct.init_vals, field_name);
        JikNode **override_node = TabJikNode_get(init_vals, field_name);
        assert(default_node);
        JikNode *final_node = (override_node == NULL) ? *default_node : *override_node;
        if (jik_node_is_allocated_literal(final_node)) {
            jik_set_alloc_spec(final_node, as);
        }
        char *final_expr = (override_node == NULL)
                               ? jik_codegen_emit_expression(cg, *default_node)
                               : jik_codegen_emit_expression(cg, *override_node);
        char *init       = JIK_STRING_NCAT(".", field_name, " = ", final_expr);
        char_buffer_append(tr, init);
        char_buffer_append(tr, ", ");
    }
    return tr->data;
}

char *
jik_codegen_emit_expr_struct_new(JikCodeGenerator *cg, JikNode *nd)
{
    char *alloc_dest = get_alloc_dest(nd->val_struct_new.alloc_spec);
    char *mn         = jik_codegen_mangle_name(nd->val_struct_new.name->val_id.mod_alias,
                                       nd->val_struct_new.name->val_id.name);
    char *siv        = get_struct_initializer_values(cg, nd);
    return JIK_STRING_NCAT(mn, "_new(", alloc_dest, ", &(struct ", mn, "){", siv, "})");
}

char *
jik_codegen_emit_expr_variant_new(JikCodeGenerator *cg, JikNode *nd)
{
    char *alloc_dest = get_alloc_dest(nd->val_variant_new.alloc_spec);
    char *mn         = jik_codegen_mangle_name(nd->val_variant_new.name->val_id.mod_alias,
                                       nd->val_variant_new.name->val_id.name);
    char *te         = NULL;
    if (nd->val_variant_new.init_expr) {
        te = jik_codegen_emit_expression(cg, nd->val_variant_new.init_expr);
    }
    else {
        JikNode **default_expr = TabJikNode_get(
            nd->val_variant_new.variant_node->val_variant.init_vals, nd->val_variant_new.tag);
        assert(default_expr);
        te = jik_codegen_emit_expression(cg, *default_expr);
    }
    char *tag_name = JIK_STRING_NCAT(
        nd->val_variant_new.variant_node->val_variant.enum_nd->jik_type->mangled_name,
        "_",
        nd->val_variant_new.tag);
    return JIK_STRING_NCAT(mn,
                           "_new(",
                           alloc_dest,
                           ", &(struct ",
                           mn,
                           "){.tag = ",
                           tag_name,
                           ", .val.",
                           nd->val_variant_new.tag,
                           " = ",
                           te,
                           "})");
}

char *
jik_codegen_emit_expr_variant_tag_check(JikCodeGenerator *cg, JikNode *nd)
{
    char *enum_mangled_name =
        nd->val_variant_tag_check.variant_node->val_variant.enum_nd->jik_type->mangled_name;
    char *ve       = jik_codegen_emit_expression(cg, nd->val_variant_tag_check.inst_node);
    char *tag_name = JIK_STRING_NCAT(enum_mangled_name, "_", nd->val_variant_tag_check.tag);
    return JIK_STRING_NCAT("(", ve, "->tag == ", tag_name, ")");
}

char *
jik_codegen_emit_expr_string(JikCodeGenerator *cg, JikNode *nd)
{
    char *alloc_dest = get_alloc_dest(nd->val_str.alloc_spec);
    if (nd->val_str.literal) {
        (void)cg;
        return JIK_STRING_NCAT(
            "jik_string_new(", "\"", sanitize_string(nd->val_str.val), "\", ", alloc_dest, ")");
    }
    jik_diag_fatal_error("internal error: unsupported string expression form", "");
}

char *
jik_codegen_get_runtime_error_fmt(JikNode *nd, char *msg)
{
    return jik_codegen_format_runtime_error(nd->token, msg);
}

char *
jik_codegen_emit_expr_member_access(JikCodeGenerator *cg, JikNode *nd)
{
    char *sobj        = jik_codegen_emit_expression(cg, nd->val_member_access.node);
    char *member_name = nd->val_member_access.member_name;
    if (nd->val_member_access.node->jik_type->name == TYPE_STRUCT) {
        return JIK_STRING_NCAT(sobj, "->", member_name);
    }
    else {
        jik_diag_fatal_error("internal error: unsupported member access operand", "");
    }
}

char *
get_vec_name(JikType *vec_type)
{
    assert(vec_type->name == TYPE_VECTOR);
    char    *name      = "vec_";
    JikType *elem_type = vec_type->val_vec.elem_type;
    while (!jik_type_is_primitive(elem_type)) {
        if (elem_type->name == TYPE_STRUCT || elem_type->name == TYPE_VARIANT ||
            elem_type->name == TYPE_DICT || elem_type->name == TYPE_OPTION) {
            assert(elem_type->mangled_name);
            name = JIK_STRING_NCAT(name, elem_type->mangled_name);
            return name;
        }
        else if (elem_type->name == TYPE_STRING) {
            return JIK_STRING_NCAT(name, jik_codegen_get_type_id_fragment(elem_type));
        }
        else if (elem_type->name == TYPE_VECTOR) {
            name      = JIK_STRING_NCAT(name, "vec_");
            elem_type = elem_type->val_vec.elem_type;
        }
    }
    return JIK_STRING_NCAT(name, jik_codegen_get_type_id_fragment(elem_type));
}

char *
get_dict_name(JikType *vec_type)
{
    assert(vec_type->name == TYPE_DICT);
    char    *name      = "dict_";
    JikType *elem_type = vec_type->val_dict.elem_type;
    while (!jik_type_is_primitive(elem_type)) {
        if (elem_type->name == TYPE_STRUCT || elem_type->name == TYPE_VARIANT ||
            elem_type->name == TYPE_VECTOR || elem_type->name == TYPE_OPTION) {
            assert(elem_type->mangled_name);
            name = JIK_STRING_NCAT(name, elem_type->mangled_name);
            return name;
        }
        else if (elem_type->name == TYPE_STRING) {
            return JIK_STRING_NCAT(name, jik_codegen_get_type_id_fragment(elem_type));
        }
        else if (elem_type->name == TYPE_DICT) {
            name      = JIK_STRING_NCAT(name, "dict_");
            elem_type = elem_type->val_dict.elem_type;
        }
    }
    return JIK_STRING_NCAT(name, jik_codegen_get_type_id_fragment(elem_type));
}

char *
get_option_name(JikType *opt_type)
{
    assert(opt_type->name == TYPE_OPTION);
    char    *name      = "opt_";
    JikType *elem_type = opt_type->val_option.elem_type;
    while (!jik_type_is_primitive(elem_type)) {
        if (elem_type->name == TYPE_STRUCT || elem_type->name == TYPE_VARIANT ||
            elem_type->name == TYPE_VECTOR || elem_type->name == TYPE_DICT ||
            elem_type->name == TYPE_OPTION) {
            assert(elem_type->mangled_name);
            name = JIK_STRING_NCAT(name, elem_type->mangled_name);
            return name;
        }
        else if (elem_type->name == TYPE_STRING) {
            return JIK_STRING_NCAT(name, jik_codegen_get_type_id_fragment(elem_type));
        }
    }
    return JIK_STRING_NCAT(name, jik_codegen_get_type_id_fragment(elem_type));
}

static void
jik_codegen_prepare_type_name(JikType *t)
{
    if (!t || t->C_name) {
        return;
    }
    if (jik_type_is_primitive(t) || t->name == TYPE_STRING || t->name == TYPE_VOID ||
        t->name == TYPE_REGION || t->name == TYPE_SITE) {
        return;
    }
    if (t->name == TYPE_VECTOR) {
        jik_codegen_prepare_type_name(t->val_vec.elem_type);
        t->mangled_name = get_vec_name(t);
        t->C_name       = JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, t->mangled_name, JIK_C_PTR_SUFFIX);
    }
    else if (t->name == TYPE_DICT) {
        jik_codegen_prepare_type_name(t->val_dict.elem_type);
        t->mangled_name = get_dict_name(t);
        t->C_name       = JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, t->mangled_name, JIK_C_PTR_SUFFIX);
    }
    else if (t->name == TYPE_OPTION) {
        jik_codegen_prepare_type_name(t->val_option.elem_type);
        t->mangled_name = get_option_name(t);
        t->C_name       = JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, t->mangled_name, JIK_C_PTR_SUFFIX);
    }
}

// TODO: refactor this, make it more general, register getter functions per type
char *
jik_codegen_emit_expr_subscript_get(JikCodeGenerator *cg, JikNode *nd)
{
    char *node = jik_codegen_emit_expression(cg, nd->val_subscript_get.node);

    if (nd->val_subscript_get.node->jik_type->name == TYPE_VECTOR) {
        char *expr = jik_codegen_emit_expression(cg, nd->val_subscript_get.expr);
        if (!nd->val_subscript_get.node->jik_type->mangled_name) {
            nd->val_subscript_get.node->jik_type->mangled_name =
                get_vec_name(nd->val_subscript_get.node->jik_type);
        }
        char *getter_name =
            JIK_STRING_NCAT(nd->val_subscript_get.node->jik_type->mangled_name, "_get");
        char *get_expr = JIK_STRING_NCAT(getter_name, "(", node, ", ", expr, ", ", DEBUG_ARG, ")");
        return get_expr;
    }
    else if (nd->val_subscript_get.node->jik_type->name == TYPE_STRUCT) {
        char **entry = TabString_get(cg->subscript_functions,
                                     nd->val_subscript_get.node->jik_type->mangled_name);
        jik_diag_fatal_error_if(
            !entry, "no sub function registered for type", jik_token_to_text(nd->token));
        char *func_name = *entry;
        char *expr      = jik_codegen_emit_expression(cg, nd->val_subscript_get.expr);
        return JIK_STRING_NCAT(func_name, "(", node, ", ", expr, ")");
    }
    else if (nd->val_subscript_get.node->jik_type->name == TYPE_STRING) {
        char *expr = jik_codegen_emit_expression(cg, nd->val_subscript_get.expr);
        return JIK_STRING_NCAT("jik_string_get", "(", node, ", ", expr, ",", DEBUG_ARG, ")");
    }
    else if (nd->val_subscript_get.node->jik_type->name == TYPE_DICT) {
        char *expr = jik_codegen_emit_expression(cg, nd->val_subscript_get.expr);
        char *getter_name =
            JIK_STRING_NCAT(nd->val_subscript_get.node->jik_type->mangled_name, "_get");
        char *get_expr = JIK_STRING_NCAT(getter_name, "(", node, ", ", expr, ", ", DEBUG_ARG, ")");
        return get_expr;
    }
    else if (nd->val_subscript_get.expr->type == NODE_VARIANT_TAG) {
        assert(nd->val_subscript_get.node->jik_type->name == TYPE_VARIANT);
        JikNode *var_node          = nd->val_subscript_get.node;
        char    *enum_mangled_name = var_node->jik_type->val_variant.enum_type->mangled_name;
        char    *tag_name          = JIK_STRING_NCAT(
            enum_mangled_name, "_", nd->val_subscript_get.expr->val_variant_tag.tag);
        char *mn = nd->val_subscript_get.node->jik_type->mangled_name;
        assert(mn);
        char *accessor = JIK_STRING_NCAT(mn, "_access");
        char *rte_txt  = jik_codegen_get_runtime_error_fmt(nd, "illegal variant payload access");
        return JIK_STRING_NCAT(accessor,
                               "(",
                               node,
                               ", ",
                               tag_name,
                               ", ",
                               rte_txt,
                               ")->val.",
                               nd->val_subscript_get.expr->val_variant_tag.tag);
    }
    else {
        jik_diag_fatal_error("internal error: unsupported subscript get operand", "");
    }
}

static void
jik_codegen_emit_stmnt_subscript_set(JikCodeGenerator *cg, JikNode *nd)
{
    char *node = jik_codegen_emit_expression(cg, nd->val_subscript_set.node);
    char *sub  = nd->val_subscript_set.sub_expr->type == NODE_VARIANT_TAG
                     ? NULL
                     : jik_codegen_emit_expression(cg, nd->val_subscript_set.sub_expr);
    char *expr = jik_codegen_emit_expression(cg, nd->val_subscript_set.expr);
    if (nd->val_subscript_set.node->jik_type->name == TYPE_VECTOR) {
        if (!nd->val_subscript_set.node->jik_type->mangled_name) {
            nd->val_subscript_set.node->jik_type->mangled_name =
                get_vec_name(nd->val_subscript_set.node->jik_type);
        }
        char *setter_name =
            JIK_STRING_NCAT(nd->val_subscript_set.node->jik_type->mangled_name, "_set");
        jik_writer_write_line(
            &cg->cw,
            JIK_STRING_NCAT(setter_name, "(", node, ", ", sub, ", ", expr, ",", DEBUG_ARG, ");"));
    }
    else if (nd->val_subscript_set.node->jik_type->name == TYPE_DICT) {
        assert(nd->val_subscript_set.node->jik_type->mangled_name);
        char *setter_name =
            JIK_STRING_NCAT(nd->val_subscript_set.node->jik_type->mangled_name, "_set");
        // TODO: fix ownership here
        jik_writer_write_line(
            &cg->cw,
            JIK_STRING_NCAT(setter_name, "(", node, ", ", sub, ", ", expr, ", ", DEBUG_ARG, ");"));
    }
    else if (nd->val_subscript_set.node->jik_type->name == TYPE_VARIANT) {
        JikNode *var_node          = nd->val_subscript_set.node;
        char    *enum_mangled_name = var_node->jik_type->val_variant.enum_type->mangled_name;
        char    *tag_name          = JIK_STRING_NCAT(
            enum_mangled_name, "_", nd->val_subscript_set.sub_expr->val_variant_tag.tag);
        char *mn = nd->val_subscript_set.node->jik_type->mangled_name;
        assert(mn);
        char *accessor = JIK_STRING_NCAT(mn, "_access");
        char *rte_txt  = jik_codegen_get_runtime_error_fmt(nd, "illegal variant payload access");
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT(accessor,
                                              "(",
                                              node,
                                              ", ",
                                              tag_name,
                                              ", ",
                                              rte_txt,
                                              ")->val.",
                                              nd->val_subscript_set.sub_expr->val_variant_tag.tag,
                                              " = ",
                                              expr,
                                              ";"));
    }
    else {
        jik_diag_fatal_error("internal error: unsupported subscript set operand", "");
    }
}

static void
jik_codegen_emit_stmnt_loop_while(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_begin_block(
        &cg->cw,
        JIK_STRING_NCAT("while (", jik_codegen_emit_expression(cg, nd->val_if.expr), ") {"));
    jik_codegen_emit_block(cg, nd->val_if.body);
    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_stmnt_loop_for(JikCodeGenerator *cg, JikNode *nd)
{
    char *start    = jik_codegen_emit_expression(cg, nd->val_for.start_expr);
    char *end      = jik_codegen_emit_expression(cg, nd->val_for.end_expr);
    char *var_name = nd->val_for.var_name->val_id.name;
    jik_writer_begin_block(&cg->cw,
                           JIK_STRING_NCAT("for (int ",
                                           var_name,
                                           " = ",
                                           start,
                                           "; ",
                                           var_name,
                                           " < ",
                                           end,
                                           "; ",
                                           var_name,
                                           "++) {"));
    jik_codegen_emit_block(cg, nd->val_for.body);
    jik_writer_end_block(&cg->cw);
}

char *
get_access_op(JikNode *cont_expr)
{
    if (cont_expr->jik_type->name == TYPE_STRING) {
        return "->";
    }
    else if (cont_expr->jik_type->name == TYPE_VECTOR) {
        return "->";
    }
    else {
        return NULL;
    }
}

static void
jik_codegen_emit_stmnt_loop_for_in(JikCodeGenerator *cg, JikNode *nd)
{
    char *var_name = nd->val_for_in.var_name->val_id.name;
    char *cont =
        JIK_STRING_NCAT("(", jik_codegen_emit_expression(cg, nd->val_for_in.container_expr), ")");
    char *jik_idx_name = "__jik_i";
    char *op           = get_access_op(nd->val_for_in.container_expr);
    assert(op);
    JikType *container_elem_type =
        jik_type_get_iterable_elem_type(nd->val_for_in.container_expr->jik_type);
    assert(container_elem_type);
    char *elem_type = container_elem_type->C_name;
    jik_writer_begin_block(&cg->cw,
                           JIK_STRING_NCAT("for (size_t ",
                                           jik_idx_name,
                                           " = 0; ",
                                           jik_idx_name,
                                           " < ",
                                           cont,
                                           op,
                                           "size; ",
                                           jik_idx_name,
                                           "++) {"));
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(elem_type, " ", var_name, " = ", cont, op, "data[", jik_idx_name, "];"));
    jik_codegen_emit_block(cg, nd->val_for_in.body);
    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_stmnt_loop_for_in_dict(JikCodeGenerator *cg, JikNode *nd)
{
    char *dict =
        JIK_STRING_NCAT("(", jik_codegen_emit_expression(cg, nd->val_for_in_dict.dict_expr), ")");
    char    *key_name      = nd->val_for_in_dict.key_name->val_id.name;
    char    *val_name      = nd->val_for_in_dict.val_name->val_id.name;
    char    *jik_idx_name  = "__jik_i";
    char    *key_type_name = (&JIK_TYPE_STRING)->C_name;
    JikType *dict_elem_type =
        jik_type_get_iterable_elem_type(nd->val_for_in_dict.dict_expr->jik_type);
    assert(dict_elem_type);
    char *val_type_name = dict_elem_type->C_name;
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("for (size_t ",
                                          jik_idx_name,
                                          " = 0; ",
                                          jik_idx_name,
                                          " < ",
                                          dict,
                                          "->capacity;",
                                          jik_idx_name,
                                          "++) {"));
    jik_writer_indent(&cg->cw);
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(
            "if (", dict, "->items[", jik_idx_name, "].key == NULL", ") { continue; }"));
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(
            key_type_name, " ", key_name, " = ", dict, "->items[", jik_idx_name, "].key;"));
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(
            val_type_name, " ", val_name, " = ", dict, "->items[", jik_idx_name, "].val;"));
    jik_codegen_emit_block(cg, nd->val_for_in_dict.body);
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_stmnt_break(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_write_line(&cg->cw, "break;");
}

static void
jik_codegen_emit_stmnt_continue(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_write_line(&cg->cw, "continue;");
}

static void
jik_codegen_emit_stmnt_match(JikCodeGenerator *cg, JikNode *nd)
{
    char  *var_expr = jik_codegen_emit_expression(cg, nd->val_match.expr);
    size_t n        = VecJikNode_size(nd->val_match.cases);
    assert(n > 0);
    for (size_t i = 0; i < n; i++) {
        char    *prefix   = i == 0 ? "" : "else ";
        JikNode *case_nd  = VecJikNode_get(nd->val_match.cases, i);
        JikNode *var_node = case_nd->val_case.variant;
        char    *enum_mangled_name =
            var_node->val_variant_new.variant_node->val_variant.enum_nd->jik_type->mangled_name;
        char *tag_name = JIK_STRING_NCAT(enum_mangled_name, "_", var_node->val_variant_new.tag);
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT(prefix, "if (", var_expr, "->tag == ", tag_name, ") {"));
        jik_writer_indent(&cg->cw);
        if (var_node->val_variant_new.init_expr &&
            var_node->val_variant_new.init_expr->type == NODE_EXPR_IDENTIFIER) {
            char    *local_name = var_node->val_variant_new.init_expr->val_id.name;
            JikNode *s = jik_scope_get_local_symbol(case_nd->val_case.body->context, local_name);
            assert(s);
            char *type_name = s->jik_type->C_name;
            jik_writer_write_line(&cg->cw,
                                  JIK_STRING_NCAT(type_name,
                                                  " ",
                                                  local_name,
                                                  " = ",
                                                  var_expr,
                                                  "->val.",
                                                  var_node->val_variant_new.tag,
                                                  ";"));
        }
        jik_codegen_emit_block(cg, case_nd->val_case.body);
        jik_writer_dedent(&cg->cw);
        jik_writer_write_line(&cg->cw, "}");
    }
}

char *
jik_codegen_emit_expr_ternary(JikCodeGenerator *cg, JikNode *nd)
{
    char *condition = jik_codegen_emit_expression(cg, nd->val_ternary.condition);
    char *expr_if   = jik_codegen_emit_expression(cg, nd->val_ternary.expr_if);
    char *expr_else = jik_codegen_emit_expression(cg, nd->val_ternary.expr_else);
    return JIK_STRING_NCAT("((", condition, ") ? (", expr_if, ") : (", expr_else, "))");
}

char *
jik_codegen_emit_expr_unop(JikCodeGenerator *cg, JikNode *nd)
{
    char *te = jik_codegen_emit_expression(cg, nd->val_unop.expr);
    if (strcmp(nd->val_unop.val, "not") == 0) {
        return JIK_STRING_NCAT("!", te);
    }
    else if (strcmp(nd->val_unop.val, "-") == 0) {
        return JIK_STRING_NCAT("-", te);
    }
    else {
        jik_diag_fatal_error("internal error: unsupported unary operator", "");
    }
    jik_diag_fatal_error("internal error: unreachable unary operator fallthrough", "");
}

char *
jik_codegen_emit_expr_binop(JikCodeGenerator *cg, JikNode *nd)
{
    char *l = jik_codegen_emit_expression(cg, nd->val_binop.left);
    char *r = jik_codegen_emit_expression(cg, nd->val_binop.right);
    if (nd->val_binop.left->jik_type == &JIK_TYPE_STRING &&
        nd->val_binop.right->jik_type == &JIK_TYPE_STRING) {
        if (strcmp(nd->val_binop.val, "==") == 0) {
            // TODO: maybe we can also call strcmp directly with s.data. Maybe a macro like
            // AS_CHAR_PTR
            return JIK_STRING_NCAT("jik_string_cmp(", l, ", ", r, ") == 0");
        }
        if (strcmp(nd->val_binop.val, "!=") == 0) {
            return JIK_STRING_NCAT("jik_string_cmp(", l, ", ", r, ") != 0");
        }
    }
    if (jik_type_equal(nd->val_binop.left->jik_type, &JIK_TYPE_BOOL)) {
        if (strcmp(nd->val_binop.val, "and") == 0) {
            return JIK_STRING_NCAT(l, " && ", r);
        }
        if (strcmp(nd->val_binop.val, "or") == 0) {
            return JIK_STRING_NCAT(l, " || ", r);
        }
    }
    return JIK_STRING_NCAT(l, " ", nd->val_binop.val, " ", r);
}

char *
jik_codegen_get_identifier(JikCodeGenerator *cg, JikNode *nd)
{
    if (nd->val_id.mod_alias) {
        return jik_codegen_mangle_name(nd->val_id.mod_alias, nd->val_id.name);
    }
    if (jik_scope_get_local_symbol(nd->context, nd->val_id.name)) {
        return nd->val_id.name;
    }
    return jik_codegen_mangle_name(nd->token->mod_alias, nd->val_id.name);
}

char *
jik_codegen_node_vec_to_initializer_list(JikCodeGenerator *cg, VecJikNode *v)
{
    size_t      n         = VecJikNode_size(v);
    JikNode    *elem_expr = VecJikNode_get(v, 0);
    CharBuffer *r         = char_buffer_new("((");
    char_buffer_append(r, elem_expr->jik_type->C_name);
    char_buffer_append(r, "[]){");
    for (size_t i = 0; i < n; i++) {
        char *expr = jik_codegen_emit_expression(cg, VecJikNode_get(v, i));
        char_buffer_append(r, expr);
        char_buffer_append(r, ", ");
    }
    char_buffer_append(r, "})");
    return r->data;
}

char *
jik_codegen_emit_expr_vector(JikCodeGenerator *cg, JikNode *nd)
{
    char *alloc_dest = get_alloc_dest(nd->val_vector.alloc_spec);
    if (nd->val_vector.init_elems) {
        char *initializer_list =
            jik_codegen_node_vec_to_initializer_list(cg, nd->val_vector.init_elems);
        size_t n         = VecJikNode_size(nd->val_vector.init_elems);
        char  *n_str     = size_t_to_string(n);
        char  *func_name = JIK_STRING_NCAT(nd->jik_type->mangled_name, "_new_from");
        return JIK_STRING_NCAT(
            func_name, "(", alloc_dest, ", ", n_str, ", ", initializer_list, ")");
    }
    else {
        return JIK_STRING_NCAT("JIK_MAKE_VEC(",
                               alloc_dest,
                               ", ",
                               nd->jik_type->mangled_name,
                               ", ",
                               jik_codegen_emit_expression(cg, nd->val_vector.size_expr),
                               ", ",
                               jik_codegen_emit_expression(cg, nd->val_vector.elem_expr),
                               ")");
    }
}

char *
jik_codegen_emit_expr_dict(JikCodeGenerator *cg, JikNode *nd)
{
    char *alloc_dest = get_alloc_dest(nd->val_dict.alloc_spec);
    if (nd->val_dict.init_values) {
        char  *func_name = JIK_STRING_NCAT(nd->jik_type->mangled_name, "_new_from");
        size_t n         = VecJikNode_size(nd->val_dict.init_keys);
        char  *item_type_name =
            JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, nd->jik_type->mangled_name, "_item");
        CharBuffer *r = char_buffer_new("(");
        char_buffer_append(r, item_type_name);
        char_buffer_append(r, "[]){");
        char *cast = JIK_STRING_NCAT("(", item_type_name, ")");
        for (size_t i = 0; i < n; i++) {
            char *key_expr =
                jik_codegen_emit_expression(cg, VecJikNode_get(nd->val_dict.init_keys, i));
            char *val_expr =
                jik_codegen_emit_expression(cg, VecJikNode_get(nd->val_dict.init_values, i));
            char_buffer_append(r, cast);
            char_buffer_append(r, "{");
            char_buffer_append(r, JIK_STRING_NCAT(".key = ", key_expr, ", .val = ", val_expr));
            char_buffer_append(r, "}, ");
        }
        char_buffer_append(r, "}");
        char *n_str = size_t_to_string(n);
        return JIK_STRING_NCAT(
            func_name, "(", alloc_dest, ", ", r->data, ", ", n_str, ",", DEBUG_ARG, ")");
    }
    else {
        return JIK_STRING_NCAT(nd->jik_type->mangled_name, "_new(", alloc_dest, ")");
    }
}

char *
jik_codegen_emit_expr_enum_new(JikCodeGenerator *cg, JikNode *nd)
{
    // TODO: Effectively mangling enumerators at multiple places, also seems nd->val_enum_new->name
    // is not needed at all?
    return JIK_STRING_NCAT(nd->jik_type->mangled_name, "_", nd->val_enum_new.enumerator);
}

char *
jik_codegen_emit_expr_regionof(JikCodeGenerator *cg, JikNode *nd)
{
    return JIK_STRING_NCAT(nd->token->lexeme, "->region");
}

char *
jik_codegen_emit_expr_must(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *call_expr = nd->val_must.expr;
    assert(call_expr->type == NODE_EXPR_CALL);
    char *func_name = get_callee_name(cg, call_expr);
    char *args      = get_callee_args(cg, call_expr);
    char *msg       = jik_codegen_format_runtime_error(nd->token, "must failed");
    char *prefix    = VecJikNode_size(call_expr->val_call.args) > 0 ? ", " : "";
    return JIK_STRING_NCAT(func_name, "_must", "(", args, prefix, msg, ")");
}

char *
jik_codegen_emit_expr_local_region(JikCodeGenerator *cg, JikNode *nd)
{
    return JIK_REGION_VAR_NAME;
}

char *
jik_codegen_emit_expr_option_some(JikCodeGenerator *cg, JikNode *nd)
{
    char *expr       = jik_codegen_emit_expression(cg, nd->val_option_some.expr);
    char *alloc_dest = get_alloc_dest(nd->val_option_some.alloc_spec);
    return JIK_STRING_NCAT(nd->jik_type->mangled_name, "_some(", expr, ", ", alloc_dest, ")");
}

char *
jik_codegen_emit_expr_option_none(JikCodeGenerator *cg, JikNode *nd)
{
    char *alloc_dest = get_alloc_dest(nd->val_option_none.alloc_spec);
    return JIK_STRING_NCAT(nd->jik_type->mangled_name, "_none(", alloc_dest, ")");
}

char *
jik_codegen_emit_expr_option_unwrap(JikCodeGenerator *cg, JikNode *nd)
{
    char *expr = jik_codegen_emit_expression(cg, nd->val_option_unwrap.expr);
    char *msg  = jik_codegen_get_runtime_error_fmt(nd, "illegal unwrap of None");
    return JIK_STRING_NCAT(
        nd->val_option_unwrap.expr->jik_type->mangled_name, "_unwrap(", expr, ", ", msg, ")");
}

char *
jik_codegen_emit_expr_option_is(JikCodeGenerator *cg, JikNode *nd)
{
    char *expr   = jik_codegen_emit_expression(cg, nd->val_option_is.expr);
    char *suffix = nd->val_option_is.is_some ? "_is_some(" : "_is_none(";
    return JIK_STRING_NCAT(nd->val_option_is.expr->jik_type->mangled_name, suffix, expr, ")");
}

static char *
escape_char(char c)
{
    char *buf = jik_alloc(8);
    switch (c) {
    case '\n':
        return "\\n";
    case '\t':
        return "\\t";
    case '\r':
        return "\\r";
    case '\b':
        return "\\b";
    case '\f':
        return "\\f";
    case '\v':
        return "\\v";
    case '\\':
        return "\\\\";
    case '\'':
        return "\\'";
    case '\"':
        return "\\\"";
    case '\0':
        return "\\0";
    default:
        if (c >= 32 && c < 127) {
            buf[0] = c;
            buf[1] = '\0';
        }
        else {
            snprintf(buf, 8, "\\%03o", (unsigned char)c);
        }
        return buf;
    }
}

char *
jik_codegen_emit_expression(JikCodeGenerator *cg, JikNode *nd)
{
    // TODO: maybe we only need lexemes, and don't need val_int in ast node!
    if (nd->type == NODE_EXPR_INTEGER) {
        return nd->token->lexeme;
    }
    else if (nd->type == NODE_EXPR_FLOAT) {
        return nd->token->lexeme;
    }
    else if (nd->type == NODE_EXPR_BOOL) {
        char *ret = nd->val_bool ? "true" : "false";
        return ret;
    }
    else if (nd->type == NODE_EXPR_CHAR) {
        char *ch_str = escape_char(nd->val_char);
        return JIK_STRING_NCAT("\'", ch_str, "\'");
    }
    else if (nd->type == NODE_EXPR_GROUPING) {
        char *te = jik_codegen_emit_expression(cg, nd->val_grouping);
        return JIK_STRING_NCAT("(", te, ")");
    }
    else if (nd->type == NODE_EXPR_IDENTIFIER) {
        return jik_codegen_get_identifier(cg, nd);
    }
    else if (nd->type == NODE_EXPR_CALL) {
        return jik_codegen_emit_expr_call(cg, nd);
    }
    else if (nd->type == NODE_EXPR_TERNARY) {
        return jik_codegen_emit_expr_ternary(cg, nd);
    }
    else if (nd->type == NODE_EXPR_BINOP) {
        return jik_codegen_emit_expr_binop(cg, nd);
    }
    else if (nd->type == NODE_EXPR_UNOP) {
        return jik_codegen_emit_expr_unop(cg, nd);
    }
    else if (nd->type == NODE_EXPR_STRUCT_NEW) {
        return jik_codegen_emit_expr_struct_new(cg, nd);
    }
    else if (nd->type == NODE_EXPR_VARIANT_NEW) {
        return jik_codegen_emit_expr_variant_new(cg, nd);
    }
    else if (nd->type == NODE_EXPR_VARIANT_TAG_CHECK) {
        return jik_codegen_emit_expr_variant_tag_check(cg, nd);
    }
    else if (nd->type == NODE_EXPR_MEMBER_ACCESS) {
        return jik_codegen_emit_expr_member_access(cg, nd);
    }
    else if (nd->type == NODE_EXPR_STRING) {
        return jik_codegen_emit_expr_string(cg, nd);
    }
    else if (nd->type == NODE_EXPR_VECTOR) {
        return jik_codegen_emit_expr_vector(cg, nd);
    }
    else if (nd->type == NODE_EXPR_DICT) {
        return jik_codegen_emit_expr_dict(cg, nd);
    }
    else if (nd->type == NODE_EXPR_SUBSCRIPT_GET) {
        return jik_codegen_emit_expr_subscript_get(cg, nd);
    }
    else if (nd->type == NODE_EXPR_ENUM_NEW) {
        return jik_codegen_emit_expr_enum_new(cg, nd);
    }
    else if (nd->type == NODE_EXPR_REGIONOF) {
        return jik_codegen_emit_expr_regionof(cg, nd);
    }
    else if (nd->type == NODE_EXPR_MUST) {
        return jik_codegen_emit_expr_must(cg, nd);
    }
    else if (nd->type == NODE_EXPR_OPTION_SOME) {
        return jik_codegen_emit_expr_option_some(cg, nd);
    }
    else if (nd->type == NODE_EXPR_OPTION_NONE) {
        return jik_codegen_emit_expr_option_none(cg, nd);
    }
    else if (nd->type == NODE_EXPR_OPTION_UNWRAP) {
        return jik_codegen_emit_expr_option_unwrap(cg, nd);
    }
    else if (nd->type == NODE_EXPR_OPTION_IS) {
        return jik_codegen_emit_expr_option_is(cg, nd);
    }
    else if (nd->type == NODE_EXPR_LOCAL_REGION) {
        return jik_codegen_emit_expr_local_region(cg, nd);
    }
    else {
        jik_node_print(nd, 0);
        jik_diag_fatal_error("expr: no translator for astnode", "");
    }
}

// -----------------------------------------------------------------------------
// Statement emission
// -----------------------------------------------------------------------------

static void
jik_codegen_emit_stmnt_member_set(JikCodeGenerator *cg, JikNode *nd)
{
    char *field_accessor = nd->val_member_set.member_name;
    char *sobj           = jik_codegen_emit_expression(cg, nd->val_member_set.node);
    char *expr           = jik_codegen_emit_expression(cg, nd->val_member_set.expr);

    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(sobj, "->", field_accessor, " = ", expr, ";"));
}

static void
jik_codegen_emit_stmnt_return(JikCodeGenerator *cg, JikNode *nd)
{
    if (nd->val_return.expr) {
        char *expr = jik_codegen_emit_expression(cg, nd->val_return.expr);
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(" __jik_res = ", expr, ";"));
    }
    jik_writer_write_line(&cg->cw, "goto __jik_cleanup;");
}

static bool
fun_has_allocs(JikNode *nd)
{
    return nd->val_function.info->has_allocs;
}

static void
jik_codegen_emit_prologue(JikCodeGenerator *cg, JikNode *nd)
{
    if (!fun_has_allocs(nd)) {
        return;
    }
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(JIK_REGION_TYPE_NAME, " ", JIK_REGION_VAR_NAME, " = jik_region_new(0);"));
}

static void
jik_codegen_emit_cleanup(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "__jik_cleanup:");
    jik_writer_indent(&cg->cw);
    if (fun_has_allocs(nd)) {
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("jik_region_free(", JIK_REGION_VAR_NAME, ");"));
    }
    char *ret_var =
        jik_type_equal(nd->jik_type->val_func.ret_type, &JIK_TYPE_VOID) ? "" : "__jik_res";
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("return ", ret_var, ";"));
}

static void
jik_codegen_emit_stmnt_assign(JikCodeGenerator *cg, JikNode *nd)
{
    char *te      = jik_codegen_emit_expression(cg, nd->val_assign.expr);
    char *id_name = nd->val_assign.id->val_id.mod_alias
                        ? jik_codegen_mangle_name(nd->val_assign.id->val_id.mod_alias,
                                                  nd->val_assign.id->val_id.name)
                        : nd->val_assign.id->val_id.name;

    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(id_name, " = ", te, ";", NULL));
}

static void
jik_codegen_emit_stmnt_declare(JikCodeGenerator *cg, JikNode *nd)
{
    char *te      = jik_codegen_emit_expression(cg, nd->val_declare.expr);
    char *id_name = nd->val_declare.id->val_id.mod_alias
                        ? jik_codegen_mangle_name(nd->val_declare.id->val_id.mod_alias,
                                                  nd->val_declare.id->val_id.name)
                        : nd->val_declare.id->val_id.name;

    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(
            nd->val_declare.expr->jik_type->C_name, " ", id_name, " = ", te, ";", NULL));
}

static void
jik_codegen_emit_cond_if(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_begin_block(
        &cg->cw,
        JIK_STRING_NCAT("if (", jik_codegen_emit_expression(cg, nd->val_if.expr), ") {", NULL));
    jik_codegen_emit_block(cg, nd->val_if.body);
    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_cond_ifelse(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_begin_block(
        &cg->cw,
        JIK_STRING_NCAT("if (", jik_codegen_emit_expression(cg, nd->val_ifelse.expr), ") {"));
    jik_codegen_emit_block(cg, nd->val_ifelse.body_if);
    jik_writer_end_block(&cg->cw);
    jik_writer_begin_block(&cg->cw, "else {");
    jik_codegen_emit_block(cg, nd->val_ifelse.body_else);
    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_cond_ifelif(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_begin_block(
        &cg->cw,
        JIK_STRING_NCAT("if (", jik_codegen_emit_expression(cg, nd->val_ifelif.expr), ") {"));
    jik_codegen_emit_block(cg, nd->val_ifelif.body_if);
    jik_writer_end_block(&cg->cw);

    assert(VecJikNode_size(nd->val_ifelif.elifs) % 2 == 0);
    VecJikNode_iter it = VecJikNode_iter_new(nd->val_ifelif.elifs);
    JikNode        *cond;
    while (VecJikNode_iter_next(&it, &cond)) {
        jik_writer_begin_block(
            &cg->cw, JIK_STRING_NCAT("else if (", jik_codegen_emit_expression(cg, cond), ") {"));
        JikNode *body;
        JIK_VEC_FATAL_ERROR_IF(!VecJikNode_iter_next(&it, &body), "missing elif body");
        jik_codegen_emit_block(cg, body);
        jik_writer_end_block(&cg->cw);
    }

    if (nd->val_ifelif.body_else != NULL) {
        jik_writer_begin_block(&cg->cw, "else {");
        jik_codegen_emit_block(cg, nd->val_ifelif.body_else);
        jik_writer_end_block(&cg->cw);
    }
}

static void
jik_codegen_emit_statement(JikCodeGenerator *cg, JikNode *nd);

static void
jik_codegen_emit_catch(JikCodeGenerator *cg, JikNode *nd)
{
    JikNode *stmnt = nd->val_catch.stmnt;
    jik_writer_begin_block(&cg->cw, "{");

    jik_writer_write_line(&cg->cw, "JikError jik_catch_err = {0};");
    jik_codegen_emit_statement(cg, stmnt);
    jik_writer_begin_block(&cg->cw, "if (jik_error_failed(&jik_catch_err)) {");
    jik_codegen_emit_block(cg, nd->val_catch.body_err);
    jik_writer_write_line(&cg->cw, "jik_error_clear(&jik_catch_err);");
    jik_writer_end_block(&cg->cw);
    jik_writer_begin_block(&cg->cw, "else {");
    jik_codegen_emit_block(cg, nd->val_catch.body_pass);
    jik_writer_end_block(&cg->cw);

    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_stmnt_call(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(get_function_call(cg, nd), ";"));
}

static void
jik_codegen_emit_stmnt_must(JikCodeGenerator *cg, JikNode *nd)
{
    char *em = jik_codegen_emit_expr_must(cg, nd);
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(em, ";"));
}

static void
jik_codegen_emit_statement(JikCodeGenerator *cg, JikNode *nd)
{
    if (nd->type == NODE_STMNT_RETURN) {
        jik_codegen_emit_stmnt_return(cg, nd);
    }
    else if (nd->type == NODE_STMNT_ASSIGN) {
        jik_codegen_emit_stmnt_assign(cg, nd);
    }
    else if (nd->type == NODE_STMNT_DECLARE) {
        jik_codegen_emit_stmnt_declare(cg, nd);
    }
    else if (nd->type == NODE_COND_IF) {
        jik_codegen_emit_cond_if(cg, nd);
    }
    else if (nd->type == NODE_COND_IFELSE) {
        jik_codegen_emit_cond_ifelse(cg, nd);
    }
    else if (nd->type == NODE_COND_IFELIF) {
        jik_codegen_emit_cond_ifelif(cg, nd);
    }
    else if (nd->type == NODE_CATCH) {
        jik_codegen_emit_catch(cg, nd);
    }
    else if (nd->type == NODE_EXPR_CALL) {
        jik_codegen_emit_stmnt_call(cg, nd);
    }
    else if (nd->type == NODE_EXPR_MUST) {
        jik_codegen_emit_stmnt_must(cg, nd);
    }
    else if (nd->type == NODE_STMNT_MEMBER_SET) {
        jik_codegen_emit_stmnt_member_set(cg, nd);
    }
    else if (nd->type == NODE_STMNT_SUBSCRIPT_SET) {
        jik_codegen_emit_stmnt_subscript_set(cg, nd);
    }
    else if (nd->type == NODE_LOOP_WHILE) {
        jik_codegen_emit_stmnt_loop_while(cg, nd);
    }
    else if (nd->type == NODE_LOOP_FOR) {
        jik_codegen_emit_stmnt_loop_for(cg, nd);
    }
    else if (nd->type == NODE_LOOP_FOR_IN) {
        jik_codegen_emit_stmnt_loop_for_in(cg, nd);
    }
    else if (nd->type == NODE_LOOP_FOR_IN_DICT) {
        jik_codegen_emit_stmnt_loop_for_in_dict(cg, nd);
    }
    else if (nd->type == NODE_STMNT_BREAK) {
        jik_codegen_emit_stmnt_break(cg, nd);
    }
    else if (nd->type == NODE_STMNT_CONTINUE) {
        jik_codegen_emit_stmnt_continue(cg, nd);
    }
    else if (nd->type == NODE_STMNT_MATCH) {
        jik_codegen_emit_stmnt_match(cg, nd);
    }
    else {
        jik_node_print(nd, 0);
        jik_diag_fatal_error("stmnt: no translator", "");
    }
}

static void
jik_codegen_emit_block(JikCodeGenerator *cg, JikNode *nd)
{
    for (size_t i = 0; i < VecJikNode_size(nd->val_block); i++) {
        jik_codegen_emit_statement(cg, VecJikNode_get(nd->val_block, i));
    }
}

char *
get_param_name_seq(JikNode *nd, bool throws)
{
    CharBuffer *r = char_buffer_new("");
    size_t      n = VecJikNode_size(nd->val_function.params);
    for (size_t i = 0; i < n; i++) {
        char *cn = VecJikType_get(nd->jik_type->val_func.param_types, i)->C_name;
        assert(cn);
        char *a =
            JIK_STRING_NCAT(cn, " ", VecJikNode_get(nd->val_function.params, i)->val_id.name, NULL);
        char_buffer_append(r, a);
        if (i < n - 1)
            char_buffer_append(r, ", ");
    }
    if (throws) {
        char *prefix = n > 0 ? ", " : "";
        char_buffer_append(r, JIK_STRING_NCAT(prefix, "JikError *jik_err_arg"));
    }
    return r->data;
}

static void
jik_codegen_emit_function_signature(JikCodeGenerator *cg, JikNode *nd, bool throws)
{
    assert(nd->jik_type->val_func.ret_type->C_name);
    jik_writer_write_line(&cg->cw, nd->jik_type->val_func.ret_type->C_name);
    char *param_seq    = get_param_name_seq(nd, throws);
    char *mangled_name = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_function.name);
    jik_writer_write(&cg->cw, JIK_STRING_NCAT(mangled_name, "(", param_seq, ")", NULL));
}

static void
jik_codegen_emit_function_signature_must(JikCodeGenerator *cg, JikNode *nd)
{
    assert(nd->jik_type->val_func.ret_type->C_name);
    jik_writer_write_line(&cg->cw, nd->jik_type->val_func.ret_type->C_name);
    char  *param_seq    = get_param_name_seq(nd, false);
    char  *mangled_name = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_function.name);
    size_t n            = VecJikNode_size(nd->val_function.params);
    char  *prefix       = n > 0 ? ", " : "";
    jik_writer_write(
        &cg->cw, JIK_STRING_NCAT(mangled_name, "_must(", param_seq, prefix, "char *msg)", NULL));
}

static void
jik_codegen_emit_function_signature_must_extern(JikCodeGenerator *cg, JikNode *nd)
{
    assert(nd->jik_type->val_func.ret_type->C_name);
    jik_writer_write_line(&cg->cw, nd->jik_type->val_func.ret_type->C_name);

    CharBuffer *r = char_buffer_new("");
    size_t      n = VecJikNode_size(nd->val_extern_function.params);
    for (size_t i = 0; i < n; i++) {
        JikType *arg_type = VecJikType_get(nd->jik_type->val_func.param_types, i);
        char    *cn       = arg_type->C_name;
        assert(cn);
        char *param_name = JIK_STRING_NCAT("p", size_t_to_string(i + 1));
        char *a          = JIK_STRING_NCAT(cn, " ", param_name, NULL);
        char_buffer_append(r, a);
        if (i < n - 1)
            char_buffer_append(r, ", ");
    }
    char *prefix = n > 0 ? ", " : "";
    jik_writer_write(
        &cg->cw,
        JIK_STRING_NCAT(
            nd->val_extern_function.C_func_name, "_must(", r->data, prefix, "char *msg)", NULL));
}

static void
jik_codegen_emit_local_declarations(JikCodeGenerator *cg, JikNode *nd)
{
    if (!jik_type_equal(nd->jik_type->val_func.ret_type, &JIK_TYPE_VOID)) {
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT(nd->jik_type->val_func.ret_type->C_name, " __jik_res = {0};"));
    }
}

static void
jik_codegen_emit_function(JikCodeGenerator *cg, JikNode *nd, bool throws)
{
    jik_writer_blank_line(&cg->cw);
    jik_codegen_emit_function_signature(cg, nd, throws);
    jik_writer_blank_line(&cg->cw);
    jik_writer_begin_block(&cg->cw, "{");
    jik_codegen_emit_prologue(cg, nd);
    jik_codegen_emit_local_declarations(cg, nd);
    jik_codegen_emit_block(cg, nd->val_function.body);
    jik_codegen_emit_cleanup(cg, nd);
    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_function_must(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_blank_line(&cg->cw);
    jik_codegen_emit_function_signature_must(cg, nd);
    jik_writer_blank_line(&cg->cw);
    jik_writer_begin_block(&cg->cw, "{");

    CharBuffer *r = char_buffer_new("");
    size_t      n = VecJikNode_size(nd->val_function.params);
    for (size_t i = 0; i < n; i++) {
        char_buffer_append(r, VecJikNode_get(nd->val_function.params, i)->val_id.name);
        if (i < n - 1)
            char_buffer_append(r, ", ");
    }

    bool  returns_void = nd->jik_type->val_func.ret_type == &JIK_TYPE_VOID;
    char *prefix_type =
        returns_void ? "" : JIK_STRING_NCAT(nd->jik_type->val_func.ret_type->C_name, " res = ");
    char *prefix_comma = n > 0 ? ", " : "";

    char *mangled_name = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_function.name);
    jik_writer_write_line(&cg->cw, "JikError jik_local_err = {0};");
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(prefix_type, mangled_name, "(", r->data, prefix_comma, "&jik_local_err);"));

    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_panic_on_error(&jik_local_err, msg);"));
    if (!returns_void) {
        jik_writer_write_line(&cg->cw, "return res;");
    }

    jik_writer_end_block(&cg->cw);
}

static void
jik_codegen_emit_function_must_extern(JikCodeGenerator *cg, JikNode *nd)
{
    jik_writer_blank_line(&cg->cw);
    jik_codegen_emit_function_signature_must_extern(cg, nd);
    jik_writer_blank_line(&cg->cw);
    jik_writer_begin_block(&cg->cw, "{");

    CharBuffer *r = char_buffer_new("");
    size_t      n = VecJikNode_size(nd->val_extern_function.params);
    for (size_t i = 0; i < n; i++) {
        char_buffer_append(r, JIK_STRING_NCAT("p", size_t_to_string(i + 1)));
        if (i < n - 1)
            char_buffer_append(r, ", ");
    }

    bool  returns_void = nd->jik_type->val_func.ret_type == &JIK_TYPE_VOID;
    char *prefix_type =
        returns_void ? "" : JIK_STRING_NCAT(nd->jik_type->val_func.ret_type->C_name, " res = ");
    char *prefix_comma = n > 0 ? ", " : "";

    jik_writer_write_line(&cg->cw, "JikError jik_local_err = {0};");
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT(prefix_type,
                                          nd->val_extern_function.C_func_name,
                                          "(",
                                          r->data,
                                          prefix_comma,
                                          "&jik_local_err);"));

    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_panic_on_error(&jik_local_err, msg);"));
    if (!returns_void) {
        jik_writer_write_line(&cg->cw, "return res;");
    }

    jik_writer_end_block(&cg->cw);
}

// -----------------------------------------------------------------------------
// Generated print helpers
// -----------------------------------------------------------------------------

static void
jik_codegen_emit_struct_print_function(JikCodeGenerator *cg, JikNode *nd, char *mangled_name)
{
    char **entry = TabString_get(cg->print_functions, mangled_name);
    assert(entry);
    char *func_name = *entry;
    jik_writer_write_line(&cg->cw, "JikString *");
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT(func_name, "(struct ", mangled_name, " *s, JikRegion *a)"));
    jik_writer_write_line(&cg->cw, "{");
    jik_writer_indent(&cg->cw);
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("if (!s) { return ", JIK_NULL_PRINT_EXPR, "; }"));
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("JikCharBuffer *cb = jik_char_buffer_new(",
                                          "\"<\"",
                                          "\"",
                                          nd->val_struct.name,
                                          " \", a);"));

    for (size_t i = 0; i < VecString_size(nd->val_struct.field_order); i++) {
        char     *field_name     = VecString_get(nd->val_struct.field_order, i);
        JikType **field_type_ptr = TabJikType_get(nd->jik_type->val_struct.field_types, field_name);
        assert(field_type_ptr);
        JikType *field_type     = *field_type_ptr;
        char    *elem_type_name = jik_codegen_get_print_type_name(field_type);
        assert(elem_type_name);
        entry = TabString_get(cg->print_functions, elem_type_name);
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, \"", field_name, "=\", a);"));
        if (entry) {
            func_name = *entry;
            jik_writer_write_line(&cg->cw,
                                  JIK_STRING_NCAT("jik_char_buffer_append(cb, ",
                                                  func_name,
                                                  "(s->",
                                                  field_name,
                                                  ", a)->data, a);"));
        }
        else {
            char *field_pretty_name = sanitize_string(jik_type_pretty_name(field_type));
            jik_writer_write_line(
                &cg->cw,
                JIK_STRING_NCAT("jik_char_buffer_append(cb, \"<", field_pretty_name, ">\", a);"));
        }
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("jik_char_buffer_append(cb, \"", " ", "\", a);"));
    }
    jik_writer_write_line(&cg->cw, "jik_char_buffer_append(cb, \">\", a);");
    jik_writer_write_line(&cg->cw, "return jik_string_new(cb->data, a);");
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_enum_print_function(JikCodeGenerator *cg, JikNode *nd, char *mangled_name)
{
    char **entry = TabString_get(cg->print_functions, mangled_name);
    assert(entry);
    char *func_name = *entry;
    jik_writer_write_line(&cg->cw, "JikString *");
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT(func_name, "(enum ", mangled_name, " s, JikRegion *a)"));
    jik_writer_write_line(&cg->cw, "{");
    jik_writer_indent(&cg->cw);
    char **entry2 = TabString_get(cg->print_functions, C_TYPE_NAME_STRING);
    assert(entry2);
    char *ret = JIK_STRING_NCAT(
        "return ", *entry2, "(jik_string_new(", nd->jik_type->mangled_name, "_1NAMES[s], a), a);");
    jik_writer_write_line(&cg->cw, ret);
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_variant_print_function(JikCodeGenerator *cg, JikNode *nd, char *mangled_name)
{
    // TODO: find a nicer way to emit custom function writing
    assert(nd->type == NODE_VARIANT);
    char **entry = TabString_get(cg->print_functions, mangled_name);
    assert(entry);
    char *func_name = *entry;
    jik_writer_write_line(&cg->cw, "JikString *");
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT(func_name, "(struct ", mangled_name, " *s, JikRegion *a)"));
    jik_writer_write_line(&cg->cw, "{");
    jik_writer_indent(&cg->cw);
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("if (!s) { return ", JIK_NULL_PRINT_EXPR, "; }"));
    jik_writer_write_line(&cg->cw, "switch (s->tag) {");
    for (size_t i = 0; i < VecString_size(nd->val_variant.member_order); i++) {
        char     *member_name = VecString_get(nd->val_variant.member_order, i);
        JikNode **member_node = TabJikNode_get(nd->val_variant.init_vals, member_name);
        assert(member_node);
        char *tag_name =
            JIK_STRING_NCAT(nd->val_variant.enum_nd->jik_type->mangled_name, "_", member_name);
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("case ", tag_name, ":"));
        jik_writer_indent(&cg->cw);

        char *elem_type_name = jik_codegen_get_print_type_name((*member_node)->jik_type);
        assert(elem_type_name);
        entry = TabString_get(cg->print_functions, elem_type_name);
        if (entry) {
            func_name = *entry;
            jik_writer_write_line(
                &cg->cw,
                JIK_STRING_NCAT("return ", func_name, "(", "s->val.", member_name, ", a);"));
        }
        else {
            char *member_pretty_name =
                sanitize_string(jik_type_pretty_name((*member_node)->jik_type));
            jik_writer_write_line(
                &cg->cw,
                JIK_STRING_NCAT("return jik_string_new(\"<", member_pretty_name, ">\", a);"));
        }

        // jik_writer_write_line(&cg->cw, "break;");
        jik_writer_dedent(&cg->cw);
    }
    jik_writer_write_line(&cg->cw, "default: break;");
    jik_writer_write_line(&cg->cw, "}");
    jik_writer_write_line(&cg->cw, "return jik_string_new(\"<invalid variant tag>\", a);");
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_vec_print_function(JikCodeGenerator *cg, JikNode *nd, char *mangled_name)
{
    // TODO: find a nicer way to emit custom function writing
    assert(nd->type == NODE_EXPR_VECTOR);
    char **entry = TabString_get(cg->print_functions, mangled_name);
    assert(entry);
    char *func_name = *entry;
    jik_writer_write_line(&cg->cw, "JikString *");
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT(func_name, "(struct ", mangled_name, " *s, JikRegion *a)"));
    jik_writer_write_line(&cg->cw, "{");
    jik_writer_indent(&cg->cw);
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("if (!s) { return ", JIK_NULL_PRINT_EXPR, "; }"));
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT("JikCharBuffer *cb = jik_char_buffer_new(", "\"[\", a);"));
    jik_writer_write_line(&cg->cw, "for (size_t i = 0; i < s->size; i++) {");
    jik_writer_indent(&cg->cw);
    JikType *elem_type      = nd->jik_type->val_vec.elem_type;
    char    *elem_type_name = jik_codegen_get_print_type_name(elem_type);
    assert(elem_type_name);
    entry = TabString_get(cg->print_functions, elem_type_name);
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, ", "\" \", a);"));
    if (entry) {
        func_name = *entry;
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("jik_char_buffer_append(cb, ",
                                              func_name,
                                              "(s->data[i], a)->data, a);"));
    }
    else {
        char *elem_pretty_name = sanitize_string(jik_type_pretty_name(elem_type));
        jik_writer_write_line(
            &cg->cw,
            JIK_STRING_NCAT("jik_char_buffer_append(cb, \"<", elem_pretty_name, ">\", a);"));
    }

    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, ", "\" ]\", a);"));
    jik_writer_write_line(&cg->cw, "return jik_string_new(cb->data, a);");
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_dict_print_function(JikCodeGenerator *cg, JikNode *nd, char *mangled_name)
{
    // TODO: find a nicer way to emit custom function writing
    assert(nd->type == NODE_EXPR_DICT);
    char **entry = TabString_get(cg->print_functions, mangled_name);
    assert(entry);
    char *func_name = *entry;
    jik_writer_write_line(&cg->cw, "JikString *");
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT(func_name, "(struct ", mangled_name, " *s, JikRegion *a)"));
    jik_writer_write_line(&cg->cw, "{");
    jik_writer_indent(&cg->cw);
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("if (!s) { return ", JIK_NULL_PRINT_EXPR, "; }"));
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT("JikCharBuffer *cb = jik_char_buffer_new(", "\"{\", a);"));
    jik_writer_write_line(&cg->cw, "for (size_t i = 0; i < s->capacity; i++) {");
    jik_writer_indent(&cg->cw);
    JikType *elem_type = nd->jik_type->val_dict.elem_type;

    // char *elem_type_name = jik_type_is_primitive(elem_type) ? elem_type->C_name :
    // get_type_name(elem_type);
    char *elem_type_name = jik_codegen_get_print_type_name(elem_type);
    assert(elem_type_name);
    entry = TabString_get(cg->print_functions, elem_type_name);
    jik_writer_write_line(&cg->cw, "if (s->items[i].key) {");
    // jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, ", "if
    // (s->items[i].key.data) {"));
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, ", "\" \", a);"));
    char **entry2 = TabString_get(cg->print_functions, C_TYPE_NAME_STRING);
    assert(entry2);
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT("jik_char_buffer_append(cb, ", *entry2, "(s->items[i].key, a)->data, a);"));
    jik_writer_write_line(&cg->cw, "jik_char_buffer_append(cb, \": \", a);");
    if (entry) {
        func_name = *entry;
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("jik_char_buffer_append(cb, ",
                                              func_name,
                                              "(s->items[i].val, a)->data, a);"));
    }
    else {
        char *elem_pretty_name = sanitize_string(jik_type_pretty_name(elem_type));
        jik_writer_write_line(
            &cg->cw,
            JIK_STRING_NCAT("jik_char_buffer_append(cb, \"<", elem_pretty_name, ">\", a);"));
    }
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, \",\", a);"));
    jik_writer_write_line(&cg->cw, "}");
    // jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, ", "}"));

    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("jik_char_buffer_append(cb, ", "\" }\", a);"));
    jik_writer_write_line(&cg->cw, "return jik_string_new(cb->data, a);");
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_option_print_function(JikCodeGenerator *cg, JikType *opt_type)
{
    char **entry = TabString_get(cg->print_functions, opt_type->mangled_name);
    assert(entry);
    char *func_name = *entry;
    jik_writer_write_line(&cg->cw, "JikString *");
    jik_writer_write_line(
        &cg->cw,
        JIK_STRING_NCAT(func_name, "(struct ", opt_type->mangled_name, " *s, JikRegion *a)"));
    jik_writer_write_line(&cg->cw, "{");
    jik_writer_indent(&cg->cw);
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("if (!s) { return ", JIK_NULL_PRINT_EXPR, "; }"));
    char *elem_type_name = jik_codegen_get_print_type_name(opt_type->val_option.elem_type);
    assert(elem_type_name);
    entry                  = TabString_get(cg->print_functions, elem_type_name);
    char *elem_pretty_name = sanitize_string(jik_type_pretty_name(opt_type->val_option.elem_type));
    jik_writer_write_line(&cg->cw, "if (s->is_some) {");
    jik_writer_indent(&cg->cw);
    if (entry) {
        char *elem_print_func = *entry;
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT("JikString *val_str = ", elem_print_func, "(s->val, a);"));
    }
    else {
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("JikString *val_str = jik_string_new(\"<",
                                              elem_pretty_name,
                                              ">\", a);"));
    }
    jik_writer_write_line(&cg->cw, "JikCharBuffer *cb = jik_char_buffer_new(\"<Some: \", a);");
    jik_writer_write_line(&cg->cw, "jik_char_buffer_append(cb, val_str->data, a);");
    jik_writer_write_line(&cg->cw, "jik_char_buffer_append(cb, \">\", a);");
    jik_writer_write_line(&cg->cw, "return jik_string_new(cb->data, a);");
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
    jik_writer_write_line(
        &cg->cw, JIK_STRING_NCAT("return jik_string_new(\"<None[", elem_pretty_name, "]>\", a);"));
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "}");
}

static void
jik_codegen_emit_enum(JikCodeGenerator *cg, JikNode *nd)
{
    char *mn = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_enum.name);
    jik_writer_write_line(&cg->cw, "\n");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_ENUM_PREFIX, mn, " {"));
    jik_writer_indent(&cg->cw);

    for (size_t i = 0; i < VecString_size(nd->val_enum.enumerator_order); i++) {
        char *enumerator = VecString_get(nd->val_enum.enumerator_order, i);
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(mn, "_", enumerator, ","));
    }

    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "};");

    jik_writer_write_line(&cg->cw, "\n");
    // Looks weird, but prevents collisions with variant constants.
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("char *", mn, "_1NAMES[] = {"));
    jik_writer_indent(&cg->cw);
    for (size_t i = 0; i < VecString_size(nd->val_enum.enumerator_order); i++) {
        char *enumerator = VecString_get(nd->val_enum.enumerator_order, i);
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("\"", nd->val_enum.name, ".", enumerator, "\","));
    }
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "};\n");
    jik_codegen_register_print_function(cg, nd->jik_type->mangled_name, NULL);
    jik_codegen_emit_enum_print_function(cg, nd, nd->jik_type->mangled_name);
}

static void
jik_codegen_emit_struct(JikCodeGenerator *cg, JikNode *nd)
{
    char *mn = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_struct.name);
    jik_writer_write_line(&cg->cw, "\n");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, mn, " {"));
    jik_writer_indent(&cg->cw);
    for (size_t i = 0; i < VecString_size(nd->val_struct.field_order); i++) {
        char     *field_name = VecString_get(nd->val_struct.field_order, i);
        JikNode **field_node = TabJikNode_get(nd->val_struct.init_vals, field_name);
        assert(field_node);
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT((*field_node)->jik_type->C_name, " ", field_name, ";"));
    }
    jik_writer_write_line(&cg->cw, "JikRegion *region;");

    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "};");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("JIK_DEFINE_STRUCT_NEW(", mn, ");"));
    jik_codegen_emit_struct_print_function(cg, nd, mn);
}

static void
jik_codegen_emit_variant(JikCodeGenerator *cg, JikNode *nd)
{
    char *mn = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_variant.name);
    jik_writer_write_line(&cg->cw, "\n");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, mn, " {"));
    jik_writer_indent(&cg->cw);
    char *men =
        jik_codegen_mangle_name(nd->token->mod_alias, nd->val_variant.enum_nd->val_enum.name);
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_ENUM_PREFIX, men, " tag;"));
    jik_writer_write_line(&cg->cw, "union {");
    jik_writer_indent(&cg->cw);
    for (size_t i = 0; i < VecString_size(nd->val_variant.member_order); i++) {
        char     *member_name = VecString_get(nd->val_variant.member_order, i);
        JikNode **member_node = TabJikNode_get(nd->val_variant.init_vals, member_name);
        assert(member_node);
        jik_writer_write_line(
            &cg->cw, JIK_STRING_NCAT((*member_node)->jik_type->C_name, " ", member_name, ";"));
    }
    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "} val;");
    jik_writer_write_line(&cg->cw, "JikRegion *region;");

    jik_writer_dedent(&cg->cw);
    jik_writer_write_line(&cg->cw, "};");
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("JIK_DEFINE_STRUCT_NEW(", mn, ");"));
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("JIK_DEFINE_VARIANT_ACCESS(", mn, ", ", men, ");"));

    jik_codegen_emit_variant_print_function(cg, nd, mn);
}

void
jik_codegen_emit_functions(JikCodeGenerator *cg)
{
    JikNode *nd;

    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.extern_functions); i++) {
        nd = VecJikNode_get(cg->ast->val_program.extern_functions, i);
        if (jik_function_throws(nd)) {
            jik_codegen_emit_function_must_extern(cg, nd);
        }
    }

    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.functions); i++) {
        nd = VecJikNode_get(cg->ast->val_program.functions, i);
        jik_codegen_emit_function(cg, nd, jik_function_throws(nd));
        if (jik_function_throws(nd)) {
            jik_codegen_emit_function_must(cg, nd);
        }
    }
}

void
jik_codegen_emit_structs(JikCodeGenerator *cg)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.structs); i++) {
        nd = VecJikNode_get(cg->ast->val_program.structs, i);
        jik_codegen_emit_struct(cg, nd);
    }
}

void
jik_codegen_emit_variants(JikCodeGenerator *cg)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.variants); i++) {
        nd = VecJikNode_get(cg->ast->val_program.variants, i);
        jik_codegen_emit_variant(cg, nd);
    }
}

void
jik_codegen_emit_enums(JikCodeGenerator *cg)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.enums); i++) {
        nd = VecJikNode_get(cg->ast->val_program.enums, i);
        jik_codegen_emit_enum(cg, nd);
    }
    // We also emit variant tag enums here
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.variants); i++) {
        nd = VecJikNode_get(cg->ast->val_program.variants, i);
        jik_codegen_emit_enum(cg, nd->val_variant.enum_nd);
    }
}

// -----------------------------------------------------------------------------
// Generated container declarations and definitions
// -----------------------------------------------------------------------------

static void
jik_codegen_emit_container_struct_declaration(JikCodeGenerator *cg,
                                              TabBool          *declared_types,
                                              char             *mangled_name)
{
    bool *declared = TabBool_get(declared_types, mangled_name);
    if (declared) {
        return;
    }
    jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, mangled_name, ";"));
    TabBool_set(declared_types, mangled_name, true);
}

static JikType *
jik_codegen_make_dict_option_type(JikNode *dict_nd)
{
    assert(dict_nd->type == NODE_EXPR_DICT);
    JikType *opt_type = jik_type_new_option(dict_nd->val_dict.elem_expr->jik_type);
    jik_codegen_prepare_type_name(opt_type);
    return opt_type;
}

// TODO: also separate this into functions for other types
static void
jik_codegen_emit_vec_declaration(JikCodeGenerator *cg, JikNode *nd)
{
    bool *declared = TabBool_get(cg->declared_vec_types, nd->jik_type->mangled_name);
    if (declared) {
        return;
    }
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("JIK_DECLARE_VEC(",
                                          nd->jik_type->mangled_name,
                                          ", ",
                                          nd->val_vector.elem_expr->jik_type->C_name,
                                          ");"));
    TabBool_set(cg->declared_vec_types, nd->jik_type->mangled_name, true);
    jik_codegen_register_print_function(cg, nd->jik_type->mangled_name, NULL);
}

static void
jik_codegen_emit_option_declaration(JikCodeGenerator *cg, JikType *opt_type)
{
    bool *declared = TabBool_get(cg->declared_option_types, opt_type->mangled_name);
    if (declared) {
        return;
    }
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("JIK_DECLARE_OPTION(",
                                          opt_type->mangled_name,
                                          ", ",
                                          opt_type->val_option.elem_type->C_name,
                                          ");"));
    TabBool_set(cg->declared_option_types, opt_type->mangled_name, true);
    jik_codegen_register_print_function(cg, opt_type->mangled_name, NULL);
}

static void
jik_codegen_emit_global_declarations(JikCodeGenerator *cg)
{
    // Global variables
    size_t n = VecJikNode_size(cg->ast->val_program.globals);
    if (n > 0) {
        jik_writer_write_line(&cg->cw, "");
    }
    JikNode *nd;
    for (size_t i = 0; i < n; i++) {
        nd                                     = VecJikNode_get(cg->ast->val_program.globals, i);
        nd->val_assign.id->val_id.mangled_name = jik_codegen_mangle_name(
            nd->val_assign.id->val_id.mod_alias, nd->val_assign.id->val_id.name);
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT(nd->val_assign.expr->jik_type->C_name,
                                              " ",
                                              nd->val_assign.id->val_id.mangled_name,
                                              ";"));
    }
    // Structs
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.structs); i++) {
        nd       = VecJikNode_get(cg->ast->val_program.structs, i);
        char *mn = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_struct.name);
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, mn, ";"));
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("JIK_DECLARE_STRUCT_NEW(", mn, ");"));
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("JIK_DECLARE_STRUCT_TOSTR(", mn, ");"));
        jik_codegen_register_print_function(cg, mn, NULL);
    }
    // Variants
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.variants); i++) {
        nd       = VecJikNode_get(cg->ast->val_program.variants, i);
        char *mn = jik_codegen_mangle_name(nd->token->mod_alias, nd->val_variant.name);
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, mn, ";"));
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("JIK_DECLARE_STRUCT_NEW(", mn, ");"));
        jik_writer_write_line(&cg->cw, JIK_STRING_NCAT("JIK_DECLARE_STRUCT_TOSTR(", mn, ");"));
        jik_codegen_register_print_function(cg, mn, NULL);
    }
    // Vector struct names
    VecJikNode_iter it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_VECTOR) {
            assert(nd->jik_type->mangled_name);
            jik_codegen_emit_container_struct_declaration(
                cg, cg->declared_vec_struct_types, nd->jik_type->mangled_name);
        }
    }
    // Dict struct names
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_DICT) {
            jik_codegen_emit_container_struct_declaration(
                cg, cg->declared_dict_struct_types, nd->jik_type->mangled_name);
        }
    }
    // Option struct names
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->jik_type->name == TYPE_OPTION) {
            jik_codegen_emit_container_struct_declaration(
                cg, cg->declared_option_struct_types, nd->jik_type->mangled_name);
        }
    }
    // Options
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->jik_type->name == TYPE_OPTION) {
            jik_codegen_emit_option_declaration(cg, nd->jik_type);
        }
    }
    // Vectors
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_VECTOR) {
            jik_codegen_emit_vec_declaration(cg, nd);
        }
    }
    if (cg->arg_vec) {
        jik_codegen_emit_vec_declaration(cg, cg->arg_vec);
    }
    // Dicts
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_DICT) {
            bool *declared = TabBool_get(cg->declared_dict_types, nd->jik_type->mangled_name);
            if (declared) {
                continue;
            }
            JikType *opt_type = jik_codegen_make_dict_option_type(nd);
            jik_codegen_emit_option_declaration(cg, opt_type);
            jik_writer_write_line(&cg->cw,
                                  JIK_STRING_NCAT("JIK_DECLARE_DICT(",
                                                  nd->jik_type->mangled_name,
                                                  ", ",
                                                  nd->val_dict.elem_expr->jik_type->C_name,
                                                  ", ",
                                                  opt_type->mangled_name,
                                                  ");"));
            TabBool_set(cg->declared_dict_types, nd->jik_type->mangled_name, true);
            jik_codegen_register_print_function(cg, nd->jik_type->mangled_name, NULL);
        }
    }
    // Functions
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.functions); i++) {
        nd = VecJikNode_get(cg->ast->val_program.functions, i);
        jik_codegen_emit_function_signature(cg, nd, jik_function_throws(nd));
        jik_writer_write_line(&cg->cw, ";");
        if (jik_function_throws(nd)) {
            jik_codegen_emit_function_signature_must(cg, nd);
            jik_writer_write_line(&cg->cw, ";");
        }
    }

    // Autogenerate "must" wrapper for externs
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.extern_functions); i++) {
        nd = VecJikNode_get(cg->ast->val_program.extern_functions, i);
        if (jik_function_throws(nd)) {
            jik_codegen_emit_function_signature_must_extern(cg, nd);
            jik_writer_write_line(&cg->cw, ";");
        }
    }
}

static void
jik_codegen_emit_vec_definition(JikCodeGenerator *cg, JikNode *nd)
{
    bool *defined = TabBool_get(cg->defined_vec_types, nd->jik_type->mangled_name);
    if (defined) {
        return;
    }
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("JIK_DEFINE_VEC(",
                                          nd->jik_type->mangled_name,
                                          ", ",
                                          nd->val_vector.elem_expr->jik_type->C_name,
                                          ");"));
    TabBool_set(cg->defined_vec_types, nd->jik_type->mangled_name, true);
    jik_codegen_emit_vec_print_function(cg, nd, nd->jik_type->mangled_name);
}

static void
jik_codegen_emit_option_definition(JikCodeGenerator *cg, JikType *opt_type)
{
    bool *defined = TabBool_get(cg->defined_option_types, opt_type->mangled_name);
    if (defined) {
        return;
    }
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("JIK_DEFINE_OPTION(",
                                          opt_type->mangled_name,
                                          ", ",
                                          opt_type->val_option.elem_type->C_name,
                                          ");"));
    TabBool_set(cg->defined_option_types, opt_type->mangled_name, true);
    jik_codegen_emit_option_print_function(cg, opt_type);
}

static void
jik_codegen_emit_vec_definitions(JikCodeGenerator *cg)
{
    JikNode        *nd;
    VecJikNode_iter it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_VECTOR) {
            jik_codegen_emit_vec_definition(cg, nd);
        }
    }
    if (cg->arg_vec) {
        jik_codegen_emit_vec_definition(cg, cg->arg_vec);
    }
}

static void
jik_codegen_emit_dict_definitions(JikCodeGenerator *cg)
{
    JikNode        *nd;
    VecJikNode_iter it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->jik_type->name == TYPE_OPTION) {
            jik_codegen_emit_option_definition(cg, nd->jik_type);
        }
    }
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_DICT) {
            bool *defined = TabBool_get(cg->defined_dict_types, nd->jik_type->mangled_name);
            if (defined) {
                continue;
            }
            JikType *opt_type = jik_codegen_make_dict_option_type(nd);
            jik_codegen_emit_option_definition(cg, opt_type);
            jik_writer_write_line(&cg->cw,
                                  JIK_STRING_NCAT("JIK_DEFINE_DICT(",
                                                  nd->jik_type->mangled_name,
                                                  ", ",
                                                  nd->val_dict.elem_expr->jik_type->C_name,
                                                  ", ",
                                                  opt_type->mangled_name,
                                                  ");"));
            TabBool_set(cg->defined_dict_types, nd->jik_type->mangled_name, true);
            jik_codegen_emit_dict_print_function(cg, nd, nd->jik_type->mangled_name);
        }
    }
}

static void
jik_codegen_prepare_named_enum_type(JikType *t, char *mod_alias, char *name)
{
    char *mn        = jik_codegen_mangle_name(mod_alias, name);
    t->mangled_name = mn;
    t->C_name       = JIK_STRING_NCAT(JIK_C_ENUM_PREFIX, mn);
}

static void
jik_codegen_prepare_named_struct_type(JikType *t, char *mod_alias, char *name)
{
    char *mn        = jik_codegen_mangle_name(mod_alias, name);
    t->mangled_name = mn;
    t->C_name       = JIK_STRING_NCAT(JIK_C_STRUCT_PREFIX, mn, JIK_C_PTR_SUFFIX);
}

static void
jik_codegen_prepare_named_extern_struct_type(JikType *t, char *mod_alias, char *name)
{
    char *mn        = jik_codegen_mangle_name(mod_alias, name);
    t->mangled_name = mn;
}

static void
jik_codegen_prepare_C_names(JikCodeGenerator *cg)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.enums); i++) {
        nd = VecJikNode_get(cg->ast->val_program.enums, i);
        jik_codegen_prepare_named_enum_type(nd->jik_type, nd->token->mod_alias, nd->val_enum.name);
    }
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.structs); i++) {
        nd = VecJikNode_get(cg->ast->val_program.structs, i);
        jik_codegen_prepare_named_struct_type(
            nd->jik_type, nd->token->mod_alias, nd->val_struct.name);
    }
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.variants); i++) {
        nd = VecJikNode_get(cg->ast->val_program.variants, i);
        jik_codegen_prepare_named_struct_type(
            nd->jik_type, nd->token->mod_alias, nd->val_variant.name);
        JikNode *enum_nd = nd->val_variant.enum_nd;
        assert(enum_nd);
        jik_codegen_prepare_named_enum_type(
            enum_nd->jik_type, enum_nd->token->mod_alias, enum_nd->val_enum.name);
    }
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.extern_structs); i++) {
        nd = VecJikNode_get(cg->ast->val_program.extern_structs, i);
        jik_codegen_prepare_named_extern_struct_type(
            nd->jik_type, nd->token->mod_alias, nd->val_struct.name);
    }
    VecJikNode_iter it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_EXPR_VARIANT_NEW) {
            JikNode *s =
                jik_scope_get_global_symbol(nd->val_variant_new.variant_node->val_variant.name,
                                            nd->val_variant_new.variant_node->token->mod_alias);
            assert(s);
            nd->jik_type->mangled_name          = s->jik_type->mangled_name;
            nd->jik_type->val_variant.enum_type = s->val_variant.enum_nd->jik_type;
            nd->jik_type->C_name                = s->jik_type->C_name;
        }
    }
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        jik_codegen_prepare_type_name(nd->jik_type);
    }
    if (cg->arg_vec) {
        jik_codegen_prepare_type_name(cg->arg_vec->jik_type);
    }
    it = VecJikNode_iter_new(cg->nodes);
    while (VecJikNode_iter_next(&it, &nd)) {
        if (nd->type == NODE_FUNCTION) {
            jik_codegen_prepare_type_name(nd->jik_type->val_func.ret_type);
            size_t n = VecJikType_size(nd->jik_type->val_func.param_types);
            for (size_t i = 0; i < n; i++) {
                JikType *pt = VecJikType_get(nd->jik_type->val_func.param_types, i);
                jik_codegen_prepare_type_name(pt);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Final translation assembly
// -----------------------------------------------------------------------------

static void
jik_codegen_emit_embedded_code(JikCodeGenerator *cg)
{
    JikNode *nd;
    for (size_t i = 0; i < VecJikNode_size(cg->ast->val_program.embedded_C); i++) {
        nd = VecJikNode_get(cg->ast->val_program.embedded_C, i);
        jik_writer_write(&cg->cw, JIK_STRING_NCAT("// --- embed ", nd->token->filepath));
        jik_writer_write(&cg->cw, nd->val_code);
        jik_writer_write_line(&cg->cw, "// --- end ");
    }
}

static void
jik_codegen_register_print_functions(JikCodeGenerator *cg)
{
    jik_codegen_register_print_function(cg, (&JIK_TYPE_INT)->C_name, "jik_int_tostr");
    jik_codegen_register_print_function(cg, (&JIK_TYPE_BOOL)->C_name, "jik_bool_tostr");
    jik_codegen_register_print_function(cg, (&JIK_TYPE_STRING)->C_name, "jik_string_tostr");
    jik_codegen_register_print_function(cg, (&JIK_TYPE_FLOAT)->C_name, "jik_double_tostr");
    jik_codegen_register_print_function(cg, (&JIK_TYPE_CHAR)->C_name, "jik_char_tostr");
}

static void
jik_codegen_emit_jik_version(JikCodeGenerator *cg)
{
    jik_writer_write_line(&cg->cw,
                          JIK_STRING_NCAT("// Generated with Jik version: ", JIK_VERSION_STRING));
}

static void
jik_codegen_emit_support_library(JikCodeGenerator *cg)
{
    if (cg->ctx->conf.unsafe_no_bounds_checks) {
        jik_writer_write_line(&cg->cw, "#define JIK_UNSAFE_NO_BOUNDS_CHECKS");
    }
    if (cg->ctx->conf.embed_core) {
        char *core_content = jik_read_file(cg->ctx->conf.jik_core_h_path);
        jik_writer_write(&cg->cw, core_content);
        jik_writer_write_section(&cg->cw, "JIK TRANSLATION");
    }
    else {
        jik_writer_write_line(&cg->cw,
                              JIK_STRING_NCAT("#include \"", cg->ctx->conf.jik_core_h_path, "\""));
    }
}

void
jik_codegen_run(JikCodeGenerator *cg)
{
    jik_codegen_prepare_C_names(cg);
    jik_codegen_register_print_functions(cg);
    jik_codegen_emit_jik_version(cg);
    jik_codegen_emit_support_library(cg);
    jik_codegen_emit_embedded_code(cg);
    jik_codegen_emit_enums(cg);
    jik_codegen_emit_global_declarations(cg);
    jik_codegen_emit_dict_definitions(cg);
    jik_codegen_emit_vec_definitions(cg);
    jik_writer_set_buffer_functions(&cg->cw);
    jik_codegen_emit_variants(cg);
    jik_codegen_emit_structs(cg);
    jik_codegen_emit_functions(cg);
    jik_codegen_emit_main_function(cg);

    // Finalization
    char_buffer_append(cg->cw.finalized_buf, cg->cw.includes->data);
    char_buffer_append(cg->cw.finalized_buf, cg->cw.functions->data);
    cg->ctx->translation = cg->cw.finalized_buf->data;
}
