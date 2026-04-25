#ifndef JIK_REGCHECK_H
#define JIK_REGCHECK_H

#include "ast.h"
#include "htab.h"

JIK_HTAB_DECLARE(TabJikAllocSpec, JikAllocSpec);
JIK_HTAB_DECLARE(TabInt, int);

typedef struct FuncInfo {
    bool             has_allocs;
    TabJikAllocSpec *spec_tab;
} FuncInfo;

void
jik_check_regions(JikNode *ast);

#endif
