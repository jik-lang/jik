#ifndef JIK_COMMON_H
#define JIK_COMMON_H

#include <stdbool.h>

#include "htab.h"
#include "vec.h"

JIK_HTAB_DECLARE(TabBool, bool);
JIK_VEC_DECLARE(VecString, char *);

void
VecString_print(VecString *v);

#endif
