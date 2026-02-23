/*
 * shelli - Educational Shell
 * main.c - REPL loop with page-based UI
 *
 * Flow: Welcome → Input → Lex → Parse → Expand → Execute → Result
 * Each stage populates StageData, then the stage browser lets users
 * navigate between full-screen pages explaining each step.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "tui/tui.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "executor.h"
#include "builtins.h"
#include "expand.h"
#include "variables.h"
#include "functions.h"
#include "jobs.h"
#include "util.h"

static volatile sig_atomic_t interrupted = 0;

static void handle_sigint(int sig) {
    (void)sig;
    interrupted = 1;
    pid_t fg = executor_get_fg_pid();
    if (fg > 0) {
        kill(fg, SIGINT);
    }
}

/* Exec logger writes to shared StageData */
static StageData *active_stage_data = NULL;

static void exec_logger(const char *message) {
    if (!active_stage_data) return;

    /* Parse the message to determine type */
    char prefix_char = '>';
    int color = COL_BLUE;
    int indent = 0;

    if (strncmp(message, "pipe()", 6) == 0) {
        prefix_char = '>';
        color = COL_BLUE;
    } else if (strncmp(message, "fork()", 6) == 0) {
        prefix_char = '>';
        color = COL_BLUE;
    } else if (strncmp(message, "  dup2", 6) == 0 ||
               strncmp(message, "  close", 7) == 0) {
        prefix_char = '~';
        color = COL_YELLOW;
        indent = 1;
    } else if (strncmp(message, "  execvp", 8) == 0 ||
               strncmp(message, "  exec:", 7) == 0) {
        prefix_char = '*';
        color = COL_PINK;
        indent = 1;
    } else if (strncmp(message, "waitpid", 7) == 0 ||
               strncmp(message, "exit", 4) == 0) {
        prefix_char = '.';
        color = COL_GREEN;
    } else if (strstr(message, "builtin:") != NULL) {
        prefix_char = '*';
        color = COL_PINK;
    } else {
        prefix_char = '>';
        color = COL_TEXT;
    }

    /* Strip leading spaces from the message for display */
    const char *text = message;
    while (*text == ' ') { text++; }

    stage_data_add_exec(active_stage_data, text, prefix_char, color, indent);
}

/*
 * Collect heredoc lines for all commands in an AST tree
 */
static int collect_heredocs_ast(AstNode *node) {
    if (!node) return 0;

    switch (node->type) {
    case NODE_COMMAND: {
        Command *cmd = node->data.command.cmd;
        if (!cmd->heredoc_delim) return 0;

        tui_suspend_raw();

        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            tui_resume_raw();
            return -1;
        }

        char linebuf[4096];
        while (1) {
            printf("heredoc> ");
            fflush(stdout);
            if (!fgets(linebuf, sizeof(linebuf), stdin)) break;

            int len = (int)strlen(linebuf);
            if (len > 0 && linebuf[len - 1] == '\n') {
                linebuf[len - 1] = '\0';
                len--;
            }
            if (strcmp(linebuf, cmd->heredoc_delim) == 0) break;

            linebuf[len] = '\n';
            linebuf[len + 1] = '\0';
            write(pipefd[1], linebuf, len + 1);
        }

        close(pipefd[1]);
        cmd->heredoc_fd = pipefd[0];

        tui_resume_raw();
        return 0;
    }
    case NODE_PIPELINE:
        for (int i = 0; i < node->data.pipeline.cmd_count; i++)
            if (collect_heredocs_ast(node->data.pipeline.cmds[i]) < 0) return -1;
        return 0;
    case NODE_LIST:
        for (int i = 0; i < node->data.list.count; i++)
            if (collect_heredocs_ast(node->data.list.entries[i].pipeline) < 0) return -1;
        return 0;
    case NODE_IF:
        if (collect_heredocs_ast(node->data.if_clause.condition) < 0) return -1;
        if (collect_heredocs_ast(node->data.if_clause.then_body) < 0) return -1;
        if (collect_heredocs_ast(node->data.if_clause.else_body) < 0) return -1;
        return 0;
    case NODE_WHILE:
    case NODE_UNTIL:
        if (collect_heredocs_ast(node->data.loop.condition) < 0) return -1;
        if (collect_heredocs_ast(node->data.loop.body) < 0) return -1;
        return 0;
    case NODE_FOR:
        if (collect_heredocs_ast(node->data.for_clause.body) < 0) return -1;
        return 0;
    case NODE_NOT:
        return collect_heredocs_ast(node->data.not_clause.child);
    case NODE_SUBSHELL:
        return collect_heredocs_ast(node->data.subshell.body);
    case NODE_FUNCTION_DEF:
        return collect_heredocs_ast(node->data.func_def.body);
    }
    return 0;
}

