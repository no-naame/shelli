/*
 * shelli - Educational Shell
 * parser.c - Recursive descent parser producing AstNode* trees
 *
 * Grammar:
 *   program    ::= list EOF
 *   list       ::= and_or ((';' | '&') and_or)* [';' | '&']
 *   and_or     ::= pipeline (('&&' | '||') pipeline)*
 *   pipeline   ::= ['!'] command ('|' command)*
 *   command    ::= simple_command | if_clause | while_clause | until_clause
 *                | for_clause | brace_group
 *   simple_cmd ::= word+ (with redirects interspersed)
 *   if_clause  ::= 'if' list 'then' list ('elif' list 'then' list)* ['else' list] 'fi'
 *   while_clause ::= 'while' list 'do' list 'done'
 *   until_clause ::= 'until' list 'do' list 'done'
 *   for_clause ::= 'for' NAME ['in' word* (';'|newline)] 'do' list 'done'
 *   brace_group ::= '{' list '}'
 *
 * Keywords are TOK_WORD tokens with specific string values.
 * PARSE_INCOMPLETE is returned when EOF is reached inside a compound command.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include "parser.h"
#include "ast.h"

#define MAX_ARGS 256

/* Parser state */
typedef struct {
    TokenList *tokens;
    int pos;
    char *error;
    int error_size;
    ParseResult result;   /* Set to PARSE_INCOMPLETE or PARSE_ERROR */
    int in_compound;      /* Nesting depth inside compound commands */
} Parser;

/* ================================================================
 * Command helpers (public via parser.h)
 * ================================================================ */

const char *redirect_type_str(int type) {
    switch (type) {
        case REDIR_NONE:   return "none";
        case REDIR_IN:     return "<";
        case REDIR_OUT:    return ">";
        case REDIR_APPEND: return ">>";
        case REDIR_HEREDOC: return "<<";
        default:           return "?";
    }
}

Command *command_new(void) {
    Command *cmd = calloc(1, sizeof(Command));
    if (!cmd) return NULL;
    cmd->argv = calloc(MAX_ARGS + 1, sizeof(char *));
    if (!cmd->argv) {
        free(cmd);
        return NULL;
    }
    cmd->arg_quoted = calloc(MAX_ARGS + 1, sizeof(int));
    if (!cmd->arg_quoted) {
        free(cmd->argv);
        free(cmd);
        return NULL;
    }
    cmd->heredoc_fd = -1;
    return cmd;
}

void command_free(Command *cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->argv);
    free(cmd->arg_quoted);
    free(cmd->redir_in.filename);
    free(cmd->redir_out.filename);
    free(cmd->heredoc_delim);
    if (cmd->heredoc_fd >= 0) close(cmd->heredoc_fd);
    free(cmd);
}

int command_add_arg(Command *cmd, const char *arg, int quoted) {
    if (cmd->argc >= MAX_ARGS) return -1;
    cmd->argv[cmd->argc] = strdup(arg);
    if (!cmd->argv[cmd->argc]) return -1;
    cmd->arg_quoted[cmd->argc] = quoted;
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

/* ================================================================
 * Parser helpers
 * ================================================================ */

static Token *peek(Parser *p) {
    if (p->pos < p->tokens->count)
        return &p->tokens->tokens[p->pos];
    return NULL;
}

static Token *advance(Parser *p) {
    Token *t = peek(p);
    if (t && t->type != TOK_EOF) p->pos++;
    return t;
}

static int at_eof(Parser *p) {
    Token *t = peek(p);
    return !t || t->type == TOK_EOF;
}

/* Check if current token is a TOK_WORD with a specific value */
static int at_word(Parser *p, const char *word) {
    Token *t = peek(p);
    return t && t->type == TOK_WORD && t->value && strcmp(t->value, word) == 0;
}

/* Consume a TOK_WORD matching a specific value. Returns 1 on success. */
static int expect_word(Parser *p, const char *word) {
    if (at_word(p, word)) {
        advance(p);
        return 1;
    }
    return 0;
}

static void parse_error(Parser *p, const char *fmt, ...) {
    if (p->result != PARSE_OK) return; /* Don't overwrite */
    p->result = PARSE_ERROR;
    if (p->error && p->error_size > 0) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(p->error, p->error_size, fmt, args);
        va_end(args);
    }
}

