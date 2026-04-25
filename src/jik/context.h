#ifndef JIK_CONTEXT_H
#define JIK_CONTEXT_H

#include "ast.h"
#include "config.h"
#include "module.h"
#include "token.h"
#include "types.h"

typedef struct JikContext {
    JikConfig conf;

    VecJikToken  *tokens;
    VecJikModule *leaves;
    VecJikModule *branches;

    JikNode    *ast;
    VecJikNode *nodes;

    JikType *args_type;

    const char *translation;

    bool math_used;
} JikContext;

void
jik_context_init(JikContext *ctx, JikConfig conf);

#endif
