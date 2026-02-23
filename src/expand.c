/*
 * shelli - Educational Shell
 * expand.c - Word expansion: tilde, environment variables, arithmetic, globbing
 *
 * Expansion order (matching real shells):
 *   1. Tilde expansion       (~  -> /home/user)
 *   2. Variable expansion    ($VAR -> value)
 *   3. Arithmetic expansion  $(( expr )) -> numeric result
 *   4. Command substitution  $(cmd) -> output (via callback)
 *   5. Glob expansion        (*.c  -> file1.c file2.c) - skipped for quoted args
 *
 * Expansion happens per-command at execution time, not before parsing.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <glob.h>
#include <stdio.h>
#include "expand.h"
#include "util.h"

#define MAX_ARGS 256

static int last_exit_code = 0;

/* Command substitution callback (set by main.c to avoid circular dependency) */
static char *(*cmd_subst_fn)(const char *cmd) = NULL;

void expand_set_exit_code(int code) {
    last_exit_code = code;
}

void expand_set_substitution_fn(char *(*fn)(const char *cmd)) {
    cmd_subst_fn = fn;
}

/* Forward declaration for var_get */
static const char *shell_var_get(const char *name);

/* Variable getter - wraps getenv, updated for shell variables */
static const char *(*var_get_fn)(const char *name) = NULL;

static const char *shell_var_get(const char *name) {
    if (var_get_fn) return var_get_fn(name);
    return getenv(name);
}

void expand_set_var_get_fn(const char *(*fn)(const char *name)) {
    var_get_fn = fn;
}

/*
 * ============================================================================
 * Arithmetic expression evaluator for $(( expr ))
 * ============================================================================
 */

typedef struct {
    const char *p;
    int error;
} ArithState;

static long arith_expr(ArithState *s);

static void arith_skip_ws(ArithState *s) {
    while (*s->p == ' ' || *s->p == '\t') s->p++;
}

static long arith_primary(ArithState *s) {
    arith_skip_ws(s);

    if (*s->p == '(') {
        s->p++;
        long val = arith_expr(s);
        arith_skip_ws(s);
        if (*s->p == ')') {
            s->p++;
        } else {
            s->error = 1;
        }
        return val;
    }

    /* Variable reference: $NAME or bare NAME */
    const char *var_start = NULL;
    if (*s->p == '$') {
        s->p++;
        var_start = s->p;
    } else if (isalpha((unsigned char)*s->p) || *s->p == '_') {
        var_start = s->p;
    }

    if (var_start) {
        while (isalnum((unsigned char)*s->p) || *s->p == '_') s->p++;
        int namelen = (int)(s->p - var_start);
        char name[256];
        if (namelen > 0 && namelen < (int)sizeof(name)) {
            memcpy(name, var_start, namelen);
            name[namelen] = '\0';
            const char *val = shell_var_get(name);
            if (val) {
                char *end;
                long n = strtol(val, &end, 10);
                if (*end == '\0') return n;
            }
        }
        return 0;
    }

    /* Number */
    if (isdigit((unsigned char)*s->p)) {
        char *end;
        long val = strtol(s->p, &end, 0);
        s->p = end;
        return val;
    }

    s->error = 1;
    return 0;
}

static long arith_unary(ArithState *s) {
    arith_skip_ws(s);
    if (*s->p == '-') { s->p++; return -arith_unary(s); }
    if (*s->p == '+') { s->p++; return arith_unary(s); }
    if (*s->p == '~') { s->p++; return ~arith_unary(s); }
    if (*s->p == '!') { s->p++; return !arith_unary(s); }
    return arith_primary(s);
}

static long arith_power(ArithState *s) {
    long base = arith_unary(s);
    arith_skip_ws(s);
    if (s->p[0] == '*' && s->p[1] == '*') {
        s->p += 2;
        long exp = arith_power(s);
        if (exp < 0) { s->error = 1; return 0; }
        long result = 1;
        for (long i = 0; i < exp; i++) result *= base;
        return result;
    }
    return base;
}

static long arith_multiplicative(ArithState *s) {
    long left = arith_power(s);
    while (!s->error) {
        arith_skip_ws(s);
        if (*s->p == '*' && s->p[1] != '*') {
            s->p++;
            left *= arith_power(s);
        } else if (*s->p == '/') {
            s->p++;
            long right = arith_power(s);
            if (right == 0) { s->error = 1; return 0; }
            left /= right;
        } else if (*s->p == '%') {
            s->p++;
            long right = arith_power(s);
            if (right == 0) { s->error = 1; return 0; }
            left %= right;
        } else {
            break;
        }
    }
    return left;
}

static long arith_additive(ArithState *s) {
    long left = arith_multiplicative(s);
    while (!s->error) {
        arith_skip_ws(s);
        if (*s->p == '+') {
            s->p++;
            left += arith_multiplicative(s);
        } else if (*s->p == '-') {
            s->p++;
            left -= arith_multiplicative(s);
        } else {
            break;
        }
    }
    return left;
}

