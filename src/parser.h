/*
 * shelli - Educational Shell
 * parser.h - Command and pipeline structures
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

/* Redirect types */
#define REDIR_NONE   0
#define REDIR_IN     1  /* < */
#define REDIR_OUT    2  /* > */
#define REDIR_APPEND 3  /* >> */

typedef struct {
    int type;           /* REDIR_NONE, REDIR_IN, REDIR_OUT, REDIR_APPEND */
    char *filename;     /* Dynamically allocated */
} Redirect;

typedef struct Command {
    char **argv;        /* NULL-terminated argument array */
    int argc;
    Redirect redir_in;  /* Input redirection */
    Redirect redir_out; /* Output redirection */
    struct Command *next; /* Next command in pipeline */
} Command;

typedef struct {
    Command *first;     /* Head of pipeline linked list */
    int cmd_count;      /* Number of commands in pipeline */
} Pipeline;

/* Parse tokens into a pipeline, returns NULL on error */
Pipeline *parser_parse(TokenList *tokens, char *error, int error_size);

/* Free a pipeline and all its commands */
void pipeline_free(Pipeline *pipeline);

/* Get string representation of redirect type */
const char *redirect_type_str(int type);

#endif /* PARSER_H */
