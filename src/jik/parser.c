#include "parser.h"

#include "ast.h"

static JikScope *
jik_parser_current_context(JikParser *p)
{
    // todo: list should have peek API
    size_t n = VecJikScope_size(p->contexts);
    assert(n > 0);
    size_t idx = n - 1;
    return VecJikScope_get(p->contexts, idx);
}

void
jik_parser_init(JikParser *p, JikContext *ctx)
{
    p->ctx        = ctx;
    p->pos        = 0;
    p->tokens     = ctx->tokens;
    p->num_tokens = VecJikToken_size(p->tokens);
    p->ast        = jik_node_new_program();
    p->contexts   = VecJikScope_new_empty();
    p->nodes      = VecJikNode_new_empty();
    VecJikScope_push(p->contexts, jik_scope_new(NULL));
    p->parsing_struct  = false;
    p->container_depth = 0;
}

static JikToken *
jik_parser_current_token(JikParser *p)
{
    if (p->pos < p->num_tokens)
        return VecJikToken_get_ptr(p->tokens, p->pos);
    return NULL;
}

static bool
jik_parser_match_token_sequence(JikParser *p, JikTokenType seq[])
{
    JikToken *t;
    for (size_t i = 0; seq[i] != TOK_ERROR; i++) {
        jik_diag_fatal_error_if(p->pos + i >= p->num_tokens, "parser EOF before sequence end", "");
        t = VecJikToken_get_ptr(p->tokens, p->pos + i);
        if (t->type != seq[i]) {
            return false;
        }
    }
    return true;
}

static JikToken *
jik_parser_eat_token(JikParser *p, JikTokenType type)
{
    if (p->pos < p->num_tokens) {
        JikToken *tok = VecJikToken_get_ptr(p->tokens, p->pos);
        if (tok->type != type) {
            jik_diag_fatal_error(
                jik_string_cat("parse error: expected ", jik_token_type_pretty_name(type)),
                jik_token_to_text(tok));
        }
        p->pos++;
        return tok;
    }
    jik_diag_fatal_error(
        jik_string_cat("parse error: EOF, expected ", jik_token_type_pretty_name(type)),
        jik_token_to_text(VecJikToken_get_ptr(p->tokens, p->num_tokens - 1)));
}

static JikNode *
jik_parser_parse_function_call(JikParser *p, JikToken *func_name_tok, char *mod_name);
static JikNode *
jik_parser_parse_option_unwrap_call(JikParser *p, JikToken *unwrap_tok);

// A parsed identifier has two token components, the name and the module name token.
typedef struct ParsedIdentifier {
    JikToken *name_tok;
    JikToken *mod_name_tok;
} ParsedIdentifier;

static ParsedIdentifier
jik_parser_get_parsed_identifier(JikParser *p)
{
    JikToken        *id_tok = jik_parser_eat_token(p, TOK_ID);
    ParsedIdentifier id     = {.name_tok = id_tok};
    JikToken        *tok    = jik_parser_current_token(p);
    if (tok->type == TOK_DOUBLE_COLON) {
        jik_parser_eat_token(p, TOK_DOUBLE_COLON);
        JikToken *mod_name_tok = id.name_tok;
        id.name_tok            = jik_parser_eat_token(p, TOK_ID);
        id.mod_name_tok        = mod_name_tok;
    }
    return id;
}

static JikNode *
jik_parser_parse_expr(JikParser *p);
static JikNode *
jik_parser_parse_struct_new(JikParser *p, JikToken *struct_name_tok, JikToken *mod_name_tok);
static JikNode *
jik_parser_parse_variant_new(JikParser *p, JikToken *var_name_tok, JikToken *mod_name_tok);

static void
jik_parser_eat_newlines_if_found(JikParser *p)
{
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_NEWLINE) {
        jik_parser_eat_token(p, TOK_NEWLINE);
    }
}

static void
jik_parser_diag_invalid_alloc_suffix(JikParser *p, JikToken *tok)
{
    jik_diag_fatal_error_if(
        tok->type == TOK_INTEGER || tok->type == TOK_FLOAT || tok->type == TOK_CHAR ||
            tok->type == TOK_STRING || tok->type == TOK_STRING_ML || tok->type == TOK_KWD_TRUE ||
            tok->type == TOK_KWD_FALSE || tok->type == TOK_UNDERSCORE || tok->type == TOK_LANG ||
            tok->type == TOK_LPAREN || tok->type == TOK_LCURL,
        "composite literal must be bound before subscripting",
        JIK_STRING_NCAT("the trailing '[...]' after a composite literal is reserved "
                        "for allocation syntax; bind the value to a name first\n",
                        jik_token_to_text(tok)));
}

static JikAllocSpec
jik_parser_get_region_spec(JikParser *p)
{
    JikAllocSpec as = (JikAllocSpec){.kind = JIK_ALLOC_LOCAL, .src = JIK_ALLOC_SRC_UNKNOWN};
    if (jik_parser_current_token(p)->type != TOK_LANG) {
        as.src = JIK_ALLOC_SRC_LOCAL;
        return as;
    }
    jik_diag_fatal_error_if(p->container_depth > 0,
                            "illegal nested region specifier",
                            jik_token_to_text(jik_parser_current_token(p)));
    jik_parser_eat_token(p, TOK_LANG);
    as.kind = JIK_ALLOC_NAMED_REGION;
    if (jik_parser_current_token(p)->type == TOK_DOT) {
        jik_parser_eat_token(p, TOK_DOT);
        as.kind = JIK_ALLOC_CONTAINER;
    }
    jik_parser_diag_invalid_alloc_suffix(p, jik_parser_current_token(p));
    JikToken *tok  = jik_parser_eat_token(p, TOK_ID);
    as.region_name = tok->lexeme;
    jik_parser_eat_token(p, TOK_RANG);
    return as;
}

static JikNode *
jik_parser_parse_vector_literal(JikParser *p)
{
    p->container_depth++;
    JikToken *tok = jik_parser_eat_token(p, TOK_LANG);
    jik_parser_eat_newlines_if_found(p);
    if (jik_parser_current_token(p)->type == TOK_RANG) {
        jik_diag_fatal_error("empty vector literal not allowed",
                             "declare an empty vector by type, for example: v: Vec[int]");
    }
    JikNode *size_expr = jik_parser_parse_expr(p);
    if (jik_parser_current_token(p)->type == TOK_KWD_OF) {
        jik_parser_eat_token(p, TOK_KWD_OF);
        JikNode *elem_expr = jik_parser_parse_expr(p);
        jik_parser_eat_token(p, TOK_RANG);
        p->container_depth--;
        return jik_node_new_vector(size_expr, elem_expr, NULL, jik_parser_current_context(p), tok);
    }
    VecJikNode *init_elems = VecJikNode_new_empty();
    VecJikNode_push(init_elems, size_expr);
    while (jik_parser_current_token(p)->type != TOK_RANG) {
        jik_parser_eat_token(p, TOK_COMMA);
        jik_parser_eat_newlines_if_found(p);
        VecJikNode_push(init_elems, jik_parser_parse_expr(p));
        jik_parser_eat_newlines_if_found(p);
    }
    jik_parser_eat_token(p, TOK_RANG);
    p->container_depth--;
    return jik_node_new_vector(NULL, NULL, init_elems, jik_parser_current_context(p), tok);
}

