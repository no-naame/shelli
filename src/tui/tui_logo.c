/*
 * shelli - Educational Shell
 * tui/tui_logo.c - ASCII art logo and splash screen with gradient glow effects
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "tui.h"

/*
 * Neon accent colors for logo
 */
#define LOGO_NEON_PINK    213   /* #ff79c6 */
#define LOGO_NEON_PURPLE  141   /* #bd93f9 */
#define LOGO_NEON_BLUE    111   /* #89b4fa */
#define LOGO_NEON_CYAN    123   /* #8be9fd */
#define LOGO_NEON_TEAL    116   /* #94e2d5 */

/*
 * Glow characters
 */
#define GLOW_1   "\342\226\221"  /* ░ */
#define GLOW_2   "\342\226\222"  /* ▒ */
#define GLOW_3   "\342\226\223"  /* ▓ */

/*
 * ASCII Art Logo with horizontal gradient (pink -> purple -> blue -> cyan -> teal)
 * Each letter uses a different gradient position
 */
static const char *LOGO[] = {
    /* S         H         E         L         L         I */
    "      \033[38;5;213m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;141m\342\226\210\342\226\210\342\225\227  \342\226\210\342\226\210\342\225\227\033[38;5;111m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;123m\342\226\210\342\226\210\342\225\227     \033[38;5;116m\342\226\210\342\226\210\342\225\227     \033[38;5;84m\342\226\210\342\226\210\342\225\227\033[0m",
    "      \033[38;5;213m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;141m\342\226\210\342\226\210\342\225\221  \342\226\210\342\226\210\342\225\221\033[38;5;111m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;123m\342\226\210\342\226\210\342\225\221     \033[38;5;116m\342\226\210\342\226\210\342\225\221     \033[38;5;84m\342\226\210\342\226\210\342\225\221\033[0m",
    "      \033[38;5;213m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;141m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\221\033[38;5;111m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227  \033[38;5;123m\342\226\210\342\226\210\342\225\221     \033[38;5;116m\342\226\210\342\226\210\342\225\221     \033[38;5;84m\342\226\210\342\226\210\342\225\221\033[0m",
    "      \033[38;5;213m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\226\210\342\226\210\342\225\221\033[38;5;141m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\226\210\342\226\210\342\225\221\033[38;5;111m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\225\235  \033[38;5;123m\342\226\210\342\226\210\342\225\221     \033[38;5;116m\342\226\210\342\226\210\342\225\221     \033[38;5;84m\342\226\210\342\226\210\342\225\221\033[0m",
    "      \033[38;5;213m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\221\033[38;5;141m\342\226\210\342\226\210\342\225\221  \342\226\210\342\226\210\342\225\221\033[38;5;111m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;123m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;116m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;84m\342\226\210\342\226\210\342\225\221\033[0m",
    "      \033[38;5;213m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;141m\342\225\232\342\225\220\342\225\235  \342\225\232\342\225\220\342\225\235\033[38;5;111m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;123m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;116m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;84m\342\225\232\342\225\220\342\225\235\033[0m",
    NULL
};

static const int LOGO_HEIGHT = 6;

/* Diamond decorations */
#define DIAMOND        "\342\227\206"  /* ◆ */
#define DIAMOND_EMPTY  "\342\227\207"  /* ◇ */

/* Tagline with diamond decorations */
static const char *TAGLINE = "see how shells work";

/* Press any key message */
static const char *PRESS_KEY = "Press any key to continue...";

/*
 * Get logo lines
 */
const char **logo_get_lines(void) {
    return LOGO;
}

/*
 * Get logo height
 */
int logo_get_height(void) {
    return LOGO_HEIGHT;
}

/*
 * Calculate visible string length (excluding ANSI escape codes)
 */
static int visible_length(const char *s) {
    int len = 0;
    int in_escape = 0;

    while (*s) {
        if (*s == '\033') {
            in_escape = 1;
        } else if (in_escape) {
            if (*s == 'm') {
                in_escape = 0;
            }
        } else {
            /* Count UTF-8 characters correctly */
            if ((*s & 0xC0) != 0x80) {
                len++;
            }
        }
        s++;
    }
    return len;
}

/*
 * Draw centered text
 */
static void draw_centered(int row, int width, const char *text, const char *color) {
    int text_len = visible_length(text);
    int col = (width - text_len) / 2;
    if (col < 1) col = 1;

    printf(CSI "%d;%dH", row, col);
    if (color) printf("%s", color);
    printf("%s", text);
    printf(COL_RESET);
}

/*
 * Draw glow border around a region
 */
