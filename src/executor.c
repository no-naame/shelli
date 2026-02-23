/*
 * shelli - Educational Shell
 * executor.c - AST tree-walking executor with fork/exec/pipe/redirect
 *
 * Walks the AST produced by the parser. Word expansion (tilde, variable,
 * glob) happens per-command at execution time via expand_command().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include "executor.h"
#include "builtins.h"
#include "expand.h"
#include "variables.h"
#include "functions.h"
#include "jobs.h"
#include "lexer.h"

static ExecLogCallback log_callback = NULL;

/* Foreground child PID for SIGINT forwarding */
static volatile pid_t fg_child_pid = 0;

/* Break/continue/return state for loops and functions */
int shell_break_count = 0;
int shell_continue_count = 0;
static int shell_return_flag = 0;
static int shell_return_value = 0;

/* Capture mode state: when non-NULL, last command's stdout goes here */
static char *capture_buf = NULL;
static int capture_size = 0;

void executor_set_logger(ExecLogCallback callback) {
    log_callback = callback;
}

pid_t executor_get_fg_pid(void) {
    return fg_child_pid;
}

static void log_msg(const char *fmt, ...) {
    if (!log_callback) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log_callback(buf);
}

/* ================================================================
 * Redirect and exit status helpers (unchanged from old executor)
 * ================================================================ */

static int setup_redirects(Command *cmd) {
    if (cmd->heredoc_fd >= 0) {
        log_msg("  redirect: stdin <<heredoc (fd %d)", cmd->heredoc_fd);
        dup2(cmd->heredoc_fd, STDIN_FILENO);
        close(cmd->heredoc_fd);
        cmd->heredoc_fd = -1;
    }
    if (cmd->redir_in.type == REDIR_IN) {
        int fd = open(cmd->redir_in.filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "shelli: %s: %s\n",
                    cmd->redir_in.filename, strerror(errno));
            return -1;
        }
        log_msg("  redirect: stdin < %s", cmd->redir_in.filename);
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (cmd->redir_out.type == REDIR_OUT) {
        int fd = open(cmd->redir_out.filename,
                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "shelli: %s: %s\n",
                    cmd->redir_out.filename, strerror(errno));
            return -1;
        }
        log_msg("  redirect: stdout > %s", cmd->redir_out.filename);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    } else if (cmd->redir_out.type == REDIR_APPEND) {
        int fd = open(cmd->redir_out.filename,
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            fprintf(stderr, "shelli: %s: %s\n",
                    cmd->redir_out.filename, strerror(errno));
            return -1;
        }
        log_msg("  redirect: stdout >> %s", cmd->redir_out.filename);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return 0;
}

static int exit_status(int status) {
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

/* Forward declaration */
static int execute_node(AstNode *node);

/* ================================================================
 * execute_simple_command: expand, then fork/exec or builtin
 * ================================================================ */

/* Clone a Command for expansion (avoids modifying AST) */
static Command *clone_cmd_for_exec(Command *src) {
    if (!src) return NULL;
    Command *dst = command_new();
    if (!dst) return NULL;
    for (int i = 0; i < src->argc; i++) {
        command_add_arg(dst, src->argv[i], src->arg_quoted ? src->arg_quoted[i] : 0);
    }
    if (src->redir_in.filename) {
        dst->redir_in.type = src->redir_in.type;
        dst->redir_in.filename = strdup(src->redir_in.filename);
    }
    if (src->redir_out.filename) {
        dst->redir_out.type = src->redir_out.type;
        dst->redir_out.filename = strdup(src->redir_out.filename);
    }
    if (src->heredoc_delim) {
        dst->heredoc_delim = strdup(src->heredoc_delim);
    }
    dst->heredoc_fd = src->heredoc_fd;
    src->heredoc_fd = -1; /* Transfer ownership */
    return dst;
}

/* Source a file: read, tokenize, parse, execute each line */
static int source_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "source: %s: %s\n", path, strerror(errno));
        return 1;
    }

    char line[4096];
    int last_exit = 0;
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        /* Strip trailing newline */
        int len = (int)strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        TokenList tokens;
        char lex_err[256] = "";
        if (lexer_tokenize(line, &tokens, lex_err, sizeof(lex_err)) < 0) continue;

        AstNode *ast = NULL;
        char parse_err[256] = "";
        ParseResult r = parser_parse_ast(&tokens, &ast, parse_err, sizeof(parse_err));
        tokenlist_free(&tokens);
        if (r != PARSE_OK || !ast) { ast_free(ast); continue; }

        last_exit = execute_node(ast);
        expand_set_exit_code(last_exit);
        ast_free(ast);
    }

    fclose(f);
    return last_exit;
}

