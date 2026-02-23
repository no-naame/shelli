/*
 * shelli - Educational Shell
 * ast.c - Abstract Syntax Tree construction, freeing, cloning
 */

#include <stdlib.h>
#include <string.h>
#include "ast.h"

static AstNode *ast_alloc(AstNodeType type) {
    AstNode *n = calloc(1, sizeof(AstNode));
    if (!n) return NULL;
    n->type = type;
    return n;
}

AstNode *ast_new_command(Command *cmd) {
    AstNode *n = ast_alloc(NODE_COMMAND);
    if (!n) return NULL;
    n->data.command.cmd = cmd;
    return n;
}

AstNode *ast_new_pipeline(AstNode **cmds, int count, int negated) {
    AstNode *n = ast_alloc(NODE_PIPELINE);
    if (!n) return NULL;
    n->data.pipeline.cmds = cmds;
    n->data.pipeline.cmd_count = count;
    n->data.pipeline.negated = negated;
    return n;
}

AstNode *ast_new_list(ListEntry *entries, int count) {
    AstNode *n = ast_alloc(NODE_LIST);
    if (!n) return NULL;
    n->data.list.entries = entries;
    n->data.list.count = count;
    return n;
}

AstNode *ast_new_if(AstNode *condition, AstNode *then_body, AstNode *else_body) {
    AstNode *n = ast_alloc(NODE_IF);
    if (!n) return NULL;
    n->data.if_clause.condition = condition;
    n->data.if_clause.then_body = then_body;
    n->data.if_clause.else_body = else_body;
    return n;
}

AstNode *ast_new_while(AstNode *condition, AstNode *body) {
    AstNode *n = ast_alloc(NODE_WHILE);
    if (!n) return NULL;
    n->data.loop.condition = condition;
    n->data.loop.body = body;
    return n;
}

AstNode *ast_new_until(AstNode *condition, AstNode *body) {
    AstNode *n = ast_alloc(NODE_UNTIL);
    if (!n) return NULL;
    n->data.loop.condition = condition;
    n->data.loop.body = body;
    return n;
}

AstNode *ast_new_for(const char *var, char **words, int word_count, AstNode *body) {
    AstNode *n = ast_alloc(NODE_FOR);
    if (!n) return NULL;
    n->data.for_clause.var_name = strdup(var);
    n->data.for_clause.words = words;
    n->data.for_clause.word_count = word_count;
    n->data.for_clause.body = body;
    return n;
}

AstNode *ast_new_function_def(const char *name, AstNode *body) {
    AstNode *n = ast_alloc(NODE_FUNCTION_DEF);
    if (!n) return NULL;
    n->data.func_def.name = strdup(name);
    n->data.func_def.body = body;
    return n;
}

AstNode *ast_new_not(AstNode *child) {
    AstNode *n = ast_alloc(NODE_NOT);
    if (!n) return NULL;
    n->data.not_clause.child = child;
    return n;
}

AstNode *ast_new_subshell(AstNode *body) {
    AstNode *n = ast_alloc(NODE_SUBSHELL);
    if (!n) return NULL;
    n->data.subshell.body = body;
    return n;
}

/* Use public command_free from parser.c */

void ast_free(AstNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_COMMAND:
        command_free(node->data.command.cmd);
        break;

    case NODE_PIPELINE:
        for (int i = 0; i < node->data.pipeline.cmd_count; i++) {
            ast_free(node->data.pipeline.cmds[i]);
        }
        free(node->data.pipeline.cmds);
        break;

    case NODE_LIST:
        for (int i = 0; i < node->data.list.count; i++) {
            ast_free(node->data.list.entries[i].pipeline);
        }
        free(node->data.list.entries);
        break;

    case NODE_IF:
        ast_free(node->data.if_clause.condition);
        ast_free(node->data.if_clause.then_body);
        ast_free(node->data.if_clause.else_body);
        break;

    case NODE_WHILE:
    case NODE_UNTIL:
        ast_free(node->data.loop.condition);
        ast_free(node->data.loop.body);
        break;

    case NODE_FOR:
        free(node->data.for_clause.var_name);
        for (int i = 0; i < node->data.for_clause.word_count; i++) {
            free(node->data.for_clause.words[i]);
        }
        free(node->data.for_clause.words);
        ast_free(node->data.for_clause.body);
        break;

    case NODE_FUNCTION_DEF:
        free(node->data.func_def.name);
        ast_free(node->data.func_def.body);
        break;

    case NODE_NOT:
        ast_free(node->data.not_clause.child);
        break;

    case NODE_SUBSHELL:
        ast_free(node->data.subshell.body);
        break;
    }

    free(node);
}

