#ifndef JIK_ALLOC_H
#define JIK_ALLOC_H

#include <stddef.h>

void
jik_arena_init(void);

char *
jik_alloc(size_t size);

char *
jik_realloc(char *p, size_t size, size_t old_size);

void
jik_arena_free(void);

#endif
