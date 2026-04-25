#include "scope.h"

#include <assert.h>

#include "alloc.h"
#include "diag.h"
#include "types.h"
#include "utils.h"

JIK_HTAB_DEFINE(TabJikNode, struct JikNode *);
JIK_HTAB_DEFINE(TabJikNamespace, JikNamespace);
JIK_VEC_DEFINE(VecJikScope, JikScope *);

// Global namespace - stores function names, global identifiers and enums.
// Add struct and variant namespaces later.
static TabJikNamespace *GLOBAL_NSP;
static TabJikNode      *BUILTINS_NSP;

void
jik_init_namespaces(void)
{
    GLOBAL_NSP   = TabJikNamespace_new();
    BUILTINS_NSP = TabJikNode_new();
}

bool
jik_scope_add_global_symbol(char *name, char *mod_alias, struct JikNode *nd)
{
    JikNamespace *ns = TabJikNamespace_get(GLOBAL_NSP, mod_alias);
    if (!ns) {
        JikNamespace n = (JikNamespace){.symbols = TabJikNode_new()};
        TabJikNode_set(n.symbols, name, nd);
        TabJikNamespace_set(GLOBAL_NSP, mod_alias, n);
    }
    else {
        struct JikNode **n1 = TabJikNode_get(ns->symbols, name);
        if (n1) {
            return false;
        }
        TabJikNode_set(ns->symbols, name, nd);
    }
    // printf("added global: %s::%s\n", mod_alias, name);
    return true;
}

struct JikNode *
jik_scope_get_global_symbol(char *name, char *mod_alias)
{
    JikNamespace *ns = TabJikNamespace_get(GLOBAL_NSP, mod_alias);
    if (!ns) {
        return NULL;
    }
    struct JikNode **s = TabJikNode_get(ns->symbols, name);
    if (!s) {
        return NULL;
    }
    return *s;
}

bool
jik_scope_add_local_symbol(JikScope *ctx, char *name, struct JikNode *nd)
{
    struct JikNode **s = TabJikNode_get(ctx->symbols, name);
    if (s) {
        return false;
    }
    TabJikNode_set(ctx->symbols, name, nd);
    return true;
}

void
jik_scope_overwrite_local_symbol(JikScope *ctx, char *name, struct JikNode *nd)
{
    TabJikNode_set(ctx->symbols, name, nd);
}

struct JikNode *
jik_scope_get_builtin_symbol(char *name)
{
    struct JikNode **s = TabJikNode_get(BUILTINS_NSP, name);
    if (!s) {
        return NULL;
    }
    return *s;
}

bool
jik_scope_add_builtin_symbol(char *name, struct JikNode *nd)
{
    struct JikNode **s = TabJikNode_get(BUILTINS_NSP, name);
    if (s) {
        return false;
    }
    TabJikNode_set(BUILTINS_NSP, name, nd);
    return true;
}

JikScope *
jik_scope_new(JikScope *parent)
{
    JikScope *sc = (JikScope *)jik_alloc(sizeof(JikScope));
    sc->parent   = parent;
    sc->symbols  = TabJikNode_new();
    return sc;
}

struct JikNode *
jik_scope_get_symbol(JikScope *sc, char *name, char *mod_alias, char *home_mod_alias)
{
    if (mod_alias) {
        return jik_scope_get_global_symbol(name, mod_alias);
    }
    struct JikNode **s = TabJikNode_get(sc->symbols, name);
    if (s != NULL) {
        return *s;
    }
    sc = sc->parent;
    while (sc != NULL) {
        s = TabJikNode_get(sc->symbols, name);
        if (s != NULL)
            return *s;
        sc = sc->parent;
    }
    return jik_scope_get_global_symbol(name, home_mod_alias);
}

struct JikNode *
jik_scope_get_local_symbol(JikScope *sc, char *name)
{
    assert(sc->symbols);
    struct JikNode **s = TabJikNode_get(sc->symbols, name);
    if (s != NULL) {
        return *s;
    }
    sc = sc->parent;
    while (sc != NULL) {
        s = TabJikNode_get(sc->symbols, name);
        if (s != NULL)
            return *s;
        sc = sc->parent;
    }
    return NULL;
}

struct JikNode *
jik_scope_get_symbol_in_block(JikScope *sc, char *name)
{
    assert(sc->symbols);
    struct JikNode **s = TabJikNode_get(sc->symbols, name);
    if (s != NULL) {
        return *s;
    }
    return NULL;
}

struct JikNode *
jik_scope_get_function(JikScope *sc, char *name, char *mod_alias, char *home_mod_alias)
{
    struct JikNode *s = jik_scope_get_symbol(sc, name, mod_alias, home_mod_alias);
    if (s) {
        return s;
    }
    return jik_scope_get_builtin_symbol(name);
}
