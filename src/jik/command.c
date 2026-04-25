#include "command.h"

#include <string.h>

#include "diag.h"
#include "utils.h"

const JikCommand JIK_COMMANDS[] = {
    // COMMAND: tran
    {
        .name            = "tran",
        .help_desc       = "Translate a Jik source code file to C.",
        .help_desc_short = "translate Jik source to C",
        .help_notes      = "Default output path: <input-basename>.c when --out is omitted.\n"
                           "--unsafe-no-bounds-checks disables generated vector get/set bounds checks.\n"
                           "--verbose prints translation pipeline steps.\n"
                           "--format-c formats the generated C file in place.",
        .num_args        = 1,
        .args =
            {
                {.name = "filepath", .help_desc = "Input file"},
            },
        .num_options = 5,
        .options =
            {
                {
                    .name      = "--out",
                    .help_desc = "Write output to file.",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "filepath", .help_desc = "Output file path."},
                        },
                },
                {
                    .name      = "--embed-core",
                    .help_desc = "Embed Jik support library (core.h) into the translation.",
                    .num_args  = 0,
                },
                {
                    .name      = "--format-c",
                    .help_desc = "Format the generated C code using clang-format. Fails if "
                                 "clang-format is not available.",
                    .num_args  = 0,
                },
                {
                    .name = "--unsafe-no-bounds-checks",
                    .help_desc =
                        "Disable runtime vector bounds checks for generated get/set operations.",
                    .num_args = 0,
                },
                {
                    .name      = "--verbose",
                    .help_desc = "Print detailed pipeline status information.",
                    .num_args  = 0,
                },
            },
    },
    // COMMAND: check
    {
        .name      = "check",
        .help_desc = "Parse and analyze a Jik source code file without generating C or invoking "
                     "the host compiler.",
        .help_desc_short = "parse and analyze Jik source",
        .help_notes      = "--verbose prints analysis pipeline steps.",
        .num_args        = 1,
        .args =
            {
                {.name = "filepath", .help_desc = "Input file"},
            },
        .num_options = 1,
        .options =
            {
                {
                    .name      = "--verbose",
                    .help_desc = "Print detailed pipeline status information.",
                    .num_args  = 0,
                },
            },
    },
    // COMMAND: doctor
    {
        .name      = "doctor",
        .help_desc = "Report resolved Jik paths, compiler selection, and availability of relevant "
                     "host tools.",
        .help_desc_short = "report CLI environment status",
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
                      "--unsafe-no-bounds-checks disables generated vector get/set bounds checks.\n"
                      "--release builds with release-oriented host compiler flags.\n"
                      "--verbose prints translation and build pipeline steps.\n"
                      "Default executable path: <input-basename>.exe when --out is omitted.",
        .num_args   = 1,
        .args =
            {
                {.name = "filepath", .help_desc = "Input file"},
            },
        .num_options = 6,
        .options =
            {
                {
                    .name      = "--out",
                    .help_desc = "Write output to file.",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "filepath", .help_desc = "Output file path."},
                        },
                },
                {
                    .name      = "--cc",
                    .help_desc = "C compiler to use.",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "compiler_name", .help_desc = "C compiler name."},
                        },
                },
                {
                    .name      = "--ccflags",
                    .help_desc = "C compiler arguments",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "args", .help_desc = "C compiler arguments."},
                        },
                },
                {
                    .name      = "--release",
                    .help_desc = "Build with release-oriented host compiler flags.",
                    .num_args  = 0,
                },
                {
                    .name = "--unsafe-no-bounds-checks",
                    .help_desc =
                        "Disable runtime vector bounds checks for generated get/set operations.",
                    .num_args = 0,
                },
                {
                    .name      = "--verbose",
                    .help_desc = "Print detailed pipeline status information.",
                    .num_args  = 0,
                },
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
                      "--unsafe-no-bounds-checks disables generated vector get/set bounds checks.\n"
                      "--release builds with release-oriented host compiler flags.\n"
                      "--verbose prints translation, build, and run pipeline steps.\n"
                      "The executable is written to <input-basename>.exe, run, and then removed.",
        .num_args   = 1,
        .args =
            {
                {.name = "filepath", .help_desc = "Input file"},
            },
        .num_options = 5,
        .options =
            {
                {
                    .name      = "--cc",
                    .help_desc = "C compiler to use.",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "compiler_name", .help_desc = "C compiler name."},
                        },
                },
                {
                    .name      = "--ccflags",
                    .help_desc = "C compiler arguments",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "args", .help_desc = "C compiler arguments."},
                        },
                },
                {
                    .name      = "--release",
                    .help_desc = "Build with release-oriented host compiler flags.",
                    .num_args  = 0,
                },
                {
                    .name = "--unsafe-no-bounds-checks",
                    .help_desc =
                        "Disable runtime vector bounds checks for generated get/set operations.",
                    .num_args = 0,
                },
                {
                    .name      = "--verbose",
                    .help_desc = "Print detailed pipeline status information.",
                    .num_args  = 0,
                },
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
                {.name = "filepath", .help_desc = "Input file"},
            },
        .num_options = 1,
        .options =
            {
                {
                    .name      = "--cc",
                    .help_desc = "C compiler to use.",
                    .num_args  = 1,
                    .args =
                        {
                            {.name = "compiler_name", .help_desc = "C compiler name."},
                        },
                },
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
                {.name = "command", .help_desc = "Name of command"},
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
