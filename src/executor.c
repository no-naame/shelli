/*
 * shelli - Educational Shell
 * executor.c - Command execution with fork/exec/pipe/redirect
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "executor.h"
#include "builtins.h"

static ExecLogCallback log_callback = NULL;

void executor_set_logger(ExecLogCallback callback) {
    log_callback = callback;
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

static int setup_redirects(Command *cmd) {
    /* Input redirect */
    if (cmd->redir_in.type == REDIR_IN) {
        int fd = open(cmd->redir_in.filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "shelli: %s: %s\n",
                    cmd->redir_in.filename, strerror(errno));
            return -1;
        }
        log_msg("  redirect: stdin ◄── %s", cmd->redir_in.filename);
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    /* Output redirect */
    if (cmd->redir_out.type == REDIR_OUT) {
        int fd = open(cmd->redir_out.filename,
                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "shelli: %s: %s\n",
                    cmd->redir_out.filename, strerror(errno));
            return -1;
        }
        log_msg("  redirect: stdout ──► %s (truncate)", cmd->redir_out.filename);
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
        log_msg("  redirect: stdout ──► %s (append)", cmd->redir_out.filename);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    return 0;
}

static int execute_single(Command *cmd) {
    /* Check for built-in */
    if (builtin_is_builtin(cmd->argv[0])) {
        int should_exit = 0;
        log_msg("builtin: %s", cmd->argv[0]);
        return builtin_execute(cmd, &should_exit);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child process */
        if (setup_redirects(cmd) < 0) {
            _exit(1);
        }
        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "shelli: %s: %s\n", cmd->argv[0], strerror(errno));
        _exit(127);
    }

    /* Parent process */
    log_msg("fork() → pid %d (%s)", pid, cmd->argv[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static int execute_pipeline(Pipeline *pipeline) {
    int cmd_count = pipeline->cmd_count;

    if (cmd_count == 1) {
        return execute_single(pipeline->first);
    }

    /* Allocate pipes: we need (cmd_count - 1) pipes */
    int (*pipes)[2] = malloc((cmd_count - 1) * sizeof(int[2]));
    if (!pipes) {
        perror("malloc");
        return 1;
    }

    /* Create all pipes */
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            free(pipes);
            return 1;
        }
        log_msg("pipe() → fd[%d, %d]", pipes[i][0], pipes[i][1]);
    }

    /* Fork all children */
    pid_t *pids = malloc(cmd_count * sizeof(pid_t));
    if (!pids) {
        perror("malloc");
        free(pipes);
        return 1;
    }

    Command *cmd = pipeline->first;
    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            /* TODO: clean up properly */
            free(pipes);
            free(pids);
            return 1;
        }

        if (pids[i] == 0) {
            /* Child process */

            /* Connect to previous pipe (read end) */
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }

            /* Connect to next pipe (write end) */
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            /* Close all pipe fds */
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /* Apply redirects (may override pipe connections) */
            if (setup_redirects(cmd) < 0) {
                _exit(1);
            }

            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "shelli: %s: %s\n", cmd->argv[0], strerror(errno));
            _exit(127);
        }

        log_msg("fork() → pid %d (%s)", pids[i], cmd->argv[0]);
        cmd = cmd->next;
    }

    /* Parent: close all pipes */
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    /* Log pipe connections */
    cmd = pipeline->first;
    for (int i = 0; i < cmd_count - 1; i++) {
        log_msg("pipe: %d stdout ──► %d stdin", pids[i], pids[i+1]);
        cmd = cmd->next;
    }

    /* Wait for all children */
    int last_status = 0;
    for (int i = 0; i < cmd_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (i == cmd_count - 1 && WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        }
    }

    free(pipes);
    free(pids);

    return last_status;
}

int executor_run(Pipeline *pipeline) {
    if (!pipeline || !pipeline->first) {
        return 0;
    }
    return execute_pipeline(pipeline);
}
