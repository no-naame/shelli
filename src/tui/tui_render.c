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

static char result_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int result_count = 0;
static int result_exit_code = 0;

/* Current stage */
static TuiStage current_stage = STAGE_INPUT;
static int stage_completed[STAGE_COUNT] = {0};

/* Debug mode */
static int debug_mode = 0;

/* External function from tui_core.c */
int term_get_width(void);
int term_get_height(void);

/*
 * Box drawing characters - Light (for inner panels)
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

/*
 * Box drawing characters - Heavy (for outer frame)
 */
#define HEAVY_TL "\342\224\217"  /* ┏ */
#define HEAVY_TR "\342\224\223"  /* ┓ */
#define HEAVY_BL "\342\224\227"  /* ┗ */
#define HEAVY_BR "\342\224\233"  /* ┛ */
#define HEAVY_H  "\342\224\201"  /* ━ */
#define HEAVY_V  "\342\224\203"  /* ┃ */

/*
 * Double-line box characters (for accent)
 */
#define DOUBLE_TL "\342\225\224"  /* ╔ */
#define DOUBLE_TR "\342\225\227"  /* ╗ */
#define DOUBLE_BL "\342\225\232"  /* ╚ */
#define DOUBLE_BR "\342\225\235"  /* ╝ */
#define DOUBLE_H  "\342\225\220"  /* ═ */
#define DOUBLE_V  "\342\225\221"  /* ║ */

/*
 * Glow effect characters (for fade effects)
 */
#define GLOW_1   "\342\226\221"  /* ░ */
#define GLOW_2   "\342\226\222"  /* ▒ */
#define GLOW_3   "\342\226\223"  /* ▓ */
#define GLOW_4   "\342\226\210"  /* █ */

/*
 * Tree drawing characters
 */
#define TREE_VERT   "\342\224\202"  /* │ */
#define TREE_BRANCH "\342\224\234\342\224\200\342\224\200"  /* ├── */
#define TREE_LAST   "\342\224\224\342\224\200\342\224\200"  /* └── */
#define TREE_ARROW  "\342\206\223"  /* ↓ */
#define TREE_RARROW "\342\206\222"  /* → */

/*
 * Stage indicator symbols - Enhanced
 */
#define STAGE_FILLED   "\342\227\211"  /* ◉ (filled circle) */
#define STAGE_EMPTY    "\342\227\216"  /* ◎ (double circle) */
#define STAGE_CONNECT  "\342\224\201\342\224\201\342\224\201\342\224\201"  /* ━━━━ */

/* Legacy stage symbols (for compatibility) */
#define STAGE_ACTIVE   "\342\227\217"  /* ● (filled circle) */
#define STAGE_INACTIVE "\342\227\213"  /* ○ (empty circle) */
#define STAGE_DONE     "\342\234\223"  /* ✓ (checkmark) */

/*
 * Decorative elements
 */
#define DIAMOND        "\342\227\206"  /* ◆ */
#define DIAMOND_EMPTY  "\342\227\207"  /* ◇ */

/*
 * Move cursor to position
 */
static void move_to(int row, int col) {
    printf(CSI "%d;%dH", row, col);
}

/*
 * Print a horizontal line (light)
 */
static void print_hline(int count) {
    for (int i = 0; i < count; i++) {
        printf("%s", BOX_H);
    }
}

/*
 * Print a heavy horizontal line
 */
static void print_heavy_hline(int count) {
    for (int i = 0; i < count; i++) {
        printf("%s", HEAVY_H);
    }
}

/*
 * Draw empty row with heavy borders (for outer frame)
 */