static JikNode *
jik_parser_parse_dict_literal(JikParser *p)
{
    p->container_depth++;
    JikToken *tok = jik_parser_eat_token(p, TOK_LCURL);
    jik_parser_eat_newlines_if_found(p);
    if (jik_parser_current_token(p)->type == TOK_RCURL) {
        jik_diag_fatal_error("empty dictionary literal not allowed",
                             "declare an empty dictionary by type, for example: d: Dict[int]");
    }
    VecJikNode *init_keys   = VecJikNode_new_empty();
    VecJikNode *init_values = VecJikNode_new_empty();

    while (jik_parser_current_token(p)->type != TOK_RCURL) {
        VecJikNode_push(init_keys, jik_parser_parse_expr(p));
        jik_parser_eat_token(p, TOK_COLON);
        VecJikNode_push(init_values, jik_parser_parse_expr(p));
        jik_parser_eat_newlines_if_found(p);
        if (jik_parser_current_token(p)->type == TOK_RCURL) {
            jik_parser_eat_token(p, TOK_RCURL);
            p->container_depth--;
            return jik_node_new_dict_literal(
                NULL, init_keys, init_values, jik_parser_current_context(p), tok);
        }
        jik_parser_eat_token(p, TOK_COMMA);
        jik_parser_eat_newlines_if_found(p);
    }

    p->container_depth--;
    return jik_node_new_dict_literal(
        NULL, init_keys, init_values, jik_parser_current_context(p), tok);
}

static JikNode *
jik_parser_parse_atom(JikParser *p)
{
    JikToken *tok = jik_parser_current_token(p);
    if (tok->type == TOK_INTEGER) {
        jik_parser_eat_token(p, TOK_INTEGER);
        return jik_node_new_integer(jik_parser_current_context(p), tok);
    }
    else if (tok->type == TOK_FLOAT) {
        jik_parser_eat_token(p, TOK_FLOAT);
        return jik_node_new_float(jik_parser_current_context(p), tok);
    }
    else if (tok->type == TOK_CHAR) {
        jik_parser_eat_token(p, TOK_CHAR);
        return jik_node_new_char(tok->lexeme_char, jik_parser_current_context(p), tok);
    }
    else if (tok->type == TOK_KWD_TRUE || tok->type == TOK_KWD_FALSE) {
        jik_parser_eat_token(p, tok->type);
        bool val = strcmp(tok->lexeme, "true") == 0 ? true : false;
        return jik_node_new_boolean(val, jik_parser_current_context(p), tok);
    }
    else if (tok->type == TOK_STRING) {
        jik_parser_eat_token(p, TOK_STRING);
        JikNode *str =
            jik_node_new_string(tok->lexeme, true, false, jik_parser_current_context(p), tok);
        jik_set_alloc_spec(str, jik_parser_get_region_spec(p));
        return str;
    }
    else if (tok->type == TOK_STRING_ML) {
        jik_parser_eat_token(p, TOK_STRING_ML);
        JikNode *str =
            jik_node_new_string(tok->lexeme, true, true, jik_parser_current_context(p), tok);
        jik_set_alloc_spec(str, jik_parser_get_region_spec(p));
        return str;
    }
    else if (tok->type == TOK_KWD_SOME) {
        JikToken *some_tok = jik_parser_eat_token(p, TOK_KWD_SOME);
        jik_parser_eat_token(p, TOK_LCURL);
        JikNode *expr = jik_parser_parse_expr(p);
        jik_parser_eat_newlines_if_found(p);
        jik_parser_eat_token(p, TOK_RCURL);
        JikNode *ret = jik_node_new_option_some(expr, jik_parser_current_context(p), some_tok);
        jik_set_alloc_spec(ret, jik_parser_get_region_spec(p));
        return ret;
    }
    else if (tok->type == TOK_KWD_NONE) {
        JikToken *none_tok = jik_parser_eat_token(p, TOK_KWD_NONE);
        JikNode  *ret      = jik_node_new_option_none(jik_parser_current_context(p), none_tok);
        jik_set_alloc_spec(ret, jik_parser_get_region_spec(p));
        return ret;
    }
    else if (tok->type == TOK_ID) {
        ParsedIdentifier id = jik_parser_get_parsed_identifier(p);
        tok                 = jik_parser_current_token(p);
        char *mod_name      = id.mod_name_tok ? id.mod_name_tok->lexeme : NULL;
        if (tok->type == TOK_LPAREN) {
            if (!mod_name && strcmp(id.name_tok->lexeme, "unwrap") == 0) {
                return jik_parser_parse_option_unwrap_call(p, id.name_tok);
            }
            return jik_parser_parse_function_call(p, id.name_tok, mod_name);
        }
        if (tok->type == TOK_LCURL) {
            JikNode *struct_nd = jik_parser_parse_struct_new(p, id.name_tok, id.mod_name_tok);
            jik_set_alloc_spec(struct_nd, jik_parser_get_region_spec(p));
            return struct_nd;
        }
        if (jik_parser_match_token_sequence(
                p, (JikTokenType[]){TOK_DOT, TOK_ID, TOK_LCURL, TOK_ERROR})) {
            return jik_parser_parse_variant_new(p, id.name_tok, id.mod_name_tok);
        }
        return jik_node_new_identifier(
            id.name_tok->lexeme, mod_name, jik_parser_current_context(p), tok);
    }
    else if (tok->type == TOK_LPAREN) {
        JikToken *lpar_tok = jik_parser_eat_token(p, TOK_LPAREN);
        JikNode  *expr     = jik_parser_parse_expr(p);
        jik_parser_eat_token(p, TOK_RPAREN);
        return jik_node_new_grouping(expr, jik_parser_current_context(p), lpar_tok);
    }
    else if (tok->type == TOK_LANG) {
        JikNode *vec_lit = jik_parser_parse_vector_literal(p);
        jik_set_alloc_spec(vec_lit, jik_parser_get_region_spec(p));
        return vec_lit;
    }
    else if (tok->type == TOK_LCURL) {
        JikNode *dict_lit = jik_parser_parse_dict_literal(p);
        jik_set_alloc_spec(dict_lit, jik_parser_get_region_spec(p));
        return dict_lit;
    }
    else if (tok->type == TOK_DOT) {
        jik_parser_eat_token(p, TOK_DOT);
        JikToken *id_tok = jik_parser_eat_token(p, TOK_ID);
        return jik_node_new_regionof(jik_parser_current_context(p), id_tok);
    }
    else if (tok->type == TOK_UNDERSCORE) {
        JikToken *id_tok = jik_parser_eat_token(p, TOK_UNDERSCORE);
        return jik_node_new_local_region(jik_parser_current_context(p), id_tok);
    }
    else {
        jik_diag_fatal_error("parse error: unexpected token", jik_token_to_text(tok));
    }
}

static JikNode *
jik_parser_parse_primary(JikParser *p)
{
    JikNode  *node = jik_parser_parse_atom(p);
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL &&
           (tok->type == TOK_DOT || tok->type == TOK_LANG || tok->type == TOK_QMARK)) {
        if (tok->type == TOK_DOT) {
            jik_parser_eat_token(p, tok->type);
            JikToken *id_tok = jik_parser_eat_token(p, TOK_ID);
            node             = jik_node_new_member_access(
                node, id_tok->lexeme, jik_parser_current_context(p), id_tok);
        }
        else if (tok->type == TOK_LANG) {
            JikToken *lang_tok = jik_parser_eat_token(p, TOK_LANG);
            JikNode  *expr     = jik_parser_parse_expr(p);
            jik_parser_eat_token(p, TOK_RANG);
            node = jik_node_new_subscript_get(node, expr, jik_parser_current_context(p), lang_tok);
        }
        else if (tok->type == TOK_QMARK) {
            JikToken *qmark_tok = jik_parser_eat_token(p, TOK_QMARK);
            node = jik_node_new_option_unwrap(node, jik_parser_current_context(p), qmark_tok);
        }
    }
    return node;
}

static JikNode *
jik_parser_parse_unary(JikParser *p)
{
    JikToken *tok = jik_parser_current_token(p);
    if (tok != NULL &&
        (tok->type == TOK_KWD_NOT || tok->type == TOK_OP_MINUS || tok->type == TOK_KWD_MUST)) {
        JikToken *op_tok = jik_parser_eat_token(p, tok->type);
        JikNode  *expr   = jik_parser_parse_unary(p);
        if (tok->type == TOK_KWD_MUST) {
            jik_diag_fatal_error_if(expr->type != NODE_EXPR_CALL,
                                    "expected function call",
                                    jik_token_to_text(expr->token));
            expr->val_call.must = true;
            return jik_node_new_must(expr, jik_parser_current_context(p), op_tok);
        }
        return jik_node_new_unop(expr, op_tok->lexeme, jik_parser_current_context(p), op_tok);
    }
    return jik_parser_parse_primary(p);
}

