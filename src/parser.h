/*
 * shelli - Educational Shell
 * parser.h - Command structures and recursive descent parser interface
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

/* Forward declaration */
typedef struct AstNode AstNode;

/* Redirect types */
#define REDIR_NONE   0
#define REDIR_IN     1  /* < */
#define REDIR_OUT    2  /* > */
#define REDIR_APPEND 3  /* >> */
#define REDIR_HEREDOC 4 /* << */

typedef struct {
    int type;           /* REDIR_NONE, REDIR_IN, REDIR_OUT, REDIR_APPEND */
    char *filename;     /* Dynamically allocated */
} Redirect;

typedef struct Command {
    char **argv;        /* NULL-terminated argument array */
    int argc;
    int *arg_quoted;    /* Parallel to argv: 1 if arg was quoted (skip glob) */
    Redirect redir_in;  /* Input redirection */
    Redirect redir_out; /* Output redirection */
    char *heredoc_delim; /* Delimiter word for << (NULL if none) */
    int   heredoc_fd;    /* Pipe read-end fd filled in before execution (-1 if none) */
} Command;

/* Parse result tri-state */
typedef enum {
    PARSE_OK,           /* Successfully parsed */
    PARSE_ERROR,        /* Syntax error */
    PARSE_INCOMPLETE    /* Need more input (unclosed compound command) */
} ParseResult;

/* Create a new empty command */
Command *command_new(void);

/* Free a command and all its data */
void command_free(Command *cmd);

/* Add an argument to a command (with quoted flag) */
int command_add_arg(Command *cmd, const char *arg, int quoted);

/* Parse tokens into an AST.
 * On PARSE_OK, *out is set to the root AstNode (caller must ast_free).
 * On PARSE_ERROR, error message is written to error buf.
 * On PARSE_INCOMPLETE, *out is NULL and no error message. */
ParseResult parser_parse_ast(TokenList *tokens, AstNode **out,
                             char *error, int error_size);

/* Get string representation of redirect type */
const char *redirect_type_str(int type);

#endif /* PARSER_H */
