#ifndef JIK_CONTEXT_H
#define JIK_CONTEXT_H

#include "ast.h"
#include "config.h"
#include "module.h"
#include "token.h"
#include "types.h"

typedef enum JikBuildPlatform {
    JIK_BUILD_PLATFORM_ALL,
    JIK_BUILD_PLATFORM_WINDOWS,
    JIK_BUILD_PLATFORM_LINUX,
} JikBuildPlatform;

typedef enum JikBuildProfile {
    JIK_BUILD_PROFILE_UNKNOWN,
    JIK_BUILD_PROFILE_ALL,
    JIK_BUILD_PROFILE_WINDOWS_X64_MINGW,
    JIK_BUILD_PROFILE_LINUX_X64_GNU,
} JikBuildProfile;

typedef enum JikBuildDirectiveKind {
    JIK_BUILD_INCLUDE_DIR,
    JIK_BUILD_LIB_DIR,
    JIK_BUILD_LINK,
    JIK_BUILD_COPY,
} JikBuildDirectiveKind;

typedef struct JikBuildDirective {
    JikBuildDirectiveKind kind;
    JikBuildPlatform      platform;
    JikBuildProfile       profile;
    VecString            *args;
    JikToken             *token;
} JikBuildDirective;

JIK_VEC_DECLARE(VecJikBuildDirective, JikBuildDirective);

typedef struct JikContext {
    JikConfig conf;

    VecJikToken          *tokens;
    VecJikModule         *leaves;
    VecJikModule         *branches;
    VecJikModule         *modules;
    TabString            *module_ids;
    TabString            *module_paths;
    VecJikBuildDirective *build_directives;

    JikNode    *ast;
    VecJikNode *nodes;

    JikType *args_type;

    const char *translation;
} JikContext;

void
jik_context_init(JikContext *ctx, JikConfig conf);

JikModule *
jik_context_find_module(JikContext *ctx, char *module_id);

#endif
