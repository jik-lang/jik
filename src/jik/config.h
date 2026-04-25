#ifndef JIK_CONFIG_H
#define JIK_CONFIG_H

#include <stdbool.h>

typedef struct JikConfig {
    char *command;
    char *input_file;
    char *target_name;
    char *output_file;
    char *cc;
    char *cc_flags;
    char *help_topic;
    char *jik_root_dir;
    char *jiklib_path;
    char *jik_core_h_path;
    bool  embed_core;
    bool  format_c;
    bool  release;
    bool  unsafe_no_bounds_checks;
    bool  verbose;
} JikConfig;

JikConfig
jik_config_make(int argc, char **argv);
void
jik_config_print(JikConfig *conf);

#endif
