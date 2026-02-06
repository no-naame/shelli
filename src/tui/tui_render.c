/*
 * shelli - Educational Shell
 * tui/tui_render.c - Double-buffered rendering engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tui.h"

/*
 * Panel content buffers
 */
#define MAX_PANEL_LINES 32
#define MAX_LINE_LEN 512

static char input_content[MAX_LINE_LEN] = "";
static int input_cursor = 0;

static char tokenize_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int tokenize_count = 0;

static char parse_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int parse_count = 0;

static char exec_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int exec_count = 0;

static char result_content[MAX_LINE_LEN] = "";

/* Current stage */
static TuiStage current_stage = STAGE_INPUT;
static int stage_completed[STAGE_COUNT] = {0};

/* Debug mode */
static int debug_mode = 0;

/* External function from tui_core.c */
int term_get_width(void);
int term_get_height(void);

/*
 * Box drawing characters (Unicode - rounded corners)
 */
#define BOX_TL "\342\225\255"  /* ╭ */
#define BOX_TR "\342\225\256"  /* ╮ */
#define BOX_BL "\342\225\260"  /* ╰ */
#define BOX_BR "\342\225\257"  /* ╯ */
#define BOX_H  "\342\224\200"  /* ─ */
#define BOX_V  "\342\224\202"  /* │ */
#define BOX_LT "\342\224\234"  /* ├ */
#define BOX_RT "\342\224\244"  /* ┤ */
#define BOX_TT "\342\224\254"  /* ┬ */
#define BOX_BT "\342\224\264"  /* ┴ */
#define BOX_X  "\342\224\274"  /* ┼ */

/* Stage indicator symbols */
#define STAGE_ACTIVE   "\342\227\217"  /* ● (filled circle) */
#define STAGE_INACTIVE "\342\227\213"  /* ○ (empty circle) */
#define STAGE_DONE     "\342\234\223"  /* ✓ (checkmark) */

/*
 * Move cursor to position
 */
static void move_to(int row, int col) {
    printf(CSI "%d;%dH", row, col);
}

/*
 * Print a horizontal line
 */
static void print_hline(int count) {
    for (int i = 0; i < count; i++) {
        printf("%s", BOX_H);
    }
}

/*
 * Draw empty row with borders
 */
