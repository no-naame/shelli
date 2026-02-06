/*
 * shelli - Educational Shell
 * tui/tui_widgets.c - Reusable UI widgets (panels, boxes, spinners, progress)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tui.h"

/*
 * Box drawing characters (rounded corners)
 */
#define BOX_TL "\342\225\255"  /* ╭ */
#define BOX_TR "\342\225\256"  /* ╮ */
#define BOX_BL "\342\225\260"  /* ╰ */
#define BOX_BR "\342\225\257"  /* ╯ */
#define BOX_H  "\342\224\200"  /* ─ */
#define BOX_V  "\342\224\202"  /* │ */

/*
 * Spinner frames (Braille pattern)
 */
static const char *SPINNER[] = {
    "\342\240\213",  /* ⠋ */
    "\342\240\231",  /* ⠙ */
    "\342\240\271",  /* ⠹ */
    "\342\240\270",  /* ⠸ */
    "\342\240\274",  /* ⠼ */
    "\342\240\264",  /* ⠴ */
    "\342\240\246",  /* ⠦ */
    "\342\240\247",  /* ⠧ */
    "\342\240\207",  /* ⠇ */
    "\342\240\217",  /* ⠏ */
};

#define SPINNER_COUNT 10

/*
 * Progress bar characters
 */
#define PROG_FULL  "\342\226\210"  /* █ */
#define PROG_SEVEN "\342\226\211"  /* ▉ */
#define PROG_SIX   "\342\226\212"  /* ▊ */
#define PROG_FIVE  "\342\226\213"  /* ▋ */
#define PROG_FOUR  "\342\226\214"  /* ▌ */
#define PROG_THREE "\342\226\215"  /* ▍ */
#define PROG_TWO   "\342\226\216"  /* ▎ */
#define PROG_ONE   "\342\226\217"  /* ▏ */
#define PROG_EMPTY " "

/*
 * Move cursor helper
 */
static void widget_move(int row, int col) {
    printf(CSI "%d;%dH", row, col);
}

/*
 * Print horizontal line of given character
 */
static void widget_hline(int count, const char *ch) {
    for (int i = 0; i < count; i++) {
        printf("%s", ch);
    }
}

/*
 * Draw a rounded box at position
 */
void widget_box(int x, int y, int width, int height,
                const char *title, int color) {
    /* Top border */
    widget_move(y, x);
    printf(CSI "38;5;%dm", color);

    printf("%s", BOX_TL);

    if (title && title[0]) {
        printf("%s ", BOX_H);
        printf(COL_RESET);
        printf(CSI "38;5;%dm%s", COL_BLUE, title);
        printf(CSI "38;5;%dm", color);
        printf(" ");
        int title_len = (int)strlen(title);
        widget_hline(width - title_len - 5, BOX_H);
    } else {
        widget_hline(width - 2, BOX_H);
    }

    printf("%s" COL_RESET, BOX_TR);

    /* Side borders */
    for (int row = 1; row < height - 1; row++) {
        widget_move(y + row, x);
        printf(CSI "38;5;%dm%s" COL_RESET, color, BOX_V);
        widget_move(y + row, x + width - 1);
        printf(CSI "38;5;%dm%s" COL_RESET, color, BOX_V);
    }

    /* Bottom border */
    widget_move(y + height - 1, x);
    printf(CSI "38;5;%dm", color);
    printf("%s", BOX_BL);
    widget_hline(width - 2, BOX_H);
    printf("%s" COL_RESET, BOX_BR);
}

/*
 * Get spinner character for frame
 */
const char *widget_spinner(int frame) {
    return SPINNER[frame % SPINNER_COUNT];
}

/*
 * Draw a spinner at position
 */
void widget_draw_spinner(int x, int y, int frame, int color) {
    widget_move(y, x);
    printf(CSI "38;5;%dm%s" COL_RESET, color, widget_spinner(frame));
}

/*
 * Draw a progress bar
 * percent: 0.0 to 1.0
 */
