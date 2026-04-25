#ifndef JIK_CODEGEN_H
#define JIK_CODEGEN_H

#include "ast.h"
#include "common.h"
#include "context.h"
#include "htab.h"
#include "writer.h"

JIK_HTAB_DECLARE(TabString, char *);

typedef struct JikCodeGenerator {
    JikContext *ctx;
    JikNode    *ast;
    VecJikNode *nodes;
    JikWriter   cw;
    TabString  *print_functions;
    TabString  *len_functions;
    TabString  *subscript_functions;
    TabBool    *declared_vec_types;
    TabBool    *declared_vec_struct_types;
    TabBool    *declared_dict_types;
    TabBool    *declared_dict_struct_types;
    TabBool    *declared_option_types;
    TabBool    *declared_option_struct_types;
    TabBool    *defined_vec_types;
    TabBool    *defined_dict_types;
    TabBool    *defined_option_types;
    JikNode    *arg_vec;
} JikCodeGenerator;

void
jik_codegen_init(JikCodeGenerator *cg, JikContext *ctx);
void
jik_codegen_run(JikCodeGenerator *cg);

#endif