static void draw_heavy_empty_row(int row, int width) {
    move_to(row, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("%*s", width - 2, "");
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
}

/*
 * Neon accent colors for enhanced visuals
 */
#define COL_NEON_PINK    213   /* #ff79c6 */
#define COL_NEON_CYAN    123   /* #8be9fd */
#define COL_NEON_PURPLE  141   /* #bd93f9 */
#define COL_MATRIX_GREEN 84    /* #50fa7b */

/*
 * Draw the enhanced stage indicator bar with gradient effect
 */
static void draw_stage_indicator(int row, int width) {
    move_to(row, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Calculate centering - enhanced design is wider */
    /* Stage bar: "◉ ━━━━ ◉ ━━━━ ◎ ━━━━ ◎ ━━━━ ◎" with labels below */
    int stage_bar_len = 55;  /* Approximate length */
    int padding = (width - 2 - stage_bar_len) / 2;
    if (padding < 4) padding = 4;

    printf("%*s", padding, "");

    /* Color gradient for stages */
    const int stage_colors[] = {COL_NEON_PINK, COL_NEON_PURPLE, COL_BLUE, COL_NEON_CYAN, COL_MATRIX_GREEN};

    for (int i = 0; i < STAGE_COUNT; i++) {
        /* Stage indicator */
        if (stage_completed[i]) {
            printf(CSI "38;5;%dm%s" COL_RESET, COL_MATRIX_GREEN, STAGE_FILLED);
        } else if (i == (int)current_stage) {
            /* Pulsing effect simulation - use bright color */
            printf(COL_BOLD CSI "38;5;%dm%s" COL_RESET, stage_colors[i], STAGE_FILLED);
        } else {
            printf(FG_OVERLAY "%s" COL_RESET, STAGE_EMPTY);
        }

        /* Connector (except for last) */
        if (i < STAGE_COUNT - 1) {
            if (stage_completed[i]) {
                printf(CSI "38;5;%dm %s " COL_RESET, COL_MATRIX_GREEN, STAGE_CONNECT);
            } else if (i == (int)current_stage) {
                /* Gradient connector */
                printf(CSI "38;5;%dm " COL_RESET, stage_colors[i]);
                printf(CSI "38;5;%dm%s%s" COL_RESET, stage_colors[i], HEAVY_H, HEAVY_H);
                printf(CSI "38;5;%dm%s%s" COL_RESET, stage_colors[i+1], HEAVY_H, HEAVY_H);
                printf(" ");
            } else {
                printf(FG_OVERLAY " %s " COL_RESET, STAGE_CONNECT);
            }
        }
    }

    /* Fill rest of line */
    move_to(row, width);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
}

/*
 * Draw stage labels row
 */
static void draw_stage_labels(int row, int width) {
    move_to(row, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    const char *names[] = {"INPUT", "TOKEN", "PARSE", "EXEC", "RESULT"};
    const int stage_colors[] = {COL_NEON_PINK, COL_NEON_PURPLE, COL_BLUE, COL_NEON_CYAN, COL_MATRIX_GREEN};

    /* Calculate padding to align with circles above */
    int stage_bar_len = 55;
    int padding = (width - 2 - stage_bar_len) / 2;
    if (padding < 4) padding = 4;

    /* Offset for label centering under circles */
    printf("%*s", padding - 2, "");

    for (int i = 0; i < STAGE_COUNT; i++) {
        if (stage_completed[i]) {
            printf(CSI "38;5;%dm%s" COL_RESET, COL_MATRIX_GREEN, names[i]);
        } else if (i == (int)current_stage) {
            printf(COL_BOLD CSI "38;5;%dm%s" COL_RESET, stage_colors[i], names[i]);
        } else {
            printf(FG_OVERLAY "%s" COL_RESET, names[i]);
        }

        if (i < STAGE_COUNT - 1) {
            printf("   ");  /* Spacing between labels */
        }
    }

    /* Fill rest of line */
    move_to(row, width);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
}

/*
 * Nerd Font icons (with ASCII fallbacks)
 */
#define ICON_TERMINAL   "\357\200\244"   /* nf-fa-terminal  */
#define ICON_KEYWORD    "\357\200\207"   /* nf-fa-asterisk  for keywords */
#define ICON_STRUCT     "\357\204\213"   /* nf-fa-sitemap   for structure */
#define ICON_PLAY       "\357\200\213"   /* nf-fa-play  */
#define ICON_CHECK      "\357\200\214"   /* nf-fa-check  */
#define ICON_FOLDER     "\357\200\266"   /* nf-fa-folder  */
#define ICON_COG        "\357\200\223"   /* nf-fa-cog  for execute */

/*
 * Draw a labeled box header with optional icon
 */
static void draw_box_header(int row, int col, int width, const char *label) {
    const char *icon = "";
    int label_color = COL_BLUE;

    /* Assign icons and colors based on label */
    if (strcmp(label, "INPUT") == 0) {
        icon = ICON_TERMINAL;
        label_color = COL_NEON_CYAN;
    } else if (strcmp(label, "TOKENIZE") == 0) {
        icon = ICON_KEYWORD;
        label_color = COL_NEON_PINK;
    } else if (strcmp(label, "PARSE") == 0) {
        icon = ICON_STRUCT;
        label_color = COL_NEON_PURPLE;
    } else if (strcmp(label, "EXECUTE") == 0) {
        icon = ICON_COG;
        label_color = COL_LAVENDER;
    } else if (strcmp(label, "RESULT") == 0) {
        icon = ICON_CHECK;
        label_color = COL_MATRIX_GREEN;
    }

    move_to(row, col);
    printf(FG_OVERLAY "%s%s " COL_RESET, BOX_TL, BOX_H);
    printf(CSI "38;5;%dm%s " COL_RESET, label_color, icon);
    printf(CSI "38;5;%dm%s" COL_RESET, label_color, label);
    printf(FG_OVERLAY " " COL_RESET);
    int label_len = (int)strlen(label) + 3;  /* icon + space + label */
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
 * Draw glow effect footer bar
 */
static void draw_glow_footer(int row, int width) {
    move_to(row, 1);

    /* Left glow: ░▒▓ */
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, GLOW_1);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_SUBTEXT, GLOW_2);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_TEXT, GLOW_3);
    printf(" ");

    /* Help items with subtle color coding */
    printf(CSI "38;5;%dm[?]" COL_RESET " ", COL_NEON_CYAN);
    printf(FG_SUBTEXT "help  " COL_RESET);

    printf(CSI "38;5;%dm[\342\206\221\342\206\223]" COL_RESET " ", COL_NEON_PURPLE);
    printf(FG_SUBTEXT "history  " COL_RESET);

    printf(CSI "38;5;%dm[^L]" COL_RESET " ", COL_NEON_PINK);
    printf(FG_SUBTEXT "clear  " COL_RESET);

    printf(CSI "38;5;%dm[q]" COL_RESET " ", COL_RED);
    printf(FG_SUBTEXT "quit" COL_RESET);

    /* Right glow: ▓▒░ at end of line */
    int fill_to = width - 4;
    move_to(row, fill_to);
    printf(" ");
    printf(CSI "38;5;%dm%s" COL_RESET, COL_TEXT, GLOW_3);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_SUBTEXT, GLOW_2);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, GLOW_1);
}

