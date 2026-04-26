// Ensure POSIX APIs (strdup) are declared when compiling with -std=c11.
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "lexer.h"

#include <ctype.h>
#include <string.h>

#include "alloc.h"
#include "charbuf.h"
#include "context.h"
#include "diag.h"

static VecString *
jik_lexer_split_lines(const char *code)
{
    VecString *v = VecString_new_empty();

    const char *start = code;
    char       *next  = strstr(start, "\n");
    char       *line;
    while (next) {
        size_t len = next - start;
        line       = jik_alloc(len + 1);
        strncpy(line, start, len);
        line[len] = '\0';
        // Accept Windows CRLF by stripping a trailing '\r'.
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        VecString_push(v, line);
        start = next + 1;
        next  = strstr(start, "\n");
        if (!next) {
            char  *tail = strdup(start);
            size_t tlen = strlen(tail);
            if (tlen > 0 && tail[tlen - 1] == '\r') {
                tail[tlen - 1] = '\0';
            }
            VecString_push(v, tail);
        }
    }
    return v;
}

void
jik_lexer_init(JikLexer *lex, JikContext *ctx, char *code, char *filepath)
{
    lex->ctx        = ctx;
    lex->code       = code;
    lex->filepath   = filepath;
    lex->pos        = 0;
    lex->lineno     = 1;
    lex->colno      = 1;
    lex->line_start = 1;
    lex->mark_pos   = 0;
    lex->mark_col   = 0;
    lex->lines      = jik_lexer_split_lines(code);
}

static char
jik_lexer_current_char(JikLexer *lex)
{
    return lex->code[lex->pos];
}

static char *
jik_lexer_current_ptr(JikLexer *lex)
{
    return &lex->code[lex->pos];
}

static char
jik_lexer_peek_char(JikLexer *lex, int offset)
{
    /* Example only: replace with your actual implementation. */
    return lex->code[lex->pos + offset];
}

static void
jik_lexer_mark_pos(JikLexer *lex)
{
    lex->mark_pos = lex->pos;
}

static void
jik_lexer_mark_col(JikLexer *lex)
{
    lex->mark_col = lex->colno;
}

static bool
jik_lexer_reached_EOF(JikLexer *lex)
{
    return lex->code[lex->pos] == '\0';
}

static void
jik_lexer_advance(JikLexer *lex)
{
    lex->pos++;
    lex->colno++;
}

static void
jik_lexer_advance_by(JikLexer *lex, size_t n)
{
    lex->pos += n;
    lex->colno += n;
}

static char *
jik_lexer_get_lexeme(JikLexer *lex, size_t start, size_t end)
{
    size_t len    = end - start;
    char  *lexeme = jik_alloc(len + 1);
    strncpy(lexeme, lex->code + start, len);
    lexeme[len] = '\0';
    return lexeme;
}

static void
jik_lexer_advance_newline(JikLexer *lex)
{
    lex->pos++;
    lex->lineno++;
    lex->colno      = 1;
    lex->line_start = lex->pos;
}

static JikToken
jik_lexer_make_token(JikLexer *lex, JikTokenType type, char *lexeme)
{
    return (JikToken){.type     = type,
                      .lexeme   = lexeme,
                      .lineno   = lex->lineno,
                      .colno    = lex->colno,
                      .filepath = lex->filepath,
                      .codeline = VecString_get(lex->lines, lex->lineno - 1)};
}

static JikToken
jik_lexer_make_token_from_mark(JikLexer *lex, JikTokenType type)
{
    return (JikToken){.type     = type,
                      .lexeme   = jik_lexer_get_lexeme(lex, lex->mark_pos, lex->pos),
                      .lineno   = lex->lineno,
                      .colno    = lex->mark_col,
                      .filepath = lex->filepath,
                      .codeline = VecString_get(lex->lines, lex->lineno - 1)};
}

static JikToken
jik_lexer_make_token_from_lexeme(JikLexer *lex, JikTokenType type, char *lexeme)
{
    return (JikToken){.type     = type,
                      .lexeme   = lexeme,
                      .lineno   = lex->lineno,
                      .colno    = lex->mark_col,
                      .filepath = lex->filepath,
                      .codeline = VecString_get(lex->lines, lex->lineno - 1)};
}

