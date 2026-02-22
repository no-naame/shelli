/*
 * shelli - Educational Shell
 * builtins.c - Built-in commands: cd, pwd, exit, help, export, unset
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "builtins.h"
#include "tui/tui.h"

static const char *builtins[] = {"cd", "pwd", "exit", "help", "export", "unset", "history", NULL};

static const char *help_text =
    "shelli - Educational Shell\n"
    "\n"
    "Built-in commands:\n"
    "  cd [dir]       Change directory (default: $HOME)\n"
    "  pwd            Print working directory\n"
    "  exit [n]       Exit shell with status n (default: 0)\n"
    "  help           Show this help message\n"
    "  export VAR=val Set environment variable\n"
    "  unset VAR      Remove environment variable\n"
    "  history        Show command history\n"
    "\n"
    "History expansion:\n"
    "  !!             Repeat last command\n"
    "  !n             Repeat command number n\n"
    "\n"
    "Features:\n"
    "  - Pipes:       cmd1 | cmd2 | cmd3\n"
    "  - Redirects:   cmd < in.txt, cmd > out.txt, cmd >> log.txt\n"
    "  - Quoting:     'single quotes', \"double quotes\"\n"
    "  - Variables:   $VAR, ${VAR}, $? (last exit code)\n"
    "  - Tilde:       ~/path expands to $HOME/path\n"
    "  - Globbing:    *.c, file?.txt, [abc].txt\n"
    "  - Semicolons:  cmd1 ; cmd2 (sequential execution)\n"
    "  - Logic:       cmd1 && cmd2, cmd1 || cmd2\n"
    "  - Tab:         Tab completion for commands and files\n"
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

static int builtin_export(Command *cmd) {
    if (cmd->argc < 2) {
        /* No args: print all environment variables */
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("export %s\n", *env);
        }
        return 0;
    }

    for (int i = 1; i < cmd->argc; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            /* export VAR=value - split at = */
            char *name = strdup(cmd->argv[i]);
            name[eq - cmd->argv[i]] = '\0';
            const char *value = eq + 1;
            if (setenv(name, value, 1) < 0) {
                fprintf(stderr, "export: %s: %s\n", name, strerror(errno));
                free(name);
                return 1;
            }
            free(name);
        }
    }
    return 0;
}

static int builtin_unset(Command *cmd) {
    if (cmd->argc < 2) {
        fprintf(stderr, "unset: not enough arguments\n");
        return 1;
    }

    for (int i = 1; i < cmd->argc; i++) {
        if (unsetenv(cmd->argv[i]) < 0) {
            fprintf(stderr, "unset: %s: %s\n", cmd->argv[i], strerror(errno));
            return 1;
        }
    }
    return 0;
}

static int do_help(void) {
    printf("%s", help_text);
    return 0;
}

static int builtin_history(void) {
    tui_history_print();
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
    } else if (strcmp(cmd->argv[0], "export") == 0) {
        return builtin_export(cmd);
    } else if (strcmp(cmd->argv[0], "unset") == 0) {
        return builtin_unset(cmd);
    } else if (strcmp(cmd->argv[0], "history") == 0) {
        return builtin_history();
    }

    return 1;
}

const char *builtin_help(void) {
    return help_text;
}
