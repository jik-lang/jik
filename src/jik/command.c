#include "command.h"

#include <string.h>

#include "diag.h"
#include "utils.h"

#define JIK_ARG_INPUT_FILE                                                                         \
    {                                                                                              \
        .name = "filepath", .help_desc = "Input file"                                              \
    }
#define JIK_ARG_OUTPUT_FILE                                                                        \
    {                                                                                              \
        .name = "filepath", .help_desc = "Output file path."                                       \
    }
#define JIK_ARG_COMPILER_NAME                                                                      \
    {                                                                                              \
        .name = "compiler_name", .help_desc = "C compiler name."                                   \
    }
#define JIK_ARG_CCFLAGS                                                                            \
    {                                                                                              \
        .name = "args", .help_desc = "C compiler arguments."                                       \
    }
#define JIK_ARG_COMMAND                                                                            \
    {                                                                                              \
        .name = "command", .help_desc = "Name of command"                                          \
    }

#define JIK_OPT_OUT                                                                                \
    {                                                                                              \
        .name = "--out", .help_desc = "Write output to file.", .num_args = 1, .args = {            \
            JIK_ARG_OUTPUT_FILE,                                                                   \
        }                                                                                          \
    }

#define JIK_OPT_CC                                                                                 \
    {                                                                                              \
        .name = "--cc", .help_desc = "C compiler to use.", .num_args = 1, .args = {                \
            JIK_ARG_COMPILER_NAME,                                                                 \
        }                                                                                          \
    }

#define JIK_OPT_CCFLAGS                                                                            \
    {                                                                                              \
        .name = "--ccflags", .help_desc = "C compiler arguments", .num_args = 1, .args = {         \
            JIK_ARG_CCFLAGS,                                                                       \
        }                                                                                          \
    }

#define JIK_OPT_EMBED_CORE                                                                         \
    {                                                                                              \
        .name      = "--embed-core",                                                               \
        .help_desc = "Embed Jik support library (core.h) into the translation.", .num_args = 0,    \
    }

#define JIK_OPT_FORMAT_C                                                                           \
    {                                                                                              \
        .name      = "--format-c",                                                                 \
        .help_desc = "Format the generated C code using clang-format. Fails if "                   \
                     "clang-format is not available.",                                             \
        .num_args  = 0,                                                                            \
    }

#define JIK_OPT_RELEASE                                                                            \
    {                                                                                              \
        .name = "--release", .help_desc = "Build with release-oriented host compiler flags.",      \
        .num_args = 0,                                                                             \
    }

#define JIK_OPT_UNSAFE_NO_BOUNDS_CHECKS                                                            \
    {                                                                                              \
        .name      = "--unsafe-no-bounds-checks",                                                  \
        .help_desc = "Disable runtime vector bounds checks for generated get/set operations.",     \
        .num_args  = 0,                                                                            \
    }

#define JIK_OPT_REGION_STATS                                                                       \
    {                                                                                              \
        .name      = "--region-stats",                                                             \
        .help_desc = "Print runtime region lifecycle statistics at program exit.", .num_args = 0,  \
    }

#define JIK_OPT_VERBOSE                                                                            \
    {                                                                                              \
        .name = "--verbose", .help_desc = "Print detailed pipeline status information.",           \
        .num_args = 0,                                                                             \
    }