static JikNode *
jik_parser_parse_factor(JikParser *p)
{
    JikNode  *node = jik_parser_parse_unary(p);
    JikToken *tok;
    // JikNode *node;
    while ((tok = jik_parser_current_token(p)) != NULL &&
           (tok->type == TOK_OP_TIMES || tok->type == TOK_OP_DIV || tok->type == TOK_OP_MOD)) {
        JikToken *op    = jik_parser_eat_token(p, tok->type);
        JikNode  *right = jik_parser_parse_unary(p);
        node = jik_node_new_binop(node, right, op->lexeme, jik_parser_current_context(p), tok);
    }
    return node;
}

static JikNode *
jik_parser_parse_term(JikParser *p)
{
    JikNode  *node = jik_parser_parse_factor(p);
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL &&
           (tok->type == TOK_OP_PLUS || tok->type == TOK_OP_MINUS)) {
        char *val = tok->lexeme;
        jik_parser_eat_token(p, tok->type);
        JikNode *right = jik_parser_parse_factor(p);
        node           = jik_node_new_binop(node, right, val, jik_parser_current_context(p), tok);
    }
    return node;
}

static bool
jik_parser_is_op_comp(JikToken *tok)
{
    return tok->type == TOK_OP_LT || tok->type == TOK_OP_GT || tok->type == TOK_OP_EQ ||
           tok->type == TOK_OP_NEQ || tok->type == TOK_OP_GEQ || tok->type == TOK_OP_LEQ;
}

static JikNode *
jik_parser_parse_comparison(JikParser *p)
{
    JikNode  *node = jik_parser_parse_term(p);
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL && jik_parser_is_op_comp(tok)) {
        JikToken *op    = jik_parser_eat_token(p, tok->type);
        JikNode  *right = jik_parser_parse_term(p);
        node = jik_node_new_binop(node, right, op->lexeme, jik_parser_current_context(p), op);
    }
    return node;
}

static JikNode *
jik_parser_parse_is(JikParser *p)
{
    JikNode  *left = jik_parser_parse_comparison(p);
    JikToken *tok  = jik_parser_current_token(p);
    if (!tok || tok->type != TOK_KWD_IS) {
        return left;
    }
    jik_parser_eat_token(p, TOK_KWD_IS);
    if (jik_parser_current_token(p)->type == TOK_KWD_SOME) {
        jik_parser_eat_token(p, TOK_KWD_SOME);
        return jik_node_new_option_is(left, true, jik_parser_current_context(p), tok);
    }
    if (jik_parser_current_token(p)->type == TOK_KWD_NONE) {
        jik_parser_eat_token(p, TOK_KWD_NONE);
        return jik_node_new_option_is(left, false, jik_parser_current_context(p), tok);
    }
    // JikToken *id_tok = jik_parser_eat_token(p, TOK_ID);
    JikNode *nd_id = jik_parser_parse_atom(p);
    jik_diag_fatal_error_if(nd_id->type != NODE_EXPR_IDENTIFIER,
                            "expected identifier",
                            jik_token_to_text(nd_id->token));
    jik_parser_eat_token(p, TOK_DOT);
    JikToken *id_tok = jik_parser_eat_token(p, TOK_ID);
    return jik_node_new_variant_tag_check(
        left, nd_id, id_tok->lexeme, jik_parser_current_context(p), tok);
}

static JikNode *
jik_parser_parse_option_unwrap_call(JikParser *p, JikToken *unwrap_tok)
{
    jik_parser_eat_token(p, TOK_LPAREN);
    jik_parser_eat_newlines_if_found(p);
    JikNode *expr = jik_parser_parse_expr(p);
    jik_parser_eat_newlines_if_found(p);
    jik_parser_eat_token(p, TOK_RPAREN);
    return jik_node_new_option_unwrap(expr, jik_parser_current_context(p), unwrap_tok);
}

static JikNode *
jik_parser_parse_logical_and(JikParser *p)
{
    JikNode  *node = jik_parser_parse_is(p);
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_KWD_AND) {
        JikToken *op    = jik_parser_eat_token(p, tok->type);
        JikNode  *right = jik_parser_parse_is(p);
        node = jik_node_new_binop(node, right, op->lexeme, jik_parser_current_context(p), op);
    }
    return node;
}

static JikNode *
jik_parser_parse_logical_or(JikParser *p)
{
    JikNode  *node = jik_parser_parse_logical_and(p);
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_KWD_OR) {
        JikToken *op    = jik_parser_eat_token(p, tok->type);
        JikNode  *right = jik_parser_parse_logical_and(p);
        node = jik_node_new_binop(node, right, op->lexeme, jik_parser_current_context(p), op);
    }
    return node;
}

static JikNode *
jik_parser_parse_ternary(JikParser *p)
{
    JikNode  *expr_if = jik_parser_parse_logical_or(p);
    JikToken *tok     = jik_parser_current_token(p);
    if (tok == NULL || tok->type != TOK_KWD_IF) {
        return expr_if;
    }
    jik_parser_eat_token(p, TOK_KWD_IF);
    JikNode *condition = jik_parser_parse_expr(p);
    jik_parser_eat_token(p, TOK_KWD_ELSE);
    JikNode *expr_else = jik_parser_parse_ternary(p);
    return jik_node_new_ternary(condition, expr_if, expr_else, jik_parser_current_context(p), tok);
}

static JikNode *
jik_parser_parse_expr(JikParser *p)
{
    return jik_parser_parse_ternary(p);
}

static VecJikNode *
jik_parser_parse_args(JikParser *p)
{
    VecJikNode *args = VecJikNode_new_empty();
    if (jik_parser_current_token(p) != NULL && jik_parser_current_token(p)->type == TOK_RPAREN)
        return args;
    VecJikNode_push(args, jik_parser_parse_expr(p));
    while (jik_parser_current_token(p) != NULL && jik_parser_current_token(p)->type == TOK_COMMA) {
        jik_parser_eat_token(p, TOK_COMMA);
        jik_parser_eat_newlines_if_found(p);
        VecJikNode_push(args, jik_parser_parse_expr(p));
    }
    jik_parser_eat_newlines_if_found(p);
    return args;
}

static JikNode *
jik_parser_parse_function_call(JikParser *p, JikToken *func_name_tok, char *mod_name)
{
    jik_parser_eat_token(p, TOK_LPAREN);
    jik_parser_eat_newlines_if_found(p);
    VecJikNode *args = jik_parser_parse_args(p);
    jik_parser_eat_newlines_if_found(p);
    jik_parser_eat_token(p, TOK_RPAREN);
    if (!mod_name) {
        mod_name = func_name_tok->mod_alias;
    }
    JikNode *nd_call = jik_node_new_call(
        jik_node_new_identifier(
            func_name_tok->lexeme, mod_name, jik_parser_current_context(p), func_name_tok),
        args,
        jik_parser_current_context(p),
        func_name_tok);
    nd_call->val_call.parent_func = p->parsed_function;
    return nd_call;
}

static JikNode *
jik_parser_parse_return(JikParser *p)
{
    assert(p->parsed_function);
    p->parsed_function->val_function.has_return = true;
    JikToken *tok                               = jik_parser_eat_token(p, TOK_KWD_RETURN);
    if (jik_parser_current_token(p)->type == TOK_NEWLINE) {
        return jik_node_new_return(NULL, p->parsed_function, jik_parser_current_context(p), tok);
    }
    JikNode *expr = jik_parser_parse_expr(p);
    return jik_node_new_return(expr, p->parsed_function, jik_parser_current_context(p), tok);
}

static bool
is_expr_lvalue(JikNode *expr)
{
    return expr->type == NODE_EXPR_IDENTIFIER || expr->type == NODE_EXPR_MEMBER_ACCESS ||
           expr->type == NODE_EXPR_SUBSCRIPT_GET;
}