static int execute_simple_command(Command *orig_cmd, char *output, int output_size) {
    if (!orig_cmd || !orig_cmd->argv || !orig_cmd->argv[0]) return 0;

    /* Check for VAR=value assignment (no command name) */
    if (strchr(orig_cmd->argv[0], '=') && orig_cmd->argc == 1) {
        char *eq = strchr(orig_cmd->argv[0], '=');
        char *name = strdup(orig_cmd->argv[0]);
        name[eq - orig_cmd->argv[0]] = '\0';
        /* Expand the value */
        char *val = expand_word(eq + 1, 0);
        var_set(name, val ? val : "");
        free(val);
        free(name);
        return 0;
    }

    /* Clone command so expansion doesn't modify the AST */
    Command *cmd = clone_cmd_for_exec(orig_cmd);
    if (!cmd) return 1;

    /* Expand words at execution time */
    expand_command(cmd);

    if (!cmd->argv[0]) { command_free(cmd); return 0; }

    int is_builtin = builtin_is_builtin(cmd->argv[0]);

    /* Builtins that must run in parent process */
    if (is_builtin && (strcmp(cmd->argv[0], "cd") == 0 ||
                       strcmp(cmd->argv[0], "export") == 0 ||
                       strcmp(cmd->argv[0], "unset") == 0 ||
                       strcmp(cmd->argv[0], "local") == 0)) {
        int should_exit = 0;
        log_msg("builtin: %s", cmd->argv[0]);
        int ret = builtin_execute(cmd, &should_exit);
        if (output && output_size > 0) output[0] = '\0';
        command_free(cmd);
        return ret;
    }

    /* Check for break/continue builtins */
    if (strcmp(cmd->argv[0], "break") == 0) {
        shell_break_count = cmd->argc > 1 ? atoi(cmd->argv[1]) : 1;
        if (shell_break_count < 1) shell_break_count = 1;
        command_free(cmd);
        return 0;
    }
    if (strcmp(cmd->argv[0], "continue") == 0) {
        shell_continue_count = cmd->argc > 1 ? atoi(cmd->argv[1]) : 1;
        if (shell_continue_count < 1) shell_continue_count = 1;
        command_free(cmd);
        return 0;
    }

    /* return builtin */
    if (strcmp(cmd->argv[0], "return") == 0) {
        shell_return_flag = 1;
        shell_return_value = cmd->argc > 1 ? atoi(cmd->argv[1]) : 0;
        command_free(cmd);
        return shell_return_value;
    }

    /* source/. builtin */
    if (strcmp(cmd->argv[0], "source") == 0 || strcmp(cmd->argv[0], ".") == 0) {
        if (cmd->argc < 2) {
            fprintf(stderr, "%s: filename argument required\n", cmd->argv[0]);
            command_free(cmd);
            return 1;
        }
        log_msg("source: %s", cmd->argv[1]);
        int ret = source_file(cmd->argv[1]);
        command_free(cmd);
        return ret;
    }

    /* Check function table */
    AstNode *func_body = func_lookup(cmd->argv[0]);
    if (func_body) {
        log_msg("function: %s", cmd->argv[0]);
        var_push_scope();
        /* Set positional parameters from function args */
        var_set_positional(cmd->argc - 1, cmd->argv + 1);
        int ret = execute_node(func_body);
        if (shell_return_flag) {
            ret = shell_return_value;
            shell_return_flag = 0;
        }
        var_pop_scope();
        command_free(cmd);
        return ret;
    }

    /* Non-capturing, non-piped builtin: run in parent */
    if (!output && is_builtin) {
        int should_exit = 0;
        log_msg("builtin: %s", cmd->argv[0]);
        int ret = builtin_execute(cmd, &should_exit);
        command_free(cmd);
        return ret;
    }

    /* Create capture pipe if needed */
    int capture_pipe[2] = {-1, -1};
    if (output && output_size > 0) {
        if (pipe(capture_pipe) < 0) {
            perror("pipe");
            command_free(cmd);
            return 1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        if (capture_pipe[0] >= 0) { close(capture_pipe[0]); close(capture_pipe[1]); }
        command_free(cmd);
        return 1;
    }

    if (pid == 0) {
        /* Child */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (capture_pipe[0] >= 0) {
            close(capture_pipe[0]);
            if (cmd->redir_out.type == 0) {
                dup2(capture_pipe[1], STDOUT_FILENO);
            }
            close(capture_pipe[1]);
        }

        if (setup_redirects(cmd) < 0) _exit(1);

        if (is_builtin) {
            int should_exit = 0;
            int ret = builtin_execute(cmd, &should_exit);
            _exit(ret);
        } else {
            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "shelli: %s: %s\n", cmd->argv[0], strerror(errno));
            _exit(127);
        }
    }

    /* Parent */
    fg_child_pid = pid;

    if (is_builtin) {
        log_msg("builtin: %s", cmd->argv[0]);
    } else {
        log_msg("fork() -> pid %d (%s)", pid, cmd->argv[0]);
    }

    if (capture_pipe[1] >= 0) close(capture_pipe[1]);

    if (output && output_size > 0 && capture_pipe[0] >= 0) {
        int total_read = 0;
        int bytes_left = output_size - 1;
        while (bytes_left > 0) {
            int n = read(capture_pipe[0], output + total_read, bytes_left);
            if (n <= 0) break;
            total_read += n;
            bytes_left -= n;
        }
        output[total_read] = '\0';
        while (total_read > 0 && (output[total_read - 1] == '\n' || output[total_read - 1] == '\r')) {
            output[--total_read] = '\0';
        }
        close(capture_pipe[0]);
    }

    int status;
    waitpid(pid, &status, 0);
    fg_child_pid = 0;

    command_free(cmd);
    return exit_status(status);
}