static void draw_empty_row(int row, int width) {
    move_to(row, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("%*s", width - 2, "");
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
}

/*
 * Draw the stage indicator bar
 */
static void draw_stage_indicator(int row, int width) {
    move_to(row, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Calculate centering */
    /* Stage bar: "● INPUT ─── ● TOKENIZE ─── ○ PARSE ─── ○ EXECUTE ─── ○ RESULT" */
    int stage_bar_len = 65;  /* Approximate length */
    int padding = (width - 2 - stage_bar_len) / 2;
    if (padding < 2) padding = 2;

    printf("%*s", padding, "");

    const char *names[] = {"INPUT", "TOKENIZE", "PARSE", "EXECUTE", "RESULT"};

    for (int i = 0; i < STAGE_COUNT; i++) {
        /* Stage indicator */
        if (stage_completed[i]) {
            printf(FG_GREEN "%s" COL_RESET, STAGE_DONE);
        } else if (i == (int)current_stage) {
            printf(FG_BLUE "%s" COL_RESET, STAGE_ACTIVE);
        } else {
            printf(FG_OVERLAY "%s" COL_RESET, STAGE_INACTIVE);
        }

        /* Stage name */
        if (i == (int)current_stage) {
            printf(FG_TEXT " %s" COL_RESET, names[i]);
        } else if (stage_completed[i]) {
            printf(FG_GREEN " %s" COL_RESET, names[i]);
        } else {
            printf(FG_OVERLAY " %s" COL_RESET, names[i]);
        }

        /* Connector */
        if (i < STAGE_COUNT - 1) {
            printf(FG_OVERLAY " %s%s%s " COL_RESET, BOX_H, BOX_H, BOX_H);
        }
    }

    /* Fill rest of line */
    move_to(row, width);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
}

/*
 * Draw a labeled box header
 */
static void draw_box_header(int row, int col, int width, const char *label) {
    move_to(row, col);
    printf(FG_OVERLAY "%s%s " COL_RESET, BOX_TL, BOX_H);
    printf(FG_BLUE "%s" COL_RESET, label);
    printf(FG_OVERLAY " " COL_RESET);
    int label_len = (int)strlen(label);
    int remaining = width - label_len - 5;
    if (remaining > 0) {
        print_hline(remaining);
    }
    printf(FG_OVERLAY "%s" COL_RESET, BOX_TR);
}

/*
 * Draw a box footer (optionally with right-aligned text)
 */
static void draw_box_footer(int row, int col, int width, const char *right_text) {
    move_to(row, col);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_BL);

    if (right_text && right_text[0]) {
        int text_len = (int)strlen(right_text);
        int fill = width - text_len - 4;
        if (fill > 0) {
            print_hline(fill);
        }
        printf(FG_OVERLAY " %s " COL_RESET, right_text);
    } else {
        print_hline(width - 2);
    }

    printf(FG_OVERLAY "%s" COL_RESET, BOX_BR);
}

/*
 * Draw the complete frame
 */
void tui_draw_frame(void) {
    int w, h;
    tui_get_size(&w, &h);

    int split_col = w / 2;

    /* Clear screen */
    printf(BG_BASE);
    printf(SCR_CLEAR);

    /*
     * Layout (minimum 24 rows):
     * Row 1:     Top border with title
     * Row 2:     Empty
     * Row 3-4:   INPUT box header + content
     * Row 5:     INPUT box footer
     * Row 6:     Empty
     * Row 7:     Stage indicator
     * Row 8:     Empty
     * Row 9-14:  TOKENIZE / PARSE boxes (side by side)
     * Row 15:    Empty
     * Row 16-20: EXECUTE box
     * Row 21:    Empty
     * Row 22-24: RESULT box
     * Row h:     Bottom bar with help
     */

    /* Row 1: Top border with title */
    move_to(1, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_TL);
    print_hline(2);
    printf(FG_BLUE " shelli " COL_RESET);
    printf(FG_OVERLAY);
    int title_fill = w - 12 - 12;
    if (title_fill > 0) print_hline(title_fill);
    printf(COL_RESET FG_TEAL "[?] help" COL_RESET FG_OVERLAY);
    print_hline(1);
    printf("%s" COL_RESET, BOX_TR);

    /* Row 2: Empty inside main box */
    draw_empty_row(2, w);

    /* Row 3: INPUT box header */
    move_to(3, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_header(3, 3, w - 4, "INPUT");
    move_to(3, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 4: INPUT content */
    move_to(4, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  " FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf(" " FG_TEAL "\342\235\257" COL_RESET " ");  /* ❯ prompt */
    printf("%s", input_content);
    move_to(4, w - 2);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    move_to(4, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 5: INPUT box footer */
    move_to(5, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_footer(5, 3, w - 4, NULL);
    move_to(5, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 6: Empty */
    draw_empty_row(6, w);

    /* Row 7: Stage indicator */
    draw_stage_indicator(7, w);

    /* Row 8: Empty */
    draw_empty_row(8, w);

    /* Row 9: TOKENIZE / PARSE headers */
    move_to(9, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    /* TOKENIZE header */
    int tok_width = split_col - 4;
    draw_box_header(9, 3, tok_width, "TOKENIZE");
    printf(" ");
    /* PARSE header */
    int parse_width = w - split_col - 3;
    draw_box_header(9, split_col + 1, parse_width, "PARSE");
    move_to(9, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Rows 10-13: TOKENIZE / PARSE content */
    for (int r = 10; r <= 13; r++) {
        int line_idx = r - 10;
        move_to(r, 1);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        printf("  " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        /* TOKENIZE content */
        if (line_idx < tokenize_count) {
            printf("%s", tokenize_lines[line_idx]);
        }

        move_to(r, split_col - 1);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        printf(" " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        /* PARSE content */
        if (line_idx < parse_count) {
            printf("%s", parse_lines[line_idx]);
        }

        move_to(r, w - 2);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        move_to(r, w);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    }

    /* Row 14: TOKENIZE / PARSE footers */
    move_to(14, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_footer(14, 3, tok_width, NULL);
    printf(" ");
    draw_box_footer(14, split_col + 1, parse_width, NULL);
    move_to(14, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 15: Empty */
    draw_empty_row(15, w);

    /* Row 16: EXECUTE header */
    move_to(16, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_header(16, 3, w - 4, "EXECUTE");
    move_to(16, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Rows 17-19: EXECUTE content */
    for (int r = 17; r <= 19; r++) {
        int line_idx = r - 17;
        move_to(r, 1);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        printf("  " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        if (line_idx < exec_count) {
            printf("%s", exec_lines[line_idx]);
        }

        move_to(r, w - 2);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        move_to(r, w);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    }

    /* Row 20: EXECUTE footer */
    move_to(20, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_footer(20, 3, w - 4, NULL);
    move_to(20, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 21: Empty */
    draw_empty_row(21, w);

    /* Row 22: RESULT header */
    move_to(22, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_header(22, 3, w - 4, "RESULT");
    move_to(22, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 23: RESULT content */
    move_to(23, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  " FG_OVERLAY "%s" COL_RESET " ", BOX_V);
    printf("%s", result_content);
    move_to(23, w - 2);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    move_to(23, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 24: RESULT footer */
    move_to(24, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf("  ");
    draw_box_footer(24, 3, w - 4, NULL);
    move_to(24, w);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);

    /* Row 25: Empty */
    if (h >= 25) {
        draw_empty_row(25, w);
    }

    /* Bottom border */
    int bottom_row = (h >= 26) ? 26 : h;
    move_to(bottom_row, 1);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_BL);
    print_hline(w - 2);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_BR);

    /* Help bar at very bottom */
    if (h >= 27) {
        move_to(h, 1);
        printf(FG_OVERLAY "  [?] help   [\342\206\221\342\206\223] history   [Tab] next   [Esc] cancel   [q] quit" COL_RESET);
    }

    fflush(stdout);
}

/*
 * Render input line with cursor
 */
void render_input_line(const char *line, int cursor_pos) {
    /* Update stored content */
    strncpy(input_content, line, MAX_LINE_LEN - 1);
    input_content[MAX_LINE_LEN - 1] = '\0';
    input_cursor = cursor_pos;

    /* Render just the input line area */
    move_to(4, 5);
    printf(FG_TEAL "\342\235\257" COL_RESET " ");  /* ❯ */
    printf(FG_TEXT "%s" COL_RESET, line);

    /* Clear rest of line */
    printf(CSI "K");

    /* Position cursor */
    move_to(4, 7 + cursor_pos);

    fflush(stdout);
}

/*
 * Set current stage
 */
void tui_stage_begin(TuiStage stage) {
    current_stage = stage;
    /* Redraw stage indicator */
    int w = term_get_width();
    draw_stage_indicator(7, w);
    fflush(stdout);
}

/*
 * Mark stage as complete
 */
void tui_stage_end(TuiStage stage) {
    stage_completed[stage] = 1;
    /* Redraw stage indicator */
    int w = term_get_width();
    draw_stage_indicator(7, w);
    fflush(stdout);
}

/*
 * Clear a panel
 */
void tui_clear_panel(PanelId panel) {
    switch (panel) {
        case PANEL_INPUT:
            input_content[0] = '\0';
            input_cursor = 0;
            break;

        case PANEL_TOKENIZE:
            tokenize_count = 0;
            for (int i = 0; i < MAX_PANEL_LINES; i++) {
                tokenize_lines[i][0] = '\0';
            }
            break;

        case PANEL_PARSE:
            parse_count = 0;
            for (int i = 0; i < MAX_PANEL_LINES; i++) {
                parse_lines[i][0] = '\0';
            }
            break;

        case PANEL_EXECUTE:
            exec_count = 0;
            for (int i = 0; i < MAX_PANEL_LINES; i++) {
                exec_lines[i][0] = '\0';
            }
            break;

        case PANEL_RESULT:
            result_content[0] = '\0';
            break;
    }

    /* Reset stages when clearing INPUT */
    if (panel == PANEL_INPUT) {
        for (int i = 0; i < STAGE_COUNT; i++) {
            stage_completed[i] = 0;
        }
        current_stage = STAGE_INPUT;
    }

    /* Redraw frame to show cleared panels */
    tui_draw_frame();
}

/*
 * Clear all processing panels (TOKENIZE, PARSE, EXECUTE, RESULT) at once
 * More efficient than calling tui_clear_panel 4 times
 */
void tui_clear_all_panels(void) {
    /* Clear TOKENIZE */
    tokenize_count = 0;
    for (int i = 0; i < MAX_PANEL_LINES; i++) {
        tokenize_lines[i][0] = '\0';
    }

    /* Clear PARSE */
    parse_count = 0;
    for (int i = 0; i < MAX_PANEL_LINES; i++) {
        parse_lines[i][0] = '\0';
    }

    /* Clear EXECUTE */
    exec_count = 0;
    for (int i = 0; i < MAX_PANEL_LINES; i++) {
        exec_lines[i][0] = '\0';
    }

    /* Clear RESULT */
    result_content[0] = '\0';

    /* Reset stages */
    for (int i = 0; i < STAGE_COUNT; i++) {
        stage_completed[i] = 0;
    }
    current_stage = STAGE_INPUT;

    /* Single redraw */
    tui_draw_frame();

    /* Small pause so user sees the cleared state */
    usleep(100000);  /* 100ms */
}

/*
 * Update panel content
 */
void tui_update_panel(PanelId panel, const char *content) {
    switch (panel) {
        case PANEL_INPUT:
            strncpy(input_content, content, MAX_LINE_LEN - 1);
            input_content[MAX_LINE_LEN - 1] = '\0';
            break;

        case PANEL_TOKENIZE:
            if (tokenize_count < MAX_PANEL_LINES) {
                strncpy(tokenize_lines[tokenize_count], content, MAX_LINE_LEN - 1);
                tokenize_lines[tokenize_count][MAX_LINE_LEN - 1] = '\0';
                tokenize_count++;
            }
            break;

        case PANEL_PARSE:
            if (parse_count < MAX_PANEL_LINES) {
                strncpy(parse_lines[parse_count], content, MAX_LINE_LEN - 1);
                parse_lines[parse_count][MAX_LINE_LEN - 1] = '\0';
                parse_count++;
            }
            break;

        case PANEL_EXECUTE:
            if (exec_count < MAX_PANEL_LINES) {
                strncpy(exec_lines[exec_count], content, MAX_LINE_LEN - 1);
                exec_lines[exec_count][MAX_LINE_LEN - 1] = '\0';
                exec_count++;
            }
            break;

        case PANEL_RESULT:
            strncpy(result_content, content, MAX_LINE_LEN - 1);
            result_content[MAX_LINE_LEN - 1] = '\0';
            break;
    }

    tui_draw_frame();
}

/*
 * Animation delay in microseconds (150ms per item)
 */
#define ANIM_DELAY_US 150000

/*
 * Display tokenization results with animation
 */
void tui_show_tokens(TokenList *tokens) {
    tokenize_count = 0;

    tui_stage_begin(STAGE_TOKENIZE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Pause before starting */

    for (int i = 0; i < tokens->count && tokenize_count < MAX_PANEL_LINES; i++) {
        Token *tok = &tokens->tokens[i];
        char buf[MAX_LINE_LEN];

        if (tok->value) {
            snprintf(buf, sizeof(buf), FG_PINK "[%s]" COL_RESET " \"" FG_GREEN "%s" COL_RESET "\"",
                     token_type_str(tok->type), tok->value);
        } else {
            snprintf(buf, sizeof(buf), FG_PINK "[%s]" COL_RESET,
                     token_type_str(tok->type));
        }

        strncpy(tokenize_lines[tokenize_count], buf, MAX_LINE_LEN - 1);
        tokenize_lines[tokenize_count][MAX_LINE_LEN - 1] = '\0';
        tokenize_count++;

        /* Animate: show each token one by one */
        tui_draw_frame();
        usleep(ANIM_DELAY_US);
    }

    tui_stage_end(STAGE_TOKENIZE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Pause after completion */
}

/*
 * Display parse results with animation
 */
void tui_show_pipeline(Pipeline *pipeline) {
    parse_count = 0;

    if (!pipeline) return;

    tui_stage_begin(STAGE_PARSE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Pause before starting */

    Command *cmd = pipeline->first;
    int idx = 0;

    while (cmd && parse_count < MAX_PANEL_LINES) {
        char buf[MAX_LINE_LEN];
        char args[MAX_LINE_LEN] = "";

        /* Build argument string */
        for (int i = 0; i < cmd->argc && strlen(args) < MAX_LINE_LEN - 50; i++) {
            if (i > 0) strcat(args, " ");
            strncat(args, cmd->argv[i], MAX_LINE_LEN - strlen(args) - 1);
        }

        /* Format command line */
        snprintf(buf, sizeof(buf), FG_PEACH "cmd[%d]:" COL_RESET " %s",
                 idx, args);

        strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
        parse_lines[parse_count][MAX_LINE_LEN - 1] = '\0';
        parse_count++;

        /* Animate: show command */
        tui_draw_frame();
        usleep(ANIM_DELAY_US);

        /* Show redirects */
        if (cmd->redir_in.type && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "   " FG_YELLOW "\342\227\204" COL_RESET " %s",
                     cmd->redir_in.filename);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;

            tui_draw_frame();
            usleep(ANIM_DELAY_US);
        }
        if (cmd->redir_out.type && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "   " FG_YELLOW "\342\226\272" COL_RESET " %s %s",
                     cmd->redir_out.type == REDIR_APPEND ? ">>" : ">",
                     cmd->redir_out.filename);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;

            tui_draw_frame();
            usleep(ANIM_DELAY_US);
        }

        /* Show pipe indicator */
        if (cmd->next && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "   " FG_TEAL "\342\206\223 pipe" COL_RESET);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;

            tui_draw_frame();
            usleep(ANIM_DELAY_US);
        }

        cmd = cmd->next;
        idx++;
    }

    tui_stage_end(STAGE_PARSE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Pause after completion */
}

/*
 * Log execution message with animation
 */
void tui_log_exec(const char *message) {
    if (exec_count == 0) {
        tui_stage_begin(STAGE_EXECUTE);
        tui_draw_frame();
        usleep(ANIM_DELAY_US);
    }

    if (exec_count < MAX_PANEL_LINES) {
        /* Add spinner prefix */
        char buf[MAX_LINE_LEN];
        snprintf(buf, sizeof(buf), FG_LAVENDER "%s" COL_RESET " %s",
                 tui_spinner_frame(exec_count), message);

        strncpy(exec_lines[exec_count], buf, MAX_LINE_LEN - 1);
        exec_lines[exec_count][MAX_LINE_LEN - 1] = '\0';
        exec_count++;
    }

    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Animate each log entry */
}

/*
 * Show result with exit code and animation
 */
void tui_show_result(int exit_code, const char *output) {
    tui_stage_begin(STAGE_RESULT);
    tui_stage_end(STAGE_EXECUTE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);

    char buf[MAX_LINE_LEN];

    if (output && output[0]) {
        snprintf(buf, sizeof(buf), "%s%*s" FG_OVERLAY "exit: " COL_RESET "%s%d" COL_RESET,
                 output,
                 40 - (int)strlen(output), "",
                 exit_code == 0 ? FG_GREEN : FG_RED,
                 exit_code);
    } else {
        snprintf(buf, sizeof(buf), "%*s" FG_OVERLAY "exit: " COL_RESET "%s%d" COL_RESET,
                 40, "",
                 exit_code == 0 ? FG_GREEN : FG_RED,
                 exit_code);
    }

    strncpy(result_content, buf, MAX_LINE_LEN - 1);
    result_content[MAX_LINE_LEN - 1] = '\0';

    tui_stage_end(STAGE_RESULT);
    tui_draw_frame();
}

/*
 * Show error message
 */
void tui_show_error(const char *message) {
    char buf[MAX_LINE_LEN];
    snprintf(buf, sizeof(buf), FG_RED "%s" COL_RESET, message);

    strncpy(result_content, buf, MAX_LINE_LEN - 1);
    result_content[MAX_LINE_LEN - 1] = '\0';

    tui_draw_frame();
}

/*
 * Wait for keypress in debug mode
 */
void tui_wait_step(const char *step_name) {
    if (!debug_mode) return;

    int h = term_get_height();

    move_to(h, 1);
    printf(FG_YELLOW "[DEBUG]" COL_RESET " %s - Press Enter to continue...", step_name);
    fflush(stdout);

    /* Wait for Enter */
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\r' || c == '\n') break;
    }

    move_to(h, 1);
    printf(SCR_CLEAR_LINE);
    fflush(stdout);
}

/*
 * Check debug mode
 */
int tui_is_debug(void) {
    return debug_mode;
}

/*
 * Set debug mode
 */
void tui_set_debug(int enabled) {
    debug_mode = enabled;
}
