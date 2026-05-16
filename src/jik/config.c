// Ensure POSIX APIs (readlink) are declared when compiling with -std=c11.
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "command.h"
#include "diag.h"
#include "utils.h"

#if defined(_WIN32)
#include <windows.h>
#define PATH_SEP "\\"
#elif defined(__linux__)
#include <unistd.h>
#define PATH_SEP "/"
#else
#error "Unsupported platform"
#endif

static char *
get_program_path(void)
{
    size_t cap = 256;
    for (;;) {
        char *path = malloc(cap);
        if (!path) {
            return NULL;
        }

#if defined(_WIN32)
        SetLastError(0);
        DWORD len = GetModuleFileNameA(NULL, path, (DWORD)cap);
        if (len == 0) {
            free(path);
            return NULL;
        }
        if (len < cap - 1 || (len == cap - 1 && GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
            path[len] = '\0';
            return path;
        }
#elif defined(__linux__)
        ssize_t len = readlink("/proc/self/exe", path, cap - 1);
        if (len == -1) {
            free(path);
            return NULL;
        }
        if ((size_t)len < cap - 1) {
            path[len] = '\0';
            return path;
        }
#endif

        free(path);
        if (cap > ((size_t)-1) / 2) {
            return NULL;
        }
        cap *= 2;
    }
}

static char *
get_program_dir(void)
{
    char *path = get_program_path();
    if (!path) {
        return NULL;
    }

    char *slash = strrchr(path, PATH_SEP[0]);
    if (!slash) {
        free(path);
        return NULL;
    }
    *slash = '\0';

    return path;
}

static bool
is_option(char *str)
{
    return strncmp(str, "--", 2) == 0 && strlen(str) > 2;
}

static char *
make_target_name(char *input_file)
{
    char *dot = strrchr(input_file, '.');
    if (dot && dot != input_file) {
        size_t base_len = (size_t)(dot - input_file);
        char  *tgt_name = jik_alloc(base_len + 1);
        strncpy(tgt_name, input_file, base_len);
        tgt_name[base_len] = '\0';
        return tgt_name;
    }
    return input_file;
}

JikConfig
jik_config_make(int argc, char **argv)
{
    JikConfig conf = {0};
    if (argc == 1) {
        conf.command = "help_general";
        return conf;
    }
    char      *command = NULL;
    JikCommand cmd;
    size_t     argc_sz = (size_t)argc;
    for (size_t i = 1; i < argc_sz; i++) {
        if (is_option(argv[i])) {
            jik_diag_fatal_error_if(!command, "input error", "commands must precede options");
            JikOption opt = jik_get_option(cmd, argv[i]);
            jik_diag_fatal_error_if(i + opt.num_args >= argc_sz,
                                    "input error",
                                    JIK_STRING_NCAT("missing arguments for option: ", argv[i]));
            if (strcmp(argv[i], "--cc") == 0) {
                conf.cc = argv[++i];
            }
            else if (strcmp(argv[i], "--ccflags") == 0) {
                conf.cc_flags = argv[++i];
            }
            else if (strcmp(argv[i], "--out") == 0) {
                conf.output_file = argv[++i];
            }
            else if (strcmp(argv[i], "--embed-core") == 0) {
                conf.embed_core = true;
            }
            else if (strcmp(argv[i], "--format-c") == 0) {
                conf.format_c = true;
            }
            else if (strcmp(argv[i], "--release") == 0) {
                conf.release = true;
            }
            else if (strcmp(argv[i], "--unsafe-no-bounds-checks") == 0) {
                conf.unsafe_no_bounds_checks = true;
            }
            else if (strcmp(argv[i], "--verbose") == 0) {
                conf.verbose = true;
            }
        }
        else {
            command = argv[i];

            cmd = jik_get_command(argv[i]);

            if (strcmp(argv[i], "help") == 0 && argc_sz == 2) {
                conf.command = "help_general";
                break;
            }

            jik_diag_fatal_error_if(i + cmd.num_args >= argc_sz,
                                    "input error",
                                    JIK_STRING_NCAT("missing arguments for command: ", argv[i]));
            jik_diag_fatal_error_if(cmd.num_args == 0 && cmd.num_options == 0 && argc_sz > 2,
                                    "input error",
                                    JIK_STRING_NCAT("command takes no arguments: ", argv[i]));

            if (strcmp(argv[i], "tran") == 0) {
                conf.command     = argv[i];
                conf.input_file  = argv[++i];
                conf.target_name = make_target_name(conf.input_file);
            }
            else if (strcmp(argv[i], "check") == 0) {
                conf.command     = argv[i];
                conf.input_file  = argv[++i];
                conf.target_name = make_target_name(conf.input_file);
            }
            else if (strcmp(argv[i], "run") == 0) {
                conf.command     = argv[i];
                conf.input_file  = argv[++i];
                conf.target_name = make_target_name(conf.input_file);
            }
            else if (strcmp(argv[i], "build") == 0) {
                conf.command     = argv[i];
                conf.input_file  = argv[++i];
                conf.target_name = make_target_name(conf.input_file);
            }
            else if (strcmp(argv[i], "memchk") == 0) {
                conf.command     = argv[i];
                conf.input_file  = argv[++i];
                conf.target_name = make_target_name(conf.input_file);
            }
            else if (strcmp(argv[i], "version") == 0) {
                conf.command = argv[i];
            }
            else if (strcmp(argv[i], "env") == 0) {
                conf.command = argv[i];
            }
            else if (strcmp(argv[i], "help") == 0) {
                conf.command = argv[i];
                if (i + 1 < argc_sz) {
                    conf.help_topic = argv[++i];
                }
            }
        }
    }
    conf.jik_root_dir = get_program_dir();
    assert(conf.jik_root_dir);
    conf.jiklib_path     = JIK_STRING_NCAT(conf.jik_root_dir, PATH_SEP, "jiklib", PATH_SEP);
    conf.jik_core_h_path = JIK_STRING_NCAT(
        conf.jik_root_dir, PATH_SEP, "support", PATH_SEP, "include", PATH_SEP, "core.h");
    return conf;
}

void
jik_config_print(JikConfig *conf)
{
    printf("<Config:\n\tcommand=%s\n\tinput_file=%s\n\tcc=%s\n>",
           conf->command,
           conf->input_file,
           conf->cc);
}