static long arith_expr(ArithState *s) {
    return arith_additive(s);
}

static long arith_evaluate(const char *expr, int *ok) {
    ArithState s;
    s.p = expr;
    s.error = 0;
    long result = arith_expr(&s);
    arith_skip_ws(&s);
    if (s.error || (*s.p != '\0')) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return result;
}

/*
 * Tilde expansion: replace leading ~ with $HOME
 */
static char *expand_tilde(const char *word) {
    if (word[0] != '~') return NULL;
    if (word[1] != '\0' && word[1] != '/') return NULL;

    const char *home = shell_var_get("HOME");
    if (!home) return NULL;

    size_t hlen = strlen(home);
    size_t wlen = strlen(word + 1);
    char *result = malloc(hlen + wlen + 1);
    if (!result) return NULL;

    strcpy(result, home);
    strcat(result, word + 1);
    return result;
}

/*
 * Variable, arithmetic, and command substitution expansion.
 */
static char *expand_variables(const char *word) {
    if (!strchr(word, '$')) return NULL;

    DynBuf result;
    dynbuf_init(&result);
    const char *p = word;

    while (*p) {
        if (*p == '$') {
            p++;
            if (*p == '(') {
                if (*(p + 1) == '(') {
                    /* $(( expr )) */
                    p += 2;
                    const char *expr_start = p;
                    int depth = 1;
                    while (*p && depth > 0) {
                        if (*p == '(') depth++;
                        else if (*p == ')') {
                            depth--;
                            if (depth == 0) break;
                        }
                        p++;
                    }
                    int exprlen = (int)(p - expr_start);
                    char *expr = malloc(exprlen + 1);
                    if (expr) {
                        memcpy(expr, expr_start, exprlen);
                        expr[exprlen] = '\0';
                        int ok = 0;
                        long val = arith_evaluate(expr, &ok);
                        if (ok) {
                            char num[32];
                            snprintf(num, sizeof(num), "%ld", val);
                            dynbuf_append_str(&result, num);
                        } else {
                            fprintf(stderr, "shelli: arithmetic error: %s\n", expr);
                        }
                        free(expr);
                    }
                    if (*p == ')') p++;
                    if (*p == ')') p++;
                } else {
                    /* $( cmd ) */
                    p++;
                    const char *cmd_start = p;
                    int depth = 1;
                    while (*p && depth > 0) {
                        if (*p == '(') depth++;
                        else if (*p == ')') depth--;
                        if (depth > 0) p++;
                    }
                    int cmdlen = (int)(p - cmd_start);
                    if (*p == ')') p++;

                    if (cmd_subst_fn) {
                        char *cmd = malloc(cmdlen + 1);
                        if (cmd) {
                            memcpy(cmd, cmd_start, cmdlen);
                            cmd[cmdlen] = '\0';
                            char *output = cmd_subst_fn(cmd);
                            if (output) {
                                int olen = (int)strlen(output);
                                while (olen > 0 && output[olen - 1] == '\n')
                                    output[--olen] = '\0';
                                dynbuf_append_str(&result, output);
                                free(output);
                            }
                            free(cmd);
                        }
                    } else {
                        dynbuf_append_str(&result, "$(");
                        dynbuf_append(&result, cmd_start, cmdlen);
                        dynbuf_push(&result, ')');
                    }
                }
            } else if (*p == '?') {
                char num[16];
                snprintf(num, sizeof(num), "%d", last_exit_code);
                dynbuf_append_str(&result, num);
                p++;
            } else if (*p == '$') {
                char num[16];
                snprintf(num, sizeof(num), "%d", (int)getpid());
                dynbuf_append_str(&result, num);
                p++;
            } else if (*p == '#') {
                const char *val = shell_var_get("#");
                if (val) dynbuf_append_str(&result, val);
                else dynbuf_push(&result, '0');
                p++;
            } else if (*p == '@' || *p == '*') {
                char name[2] = { *p, '\0' };
                const char *val = shell_var_get(name);
                if (val) dynbuf_append_str(&result, val);
                p++;
            } else if (*p >= '1' && *p <= '9') {
                char name[2] = { *p, '\0' };
                const char *val = shell_var_get(name);
                if (val) dynbuf_append_str(&result, val);
                p++;
            } else if (*p == '{') {
                p++;
                const char *start = p;
                while (*p && *p != '}') p++;
                if (*p == '}') {
                    int namelen = (int)(p - start);
                    char *name = malloc(namelen + 1);
                    if (name) {
                        memcpy(name, start, namelen);
                        name[namelen] = '\0';
                        const char *val = shell_var_get(name);
                        if (val) dynbuf_append_str(&result, val);
                        free(name);
                    }
                    p++;
                }
            } else if (isalpha((unsigned char)*p) || *p == '_') {
                const char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_') p++;
                int namelen = (int)(p - start);
                char *name = malloc(namelen + 1);
                if (name) {
                    memcpy(name, start, namelen);
                    name[namelen] = '\0';
                    const char *val = shell_var_get(name);
                    if (val) dynbuf_append_str(&result, val);
                    free(name);
                }
            } else {
                dynbuf_push(&result, '$');
            }
        } else {
            dynbuf_push(&result, *p++);
        }
    }

    char *expanded = dynbuf_steal(&result);
    if (strcmp(expanded, word) == 0) {
        free(expanded);
        return NULL;
    }
    return expanded;
}

