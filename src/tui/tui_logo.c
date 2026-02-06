/*
 * shelli - Educational Shell
 * tui/tui_logo.c - ASCII art logo and splash screen
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "tui.h"

/*
 * ASCII Art Logo (Unicode block characters)
 */
static const char *LOGO[] = {
    "        \033[38;5;111m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;147m\342\226\210\342\226\210\342\225\227  \342\226\210\342\226\210\342\225\227\033[38;5;218m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;116m\342\226\210\342\226\210\342\225\227     \033[38;5;114m\342\226\210\342\226\210\342\225\227     \033[38;5;221m\342\226\210\342\226\210\342\225\227\033[0m",
    "        \033[38;5;111m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;147m\342\226\210\342\226\210\342\225\221  \342\226\210\342\226\210\342\225\221\033[38;5;218m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;116m\342\226\210\342\226\210\342\225\221     \033[38;5;114m\342\226\210\342\226\210\342\225\221     \033[38;5;221m\342\226\210\342\226\210\342\225\221\033[0m",
    "        \033[38;5;111m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;147m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\221\033[38;5;218m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227  \033[38;5;116m\342\226\210\342\226\210\342\225\221     \033[38;5;114m\342\226\210\342\226\210\342\225\221     \033[38;5;221m\342\226\210\342\226\210\342\225\221\033[0m",
    "        \033[38;5;111m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\226\210\342\226\210\342\225\221\033[38;5;147m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\226\210\342\226\210\342\225\221\033[38;5;218m\342\226\210\342\226\210\342\225\224\342\225\220\342\225\220\342\225\235  \033[38;5;116m\342\226\210\342\226\210\342\225\221     \033[38;5;114m\342\226\210\342\226\210\342\225\221     \033[38;5;221m\342\226\210\342\226\210\342\225\221\033[0m",
    "        \033[38;5;111m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\221\033[38;5;147m\342\226\210\342\226\210\342\225\221  \342\226\210\342\226\210\342\225\221\033[38;5;218m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;116m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;114m\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\226\210\342\225\227\033[38;5;221m\342\226\210\342\226\210\342\225\221\033[0m",
    "        \033[38;5;111m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;147m\342\225\232\342\225\220\342\225\235  \342\225\232\342\225\220\342\225\235\033[38;5;218m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;116m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;114m\342\225\232\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\220\342\225\235\033[38;5;221m\342\225\232\342\225\220\342\225\235\033[0m",
    NULL
};

static const int LOGO_HEIGHT = 6;

/* Tagline */
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
 * Draw the splash screen
 */
void splash_draw(int width, int height) {
    /* Clear screen with base background */
    printf(BG_BASE);
    printf(SCR_CLEAR);
    printf(CUR_HOME);

    /* Calculate vertical centering */
    int total_height = LOGO_HEIGHT + 4;  /* logo + 2 blank + tagline + press key */
    int start_row = (height - total_height) / 2;
    if (start_row < 1) start_row = 1;

    /* Draw logo */
    for (int i = 0; i < LOGO_HEIGHT && LOGO[i]; i++) {
        int row = start_row + i;
        int logo_len = visible_length(LOGO[i]);
        int col = (width - logo_len) / 2;
        if (col < 1) col = 1;

        printf(CSI "%d;%dH", row, col);
        printf("%s", LOGO[i]);
    }

    /* Draw tagline (with shell emoji) */
    char tagline_buf[128];
    snprintf(tagline_buf, sizeof(tagline_buf), FG_SUBTEXT "  %s", TAGLINE);
    draw_centered(start_row + LOGO_HEIGHT + 2, width, tagline_buf, NULL);

    /* Draw press key message */
    draw_centered(start_row + LOGO_HEIGHT + 4, width, PRESS_KEY, FG_OVERLAY);

    fflush(stdout);
}

/*
 * Animate splash screen (simple fade-in effect)
 */
void splash_animate(int width, int height, int frame) {
    /* Simple animation: just draw when frame hits threshold */
    if (frame == 0) {
        /* Initial draw */
        printf(BG_BASE);
        printf(SCR_CLEAR);
        fflush(stdout);
    } else if (frame >= 3) {
        /* Show full splash */
        splash_draw(width, height);
    }
}
