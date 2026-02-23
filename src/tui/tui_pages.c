/*
 * shelli - Educational Shell
 * tui/tui_pages.c - Welcome page, stage browser, and StageData helpers
 *
 * The welcome page is the first screen shown on launch.
 * The stage browser lets users navigate between processing stages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tui.h"

/* Box drawing */
#define HEAVY_TL "\342\224\217"  /* ┏ */
#define HEAVY_TR "\342\224\223"  /* ┓ */
#define HEAVY_BL "\342\224\227"  /* ┗ */
#define HEAVY_BR "\342\224\233"  /* ┛ */
#define HEAVY_H  "\342\224\201"  /* ━ */
#define HEAVY_V  "\342\224\203"  /* ┃ */
#define DIAMOND  "\342\227\206"  /* ◆ */
#define GLOW_1   "\342\226\221"  /* ░ */
#define GLOW_2   "\342\226\222"  /* ▒ */

/*
 * ============================================================================
 * StageData Helpers
 * ============================================================================
 */

void stage_data_init(StageData *sd) {
    memset(sd, 0, sizeof(StageData));
}

void stage_data_set_input(StageData *sd, const char *input) {
    if (!input) { sd->input[0] = '\0'; return; }
    strncpy(sd->input, input, MAX_SD_LINE - 1);
    sd->input[MAX_SD_LINE - 1] = '\0';
}

void stage_data_add_token(StageData *sd, const char *value,
                          const char *label, int color, int is_cmd) {
    if (sd->token_count >= MAX_SD_TOKENS) return;
    TokenDisplay *td = &sd->tokens[sd->token_count];
    strncpy(td->value, value ? value : "", 63);
    td->value[63] = '\0';
    strncpy(td->label, label ? label : "", 31);
    td->label[31] = '\0';
    td->color = color;
    td->is_command = is_cmd;
    sd->token_count++;
}

void stage_data_add_ast_line(StageData *sd, const char *prefix,
                             const char *icon, const char *label,
                             const char *detail, int icon_color,
                             int detail_color) {
    if (sd->ast_count >= MAX_SD_AST_LINES) return;
    AstDisplayLine *al = &sd->ast_lines[sd->ast_count];
    strncpy(al->prefix, prefix ? prefix : "", 127);
    al->prefix[127] = '\0';
    strncpy(al->icon, icon ? icon : "", 7);
    al->icon[7] = '\0';
    strncpy(al->label, label ? label : "", 31);
    al->label[31] = '\0';
    strncpy(al->detail, detail ? detail : "", 255);
    al->detail[255] = '\0';
    al->icon_color = icon_color;
    al->detail_color = detail_color;
    sd->ast_count++;
}

void stage_data_add_expansion(StageData *sd, const char *original,
                              const char *expanded, const char *type_label) {
    if (sd->expansion_count >= MAX_SD_EXPANSIONS) return;
    ExpansionDisplay *ed = &sd->expansions[sd->expansion_count];
    strncpy(ed->original, original ? original : "", 255);
    ed->original[255] = '\0';
    strncpy(ed->expanded, expanded ? expanded : "", 511);
    ed->expanded[511] = '\0';
    strncpy(ed->type_label, type_label ? type_label : "", 63);
    ed->type_label[63] = '\0';
    sd->expansion_count++;
}

void stage_data_add_exec(StageData *sd, const char *text, char prefix_char,
                         int color, int indent) {
    if (sd->exec_count >= MAX_SD_EXEC_LINES) return;
    ExecLine *el = &sd->exec_lines[sd->exec_count];
    strncpy(el->text, text ? text : "", MAX_SD_LINE - 1);
    el->text[MAX_SD_LINE - 1] = '\0';
    el->prefix_char = prefix_char;
    el->color = color;
    el->indent = indent;
    sd->exec_count++;
}

