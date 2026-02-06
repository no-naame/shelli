/*
 * shelli - Educational Shell
 * tui/tui_theme.c - Catppuccin color palette with 256-color support
 */

#include <stdio.h>
#include "tui.h"

/*
 * Theme structure for potential future theme switching
 */
typedef struct {
    /* Backgrounds */
    int base;
    int surface;
    int overlay;

    /* Text */
    int text;
    int subtext;

    /* Accents */
    int blue;
    int pink;
    int green;
    int peach;
    int red;
    int lavender;
    int teal;
    int yellow;
} Theme;

/* Catppuccin Mocha theme (default) */
static const Theme theme_catppuccin = {
    .base     = 234,  /* #1e1e2e */
    .surface  = 236,  /* #313244 */
    .overlay  = 243,  /* #6c7086 */
    .text     = 254,  /* #cdd6f4 */
    .subtext  = 249,  /* #a6adc8 */
    .blue     = 111,  /* #89b4fa */
    .pink     = 218,  /* #f5c2e7 */
    .green    = 114,  /* #a6e3a1 */
    .peach    = 216,  /* #fab387 */
    .red      = 204,  /* #f38ba8 */
    .lavender = 147,  /* #b4befe */
    .teal     = 116,  /* #94e2d5 */
    .yellow   = 221,  /* #f9e2af */
};

/* Current active theme */
static const Theme *current_theme = &theme_catppuccin;

/*
 * Get current theme
 */
const Theme *theme_get_current(void) {
    return current_theme;
}

/*
 * Apply 256-color mode (called during init)
 */
void theme_apply_256(void) {
    /* Enable 256 color mode */
    printf(CSI "38;5;%dm", current_theme->text);
    printf(COL_RESET);
}

/*
 * Print a foreground color escape sequence
 */
void theme_fg(int color) {
    printf(CSI "38;5;%dm", color);
}

/*
 * Print a background color escape sequence
 */
void theme_bg(int color) {
    printf(CSI "48;5;%dm", color);
}

/*
 * Print bold attribute
 */
void theme_bold(void) {
    printf(COL_BOLD);
}

/*
 * Print dim attribute
 */
void theme_dim(void) {
    printf(COL_DIM);
}

/*
 * Reset all attributes
 */
void theme_reset(void) {
    printf(COL_RESET);
}