static int
jik_lexer_hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static char
jik_lexer_decode_escape(JikLexer *lex, JikToken *tok, bool allow_nul)
{
    char ch = jik_lexer_current_char(lex);
    switch (ch) {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        if (!allow_nul) {
            jik_diag_fatal_error("syntax error: \\0 is not supported in string literals",
                                 jik_token_to_text(tok));
        }
        return '\0';
    case 'x': {
        int hi = jik_lexer_hex_digit_value(jik_lexer_peek_char(lex, 1));
        int lo = jik_lexer_hex_digit_value(jik_lexer_peek_char(lex, 2));
        jik_diag_fatal_error_if(hi < 0 || lo < 0,
                                "syntax error: invalid hex escape sequence",
                                jik_token_to_text(tok));
        char value = (char)((hi << 4) | lo);
        if (!allow_nul && value == '\0') {
            jik_diag_fatal_error("syntax error: \\x00 is not supported in string literals",
                                 jik_token_to_text(tok));
        }
        jik_lexer_advance_by(lex, 2);
        return value;
    }
    case '\\':
        return '\\';
    case '\'':
        return '\'';
    case '\"':
        return '\"';
    default:
        jik_diag_fatal_error("syntax error: unknown escape sequence", jik_token_to_text(tok));
        return '\0';
    }
}

static JikToken
jik_lexer_lex_number(JikLexer *lex)
{
    jik_lexer_mark_pos(lex);
    jik_lexer_mark_col(lex);

    // integer part (at least one digit assumed by caller)
    while (isdigit(jik_lexer_current_char(lex))) {
        jik_lexer_advance(lex);
    }

    int is_float = false;

    // fractional part: only if '.' followed by a digit
    if (jik_lexer_current_char(lex) == '.' && isdigit(jik_lexer_peek_char(lex, 1))) {
        is_float = true;
        jik_lexer_advance(lex); // consume '.'
        while (isdigit(jik_lexer_current_char(lex))) {
            jik_lexer_advance(lex);
        }
    }

    // exponent part: [eE][+-]?[0-9]+
    {
        char c = jik_lexer_current_char(lex);
        if (c == 'e' || c == 'E') {
            char p1 = jik_lexer_peek_char(lex, 1);
            char p2 = jik_lexer_peek_char(lex, 2);

            int has_digits = isdigit(p1) || ((p1 == '+' || p1 == '-') && isdigit(p2));

            if (has_digits) {
                is_float = true;
                jik_lexer_advance(lex); // consume e/E
                if (jik_lexer_current_char(lex) == '+' || jik_lexer_current_char(lex) == '-') {
                    jik_lexer_advance(lex); // consume sign
                }
                while (isdigit(jik_lexer_current_char(lex))) {
                    jik_lexer_advance(lex);
                }
            }
            // else: do not consume 'e'/'E' at all; leave it for next token
        }
    }

    return jik_lexer_make_token_from_mark(lex, is_float ? TOK_FLOAT : TOK_INTEGER);
}

static JikToken
jik_lexer_lex_string(JikLexer *lex)
{
    jik_lexer_advance(lex);
    jik_lexer_mark_pos(lex);
    jik_lexer_mark_col(lex);
    JikToken    tok = jik_lexer_make_token_from_mark(lex, TOK_ERROR);
    CharBuffer *buf = char_buffer_new("");
    while (jik_lexer_current_char(lex) != '\"') {
        jik_diag_fatal_error_if(jik_lexer_reached_EOF(lex),
                                "syntax error: unterminated string literal",
                                jik_token_to_text(&tok));
        jik_diag_fatal_error_if(jik_lexer_current_char(lex) == '\n',
                                "syntax error: newline not allowed in string literal",
                                jik_token_to_text(&tok));
        if (jik_lexer_current_char(lex) == '\\') {
            jik_lexer_advance(lex);
            jik_diag_fatal_error_if(jik_lexer_reached_EOF(lex),
                                    "syntax error: unterminated string literal",
                                    jik_token_to_text(&tok));
            char_buffer_push(buf, jik_lexer_decode_escape(lex, &tok, false));
            jik_lexer_advance(lex);
            continue;
        }
        char_buffer_push(buf, jik_lexer_current_char(lex));
        jik_lexer_advance(lex);
    }
    return jik_lexer_make_token_from_lexeme(lex, TOK_STRING, char_buffer_data(buf));
}