static void parse_incomplete(Parser *p) {
    if (p->result != PARSE_OK) return;
    p->result = PARSE_INCOMPLETE;
}

/* Check if word is a keyword that terminates a simple command */
static int is_keyword(const char *word) {
    return strcmp(word, "if") == 0 || strcmp(word, "then") == 0 ||
           strcmp(word, "elif") == 0 || strcmp(word, "else") == 0 ||
           strcmp(word, "fi") == 0 || strcmp(word, "while") == 0 ||
           strcmp(word, "until") == 0 || strcmp(word, "for") == 0 ||
           strcmp(word, "do") == 0 || strcmp(word, "done") == 0 ||
           strcmp(word, "{") == 0 || strcmp(word, "}") == 0;
}

/* Check if current token is a list terminator (for compound interiors) */
static int at_list_end(Parser *p) {
    Token *t = peek(p);
    if (!t || t->type == TOK_EOF) return 1;
    if (t->type != TOK_WORD || !t->value) return 0;
    return strcmp(t->value, "then") == 0 ||
           strcmp(t->value, "elif") == 0 ||
           strcmp(t->value, "else") == 0 ||
           strcmp(t->value, "fi") == 0 ||
           strcmp(t->value, "do") == 0 ||
           strcmp(t->value, "done") == 0 ||
           strcmp(t->value, "}") == 0;
}

/* ================================================================
 * Forward declarations
 * ================================================================ */

static AstNode *parse_list(Parser *p);
static AstNode *parse_and_or(Parser *p);
static AstNode *parse_pipeline(Parser *p);
static AstNode *parse_command(Parser *p);
static AstNode *parse_simple_command(Parser *p);
static AstNode *parse_if(Parser *p);
static AstNode *parse_while(Parser *p);
static AstNode *parse_until(Parser *p);
static AstNode *parse_for(Parser *p);
static AstNode *parse_brace_group(Parser *p);

/* ================================================================
 * parse_list: and_or ((';' | '&') and_or)* [';' | '&']
 * ================================================================ */

static AstNode *parse_list(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    /* Collect list entries */
    int cap = 8;
    int count = 0;
    ListEntry *entries = malloc(cap * sizeof(ListEntry));
    if (!entries) { parse_error(p, "memory allocation failed"); return NULL; }

    /* Skip leading semicolons */
    while (!at_eof(p) && peek(p)->type == TOK_SEMI) advance(p);

    /* Parse first and_or (may be empty if at list end) */
    if (at_eof(p) || at_list_end(p)) {
        free(entries);
        return NULL; /* Empty list */
    }

    AstNode *node = parse_and_or(p);
    if (p->result != PARSE_OK) { free(entries); ast_free(node); return NULL; }
    if (!node) { free(entries); return NULL; }

    /* Determine separator */
    ListSepType sep = LIST_SEP_SEMI;

    while (p->result == PARSE_OK) {
        Token *t = peek(p);
        if (!t) break;

        if (t->type == TOK_SEMI) {
            sep = LIST_SEP_SEMI;
            advance(p);
        } else if (t->type == TOK_BG) {
            sep = LIST_SEP_BG;
            advance(p);
        } else {
            sep = LIST_SEP_SEMI; /* Implicit semicolon at end */
            break;
        }

        /* Store this entry */
        if (count >= cap) {
            cap *= 2;
            ListEntry *ne = realloc(entries, cap * sizeof(ListEntry));
            if (!ne) { parse_error(p, "memory allocation failed"); ast_free(node); free(entries); return NULL; }
            entries = ne;
        }
        entries[count].pipeline = node;
        entries[count].sep = sep;
        count++;
        node = NULL;

        /* Skip extra semicolons */
        while (!at_eof(p) && peek(p)->type == TOK_SEMI) advance(p);

        /* Check for end of list or EOF */
        if (at_eof(p) || at_list_end(p)) break;

        /* Parse next and_or */
        node = parse_and_or(p);
        if (p->result != PARSE_OK) { ast_free(node); break; }
        if (!node) break;
    }

    /* Store final entry if any */
    if (node) {
        if (count >= cap) {
            cap *= 2;
            ListEntry *ne = realloc(entries, cap * sizeof(ListEntry));
            if (!ne) { parse_error(p, "memory allocation failed"); ast_free(node); free(entries); return NULL; }
            entries = ne;
        }
        entries[count].pipeline = node;
        entries[count].sep = LIST_SEP_NONE;
        count++;
    }

    if (p->result != PARSE_OK) {
        for (int i = 0; i < count; i++) ast_free(entries[i].pipeline);
        free(entries);
        return NULL;
    }

    if (count == 0) {
        free(entries);
        return NULL;
    }

    /* If single entry, return it directly (no wrapping list node) */
    if (count == 1 && entries[0].sep == LIST_SEP_NONE) {
        AstNode *single = entries[0].pipeline;
        free(entries);
        return single;
    }

    return ast_new_list(entries, count);
}