static JikNode *
jik_parser_parse_cond_if(JikParser *);
static JikNode *
jik_parser_parse_loop_while(JikParser *);
static JikNode *
jik_parser_parse_loop_for(JikParser *);
static void
jik_parser_eat_newlines(JikParser *);
static JikNode *
jik_parser_parse_match(JikParser *p);
static JikNode *
jik_parser_parse_try(JikParser *p);
static JikNode *
jik_parser_parse_type_desc(JikParser *p);

static JikNode *
jik_parser_get_assigned_expr(JikParser *p, JikNode *lhs)
{
    if (jik_parser_current_token(p)->type == TOK_ASSIGN) {
        jik_parser_eat_token(p, TOK_ASSIGN);
        JikNode *rhs = jik_parser_parse_expr(p);
        return rhs;
    }
    if (jik_parser_current_token(p)->type == TOK_OP_PLUS_EQ) {
        JikToken *op_tok = jik_parser_eat_token(p, TOK_OP_PLUS_EQ);
        JikNode  *rhs    = jik_parser_parse_expr(p);
        return jik_node_new_binop(lhs, rhs, "+", jik_parser_current_context(p), op_tok);
    }
    if (jik_parser_current_token(p)->type == TOK_OP_MINUS_EQ) {
        JikToken *op_tok = jik_parser_eat_token(p, TOK_OP_MINUS_EQ);
        JikNode  *rhs    = jik_parser_parse_expr(p);
        return jik_node_new_binop(lhs, rhs, "-", jik_parser_current_context(p), op_tok);
    }
    if (jik_parser_current_token(p)->type == TOK_OP_TIMES_EQ) {
        JikToken *op_tok = jik_parser_eat_token(p, TOK_OP_TIMES_EQ);
        JikNode  *rhs    = jik_parser_parse_expr(p);
        return jik_node_new_binop(lhs, rhs, "*", jik_parser_current_context(p), op_tok);
    }
    if (jik_parser_current_token(p)->type == TOK_OP_DIV_EQ) {
        JikToken *op_tok = jik_parser_eat_token(p, TOK_OP_DIV_EQ);
        JikNode  *rhs    = jik_parser_parse_expr(p);
        return jik_node_new_binop(lhs, rhs, "/", jik_parser_current_context(p), op_tok);
    }
    jik_diag_fatal_error("parse error: expected assignment",
                         jik_token_to_text(jik_parser_current_token(p)));
}

static JikNode *
jik_parser_parse_statement(JikParser *p)
{
    JikToken *tok = jik_parser_current_token(p);
    if (tok->type == TOK_KWD_RETURN) {
        return jik_parser_parse_return(p);
    }
    else if (tok->type == TOK_KWD_IF) {
        return jik_parser_parse_cond_if(p);
    }
    else if (tok->type == TOK_KWD_MATCH) {
        return jik_parser_parse_match(p);
    }
    else if (tok->type == TOK_KWD_WHILE) {
        return jik_parser_parse_loop_while(p);
    }
    else if (tok->type == TOK_KWD_FOR) {
        return jik_parser_parse_loop_for(p);
    }
    else if (tok->type == TOK_KWD_BREAK) {
        JikToken *break_tok = jik_parser_eat_token(p, TOK_KWD_BREAK);
        return jik_node_new_break(jik_parser_current_context(p), break_tok);
    }
    else if (tok->type == TOK_KWD_CONTINUE) {
        JikToken *cont_tok = jik_parser_eat_token(p, TOK_KWD_CONTINUE);
        return jik_node_new_continue(jik_parser_current_context(p), cont_tok);
    }
    else if (tok->type == TOK_KWD_TRY) {
        return jik_parser_parse_try(p);
    }
    else {
        JikNode *expr = jik_parser_parse_expr(p);
        if (is_expr_lvalue(expr)) {
            if (expr->type == NODE_EXPR_IDENTIFIER) {
                if (jik_parser_current_token(p)->type == TOK_DECLARE) {
                    jik_parser_eat_token(p, TOK_DECLARE);
                    JikNode *rhs = jik_parser_parse_expr(p);
                    return jik_node_new_declare(
                        expr,
                        rhs,
                        jik_node_new_placeholder(jik_parser_current_context(p), tok),
                        jik_parser_current_context(p),
                        tok);
                }
                if (jik_parser_current_token(p)->type == TOK_COLON) {
                    jik_parser_eat_token(p, TOK_COLON);
                    JikNode *td = jik_parser_parse_type_desc(p);
                    return jik_node_new_declare(
                        expr,
                        jik_node_new_placeholder(jik_parser_current_context(p), tok),
                        td,
                        jik_parser_current_context(p),
                        tok);
                }
                JikNode *ae = jik_parser_get_assigned_expr(p, expr);
                return jik_node_new_assign(expr, ae, jik_parser_current_context(p), tok);
            }
            else if (expr->type == NODE_EXPR_MEMBER_ACCESS) {
                return jik_node_new_member_set(expr->val_member_access.node,
                                               expr->val_member_access.member_name,
                                               jik_parser_get_assigned_expr(p, expr),
                                               jik_parser_current_context(p),
                                               tok);
            }
            else if (expr->type == NODE_EXPR_SUBSCRIPT_GET) {
                return jik_node_new_subscript_set(expr->val_subscript_get.node,
                                                  expr->val_subscript_get.expr,
                                                  jik_parser_get_assigned_expr(p, expr),
                                                  jik_parser_current_context(p),
                                                  tok);
            }
        }
        else if (expr->type == NODE_EXPR_CALL) {
            return expr;
        }
        else if (expr->type == NODE_EXPR_MUST) {
            return expr;
        }
        else
            jik_diag_fatal_error("parse error: invalid statement", jik_token_to_text(tok));
    }
    jik_diag_fatal_error("parse error: unexpected end of statement", jik_token_to_text(tok));
}

static void
jik_parser_eat_newlines(JikParser *p)
{
    jik_parser_eat_token(p, TOK_NEWLINE);
    JikToken *tok;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_NEWLINE) {
        jik_parser_eat_token(p, TOK_NEWLINE);
    }
}

static JikNode *
jik_parser_parse_block(JikParser *p)
{
    VecJikScope_push(p->contexts, jik_scope_new(jik_parser_current_context(p)));
    JikNode  *block = jik_node_new_block();
    JikToken *tok;
    JikNode  *nd;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type != TOK_KWD_END &&
           tok->type != TOK_KWD_ELSE && tok->type != TOK_KWD_ELIF && tok->type != TOK_KWD_CASE &&
           tok->type != TOK_KWD_EXCEPT) {
        nd = jik_parser_parse_statement(p);
        jik_parser_eat_newlines(p);
        VecJikNode_push(block->val_block, nd);
    }
    block->context = VecJikScope_pop(p->contexts);
    return block;
}