/*
 * Command substitution callback for $(...)
 */
static char *cmd_subst_callback(const char *cmd_str) {
    TokenList tokens;
    char lex_error[256] = "";
    if (lexer_tokenize(cmd_str, &tokens, lex_error, sizeof(lex_error)) < 0)
        return NULL;

    AstNode *ast = NULL;
    char parse_error[256] = "";
    ParseResult r = parser_parse_ast(&tokens, &ast, parse_error, sizeof(parse_error));
    tokenlist_free(&tokens);

    if (r != PARSE_OK || !ast) {
        ast_free(ast);
        return NULL;
    }

    char *output = malloc(8192);
    if (!output) { ast_free(ast); return NULL; }
    output[0] = '\0';

    executor_run_ast_capture(ast, output, 8192);
    ast_free(ast);
    return output;
}

/*
 * Capture expansions: snapshot argv before expand, diff after
 */
static void capture_expansions(StageData *sd, AstNode *ast) {
    if (!ast) return;

    switch (ast->type) {
    case NODE_COMMAND: {
        Command *cmd = ast->data.command.cmd;
        if (!cmd || cmd->argc == 0) return;

        /* Snapshot pre-expansion args */
        int orig_argc = cmd->argc;
        char **orig_argv = malloc(orig_argc * sizeof(char *));
        for (int i = 0; i < orig_argc; i++)
            orig_argv[i] = strdup(cmd->argv[i]);

        /* Expand */
        expand_command(cmd);

        /* Compare and record differences */
        /* Check original args against expanded ones */
        int new_i = 0;
        for (int i = 0; i < orig_argc && new_i < cmd->argc; i++) {
            const char *orig = orig_argv[i];

            /* Check for variable/tilde expansion */
            if (strchr(orig, '$') || strchr(orig, '~') ||
                (strstr(orig, "$((") != NULL)) {
                /* This word was expanded */
                const char *type = "variable expansion";
                if (orig[0] == '~') type = "tilde expansion";
                else if (strstr(orig, "$((")) type = "arithmetic expansion";
                else if (strstr(orig, "$(")) type = "command substitution";

                stage_data_add_expansion(sd, orig, cmd->argv[new_i], type);
                new_i++;
            } else if (!cmd->arg_quoted || !cmd->arg_quoted[i]) {
                /* Check for glob expansion (unquoted, might have expanded to multiple) */
                if (strchr(orig, '*') || strchr(orig, '?') || strchr(orig, '[')) {
                    /* Glob pattern - collect all expanded results */
                    char expanded[512] = "";
                    int start_i = new_i;
                    /* Count how many args this glob expanded to */
                    while (new_i < cmd->argc &&
                           (new_i == start_i ||
                            (i + 1 < orig_argc &&
                             strcmp(cmd->argv[new_i], orig_argv[i + 1]) != 0))) {
                        if (new_i > start_i) {
                            int elen = (int)strlen(expanded);
                            if (elen + 1 < 510)
                                strcat(expanded, "\n");
                        }
                        int elen = (int)strlen(expanded);
                        int avlen = (int)strlen(cmd->argv[new_i]);
                        if (elen + avlen < 510)
                            strcat(expanded, cmd->argv[new_i]);
                        new_i++;

                        /* Simple heuristic: if next orig matches, stop */
                        if (i + 1 < orig_argc && new_i < cmd->argc &&
                            strcmp(cmd->argv[new_i], orig_argv[i + 1]) == 0)
                            break;
                    }
                    if (strcmp(expanded, orig) != 0) {
                        stage_data_add_expansion(sd, orig, expanded,
                                                 "glob expansion");
                    }
                } else {
                    new_i++;
                }
            } else {
                new_i++;
            }
        }

        for (int i = 0; i < orig_argc; i++) free(orig_argv[i]);
        free(orig_argv);
        return;
    }
    case NODE_PIPELINE:
        for (int i = 0; i < ast->data.pipeline.cmd_count; i++)
            capture_expansions(sd, ast->data.pipeline.cmds[i]);
        return;
    case NODE_LIST:
        for (int i = 0; i < ast->data.list.count; i++)
            capture_expansions(sd, ast->data.list.entries[i].pipeline);
        return;
    case NODE_IF:
        capture_expansions(sd, ast->data.if_clause.condition);
        capture_expansions(sd, ast->data.if_clause.then_body);
        capture_expansions(sd, ast->data.if_clause.else_body);
        return;
    case NODE_WHILE:
    case NODE_UNTIL:
        capture_expansions(sd, ast->data.loop.condition);
        capture_expansions(sd, ast->data.loop.body);
        return;
    case NODE_FOR:
        capture_expansions(sd, ast->data.for_clause.body);
        return;
    case NODE_NOT:
        capture_expansions(sd, ast->data.not_clause.child);
        return;
    case NODE_SUBSHELL:
        capture_expansions(sd, ast->data.subshell.body);
        return;
    case NODE_FUNCTION_DEF:
        capture_expansions(sd, ast->data.func_def.body);
        return;
    }
}

