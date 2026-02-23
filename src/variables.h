/*
 * shelli - Educational Shell
 * variables.h - Shell variable scoping interface
 *
 * Three-tier lookup: local scope -> shell globals -> getenv()
 */

#ifndef VARIABLES_H
#define VARIABLES_H

/* Initialize variable system */
void var_init(void);

/* Set a shell variable (creates if needed) */
void var_set(const char *name, const char *value);

/* Get a variable value (checks local -> global -> env) */
const char *var_get(const char *name);

/* Export a variable to the environment */
void var_export(const char *name);

/* Unset a variable */
void var_unset(const char *name);

/* Push a new local scope (for function calls) */
void var_push_scope(void);

/* Pop local scope (on function return) */
void var_pop_scope(void);

/* Set a local variable (only valid inside a scope) */
void var_set_local(const char *name, const char *value);

/* Set positional parameters $1, $2, ... for function args */
void var_set_positional(int argc, char **argv);

/* Get positional parameter count ($#) */
int var_positional_count(void);

/* Get positional parameter ($1, $2, ...) */
const char *var_positional_get(int n);

/* Clean up variable system */
void var_cleanup(void);

#endif /* VARIABLES_H */
