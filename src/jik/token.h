#ifndef JIK_TOKEN_H
#define JIK_TOKEN_H

#include "common.h"
#include "vec.h"

typedef struct JikToken JikToken;

#define TOKEN_NAMES                                                                                \
    X(TOK_NEWLINE)                                                                                 \
    X(TOK_INTEGER)                                                                                 \
    X(TOK_FLOAT)                                                                                   \
    X(TOK_STRING)                                                                                  \
    X(TOK_STRING_ML)                                                                               \
    X(TOK_CHAR)                                                                                    \
    X(TOK_ID)                                                                                      \
    X(TOK_LPAREN)                                                                                  \
    X(TOK_RPAREN)                                                                                  \
    X(TOK_LCURL)                                                                                   \
    X(TOK_RCURL)                                                                                   \
    X(TOK_LANG)                                                                                    \
    X(TOK_RANG)                                                                                    \
    X(TOK_COLON)                                                                                   \
    X(TOK_DOUBLE_COLON)                                                                            \
    X(TOK_COMMA)                                                                                   \
    X(TOK_KWD_FUNC)                                                                                \
    X(TOK_KWD_THROWS)                                                                              \
    X(TOK_KWD_MUST)                                                                                \
    X(TOK_KWD_TRY)                                                                                 \
    X(TOK_KWD_EXCEPT)                                                                              \
    X(TOK_KWD_END)                                                                                 \
    X(TOK_KWD_STRUCT)                                                                              \
    X(TOK_KWD_ENUM)                                                                                \
    X(TOK_KWD_VARIANT)                                                                             \
    X(TOK_KWD_IF)                                                                                  \
    X(TOK_KWD_ELIF)                                                                                \
    X(TOK_KWD_ELSE)                                                                                \
    X(TOK_KWD_WHILE)                                                                               \
    X(TOK_KWD_FOR)                                                                                 \
    X(TOK_KWD_BREAK)                                                                               \
    X(TOK_KWD_CONTINUE)                                                                            \
    X(TOK_KWD_RETURN)                                                                              \
    X(TOK_KWD_USE)                                                                                 \
    X(TOK_KWD_AS)                                                                                  \
    X(TOK_KWD_OF)                                                                                  \
    X(TOK_KWD_EXTERN)                                                                              \
    X(TOK_KWD_INIT)                                                                                \
    X(TOK_KWD_TRUE)                                                                                \
    X(TOK_KWD_FALSE)                                                                               \
    X(TOK_KWD_IS)                                                                                  \
    X(TOK_KWD_IN)                                                                                  \
    X(TOK_ARROW)                                                                                   \
    X(TOK_OP_PLUS)                                                                                 \
    X(TOK_OP_PLUS_EQ)                                                                              \
    X(TOK_OP_MINUS)                                                                                \
    X(TOK_OP_MINUS_EQ)                                                                             \
    X(TOK_OP_TIMES)                                                                                \
    X(TOK_OP_TIMES_EQ)                                                                             \
    X(TOK_OP_DIV)                                                                                  \
    X(TOK_OP_DIV_EQ)                                                                               \
    X(TOK_OP_MOD)                                                                                  \
    X(TOK_OP_LT)                                                                                   \
    X(TOK_OP_GT)                                                                                   \
    X(TOK_OP_EQ)                                                                                   \
    X(TOK_OP_NEQ)                                                                                  \
    X(TOK_OP_GEQ)                                                                                  \
    X(TOK_OP_LEQ)                                                                                  \
    X(TOK_KWD_AND)                                                                                 \
    X(TOK_KWD_OR)                                                                                  \
    X(TOK_KWD_NOT)                                                                                 \
    X(TOK_KWD_VEC)                                                                                 \
    X(TOK_KWD_DICT)                                                                                \
    X(TOK_KWD_OPTION)                                                                              \
    X(TOK_KWD_SOME)                                                                                \
    X(TOK_KWD_NONE)                                                                                \
    X(TOK_KWD_MATCH)                                                                               \
    X(TOK_KWD_CASE)                                                                                \
    X(TOK_KWD_FOREIGN)                                                                             \
    X(TOK_UNDERSCORE)                                                                              \
    X(TOK_ASSIGN)                                                                                  \
    X(TOK_DECLARE)                                                                                 \
    X(TOK_DOT)                                                                                     \
    X(TOK_QMARK)                                                                                   \
    X(TOK_EMBEDDED_C)                                                                              \
    X(TOK_EOF)                                                                                     \
    X(TOK_ERROR)

typedef enum JikTokenType {
#define X(name) name,
    TOKEN_NAMES
#undef X
        NUM_TOKENS
} JikTokenType;

struct JikToken {
    JikTokenType type;
    char        *lexeme;
    char         lexeme_char;
    size_t       lineno;
    size_t       colno;
    char        *filepath;
    char        *codeline;
    char        *mod_alias;
    TabBool     *used_aliases; // TODO: clumsy
};

JIK_VEC_DECLARE(VecJikToken, JikToken);

void
JikToken_print(JikToken *t);

void
VecJikToken_print(VecJikToken *tokens);

const char *
jik_token_type_name(JikTokenType type);

const char *
jik_token_type_pretty_name(JikTokenType type);

char *
jik_token_to_text(JikToken *t);

#endif
