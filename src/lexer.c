/*
 * shelli - Educational Shell
 * lexer.c - Tokenization state machine
 *
 * Uses DynBuf for word accumulation (no fixed buffer overflow).
 * Handles double-quote escape sequences: \" \\ \$ \`
 * Tracks quoted status per token for glob suppression.
 * Single & produces TOK_BG with clear error-free behavior.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "lexer.h"
#include "util.h"

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

static int tokenlist_add(TokenList *list, TokenType type, const char *value, int quoted) {
    if (list->count >= list->capacity) {
        if (tokenlist_grow(list) < 0) return -1;
    }
    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    tok->quoted = quoted;
    return 0;
}

const char *token_type_str(TokenType type) {
    switch (type) {
        case TOK_WORD:      return "WORD";
        case TOK_PIPE:      return "PIPE";
        case TOK_REDIR_IN:  return "REDIR_IN";
        case TOK_REDIR_OUT: return "REDIR_OUT";
        case TOK_REDIR_APP: return "REDIR_APP";
        case TOK_HEREDOC:   return "HEREDOC";
        case TOK_SEMI:      return "SEMI";
        case TOK_AND:       return "AND";
        case TOK_OR:        return "OR";
        case TOK_BG:        return "BG";
        case TOK_EOF:       return "EOF";
        default:            return "UNKNOWN";
    }
}

int lexer_tokenize(const char *input, TokenList *list,
                   char *error_buf, int error_buf_size) {
    LexerState state = STATE_START;
    DynBuf word;
    dynbuf_init(&word);
    int was_quoted = 0;  /* track if current word includes any quoted part */
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
            } else if (c == ';') {
                if (tokenlist_add(list, TOK_SEMI, ";", 0) < 0) goto oom;
                p++;
            } else if (c == '&') {
                if (*(p + 1) == '&') {
                    if (tokenlist_add(list, TOK_AND, "&&", 0) < 0) goto oom;
                    p += 2;
                } else {
                    /* Single & = background operator */
                    if (tokenlist_add(list, TOK_BG, "&", 0) < 0) goto oom;
                    p++;
                }
            } else if (c == '|') {
                if (*(p + 1) == '|') {
                    if (tokenlist_add(list, TOK_OR, "||", 0) < 0) goto oom;
                    p += 2;
                } else {
                    if (tokenlist_add(list, TOK_PIPE, "|", 0) < 0) goto oom;
                    p++;
                }
            } else if (c == '<') {
                if (*(p + 1) == '<') {
                    if (tokenlist_add(list, TOK_HEREDOC, "<<", 0) < 0) goto oom;
                    p += 2;
                } else {
                    if (tokenlist_add(list, TOK_REDIR_IN, "<", 0) < 0) goto oom;
                    p++;
                }
            } else if (c == '>') {
                if (*(p + 1) == '>') {
                    if (tokenlist_add(list, TOK_REDIR_APP, ">>", 0) < 0) goto oom;
                    p += 2;
                } else {
                    if (tokenlist_add(list, TOK_REDIR_OUT, ">", 0) < 0) goto oom;
                    p++;
                }
            } else if (c == '\'') {
                was_quoted = 1;
                state = STATE_SQUOTE;
                p++;
            } else if (c == '"') {
                was_quoted = 1;
                state = STATE_DQUOTE;
                p++;
            } else {
                /* Start of word */
                state = STATE_WORD;
                was_quoted = 0;
                dynbuf_push(&word, c);
                p++;
            }
            break;

        case STATE_WORD:
            if (c == '\0' || isspace(c) || c == '|' || c == '<' || c == '>' || c == ';' || c == '&') {
                /* End of word - emit token */
                char *val = dynbuf_steal(&word);
                if (tokenlist_add(list, TOK_WORD, val, was_quoted) < 0) {
                    free(val);
                    goto oom;
                }
                free(val);
                dynbuf_init(&word);
                was_quoted = 0;
                state = STATE_START;
            } else if (c == '\'') {
                was_quoted = 1;
                state = STATE_SQUOTE;
                p++;
            } else if (c == '"') {
                was_quoted = 1;
                state = STATE_DQUOTE;
                p++;
            } else {
                dynbuf_push(&word, c);
                p++;
            }
            break;

        case STATE_SQUOTE:
            if (c == '\0') {
                if (error_buf && error_buf_size > 0)
                    snprintf(error_buf, error_buf_size, "unterminated single quote");
                goto error;
            } else if (c == '\'') {
                state = STATE_WORD;
                p++;
            } else {
                dynbuf_push(&word, c);
                p++;
            }
            break;

        case STATE_DQUOTE:
            if (c == '\0') {
                if (error_buf && error_buf_size > 0)
                    snprintf(error_buf, error_buf_size, "unterminated double quote");
                goto error;
            } else if (c == '\\') {
                /* Escape sequences inside double quotes (POSIX) */
                char next = *(p + 1);
                if (next == '"' || next == '\\' || next == '$' || next == '`') {
                    dynbuf_push(&word, next);
                    p += 2;
                } else {
                    /* Other \X: keep the backslash literally */
                    dynbuf_push(&word, c);
                    p++;
                }
            } else if (c == '"') {
                state = STATE_WORD;
                p++;
            } else {
                dynbuf_push(&word, c);
                p++;
            }
            break;
        }
    }

done:
    /* Finish any pending word */
    if (word.len > 0) {
        char *val = dynbuf_steal(&word);
        if (tokenlist_add(list, TOK_WORD, val, was_quoted) < 0) {
            free(val);
            goto oom;
        }
        free(val);
    } else {
        dynbuf_free(&word);
    }

    /* Add EOF token */
    if (tokenlist_add(list, TOK_EOF, NULL, 0) < 0) goto oom;

    return 0;

oom:
    if (error_buf && error_buf_size > 0)
        snprintf(error_buf, error_buf_size, "memory allocation failed");
error:
    dynbuf_free(&word);
    tokenlist_free(list);
    return -1;
}
