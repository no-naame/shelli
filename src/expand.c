/*
 * shelli - Educational Shell
 * expand.c - Word expansion: tilde, environment variables, arithmetic, globbing
 *
 * Expansion order (matching real shells):
 *   1. Tilde expansion       (~  → /home/user)
 *   2. Variable expansion    ($VAR → value)
 *   3. Arithmetic expansion  $(( expr )) → numeric result
 *   4. Glob expansion        (*.c  → file1.c file2.c)
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <glob.h>
#include <stdio.h>
#include "expand.h"

static int last_exit_code = 0;

void expand_set_exit_code(int code) {
    last_exit_code = code;
}

/*
 * ============================================================================
 * Arithmetic expression evaluator for $(( expr ))
 *
 * Grammar (precedence, low → high):
 *   expr     ::= additive
 *   additive ::= multiplicative (('+' | '-') multiplicative)*
 *   multi    ::= power    (('*' | '/' | '%') power)*
 *   power    ::= unary    ('**' unary)*
 *   unary    ::= '-' unary | primary
 *   primary  ::= '(' expr ')' | NUMBER | '$'VARNAME | VARNAME
 * ============================================================================
 */

typedef struct {
    const char *p;  /* current position in expression string */
    int error;      /* non-zero if a parse/eval error occurred */
} ArithState;

static long arith_expr(ArithState *s);  /* forward declaration */

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

    /* Variable reference: $NAME or bare NAME (common in arithmetic) */
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
            const char *val = getenv(name);
            if (val) {
                char *end;
                long n = strtol(val, &end, 10);
                if (*end == '\0') return n;
            }
        }
        return 0;
    }

    /* Number (decimal, hex 0x..., octal 0...) */
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
    if (*s->p == '-') {
        s->p++;
        return -arith_unary(s);
    }
    if (*s->p == '+') {
        s->p++;
        return arith_unary(s);
    }
    if (*s->p == '~') {
        s->p++;
        return ~arith_unary(s);
    }
    if (*s->p == '!') {
        s->p++;
        return !arith_unary(s);
    }
    return arith_primary(s);
}

