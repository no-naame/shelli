/*
 * shelli - Educational Shell
 * executor.h - AST tree-walking executor interface
 */

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <sys/types.h>
#include "ast.h"

/* Callback for logging execution steps */
typedef void (*ExecLogCallback)(const char *message);

/* Set the logging callback for execution tracing */
void executor_set_logger(ExecLogCallback callback);

/* Get the current foreground child PID (for signal forwarding) */
pid_t executor_get_fg_pid(void);

/* Execute an AST tree, returns exit status of last command */
int executor_run_ast(AstNode *node);

/* Execute an AST and capture stdout of the last command */
int executor_run_ast_capture(AstNode *node, char *output, int output_size);

/* Break/continue state for loops */
extern int shell_break_count;
extern int shell_continue_count;

#endif /* EXECUTOR_H */
