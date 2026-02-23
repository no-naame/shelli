/*
 * shelli - Educational Shell
 * builtins.c - All built-in commands
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "builtins.h"
#include "test_builtin.h"
#include "variables.h"
#include "functions.h"
#include "jobs.h"
#include "tui/tui.h"

static const char *builtins[] = {
    "cd", "pwd", "exit", "help", "export", "unset", "history",
    "test", "[", "echo", "type", "true", "false",
    "local", "return", "source", ".",
    "jobs", "fg", "bg",
    NULL
};

static const char *help_text =
    "shelli - Educational Shell\n"
    "\n"
    "Built-in commands:\n"
    "  cd [dir]         Change directory (default: $HOME)\n"
    "  pwd              Print working directory\n"
    "  exit [n]         Exit shell with status n (default: 0)\n"
    "  help             Show this help message\n"
    "  export VAR=val   Set environment variable\n"
    "  unset VAR        Remove environment variable\n"
    "  history          Show command history\n"
    "  test EXPR / [    Evaluate conditional expression\n"
    "  echo [-ne] args  Print arguments\n"
    "  type cmd         Show command type\n"
    "  true / false     Return 0 / 1\n"
    "  local VAR=val    Set local variable (in functions)\n"
    "  return [n]       Return from function\n"
    "  source file      Execute commands from file\n"
    "  jobs             List background jobs\n"
    "  fg [%n]          Bring job to foreground\n"
    "  bg [%n]          Continue job in background\n"
    "\n"
    "Control flow:\n"
    "  if COND; then BODY; [elif COND; then BODY;] [else BODY;] fi\n"
    "  while COND; do BODY; done\n"
    "  until COND; do BODY; done\n"
    "  for VAR in WORDS; do BODY; done\n"
    "\n"
    "Operators:\n"
    "  cmd1 | cmd2      Pipeline\n"
    "  cmd1 ; cmd2      Sequential execution\n"
    "  cmd1 && cmd2     Run cmd2 if cmd1 succeeds\n"
    "  cmd1 || cmd2     Run cmd2 if cmd1 fails\n"
    "  ! cmd            Invert exit code\n"
    "  cmd &            Run in background\n"
    "\n"
    "Features:\n"
    "  - Redirects:     < in, > out, >> append, << heredoc\n"
    "  - Quoting:       'single quotes', \"double quotes\"\n"
    "  - Variables:     $VAR, ${VAR}, $?, $#, $1-$9\n"
    "  - Arithmetic:    $((expr))\n"
    "  - Cmd subst:     $(command)\n"
    "  - Tilde:         ~/path\n"
    "  - Globbing:      *.c, file?.txt, [abc].txt\n"
    "  - Functions:     name() { body; }\n"
    "  - Tab:           Tab completion\n"
    "\n"
    "Test expressions:\n"
    "  -e/-f/-d file    File exists / is regular / is directory\n"
    "  -r/-w/-x file    File is readable / writable / executable\n"
    "  -n/-z str        String is non-empty / empty\n"
    "  s1 = s2          Strings equal\n"
    "  n1 -eq n2        Numeric equal (-ne, -lt, -le, -gt, -ge)\n"
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
        dir = var_get("HOME");
        if (!dir) dir = getenv("HOME");
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
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("export %s\n", *env);
        }
        return 0;
    }

    for (int i = 1; i < cmd->argc; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            char *name = strdup(cmd->argv[i]);
            name[eq - cmd->argv[i]] = '\0';
            const char *value = eq + 1;
            var_set(name, value);
            setenv(name, value, 1);
            free(name);
        } else {
            /* export existing variable */
            var_export(cmd->argv[i]);
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
        var_unset(cmd->argv[i]);
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

static int builtin_echo(Command *cmd) {
    int no_newline = 0;
    int interpret_escapes = 0;
    int arg_start = 1;

    /* Parse flags */
    while (arg_start < cmd->argc) {
        if (strcmp(cmd->argv[arg_start], "-n") == 0) {
            no_newline = 1;
            arg_start++;
        } else if (strcmp(cmd->argv[arg_start], "-e") == 0) {
            interpret_escapes = 1;
            arg_start++;
        } else if (strcmp(cmd->argv[arg_start], "-E") == 0) {
            interpret_escapes = 0;
            arg_start++;
        } else if (strcmp(cmd->argv[arg_start], "-ne") == 0 ||
                   strcmp(cmd->argv[arg_start], "-en") == 0) {
            no_newline = 1;
            interpret_escapes = 1;
            arg_start++;
        } else {
            break;
        }
    }

    for (int i = arg_start; i < cmd->argc; i++) {
        if (i > arg_start) putchar(' ');

        if (interpret_escapes) {
            const char *p = cmd->argv[i];
            while (*p) {
                if (*p == '\\' && *(p + 1)) {
                    p++;
                    switch (*p) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case '\\': putchar('\\'); break;
                    case 'a': putchar('\a'); break;
                    case 'b': putchar('\b'); break;
                    case 'f': putchar('\f'); break;
                    case 'v': putchar('\v'); break;
                    case '0': {
                        /* Octal */
                        int val = 0;
                        p++;
                        for (int j = 0; j < 3 && *p >= '0' && *p <= '7'; j++, p++) {
                            val = val * 8 + (*p - '0');
                        }
                        putchar(val);
                        continue; /* skip p++ */
                    }
                    default:
                        putchar('\\');
                        putchar(*p);
                        break;
                    }
                    p++;
                } else {
                    putchar(*p++);
                }
            }
        } else {
            fputs(cmd->argv[i], stdout);
        }
    }

    if (!no_newline) putchar('\n');
    fflush(stdout);
    return 0;
}