/*
 * Expand history references: !! and !n
 */
static char *expand_history(const char *line) {
    if (line[0] != '!') return strdup(line);

    const char *ref = line + 1;
    const char *replacement = NULL;
    int count = tui_history_count();

    if (ref[0] == '!') {
        if (count == 0) return NULL;
        replacement = tui_history_get(count);
    } else {
        char *endptr;
        long n = strtol(ref, &endptr, 10);
        if (*endptr != '\0' || n < 1) return NULL;
        replacement = tui_history_get((int)n);
        if (!replacement) return NULL;
    }

    return strdup(replacement);
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\nOptions:\n");
    printf("  --anim-speed=SPEED   Animation speed: none, fast, normal, slow\n");
    printf("  --no-splash          Skip the welcome screen\n");
    printf("  --help               Show this help message\n");
    printf("\nshelli is an educational shell that visualizes how shells work.\n");
}

/*
 * Check if the AST is a single "exit" command
 */
static int is_exit_command(AstNode *ast) {
    if (!ast) return 0;
    if (ast->type == NODE_COMMAND) {
        Command *cmd = ast->data.command.cmd;
        return cmd && cmd->argc > 0 && strcmp(cmd->argv[0], "exit") == 0;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int show_splash = 1;
    AnimSpeed anim_speed = ANIM_SPEED_NORMAL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-splash") == 0) {
            show_splash = 0;
        } else if (strncmp(argv[i], "--anim-speed=", 13) == 0) {
            const char *val = argv[i] + 13;
            if (strcmp(val, "none") == 0) anim_speed = ANIM_SPEED_NONE;
            else if (strcmp(val, "fast") == 0) anim_speed = ANIM_SPEED_FAST;
            else if (strcmp(val, "normal") == 0) anim_speed = ANIM_SPEED_NORMAL;
            else if (strcmp(val, "slow") == 0) anim_speed = ANIM_SPEED_SLOW;
            else {
                fprintf(stderr, "Unknown animation speed: %s\n", val);
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (tui_init() < 0) {
        fprintf(stderr, "Failed to initialize TUI\n");
        return 1;
    }

    tui_history_load();
    suggest_init();
    suggest_build_freq_table();

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGQUIT, SIG_IGN);

    var_init();
    func_init();
    jobs_init();

    tui_set_anim_speed(anim_speed);
    executor_set_logger(exec_logger);
    expand_set_substitution_fn(cmd_subst_callback);
    expand_set_var_get_fn(var_get);

    /* Load ~/.shellirc if it exists */
    {
        const char *home = getenv("HOME");
        if (home) {
            char rcpath[4096];
            snprintf(rcpath, sizeof(rcpath), "%s/.shellirc", home);
            if (access(rcpath, R_OK) == 0) {
                FILE *rcf = fopen(rcpath, "r");
                if (rcf) {
                    char line[4096];
                    while (fgets(line, sizeof(line), rcf)) {
                        char *p = line;
                        while (*p == ' ' || *p == '\t') p++;
                        if (*p == '#' || *p == '\n' || *p == '\0') continue;
                        int len = (int)strlen(line);
                        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

                        TokenList rc_tokens;
                        char lex_err[256] = "";
                        if (lexer_tokenize(line, &rc_tokens, lex_err, sizeof(lex_err)) < 0) continue;
                        AstNode *rc_ast = NULL;
                        char pe[256] = "";
                        ParseResult r = parser_parse_ast(&rc_tokens, &rc_ast, pe, sizeof(pe));
                        tokenlist_free(&rc_tokens);
                        if (r == PARSE_OK && rc_ast) {
                            executor_run_ast(rc_ast);
                            ast_free(rc_ast);
                        } else {
                            ast_free(rc_ast);
                        }
                    }
                    fclose(rcf);
                }
            }
        }
    }

    /* Show welcome page */
    if (show_splash) {
        if (tui_welcome_page() != 0) {
            tui_history_save();
            tui_cleanup();
            return 0;
        }
    }

    int last_exit = 0;
    int should_exit = 0;

    /* Multi-line accumulation buffer */
    DynBuf accumulated;
    dynbuf_init(&accumulated);
    int multiline = 0;

    while (!should_exit) {
        interrupted = 0;
        jobs_check();

        char *line = tui_read_line();
        if (!line) {
            break; /* EOF / Ctrl+D */
        }

        if (line[0] == '\0') {
            free(line);
            if (multiline) {
                dynbuf_push(&accumulated, '\n');
                continue;
            }
            continue;
        }

        if (!multiline && line[0] == '!') {
            char *expanded = expand_history(line);
            free(line);
            if (!expanded) continue;
            line = expanded;
        }

        /* Accumulate input */
        if (multiline) dynbuf_push(&accumulated, '\n');
        dynbuf_append_str(&accumulated, line);
        free(line);

        /* Tokenize */
        TokenList tokens;
        char lex_error[256] = "";
        char *input_str = dynbuf_steal(&accumulated);

        if (lexer_tokenize(input_str, &tokens, lex_error, sizeof(lex_error)) < 0) {
            /* Could be unclosed quote in multiline */
            dynbuf_init(&accumulated);
            dynbuf_append_str(&accumulated, input_str);
            free(input_str);
            multiline = 1;
            continue;
        }

        /* Parse */
        AstNode *ast = NULL;
        char parse_err[256] = "";
        ParseResult result = parser_parse_ast(&tokens, &ast, parse_err, sizeof(parse_err));

        if (result == PARSE_INCOMPLETE) {
            tokenlist_free(&tokens);
            dynbuf_init(&accumulated);
            dynbuf_append_str(&accumulated, input_str);
            free(input_str);
            multiline = 1;
            continue;
        }

        /* Input is complete */
        multiline = 0;
        dynbuf_init(&accumulated);

        /* Initialize StageData */
        StageData sd;
        stage_data_init(&sd);
        stage_data_set_input(&sd, input_str);

        /* Populate tokens */
        expand_set_exit_code(last_exit);
        stage_data_populate_tokens(&sd, &tokens);

        if (result == PARSE_ERROR) {
            stage_data_set_error(&sd, parse_err[0] ? parse_err : "Parse error");
            tokenlist_free(&tokens);
            free(input_str);
            ast_free(ast);

            /* Show stage browser with error */
            int action = tui_stage_browser(&sd);
            if (action == 1) { should_exit = 1; }
            continue;
        }

        tokenlist_free(&tokens);

        if (!ast) {
            free(input_str);
            continue;
        }

        /* Populate AST display */
        stage_data_populate_ast(&sd, ast);

        /* Collect heredocs */
        if (collect_heredocs_ast(ast) < 0) {
            ast_free(ast);
            free(input_str);
            continue;
        }

        /* Check for exit */
        if (is_exit_command(ast)) {
            Command *cmd = ast->data.command.cmd;
            expand_command(cmd);
            int dummy;
            last_exit = builtin_execute(cmd, &dummy);
            should_exit = 1;

            stage_data_add_exec(&sd, "builtin: exit", '*', COL_PINK, 0);
            stage_data_set_output(&sd, "Goodbye!");
            stage_data_set_result(&sd, last_exit);

            tui_stage_browser(&sd);
            ast_free(ast);
            free(input_str);
            continue;
        }

        /* Record usage for suggestion frequency ranking */
        suggest_record_usage(input_str);

        /* Capture expansions (snapshot, expand, diff) */
        active_stage_data = &sd;
        capture_expansions(&sd, ast);

        /* Execute and capture output */
        char output_buf[4096] = "";
        last_exit = executor_run_ast_capture(ast, output_buf, sizeof(output_buf));
        expand_set_exit_code(last_exit);

        active_stage_data = NULL;

        /* Populate result */
        stage_data_set_output(&sd, output_buf[0] ? output_buf : NULL);
        stage_data_set_result(&sd, last_exit);

        /* Show stage browser */
        int action = tui_stage_browser(&sd);
        if (action == 1) {
            should_exit = 1;
        }

        ast_free(ast);
        free(input_str);
    }

    dynbuf_free(&accumulated);
    tui_history_save();
    suggest_cleanup();
    var_cleanup();
    func_cleanup();
    jobs_cleanup();
    tui_cleanup();

    return last_exit;
}
