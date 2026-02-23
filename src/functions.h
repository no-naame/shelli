/*
 * shelli - Educational Shell
 * functions.h - Function table interface
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "ast.h"

/* Initialize function table */
void func_init(void);

/* Define a function (stores ast_clone of body) */
void func_define(const char *name, AstNode *body);

/* Look up a function by name (returns body or NULL) */
AstNode *func_lookup(const char *name);

/* Remove a function definition */
void func_undefine(const char *name);

/* Check if name is a defined function */
int func_is_function(const char *name);

/* Clean up function table */
void func_cleanup(void);

#endif /* FUNCTIONS_H */