/* ================================================================
 * parse_and_or: pipeline (('&&' | '||') pipeline)*
 * ================================================================ */

static AstNode *parse_and_or(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    AstNode *left = parse_pipeline(p);
    if (p->result != PARSE_OK || !left) return left;

    /* Check for && or || chaining */
    int cap = 4, count = 0;
    ListEntry *entries = NULL;

    while (p->result == PARSE_OK) {
        Token *t = peek(p);
        if (!t) break;

        ListSepType sep;
        if (t->type == TOK_AND) {
            sep = LIST_SEP_AND;
        } else if (t->type == TOK_OR) {
            sep = LIST_SEP_OR;
        } else {
            break;
        }
        advance(p);

        /* Lazy-init entries array */
        if (!entries) {
            entries = malloc(cap * sizeof(ListEntry));
            if (!entries) { parse_error(p, "memory allocation failed"); ast_free(left); return NULL; }
            entries[0].pipeline = left;
            entries[0].sep = sep;
            count = 1;
            left = NULL;
        } else {
            entries[count - 1].sep = sep; /* Set sep on previous entry */
        }

        /* If EOF after && or ||, need more input */
        if (at_eof(p)) {
            if (p->in_compound) {
                parse_incomplete(p);
            } else {
                parse_error(p, "Syntax error: unexpected end of input after '%s'",
                            sep == LIST_SEP_AND ? "&&" : "||");
            }
            for (int i = 0; i < count; i++) ast_free(entries[i].pipeline);
            free(entries);
            return NULL;
        }

        AstNode *right = parse_pipeline(p);
        if (p->result != PARSE_OK) {
            ast_free(right);
            for (int i = 0; i < count; i++) ast_free(entries[i].pipeline);
            free(entries);
            return NULL;
        }

        if (count >= cap) {
            cap *= 2;
            ListEntry *ne = realloc(entries, cap * sizeof(ListEntry));
            if (!ne) {
                parse_error(p, "memory allocation failed");
                ast_free(right);
                for (int i = 0; i < count; i++) ast_free(entries[i].pipeline);
                free(entries);
                return NULL;
            }
            entries = ne;
        }
        entries[count].pipeline = right;
        entries[count].sep = LIST_SEP_NONE;
        count++;
    }

    if (!entries) return left; /* No &&/|| found */

    return ast_new_list(entries, count);
}

/* ================================================================
 * parse_pipeline: ['!'] command ('|' command)*
 * ================================================================ */

