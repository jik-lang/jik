// Ensure POSIX APIs (strdup/popen/pclose) are declared when compiling with -std=c11.
// This file is part of the seed compiler and is intended to be portable across
// POSIX and Windows.
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "compiler.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "charbuf.h"
#include "codegen.h"
#include "context.h"
#include "diag.h"
#include "help.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"
#include "utils.h"
#include "version.h"

static void
jik_compiler_verbose(JikConfig *conf, char *phase, char *details)
{
    if (!conf->verbose) {
        return;
    }
    printf("[jik] %-9s %s\n", phase, details);
}

static char *
alias_from_filepath(char *filepath)
{
    char *last_slash = strrchr(filepath, '/');
    if (last_slash)
        return last_slash + 1;
    else
        return filepath;
}

static bool
jik_module_path_is_stdlib(char *path)
{
    return strncmp(path, "jik/", 4) == 0;
}

// Check if path is stdlib path
static char *
get_proper_path(char *path, JikContext *ctx)
{
    if (jik_module_path_is_stdlib(path)) {
        char *tail = strdup(path + 4);
        if (strcmp(tail, "math.jik") == 0) {
            ctx->math_used = true;
        }
        return JIK_STRING_NCAT(ctx->conf.jiklib_path, tail);
    }
    return path;
}

static char *
jik_module_dirname(char *filepath)
{
    char *last_slash     = strrchr(filepath, '/');
    char *last_backslash = strrchr(filepath, '\\');
    char *last_sep       = last_slash;
    if (last_backslash && (!last_sep || last_backslash > last_sep))
        last_sep = last_backslash;
    if (!last_sep)
        return "";

    size_t len = (size_t)(last_sep - filepath);
    char  *dir = jik_alloc(len + 1);
    strncpy(dir, filepath, len);
    dir[len] = '\0';
    return dir;
}

static bool
jik_module_path_is_absolute(char *path)
{
    return path[0] == '/' || path[0] == '\\' || (strlen(path) > 2 && path[1] == ':');
}

static char *
jik_module_resolve_use_path(char *use_path, char *current_source_path)
{
    if (jik_module_path_is_stdlib(use_path) || jik_module_path_is_absolute(use_path)) {
        return use_path;
    }

    char *dir = jik_module_dirname(current_source_path);
    if (dir[0] == '\0') {
        return use_path;
    }

    return JIK_STRING_NCAT(dir, "/", use_path);
}

static void
jik_compiler_validate_use_path(char *path, JikToken *tok)
{
    if (strchr(path, '\\') != NULL) {
        jik_diag_fatal_error("module paths in use declarations must use '/' separators",
                             jik_token_to_text(tok));
    }
}

static size_t
jik_compiler_require_use_newline(VecJikToken *tokens, size_t next_idx)
{
    if (next_idx >= VecJikToken_size(tokens)) {
        return next_idx;
    }

    JikToken *next = VecJikToken_get_ptr(tokens, next_idx);
    jik_diag_fatal_error_if(next->type != TOK_NEWLINE,
                            "expected newline after use declaration",
                            jik_token_to_text(next));
    return next_idx;
}

