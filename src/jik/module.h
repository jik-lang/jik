#ifndef JIK_MODULE_H
#define JIK_MODULE_H

#include <stdbool.h>

#include "common.h"
#include "token.h"
#include "vec.h"

typedef struct JikModule {
    char        *filepath;
    char        *alias;
    VecJikToken *tokens;
    bool         is_leaf;
    TabBool     *usages;
    TabBool     *used_aliases;
} JikModule;

JIK_VEC_DECLARE(VecJikModule, JikModule);

#endif
