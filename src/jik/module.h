#ifndef JIK_MODULE_H
#define JIK_MODULE_H

#include <stdbool.h>

#include "common.h"
#include "token.h"
#include "vec.h"

typedef struct JikModule {
    char        *filepath;
    char        *alias;
    char        *module_id;
    VecJikToken *tokens;
    bool         is_leaf;
    TabBool     *usages;
    TabString   *imports;
} JikModule;

JIK_VEC_DECLARE(VecJikModule, JikModule);

#endif
