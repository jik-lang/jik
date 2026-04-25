#ifndef JIK_SEMANTIC_H
#define JIK_SEMANTIC_H

#include <stdbool.h>

#include "ast.h"
#include "common.h"
#include "context.h"

typedef struct JikSemanticAnalyzer {
    JikContext *ctx;
    VecJikNode *nodes;
    VecJikNode *uninferred;
    bool        main_defined;
    TabJikNode *function_nodes;
    TabJikNode *struct_nodes;
    TabJikNode *variant_nodes;
    TabJikNode *extern_function_nodes;
    TabJikNode *extern_struct_nodes;
    TabJikNode *enum_nodes;
    TabBool    *funs_with_returns;
    TabJikNode *allocated_symbols;
    JikNode    *current_struct;
    bool        needs_recollect;
} JikSemanticAnalyzer;

void
jik_semantic_init(JikSemanticAnalyzer *sa, JikContext *ctx);
void
jik_semantic_run(JikSemanticAnalyzer *sa);
JikType *
jik_semantic_resolve_type(JikNode *nd);
bool
is_main_function(JikNode *nd);
void
jik_ensure_valid_variant_tag(JikNode *nd);
void
jik_semantic_ensure_module_used(JikNode *nd);

#endif