/* ================================================================
 * execute_pipeline_node: pipe N commands together
 * ================================================================ */

static int execute_pipeline_node(AstNode *node) {
    int cmd_count = node->data.pipeline.cmd_count;

    /* For each command in pipeline, expand and fork */
    if (cmd_count == 1) {
        AstNode *c = node->data.pipeline.cmds[0];
        if (c->type == NODE_COMMAND) {
            int result = execute_simple_command(c->data.command.cmd,
                                                capture_buf, capture_size);
            if (node->data.pipeline.negated) result = (result == 0) ? 1 : 0;
            return result;
        }
        /* Compound command in pipeline: execute recursively */
        int result = execute_node(c);
        if (node->data.pipeline.negated) result = (result == 0) ? 1 : 0;
        return result;
    }

    /* Multi-command pipeline */
    int (*pipes)[2] = malloc((cmd_count - 1) * sizeof(int[2]));
    if (!pipes) { perror("malloc"); return 1; }

    /* Capture pipe for last command */
    int cap_pipe[2] = {-1, -1};
    if (capture_buf && capture_size > 0) {
        if (pipe(cap_pipe) < 0) { perror("pipe"); free(pipes); return 1; }
    }

    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            if (cap_pipe[0] >= 0) { close(cap_pipe[0]); close(cap_pipe[1]); }
            free(pipes);
            return 1;
        }
    }

    pid_t *pids = malloc(cmd_count * sizeof(pid_t));
    if (!pids) {
        perror("malloc");
        for (int i = 0; i < cmd_count - 1; i++) { close(pipes[i][0]); close(pipes[i][1]); }
        if (cap_pipe[0] >= 0) { close(cap_pipe[0]); close(cap_pipe[1]); }
        free(pipes);
        return 1;
    }

    for (int i = 0; i < cmd_count; i++) {
        AstNode *c = node->data.pipeline.cmds[i];

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            for (int j = 0; j < i; j++) kill(pids[j], SIGTERM);
            for (int j = 0; j < cmd_count - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            if (cap_pipe[0] >= 0) { close(cap_pipe[0]); close(cap_pipe[1]); }
            for (int j = 0; j < i; j++) { int d; waitpid(pids[j], &d, 0); }
            free(pipes); free(pids);
            return 1;
        }

        if (pids[i] == 0) {
            /* Child */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            } else if (cap_pipe[0] >= 0) {
                Command *cmd = (c->type == NODE_COMMAND) ? c->data.command.cmd : NULL;
                if (!cmd || cmd->redir_out.type == 0) {
                    dup2(cap_pipe[1], STDOUT_FILENO);
                }
            }

            for (int j = 0; j < cmd_count - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            if (cap_pipe[0] >= 0) { close(cap_pipe[0]); close(cap_pipe[1]); }

            if (c->type == NODE_COMMAND) {
                Command *cmd = c->data.command.cmd;
                expand_command(cmd); /* Safe: we're in a child process */
                if (setup_redirects(cmd) < 0) _exit(1);

                if (builtin_is_builtin(cmd->argv[0])) {
                    int should_exit = 0;
                    int ret = builtin_execute(cmd, &should_exit);
                    _exit(ret);
                } else {
                    execvp(cmd->argv[0], cmd->argv);
                    fprintf(stderr, "shelli: %s: %s\n", cmd->argv[0], strerror(errno));
                    _exit(127);
                }
            } else {
                /* Compound command in pipeline child */
                int ret = execute_node(c);
                _exit(ret);
            }
        }

        if (c->type == NODE_COMMAND) {
            log_msg("fork() -> pid %d (%s)", pids[i], c->data.command.cmd->argv[0]);
        }
    }

    /* Parent: close all pipes */
    for (int i = 0; i < cmd_count - 1; i++) { close(pipes[i][0]); close(pipes[i][1]); }
    if (cap_pipe[1] >= 0) close(cap_pipe[1]);

    /* Read captured output */
    if (capture_buf && capture_size > 0 && cap_pipe[0] >= 0) {
        int total_read = 0;
        int bytes_left = capture_size - 1;
        while (bytes_left > 0) {
            int n = read(cap_pipe[0], capture_buf + total_read, bytes_left);
            if (n <= 0) break;
            total_read += n;
            bytes_left -= n;
        }
        capture_buf[total_read] = '\0';
        while (total_read > 0 && (capture_buf[total_read - 1] == '\n' || capture_buf[total_read - 1] == '\r'))
            capture_buf[--total_read] = '\0';
        close(cap_pipe[0]);
    }

    /* Wait for all children */
    int last_status = 0;
    for (int i = 0; i < cmd_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == cmd_count - 1) last_status = exit_status(status);
    }

    free(pipes);
    free(pids);

    if (node->data.pipeline.negated) last_status = (last_status == 0) ? 1 : 0;
    return last_status;
}