static JikNode *
jik_parser_parse_cond_if(JikParser *p)
{
    JikToken *tok     = jik_parser_eat_token(p, TOK_KWD_IF);
    JikNode  *if_expr = jik_parser_parse_expr(p);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);
    JikNode  *if_body = jik_parser_parse_block(p);
    JikToken *curr    = jik_parser_current_token(p);
    if (curr->type == TOK_KWD_END) {
        jik_parser_eat_token(p, TOK_KWD_END);
        return jik_node_new_cond_if(if_expr, if_body, jik_parser_current_context(p), tok);
    }
    if (curr->type == TOK_KWD_ELSE) {
        jik_parser_eat_token(p, TOK_KWD_ELSE);
        jik_parser_eat_token(p, TOK_COLON);
        jik_parser_eat_newlines(p);
        JikNode *body_else = jik_parser_parse_block(p);
        jik_parser_eat_token(p, TOK_KWD_END);
        return jik_node_new_cond_ifelse(
            if_expr, if_body, body_else, jik_parser_current_context(p), tok);
    }
    VecJikNode *elifs = VecJikNode_new_empty();
    while (curr->type == TOK_KWD_ELIF) {
        jik_parser_eat_token(p, TOK_KWD_ELIF);
        JikNode *expr = jik_parser_parse_expr(p);
        VecJikNode_push(elifs, expr);
        jik_parser_eat_token(p, TOK_COLON);
        jik_parser_eat_newlines(p);
        JikNode *body = jik_parser_parse_block(p);
        VecJikNode_push(elifs, body);
        curr = jik_parser_current_token(p);
        if (curr->type == TOK_KWD_ELSE) {
            jik_parser_eat_token(p, TOK_KWD_ELSE);
            jik_parser_eat_token(p, TOK_COLON);
            jik_parser_eat_newlines(p);
            JikNode *body_else = jik_parser_parse_block(p);
            jik_parser_eat_token(p, TOK_KWD_END);
            return jik_node_new_cond_ifelif(
                if_expr, if_body, elifs, body_else, jik_parser_current_context(p), tok);
        }
        if (curr->type == TOK_KWD_END) {
            jik_parser_eat_token(p, TOK_KWD_END);
            return jik_node_new_cond_ifelif(
                if_expr, if_body, elifs, NULL, jik_parser_current_context(p), tok);
        }
    }
    jik_diag_fatal_error("parse error: expected elif", jik_token_to_text(curr));
}

static JikNode *
jik_parser_parse_match(JikParser *p)
{
    JikToken *tok  = jik_parser_eat_token(p, TOK_KWD_MATCH);
    JikNode  *expr = jik_parser_parse_expr(p);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);
    JikToken   *curr  = jik_parser_current_token(p);
    VecJikNode *cases = VecJikNode_new_empty();
    JikNode    *match = jik_node_new_match(NULL, NULL, jik_parser_current_context(p), tok);
    while (curr->type != TOK_KWD_END) {
        jik_parser_eat_token(p, TOK_KWD_CASE);
        JikNode *var_tag = jik_parser_parse_atom(p);
        jik_diag_fatal_error_if(
            var_tag->type != NODE_EXPR_VARIANT_NEW, "expected variant", jik_token_to_text(curr));
        if (var_tag->val_variant_new.init_expr &&
            var_tag->val_variant_new.init_expr->type != NODE_EXPR_IDENTIFIER) {
            jik_diag_fatal_error("expected identifier",
                                 jik_token_to_text(var_tag->val_variant_new.init_expr->token));
        }
        jik_parser_eat_token(p, TOK_COLON);
        jik_parser_eat_newlines(p);
        JikNode *body = jik_parser_parse_block(p);
        JikNode *case_ =
            jik_node_new_case(var_tag, body, match, jik_parser_current_context(p), tok);
        VecJikNode_push(cases, case_);
        curr = jik_parser_current_token(p);
    }
    jik_parser_eat_token(p, TOK_KWD_END);
    match->val_match.expr  = expr;
    match->val_match.cases = cases;
    // return jik_node_new_match(expr, cases, jik_parser_current_context(p), tok);
    return match;
}

static JikNode *
jik_parser_parse_try(JikParser *p)
{
    JikToken *tok   = jik_parser_eat_token(p, TOK_KWD_TRY);
    JikNode  *stmnt = jik_parser_parse_statement(p);
    jik_diag_fatal_error_if(stmnt->type != NODE_STMNT_DECLARE && stmnt->type != NODE_EXPR_CALL,
                            "expected declare or call",
                            jik_token_to_text(stmnt->token));
    jik_diag_fatal_error_if(stmnt->type == NODE_STMNT_DECLARE &&
                                stmnt->val_declare.expr->type != NODE_EXPR_CALL,
                            "expected function call",
                            jik_token_to_text(stmnt->token));
    if (stmnt->type == NODE_STMNT_DECLARE) {
        stmnt->val_declare.expr->val_call.must = true;
    }
    else {
        stmnt->val_call.must = true;
    }

    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);

    JikNode *pass_body = jik_parser_parse_block(p);
    // Ugly, but works. That way we make the declared symbol be vaid only in the try block.
    if (stmnt->type == NODE_STMNT_DECLARE) {
        stmnt->context                   = pass_body->context;
        stmnt->val_declare.expr->context = pass_body->context;
        stmnt->val_declare.id->context   = pass_body->context;
    }
    else {
        stmnt->context = pass_body->context;
    }

    jik_parser_eat_token(p, TOK_KWD_EXCEPT);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);
    JikNode *err_body = jik_parser_parse_block(p);
    jik_parser_eat_token(p, TOK_KWD_END);
    return jik_node_new_catch(stmnt, err_body, pass_body, jik_parser_current_context(p), tok);
}

static JikNode *
jik_parser_parse_loop_while(JikParser *p)
{
    JikToken *tok  = jik_parser_eat_token(p, TOK_KWD_WHILE);
    JikNode  *expr = jik_parser_parse_expr(p);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);
    JikNode *body = jik_parser_parse_block(p);
    jik_parser_eat_token(p, TOK_KWD_END);
    return jik_node_new_loop_while(expr, body, jik_parser_current_context(p), tok);
}

static JikNode *
jik_parser_parse_loop_for(JikParser *p)
{
    JikToken *tok = jik_parser_eat_token(p, TOK_KWD_FOR);

    JikToken *var_name_tok = jik_parser_eat_token(p, TOK_ID);

    // TODO: probably can be written better
    if (jik_parser_current_token(p)->type == TOK_KWD_IN) {
        jik_parser_eat_token(p, TOK_KWD_IN);
        JikNode *obj_expr = jik_parser_parse_expr(p);
        jik_parser_eat_token(p, TOK_COLON);
        jik_parser_eat_newlines(p);
        // VecJikScope_push(p->contexts, jik_scope_new(jik_parser_current_context(p)));
        JikNode *var_name = jik_node_new_identifier(var_name_tok->lexeme,
                                                    var_name_tok->mod_alias,
                                                    jik_parser_current_context(p),
                                                    var_name_tok);
        JikNode *body     = jik_parser_parse_block(p);
        // VecJikScope_pop(p->contexts);
        jik_parser_eat_token(p, TOK_KWD_END);
        return jik_node_new_loop_for_in(
            var_name, obj_expr, body, jik_parser_current_context(p), tok);
    }
    else if (jik_parser_current_token(p)->type == TOK_COMMA) {
        jik_parser_eat_token(p, TOK_COMMA);
        JikToken *var_name_tok2 = jik_parser_eat_token(p, TOK_ID);
        jik_parser_eat_token(p, TOK_KWD_IN);
        JikNode *obj_expr = jik_parser_parse_expr(p);
        jik_parser_eat_token(p, TOK_COLON);
        jik_parser_eat_newlines(p);
        // VecJikScope_push(p->contexts, jik_scope_new(jik_parser_current_context(p)));
        JikNode *var_name  = jik_node_new_identifier(var_name_tok->lexeme,
                                                    var_name_tok->mod_alias,
                                                    jik_parser_current_context(p),
                                                    var_name_tok);
        JikNode *var_name2 = jik_node_new_identifier(var_name_tok2->lexeme,
                                                     var_name_tok2->mod_alias,
                                                     jik_parser_current_context(p),
                                                     var_name_tok2);
        JikNode *body      = jik_parser_parse_block(p);
        // VecJikScope_pop(p->contexts);
        jik_parser_eat_token(p, TOK_KWD_END);
        return jik_node_new_loop_for_in_dict(
            var_name, var_name2, obj_expr, body, jik_parser_current_context(p), tok);
    }

    JikNode *var_name = jik_node_new_identifier(
        var_name_tok->lexeme, var_name_tok->mod_alias, jik_parser_current_context(p), var_name_tok);
    var_name->jik_type = &JIK_TYPE_INT;
    jik_parser_eat_token(p, TOK_ASSIGN);
    JikNode *start_expr = jik_parser_parse_expr(p);
    jik_parser_eat_token(p, TOK_COMMA);
    JikNode *end_expr = jik_parser_parse_expr(p);

    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);
    // VecJikScope_push(p->contexts, jik_scope_new(jik_parser_current_context(p)));
    JikNode *body = jik_parser_parse_block(p);
    // VecJikScope_pop(p->contexts);
    jik_parser_eat_token(p, TOK_KWD_END);

    return jik_node_new_loop_for(
        var_name, start_expr, end_expr, body, jik_parser_current_context(p), tok);
}

