#include "token.h"

#include "alloc.h"
#include "charbuf.h"
#include "utils.h"

const char *JIK_TOKEN_NAMES[NUM_TOKENS] = {
#define X(name) [name] = #name,
    TOKEN_NAMES
#undef X
};

JIK_VEC_DEFINE(VecJikToken, JikToken);

void
JikToken_print(JikToken *t)
{
    printf("<JikToken: type=%s, lexeme=%s, lexeme_char=%c, lineno=%zu, colno=%zu, filepath=%s, "
           "codeline=%s>",
           JIK_TOKEN_NAMES[t->type],
           t->lexeme,
           t->lexeme_char,
           t->lineno,
           t->colno,
           t->filepath,
           t->codeline);
}

void
VecJikToken_print(VecJikToken *tokens)
{
    size_t n = VecJikToken_size(tokens);
    for (size_t i = 0; i < n; i++) {
        JikToken_print(VecJikToken_get_ptr(tokens, i));
        printf("\n");
    }
}

const char *
jik_token_type_name(JikTokenType type)
{
    return JIK_TOKEN_NAMES[type];
}

const char *
jik_token_type_pretty_name(JikTokenType type)
{
    switch (type) {
    case TOK_NEWLINE:
        return "newline";
    case TOK_INTEGER:
        return "integer literal";
    case TOK_FLOAT:
        return "float literal";
    case TOK_STRING:
        return "string literal";
    case TOK_STRING_ML:
        return "multiline string literal";
    case TOK_CHAR:
        return "character literal";
    case TOK_ID:
        return "identifier";
    case TOK_LPAREN:
        return "(";
    case TOK_RPAREN:
        return ")";
    case TOK_LCURL:
        return "{";
    case TOK_RCURL:
        return "}";
    case TOK_LANG:
        return "[";
    case TOK_RANG:
        return "]";
    case TOK_COLON:
        return ":";
    case TOK_DOUBLE_COLON:
        return "::";
    case TOK_COMMA:
        return ",";
    case TOK_KWD_FUNC:
        return "func";
    case TOK_KWD_THROWS:
        return "throws";
    case TOK_KWD_MUST:
        return "must";
    case TOK_KWD_TRY:
        return "try";
    case TOK_KWD_EXCEPT:
        return "except";
    case TOK_KWD_END:
        return "end";
    case TOK_KWD_STRUCT:
        return "struct";
    case TOK_KWD_ENUM:
        return "enum";
    case TOK_KWD_VARIANT:
        return "variant";
    case TOK_KWD_IF:
        return "if";
    case TOK_KWD_ELIF:
        return "elif";
    case TOK_KWD_ELSE:
        return "else";
    case TOK_KWD_WHILE:
        return "while";
    case TOK_KWD_FOR:
        return "for";
    case TOK_KWD_BREAK:
        return "break";
    case TOK_KWD_CONTINUE:
        return "continue";
    case TOK_KWD_RETURN:
        return "return";
    case TOK_KWD_USE:
        return "use";
    case TOK_KWD_AS:
        return "as";
    case TOK_KWD_OF:
        return "of";
    case TOK_KWD_EXTERN:
        return "extern";
    case TOK_KWD_TRUE:
        return "true";
    case TOK_KWD_FALSE:
        return "false";
    case TOK_KWD_IS:
        return "is";
    case TOK_KWD_IN:
        return "in";
    case TOK_ARROW:
        return "->";
    case TOK_OP_PLUS:
        return "+";
    case TOK_OP_PLUS_EQ:
        return "+=";
    case TOK_OP_MINUS:
        return "-";
    case TOK_OP_MINUS_EQ:
        return "-=";
    case TOK_OP_TIMES:
        return "*";
    case TOK_OP_TIMES_EQ:
        return "*=";
    case TOK_OP_DIV:
        return "/";
    case TOK_OP_DIV_EQ:
        return "/=";
    case TOK_OP_MOD:
        return "%";
    case TOK_OP_LT:
        return "<";
    case TOK_OP_GT:
        return ">";
    case TOK_OP_EQ:
        return "==";
    case TOK_OP_NEQ:
        return "!=";
    case TOK_OP_GEQ:
        return ">=";
    case TOK_OP_LEQ:
        return "<=";
    case TOK_KWD_AND:
        return "and";
    case TOK_KWD_OR:
        return "or";
    case TOK_KWD_NOT:
        return "not";
    case TOK_KWD_VEC:
        return "Vec";
    case TOK_KWD_DICT:
        return "Dict";
    case TOK_KWD_OPTION:
        return "Option";
    case TOK_KWD_SOME:
        return "Some";
    case TOK_KWD_NONE:
        return "None";
    case TOK_KWD_MATCH:
        return "match";
    case TOK_KWD_CASE:
        return "case";
    case TOK_KWD_FOREIGN:
        return "foreign";
    case TOK_UNDERSCORE:
        return "_";
    case TOK_ASSIGN:
        return "=";
    case TOK_DECLARE:
        return ":=";
    case TOK_DOT:
        return ".";
    case TOK_QMARK:
        return "?";
    case TOK_EMBEDDED_C:
        return "embedded C block";
    case TOK_EOF:
        return "end of file";
    case TOK_ERROR:
        return "invalid token";
    default:
        return "token";
    }
}

char *
jik_token_to_text(JikToken *t)
{
    CharBuffer *out = char_buffer_new("");
    char_buffer_append(out,
                       JIK_STRING_NCAT("  --> ",
                                       t->filepath,
                                       ":",
                                       size_t_to_string(t->lineno),
                                       ":",
                                       size_t_to_string(t->colno),
                                       "\n"));
    char  *str_lineno = size_t_to_string(t->lineno);
    size_t n          = strlen(str_lineno);
    for (size_t i = 0; i < n + 1; i++) {
        char_buffer_push(out, ' ');
    }
    char_buffer_append(out, "|\n");
    char_buffer_append(out, str_lineno);
    char_buffer_append(out, " |");
    char_buffer_append(out, JIK_STRING_NCAT(t->codeline, "\n"));
    for (size_t i = 0; i < n + 1; i++) {
        char_buffer_push(out, ' ');
    }
    char_buffer_append(out, "|");
    for (size_t i = 0; i < t->colno - 1; i++) {
        char_buffer_push(out, ' ');
    }
    char_buffer_push(out, '^');
    return out->data;
}
