/*
 * shelli - Educational Shell
 * functions.c - Function table: define, lookup, undefine
 */

#include <stdlib.h>
#include <string.h>
#include "functions.h"

#define MAX_FUNCTIONS 128

typedef struct {
    char *name;
    AstNode *body;
} ShellFunc;

static ShellFunc func_table[MAX_FUNCTIONS];
static int func_count = 0;

void func_init(void) {
    func_count = 0;
}

void func_define(const char *name, AstNode *body) {
    /* Update existing */
    for (int i = 0; i < func_count; i++) {
        if (strcmp(func_table[i].name, name) == 0) {
            ast_free(func_table[i].body);
            func_table[i].body = ast_clone(body);
            return;
        }
    }
    /* Add new */
    if (func_count >= MAX_FUNCTIONS) return;
    func_table[func_count].name = strdup(name);
    func_table[func_count].body = ast_clone(body);
    func_count++;
}

AstNode *func_lookup(const char *name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(func_table[i].name, name) == 0) {
            return func_table[i].body;
        }
    }
    return NULL;
}

void func_undefine(const char *name) {
    for (int i = 0; i < func_count; i++) {
        if (strcmp(func_table[i].name, name) == 0) {
            free(func_table[i].name);
            ast_free(func_table[i].body);
            func_table[i] = func_table[func_count - 1];
            func_count--;
            return;
        }
    }
}

int func_is_function(const char *name) {
    return func_lookup(name) != NULL;
}

void func_cleanup(void) {
    for (int i = 0; i < func_count; i++) {
        free(func_table[i].name);
        ast_free(func_table[i].body);
    }
    func_count = 0;
}
