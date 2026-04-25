#include "help.h"

#include <string.h>

#include "charbuf.h"
#include "command.h"
#include "diag.h"
#include "utils.h"

#define INDENT_LVL_1 "    "
#define INDENT_LVL_2 "          "

char *
jik_get_help_general(void)
{
    CharBuffer *cb =
        char_buffer_new("The command-line toolchain for the Jik programming language.\n\n"
                        "Usage:\n\n");

    char_buffer_append(
        cb, JIK_STRING_NCAT(INDENT_LVL_1, "jik <command> [<args>...] [--<option> [<value>]]..."));
    char_buffer_append(cb, "\n\nAvailable commands:\n\n");

    size_t spacing_const = 10;
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        size_t req_spaces = spacing_const - strlen(JIK_COMMANDS[i].name);
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, JIK_COMMANDS[i].name));
        for (size_t i = 0; i < req_spaces; i++) {
            char_buffer_push(cb, ' ');
        }
        char_buffer_append(cb,
                           JIK_STRING_NCAT(INDENT_LVL_1, JIK_COMMANDS[i].help_desc_short, "\n"));
    }
    return cb->data;
}

char *
jik_get_help(char *topic)
{
    JikCommand  cmd = jik_get_command(topic);
    CharBuffer *cb  = char_buffer_new(cmd.help_desc);
    char       *notes_start;
    char       *notes_end;
    char_buffer_append(cb, "\n\n");
    char_buffer_append(cb, "Usage:\n");
    char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "jik ", cmd.name));
    for (size_t i = 0; i < cmd.num_args; i++) {
        char_buffer_append(cb, " <");
        char_buffer_append(cb, cmd.args[i].name);
        char_buffer_append(cb, ">");
    }
    for (size_t i = 0; i < cmd.num_options; i++) {
        char_buffer_append(cb, " [");
        char_buffer_append(cb, cmd.options[i].name);
        for (size_t j = 0; j < cmd.options[i].num_args; j++) {
            char_buffer_append(cb, " <");
            char_buffer_append(cb, cmd.options[i].args[j].name);
            char_buffer_append(cb, "> ");
        }
        char_buffer_append(cb, "]");
    }
    char_buffer_append(cb, "\n\n");

    if (cmd.num_args > 0) {
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "Arguments:\n"));
        for (size_t i = 0; i < cmd.num_args; i++) {
            char_buffer_append(
                cb,
                JIK_STRING_NCAT(
                    INDENT_LVL_2, cmd.args[i].name, "    ", cmd.args[i].help_desc, "\n"));
        }
        char_buffer_append(cb, "\n");
    }

    if (cmd.num_options > 0) {
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "Options:\n"));
        for (size_t i = 0; i < cmd.num_options; i++) {
            char_buffer_append(
                cb,
                JIK_STRING_NCAT(
                    INDENT_LVL_2, cmd.options[i].name, "    ", cmd.options[i].help_desc, "\n"));
        }
        char_buffer_append(cb, "\n");
    }

    if (cmd.help_notes) {
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "Notes:\n"));
        notes_start = cmd.help_notes;
        while (notes_start && *notes_start) {
            notes_end = strchr(notes_start, '\n');
            char_buffer_append(cb, INDENT_LVL_2);
            if (notes_end) {
                for (char *p = notes_start; p < notes_end; p++) {
                    char_buffer_push(cb, *p);
                }
                char_buffer_append(cb, "\n");
                notes_start = notes_end + 1;
            }
            else {
                char_buffer_append(cb, notes_start);
                char_buffer_append(cb, "\n");
                break;
            }
        }
    }
    return cb->data;
}