/* Clone a Command (deep copy including arg_quoted) */
static Command *clone_command(Command *src) {
    if (!src) return NULL;
    Command *dst = calloc(1, sizeof(Command));
    if (!dst) return NULL;

    dst->argc = src->argc;
    dst->argv = calloc(src->argc + 1, sizeof(char *));
    if (!dst->argv) { free(dst); return NULL; }

    dst->arg_quoted = calloc(src->argc + 1, sizeof(int));
    if (!dst->arg_quoted) { free(dst->argv); free(dst); return NULL; }

    for (int i = 0; i < src->argc; i++) {
        dst->argv[i] = strdup(src->argv[i]);
        if (src->arg_quoted) dst->arg_quoted[i] = src->arg_quoted[i];
    }
    dst->argv[src->argc] = NULL;

    if (src->redir_in.filename) {
        dst->redir_in.type = src->redir_in.type;
        dst->redir_in.filename = strdup(src->redir_in.filename);
    }
    if (src->redir_out.filename) {
        dst->redir_out.type = src->redir_out.type;
        dst->redir_out.filename = strdup(src->redir_out.filename);
    }
    if (src->heredoc_delim) {
        dst->heredoc_delim = strdup(src->heredoc_delim);
    }
    dst->heredoc_fd = -1;

    return dst;
}

AstNode *ast_clone(AstNode *node) {
    if (!node) return NULL;

    switch (node->type) {
    case NODE_COMMAND:
        return ast_new_command(clone_command(node->data.command.cmd));

    case NODE_PIPELINE: {
        int n = node->data.pipeline.cmd_count;
        AstNode **cmds = malloc(n * sizeof(AstNode *));
        for (int i = 0; i < n; i++) {
            cmds[i] = ast_clone(node->data.pipeline.cmds[i]);
        }
        return ast_new_pipeline(cmds, n, node->data.pipeline.negated);
    }

    case NODE_LIST: {
        int n = node->data.list.count;
        ListEntry *entries = malloc(n * sizeof(ListEntry));
        for (int i = 0; i < n; i++) {
            entries[i].pipeline = ast_clone(node->data.list.entries[i].pipeline);
            entries[i].sep = node->data.list.entries[i].sep;
        }
        return ast_new_list(entries, n);
    }

    case NODE_IF:
        return ast_new_if(
            ast_clone(node->data.if_clause.condition),
            ast_clone(node->data.if_clause.then_body),
            ast_clone(node->data.if_clause.else_body)
        );

    case NODE_WHILE:
        return ast_new_while(
            ast_clone(node->data.loop.condition),
            ast_clone(node->data.loop.body)
        );

    case NODE_UNTIL:
        return ast_new_until(
            ast_clone(node->data.loop.condition),
            ast_clone(node->data.loop.body)
        );

    case NODE_FOR: {
        int wc = node->data.for_clause.word_count;
        char **words = malloc((wc + 1) * sizeof(char *));
        for (int i = 0; i < wc; i++) {
            words[i] = strdup(node->data.for_clause.words[i]);
        }
        words[wc] = NULL;
        return ast_new_for(
            node->data.for_clause.var_name,
            words, wc,
            ast_clone(node->data.for_clause.body)
        );
    }

    case NODE_FUNCTION_DEF:
        return ast_new_function_def(
            node->data.func_def.name,
            ast_clone(node->data.func_def.body)
        );

    case NODE_NOT:
        return ast_new_not(ast_clone(node->data.not_clause.child));

    case NODE_SUBSHELL:
        return ast_new_subshell(ast_clone(node->data.subshell.body));
    }

    return NULL;
}

const char *ast_type_str(AstNodeType type) {
    switch (type) {
    case NODE_COMMAND:      return "Command";
    case NODE_PIPELINE:     return "Pipeline";
    case NODE_LIST:         return "List";
    case NODE_IF:           return "If";
    case NODE_WHILE:        return "While";
    case NODE_UNTIL:        return "Until";
    case NODE_FOR:          return "For";
    case NODE_FUNCTION_DEF: return "Function";
    case NODE_NOT:          return "Not";
    case NODE_SUBSHELL:     return "Subshell";
    }
    return "Unknown";
}