static VecJikModule *
jik_compiler_get_usages(JikContext *ctx, JikModule *mod)
{
    VecJikModule *usages = VecJikModule_new_empty();
    char         *full_path =
        strcmp(mod->alias, "main") == 0 ? mod->filepath : jik_string_cat(mod->filepath, ".jik");
    char    *pp = get_proper_path(full_path, ctx);
    JikLexer lex;
    char    *code = jik_read_file(pp);
    if (code == NULL) {
        jik_diag_fatal_error("file not found: ", pp);
    }
    jik_lexer_init(&lex, ctx, code, pp);
    // Tokenize module
    mod->tokens = jik_lexer_tokenize(&lex);

    jik_diag_fatal_error_if(
        VecJikToken_size(mod->tokens) == 0, "empty module cannot be compiled", mod->filepath);

    JikToken *t1;
    // Add module aliases to tokens
    for (size_t i = 0; i < VecJikToken_size(mod->tokens); i++) {
        t1               = VecJikToken_get_ptr(mod->tokens, i);
        t1->mod_alias    = mod->alias;
        t1->used_aliases = mod->used_aliases;
    }
    mod->usages = TabBool_new();
    size_t    i = 0;
    size_t    n = VecJikToken_size(mod->tokens);
    JikToken *tok;
    TabBool  *seen_filepaths  = TabBool_new();
    bool      usage_not_first = false;
    while (i < n) {
        tok = VecJikToken_get_ptr(mod->tokens, i);
        if (tok->type == TOK_NEWLINE) {
            i++;
            continue;
        }
        else if (tok->type != TOK_KWD_USE) {
            usage_not_first = true;
            i++;
        }
        else if (tok->type == TOK_KWD_USE) {
            JikModule used_mod = {.usages       = TabBool_new(),
                                  .tokens       = VecJikToken_new_empty(),
                                  .used_aliases = TabBool_new()};
            if (usage_not_first) {
                jik_diag_fatal_error("syntax error", jik_token_to_text(tok));
            }
            if (i + 1 >= n) {
                jik_diag_fatal_error("syntax error", jik_token_to_text(tok));
            }
            tok = VecJikToken_get_ptr(mod->tokens, i + 1);
            if (tok->type != TOK_STRING) {
                jik_diag_fatal_error("expected string", jik_token_to_text(tok));
            }
            jik_compiler_validate_use_path(tok->lexeme, tok);
            used_mod.filepath = jik_module_resolve_use_path(tok->lexeme, pp);
            bool *fp          = TabBool_get(seen_filepaths, used_mod.filepath);
            if (fp != NULL) {
                char *msg = jik_string_cat("reuse of module ", used_mod.filepath);
                jik_diag_fatal_error(msg, jik_token_to_text(tok));
            }
            TabBool_set(seen_filepaths, used_mod.filepath, true);
            TabBool_set(mod->usages, used_mod.filepath, true);

            if (i + 3 < n && VecJikToken_get_ptr(mod->tokens, i + 2)->type == TOK_KWD_AS) {
                tok = VecJikToken_get_ptr(mod->tokens, i + 3);
                if (tok->type != TOK_ID) {
                    jik_diag_fatal_error("expected alias", jik_token_to_text(tok));
                }
                if (strcmp(tok->lexeme, "main") == 0) {
                    jik_diag_fatal_error("alias \"main\" not allowed", jik_token_to_text(tok));
                }
                if (jik_identifier_has_reserved_prefix(tok->lexeme)) {
                    jik_diag_fatal_error(JIK_STRING_NCAT("identifier \"",
                                                         tok->lexeme,
                                                         "\" uses reserved prefix \"jik_\""),
                                         jik_token_to_text(tok));
                }
                used_mod.alias = tok->lexeme;
                jik_diag_fatal_error_if(
                    TabBool_get(mod->used_aliases, used_mod.alias) != NULL,
                    JIK_STRING_NCAT("alias already used in module: ", used_mod.alias),
                    jik_token_to_text(tok));
                TabBool_set(mod->used_aliases, used_mod.alias, true);
                jik_compiler_require_use_newline(mod->tokens, i + 4);
                i += 5;
            }
            else {
                used_mod.alias = strdup(alias_from_filepath(used_mod.filepath));
                if (jik_identifier_has_reserved_prefix(used_mod.alias)) {
                    jik_diag_fatal_error(JIK_STRING_NCAT("identifier \"",
                                                         used_mod.alias,
                                                         "\" uses reserved prefix \"jik_\""),
                                         jik_token_to_text(tok));
                }
                jik_diag_fatal_error_if(
                    TabBool_get(mod->used_aliases, used_mod.alias) != NULL,
                    JIK_STRING_NCAT("alias already used in module: ", used_mod.alias),
                    jik_token_to_text(tok));
                TabBool_set(mod->used_aliases, used_mod.alias, true);
                jik_compiler_require_use_newline(mod->tokens, i + 2);
                i += 3;
            }
            VecJikModule_push(usages, used_mod);
        }
    }
    if (VecJikModule_size(usages) == 0)
        mod->is_leaf = true;
    return usages;
}

