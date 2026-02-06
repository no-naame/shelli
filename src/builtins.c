/*
 * shelli - Educational Shell
 * builtins.c - Built-in commands: cd, pwd, exit, help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "builtins.h"

static const char *builtins[] = {"cd", "pwd", "exit", "help", NULL};

static const char *help_text =
    "shelli - Educational Shell\n"
    "\n"
    "Built-in commands:\n"
    "  cd [dir]    Change directory (default: $HOME)\n"
    "  pwd         Print working directory\n"
    "  exit [n]    Exit shell with status n (default: 0)\n"
    "  help        Show this help message\n"
    "\n"
    "Features:\n"
    "  - Pipes: cmd1 | cmd2 | cmd3\n"
    "  - Redirects: cmd < in.txt, cmd > out.txt, cmd >> log.txt\n"
    "  - Quoting: 'single quotes', \"double quotes\"\n"
    "\n"
    "Debug mode:\n"
    "  Run with --debug to see step-by-step execution\n";

int builtin_is_builtin(const char *name) {
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(name, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int builtin_cd(Command *cmd) {
    const char *dir;

    if (cmd->argc < 2) {
        dir = getenv("HOME");
        if (!dir) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else {
        dir = cmd->argv[1];
    }

    if (chdir(dir) < 0) {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    return 0;
}

static int builtin_pwd(void) {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}

static int builtin_exit(Command *cmd, int *should_exit) {
    *should_exit = 1;
    if (cmd->argc >= 2) {
        return atoi(cmd->argv[1]);
    }
    return 0;
}

static int do_help(void) {
    printf("%s", help_text);
    return 0;
}

int builtin_execute(Command *cmd, int *should_exit) {
    *should_exit = 0;

    if (strcmp(cmd->argv[0], "cd") == 0) {
        return builtin_cd(cmd);
    } else if (strcmp(cmd->argv[0], "pwd") == 0) {
        return builtin_pwd();
    } else if (strcmp(cmd->argv[0], "exit") == 0) {
        return builtin_exit(cmd, should_exit);
    } else if (strcmp(cmd->argv[0], "help") == 0) {
        return do_help();
    }

    return 1;
}

const char *builtin_help(void) {
    return help_text;
}
