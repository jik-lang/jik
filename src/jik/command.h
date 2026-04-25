#ifndef JIK_COMMAND_H
#define JIK_COMMAND_H

#include <stddef.h>

#define MAX_ARGS    5
#define MAX_OPTIONS 10

typedef struct JikArg {
    char *name;
    char *help_desc;
} JikArg;

typedef struct JikOption {
    char  *name;
    char  *help_desc;
    size_t num_args;
    JikArg args[MAX_ARGS];
} JikOption;

typedef struct JikCommand {
    char     *name;
    char     *help_desc;
    char     *help_desc_short;
    char     *help_notes;
    size_t    num_args;
    JikArg    args[MAX_ARGS];
    size_t    num_options;
    JikOption options[MAX_OPTIONS];
} JikCommand;

extern const JikCommand JIK_COMMANDS[];
extern const size_t     NUM_COMMANDS;

JikCommand
jik_get_command(char *name);

JikOption
jik_get_option(JikCommand cmd, char *name);

#endif
