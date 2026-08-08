#include "context.h"

#include <string.h>

JIK_VEC_DEFINE(VecJikBuildDirective, JikBuildDirective);

void
jik_context_init(JikContext *ctx, JikConfig conf)
{
    ctx->conf             = conf;
    ctx->tokens           = VecJikToken_new_empty();
    ctx->leaves           = VecJikModule_new_empty();
    ctx->branches         = VecJikModule_new_empty();
    ctx->modules          = VecJikModule_new_empty();
    ctx->module_ids       = TabString_new();
    ctx->module_paths     = TabString_new();
    ctx->build_directives = VecJikBuildDirective_new_empty();
    ctx->ast              = NULL;
    ctx->nodes            = VecJikNode_new_empty();
    ctx->args_type        = NULL;
    ctx->translation      = NULL;
}

JikModule *
jik_context_find_module(JikContext *ctx, char *module_id)
{
    for (size_t i = 0; i < VecJikModule_size(ctx->modules); i++) {
        JikModule *mod = VecJikModule_get_ptr(ctx->modules, i);
        if (strcmp(mod->module_id, module_id) == 0) {
            return mod;
        }
    }
    return NULL;
}