void stage_data_set_output(StageData *sd, const char *output) {
    if (!output || !output[0]) { sd->output_count = 0; return; }
    sd->output_count = 0;
    const char *p = output;
    while (*p && sd->output_count < MAX_SD_OUTPUT_LINES) {
        const char *nl = strchr(p, '\n');
        int len;
        if (nl) {
            len = (int)(nl - p);
        } else {
            len = (int)strlen(p);
        }
        if (len >= MAX_SD_LINE) len = MAX_SD_LINE - 1;
        memcpy(sd->output_lines[sd->output_count], p, len);
        sd->output_lines[sd->output_count][len] = '\0';
        sd->output_count++;
        if (nl) p = nl + 1; else break;
    }
}

void stage_data_set_result(StageData *sd, int exit_code) {
    sd->exit_code = exit_code;
}

void stage_data_set_error(StageData *sd, const char *error) {
    if (!error) { sd->has_error = 0; return; }
    sd->has_error = 1;
    strncpy(sd->error_msg, error, MAX_SD_LINE - 1);
    sd->error_msg[MAX_SD_LINE - 1] = '\0';
}

/*
 * ============================================================================
 * Populate tokens from TokenList
 * ============================================================================
 */

void stage_data_populate_tokens(StageData *sd, TokenList *tokens) {
    sd->token_count = 0;
    int cmd_pos = 1; /* Next WORD is in command position */

    for (int i = 0; i < tokens->count; i++) {
        Token *tok = &tokens->tokens[i];
        if (tok->type == TOK_EOF) break;

        const char *val = tok->value ? tok->value : "";
        const char *type_str = token_type_str(tok->type);
        int color = token_type_color(tok->type);
        int is_cmd = 0;

        if (tok->type == TOK_WORD) {
            if (is_shell_keyword(val)) {
                color = COL_MAUVE;
                type_str = "KEYWORD";
            } else if (cmd_pos) {
                color = COL_PINK;
                is_cmd = 1;
            }
            cmd_pos = 0;
        } else {
            /* After pipe/semi/and/or, next WORD is command position */
            if (tok->type == TOK_PIPE || tok->type == TOK_SEMI ||
                tok->type == TOK_AND || tok->type == TOK_OR) {
                cmd_pos = 1;
            }
            /* Use operator text as display value */
            switch (tok->type) {
            case TOK_PIPE:      val = "|"; break;
            case TOK_SEMI:      val = ";"; break;
            case TOK_AND:       val = "&&"; break;
            case TOK_OR:        val = "||"; break;
            case TOK_BG:        val = "&"; break;
            case TOK_REDIR_IN:  val = "<"; break;
            case TOK_REDIR_OUT: val = ">"; break;
            case TOK_REDIR_APP: val = ">>"; break;
            case TOK_HEREDOC:   val = "<<"; break;
            default: break;
            }
        }

        /* Quoted strings get green */
        if (tok->quoted) {
            color = COL_GREEN;
            type_str = "STRING";
        }

        stage_data_add_token(sd, val, type_str, color, is_cmd);
    }
}

/*
 * ============================================================================
 * Populate AST lines from AstNode tree
 * ============================================================================
 */

/* Tree drawing chars */
#define TREE_BRANCH "\342\224\234\342\224\200\342\224\200"  /* ├── */
#define TREE_LAST   "\342\224\224\342\224\200\342\224\200"  /* └── */
#define TREE_PIPE   "\342\224\202"                          /* │ */

