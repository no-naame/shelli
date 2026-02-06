/*
 * shelli - Educational Shell
 * parser.c - Token to command parsing
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "parser.h"

#define MAX_ARGS 256

const char *redirect_type_str(int type) {
    switch (type) {
        case REDIR_NONE:   return "none";
        case REDIR_IN:     return "<";
        case REDIR_OUT:    return ">";
        case REDIR_APPEND: return ">>";
        default:           return "?";
    }
}

static Command *command_new(void) {
    Command *cmd = calloc(1, sizeof(Command));
    if (!cmd) return NULL;
    cmd->argv = calloc(MAX_ARGS + 1, sizeof(char *));
    if (!cmd->argv) {
        free(cmd);
        return NULL;
    }
    return cmd;
}

static void command_free(Command *cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->argv);
    free(cmd->redir_in.filename);
    free(cmd->redir_out.filename);
    free(cmd);
}

void pipeline_free(Pipeline *pipeline) {
    if (!pipeline) return;
    Command *cmd = pipeline->first;
    while (cmd) {
        Command *next = cmd->next;
        command_free(cmd);
        cmd = next;
    }
    free(pipeline);
}

static int command_add_arg(Command *cmd, const char *arg) {
    if (cmd->argc >= MAX_ARGS) return -1;
    cmd->argv[cmd->argc] = strdup(arg);
    if (!cmd->argv[cmd->argc]) return -1;
    cmd->argc++;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

Pipeline *parser_parse(TokenList *tokens, char *error, int error_size) {
    Pipeline *pipeline = calloc(1, sizeof(Pipeline));
    if (!pipeline) {
        snprintf(error, error_size, "Memory allocation failed");
        return NULL;
    }

    Command *current = NULL;
    Command *last = NULL;
    int i = 0;
    int expecting_filename = 0;
    int redirect_type = 0;

    while (i < tokens->count && tokens->tokens[i].type != TOK_EOF) {
        Token *tok = &tokens->tokens[i];

        switch (tok->type) {
        case TOK_WORD:
            if (expecting_filename) {
                /* This word is a redirect filename */
                if (redirect_type == REDIR_IN) {
                    current->redir_in.type = REDIR_IN;
                    current->redir_in.filename = strdup(tok->value);
                } else {
                    current->redir_out.type = redirect_type;
                    current->redir_out.filename = strdup(tok->value);
                }
                expecting_filename = 0;
                redirect_type = 0;
            } else {
                /* Regular argument */
                if (!current) {
                    current = command_new();
                    if (!current) {
                        snprintf(error, error_size, "Memory allocation failed");
                        pipeline_free(pipeline);
                        return NULL;
                    }
                    if (!pipeline->first) {
                        pipeline->first = current;
                    }
                    if (last) {
                        last->next = current;
                    }
                    pipeline->cmd_count++;
                }
                if (command_add_arg(current, tok->value) < 0) {
                    snprintf(error, error_size, "Too many arguments");
                    pipeline_free(pipeline);
                    return NULL;
                }
            }
            break;

        case TOK_PIPE:
            if (!current || current->argc == 0) {
                snprintf(error, error_size, "Syntax error: unexpected '|'");
                pipeline_free(pipeline);
                return NULL;
            }
            if (expecting_filename) {
                snprintf(error, error_size, "Syntax error: missing filename after redirect");
                pipeline_free(pipeline);
                return NULL;
            }
            last = current;
            current = NULL;
            break;

        case TOK_REDIR_IN:
            if (!current) {
                snprintf(error, error_size, "Syntax error: redirect without command");
                pipeline_free(pipeline);
                return NULL;
            }
            expecting_filename = 1;
            redirect_type = REDIR_IN;
            break;

        case TOK_REDIR_OUT:
            if (!current) {
                snprintf(error, error_size, "Syntax error: redirect without command");
                pipeline_free(pipeline);
                return NULL;
            }
            expecting_filename = 1;
            redirect_type = REDIR_OUT;
            break;

        case TOK_REDIR_APP:
            if (!current) {
                snprintf(error, error_size, "Syntax error: redirect without command");
                pipeline_free(pipeline);
                return NULL;
            }
            expecting_filename = 1;
            redirect_type = REDIR_APPEND;
            break;

        case TOK_EOF:
            break;
        }
        i++;
    }

    if (expecting_filename) {
        snprintf(error, error_size, "Syntax error: missing filename after redirect");
        pipeline_free(pipeline);
        return NULL;
    }

    if (pipeline->cmd_count == 0) {
        /* Empty input is valid */
        free(pipeline);
        return NULL;
    }

    return pipeline;
}
