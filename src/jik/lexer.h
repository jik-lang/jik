#ifndef JIK_LEXER_H
#define JIK_LEXER_H

#include "common.h"
#include "context.h"
#include "token.h"

typedef struct JikLexer {
    JikContext *ctx;
    char       *code;
    char       *filepath;
    size_t      pos;
    size_t      lineno;
    size_t      colno;
    size_t      line_start;
    size_t      mark_pos;
    size_t      mark_col;
    VecString  *lines;
} JikLexer;

void
jik_lexer_init(JikLexer *lex, JikContext *ctx, char *code, char *filepath);
VecJikToken *
jik_lexer_tokenize(JikLexer *lex);

#endif