static AstNode *parse_pipeline(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    /* Check for ! (NOT) */
    int negated = 0;
    if (at_word(p, "!")) {
        negated = 1;
        advance(p);
    }

    AstNode *first = parse_command(p);
    if (p->result != PARSE_OK || !first) return first;

    /* Check for pipes */
    if (peek(p) && peek(p)->type == TOK_PIPE) {
        int cap = 4, count = 1;
        AstNode **cmds = malloc(cap * sizeof(AstNode *));
        if (!cmds) { parse_error(p, "memory allocation failed"); ast_free(first); return NULL; }
        cmds[0] = first;

        while (peek(p) && peek(p)->type == TOK_PIPE) {
            advance(p); /* consume | */

            if (at_eof(p)) {
                if (p->in_compound) {
                    parse_incomplete(p);
                } else {
                    parse_error(p, "Syntax error: unexpected end of input after '|'");
                }
                for (int i = 0; i < count; i++) ast_free(cmds[i]);
                free(cmds);
                return NULL;
            }

            AstNode *cmd = parse_command(p);
            if (p->result != PARSE_OK || !cmd) {
                ast_free(cmd);
                for (int i = 0; i < count; i++) ast_free(cmds[i]);
                free(cmds);
                return NULL;
            }

            if (count >= cap) {
                cap *= 2;
                AstNode **nc = realloc(cmds, cap * sizeof(AstNode *));
                if (!nc) {
                    parse_error(p, "memory allocation failed");
                    ast_free(cmd);
                    for (int i = 0; i < count; i++) ast_free(cmds[i]);
                    free(cmds);
                    return NULL;
                }
                cmds = nc;
            }
            cmds[count++] = cmd;
        }

        AstNode *pipeline = ast_new_pipeline(cmds, count, negated);
        return pipeline;
    }

    /* Single command, wrap in NOT if needed */
    if (negated) {
        return ast_new_not(first);
    }
    return first;
}

/* ================================================================
 * parse_command: dispatches to compound or simple
 * ================================================================ */

static AstNode *parse_command(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    if (at_eof(p) || at_list_end(p)) {
        return NULL;
    }

    if (at_word(p, "if"))    return parse_if(p);
    if (at_word(p, "while")) return parse_while(p);
    if (at_word(p, "until")) return parse_until(p);
    if (at_word(p, "for"))   return parse_for(p);
    if (at_word(p, "{"))     return parse_brace_group(p);

    /* Check for function definition: NAME() { body } */
    Token *t = peek(p);
    if (t && t->type == TOK_WORD && t->value) {
        int len = (int)strlen(t->value);
        if (len > 2 && t->value[len - 2] == '(' && t->value[len - 1] == ')') {
            /* Function definition */
            char *name = strdup(t->value);
            name[len - 2] = '\0'; /* Strip () */
            advance(p);

            /* Expect brace group */
            if (!at_word(p, "{")) {
                if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected '{' after function name"); }
                free(name);
                return NULL;
            }

            p->in_compound++;
            if (!expect_word(p, "{")) {
                parse_error(p, "Syntax error: expected '{'");
                free(name);
                p->in_compound--;
                return NULL;
            }

            while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

            AstNode *body = parse_list(p);
            if (p->result != PARSE_OK) { p->in_compound--; free(name); ast_free(body); return NULL; }

            if (!expect_word(p, "}")) {
                if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected '}'"); }
                p->in_compound--;
                free(name);
                ast_free(body);
                return NULL;
            }

            p->in_compound--;
            AstNode *node = ast_new_function_def(name, body);
            free(name);
            return node;
        }
    }

    return parse_simple_command(p);
}

/* ================================================================
 * parse_simple_command: word+ with interspersed redirects
 * ================================================================ */

