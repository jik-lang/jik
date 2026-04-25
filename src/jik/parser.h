#ifndef JIK_PARSER_H
#define JIK_PARSER_H

#include "ast.h"
#include "common.h"
#include "context.h"
#include "diag.h"
#include "token.h"

typedef struct JikParser {
    JikContext  *ctx;
    size_t       pos;
    VecJikToken *tokens;
    size_t       num_tokens;
    JikNode     *ast;
    VecJikScope *contexts;
    VecJikNode  *nodes;
    JikNode     *parsed_function;
    JikNode     *parsed_struct;
    bool         parsing_struct;
    char        *parsing_struct_name;
    size_t       container_depth;
} JikParser;

void
jik_parser_init(JikParser *p, JikContext *ctx);
void
jik_parser_run(JikParser *p);
bool
is_base_literal(JikNode *nd);

#endif