static void
jik_compiler_tokenize_modules(JikContext *ctx)
{
    VecJikModule *unprocessed_modules = VecJikModule_new_empty();
    VecJikModule_push(unprocessed_modules,
                      (JikModule){.filepath     = ctx->conf.input_file,
                                  .alias        = "main",
                                  .used_aliases = TabBool_new()});
    TabBool      *seen_filepaths = TabBool_new();
    JikModule     current;
    VecJikModule *usages;
    while (VecJikModule_size(unprocessed_modules) > 0) {
        current = VecJikModule_pop(unprocessed_modules);

        bool *fp = TabBool_get(seen_filepaths, current.filepath);
        if (fp != NULL) {
            continue;
        }
        TabBool_set(seen_filepaths, current.filepath, true);

        usages = jik_compiler_get_usages(ctx, &current);
        if (current.is_leaf) {
            VecJikModule_push(ctx->leaves, current);
        }
        else {
            VecJikModule_push(ctx->branches, current);
        }
        for (size_t i = 0; i < VecJikModule_size(usages); i++)
            VecJikModule_push(unprocessed_modules, VecJikModule_get(usages, i));
    }
}

static bool
jik_compiler_can_process_module(JikModule *mod, TabBool *processed_modules)
{
    bool         res = true;
    TabBool_iter it  = TabBool_iter_new(mod->usages);
    TabBool_item item;
    while (TabBool_iter_next(&it, &item)) {
        bool *x = TabBool_get(processed_modules, item.key);
        if (x == NULL) {
            res = false;
        }
    }
    return res;
}

static void
jik_compiler_import_cycle_error(VecJikModule *mods, TabBool *processed_modules)
{
    CharBuffer *details = char_buffer_new("could not resolve imports for modules:");
    bool        found   = false;
    for (size_t i = 0; i < VecJikModule_size(mods); i++) {
        JikModule *mod = VecJikModule_get_ptr(mods, i);
        if (TabBool_get(processed_modules, mod->filepath) != NULL) {
            continue;
        }
        found = true;
        char_buffer_append(details, "\n- ");
        char_buffer_append(details, mod->filepath);
    }
    jik_diag_fatal_error(found ? "module import cycle detected"
                               : "internal error: unresolved module ordering",
                         details->data);
}

static void
jik_compiler_merge_tokens(JikContext *ctx)
{
    if (VecJikModule_size(ctx->leaves) == 0) {
        jik_compiler_import_cycle_error(ctx->branches, TabBool_new());
    }
    JikModule current;
    TabBool  *processed_modules = TabBool_new();
    while (VecJikModule_size(ctx->leaves) > 0) {
        current = VecJikModule_pop(ctx->leaves);
        VecJikToken_extend(ctx->tokens, current.tokens);
        TabBool_set(processed_modules, current.filepath, true);
    }
    size_t     num_branches = VecJikModule_size(ctx->branches);
    size_t     i            = 0;
    JikModule *mod;
    while (i < num_branches) {
        bool made_progress = false;
        for (size_t j = 0; j < num_branches; j++) {
            mod = VecJikModule_get_ptr(ctx->branches, j);
            if (TabBool_get(processed_modules, mod->filepath) != NULL)
                continue;
            if (jik_compiler_can_process_module(mod, processed_modules)) {
                VecJikToken_extend(ctx->tokens, mod->tokens);
                TabBool_set(processed_modules, mod->filepath, true);
                i++;
                made_progress = true;
            }
        }
        if (!made_progress) {
            jik_compiler_import_cycle_error(ctx->branches, processed_modules);
        }
    }
}

static void
jik_compiler_dump_translation(JikContext *ctx)
{
    if (!ctx->conf.output_file) {
        ctx->conf.output_file = jik_string_cat(ctx->conf.target_name, ".c");
    }
    jik_compiler_verbose(&ctx->conf, "write", ctx->conf.output_file);
    FILE *f = fopen(ctx->conf.output_file, "w");
    jik_diag_fatal_error_if(f == NULL, "error opening output file", "");
    jik_diag_fatal_error_if(
        fprintf(f, "%s\n", ctx->translation) < 0, "error writing to output file", "");
    fclose(f);
}

#ifdef _WIN32
#define POPEN   _popen
#define PCLOSE  _pclose
#define OUT_EXT ".exe"
#else
#define POPEN   popen
#define PCLOSE  pclose
#define OUT_EXT ""
#endif