static JikToken
jik_lexer_lex_multiline_string(JikLexer *lex)
{
    int start_colno = lex->colno;
    (void)start_colno;
    jik_lexer_advance_by(lex, 3);
    jik_lexer_mark_pos(lex);
    jik_lexer_mark_col(lex);
    JikToken err = jik_lexer_make_token_from_mark(lex, TOK_ERROR);

    while (!jik_lexer_reached_EOF(lex)) {
        if (strncmp(jik_lexer_current_ptr(lex), "\"\"\"", 3) == 0) {
            break;
        }
        char ch = jik_lexer_current_char(lex);
        if (ch == '\n') {
            jik_lexer_advance_newline(lex);
            continue;
        }
        if (ch == '\r') {
            jik_lexer_advance(lex);
            if (jik_lexer_current_char(lex) == '\n') {
                jik_lexer_advance_newline(lex);
            }
            continue;
        }
        jik_lexer_advance(lex);
    }
    if (jik_lexer_reached_EOF(lex)) {
        jik_diag_fatal_error("syntax error: unterminated multiline string literal",
                             jik_token_to_text(&err));
    }
    return jik_lexer_make_token_from_mark(lex, TOK_STRING_ML);
}

static JikToken
jik_lexer_lex_char(JikLexer *lex)
{
    jik_lexer_advance(lex);
    JikToken tok = jik_lexer_make_token(lex, TOK_CHAR, NULL);
    char     ch  = jik_lexer_current_char(lex);
    if (ch == '\\') {
        jik_lexer_advance(lex);
        ch = jik_lexer_decode_escape(lex, &tok, true);
    }
    jik_lexer_advance(lex);

    if (jik_lexer_current_char(lex) != '\'') {
        jik_diag_fatal_error("syntax error", jik_token_to_text(&tok));
    }

    jik_lexer_advance(lex);
    tok.lexeme_char = ch;
    return tok;
}

static const char *
jik_next_line(const char *p)
{
    while (*p && *p != '\n')
        p++;
    if (*p == '\n')
        p++;
    return p;
}

static JikToken
jik_lexer_lex_cblock(JikLexer *lex)
{
    const char *code = lex->code;
    jik_lexer_advance_by(lex, (int)strlen("@embed"));
    jik_diag_fatal_error_if(code[lex->pos] != '{', "syntax error", "expected '{' after @embed");
    lex->pos++;
    size_t delim_start = (size_t)lex->pos;

    char c = code[lex->pos];
    jik_diag_fatal_error_if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'),
                            "syntax error",
                            "invalid embed delimiter");

    lex->pos++;
    while (true) {
        c = code[lex->pos];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '_') {
            lex->pos++;
        }
        else {
            break;
        }
    }

    size_t delim_len = (size_t)lex->pos - delim_start;
    jik_diag_fatal_error_if(
        code[lex->pos] != '}', "syntax error", "expected '}' after embed delimiter");
    lex->pos++;
    jik_diag_fatal_error_if(
        code[lex->pos] != '\n', "syntax error", "expected newline after @embed{...}");
    lex->pos++;
    lex->lineno++;
    lex->colno = 0;

    const char *c_start = code + lex->pos;
    jik_lexer_mark_pos(lex);
    jik_lexer_mark_col(lex);
    const char *delim   = code + delim_start;
    const char *p       = c_start;
    const char *end_ptr = NULL;

    while (*p) {
        const char *line = p;
        if (strncmp(line, delim, delim_len) == 0) {
            const char *q = line + delim_len;
            while (*q == ' ' || *q == '\t' || *q == '\r')
                q++;
            if (*q == '\n' || *q == '\0') {
                end_ptr = line; // embedded C ends before this line
                break;
            }
        }
        p = jik_next_line(p);
    }

    jik_diag_fatal_error_if(end_ptr == NULL, "syntax error", "embed terminator not found");

    lex->pos   = (int)(end_ptr - code);
    JikToken t = jik_lexer_make_token_from_mark(lex, TOK_EMBEDDED_C);
    lex->pos += (int)delim_len;

    while (code[lex->pos] == ' ' || code[lex->pos] == '\t' || code[lex->pos] == '\r') {
        lex->pos++;
    }

    if (code[lex->pos] == '\n') {
        lex->pos++;
        lex->lineno++;
        lex->colno = 0;
    }
    size_t num_newlines = 0;
    for (const char *s = c_start; s < end_ptr; s++) {
        if (*s == '\n')
            num_newlines++;
    }
    lex->lineno += num_newlines;
    return t;
}