const JikCommand JIK_COMMANDS[] = {
    // COMMAND: tran
    {
        .name            = "tran",
        .help_desc       = "Translate a Jik source code file to C.",
        .help_desc_short = "translate Jik source to C",
        .help_notes      = "Default output path: <input-basename>.c when --out is omitted.\n"
                           "--format-c   formats the generated C file in place.",
        .num_args        = 1,
        .args =
            {
                JIK_ARG_INPUT_FILE,
            },
        .num_options = 6,
        .options =
            {
                JIK_OPT_OUT,
                JIK_OPT_EMBED_CORE,
                JIK_OPT_FORMAT_C,
                JIK_OPT_UNSAFE_NO_BOUNDS_CHECKS,
                JIK_OPT_REGION_STATS,
                JIK_OPT_VERBOSE,
            },
    },
    // COMMAND: check
    {
        .name      = "check",
        .help_desc = "Parse and analyze a Jik source code file without generating C or invoking "
                     "the host compiler.",
        .help_desc_short = "parse and analyze Jik source",
        .num_args        = 1,
        .args =
            {
                JIK_ARG_INPUT_FILE,
            },
        .num_options = 1,
        .options =
            {
                JIK_OPT_VERBOSE,
            },
    },
    // COMMAND: env
    {
        .name            = "env",
        .help_desc       = "Print resolved Jik configuration values as key=value lines.",
        .help_desc_short = "print resolved Jik configuration",
        .num_options     = 0,
    },
    // COMMAND: build
    {
        .name            = "build",
        .help_desc       = "Translate a Jik source code file to C and build an executable with the "
                           "selected C compiler.",
        .help_desc_short = "translate and build Jik source to executable",
        .help_notes = "Compiler selection order: --cc, then JIK_CC, then a host default compiler "
                      "when available.\n"
                      "Default executable path: <input_file>.exe when --out is omitted.",
        .num_args   = 1,
        .args =
            {
                JIK_ARG_INPUT_FILE,
            },
        .num_options = 7,
        .options =
            {
                JIK_OPT_OUT,
                JIK_OPT_CC,
                JIK_OPT_CCFLAGS,
                JIK_OPT_RELEASE,
                JIK_OPT_UNSAFE_NO_BOUNDS_CHECKS,
                JIK_OPT_REGION_STATS,
                JIK_OPT_VERBOSE,
            },
    },
    // COMMAND: run
    {
        .name = "run",
        .help_desc =
            "Translate a Jik source code file to C, build it with the selected C compiler, and run "
            "the executable.",
        .help_desc_short = "translate, build and run executable",
        .help_notes = "Compiler selection order: --cc, then JIK_CC, then a host default compiler "
                      "when available.\n"
                      "The executable is written to <input_file>.exe, run, and then removed.",
        .num_args   = 1,
        .args =
            {
                JIK_ARG_INPUT_FILE,
            },
        .num_options = 6,
        .options =
            {
                JIK_OPT_CC,
                JIK_OPT_CCFLAGS,
                JIK_OPT_RELEASE,
                JIK_OPT_UNSAFE_NO_BOUNDS_CHECKS,
                JIK_OPT_REGION_STATS,
                JIK_OPT_VERBOSE,
            },
    },
    // COMMAND: memchk
    {
        .name      = "memchk",
        .help_desc = "Translate a Jik source code file to C, build it with debug flags, and run "
                     "valgrind memcheck.",
        .help_desc_short = "build and run valgrind memcheck",
        .help_notes = "Compiler selection order: --cc, then JIK_CC, then a host default compiler "
                      "when available.\n"
                      "Builds with debug flags and then runs valgrind memcheck.",
        .num_args   = 1,
        .args =
            {
                JIK_ARG_INPUT_FILE,
            },
        .num_options = 1,
        .options =
            {
                JIK_OPT_CC,
            },
    },
    // COMMAND: help
    {
        .name            = "help",
        .help_desc       = "Show help for a specific command.",
        .help_desc_short = "show help for command",
        .num_args        = 1,
        .args =
            {
                JIK_ARG_COMMAND,
            },
    },
    // COMMAND: version
    {
        .name            = "version",
        .help_desc       = "Shows the version of Jik.",
        .help_desc_short = "show Jik version",
    },
};

const size_t NUM_COMMANDS = sizeof(JIK_COMMANDS) / sizeof(JIK_COMMANDS[0]);

JikCommand
jik_get_command(char *name)
{
    size_t num_commands = sizeof(JIK_COMMANDS) / sizeof(JIK_COMMANDS[0]);
    for (size_t i = 0; i < num_commands; i++) {
        if (strcmp(name, JIK_COMMANDS[i].name) == 0) {
            return JIK_COMMANDS[i];
        }
    }
    jik_diag_fatal_error("input error", jik_string_cat("unknown command: ", name));
}

JikOption
jik_get_option(JikCommand cmd, char *name)
{
    jik_diag_fatal_error_if(cmd.num_options == 0, "input error", "given command has no options");
    for (size_t i = 0; i < cmd.num_options; i++) {
        if (strcmp(name, cmd.options[i].name) == 0) {
            return cmd.options[i];
        }
    }
    jik_diag_fatal_error("input error", jik_string_cat("unknown option: ", name));
}
