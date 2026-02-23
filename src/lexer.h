/*
 * shelli - Educational Shell
 * lexer.h - Token types and lexer interface
 */

#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOK_WORD,       /* Command or argument */
    TOK_PIPE,       /* | */
    TOK_REDIR_IN,   /* < */
    TOK_REDIR_OUT,  /* > */
    TOK_REDIR_APP,  /* >> */
    TOK_HEREDOC,    /* << DELIMITER (value holds the delimiter word) */
    TOK_SEMI,       /* ; */
    TOK_AND,        /* && */
    TOK_OR,         /* || */
    TOK_BG,         /* & (background) */
    TOK_EOF         /* End of input */
} TokenType;

typedef struct {
    TokenType type;
    char *value;    /* Dynamically allocated, NULL for operators */
    int quoted;     /* 1 if token came from quoted string (skip glob) */
} Token;

typedef struct {
    Token *tokens;
    int count;
    int capacity;
} TokenList;

/* Initialize an empty token list */
void tokenlist_init(TokenList *list);

/* Free all tokens and the list */
void tokenlist_free(TokenList *list);

/* Tokenize input string, returns 0 on success, -1 on error.
 * On error, writes a message to error_buf (if non-NULL). */
int lexer_tokenize(const char *input, TokenList *list,
                   char *error_buf, int error_buf_size);

/* Get string representation of token type */
const char *token_type_str(TokenType type);

#endif /* LEXER_H */