static AstNode *parse_simple_command(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    Command *cmd = command_new();
    if (!cmd) { parse_error(p, "memory allocation failed"); return NULL; }

    int got_word = 0;

    while (p->result == PARSE_OK) {
        Token *t = peek(p);
        if (!t || t->type == TOK_EOF) break;

        if (t->type == TOK_WORD) {
            /* Check if it's a keyword that should terminate this command */
            if (t->value && is_keyword(t->value) && got_word) {
                break; /* Let the caller handle it */
            }
            if (t->value && is_keyword(t->value) && !got_word) {
                /* A keyword at the start of a simple command is an error -
                 * but parse_command should have dispatched it. If we got here,
                 * it's something like 'then' appearing where a command is expected */
                break;
            }

            command_add_arg(cmd, t->value, t->quoted);
            got_word = 1;
            advance(p);
        } else if (t->type == TOK_REDIR_IN) {
            advance(p);
            Token *fn = peek(p);
            if (!fn || fn->type != TOK_WORD || !fn->value) {
                parse_error(p, "Syntax error: missing filename after '<'");
                command_free(cmd);
                return NULL;
            }
            free(cmd->redir_in.filename);
            cmd->redir_in.type = REDIR_IN;
            cmd->redir_in.filename = strdup(fn->value);
            advance(p);
            got_word = 1;
        } else if (t->type == TOK_REDIR_OUT) {
            advance(p);
            Token *fn = peek(p);
            if (!fn || fn->type != TOK_WORD || !fn->value) {
                parse_error(p, "Syntax error: missing filename after '>'");
                command_free(cmd);
                return NULL;
            }
            free(cmd->redir_out.filename);
            cmd->redir_out.type = REDIR_OUT;
            cmd->redir_out.filename = strdup(fn->value);
            advance(p);
            got_word = 1;
        } else if (t->type == TOK_REDIR_APP) {
            advance(p);
            Token *fn = peek(p);
            if (!fn || fn->type != TOK_WORD || !fn->value) {
                parse_error(p, "Syntax error: missing filename after '>>'");
                command_free(cmd);
                return NULL;
            }
            free(cmd->redir_out.filename);
            cmd->redir_out.type = REDIR_APPEND;
            cmd->redir_out.filename = strdup(fn->value);
            advance(p);
            got_word = 1;
        } else if (t->type == TOK_HEREDOC) {
            advance(p);
            Token *delim = peek(p);
            if (!delim || delim->type != TOK_WORD || !delim->value) {
                parse_error(p, "Syntax error: missing heredoc delimiter");
                command_free(cmd);
                return NULL;
            }
            free(cmd->heredoc_delim);
            cmd->heredoc_delim = strdup(delim->value);
            advance(p);
            got_word = 1;
        } else {
            /* Pipe, semi, and, or, bg — stop */
            break;
        }
    }

    if (!got_word || cmd->argc == 0) {
        command_free(cmd);
        return NULL;
    }

    return ast_new_command(cmd);
}

/* ================================================================
 * parse_if: 'if' list 'then' list ('elif' list 'then' list)* ['else' list] 'fi'
 * ================================================================ */