static VecJikNode *
jik_parser_parse_param_sequence(JikParser *p, bool is_extern)
{
    JikToken   *tok;
    VecJikNode *params = VecJikNode_new_empty();

    bool foreign = false;
    if ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_KWD_FOREIGN) {
        foreign = true;
        jik_parser_eat_token(p, TOK_KWD_FOREIGN);
    }

    if ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_ID) {
        JikNode *id =
            jik_node_new_identifier(tok->lexeme, NULL, jik_parser_current_context(p), tok);
        id->val_id.is_foreign = foreign;
        jik_parser_eat_token(p, TOK_ID);
        if (jik_parser_current_token(p)->type == TOK_COLON) {
            jik_parser_eat_token(p, TOK_COLON);
            id->val_id.type_annot = jik_parser_parse_type_desc(p);
        }
        else {
            jik_diag_fatal_error_if(is_extern, "expected type annotation", jik_token_to_text(tok));
        }
        VecJikNode_push(params, id);
    }
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_COMMA) {
        jik_parser_eat_token(p, TOK_COMMA);
        jik_parser_eat_newlines_if_found(p);

        bool foreign = false;
        if ((tok = jik_parser_current_token(p)) != NULL && tok->type == TOK_KWD_FOREIGN) {
            foreign = true;
            jik_parser_eat_token(p, TOK_KWD_FOREIGN);
        }

        JikToken *t2 = jik_parser_eat_token(p, TOK_ID);
        JikNode  *id = jik_node_new_identifier(t2->lexeme, NULL, jik_parser_current_context(p), t2);
        id->val_id.is_foreign = foreign;
        if (jik_parser_current_token(p)->type == TOK_COLON) {
            jik_parser_eat_token(p, TOK_COLON);
            id->val_id.type_annot = jik_parser_parse_type_desc(p);
        }
        else {
            jik_diag_fatal_error_if(is_extern, "expected type annotation", jik_token_to_text(tok));
        }
        VecJikNode_push(params, id);
    }
    return params;
}

static JikNode *
jik_parser_parse_global(JikParser *p)
{
    JikToken *global_name_tok = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_DECLARE);
    JikNode *expr = jik_parser_parse_expr(p);
    JikNode *id   = jik_node_new_identifier(global_name_tok->lexeme,
                                          global_name_tok->mod_alias,
                                          jik_parser_current_context(p),
                                          global_name_tok);
    if (jik_node_is_allocated_literal(expr)) {
        jik_set_alloc_spec(expr,
                           (JikAllocSpec){.kind = JIK_ALLOC_GLOBAL, .src = JIK_ALLOC_SRC_FOREIGN});
    }
    id->val_id.is_global = true;
    JikNode *declare     = jik_node_new_declare(
        id,
        expr,
        jik_node_new_placeholder(jik_parser_current_context(p), global_name_tok),
        jik_parser_current_context(p),
        global_name_tok);
    declare->val_declare.global = true;
    return declare;
}

static JikNode *
jik_parser_parse_type_desc(JikParser *p)
{
    JikToken *tok = jik_parser_current_token(p);
    if (tok->type == TOK_ID) {
        ParsedIdentifier id        = jik_parser_get_parsed_identifier(p);
        char            *mod_name  = id.mod_name_tok ? id.mod_name_tok->lexeme : NULL;
        JikNode         *type_name = jik_node_new_identifier(
            id.name_tok->lexeme, mod_name, jik_parser_current_context(p), tok);
        return jik_node_new_type_desc(type_name,
                                      NULL,
                                      TYPE_UNKNOWN,
                                      jik_parser_get_region_spec(p),
                                      jik_parser_current_context(p),
                                      tok);
    }
    else if (tok->type == TOK_KWD_VEC) {
        jik_parser_eat_token(p, TOK_KWD_VEC);
        jik_parser_eat_token(p, TOK_LANG);
        JikNode *td = jik_parser_parse_type_desc(p);
        jik_parser_eat_token(p, TOK_RANG);
        return jik_node_new_type_desc(NULL,
                                      td,
                                      TYPE_VECTOR,
                                      jik_parser_get_region_spec(p),
                                      jik_parser_current_context(p),
                                      tok);
    }
    else if (tok->type == TOK_KWD_DICT) {
        jik_parser_eat_token(p, TOK_KWD_DICT);
        jik_parser_eat_token(p, TOK_LANG);
        JikNode *td = jik_parser_parse_type_desc(p);
        jik_parser_eat_token(p, TOK_RANG);
        return jik_node_new_type_desc(
            NULL, td, TYPE_DICT, jik_parser_get_region_spec(p), jik_parser_current_context(p), tok);
    }
    else if (tok->type == TOK_KWD_OPTION) {
        jik_parser_eat_token(p, TOK_KWD_OPTION);
        jik_parser_eat_token(p, TOK_LANG);
        JikNode *td = jik_parser_parse_type_desc(p);
        jik_parser_eat_token(p, TOK_RANG);
        return jik_node_new_type_desc(NULL,
                                      td,
                                      TYPE_OPTION,
                                      jik_parser_get_region_spec(p),
                                      jik_parser_current_context(p),
                                      tok);
    }
    else {
        jik_diag_fatal_error("parse error: expected type name", jik_token_to_text(tok));
    }
}

static JikNode *
jik_parser_parse_function(JikParser *p, bool throws)
{
    jik_parser_eat_token(p, TOK_KWD_FUNC);
    JikToken *tok_func_name = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_LPAREN);
    jik_parser_eat_newlines_if_found(p);
    // VecJikScope_push(p->contexts, jik_scope_new(jik_parser_current_context(p)));
    VecJikNode *params = jik_parser_parse_param_sequence(p, false);
    jik_parser_eat_newlines_if_found(p);
    jik_parser_eat_token(p, TOK_RPAREN);
    JikNode *ret_type_annot = NULL;
    if (jik_parser_current_token(p)->type == TOK_ARROW) {
        jik_parser_eat_token(p, TOK_ARROW);
        ret_type_annot = jik_parser_parse_type_desc(p);
    }
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);

    JikNode *function                     = jik_node_new_function(tok_func_name->lexeme,
                                              params,
                                              NULL,
                                              NULL,
                                              throws,
                                              jik_parser_current_context(p),
                                              tok_func_name);
    function->val_function.ret_type_annot = ret_type_annot;
    p->parsed_function                    = function;

    JikNode *body = jik_parser_parse_block(p);
    // VecJikScope_pop(p->contexts);
    function->val_function.body = body;
    size_t num_params           = VecJikNode_size(params);

    function->jik_type = jik_type_new_function(num_params);
    jik_parser_eat_token(p, TOK_KWD_END);
    p->parsed_function = NULL;
    return function;
}

static JikNode *
jik_parser_parse_extern_function(JikParser *p, bool throws, bool init)
{
    JikToken *tok_C_func_name = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_KWD_AS);
    jik_parser_eat_newlines_if_found(p);
    JikToken *tok_func_name = jik_parser_eat_token(p, TOK_ID);
    JikNode  *function      = jik_node_new_extern_function(tok_func_name->lexeme,
                                                     tok_C_func_name->lexeme,
                                                     throws,
                                                     init,
                                                     jik_parser_current_context(p),
                                                     tok_func_name);
    jik_parser_eat_token(p, TOK_LPAREN);
    VecJikNode *params = jik_parser_parse_param_sequence(p, true);
    jik_parser_eat_token(p, TOK_RPAREN);
    jik_parser_eat_token(p, TOK_ARROW);
    JikNode *ret_nd                        = jik_parser_parse_type_desc(p);
    function->val_extern_function.params   = params;
    function->val_extern_function.ret_node = ret_nd;
    return function;
}