static int
jik_compiler_run_binary(char *bin_fp)
{
    char *cmd = jik_alloc(strlen(bin_fp) + 1);
    strcpy(cmd, bin_fp);
#ifdef _WIN32
    for (char *p = cmd; *p; ++p) {
        if (*p == '/')
            *p = '\\';
    }
#else
    cmd = JIK_STRING_NCAT("./", cmd);
#endif
    cmd = shell_quote_arg(cmd);
    // TODO: see if we want to use POPEN here also
    return system(cmd);
}

static char *
get_jik_cc_env()
{
    char *v = getenv("JIK_CC"); // returns NULL if not set
    return (v && *v) ? v : NULL;
}

static char *
jik_get_compiler_from_conf(JikConfig *conf)
{
    char *compiler = NULL;
    if (conf->cc) {
        compiler = conf->cc;
    }
    else {
        char *jik_cc = get_jik_cc_env();
        compiler     = jik_cc;
        // Usability fallback: if no explicit compiler is configured, try the
        // system default C compiler.
#ifndef _WIN32
        if (!compiler) {
            int status = system("cc --version > /dev/null 2>&1");
            if (status == 0) {
                compiler = "cc";
            }
        }
#endif
    }
    return compiler;
}

static void
jik_compiler_env(JikConfig *conf)
{
    char *compiler = jik_get_compiler_from_conf(conf);

    printf("version=%s\n", JIK_VERSION_STRING);
#ifdef _WIN32
    printf("platform=windows\n");
#else
    printf("platform=linux\n");
#endif
    printf("root=%s\n", conf->jik_root_dir);
    printf("jiklib=%s\n", conf->jiklib_path);
    printf("core=%s\n", conf->jik_core_h_path);
    printf("cc=%s\n", compiler ? compiler : "");
}

static char *
jik_get_linker_args(JikContext *ctx)
{
#ifdef _WIN32
    return "";
#else
    if (ctx->math_used) {
        return "-lm";
    }
    return "";
#endif
}

static void
jik_compiler_build(JikContext *ctx, bool run)
{
    char *cmd      = NULL;
    char *out_bin  = ctx->conf.output_file ? ctx->conf.output_file
                                           : jik_string_cat(ctx->conf.target_name, OUT_EXT);
    char *compiler = jik_get_compiler_from_conf(&ctx->conf);
    jik_diag_fatal_error_if(
        !compiler, "no compiler found, either set using JIK_CC or with the --cc option", "");
#ifdef _WIN32
    char *default_cc_flags = "-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS";
#else
    char *default_cc_flags = "";
#endif
    char *release_cc_flags = ctx->conf.release ? "-O3" : "";
    char *user_cc_flags    = ctx->conf.cc_flags ? ctx->conf.cc_flags : "";
    char *base_cc_flags    = *release_cc_flags
                                 ? JIK_STRING_NCAT(default_cc_flags, " ", release_cc_flags)
                                 : default_cc_flags;
    char *cc_flags =
        *user_cc_flags ? JIK_STRING_NCAT(base_cc_flags, " ", user_cc_flags) : base_cc_flags;
    char *linker_args    = jik_get_linker_args(ctx);
    char *quoted_out_bin = shell_quote_arg(out_bin);
    cmd                  = JIK_STRING_NCAT(
        compiler, " -x c ", cc_flags, " -o ", quoted_out_bin, " -", " ", linker_args);
    jik_compiler_verbose(&ctx->conf, "compile", cmd);
    FILE *cc_pipe = POPEN(cmd, "w");
    jik_diag_fatal_error_if(cc_pipe == NULL, "error opening CC", "");
    fputs(ctx->translation, cc_pipe);
    int ret = PCLOSE(cc_pipe);
    jik_diag_fatal_error_if(ret != 0, "CC compilation failed", "");

    if (run) {
        jik_compiler_verbose(&ctx->conf, "run", out_bin);
        int status = jik_compiler_run_binary(out_bin);
        remove(out_bin);
        if (status != 0) {
            exit(EXIT_FAILURE);
        }
    }
}

