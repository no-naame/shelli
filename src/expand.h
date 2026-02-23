/*
 * shelli - Educational Shell
 * expand.h - Word expansion interface (tilde, variables, globbing, command subst)
 *
 * Expansion is now per-command at execution time, not pre-parse.
 */

#ifndef EXPAND_H
#define EXPAND_H

#include "parser.h"

/* Set the last exit code for $? expansion */
void expand_set_exit_code(int code);

/* Set command substitution callback (called with cmd string, returns malloc'd output) */
void expand_set_substitution_fn(char *(*fn)(const char *cmd));

/* Set variable lookup function (replaces getenv for shell variable support) */
void expand_set_var_get_fn(const char *(*fn)(const char *name));

/* Expand a command's argv[] in-place.
 * Uses arg_quoted[] to suppress glob expansion for quoted words.
 * Glob expansion may grow argv (reallocs argv and arg_quoted).
 * Returns 0 on success, -1 on error. */
int expand_command(Command *cmd);

/* Expand a single word, returns malloc'd result.
 * If quoted is nonzero, skip glob expansion.
 * Used for 'for' word lists and redirect filenames. */
char *expand_word(const char *word, int quoted);

#endif /* EXPAND_H */
