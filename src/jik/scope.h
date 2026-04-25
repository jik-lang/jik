#ifndef JIK_SCOPE_H
#define JIK_SCOPE_H

#include "common.h"
#include "htab.h"
#include "token.h"
#include "types.h"
#include "vec.h"

JIK_HTAB_DECLARE(TabJikNode, struct JikNode *);

struct JikNode;

typedef struct JikScope {
    struct JikScope *parent;
    TabJikNode      *symbols;
} JikScope;

typedef struct JikNamespace {
    TabJikNode *symbols;
} JikNamespace;

JIK_HTAB_DECLARE(TabJikNamespace, JikNamespace);

JIK_VEC_DECLARE(VecJikScope, JikScope *);

void
jik_init_namespaces(void);
JikScope *
jik_scope_new(JikScope *parent);
bool
jik_scope_add_global_symbol(char *name, char *mod_alias, struct JikNode *nd);
bool
jik_scope_add_local_symbol(JikScope *ctx, char *name, struct JikNode *nd);
void
jik_scope_overwrite_local_symbol(JikScope *ctx, char *name, struct JikNode *nd);
struct JikNode *
jik_scope_get_global_symbol(char *name, char *mod_alias);
struct JikNode *
jik_scope_get_builtin_symbol(char *name);
bool
jik_scope_add_builtin_symbol(char *name, struct JikNode *nd);
struct JikNode *
jik_scope_get_symbol_in_block(JikScope *sc, char *name);
struct JikNode *
jik_scope_get_local_symbol(JikScope *sc, char *name);
struct JikNode *
jik_scope_get_symbol(JikScope *sc, char *name, char *mod_alias, char *home_mod_alias);
struct JikNode *
jik_scope_get_function(JikScope *sc, char *name, char *mod_alias, char *home_mod_alias);

#endif