static AstNode *parse_if(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    if (!expect_word(p, "if")) {
        parse_error(p, "Syntax error: expected 'if'");
        return NULL;
    }

    p->in_compound++;

    /* Skip optional semicolons after 'if' */
    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    /* Parse condition */
    AstNode *condition = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(condition); return NULL; }
    if (!condition) {
        if (at_eof(p)) { parse_incomplete(p); p->in_compound--; return NULL; }
        parse_error(p, "Syntax error: missing condition after 'if'");
        p->in_compound--;
        return NULL;
    }

    /* Skip optional semicolons before 'then' */
    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    if (!expect_word(p, "then")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'then'"); }
        p->in_compound--;
        ast_free(condition);
        return NULL;
    }

    /* Skip optional semicolons after 'then' */
    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    /* Parse then body */
    AstNode *then_body = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(condition); ast_free(then_body); return NULL; }

    /* Check for elif/else/fi */
    AstNode *else_body = NULL;

    if (at_word(p, "elif")) {
        /* elif is parsed as a nested if */
        else_body = parse_if(p);
        /* parse_if consumed the nested if; the outer 'fi' was consumed by the inner */
        /* Actually, elif reuses 'if' logic but the final 'fi' closes ALL */
        /* We need to handle this differently: elif creates a chain */
        /* Let's re-parse: replace 'elif' with 'if' conceptually */
        if (p->result != PARSE_OK) {
            ast_free(condition);
            ast_free(then_body);
            ast_free(else_body);
            p->in_compound--;
            return NULL;
        }
        p->in_compound--;
        return ast_new_if(condition, then_body, else_body);
    } else if (expect_word(p, "else")) {
        /* Skip optional semicolons after 'else' */
        while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

        else_body = parse_list(p);
        if (p->result != PARSE_OK) {
            p->in_compound--;
            ast_free(condition);
            ast_free(then_body);
            ast_free(else_body);
            return NULL;
        }
    }

    if (!expect_word(p, "fi")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'fi'"); }
        p->in_compound--;
        ast_free(condition);
        ast_free(then_body);
        ast_free(else_body);
        return NULL;
    }

    p->in_compound--;
    return ast_new_if(condition, then_body, else_body);
}

/* ================================================================
 * parse_while: 'while' list 'do' list 'done'
 * ================================================================ */

static AstNode *parse_while(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    if (!expect_word(p, "while")) {
        parse_error(p, "Syntax error: expected 'while'");
        return NULL;
    }

    p->in_compound++;

    /* Skip optional semicolons */
    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    AstNode *condition = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(condition); return NULL; }
    if (!condition) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: missing condition after 'while'"); }
        p->in_compound--;
        return NULL;
    }

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    if (!expect_word(p, "do")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'do'"); }
        p->in_compound--;
        ast_free(condition);
        return NULL;
    }

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    AstNode *body = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(condition); ast_free(body); return NULL; }

    if (!expect_word(p, "done")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'done'"); }
        p->in_compound--;
        ast_free(condition);
        ast_free(body);
        return NULL;
    }

    p->in_compound--;
    return ast_new_while(condition, body);
}

/* ================================================================
 * parse_until: 'until' list 'do' list 'done'
 * ================================================================ */

static AstNode *parse_until(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    if (!expect_word(p, "until")) {
        parse_error(p, "Syntax error: expected 'until'");
        return NULL;
    }

    p->in_compound++;

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    AstNode *condition = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(condition); return NULL; }
    if (!condition) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: missing condition after 'until'"); }
        p->in_compound--;
        return NULL;
    }

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    if (!expect_word(p, "do")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'do'"); }
        p->in_compound--;
        ast_free(condition);
        return NULL;
    }

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    AstNode *body = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(condition); ast_free(body); return NULL; }

    if (!expect_word(p, "done")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'done'"); }
        p->in_compound--;
        ast_free(condition);
        ast_free(body);
        return NULL;
    }

    p->in_compound--;
    return ast_new_until(condition, body);
}

/* ================================================================
 * parse_for: 'for' NAME ['in' word* (';')] 'do' list 'done'
 * ================================================================ */

