/*
 * shelli - Educational Shell
 * ast.h - Abstract Syntax Tree node types
 *
 * The AST represents the parsed structure of shell commands.
 * It supports nested structures (if/while/for/functions) via
 * a tree of AstNode pointers.
 */

#ifndef AST_H
#define AST_H

#include "parser.h"

typedef enum {
    NODE_COMMAND,       /* Simple command: argv + redirects */
    NODE_PIPELINE,      /* cmd1 | cmd2 | cmd3 */
    NODE_LIST,          /* pipeline ; pipeline && pipeline || pipeline */
    NODE_IF,            /* if cond; then body; [elif cond; then body;]* [else body;] fi */
    NODE_WHILE,         /* while cond; do body; done */
    NODE_UNTIL,         /* until cond; do body; done */
    NODE_FOR,           /* for var in words; do body; done */
    NODE_FUNCTION_DEF,  /* name() { body; } */
    NODE_NOT,           /* ! pipeline (invert exit code) */
    NODE_SUBSHELL       /* ( list ) */
} AstNodeType;

/* List separator types */
typedef enum {
    LIST_SEP_SEMI,      /* ; (sequential) */
    LIST_SEP_AND,       /* && (conditional and) */
    LIST_SEP_OR,        /* || (conditional or) */
    LIST_SEP_BG,        /* & (background) */
    LIST_SEP_NONE       /* end of list */
} ListSepType;

/* A list entry: a pipeline with a separator */
typedef struct {
    struct AstNode *pipeline;
    ListSepType sep;
} ListEntry;

struct AstNode {
    AstNodeType type;

    union {
        /* NODE_COMMAND */
        struct {
            Command *cmd;   /* Owns the Command (freed with ast_free) */
        } command;

        /* NODE_PIPELINE */
        struct {
            AstNode **cmds;     /* Array of NODE_COMMAND nodes */
            int cmd_count;
            int negated;        /* ! prefix */
        } pipeline;

        /* NODE_LIST */
        struct {
            ListEntry *entries;
            int count;
        } list;

        /* NODE_IF */
        struct {
            AstNode *condition;     /* condition list */
            AstNode *then_body;     /* then body list */
            AstNode *else_body;     /* else body list (or another NODE_IF for elif) */
        } if_clause;

        /* NODE_WHILE / NODE_UNTIL */
        struct {
            AstNode *condition;
            AstNode *body;
        } loop;

        /* NODE_FOR */
        struct {
            char *var_name;
            char **words;       /* NULL-terminated array of words */
            int word_count;
            AstNode *body;
        } for_clause;

        /* NODE_FUNCTION_DEF */
        struct {
            char *name;
            AstNode *body;
        } func_def;

        /* NODE_NOT */
        struct {
            AstNode *child;
        } not_clause;

        /* NODE_SUBSHELL */
        struct {
            AstNode *body;
        } subshell;
    } data;
};

/* Create a new command node from a Command struct (takes ownership) */
AstNode *ast_new_command(Command *cmd);

/* Create a pipeline node from an array of command nodes */
AstNode *ast_new_pipeline(AstNode **cmds, int count, int negated);

/* Create a list node from entries */
AstNode *ast_new_list(ListEntry *entries, int count);

/* Create an if node */
AstNode *ast_new_if(AstNode *condition, AstNode *then_body, AstNode *else_body);

/* Create a while/until node */
AstNode *ast_new_while(AstNode *condition, AstNode *body);
AstNode *ast_new_until(AstNode *condition, AstNode *body);

/* Create a for node */
AstNode *ast_new_for(const char *var, char **words, int word_count, AstNode *body);

/* Create a function definition node */
AstNode *ast_new_function_def(const char *name, AstNode *body);

/* Create a NOT node */
AstNode *ast_new_not(AstNode *child);

/* Create a subshell node */
AstNode *ast_new_subshell(AstNode *body);

/* Free an AST node recursively */
void ast_free(AstNode *node);

/* Clone an AST node (deep copy, for function bodies) */
AstNode *ast_clone(AstNode *node);

/* Get string representation of node type */
const char *ast_type_str(AstNodeType type);

#endif /* AST_H */