static JikTokenType
match_keyword_or_id(const char *kwd)
{
    if (strcmp(kwd, "func") == 0)
        return TOK_KWD_FUNC;
    if (strcmp(kwd, "throws") == 0)
        return TOK_KWD_THROWS;
    if (strcmp(kwd, "must") == 0)
        return TOK_KWD_MUST;
    if (strcmp(kwd, "try") == 0)
        return TOK_KWD_TRY;
    if (strcmp(kwd, "except") == 0)
        return TOK_KWD_EXCEPT;
    if (strcmp(kwd, "end") == 0)
        return TOK_KWD_END;
    if (strcmp(kwd, "struct") == 0)
        return TOK_KWD_STRUCT;
    if (strcmp(kwd, "enum") == 0)
        return TOK_KWD_ENUM;
    if (strcmp(kwd, "variant") == 0)
        return TOK_KWD_VARIANT;
    if (strcmp(kwd, "if") == 0)
        return TOK_KWD_IF;
    if (strcmp(kwd, "elif") == 0)
        return TOK_KWD_ELIF;
    if (strcmp(kwd, "else") == 0)
        return TOK_KWD_ELSE;
    if (strcmp(kwd, "while") == 0)
        return TOK_KWD_WHILE;
    if (strcmp(kwd, "for") == 0)
        return TOK_KWD_FOR;
    if (strcmp(kwd, "break") == 0)
        return TOK_KWD_BREAK;
    if (strcmp(kwd, "continue") == 0)
        return TOK_KWD_CONTINUE;
    if (strcmp(kwd, "return") == 0)
        return TOK_KWD_RETURN;
    if (strcmp(kwd, "use") == 0)
        return TOK_KWD_USE;
    if (strcmp(kwd, "as") == 0)
        return TOK_KWD_AS;
    if (strcmp(kwd, "extern") == 0)
        return TOK_KWD_EXTERN;
    if (strcmp(kwd, "true") == 0)
        return TOK_KWD_TRUE;
    if (strcmp(kwd, "false") == 0)
        return TOK_KWD_FALSE;
    if (strcmp(kwd, "of") == 0)
        return TOK_KWD_OF;
    if (strcmp(kwd, "and") == 0)
        return TOK_KWD_AND;
    if (strcmp(kwd, "or") == 0)
        return TOK_KWD_OR;
    if (strcmp(kwd, "not") == 0)
        return TOK_KWD_NOT;
    if (strcmp(kwd, "is") == 0)
        return TOK_KWD_IS;
    if (strcmp(kwd, "Vec") == 0)
        return TOK_KWD_VEC;
    if (strcmp(kwd, "Dict") == 0)
        return TOK_KWD_DICT;
    if (strcmp(kwd, "Option") == 0)
        return TOK_KWD_OPTION;
    if (strcmp(kwd, "Some") == 0)
        return TOK_KWD_SOME;
    if (strcmp(kwd, "None") == 0)
        return TOK_KWD_NONE;
    if (strcmp(kwd, "in") == 0)
        return TOK_KWD_IN;
    if (strcmp(kwd, "match") == 0)
        return TOK_KWD_MATCH;
    if (strcmp(kwd, "case") == 0)
        return TOK_KWD_CASE;
    if (strcmp(kwd, "foreign") == 0)
        return TOK_KWD_FOREIGN;
    if (strcmp(kwd, "_") == 0)
        return TOK_UNDERSCORE;
    return TOK_ID;
}

static JikTokenType
match_char(char ch)
{
    switch (ch) {
    case '(': {
        return TOK_LPAREN;
    }
    case ')': {
        return TOK_RPAREN;
    }
    case '{': {
        return TOK_LCURL;
    }
    case '}': {
        return TOK_RCURL;
    }
    case '[': {
        return TOK_LANG;
    }
    case ']': {
        return TOK_RANG;
    }
    case '+': {
        return TOK_OP_PLUS;
    }
    case '-': {
        return TOK_OP_MINUS;
    }
    case '*': {
        return TOK_OP_TIMES;
    }
    case '/': {
        return TOK_OP_DIV;
    }
    case '%': {
        return TOK_OP_MOD;
    }
    case ':': {
        return TOK_COLON;
    }
    case ',': {
        return TOK_COMMA;
    }
    case '<': {
        return TOK_OP_LT;
    }
    case '>': {
        return TOK_OP_GT;
    }
    case '=': {
        return TOK_ASSIGN;
    }
    case '.': {
        return TOK_DOT;
    }
    case '?': {
        return TOK_QMARK;
    }
    default:
        return TOK_ERROR;
    }
}

static JikToken
jik_lexer_lex_kwd_id(JikLexer *lex)
{
    jik_lexer_mark_pos(lex);
    jik_lexer_mark_col(lex);
    while (isalnum(jik_lexer_current_char(lex)) || jik_lexer_current_char(lex) == '_') {
        jik_lexer_advance(lex);
    }
    char        *lexeme = jik_lexer_get_lexeme(lex, lex->mark_pos, lex->pos);
    JikTokenType t      = match_keyword_or_id(lexeme);
    return jik_lexer_make_token_from_mark(lex, t);
}