static void draw_glow_border(int start_row, int width, int height, int box_width) {
    int start_col = (width - box_width) / 2 - 4;
    if (start_col < 1) start_col = 1;
    int end_col = start_col + box_width + 8;

    /* Top glow line */
    printf(CSI "%d;%dH", start_row - 1, start_col);
    printf(CSI "38;5;%dm", COL_OVERLAY);
    for (int i = 0; i < box_width + 8; i++) {
        printf("%s", GLOW_1);
    }
    printf(COL_RESET);

    /* Side glow (left and right) */
    for (int r = 0; r < height; r++) {
        /* Left glow */
        printf(CSI "%d;%dH", start_row + r, start_col);
        printf(CSI "38;5;%dm%s%s%s" COL_RESET, COL_OVERLAY, GLOW_1, GLOW_2, GLOW_3);

        /* Right glow */
        printf(CSI "%d;%dH", start_row + r, end_col - 3);
        printf(CSI "38;5;%dm%s%s%s" COL_RESET, COL_OVERLAY, GLOW_3, GLOW_2, GLOW_1);
    }

    /* Bottom glow line */
    printf(CSI "%d;%dH", start_row + height, start_col);
    printf(CSI "38;5;%dm", COL_OVERLAY);
    for (int i = 0; i < box_width + 8; i++) {
        printf("%s", GLOW_1);
    }
    printf(COL_RESET);
}

/*
 * Draw the splash screen with glow effects
 */
void splash_draw(int width, int height) {
    /* Clear screen with base background */
    printf(BG_BASE);
    printf(SCR_CLEAR);
    printf(CUR_HOME);

    /* Calculate vertical centering */
    int total_height = LOGO_HEIGHT + 6;  /* logo + glow + tagline + press key */
    int start_row = (height - total_height) / 2;
    if (start_row < 3) start_row = 3;

    /* Draw glow border around logo */
    int logo_width = 52;  /* Approximate visible width of logo */
    draw_glow_border(start_row, width, LOGO_HEIGHT, logo_width);

    /* Draw logo */
    for (int i = 0; i < LOGO_HEIGHT && LOGO[i]; i++) {
        int row = start_row + i;
        int logo_len = visible_length(LOGO[i]);
        int col = (width - logo_len) / 2;
        if (col < 1) col = 1;

        printf(CSI "%d;%dH", row, col);
        printf("%s", LOGO[i]);
    }

    /* Draw tagline with diamond decorations */
    char tagline_buf[128];
    snprintf(tagline_buf, sizeof(tagline_buf),
             CSI "38;5;%dm%s" COL_RESET FG_SUBTEXT " %s " COL_RESET CSI "38;5;%dm%s" COL_RESET,
             LOGO_NEON_PINK, DIAMOND, TAGLINE, LOGO_NEON_CYAN, DIAMOND);
    draw_centered(start_row + LOGO_HEIGHT + 2, width, tagline_buf, NULL);

    /* Draw press key message with subtle glow */
    printf(CSI "%d;%dH", start_row + LOGO_HEIGHT + 4, 1);
    char press_buf[128];
    snprintf(press_buf, sizeof(press_buf),
             CSI "38;5;%dm%s%s" COL_RESET FG_OVERLAY " %s " COL_RESET CSI "38;5;%dm%s%s" COL_RESET,
             COL_OVERLAY, GLOW_1, GLOW_2, PRESS_KEY, COL_OVERLAY, GLOW_2, GLOW_1);
    draw_centered(start_row + LOGO_HEIGHT + 4, width, press_buf, NULL);

    fflush(stdout);
}

/*
 * Animate splash screen with glow fade-in effect
 * Frame sequence: 0-1 dark, 2-3 glow building, 4+ full
 */
void splash_animate(int width, int height, int frame) {
    /* Calculate vertical centering */
    int total_height = LOGO_HEIGHT + 6;
    int start_row = (height - total_height) / 2;
    if (start_row < 3) start_row = 3;

    if (frame == 0) {
        /* Initial clear */
        printf(BG_BASE);
        printf(SCR_CLEAR);
        fflush(stdout);
    } else if (frame == 1) {
        /* First glow frame - show faint outline */
        printf(BG_BASE);
        printf(SCR_CLEAR);

        int logo_width = 52;
        int start_col = (width - logo_width) / 2 - 4;
        if (start_col < 1) start_col = 1;

        /* Draw faint glow outline */
        for (int r = 0; r < LOGO_HEIGHT; r++) {
            printf(CSI "%d;%dH", start_row + r, start_col);
            printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, GLOW_1);
            printf(CSI "%d;%dH", start_row + r, start_col + logo_width + 7);
            printf(CSI "38;5;%dm%s" COL_RESET, COL_OVERLAY, GLOW_1);
        }
        fflush(stdout);
    } else if (frame == 2) {
        /* Second glow frame - show medium glow */
        printf(BG_BASE);
        printf(SCR_CLEAR);

        int logo_width = 52;
        int start_col = (width - logo_width) / 2 - 4;
        if (start_col < 1) start_col = 1;

        /* Draw medium glow outline */
        for (int r = 0; r < LOGO_HEIGHT; r++) {
            printf(CSI "%d;%dH", start_row + r, start_col);
            printf(CSI "38;5;%dm%s%s" COL_RESET, COL_OVERLAY, GLOW_1, GLOW_2);
            printf(CSI "%d;%dH", start_row + r, start_col + logo_width + 6);
            printf(CSI "38;5;%dm%s%s" COL_RESET, COL_OVERLAY, GLOW_2, GLOW_1);
        }
        fflush(stdout);
    } else if (frame >= 3) {
        /* Show full splash with all effects */
        splash_draw(width, height);
    }
}
