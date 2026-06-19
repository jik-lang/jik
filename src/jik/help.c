#include "help.h"

#include <stdbool.h>
#include <string.h>

#include "alloc.h"
#include "charbuf.h"
#include "command.h"
#include "diag.h"
#include "utils.h"

#if defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

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

static JikCommand *
jik_find_command(char *topic)
{
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(topic, JIK_COMMANDS[i].name) == 0) {
            return (JikCommand *)&JIK_COMMANDS[i];
        }
    }
    return NULL;
}

static char *
jik_get_command_help(JikCommand *cmd)
{
    CharBuffer *cb = char_buffer_new(cmd->help_desc);
    char       *notes_start;
    char       *notes_end;
    char_buffer_append(cb, "\n\n");
    char_buffer_append(cb, "Usage:\n");
    char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "jik ", cmd->name));
    for (size_t i = 0; i < cmd->num_args; i++) {
        char_buffer_append(cb, " <");
        char_buffer_append(cb, cmd->args[i].name);
        char_buffer_append(cb, ">");
    }
    for (size_t i = 0; i < cmd->num_options; i++) {
        char_buffer_append(cb, " [");
        char_buffer_append(cb, cmd->options[i].name);
        for (size_t j = 0; j < cmd->options[i].num_args; j++) {
            char_buffer_append(cb, " <");
            char_buffer_append(cb, cmd->options[i].args[j].name);
            char_buffer_append(cb, "> ");
        }
        char_buffer_append(cb, "]");
    }
    char_buffer_append(cb, "\n\n");

    if (cmd->num_args > 0) {
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "Arguments:\n"));
        for (size_t i = 0; i < cmd->num_args; i++) {
            char_buffer_append(
                cb,
                JIK_STRING_NCAT(
                    INDENT_LVL_2, cmd->args[i].name, "    ", cmd->args[i].help_desc, "\n"));
        }
        char_buffer_append(cb, "\n");
    }

    if (cmd->num_options > 0) {
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "Options:\n"));
        for (size_t i = 0; i < cmd->num_options; i++) {
            char_buffer_append(
                cb,
                JIK_STRING_NCAT(
                    INDENT_LVL_2, cmd->options[i].name, "    ", cmd->options[i].help_desc, "\n"));
        }
        char_buffer_append(cb, "\n");
    }

    if (cmd->help_notes) {
        char_buffer_append(cb, JIK_STRING_NCAT(INDENT_LVL_1, "Notes:\n"));
        notes_start = cmd->help_notes;
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

static char *
jik_help_trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r') {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        *--end = '\0';
    }
    return s;
}

static bool
jik_help_key_matches(char *keys, char *topic)
{
    char *key = keys;
    for (;;) {
        char *comma = strchr(key, ',');
        if (comma) {
            *comma = '\0';
        }
        if (strcmp(jik_help_trim(key), topic) == 0) {
            return true;
        }
        if (!comma) {
            return false;
        }
        key = comma + 1;
    }
}

static char *
jik_help_docs_dir(char *jik_root_dir)
{
    return JIK_STRING_NCAT(jik_root_dir, PATH_SEP, "docs");
}

static char *
jik_help_index_path(char *jik_root_dir)
{
    return JIK_STRING_NCAT(jik_help_docs_dir(jik_root_dir), PATH_SEP, "help-index.txt");
}

static char *
jik_help_find_doc_path(char *jik_root_dir, char *topic)
{
    char *docs_dir = jik_help_docs_dir(jik_root_dir);
    char *index    = jik_read_file(jik_help_index_path(jik_root_dir));
    if (!index) {
        return NULL;
    }

    char *line = index;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = '\0';
        }

        char *clean = jik_help_trim(line);
        if (*clean && *clean != '#') {
            char *eq = strchr(clean, '=');
            if (eq) {
                *eq       = '\0';
                char *rel = jik_help_trim(eq + 1);
                if (jik_help_key_matches(jik_help_trim(clean), topic)) {
                    return JIK_STRING_NCAT(docs_dir, PATH_SEP, rel);
                }
            }
        }

        line = next ? next + 1 : NULL;
    }
    return NULL;
}

static char *
jik_help_topics(char *jik_root_dir)
{
    char *index = jik_read_file(jik_help_index_path(jik_root_dir));
    if (!index) {
        return NULL;
    }

    CharBuffer *out  = char_buffer_new("Documentation topics:\n\n");
    char       *line = index;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = '\0';
        }

        char *clean = jik_help_trim(line);
        if (*clean && *clean != '#') {
            char *eq = strchr(clean, '=');
            if (eq) {
                *eq = '\0';
                char_buffer_append(out, INDENT_LVL_1);
                char_buffer_append(out, jik_help_trim(clean));
                char_buffer_append(out, "\n");
            }
        }

        line = next ? next + 1 : NULL;
    }
    return out->data;
}

static char *
jik_get_topic_help(char *topic, char *jik_root_dir)
{
    if (strcmp(topic, "topics") == 0) {
        return jik_help_topics(jik_root_dir);
    }

    char *path = jik_help_find_doc_path(jik_root_dir, topic);
    return path ? jik_read_file(path) : NULL;
}

char *
jik_get_help(char *topic, char *jik_root_dir)
{
    JikCommand *cmd = jik_find_command(topic);
    if (cmd) {
        return jik_get_command_help(cmd);
    }

    char *doc = jik_get_topic_help(topic, jik_root_dir);
    if (doc) {
        return doc;
    }

    jik_diag_fatal_error("unknown help topic",
                         JIK_STRING_NCAT(topic, "\n\nTry:\n  jik help topics"));
}