static int builtin_type(Command *cmd) {
    if (cmd->argc < 2) {
        fprintf(stderr, "type: missing argument\n");
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < cmd->argc; i++) {
        const char *name = cmd->argv[i];

        if (builtin_is_builtin(name)) {
            printf("%s is a shell builtin\n", name);
        } else if (func_is_function(name)) {
            printf("%s is a function\n", name);
        } else {
            /* Search PATH */
            const char *path = getenv("PATH");
            if (!path) path = "/usr/bin:/bin";

            char *pathdup = strdup(path);
            char *dir = strtok(pathdup, ":");
            int found = 0;

            while (dir) {
                char fullpath[4096];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, name);
                if (access(fullpath, X_OK) == 0) {
                    printf("%s is %s\n", name, fullpath);
                    found = 1;
                    break;
                }
                dir = strtok(NULL, ":");
            }
            free(pathdup);

            if (!found) {
                fprintf(stderr, "%s: not found\n", name);
                ret = 1;
            }
        }
    }
    return ret;
}

/* `local` builtin - handled in executor but registration needed */
static int builtin_local(Command *cmd) {
    for (int i = 1; i < cmd->argc; i++) {
        char *eq = strchr(cmd->argv[i], '=');
        if (eq) {
            char *name = strdup(cmd->argv[i]);
            name[eq - cmd->argv[i]] = '\0';
            var_set_local(name, eq + 1);
            free(name);
        } else {
            var_set_local(cmd->argv[i], "");
        }
    }
    return 0;
}

/* Source/. builtin - reads file and returns non-zero to signal caller */
static int builtin_source(Command *cmd) {
    if (cmd->argc < 2) {
        fprintf(stderr, "source: filename argument required\n");
        return 1;
    }
    /* Actual execution handled in executor which calls source_file() */
    return 0;
}

static int builtin_jobs(void) {
    jobs_print();
    return 0;
}

static int builtin_fg(Command *cmd) {
    int job_id;
    if (cmd->argc >= 2) {
        const char *arg = cmd->argv[1];
        if (arg[0] == '%') arg++;
        job_id = atoi(arg);
    } else {
        job_id = job_most_recent();
    }
    if (job_id <= 0) {
        fprintf(stderr, "fg: no current job\n");
        return 1;
    }
    return job_foreground(job_id);
}

static int builtin_bg(Command *cmd) {
    int job_id;
    if (cmd->argc >= 2) {
        const char *arg = cmd->argv[1];
        if (arg[0] == '%') arg++;
        job_id = atoi(arg);
    } else {
        job_id = job_most_recent();
    }
    if (job_id <= 0) {
        fprintf(stderr, "bg: no current job\n");
        return 1;
    }
    return job_background(job_id);
}

int builtin_execute(Command *cmd, int *should_exit) {
    *should_exit = 0;

    if (strcmp(cmd->argv[0], "cd") == 0) return builtin_cd(cmd);
    if (strcmp(cmd->argv[0], "pwd") == 0) return builtin_pwd();
    if (strcmp(cmd->argv[0], "exit") == 0) return builtin_exit(cmd, should_exit);
    if (strcmp(cmd->argv[0], "help") == 0) return do_help();
    if (strcmp(cmd->argv[0], "export") == 0) return builtin_export(cmd);
    if (strcmp(cmd->argv[0], "unset") == 0) return builtin_unset(cmd);
    if (strcmp(cmd->argv[0], "history") == 0) return builtin_history();
    if (strcmp(cmd->argv[0], "test") == 0 || strcmp(cmd->argv[0], "[") == 0)
        return test_builtin_execute(cmd->argc, cmd->argv);
    if (strcmp(cmd->argv[0], "echo") == 0) return builtin_echo(cmd);
    if (strcmp(cmd->argv[0], "type") == 0) return builtin_type(cmd);
    if (strcmp(cmd->argv[0], "true") == 0) return 0;
    if (strcmp(cmd->argv[0], "false") == 0) return 1;
    if (strcmp(cmd->argv[0], "local") == 0) return builtin_local(cmd);
    if (strcmp(cmd->argv[0], "return") == 0) return 0; /* handled in executor */
    if (strcmp(cmd->argv[0], "source") == 0 || strcmp(cmd->argv[0], ".") == 0)
        return builtin_source(cmd);
    if (strcmp(cmd->argv[0], "jobs") == 0) return builtin_jobs();
    if (strcmp(cmd->argv[0], "fg") == 0) return builtin_fg(cmd);
    if (strcmp(cmd->argv[0], "bg") == 0) return builtin_bg(cmd);

    return 1;
}

const char *builtin_help(void) {
    return help_text;
}