void widget_progress(int x, int y, int width, double percent, int color) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 1.0) percent = 1.0;

    int filled = (int)(percent * width);
    double remainder = (percent * width) - filled;

    widget_move(y, x);
    printf(CSI "38;5;%dm", color);

    /* Full blocks */
    for (int i = 0; i < filled; i++) {
        printf("%s", PROG_FULL);
    }

    /* Partial block */
    if (filled < width) {
        int partial = (int)(remainder * 8);
        switch (partial) {
            case 7: printf("%s", PROG_SEVEN); break;
            case 6: printf("%s", PROG_SIX); break;
            case 5: printf("%s", PROG_FIVE); break;
            case 4: printf("%s", PROG_FOUR); break;
            case 3: printf("%s", PROG_THREE); break;
            case 2: printf("%s", PROG_TWO); break;
            case 1: printf("%s", PROG_ONE); break;
            default: printf("%s", PROG_EMPTY); break;
        }
        filled++;
    }

    /* Empty space */
    printf(CSI "38;5;%dm", COL_OVERLAY);
    for (int i = filled; i < width; i++) {
        printf("%s", PROG_EMPTY);
    }

    printf(COL_RESET);
}

/*
 * Draw stage indicator
 * stages: number of stages
 * current: current stage (0-indexed)
 * completed: bitmask of completed stages
 */
void widget_stages(int x, int y, int stages, int current, int completed,
                   const char **labels) {
    widget_move(y, x);

    for (int i = 0; i < stages; i++) {
        int is_complete = (completed >> i) & 1;
        int is_current = (i == current);

        /* Circle indicator */
        if (is_complete) {
            printf(FG_GREEN "\342\234\223" COL_RESET);  /* ✓ */
        } else if (is_current) {
            printf(FG_BLUE "\342\227\217" COL_RESET);   /* ● */
        } else {
            printf(FG_OVERLAY "\342\227\213" COL_RESET); /* ○ */
        }

        /* Label */
        if (labels && labels[i]) {
            if (is_complete) {
                printf(FG_GREEN " %s" COL_RESET, labels[i]);
            } else if (is_current) {
                printf(FG_TEXT " %s" COL_RESET, labels[i]);
            } else {
                printf(FG_OVERLAY " %s" COL_RESET, labels[i]);
            }
        }

        /* Connector (except for last) */
        if (i < stages - 1) {
            printf(FG_OVERLAY " %s%s%s " COL_RESET, BOX_H, BOX_H, BOX_H);
        }
    }
}

/*
 * Draw a panel with title and content lines
 */
void widget_panel(int x, int y, int width, int height,
                  const char *title, const char **lines, int line_count,
                  int border_color) {
    /* Draw box */
    widget_box(x, y, width, height, title, border_color);

    /* Draw content lines */
    int max_lines = height - 2;
    (void)width;  /* Reserved for future truncation */

    for (int i = 0; i < max_lines && i < line_count; i++) {
        widget_move(y + 1 + i, x + 2);

        if (lines[i]) {
            /* Truncate if needed (simple approach) */
            printf("%s", lines[i]);
        }
    }
}

/*
 * Draw centered text
 */
void widget_centered_text(int y, int screen_width, const char *text, int color) {
    int text_len = (int)strlen(text);
    int x = (screen_width - text_len) / 2;
    if (x < 1) x = 1;

    widget_move(y, x);
    printf(CSI "38;5;%dm%s" COL_RESET, color, text);
}

/*
 * Draw a horizontal divider
 */
void widget_divider(int x, int y, int width, int color) {
    widget_move(y, x);
    printf(CSI "38;5;%dm", color);
    widget_hline(width, BOX_H);
    printf(COL_RESET);
}

/*
 * Draw a label with value
 */
void widget_label_value(int x, int y, const char *label, const char *value,
                        int label_color, int value_color) {
    widget_move(y, x);
    printf(CSI "38;5;%dm%s:" COL_RESET " ", label_color, label);
    printf(CSI "38;5;%dm%s" COL_RESET, value_color, value);
}

/*
 * Draw a badge (label with background)
 */
void widget_badge(int x, int y, const char *text, int fg_color, int bg_color) {
    widget_move(y, x);
    printf(CSI "38;5;%dm" CSI "48;5;%dm %s " COL_RESET, fg_color, bg_color, text);
}