static void
jik_compiler_memchk(JikContext *ctx)
{
    jik_diag_fatal_error_if(!system_has_tool("valgrind", "--version"), "valgrind not found", "");
    char *compiler = jik_get_compiler_from_conf(&ctx->conf);
    jik_diag_fatal_error_if(
        !compiler, "no compiler found, either set using JIK_CC or with --cc flag", "");
    char *out_bin        = jik_string_cat(ctx->conf.target_name, OUT_EXT);
    char *quoted_out_bin = shell_quote_arg(out_bin);
    char *cmd            = JIK_STRING_NCAT(compiler, " -g -O0 -x c -o ", quoted_out_bin, " -");
    jik_compiler_verbose(&ctx->conf, "compile", cmd);
    FILE *cc_pipe = POPEN(cmd, "w");
    jik_diag_fatal_error_if(cc_pipe == NULL, "error opening CC", "");
    fputs(ctx->translation, cc_pipe);
    int ret = PCLOSE(cc_pipe);
    jik_diag_fatal_error_if(ret != 0, "CC compilation failed", "");
    char *vlgr_cmd = JIK_STRING_NCAT("valgrind "
                                     "--tool=memcheck "
                                     "--leak-check=full "
                                     "--show-leak-kinds=all "
                                     "--track-origins=yes "
                                     "--errors-for-leak-kinds=all "
                                     "--track-fds=yes ",
                                     quoted_out_bin);
    jik_compiler_verbose(&ctx->conf, "memchk", vlgr_cmd);
    // TODO: redirect valgrind out to buffer?
    int status = system(vlgr_cmd);
    jik_diag_fatal_error_if(status != 0, "valgrind failed", "");
}

void
jik_compiler_run(JikConfig conf)
{
    if (strcmp(conf.command, "version") == 0) {
        printf("jik version: %s\n", JIK_VERSION_STRING);
        return;
    }
    if (strcmp(conf.command, "env") == 0) {
        jik_compiler_env(&conf);
        return;
    }
    if (strcmp(conf.command, "help_general") == 0) {
        printf("%s", jik_get_help_general());
        return;
    }
    if (strcmp(conf.command, "help") == 0) {
        printf("%s", jik_get_help(conf.help_topic));
        return;
    }

    JikContext ctx;
    jik_context_init(&ctx, conf);
    jik_compiler_verbose(&conf, "tokenize", conf.input_file);
    jik_compiler_tokenize_modules(&ctx);
    jik_compiler_verbose(&conf, "merge", "resolving module order");
    jik_compiler_merge_tokens(&ctx);
    // VecJikToken_print(ctx.tokens);

    JikParser p;
    jik_parser_init(&p, &ctx);
    jik_compiler_verbose(&conf, "parse", "building AST");
    jik_parser_run(&p);
    jik_collect_nodes(p.ast, ctx.nodes);
    // jik_node_print(p.ast, 0);

    JikSemanticAnalyzer sa;
    jik_semantic_init(&sa, &ctx);
    jik_compiler_verbose(&conf, "semantic", "analyzing program");
    jik_semantic_run(&sa);
    // jik_node_print(p.ast, 0);

    if (strcmp(conf.command, "check") == 0) {
        jik_diag_messages_print();
        printf("check passed\n");
        return;
    }

    JikCodeGenerator cg;
    jik_codegen_init(&cg, &ctx);
    jik_compiler_verbose(&conf, "codegen", "generating C translation");
    jik_codegen_run(&cg);

    jik_diag_messages_print();
    // printf("%s", ctx.translation);
    if (strcmp(conf.command, "tran") == 0) {
        jik_compiler_dump_translation(&ctx);
        if (conf.format_c) {
            jik_diag_fatal_error_if(
                !system_has_tool("clang-format", "--version"), "clang-format not available", "");
            jik_compiler_verbose(&conf, "format", ctx.conf.output_file);
            char *quoted_output = shell_quote_arg(ctx.conf.output_file);
            int   status        = system(JIK_STRING_NCAT("clang-format -i ", quoted_output));
            jik_diag_fatal_error_if(status != 0, "clang-format failed", "");
        }
    }
    else if (strcmp(conf.command, "run") == 0) {
        jik_compiler_build(&ctx, true);
    }
    else if (strcmp(conf.command, "build") == 0) {
        jik_compiler_build(&ctx, false);
    }
    else if (strcmp(conf.command, "memchk") == 0) {
        jik_compiler_memchk(&ctx);
    }
}