/*
 * Check if a word contains glob metacharacters
 */
static int has_glob_chars(const char *word) {
    for (const char *p = word; *p; p++) {
        if (*p == '*' || *p == '?' || *p == '[') return 1;
    }
    return 0;
}

/*
 * Expand a single word (tilde + variable). Returns malloc'd copy.
 * If quoted, skip glob expansion.
 */
char *expand_word(const char *word, int quoted) {
    char *current = strdup(word);
    if (!current) return NULL;

    /* 1. Tilde expansion (skip if quoted) */
    if (!quoted) {
        char *expanded = expand_tilde(current);
        if (expanded) { free(current); current = expanded; }
    }

    /* 2. Variable expansion */
    {
        char *expanded = expand_variables(current);
        if (expanded) { free(current); current = expanded; }
    }

    /* Glob expansion not done here - only in expand_command for argv */
    return current;
}

/*
 * Expand a command's argv[] in-place.
 * Tilde + variable expansion for each arg.
 * Glob expansion if !arg_quoted[i] and word has glob chars.
 * Glob may grow argv array.
 */
int expand_command(Command *cmd) {
    if (!cmd || !cmd->argv) return 0;

    int i = 0;
    while (i < cmd->argc) {
        char *word = cmd->argv[i];
        if (!word) { i++; continue; }

        int quoted = cmd->arg_quoted ? cmd->arg_quoted[i] : 0;

        /* 1. Tilde expansion (skip if quoted) */
        if (!quoted) {
            char *expanded = expand_tilde(word);
            if (expanded) {
                free(cmd->argv[i]);
                cmd->argv[i] = expanded;
                word = expanded;
            }
        }

        /* 2. Variable expansion */
        {
            char *expanded = expand_variables(word);
            if (expanded) {
                free(cmd->argv[i]);
                cmd->argv[i] = expanded;
                word = expanded;
            }
        }

        /* 3. Glob expansion (skip if quoted) */
        if (!quoted && has_glob_chars(cmd->argv[i])) {
            glob_t gl;
            int ret = glob(cmd->argv[i], GLOB_NOCHECK | GLOB_TILDE, NULL, &gl);
            if (ret == 0 && gl.gl_pathc > 0) {
                /* Check if glob actually expanded */
                if (gl.gl_pathc == 1 && strcmp(gl.gl_pathv[0], cmd->argv[i]) == 0) {
                    globfree(&gl);
                    i++;
                    continue;
                }

                int extra = (int)gl.gl_pathc - 1;

                /* Grow argv and arg_quoted if needed */
                if (cmd->argc + extra >= MAX_ARGS) {
                    globfree(&gl);
                    i++;
                    continue;
                }

                /* Shift args after i to make room */
                for (int j = cmd->argc; j > i; j--) {
                    cmd->argv[j + extra] = cmd->argv[j];
                    if (cmd->arg_quoted) cmd->arg_quoted[j + extra] = cmd->arg_quoted[j];
                }

                /* Replace i with first match */
                free(cmd->argv[i]);
                cmd->argv[i] = strdup(gl.gl_pathv[0]);
                if (cmd->arg_quoted) cmd->arg_quoted[i] = 0;

                /* Insert remaining matches */
                for (int g = 1; g < (int)gl.gl_pathc; g++) {
                    cmd->argv[i + g] = strdup(gl.gl_pathv[g]);
                    if (cmd->arg_quoted) cmd->arg_quoted[i + g] = 0;
                }

                cmd->argc += extra;
                cmd->argv[cmd->argc] = NULL;
                i += (int)gl.gl_pathc;

                globfree(&gl);
                continue;
            } else {
                if (ret == 0) globfree(&gl);
            }
        }

        i++;
    }

    /* Also expand redirect filenames */
    if (cmd->redir_in.filename) {
        char *exp = expand_word(cmd->redir_in.filename, 0);
        if (exp) {
            free(cmd->redir_in.filename);
            cmd->redir_in.filename = exp;
        }
    }
    if (cmd->redir_out.filename) {
        char *exp = expand_word(cmd->redir_out.filename, 0);
        if (exp) {
            free(cmd->redir_out.filename);
            cmd->redir_out.filename = exp;
        }
    }

    return 0;
}