static JikNode *
jik_parser_parse_extern_struct(JikParser *p)
{
    JikToken *tok_C_struct_name = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_KWD_AS);
    jik_parser_eat_newlines_if_found(p);
    JikToken *tok_struct_name     = jik_parser_eat_token(p, TOK_ID);
    JikNode  *struct_node         = jik_node_new_struct(tok_struct_name->lexeme,
                                               TabJikNode_new(),
                                               true,
                                               jik_parser_current_context(p),
                                               tok_struct_name);
    struct_node->jik_type         = jik_type_new_struct(tok_struct_name->lexeme, TabJikType_new());
    struct_node->jik_type->C_name = JIK_STRING_NCAT("struct ", tok_C_struct_name->lexeme, " *");
    struct_node->jik_type->is_extern = true;
    return struct_node;
}

static JikNode *
jik_parser_parse_struct(JikParser *p)
{
    jik_parser_eat_token(p, TOK_KWD_STRUCT);
    JikToken *tok_struct_name = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);

    JikNode *struct_node = jik_node_new_struct(
        tok_struct_name->lexeme, NULL, false, jik_parser_current_context(p), tok_struct_name);
    p->parsed_struct = struct_node;
    JikToken   *tok;
    TabJikNode *type_descs = TabJikNode_new();
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type != TOK_KWD_END) {
        char *field_name = jik_parser_eat_token(p, TOK_ID)->lexeme;
        VecString_push(struct_node->val_struct.field_order, field_name);
        jik_parser_eat_token(p, TOK_COLON);
        JikNode *td = jik_parser_parse_type_desc(p);
        jik_parser_eat_newlines(p);
        TabJikNode_set(type_descs, field_name, td);
    }
    jik_parser_eat_token(p, TOK_KWD_END);
    struct_node->val_struct.type_descs = type_descs;
    p->parsed_struct                   = NULL;
    return struct_node;
}

static JikNode *
jik_parser_parse_variant(JikParser *p)
{
    jik_parser_eat_token(p, TOK_KWD_VARIANT);
    JikToken *tok_var_name = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);

    JikNode *variant_node = jik_node_new_variant(
        tok_var_name->lexeme, NULL, jik_parser_current_context(p), tok_var_name);
    JikToken   *tok;
    TabJikNode *type_descs   = TabJikNode_new();
    char       *first_member = NULL;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type != TOK_KWD_END) {
        char *field_name = jik_parser_eat_token(p, TOK_ID)->lexeme;
        if (!first_member) {
            first_member = field_name;
        }
        VecString_push(variant_node->val_variant.member_order, field_name);
        jik_parser_eat_token(p, TOK_COLON);
        JikNode *td = jik_parser_parse_type_desc(p);
        jik_parser_eat_newlines(p);
        TabJikNode_set(type_descs, field_name, td);
    }
    jik_parser_eat_token(p, TOK_KWD_END);
    variant_node->val_variant.type_descs   = type_descs;
    variant_node->val_variant.first_member = first_member;
    return variant_node;
}

static JikNode *
jik_parser_parse_enum(JikParser *p)
{
    jik_parser_eat_token(p, TOK_KWD_ENUM);
    JikToken *tok_enum_name = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_COLON);
    jik_parser_eat_newlines(p);
    TabBool   *enumerators      = TabBool_new();
    VecString *enumerator_order = VecString_new_empty();
    JikToken  *tok;
    char      *first = NULL;
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type != TOK_KWD_END) {
        JikToken *tok_var    = jik_parser_eat_token(p, TOK_ID);
        char     *enumerator = tok_var->lexeme;
        if (!first) {
            first = enumerator;
        }
        if (TabBool_get(enumerators, enumerator)) {
            jik_diag_fatal_error("duplicate enumerator", jik_token_to_text(tok_var));
        }
        VecString_push(enumerator_order, enumerator);
        jik_parser_eat_newlines(p);
        TabBool_set(enumerators, enumerator, true);
    }
    jik_parser_eat_token(p, TOK_KWD_END);
    JikNode *enum_node = jik_node_new_enum(
        tok_enum_name->lexeme, enumerators, first, jik_parser_current_context(p), tok_enum_name);
    for (size_t i = 0; i < VecString_size(enumerator_order); i++) {
        VecString_push(enum_node->val_enum.enumerator_order, VecString_get(enumerator_order, i));
    }
    return enum_node;
}

static JikNode *
jik_parser_parse_struct_new(JikParser *p, JikToken *struct_name_tok, JikToken *mod_name_tok)
{
    p->container_depth++;
    char    *mod_name    = mod_name_tok != NULL ? mod_name_tok->lexeme : struct_name_tok->mod_alias;
    char    *struct_name = struct_name_tok->lexeme;
    JikNode *id          = jik_node_new_identifier(
        struct_name, mod_name, jik_parser_current_context(p), struct_name_tok);
    jik_parser_eat_token(p, TOK_LCURL);
    jik_parser_eat_newlines_if_found(p);
    JikToken   *tok;
    TabJikNode *init_vals = TabJikNode_new();
    while ((tok = jik_parser_current_token(p)) != NULL && tok->type != TOK_RCURL) {
        jik_parser_eat_newlines_if_found(p);
        char *field_name = jik_parser_eat_token(p, TOK_ID)->lexeme;
        jik_parser_eat_token(p, TOK_ASSIGN);
        JikNode *expr = jik_parser_parse_expr(p);
        jik_parser_eat_newlines_if_found(p);
        TabJikNode_set(init_vals, field_name, expr);
        if (jik_parser_current_token(p)->type != TOK_RCURL) {
            jik_parser_eat_token(p, TOK_COMMA);
        }
    }
    jik_parser_eat_token(p, TOK_RCURL);
    p->container_depth--;
    return jik_node_new_struct_new(id, init_vals, jik_parser_current_context(p), struct_name_tok);
}

static JikNode *
jik_parser_parse_variant_new(JikParser *p, JikToken *var_name_tok, JikToken *mod_name_tok)
{
    char    *mod_name = mod_name_tok != NULL ? mod_name_tok->lexeme : var_name_tok->mod_alias;
    char    *var_name = var_name_tok->lexeme;
    JikNode *id =
        jik_node_new_identifier(var_name, mod_name, jik_parser_current_context(p), var_name_tok);
    jik_parser_eat_token(p, TOK_DOT);
    JikToken *variant = jik_parser_eat_token(p, TOK_ID);
    jik_parser_eat_token(p, TOK_LCURL);
    jik_parser_eat_newlines_if_found(p);
    if (jik_parser_current_token(p)->type == TOK_RCURL) {
        jik_parser_eat_token(p, TOK_RCURL);
        JikNode *ret = jik_node_new_variant_new(
            id, NULL, variant->lexeme, jik_parser_current_context(p), var_name_tok);
        jik_set_alloc_spec(ret, jik_parser_get_region_spec(p));
        return ret;
    }
    JikNode *init_expr = jik_parser_parse_expr(p);
    jik_parser_eat_newlines_if_found(p);
    jik_parser_eat_token(p, TOK_RCURL);
    JikNode *ret = jik_node_new_variant_new(
        id, init_expr, variant->lexeme, jik_parser_current_context(p), var_name_tok);
    jik_set_alloc_spec(ret, jik_parser_get_region_spec(p));
    return ret;
}

bool
is_base_literal(JikNode *nd)
{
    if (nd->type == NODE_EXPR_INTEGER) {
        return strcmp(nd->token->lexeme, "0") == 0;
    }
    if (nd->type == NODE_EXPR_STRING) {
        return strcmp(nd->val_str.val, "") == 0;
    }
    if (nd->type == NODE_EXPR_BOOL) {
        return nd->val_bool == true;
    }
    if (nd->type == NODE_EXPR_CHAR) {
        return nd->val_char == '0';
    }
    return false;
}

