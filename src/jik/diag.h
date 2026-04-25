#ifndef JIK_DIAG_H
#define JIK_DIAG_H

#include <stdbool.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define JIK_NORETURN _Noreturn
#else
#define JIK_NORETURN
#endif

void
jik_diag_init(void);

JIK_NORETURN void
jik_diag_fatal_error(char *summary, char *details);

void
jik_diag_fatal_error_if(bool cond, char *summary, char *details);

void
jik_diag_warning(char *summary, char *details);

void
jik_diag_warning_if(bool cond, char *summary, char *details);

void
jik_diag_messages_print(void);

#endif