static void ast_walk(StageData *sd, AstNode *node, const char *prefix,
                     int is_last) {
    if (!node) return;
    char connector[256];
    char child_prefix[256];

    snprintf(connector, sizeof(connector), "%s%s ",
             prefix, is_last ? TREE_LAST : TREE_BRANCH);
    snprintf(child_prefix, sizeof(child_prefix), "%s%s   ",
             prefix, is_last ? "    " : TREE_PIPE "   ");

    switch (node->type) {
    case NODE_COMMAND: {
        Command *cmd = node->data.command.cmd;
        const char *name = (cmd && cmd->argc > 0) ? cmd->argv[0] : "";
        stage_data_add_ast_line(sd, connector, ">", "CMD", name,
                                COL_PINK, COL_TEXT);

        if (cmd) {
            for (int i = 1; i < cmd->argc; i++) {
                char arg_prefix[256];
                int last_arg = (i == cmd->argc - 1 &&
                                cmd->redir_in.type == 0 &&
                                cmd->redir_out.type == 0);
                snprintf(arg_prefix, sizeof(arg_prefix), "%s%s ",
                         child_prefix,
                         last_arg ? TREE_LAST : TREE_BRANCH);
                char detail[256];
                snprintf(detail, sizeof(detail), "arg: %s", cmd->argv[i]);
                stage_data_add_ast_line(sd, arg_prefix, "", "", detail,
                                        COL_OVERLAY, COL_TEXT);
            }
            if (cmd->redir_in.type) {
                char rp[256];
                int last_r = (cmd->redir_out.type == 0);
                snprintf(rp, sizeof(rp), "%s%s ",
                         child_prefix, last_r ? TREE_LAST : TREE_BRANCH);
                char detail[256];
                snprintf(detail, sizeof(detail), "redirect: < %s",
                         cmd->redir_in.filename ? cmd->redir_in.filename : "");
                stage_data_add_ast_line(sd, rp, "", "", detail,
                                        COL_OVERLAY, COL_TEAL);
            }
            if (cmd->redir_out.type) {
                char rp[256];
                snprintf(rp, sizeof(rp), "%s%s ",
                         child_prefix, TREE_LAST);
                const char *op = cmd->redir_out.type == REDIR_APPEND ? ">>" : ">";
                char detail[256];
                snprintf(detail, sizeof(detail), "redirect: %s %s",
                         op,
                         cmd->redir_out.filename ? cmd->redir_out.filename : "");
                stage_data_add_ast_line(sd, rp, "", "", detail,
                                        COL_OVERLAY, COL_TEAL);
            }
        }
        break;
    }
    case NODE_PIPELINE: {
        stage_data_add_ast_line(sd, connector, "|", "PIPELINE", "",
                                COL_BLUE, COL_TEXT);
        for (int i = 0; i < node->data.pipeline.cmd_count; i++) {
            int last = (i == node->data.pipeline.cmd_count - 1);
            ast_walk(sd, node->data.pipeline.cmds[i], child_prefix, last);
        }
        break;
    }
    case NODE_LIST: {
        char detail[64];
        snprintf(detail, sizeof(detail), "(%d entries)", node->data.list.count);
        stage_data_add_ast_line(sd, connector, "#", "LIST", detail,
                                COL_TEXT, COL_SUBTEXT);
        for (int i = 0; i < node->data.list.count; i++) {
            int last = (i == node->data.list.count - 1);
            const char *sep = "";
            switch (node->data.list.entries[i].sep) {
            case LIST_SEP_AND:  sep = "[&&]"; break;
            case LIST_SEP_OR:   sep = "[||]"; break;
            case LIST_SEP_SEMI: sep = "[;]"; break;
            case LIST_SEP_BG:   sep = "[&]"; break;
            default: break;
            }
            if (sep[0]) {
                char sp[256];
                snprintf(sp, sizeof(sp), "%s%s %s ",
                         child_prefix,
                         last ? TREE_LAST : TREE_BRANCH,
                         sep);
                /* Inline: render the child's content after the separator */
                AstNode *child = node->data.list.entries[i].pipeline;
                if (child) {
                    ast_walk(sd, child, child_prefix, last);
                }
            } else {
                ast_walk(sd, node->data.list.entries[i].pipeline,
                         child_prefix, last);
            }
        }
        break;
    }
    case NODE_IF: {
        stage_data_add_ast_line(sd, connector, "?", "IF", "",
                                COL_MAUVE, COL_TEXT);
        /* condition */
        {
            char lp[256];
            snprintf(lp, sizeof(lp), "%s%s ", child_prefix, TREE_BRANCH);
            stage_data_add_ast_line(sd, lp, "", "condition:", "",
                                    COL_OVERLAY, COL_TEXT);
            char cp2[256];
            snprintf(cp2, sizeof(cp2), "%s%s   ", child_prefix, TREE_PIPE "   ");
            if (node->data.if_clause.condition) {
                ast_walk(sd, node->data.if_clause.condition, cp2, 1);
            }
        }
        /* then */
        {
            int has_else = (node->data.if_clause.else_body != NULL);
            char lp[256];
            snprintf(lp, sizeof(lp), "%s%s ", child_prefix,
                     has_else ? TREE_BRANCH : TREE_LAST);
            stage_data_add_ast_line(sd, lp, "", "then:", "",
                                    COL_OVERLAY, COL_TEXT);
            char cp2[256];
            snprintf(cp2, sizeof(cp2), "%s%s   ", child_prefix,
                     has_else ? TREE_PIPE "   " : "    ");
            if (node->data.if_clause.then_body) {
                ast_walk(sd, node->data.if_clause.then_body, cp2, 1);
            }
        }
        /* else */
        if (node->data.if_clause.else_body) {
            char lp[256];
            snprintf(lp, sizeof(lp), "%s%s ", child_prefix, TREE_LAST);
            stage_data_add_ast_line(sd, lp, "", "else:", "",
                                    COL_OVERLAY, COL_TEXT);
            char cp2[256];
            snprintf(cp2, sizeof(cp2), "%s    ", child_prefix);
            ast_walk(sd, node->data.if_clause.else_body, cp2, 1);
        }
        break;
    }
    case NODE_WHILE:
    case NODE_UNTIL: {
        const char *kw = (node->type == NODE_WHILE) ? "WHILE" : "UNTIL";
        stage_data_add_ast_line(sd, connector, "@", kw, "",
                                COL_MAUVE, COL_TEXT);
        /* condition */
        {
            char lp[256];
            snprintf(lp, sizeof(lp), "%s%s ", child_prefix, TREE_BRANCH);
            stage_data_add_ast_line(sd, lp, "", "condition:", "",
                                    COL_OVERLAY, COL_TEXT);
            char cp2[256];
            snprintf(cp2, sizeof(cp2), "%s%s   ", child_prefix, TREE_PIPE "   ");
            if (node->data.loop.condition) {
                ast_walk(sd, node->data.loop.condition, cp2, 1);
            }
        }
        /* body */
        {
            char lp[256];
            snprintf(lp, sizeof(lp), "%s%s ", child_prefix, TREE_LAST);
            stage_data_add_ast_line(sd, lp, "", "body:", "",
                                    COL_OVERLAY, COL_TEXT);
            char cp2[256];
            snprintf(cp2, sizeof(cp2), "%s    ", child_prefix);
            if (node->data.loop.body) {
                ast_walk(sd, node->data.loop.body, cp2, 1);
            }
        }
        break;
    }
    case NODE_FOR: {
        char detail[256];
        snprintf(detail, sizeof(detail), "%s in",
                 node->data.for_clause.var_name ?
                 node->data.for_clause.var_name : "?");
        /* Append words */
        for (int i = 0; i < node->data.for_clause.word_count; i++) {
            int dlen = (int)strlen(detail);
            if (dlen + 1 + (int)strlen(node->data.for_clause.words[i]) < 250) {
                detail[dlen] = ' ';
                strcpy(detail + dlen + 1, node->data.for_clause.words[i]);
            }
        }
        stage_data_add_ast_line(sd, connector, "@", "FOR", detail,
                                COL_MAUVE, COL_TEXT);
        /* body */
        {
            char lp[256];
            snprintf(lp, sizeof(lp), "%s%s ", child_prefix, TREE_LAST);
            stage_data_add_ast_line(sd, lp, "", "body:", "",
                                    COL_OVERLAY, COL_TEXT);
            char cp2[256];
            snprintf(cp2, sizeof(cp2), "%s    ", child_prefix);
            if (node->data.for_clause.body) {
                ast_walk(sd, node->data.for_clause.body, cp2, 1);
            }
        }
        break;
    }
    case NODE_FUNCTION_DEF: {
        stage_data_add_ast_line(sd, connector, "f", "FUNCTION",
                                node->data.func_def.name ?
                                node->data.func_def.name : "",
                                COL_TEAL, COL_TEXT);
        if (node->data.func_def.body) {
            ast_walk(sd, node->data.func_def.body, child_prefix, 1);
        }
        break;
    }
    case NODE_NOT: {
        stage_data_add_ast_line(sd, connector, "!", "NOT", "",
                                COL_RED, COL_TEXT);
        if (node->data.not_clause.child) {
            ast_walk(sd, node->data.not_clause.child, child_prefix, 1);
        }
        break;
    }
    case NODE_SUBSHELL: {
        stage_data_add_ast_line(sd, connector, "$", "SUBSHELL", "",
                                COL_PEACH, COL_TEXT);
        if (node->data.subshell.body) {
            ast_walk(sd, node->data.subshell.body, child_prefix, 1);
        }
        break;
    }
    }
}