static long arith_power(ArithState *s) {
    long base = arith_unary(s);
    arith_skip_ws(s);
    if (s->p[0] == '*' && s->p[1] == '*') {
        s->p += 2;
        long exp = arith_power(s);  /* right-associative */
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
        /* Careful: don't consume ** as a single * */
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

/*
 * Evaluate an arithmetic expression string.
 * Returns the result as a long; sets *ok=0 on error.
 */
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
 *   ~        → $HOME
 *   ~/path   → $HOME/path
 */
static char *expand_tilde(const char *word) {
    if (word[0] != '~') return NULL;

    /* Only expand ~ or ~/... (not ~user) */
    if (word[1] != '\0' && word[1] != '/') return NULL;

    const char *home = getenv("HOME");
    if (!home) return NULL;

    size_t hlen = strlen(home);
    size_t wlen = strlen(word + 1); /* skip the ~ */
    char *result = malloc(hlen + wlen + 1);
    if (!result) return NULL;

    strcpy(result, home);
    strcat(result, word + 1);
    return result;
}

/*
 * Variable and arithmetic expansion: replace $VAR, ${VAR}, $?, $((...)) in a word
 */
static char *expand_variables(const char *word) {
    /* Quick check: does this word contain a $ at all? */
    if (!strchr(word, '$')) return NULL;

    char result[4096];
    int rlen = 0;
    const char *p = word;

    while (*p && rlen < (int)sizeof(result) - 1) {
        if (*p == '$') {
            p++;
            if (*p == '(') {
                /* Could be $(( expr )) */
                if (*(p + 1) == '(') {
                    p += 2; /* skip (( */
                    /* Find matching )) */
                    const char *expr_start = p;
                    int depth = 1;
                    while (*p && depth > 0) {
                        if (*p == '(' ) depth++;
                        else if (*p == ')') {
                            depth--;
                            if (depth == 0) break;
                        }
                        p++;
                    }
                    int exprlen = (int)(p - expr_start);
                    char expr[1024];
                    if (exprlen < (int)sizeof(expr)) {
                        memcpy(expr, expr_start, exprlen);
                        expr[exprlen] = '\0';
                        int ok = 0;
                        long val = arith_evaluate(expr, &ok);
                        if (ok) {
                            char num[32];
                            snprintf(num, sizeof(num), "%ld", val);
                            int nlen = (int)strlen(num);
                            if (rlen + nlen < (int)sizeof(result) - 1) {
                                memcpy(result + rlen, num, nlen);
                                rlen += nlen;
                            }
                        } else {
                            /* On error, substitute empty string */
                            fprintf(stderr, "shelli: arithmetic error: %.*s\n",
                                    exprlen, expr_start);
                        }
                    }
                    /* Skip closing )) */
                    if (*p == ')') p++;
                    if (*p == ')') p++;
                } else {
                    /* Plain $(  — not supported yet, emit literal */
                    result[rlen++] = '$';
                    result[rlen++] = '(';
                    p++;
                }
            } else if (*p == '?') {
                /* $? - last exit code */
                char num[16];
                snprintf(num, sizeof(num), "%d", last_exit_code);
                int nlen = (int)strlen(num);
                if (rlen + nlen < (int)sizeof(result) - 1) {
                    memcpy(result + rlen, num, nlen);
                    rlen += nlen;
                }
                p++;
            } else if (*p == '{') {
                /* ${VAR} */
                p++; /* skip { */
                const char *start = p;
                while (*p && *p != '}') p++;
                if (*p == '}') {
                    int namelen = (int)(p - start);
                    char name[256];
                    if (namelen < (int)sizeof(name)) {
                        memcpy(name, start, namelen);
                        name[namelen] = '\0';
                        const char *val = getenv(name);
                        if (val) {
                            int vlen = (int)strlen(val);
                            if (rlen + vlen < (int)sizeof(result) - 1) {
                                memcpy(result + rlen, val, vlen);
                                rlen += vlen;
                            }
                        }
                    }
                    p++; /* skip } */
                }
            } else if (isalpha((unsigned char)*p) || *p == '_') {
                /* $VAR */
                const char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_') p++;
                int namelen = (int)(p - start);
                char name[256];
                if (namelen < (int)sizeof(name)) {
                    memcpy(name, start, namelen);
                    name[namelen] = '\0';
                    const char *val = getenv(name);
                    if (val) {
                        int vlen = (int)strlen(val);
                        if (rlen + vlen < (int)sizeof(result) - 1) {
                            memcpy(result + rlen, val, vlen);
                            rlen += vlen;
                        }
                    }
                }
            } else {
                /* Literal $ (e.g. end of string or unrecognized) */
                result[rlen++] = '$';
            }
        } else {
            result[rlen++] = *p++;
        }
    }

    result[rlen] = '\0';

    /* Only return a new string if something actually changed */
    if (strcmp(result, word) == 0) return NULL;

    return strdup(result);
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
 * Expand all words in a token list.
 * Tilde and variable expansion modify tokens in-place.
 * Glob expansion may insert additional tokens.
 */
int expand_words(TokenList *list) {
    /* We iterate and may insert tokens, so we work index-based */
    int i = 0;
    while (i < list->count) {
        Token *tok = &list->tokens[i];

        /* Only expand WORD tokens */
        if (tok->type != TOK_WORD || !tok->value) {
            i++;
            continue;
        }

        /* 1. Tilde expansion */
        char *expanded = expand_tilde(tok->value);
        if (expanded) {
            free(tok->value);
            tok->value = expanded;
        }

        /* 2. Variable expansion */
        expanded = expand_variables(tok->value);
        if (expanded) {
            free(tok->value);
            tok->value = expanded;
        }

        /* 3. Glob expansion */
        if (has_glob_chars(tok->value)) {
            glob_t gl;
            int ret = glob(tok->value, GLOB_NOCHECK | GLOB_TILDE, NULL, &gl);
            if (ret == 0 && gl.gl_pathc > 0) {
                /* Check if glob actually expanded (not just returned the pattern) */
                if (gl.gl_pathc == 1 && strcmp(gl.gl_pathv[0], tok->value) == 0) {
                    /* No match, keep original */
                    globfree(&gl);
                    i++;
                    continue;
                }

                /* Replace this token with first match, insert rest after */
                free(tok->value);
                tok->value = strdup(gl.gl_pathv[0]);

                /* Insert remaining matches */
                int extra = (int)gl.gl_pathc - 1;
                if (extra > 0) {
                    /* Grow token list if needed */
                    while (list->count + extra >= list->capacity) {
                        int new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
                        Token *new_tokens = realloc(list->tokens, new_cap * sizeof(Token));
                        if (!new_tokens) {
                            globfree(&gl);
                            return -1;
                        }
                        list->tokens = new_tokens;
                        list->capacity = new_cap;
                    }

                    /* Shift tokens after i to make room */
                    memmove(&list->tokens[i + 1 + extra],
                            &list->tokens[i + 1],
                            (list->count - i - 1) * sizeof(Token));
                    list->count += extra;

                    /* Insert glob results */
                    for (int g = 1; g <= extra; g++) {
                        list->tokens[i + g].type = TOK_WORD;
                        list->tokens[i + g].value = strdup(gl.gl_pathv[g]);
                    }

                    i += extra; /* Skip past inserted tokens */
                }

                globfree(&gl);
            } else {
                if (ret == 0) globfree(&gl);
            }
        }

        i++;
    }

    return 0;
}
