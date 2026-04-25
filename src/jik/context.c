#include "context.h"

void
jik_context_init(JikContext *ctx, JikConfig conf)
{
    ctx->conf        = conf;
    ctx->tokens      = VecJikToken_new_empty();
    ctx->leaves      = VecJikModule_new_empty();
    ctx->branches    = VecJikModule_new_empty();
    ctx->ast         = NULL;
    ctx->nodes       = VecJikNode_new_empty();
    ctx->args_type   = NULL;
    ctx->translation = NULL;
    ctx->math_used   = false;
}