/* ================================================================
 * execute_list_node: iterate entries with ;/&&/||/& chaining
 * ================================================================ */

static int execute_list_node(AstNode *node) {
    int last_exit = 0;

    for (int i = 0; i < node->data.list.count; i++) {
        ListEntry *entry = &node->data.list.entries[i];

        /* Check chaining condition from PREVIOUS separator */
        if (i > 0) {
            ListSepType prev_sep = node->data.list.entries[i - 1].sep;
            if (prev_sep == LIST_SEP_AND && last_exit != 0) {
                log_msg("&& skip (previous failed)");
                continue;
            }
            if (prev_sep == LIST_SEP_OR && last_exit == 0) {
                log_msg("|| skip (previous succeeded)");
                continue;
            }
        }

        last_exit = execute_node(entry->pipeline);
        expand_set_exit_code(last_exit);

        if (shell_break_count || shell_continue_count || shell_return_flag) break;

        /* Background execution */
        if (entry->sep == LIST_SEP_BG) {
            log_msg("& background");
        }
    }

    return last_exit;
}

/* ================================================================
 * execute_if_node
 * ================================================================ */

static int execute_if_node(AstNode *node) {
    int cond = execute_node(node->data.if_clause.condition);
    expand_set_exit_code(cond);

    if (cond == 0) {
        if (node->data.if_clause.then_body) {
            return execute_node(node->data.if_clause.then_body);
        }
        return 0;
    } else {
        if (node->data.if_clause.else_body) {
            return execute_node(node->data.if_clause.else_body);
        }
        return cond;
    }
}

/* ================================================================
 * execute_while_node
 * ================================================================ */

static int execute_while_node(AstNode *node) {
    int last_exit = 0;

    while (1) {
        int cond = execute_node(node->data.loop.condition);
        expand_set_exit_code(cond);
        if (cond != 0) break;

        if (node->data.loop.body) {
            last_exit = execute_node(node->data.loop.body);
            expand_set_exit_code(last_exit);
        }

        if (shell_break_count > 0) {
            shell_break_count--;
            break;
        }
        if (shell_continue_count > 0) {
            shell_continue_count--;
            continue;
        }
    }

    return last_exit;
}

/* ================================================================
 * execute_until_node
 * ================================================================ */

static int execute_until_node(AstNode *node) {
    int last_exit = 0;

    while (1) {
        int cond = execute_node(node->data.loop.condition);
        expand_set_exit_code(cond);
        if (cond == 0) break;

        if (node->data.loop.body) {
            last_exit = execute_node(node->data.loop.body);
            expand_set_exit_code(last_exit);
        }

        if (shell_break_count > 0) {
            shell_break_count--;
            break;
        }
        if (shell_continue_count > 0) {
            shell_continue_count--;
            continue;
        }
    }

    return last_exit;
}