void stage_data_populate_ast(StageData *sd, AstNode *ast) {
    sd->ast_count = 0;
    if (!ast) return;

    /* Root node renders without connector prefix */
    switch (ast->type) {
    case NODE_PIPELINE:
        stage_data_add_ast_line(sd, "   ", "|", "PIPELINE", "",
                                COL_BLUE, COL_TEXT);
        for (int i = 0; i < ast->data.pipeline.cmd_count; i++) {
            int last = (i == ast->data.pipeline.cmd_count - 1);
            ast_walk(sd, ast->data.pipeline.cmds[i], "   ", last);
        }
        break;
    case NODE_LIST:
        stage_data_add_ast_line(sd, "   ", "#", "LIST", "",
                                COL_TEXT, COL_TEXT);
        for (int i = 0; i < ast->data.list.count; i++) {
            int last = (i == ast->data.list.count - 1);
            ast_walk(sd, ast->data.list.entries[i].pipeline, "   ", last);
        }
        break;
    default:
        ast_walk(sd, ast, "", 1);
        break;
    }
}

/*
 * ============================================================================
 * Welcome Page
 * ============================================================================
 */

int tui_welcome_page(void) {
    int tw, th;
    tui_get_size(&tw, &th);
    tui_render_ensure(th, tw);

    RenderBuf *rb, *prev;
    tui_render_get_bufs(&rb, &prev);
    rbuf_clear(rb);

    /* Draw heavy outer frame */
    rbuf_put_char(rb, 0, 0, HEAVY_TL, COL_OVERLAY, COL_BASE, 0, 0);
    for (int c = 1; c < tw - 1; c++)
        rbuf_put_char(rb, 0, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_char(rb, 0, tw - 1, HEAVY_TR, COL_OVERLAY, COL_BASE, 0, 0);
    for (int r = 1; r < th - 1; r++) {
        rbuf_put_char(rb, r, 0, HEAVY_V, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_char(rb, r, tw - 1, HEAVY_V, COL_OVERLAY, COL_BASE, 0, 0);
    }
    rbuf_put_char(rb, th - 1, 0, HEAVY_BL, COL_OVERLAY, COL_BASE, 0, 0);
    for (int c = 1; c < tw - 1; c++)
        rbuf_put_char(rb, th - 1, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_char(rb, th - 1, tw - 1, HEAVY_BR, COL_OVERLAY, COL_BASE, 0, 0);

    /* Logo - draw using tui_logo.c pre-rendered lines */
    const char **logo = logo_get_lines();
    int logo_h = logo_get_height();
    int start_row = (th - (logo_h + 16)) / 2;
    if (start_row < 3) start_row = 3;

    /* Render logo centered using printf (logo lines contain ANSI escapes) */
    /* Since logo has embedded ANSI we write the logo via direct printf */
    /* First flush the frame via RenderBuf, then overlay logo with printf */
    tui_render_flush();

    /* Draw logo lines centered */
    for (int i = 0; i < logo_h && logo[i]; i++) {
        /* Calculate visible length for centering */
        int vlen = 0;
        const char *s = logo[i];
        int in_esc = 0;
        while (*s) {
            if (*s == '\033') in_esc = 1;
            else if (in_esc) { if (*s == 'm') in_esc = 0; }
            else if ((*s & 0xC0) != 0x80) vlen++;
            s++;
        }
        int col = (tw - vlen) / 2;
        if (col < 1) col = 1;
        printf(CSI "%d;%dH%s" COL_RESET, start_row + i, col, logo[i]);
    }

    /* Tagline */
    int tag_row = start_row + logo_h + 2;
    const char *tagline = "see how shells work";
    int tag_len = (int)strlen(tagline) + 4; /* diamonds + spaces */
    int tag_col = (tw - tag_len) / 2;
    printf(CSI "%d;%dH", tag_row, tag_col);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_NEON_PINK, DIAMOND);
    printf(CSI "38;5;%dm %s ", COL_SUBTEXT, tagline);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_NEON_CYAN, DIAMOND);

    /* Mascot */
    int mascot_row = tag_row + 2;
    /* Render mascot via RenderBuf on second pass */
    rbuf_clear(rb);

    /* Re-draw frame into buffer (we already flushed, but need buffer state) */
    rbuf_put_char(rb, 0, 0, HEAVY_TL, COL_OVERLAY, COL_BASE, 0, 0);
    for (int c = 1; c < tw - 1; c++)
        rbuf_put_char(rb, 0, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_char(rb, 0, tw - 1, HEAVY_TR, COL_OVERLAY, COL_BASE, 0, 0);
    for (int r = 1; r < th - 1; r++) {
        rbuf_put_char(rb, r, 0, HEAVY_V, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_char(rb, r, tw - 1, HEAVY_V, COL_OVERLAY, COL_BASE, 0, 0);
    }

    /* Mascot centered */
    int mascot_x = tw / 2 - 4;
    static int welcome_mascot_frame = 0;
    mascot_render(rb, mascot_x, mascot_row, MOOD_NORMAL, welcome_mascot_frame);
    welcome_mascot_frame = (welcome_mascot_frame + 1) % 2;

    /* Description text */
    int desc_row = mascot_row + 7;
    const char *desc[] = {
        "Shelli is an educational shell that shows you exactly",
        "what happens inside a computer when you run a command.",
        "",
        "You'll see each step: tokenizing, parsing into an AST,",
        "word expansion, process execution, and the final result."
    };
    for (int i = 0; i < 5 && desc_row + i < th - 4; i++) {
        int dl = (int)strlen(desc[i]);
        int dx = (tw - dl) / 2;
        if (dx < 4) dx = 4;
        rbuf_put_str(rb, desc_row + i, dx, desc[i], COL_TEXT, COL_BASE, 0, 0);
    }

    /* Press Enter message */
    int press_row = desc_row + 6;
    if (press_row < th - 3) {
        const char *press = "Press Enter to start";
        int pl = (int)strlen(press) + 4;
        int px = (tw - pl) / 2;
        rbuf_put_str(rb, press_row, px, GLOW_1, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_str(rb, press_row, px + 1, GLOW_2, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_str(rb, press_row, px + 2, " ", -1, COL_BASE, 0, 0);
        rbuf_put_str(rb, press_row, px + 3, press, COL_OVERLAY, COL_BASE, 0, 0);
        int end = px + 3 + (int)strlen(press);
        rbuf_put_str(rb, press_row, end, " ", -1, COL_BASE, 0, 0);
        rbuf_put_str(rb, press_row, end + 1, GLOW_2, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_str(rb, press_row, end + 2, GLOW_1, COL_OVERLAY, COL_BASE, 0, 0);
    }

    /* Hint bar at bottom */
    rbuf_put_char(rb, th - 1, 0, HEAVY_BL, COL_OVERLAY, COL_SURFACE, 0, 0);
    for (int c = 1; c < tw - 1; c++)
        rbuf_put_char(rb, th - 1, c, " ", -1, COL_SURFACE, 0, 0);
    rbuf_put_char(rb, th - 1, tw - 1, HEAVY_BR, COL_OVERLAY, COL_SURFACE, 0, 0);
    rbuf_put_str(rb, th - 1, 2, "[Enter]", COL_TEXT, COL_SURFACE, 0, 0);
    rbuf_put_str(rb, th - 1, 10, "start", COL_OVERLAY, COL_SURFACE, 0, 0);
    rbuf_put_str(rb, th - 1, tw - 10, "[Q]", COL_TEXT, COL_SURFACE, 0, 0);
    rbuf_put_str(rb, th - 1, tw - 6, "quit", COL_OVERLAY, COL_SURFACE, 0, 0);

    tui_render_flush();
    fflush(stdout);

    /* Wait for Enter or Q */
    while (1) {
        if (tui_resize_pending()) {
            tui_handle_resize();
            /* Recursive redraw */
            return tui_welcome_page();
        }
        KeyEvent key = tui_read_key();
        if (key.code == KEY_ENTER || key.code == KEY_CTRL_D ||
            (key.code == KEY_CHAR && (key.ch == 'q' || key.ch == 'Q'))) {
            /*
             * The logo was rendered via direct printf (bypassing the render
             * buffer), so we must clear the screen and invalidate the prev
             * buffer so the next page does a full redraw.
             */
            printf(SCR_CLEAR CUR_HOME);
            fflush(stdout);
            RenderBuf *f, *b;
            tui_render_get_bufs(&f, &b);
            for (int i = 0; i < b->rows * b->cols; i++)
                b->cells[i].fg = -99;

            if (key.code == KEY_ENTER) return 0;
            return 1;
        }
    }
}

/*
 * ============================================================================
 * Stage Browser
 * ============================================================================
 */

int tui_stage_browser(StageData *data) {
    TuiStage current = STAGE_TOKENIZE;
    int scroll[STAGE_COUNT] = {0};

    while (1) {
        int tw, th;
        tui_get_size(&tw, &th);
        tui_render_ensure(th, tw);

        RenderBuf *rb, *prev;
        tui_render_get_bufs(&rb, &prev);
        rbuf_clear(rb);

        /* Render current stage page */
        switch (current) {
        case STAGE_TOKENIZE:
            page_render_tokenize(rb, data, tw, th, scroll[current]);
            break;
        case STAGE_AST:
            page_render_ast(rb, data, tw, th, scroll[current]);
            break;
        case STAGE_EXPAND:
            page_render_expand(rb, data, tw, th, scroll[current]);
            break;
        case STAGE_EXECUTE:
            page_render_execute(rb, data, tw, th, scroll[current]);
            break;
        case STAGE_RESULT:
            page_render_result(rb, data, tw, th, scroll[current]);
            break;
        default:
            break;
        }

        tui_render_flush();

        /* Handle input */
        KeyEvent key = tui_read_key();

        if (key.code == KEY_NONE) {
            if (tui_resize_pending()) {
                tui_handle_resize();
                /* Force full redraw */
                RenderBuf *f, *b;
                tui_render_get_bufs(&f, &b);
                for (int i = 0; i < b->rows * b->cols; i++)
                    b->cells[i].fg = -99;
            }
            continue;
        }

        switch (key.code) {
        case KEY_RIGHT:
            if ((int)current < STAGE_COUNT - 1) {
                current++;
            }
            break;
        case KEY_LEFT:
            if ((int)current > 0) {
                current--;
            }
            break;
        case KEY_UP:
            if (scroll[current] > 0) scroll[current]--;
            break;
        case KEY_DOWN:
            scroll[current]++;
            break;
        case KEY_ENTER:
            return 0; /* New command */
        case KEY_CHAR:
            if (key.ch == 'q' || key.ch == 'Q') return 1; /* Quit */
            break;
        case KEY_CTRL_D:
            return 1; /* Quit */
        default:
            break;
        }
    }
}
