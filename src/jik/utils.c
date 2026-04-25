#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

// ---------------------------------------------------------------------------------------------------
//      IO
// ---------------------------------------------------------------------------------------------------

char *
jik_read_file(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char  *buffer = jik_alloc(file_size + 1);
    size_t bytes  = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes] = '\0';

    fclose(file);
    return buffer;
}

// ---------------------------------------------------------------------------------------------------
//      STRING
// ---------------------------------------------------------------------------------------------------

char *
jik_string_cat(const char *s1, const char *s2)
{
    char *res = jik_alloc(strlen(s1) + strlen(s2) + 1);
    strcpy(res, s1);
    strcat(res, s2);
    return res;
}

char *
jik_string_ncat(char *strings[])
{
    size_t req_len = 1;
    char **str     = strings;
    while (*str) {
        req_len += strlen(*str);
        str++;
    }
    char *res = jik_alloc(req_len);
    res[0]    = '\0';
    str       = strings;
    while (*str) {
        strcat(res, *str);
        str++;
    }
    return res;
}

char *
size_t_to_string(size_t num)
{
    int len = snprintf(NULL, 0, "%zu", num);
    if (len < 0) {
        return NULL;
    }
    char *s = jik_alloc((size_t)len + 1);
    if (!s) {
        return NULL;
    }
    int written = snprintf(s, (size_t)len + 1, "%zu", num);
    if (written != len) {
        return NULL;
    }
    return s;
}

char *
shell_quote_arg(const char *arg)
{
    if (arg == NULL) {
        return NULL;
    }

#if defined(_WIN32)
    size_t len = strlen(arg);
    char  *res = jik_alloc(len + 3);
    res[0]     = '"';
    memcpy(res + 1, arg, len);
    res[len + 1] = '"';
    res[len + 2] = '\0';
    return res;
#else
    size_t len = 2;
    for (const char *p = arg; *p != '\0'; p++) {
        len += *p == '\'' ? 4 : 1;
    }

    char *res = jik_alloc(len + 1);
    char *dst = res;
    *dst++    = '\'';
    for (const char *p = arg; *p != '\0'; p++) {
        if (*p == '\'') {
            memcpy(dst, "'\\''", 4);
            dst += 4;
        }
        else {
            *dst++ = *p;
        }
    }
    *dst++ = '\'';
    *dst   = '\0';
    return res;
#endif
}

bool
jik_identifier_has_reserved_prefix(const char *name)
{
    return name != NULL && strncmp(name, "jik_", 4) == 0;
}

// ---------------------------------------------------------------------------------------------------
//      OTHER
// ---------------------------------------------------------------------------------------------------

bool
system_has_tool(const char *tool_name, const char *probe_arg)
{
    char cmd[512];
    int  written;
    int  status;

    if (tool_name == NULL || *tool_name == '\0') {
        return false;
    }

    if (probe_arg == NULL || *probe_arg == '\0') {
        probe_arg = "--version";
    }

#if defined(_WIN32)
    written = snprintf(cmd, sizeof(cmd), "%s %s >NUL 2>&1", tool_name, probe_arg);
#else
    written = snprintf(cmd, sizeof(cmd), "%s %s >/dev/null 2>&1", tool_name, probe_arg);
#endif

    if (written < 0 || (size_t)written >= sizeof(cmd)) {
        return false;
    }

    status = system(cmd);
    return status == 0;
}
