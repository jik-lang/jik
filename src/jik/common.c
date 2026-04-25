#include "common.h"

JIK_HTAB_DEFINE(TabBool, bool);
JIK_VEC_DEFINE(VecString, char *);

void
VecString_print(VecString *v)
{
    for (size_t i = 0; i < VecString_size(v); i++)
        printf("%s, ", VecString_get(v, i));
}