static AstNode *parse_for(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    if (!expect_word(p, "for")) {
        parse_error(p, "Syntax error: expected 'for'");
        return NULL;
    }

    p->in_compound++;

    /* Get variable name */
    Token *var_tok = peek(p);
    if (!var_tok || var_tok->type != TOK_WORD || !var_tok->value) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected variable name after 'for'"); }
        p->in_compound--;
        return NULL;
    }
    char *var_name = strdup(var_tok->value);
    advance(p);

    /* Optional 'in' word list */
    char **words = NULL;
    int word_count = 0;
    int word_cap = 8;

    if (at_word(p, "in")) {
        advance(p); /* consume 'in' */

        words = malloc(word_cap * sizeof(char *));
        if (!words) { parse_error(p, "memory allocation failed"); free(var_name); p->in_compound--; return NULL; }

        /* Collect words until ';' or 'do' or EOF */
        while (p->result == PARSE_OK) {
            Token *t = peek(p);
            if (!t || t->type == TOK_EOF || t->type == TOK_SEMI) break;
            if (t->type == TOK_WORD && t->value && strcmp(t->value, "do") == 0) break;
            if (t->type != TOK_WORD) break;

            if (word_count >= word_cap) {
                word_cap *= 2;
                char **nw = realloc(words, word_cap * sizeof(char *));
                if (!nw) { parse_error(p, "memory allocation failed"); break; }
                words = nw;
            }
            words[word_count++] = strdup(t->value);
            advance(p);
        }
    }

    /* Skip optional semicolons */
    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    if (!expect_word(p, "do")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'do' in for loop"); }
        p->in_compound--;
        free(var_name);
        for (int i = 0; i < word_count; i++) free(words[i]);
        free(words);
        return NULL;
    }

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    AstNode *body = parse_list(p);
    if (p->result != PARSE_OK) {
        p->in_compound--;
        free(var_name);
        for (int i = 0; i < word_count; i++) free(words[i]);
        free(words);
        ast_free(body);
        return NULL;
    }

    if (!expect_word(p, "done")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected 'done'"); }
        p->in_compound--;
        free(var_name);
        for (int i = 0; i < word_count; i++) free(words[i]);
        free(words);
        ast_free(body);
        return NULL;
    }

    p->in_compound--;
    AstNode *result = ast_new_for(var_name, words, word_count, body);
    free(var_name);
    return result;
}

/* ================================================================
 * parse_brace_group: '{' list '}'
 * ================================================================ */

static AstNode *parse_brace_group(Parser *p) {
    if (p->result != PARSE_OK) return NULL;

    if (!expect_word(p, "{")) {
        parse_error(p, "Syntax error: expected '{'");
        return NULL;
    }

    p->in_compound++;

    while (peek(p) && peek(p)->type == TOK_SEMI) advance(p);

    AstNode *body = parse_list(p);
    if (p->result != PARSE_OK) { p->in_compound--; ast_free(body); return NULL; }

    if (!expect_word(p, "}")) {
        if (at_eof(p)) { parse_incomplete(p); } else { parse_error(p, "Syntax error: expected '}'"); }
        p->in_compound--;
        ast_free(body);
        return NULL;
    }

    p->in_compound--;

    /* A brace group is just its body (the list) */
    return body ? body : NULL;
}

/* ================================================================
 * Top-level: parser_parse_ast
 * ================================================================ */

ParseResult parser_parse_ast(TokenList *tokens, AstNode **out,
                             char *error, int error_size) {
    *out = NULL;
    if (error && error_size > 0) error[0] = '\0';

    if (!tokens || tokens->count == 0) return PARSE_OK;

    /* Check if input is only EOF */
    if (tokens->count == 1 && tokens->tokens[0].type == TOK_EOF) return PARSE_OK;

    Parser p;
    p.tokens = tokens;
    p.pos = 0;
    p.error = error;
    p.error_size = error_size;
    p.result = PARSE_OK;
    p.in_compound = 0;

    AstNode *ast = parse_list(&p);

    if (p.result == PARSE_INCOMPLETE) {
        ast_free(ast);
        return PARSE_INCOMPLETE;
    }

    if (p.result == PARSE_ERROR) {
        ast_free(ast);
        return PARSE_ERROR;
    }

    /* Check for trailing garbage */
    if (!at_eof(&p)) {
        Token *t = peek(&p);
        if (t && t->type != TOK_EOF) {
            snprintf(error, error_size, "Syntax error: unexpected '%s'",
                     t->value ? t->value : token_type_str(t->type));
            ast_free(ast);
            return PARSE_ERROR;
        }
    }

    *out = ast;
    return PARSE_OK;
}