static void
jik_lexer_skip_whitespace(JikLexer *lex)
{
    char ch = jik_lexer_current_char(lex);
    // Treat '\r' as whitespace to support CRLF sources.
    while (ch == ' ' || ch == '\t' || ch == '\r') {
        jik_lexer_advance(lex);
        ch = jik_lexer_current_char(lex);
    }
}

VecJikToken *
jik_lexer_tokenize(JikLexer *lex)
{
    VecJikToken *tokens = VecJikToken_new_empty();
    char         ch;
    JikToken     tok;
    JikTokenType tok_type;
    while (!jik_lexer_reached_EOF(lex)) {
        jik_lexer_skip_whitespace(lex);
        if (jik_lexer_reached_EOF(lex)) {
            break;
        }
        ch = jik_lexer_current_char(lex);
        if (ch == '\n') {
            tok = jik_lexer_make_token(lex, TOK_NEWLINE, "");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_newline(lex);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "//", 2) == 0) {
            ch = jik_lexer_current_char(lex);
            while (ch != '\n' && !jik_lexer_reached_EOF(lex)) {
                jik_lexer_advance(lex);
                ch = jik_lexer_current_char(lex);
            }
            continue;
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "@embed", (int)strlen("@embed")) == 0) {
            tok = jik_lexer_lex_cblock(lex);
            VecJikToken_push(tokens, tok);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "@end", (int)strlen("@end")) == 0) {
            tok = jik_lexer_make_token(lex, TOK_ERROR, "");
            jik_diag_fatal_error("syntax error", jik_token_to_text(&tok));
        }
        // TODO: refactor into two-char token helper
        else if (strncmp(jik_lexer_current_ptr(lex), "::", 2) == 0) {
            jik_lexer_advance_by(lex, 2);
            tok = jik_lexer_make_token(lex, TOK_DOUBLE_COLON, "::");
            VecJikToken_push(tokens, tok);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "->", 2) == 0) {
            jik_lexer_advance_by(lex, 2);
            tok = jik_lexer_make_token(lex, TOK_ARROW, "->");
            VecJikToken_push(tokens, tok);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "==", 2) == 0) {
            jik_lexer_advance_by(lex, 2);
            tok = jik_lexer_make_token(lex, TOK_OP_EQ, "==");
            VecJikToken_push(tokens, tok);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "!=", 2) == 0) {
            jik_lexer_advance_by(lex, 2);
            tok = jik_lexer_make_token(lex, TOK_OP_NEQ, "!=");
            VecJikToken_push(tokens, tok);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), ">=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_OP_GEQ, ">=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "<=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_OP_LEQ, "<=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "+=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_OP_PLUS_EQ, "+=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "-=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_OP_MINUS_EQ, "-=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "*=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_OP_TIMES_EQ, "*=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "/=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_OP_DIV_EQ, "/=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), ":=", 2) == 0) {
            tok = jik_lexer_make_token(lex, TOK_DECLARE, ":=");
            VecJikToken_push(tokens, tok);
            jik_lexer_advance_by(lex, 2);
        }
        else if ((tok_type = match_char(ch)) != TOK_ERROR) {
            tok = jik_lexer_make_token(
                lex, tok_type, jik_lexer_get_lexeme(lex, lex->pos, lex->pos + 1));
            VecJikToken_push(tokens, tok);
            jik_lexer_advance(lex);
        }
        else if (strncmp(jik_lexer_current_ptr(lex), "\"\"\"", 3) == 0) {
            tok = jik_lexer_lex_multiline_string(lex);
            jik_lexer_advance_by(lex, 3);
            VecJikToken_push(tokens, tok);
        }
        else if (ch == '\"') {
            tok = jik_lexer_lex_string(lex);
            VecJikToken_push(tokens, tok);
            jik_lexer_advance(lex);
        }
        else if (ch == '@') {
            tok = jik_lexer_make_token(lex, TOK_ERROR, "");
            jik_diag_fatal_error("syntax error", jik_token_to_text(&tok));
        }
        else if (ch == '\'') {
            tok = jik_lexer_lex_char(lex);
            VecJikToken_push(tokens, tok);
        }
        else if (isdigit(ch)) {
            tok = jik_lexer_lex_number(lex);
            VecJikToken_push(tokens, tok);
        }
        else if (isalpha(ch) || ch == '_') {
            tok = jik_lexer_lex_kwd_id(lex);
            VecJikToken_push(tokens, tok);
        }
        else {
            tok = jik_lexer_make_token(lex, TOK_ERROR, "");
            jik_diag_fatal_error("syntax error", jik_token_to_text(&tok));
        }
    }
    return tokens;
}