/* ================================================================
 * execute_for_node
 * ================================================================ */

static int execute_for_node(AstNode *node) {
    int last_exit = 0;

    for (int i = 0; i < node->data.for_clause.word_count; i++) {
        /* Expand the word at iteration time */
        char *expanded = expand_word(node->data.for_clause.words[i], 0);
        if (!expanded) continue;

        var_set(node->data.for_clause.var_name, expanded);
        setenv(node->data.for_clause.var_name, expanded, 1);
        free(expanded);

        if (node->data.for_clause.body) {
            last_exit = execute_node(node->data.for_clause.body);
            expand_set_exit_code(last_exit);
        }

        if (shell_break_count > 0) {
            shell_break_count--;
            break;
        }
        if (shell_continue_count > 0) {
            shell_continue_count--;
            continue;
        }
    }

    return last_exit;
}

/* ================================================================
 * execute_not_node
 * ================================================================ */

static int execute_not_node(AstNode *node) {
    int result = execute_node(node->data.not_clause.child);
    return (result == 0) ? 1 : 0;
}

/* ================================================================
 * execute_subshell_node
 * ================================================================ */

static int execute_subshell_node(AstNode *node) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        int ret = execute_node(node->data.subshell.body);
        _exit(ret);
    }

    fg_child_pid = pid;
    int status;
    waitpid(pid, &status, 0);
    fg_child_pid = 0;
    return exit_status(status);
}

/* ================================================================
 * execute_node: dispatch on node type
 * ================================================================ */

static int execute_node(AstNode *node) {
    if (!node) return 0;

    switch (node->type) {
    case NODE_COMMAND:
        return execute_simple_command(node->data.command.cmd,
                                     capture_buf, capture_size);

    case NODE_PIPELINE:
        return execute_pipeline_node(node);

    case NODE_LIST:
        return execute_list_node(node);

    case NODE_IF:
        return execute_if_node(node);

    case NODE_WHILE:
        return execute_while_node(node);

    case NODE_UNTIL:
        return execute_until_node(node);

    case NODE_FOR:
        return execute_for_node(node);

    case NODE_NOT:
        return execute_not_node(node);

    case NODE_SUBSHELL:
        return execute_subshell_node(node);

    case NODE_FUNCTION_DEF:
        log_msg("function def: %s", node->data.func_def.name);
        func_define(node->data.func_def.name, node->data.func_def.body);
        return 0;
    }

    return 1;
}

/* ================================================================
 * Public API
 * ================================================================ */

int executor_run_ast(AstNode *node) {
    if (!node) return 0;
    capture_buf = NULL;
    capture_size = 0;
    return execute_node(node);
}

int executor_run_ast_capture(AstNode *node, char *output, int output_size) {
    if (!node) {
        if (output && output_size > 0) output[0] = '\0';
        return 0;
    }
    if (!output || output_size <= 0) {
        return executor_run_ast(node);
    }
    output[0] = '\0';

    /* For simple single commands, use the fast path (capture_buf) */
    if (node->type == NODE_COMMAND) {
        capture_buf = output;
        capture_size = output_size;
        int ret = execute_node(node);
        capture_buf = NULL;
        capture_size = 0;
        return ret;
    }

    /* For compound commands (list, pipeline, if, for, while, etc.),
     * fork the entire execution and capture the child's stdout. */
    int cap_pipe[2];
    if (pipe(cap_pipe) < 0) {
        perror("pipe");
        return executor_run_ast(node);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(cap_pipe[0]);
        close(cap_pipe[1]);
        return executor_run_ast(node);
    }

    if (pid == 0) {
        /* Child: redirect stdout to pipe, run AST */
        close(cap_pipe[0]);
        dup2(cap_pipe[1], STDOUT_FILENO);
        close(cap_pipe[1]);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        int ret = execute_node(node);
        _exit(ret);
    }

    /* Parent: read captured output */
    close(cap_pipe[1]);
    int total_read = 0;
    int bytes_left = output_size - 1;
    while (bytes_left > 0) {
        int n = read(cap_pipe[0], output + total_read, bytes_left);
        if (n <= 0) break;
        total_read += n;
        bytes_left -= n;
    }
    output[total_read] = '\0';
    /* Strip trailing newlines */
    while (total_read > 0 && (output[total_read - 1] == '\n' || output[total_read - 1] == '\r'))
        output[--total_read] = '\0';
    close(cap_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    return exit_status(status);
}
