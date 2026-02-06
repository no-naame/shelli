/*
 * shelli - Educational Shell
 * lexer.c - Tokenization state machine
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

#define INITIAL_CAPACITY 16

/* Lexer states */
typedef enum {
    STATE_START,
    STATE_WORD,
    STATE_SQUOTE,
    STATE_DQUOTE
} LexerState;

void tokenlist_init(TokenList *list) {
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

void tokenlist_free(TokenList *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->tokens[i].value);
    }
    free(list->tokens);
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int tokenlist_grow(TokenList *list) {
    int new_cap = list->capacity == 0 ? INITIAL_CAPACITY : list->capacity * 2;
    Token *new_tokens = realloc(list->tokens, new_cap * sizeof(Token));
    if (!new_tokens) return -1;
    list->tokens = new_tokens;
    list->capacity = new_cap;
    return 0;
}

static int tokenlist_add(TokenList *list, TokenType type, const char *value) {
    if (list->count >= list->capacity) {
        if (tokenlist_grow(list) < 0) return -1;
    }
    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    return 0;
}

const char *token_type_str(TokenType type) {
    switch (type) {
        case TOK_WORD:      return "WORD";
        case TOK_PIPE:      return "PIPE";
        case TOK_REDIR_IN:  return "REDIR_IN";
        case TOK_REDIR_OUT: return "REDIR_OUT";
        case TOK_REDIR_APP: return "REDIR_APP";
        case TOK_EOF:       return "EOF";
        default:            return "UNKNOWN";
    }
}

int lexer_tokenize(const char *input, TokenList *list) {
    LexerState state = STATE_START;
    char wordbuf[1024];
    int wordlen = 0;
    const char *p = input;

    tokenlist_init(list);

    while (1) {
        char c = *p;

        switch (state) {
        case STATE_START:
            if (c == '\0') {
                goto done;
            } else if (isspace(c)) {
                p++;
            } else if (c == '|') {
                if (tokenlist_add(list, TOK_PIPE, "|") < 0) goto error;
                p++;
            } else if (c == '<') {
                if (tokenlist_add(list, TOK_REDIR_IN, "<") < 0) goto error;
                p++;
            } else if (c == '>') {
                if (*(p + 1) == '>') {
                    if (tokenlist_add(list, TOK_REDIR_APP, ">>") < 0) goto error;
                    p += 2;
                } else {
                    if (tokenlist_add(list, TOK_REDIR_OUT, ">") < 0) goto error;
                    p++;
                }
            } else if (c == '\'') {
                state = STATE_SQUOTE;
                p++;
            } else if (c == '"') {
                state = STATE_DQUOTE;
                p++;
            } else {
                /* Start of word */
                state = STATE_WORD;
                wordbuf[wordlen++] = c;
                p++;
            }
            break;

        case STATE_WORD:
            if (c == '\0' || isspace(c) || c == '|' || c == '<' || c == '>') {
                /* End of word */
                wordbuf[wordlen] = '\0';
                if (tokenlist_add(list, TOK_WORD, wordbuf) < 0) goto error;
                wordlen = 0;
                state = STATE_START;
            } else if (c == '\'') {
                state = STATE_SQUOTE;
                p++;
            } else if (c == '"') {
                state = STATE_DQUOTE;
                p++;
            } else {
                if (wordlen < (int)sizeof(wordbuf) - 1) {
                    wordbuf[wordlen++] = c;
                }
                p++;
            }
            break;

        case STATE_SQUOTE:
            if (c == '\0') {
                /* Unterminated quote */
                goto error;
            } else if (c == '\'') {
                state = STATE_WORD;
                p++;
            } else {
                if (wordlen < (int)sizeof(wordbuf) - 1) {
                    wordbuf[wordlen++] = c;
                }
                p++;
            }
            break;

        case STATE_DQUOTE:
            if (c == '\0') {
                /* Unterminated quote */
                goto error;
            } else if (c == '"') {
                state = STATE_WORD;
                p++;
            } else {
                if (wordlen < (int)sizeof(wordbuf) - 1) {
                    wordbuf[wordlen++] = c;
                }
                p++;
            }
            break;
        }
    }

done:
    /* Finish any pending word */
    if (wordlen > 0) {
        wordbuf[wordlen] = '\0';
        if (tokenlist_add(list, TOK_WORD, wordbuf) < 0) goto error;
    }

    /* Add EOF token */
    if (tokenlist_add(list, TOK_EOF, NULL) < 0) goto error;

    return 0;

error:
    tokenlist_free(list);
    return -1;
}
