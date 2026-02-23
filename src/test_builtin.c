/*
 * shelli - Educational Shell
 * test_builtin.c - Implementation of test / [ builtin
 *
 * Recursive descent evaluator over argv.
 * Supports file tests, string tests, numeric tests, and logic operators.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "test_builtin.h"

typedef struct {
    char **argv;
    int argc;
    int pos;
} TestState;

static int eval_expr(TestState *s);

static const char *peek(TestState *s) {
    if (s->pos < s->argc) return s->argv[s->pos];
    return NULL;
}

static const char *advance(TestState *s) {
    if (s->pos < s->argc) return s->argv[s->pos++];
    return NULL;
}

static int at(TestState *s, const char *str) {
    const char *cur = peek(s);
    return cur && strcmp(cur, str) == 0;
}

/* Primary: file tests, string tests, numeric comparisons, parenthesized */
static int eval_primary(TestState *s) {
    const char *arg = peek(s);
    if (!arg) return 1; /* empty -> false */

    /* Parenthesized expression */
    if (strcmp(arg, "(") == 0) {
        advance(s); /* consume ( */
        int result = eval_expr(s);
        if (at(s, ")")) advance(s); /* consume ) */
        return result;
    }

    /* Unary NOT */
    if (strcmp(arg, "!") == 0) {
        advance(s);
        return !eval_primary(s) ? 0 : 1;
    }

    /* Unary file tests: -e, -f, -d, -r, -w, -x, -s, -L */
    if (arg[0] == '-' && arg[1] != '\0' && arg[2] == '\0') {
        char flag = arg[1];
        if (flag == 'e' || flag == 'f' || flag == 'd' ||
            flag == 'r' || flag == 'w' || flag == 'x' ||
            flag == 's' || flag == 'L') {
            advance(s); /* consume flag */
            const char *file = advance(s);
            if (!file) { fprintf(stderr, "test: missing argument after -%c\n", flag); return 2; }

            struct stat st;
            int r;
            if (flag == 'L') r = lstat(file, &st);
            else r = stat(file, &st);

            if (r < 0) return 1; /* file doesn't exist -> false */

            switch (flag) {
            case 'e': return 0; /* exists */
            case 'f': return S_ISREG(st.st_mode) ? 0 : 1;
            case 'd': return S_ISDIR(st.st_mode) ? 0 : 1;
            case 'r': return (access(file, R_OK) == 0) ? 0 : 1;
            case 'w': return (access(file, W_OK) == 0) ? 0 : 1;
            case 'x': return (access(file, X_OK) == 0) ? 0 : 1;
            case 's': return (st.st_size > 0) ? 0 : 1;
            case 'L': return S_ISLNK(st.st_mode) ? 0 : 1;
            }
            return 1;
        }

        /* Unary string tests: -n, -z */
        if (flag == 'n' || flag == 'z') {
            advance(s);
            const char *str = advance(s);
            if (!str) {
                /* -n and -z with no arg: -n is true (string "-n" is non-empty), -z is false */
                return (flag == 'n') ? 0 : 1;
            }
            if (flag == 'z') return (strlen(str) == 0) ? 0 : 1;
            if (flag == 'n') return (strlen(str) > 0) ? 0 : 1;
            return 1;
        }
    }

    /* Check for binary operators: look ahead */
    advance(s); /* consume first operand */
    const char *op = peek(s);

    if (op) {
        /* String comparison */
        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
            advance(s);
            const char *right = advance(s);
            if (!right) { fprintf(stderr, "test: missing argument after =\n"); return 2; }
            return (strcmp(arg, right) == 0) ? 0 : 1;
        }
        if (strcmp(op, "!=") == 0) {
            advance(s);
            const char *right = advance(s);
            if (!right) { fprintf(stderr, "test: missing argument after !=\n"); return 2; }
            return (strcmp(arg, right) != 0) ? 0 : 1;
        }

        /* Numeric comparison */
        if (strcmp(op, "-eq") == 0 || strcmp(op, "-ne") == 0 ||
            strcmp(op, "-lt") == 0 || strcmp(op, "-le") == 0 ||
            strcmp(op, "-gt") == 0 || strcmp(op, "-ge") == 0) {
            advance(s);
            const char *right = advance(s);
            if (!right) { fprintf(stderr, "test: missing argument after %s\n", op); return 2; }
            long lv = strtol(arg, NULL, 10);
            long rv = strtol(right, NULL, 10);
            if (strcmp(op, "-eq") == 0) return (lv == rv) ? 0 : 1;
            if (strcmp(op, "-ne") == 0) return (lv != rv) ? 0 : 1;
            if (strcmp(op, "-lt") == 0) return (lv <  rv) ? 0 : 1;
            if (strcmp(op, "-le") == 0) return (lv <= rv) ? 0 : 1;
            if (strcmp(op, "-gt") == 0) return (lv >  rv) ? 0 : 1;
            if (strcmp(op, "-ge") == 0) return (lv >= rv) ? 0 : 1;
        }
    }

    /* Bare string: true if non-empty */
    return (strlen(arg) > 0) ? 0 : 1;
}

/* Expression with -a (AND) and -o (OR) */
static int eval_expr(TestState *s) {
    int result = eval_primary(s);

    while (peek(s)) {
        if (at(s, "-a")) {
            advance(s);
            int right = eval_primary(s);
            if (result != 0 || right != 0) result = 1;
        } else if (at(s, "-o")) {
            advance(s);
            int right = eval_primary(s);
            if (result == 0 || right == 0) result = 0;
        } else {
            break;
        }
    }

    return result;
}

int test_builtin_execute(int argc, char **argv) {
    if (argc < 2) return 1; /* No args -> false */

    int is_bracket = (strcmp(argv[0], "[") == 0);

    /* Skip command name */
    int start = 1;
    int end = argc;

    /* If invoked as [, check for closing ] */
    if (is_bracket) {
        if (argc < 2 || strcmp(argv[argc - 1], "]") != 0) {
            fprintf(stderr, "[: missing ']'\n");
            return 2;
        }
        end = argc - 1;
    }

    if (start >= end) return 1; /* empty expression -> false */

    TestState s;
    s.argv = argv + start;
    s.argc = end - start;
    s.pos = 0;

    return eval_expr(&s);
}