static void
jik_parser_skip_usage(JikParser *p)
{
    jik_parser_eat_token(p, TOK_KWD_USE);
    jik_parser_eat_token(p, TOK_STRING);
    JikToken *curr = jik_parser_current_token(p);
    if (curr->type == TOK_KWD_AS) {
        jik_parser_eat_token(p, TOK_KWD_AS);
        jik_parser_eat_token(p, TOK_ID);
    }
}

typedef struct {
    bool is_extern;
    bool is_throws;
    bool is_init;
} JikFuncPrefix;

static JikFuncPrefix
jik_parser_parse_decl_prefix(JikParser *p)
{
    JikFuncPrefix px = {0};
    bool          progressed;

    do {
        progressed    = false;
        JikToken *tok = jik_parser_current_token(p);
        if (tok == NULL) {
            break;
        }

        if (tok->type == TOK_KWD_EXTERN && !px.is_extern) {
            jik_parser_eat_token(p, TOK_KWD_EXTERN);
            px.is_extern = true;
            progressed   = true;
        }
        else if (tok->type == TOK_KWD_THROWS && !px.is_throws) {
            jik_parser_eat_token(p, TOK_KWD_THROWS);
            px.is_throws = true;
            progressed   = true;
        }
        else if (tok->type == TOK_KWD_INIT && !px.is_init) {
            jik_parser_eat_token(p, TOK_KWD_INIT);
            px.is_init = true;
            progressed = true;
        }
    } while (progressed);

    return px;
}

static void
jik_parser_parse(JikParser *p)
{
    JikToken *tok;
    JikNode  *nd;
    while ((tok = jik_parser_current_token(p)) != NULL) {
        if (tok->type == TOK_NEWLINE) {
            jik_parser_eat_token(p, TOK_NEWLINE);
            continue;
        }
        else if (tok->type == TOK_ID) {
            nd = jik_parser_parse_global(p);
            VecJikNode_push(p->ast->val_program.globals, nd);
        }
        else if (tok->type == TOK_KWD_EXTERN || tok->type == TOK_KWD_THROWS ||
                 tok->type == TOK_KWD_INIT || tok->type == TOK_KWD_FUNC ||
                 tok->type == TOK_KWD_STRUCT) {
            JikFuncPrefix px = jik_parser_parse_decl_prefix(p);
            tok              = jik_parser_current_token(p);
            jik_diag_fatal_error_if(!tok, "unexpected end of file", "");

            if (tok->type == TOK_KWD_FUNC) {
                if (px.is_extern) {
                    jik_diag_fatal_error_if(px.is_throws && px.is_init,
                                            "extern init functions cannot throw",
                                            jik_token_to_text(tok));
                    jik_parser_eat_token(p, TOK_KWD_FUNC);
                    nd = jik_parser_parse_extern_function(p, px.is_throws, px.is_init);
                    VecJikNode_push(p->ast->val_program.extern_functions, nd);
                }
                else {
                    jik_diag_fatal_error_if(px.is_init,
                                            "init is only supported for extern functions",
                                            jik_token_to_text(tok));
                    nd = jik_parser_parse_function(p, px.is_throws);
                    VecJikNode_push(p->ast->val_program.functions, nd);
                }
            }
            else if (tok->type == TOK_KWD_STRUCT) {
                jik_diag_fatal_error_if(
                    px.is_throws, "unsupported declaration", jik_token_to_text(tok));
                jik_diag_fatal_error_if(px.is_init,
                                        "init is only supported for extern functions",
                                        jik_token_to_text(tok));
                if (px.is_extern) {
                    jik_parser_eat_token(p, TOK_KWD_STRUCT);
                    nd = jik_parser_parse_extern_struct(p);
                    VecJikNode_push(p->ast->val_program.extern_structs, nd);
                }
                else {
                    nd = jik_parser_parse_struct(p);
                    VecJikNode_push(p->ast->val_program.structs, nd);
                }
            }
            else {
                jik_diag_fatal_error("unsupported declaration", jik_token_to_text(tok));
            }
        }
        else if (tok->type == TOK_KWD_VARIANT) {
            nd = jik_parser_parse_variant(p);
            VecJikNode_push(p->ast->val_program.variants, nd);
        }
        else if (tok->type == TOK_KWD_ENUM) {
            nd = jik_parser_parse_enum(p);
            VecJikNode_push(p->ast->val_program.enums, nd);
        }
        else if (tok->type == TOK_KWD_USE) {
            jik_parser_skip_usage(p);
        }
        else if (tok->type == TOK_EMBEDDED_C) {
            nd = jik_node_new_embedded_C(tok->lexeme, NULL, tok);
            VecJikNode_push(p->ast->val_program.embedded_C, nd);
            jik_parser_eat_token(p, TOK_EMBEDDED_C);
        }
        else {
            jik_diag_fatal_error("parse error - unhandled token", jik_token_to_text(tok));
        }
    }
}

static void
jik_parser_add_builtin(JikParser *p, char *name, JikType *func_type)
{
    JikNode *nd =
        jik_node_new_builtin_function(name, func_type, jik_parser_current_context(p), NULL);
    VecJikNode_push(p->ast->val_program.builtin_functions, nd);
}

JikType *
make_builtin_function_type(int num_params, JikType *ret_type, JikType *param_types[])
{
    JikType *ft           = jik_type_new_function(num_params);
    ft->val_func.ret_type = ret_type;
    ft->val_func.builtin  = true;
    for (size_t i = 0; param_types[i] != NULL; i++) {
        VecJikType_set(ft->val_func.param_types, i, param_types[i]);
    }
    return ft;
}

static void
jik_parser_add_builtins(JikParser *p)
{
    // NOTE: we use "-1" to model both variadic and polymorphic functions
    jik_parser_add_builtin(
        p, "print", make_builtin_function_type(-1, &JIK_TYPE_VOID, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p, "println", make_builtin_function_type(-1, &JIK_TYPE_VOID, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p, "concat", make_builtin_function_type(-1, &JIK_TYPE_STRING, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p,
        "assert",
        make_builtin_function_type(1, &JIK_TYPE_VOID, (JikType *[]){&JIK_TYPE_BOOL, NULL}));
    jik_parser_add_builtin(
        p, "push", make_builtin_function_type(-1, &JIK_TYPE_VOID, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p, "pop", make_builtin_function_type(-1, jik_type_new(TYPE_UNKNOWN), (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p, "len", make_builtin_function_type(-1, &JIK_TYPE_INT, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p, "clear", make_builtin_function_type(1, &JIK_TYPE_VOID, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p, "site", make_builtin_function_type(0, &JIK_TYPE_SITE, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p,
        "site_file",
        make_builtin_function_type(
            2, &JIK_TYPE_STRING, (JikType *[]){&JIK_TYPE_SITE, &JIK_TYPE_REGION, NULL}));
    jik_parser_add_builtin(
        p,
        "site_line",
        make_builtin_function_type(1, &JIK_TYPE_INT, (JikType *[]){&JIK_TYPE_SITE, NULL}));
    jik_parser_add_builtin(
        p,
        "site_code",
        make_builtin_function_type(
            2, &JIK_TYPE_STRING, (JikType *[]){&JIK_TYPE_SITE, &JIK_TYPE_REGION, NULL}));

    // new errors
    jik_parser_add_builtin(
        p, "fail", make_builtin_function_type(-1, &JIK_TYPE_VOID, (JikType *[]){NULL}));
    jik_parser_add_builtin(
        p,
        "error_msg",
        make_builtin_function_type(1, &JIK_TYPE_STRING, (JikType *[]){&JIK_TYPE_REGION, NULL}));
    jik_parser_add_builtin(
        p, "error_code", make_builtin_function_type(0, &JIK_TYPE_INT, (JikType *[]){NULL}));
}

void
jik_parser_run(JikParser *p)
{
    jik_parser_parse(p);
    jik_parser_add_builtins(p);
    p->ctx->ast = p->ast;
}
