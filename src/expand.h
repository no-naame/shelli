/*
 * shelli - Educational Shell
 * expand.h - Word expansion interface (tilde, variables, globbing)
 */

#ifndef EXPAND_H
#define EXPAND_H

#include "lexer.h"

/* Set the last exit code for $? expansion */
void expand_set_exit_code(int code);

/* Expand all words in a token list (tilde, variables, globs).
 * Modifies the token list in-place (may grow it for glob results).
 * Returns 0 on success, -1 on error. */
int expand_words(TokenList *list);

#endif /* EXPAND_H */
