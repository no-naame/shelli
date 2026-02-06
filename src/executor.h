/*
 * shelli - Educational Shell
 * executor.h - Command execution interface
 */

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

/* Callback for logging execution steps */
typedef void (*ExecLogCallback)(const char *message);

/* Set the logging callback for execution tracing */
void executor_set_logger(ExecLogCallback callback);

/* Execute a pipeline, returns exit status of last command */
int executor_run(Pipeline *pipeline);

/* Execute a pipeline and capture stdout, returns exit status */
int executor_run_capture(Pipeline *pipeline, char *output, int output_size);

#endif /* EXECUTOR_H */
