/*
 * shelli - Educational Shell
 * variables.c - Shell variable scoping with scope stack
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "variables.h"

#define MAX_VARS 256
#define MAX_SCOPES 32
#define MAX_POSITIONAL 64

typedef struct {
    char *name;
    char *value;
} ShellVar;

typedef struct {
    ShellVar vars[MAX_VARS];
    int count;
} VarScope;

static VarScope global_scope;
static VarScope *scope_stack[MAX_SCOPES];
static int scope_depth = 0;

/* Positional parameters */
static char *positional[MAX_POSITIONAL];
static int positional_count = 0;

void var_init(void) {
    memset(&global_scope, 0, sizeof(global_scope));
    scope_depth = 0;
}

static ShellVar *scope_find(VarScope *scope, const char *name) {
    for (int i = 0; i < scope->count; i++) {
        if (strcmp(scope->vars[i].name, name) == 0) return &scope->vars[i];
    }
    return NULL;
}

static void scope_set(VarScope *scope, const char *name, const char *value) {
    ShellVar *v = scope_find(scope, name);
    if (v) {
        free(v->value);
        v->value = strdup(value);
        return;
    }
    if (scope->count >= MAX_VARS) return;
    scope->vars[scope->count].name = strdup(name);
    scope->vars[scope->count].value = strdup(value);
    scope->count++;
}

static void scope_unset(VarScope *scope, const char *name) {
    for (int i = 0; i < scope->count; i++) {
        if (strcmp(scope->vars[i].name, name) == 0) {
            free(scope->vars[i].name);
            free(scope->vars[i].value);
            scope->vars[i] = scope->vars[scope->count - 1];
            scope->count--;
            return;
        }
    }
}

static void scope_free(VarScope *scope) {
    for (int i = 0; i < scope->count; i++) {
        free(scope->vars[i].name);
        free(scope->vars[i].value);
    }
    scope->count = 0;
}

void var_set(const char *name, const char *value) {
    /* If we're in a local scope, set there */
    if (scope_depth > 0) {
        VarScope *local = scope_stack[scope_depth - 1];
        ShellVar *v = scope_find(local, name);
        if (v) {
            free(v->value);
            v->value = strdup(value);
            return;
        }
    }
    /* Otherwise set in global scope */
    scope_set(&global_scope, name, value);
}

const char *var_get(const char *name) {
    /* Check positional parameters */
    if (name[0] >= '1' && name[0] <= '9' && name[1] == '\0') {
        int n = name[0] - '0';
        return var_positional_get(n);
    }
    if (strcmp(name, "#") == 0) {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d", positional_count);
        return buf;
    }
    if (strcmp(name, "@") == 0 || strcmp(name, "*") == 0) {
        /* Return all positional params joined */
        static char joined[4096];
        joined[0] = '\0';
        for (int i = 0; i < positional_count; i++) {
            if (i > 0) strcat(joined, " ");
            strncat(joined, positional[i], sizeof(joined) - strlen(joined) - 1);
        }
        return joined[0] ? joined : NULL;
    }

    /* Check local scopes (innermost first) */
    for (int i = scope_depth - 1; i >= 0; i--) {
        ShellVar *v = scope_find(scope_stack[i], name);
        if (v) return v->value;
    }

    /* Check global scope */
    ShellVar *v = scope_find(&global_scope, name);
    if (v) return v->value;

    /* Fall through to environment */
    return getenv(name);
}

void var_export(const char *name) {
    const char *val = var_get(name);
    if (val) setenv(name, val, 1);
}

void var_unset(const char *name) {
    /* Remove from local scopes */
    for (int i = scope_depth - 1; i >= 0; i--) {
        scope_unset(scope_stack[i], name);
    }
    scope_unset(&global_scope, name);
    unsetenv(name);
}

void var_push_scope(void) {
    if (scope_depth >= MAX_SCOPES) return;
    VarScope *s = calloc(1, sizeof(VarScope));
    scope_stack[scope_depth++] = s;
}

void var_pop_scope(void) {
    if (scope_depth <= 0) return;
    scope_depth--;
    scope_free(scope_stack[scope_depth]);
    free(scope_stack[scope_depth]);
    scope_stack[scope_depth] = NULL;
}

void var_set_local(const char *name, const char *value) {
    if (scope_depth <= 0) {
        /* No local scope, set globally */
        var_set(name, value);
        return;
    }
    scope_set(scope_stack[scope_depth - 1], name, value);
}

void var_set_positional(int argc, char **argv) {
    /* Free old positionals */
    for (int i = 0; i < positional_count; i++) free(positional[i]);
    positional_count = 0;

    for (int i = 0; i < argc && i < MAX_POSITIONAL; i++) {
        positional[i] = strdup(argv[i]);
        positional_count++;
    }
}

int var_positional_count(void) {
    return positional_count;
}

const char *var_positional_get(int n) {
    if (n < 1 || n > positional_count) return NULL;
    return positional[n - 1];
}

void var_cleanup(void) {
    while (scope_depth > 0) var_pop_scope();
    scope_free(&global_scope);
    for (int i = 0; i < positional_count; i++) free(positional[i]);
    positional_count = 0;
}
