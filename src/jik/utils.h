#ifndef JIK_UTILS_H
#define JIK_UTILS_H

#include <stdbool.h>
#include <stddef.h>

char *
jik_read_file(const char *path);
char *
jik_string_cat(const char *s1, const char *s2);
char *
jik_string_ncat(char *strings[]);
char *
size_t_to_string(size_t num);
char *
shell_quote_arg(const char *arg);
bool
jik_identifier_has_reserved_prefix(const char *name);
bool
system_has_tool(const char *tool_name, const char *probe_arg);

// TODO: given this, we may not need jik_string_cat at all
#define JIK_STRING_NCAT(...) jik_string_ncat((char *[]){__VA_ARGS__, NULL})

#endif
