/*
 * shelli - Educational Shell
 * builtins.h - Built-in command interface
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

/* Check if command is a built-in, returns 1 if yes */
int builtin_is_builtin(const char *name);

/* Execute built-in command, returns exit status
 * Sets *should_exit to 1 if shell should terminate */
int builtin_execute(Command *cmd, int *should_exit);

/* Get help text for all built-ins */
const char *builtin_help(void);

#endif /* BUILTINS_H */