/*
 * Draw the complete frame with ultra-aesthetic design
 */
void tui_draw_frame(void) {
    int w, h;
    tui_get_size(&w, &h);

    int split_col = w / 2;

    /* Clear screen */
    printf(BG_BASE);
    printf(SCR_CLEAR);

    /*
     * Enhanced Layout (minimum 28 rows for best experience):
     * Row 1:     Heavy top border with gradient title
     * Row 2:     Empty (breathing room)
     * Row 3:     Empty
     * Row 4-5:   INPUT box header + content
     * Row 6:     INPUT box footer
     * Row 7:     Empty
     * Row 8:     Stage indicator circles
     * Row 9:     Stage labels
     * Row 10:    Empty
     * Row 11-16: TOKENS / AST boxes (side by side)
     * Row 17:    Empty
     * Row 18-22: EXECUTION box
     * Row 23:    Empty
     * Row 24-26: RESULT box
     * Row 27:    Empty
     * Row 28:    Heavy bottom border
     * Row h:     Glow footer bar
     */

    /* Row 1: Heavy top border with gradient title */
    move_to(1, 1);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, HEAVY_TL);
    print_heavy_hline(3);

    /* Gradient "shelli" title */
    printf(" ");
    printf(CSI "38;5;%dms" COL_RESET, COL_NEON_PINK);
    printf(CSI "38;5;%dmh" COL_RESET, COL_NEON_PURPLE);
    printf(CSI "38;5;%dme" COL_RESET, COL_LAVENDER);
    printf(CSI "38;5;%dml" COL_RESET, COL_BLUE);
    printf(CSI "38;5;%dml" COL_RESET, COL_NEON_CYAN);
    printf(CSI "38;5;%dmi" COL_RESET, COL_TEAL);
    printf(" ");

    /* Decorative diamond */
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, DIAMOND_EMPTY);
    printf(FG_SUBTEXT " see how shells work " COL_RESET);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, DIAMOND_EMPTY);

    printf(FG_OVERLAY);
    int title_fill = w - 38;
    if (title_fill > 0) print_heavy_hline(title_fill);
    printf(COL_RESET CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, HEAVY_TR);

    /* Row 2-3: Empty inside main box (breathing room) */
    draw_heavy_empty_row(2, w);
    draw_heavy_empty_row(3, w);

    /* Row 4: INPUT box header */
    move_to(4, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_header(4, 4, w - 6, "INPUT");
    move_to(4, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Row 5: INPUT content */
    move_to(5, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   " FG_OVERLAY "%s" COL_RESET, BOX_V);
    printf(" " CSI "38;5;%dm\342\235\257" COL_RESET " ", COL_NEON_CYAN);  /* ❯ prompt in neon cyan */
    printf(FG_TEXT "%s" COL_RESET, input_content);
    move_to(5, w - 3);
    printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
    move_to(5, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Row 6: INPUT box footer */
    move_to(6, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_footer(6, 4, w - 6, NULL);
    move_to(6, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Row 7: Empty */
    draw_heavy_empty_row(7, w);

    /* Row 8: Stage indicator */
    draw_stage_indicator(8, w);

    /* Row 9: Stage labels */
    draw_stage_labels(9, w);

    /* Row 10: Empty */
    draw_heavy_empty_row(10, w);

    /* Row 11: TOKENS / AST headers */
    int tok_width = split_col - 5;
    int parse_width = w - split_col - 4;

    move_to(11, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_header(11, 4, tok_width, "TOKENIZE");
    printf("  ");
    draw_box_header(11, split_col + 1, parse_width, "PARSE");
    move_to(11, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Rows 12-15: TOKENS / AST content */
    for (int r = 12; r <= 15; r++) {
        int line_idx = r - 12;
        move_to(r, 1);
        printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
        printf("   " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        /* TOKENS content */
        if (line_idx < tokenize_count) {
            printf("%s", tokenize_lines[line_idx]);
        }

        move_to(r, split_col - 1);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        printf("  " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        /* AST/PARSE content */
        if (line_idx < parse_count) {
            printf("%s", parse_lines[line_idx]);
        }

        move_to(r, w - 3);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        move_to(r, w);
        printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    }

    /* Row 16: TOKENS / AST footers */
    move_to(16, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_footer(16, 4, tok_width, NULL);
    printf("  ");
    draw_box_footer(16, split_col + 1, parse_width, NULL);
    move_to(16, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Row 17: Empty */
    draw_heavy_empty_row(17, w);

    /* Row 18: EXECUTION header */
    move_to(18, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_header(18, 4, w - 6, "EXECUTE");
    move_to(18, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Rows 19-21: EXECUTION content */
    for (int r = 19; r <= 21; r++) {
        int line_idx = r - 19;
        move_to(r, 1);
        printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
        printf("   " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        if (line_idx < exec_count) {
            printf("%s", exec_lines[line_idx]);
        }

        move_to(r, w - 3);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        move_to(r, w);
        printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    }

    /* Row 22: EXECUTION footer */
    move_to(22, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_footer(22, 4, w - 6, NULL);
    move_to(22, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Row 23: Empty */
    draw_heavy_empty_row(23, w);

    /* Row 24: RESULT header with exit code */
    move_to(24, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_header(24, 4, w - 6, "RESULT");
    move_to(24, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Rows 25-28: RESULT content (4 lines for output) */
    for (int r = 25; r <= 28; r++) {
        int line_idx = r - 25;
        move_to(r, 1);
        printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
        printf("   " FG_OVERLAY "%s" COL_RESET " ", BOX_V);

        if (line_idx < result_count) {
            printf(FG_TEXT "%s" COL_RESET, result_lines[line_idx]);
        }

        move_to(r, w - 3);
        printf(FG_OVERLAY "%s" COL_RESET, BOX_V);
        move_to(r, w);
        printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    }

    /* Row 29: RESULT footer with exit code */
    char exit_str[32];
    int status_color = (result_exit_code == 0) ? COL_MATRIX_GREEN : COL_RED;
    snprintf(exit_str, sizeof(exit_str), CSI "38;5;%dmexit: %d" COL_RESET, status_color, result_exit_code);
    move_to(29, 1);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);
    printf("   ");
    draw_box_footer(29, 4, w - 6, exit_str);
    move_to(29, w);
    printf(FG_OVERLAY "%s" COL_RESET, HEAVY_V);

    /* Row 30: Empty */
    if (h >= 30) {
        draw_heavy_empty_row(30, w);
    }

    /* Bottom border (heavy) */
    int bottom_row = (h >= 31) ? 31 : h - 1;
    move_to(bottom_row, 1);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, HEAVY_BL);
    print_heavy_hline(w - 2);
    printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, HEAVY_BR);

    /* Glow footer bar at very bottom */
    if (h >= 32) {
        draw_glow_footer(h, w);
    }

    fflush(stdout);
}

/*
 * Render input line with cursor (adjusted for new layout)
 */
void render_input_line(const char *line, int cursor_pos) {
    /* Update stored content */
    strncpy(input_content, line, MAX_LINE_LEN - 1);
    input_content[MAX_LINE_LEN - 1] = '\0';
    input_cursor = cursor_pos;

    /* Render just the input line area (row 5 in new layout) */
    move_to(5, 6);
    printf(CSI "38;5;%dm\342\235\257" COL_RESET " ", COL_NEON_CYAN);  /* ❯ in neon cyan */
    printf(FG_TEXT "%s" COL_RESET, line);

    /* Clear rest of line */
    printf(CSI "K");

    /* Position cursor (adjusted for new layout: col 8 + cursor_pos) */
    move_to(5, 8 + cursor_pos);

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
            result_count = 0;
            result_exit_code = 0;
            for (int i = 0; i < MAX_PANEL_LINES; i++) {
                result_lines[i][0] = '\0';
            }
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
    result_count = 0;
    result_exit_code = 0;
    for (int i = 0; i < MAX_PANEL_LINES; i++) {
        result_lines[i][0] = '\0';
    }

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
            if (result_count < MAX_PANEL_LINES) {
                strncpy(result_lines[result_count], content, MAX_LINE_LEN - 1);
                result_lines[result_count][MAX_LINE_LEN - 1] = '\0';
                result_count++;
            }
            break;
    }

    tui_draw_frame();
}

/*
 * Animation delay in microseconds (150ms per item)
 */
#define ANIM_DELAY_US 150000

/*
 * Display tokenization results with tree-style animation
 */
void tui_show_tokens(TokenList *tokens) {
    tokenize_count = 0;

    tui_stage_begin(STAGE_TOKENIZE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Pause before starting */

    for (int i = 0; i < tokens->count && tokenize_count < MAX_PANEL_LINES; i++) {
        Token *tok = &tokens->tokens[i];
        char buf[MAX_LINE_LEN];
        const char *tree_prefix;
        int is_last = (i == tokens->count - 1);

        /* Tree-style prefix */
        tree_prefix = is_last ? TREE_LAST : TREE_BRANCH;

        if (tok->value) {
            snprintf(buf, sizeof(buf), FG_OVERLAY "%s" COL_RESET CSI "38;5;%dm%s" COL_RESET " \"" FG_GREEN "%s" COL_RESET "\"",
                     tree_prefix, COL_NEON_PINK, token_type_str(tok->type), tok->value);
        } else {
            snprintf(buf, sizeof(buf), FG_OVERLAY "%s" COL_RESET CSI "38;5;%dm%s" COL_RESET,
                     tree_prefix, COL_NEON_PINK, token_type_str(tok->type));
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
 * Display parse results with tree-style AST animation
 */
void tui_show_pipeline(Pipeline *pipeline) {
    parse_count = 0;

    if (!pipeline) return;

    tui_stage_begin(STAGE_PARSE);
    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Pause before starting */

    /* Count total commands for tree structure */
    int total_cmds = 0;
    Command *tmp = pipeline->first;
    while (tmp) { total_cmds++; tmp = tmp->next; }

    /* Draw Pipeline root if multiple commands */
    if (total_cmds > 1 && parse_count < MAX_PANEL_LINES) {
        char buf[MAX_LINE_LEN];
        snprintf(buf, sizeof(buf), CSI "38;5;%dm%s Pipeline" COL_RESET,
                 COL_NEON_PURPLE, DIAMOND);
        strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
        parse_count++;
        tui_draw_frame();
        usleep(ANIM_DELAY_US);
    }

    Command *cmd = pipeline->first;
    int idx = 0;

    while (cmd && parse_count < MAX_PANEL_LINES) {
        char buf[MAX_LINE_LEN];
        char args[MAX_LINE_LEN] = "";
        int is_last_cmd = (cmd->next == NULL);

        /* Build argument string */
        for (int i = 0; i < cmd->argc && strlen(args) < MAX_LINE_LEN - 50; i++) {
            if (i > 0) strcat(args, " ");
            strncat(args, cmd->argv[i], MAX_LINE_LEN - strlen(args) - 1);
        }

        /* Format command line with tree structure */
        const char *prefix = (total_cmds > 1) ?
            (is_last_cmd ? TREE_LAST : TREE_BRANCH) : "";

        snprintf(buf, sizeof(buf), FG_OVERLAY "%s" COL_RESET CSI "38;5;%dmcmd[%d]:" COL_RESET " %s",
                 prefix, COL_PEACH, idx, args);

        strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
        parse_lines[parse_count][MAX_LINE_LEN - 1] = '\0';
        parse_count++;

        /* Animate: show command */
        tui_draw_frame();
        usleep(ANIM_DELAY_US);

        /* Tree continuation for child items */
        const char *tree_cont = (total_cmds > 1 && !is_last_cmd) ? TREE_VERT "   " : "    ";

        /* Show redirects with tree structure */
        if (cmd->redir_in.type && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "%s" CSI "38;5;%dm%s" COL_RESET " %s",
                     tree_cont, COL_YELLOW, "\342\227\204", cmd->redir_in.filename);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;

            tui_draw_frame();
            usleep(ANIM_DELAY_US);
        }
        if (cmd->redir_out.type && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "%s" CSI "38;5;%dm%s" COL_RESET " %s %s",
                     tree_cont, COL_YELLOW, TREE_RARROW,
                     cmd->redir_out.type == REDIR_APPEND ? ">>" : ">",
                     cmd->redir_out.filename);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;

            tui_draw_frame();
            usleep(ANIM_DELAY_US);
        }

        /* Show pipe indicator with arrow */
        if (cmd->next && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), FG_OVERLAY "%s" COL_RESET "   " CSI "38;5;%dm%s" COL_RESET " pipe",
                     TREE_VERT, COL_NEON_CYAN, TREE_ARROW);
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
 * Log execution message with enhanced animation
 */
void tui_log_exec(const char *message) {
    if (exec_count == 0) {
        tui_stage_begin(STAGE_EXECUTE);
        tui_draw_frame();
        usleep(ANIM_DELAY_US);
    }

    if (exec_count < MAX_PANEL_LINES) {
        /* Add spinner prefix with neon lavender color */
        char buf[MAX_LINE_LEN];
        snprintf(buf, sizeof(buf), CSI "38;5;%dm%s" COL_RESET " " CSI "38;5;%dm%s" COL_RESET " %s",
                 COL_LAVENDER, tui_spinner_frame(exec_count),
                 COL_NEON_PURPLE, TREE_RARROW,
                 message);

        strncpy(exec_lines[exec_count], buf, MAX_LINE_LEN - 1);
        exec_lines[exec_count][MAX_LINE_LEN - 1] = '\0';
        exec_count++;
    }

    tui_draw_frame();
    usleep(ANIM_DELAY_US);  /* Animate each log entry */
}

/*
 * Show result with full multi-line output display
 */
void tui_show_result(int exit_code, const char *output) {
    tui_stage_begin(STAGE_RESULT);
    tui_stage_end(STAGE_EXECUTE);

    /* Clear previous result */
    result_count = 0;
    result_exit_code = exit_code;

    if (output && output[0]) {
        /* Parse output into lines */
        const char *line_start = output;
        const char *p = output;

        while (*p && result_count < MAX_PANEL_LINES - 1) {
            if (*p == '\n') {
                int line_len = (int)(p - line_start);
                if (line_len > MAX_LINE_LEN - 1) line_len = MAX_LINE_LEN - 1;

                strncpy(result_lines[result_count], line_start, line_len);
                result_lines[result_count][line_len] = '\0';
                result_count++;

                line_start = p + 1;
            }
            p++;
        }

        /* Handle last line (if no trailing newline) */
        if (*line_start && result_count < MAX_PANEL_LINES - 1) {
            strncpy(result_lines[result_count], line_start, MAX_LINE_LEN - 1);
            result_lines[result_count][MAX_LINE_LEN - 1] = '\0';
            result_count++;
        }
    }

    tui_stage_end(STAGE_RESULT);
    tui_draw_frame();
}

/*
 * Show error message with enhanced styling
 */
void tui_show_error(const char *message) {
    result_count = 0;
    result_exit_code = 1;

    char buf[MAX_LINE_LEN];
    snprintf(buf, sizeof(buf), CSI "38;5;%dm\357\200\215" COL_RESET " " FG_RED "%s" COL_RESET,
             COL_RED, message);

    strncpy(result_lines[0], buf, MAX_LINE_LEN - 1);
    result_lines[0][MAX_LINE_LEN - 1] = '\0';
    result_count = 1;

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
