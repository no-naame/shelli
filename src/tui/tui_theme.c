/*
 * shelli - Educational Shell
 * tui/tui_theme.c - Catppuccin color palette with neon accents and gradient support
 */

#include <stdio.h>
#include <string.h>
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

    /* Catppuccin Accents */
    int blue;
    int pink;
    int green;
    int peach;
    int red;
    int lavender;
    int teal;
    int yellow;

    /* Neon Accents (for glow effects) */
    int neon_pink;
    int neon_cyan;
    int neon_purple;
    int matrix_green;
} Theme;

/* Catppuccin Mocha theme with neon accents (default) */
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
    /* Neon accents for cyberpunk aesthetic */
    .neon_pink    = 213,  /* #ff79c6 */
    .neon_cyan    = 123,  /* #8be9fd */
    .neon_purple  = 141,  /* #bd93f9 */
    .matrix_green = 84,   /* #50fa7b */
};

/* Current active theme */
static const Theme *current_theme = &theme_catppuccin;

/*
 * Gradient color stops for various effects
 */
static const int GRADIENT_PINK_TO_CYAN[] = {
    213,  /* #ff79c6 - Neon Pink */
    141,  /* #bd93f9 - Neon Purple */
    147,  /* #b4befe - Lavender */
    111,  /* #89b4fa - Blue */
    123,  /* #8be9fd - Neon Cyan */
    116,  /* #94e2d5 - Teal */
};
#define GRADIENT_PINK_TO_CYAN_LEN 6

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

/*
 * Get neon pink color
 */
int theme_neon_pink(void) {
    return current_theme->neon_pink;
}

/*
 * Get neon cyan color
 */
int theme_neon_cyan(void) {
    return current_theme->neon_cyan;
}

/*
 * Get neon purple color
 */
int theme_neon_purple(void) {
    return current_theme->neon_purple;
}

/*
 * Get matrix green color
 */
int theme_matrix_green(void) {
    return current_theme->matrix_green;
}

/*
 * Linear interpolation between two 256-color values
 * t: 0.0 = c1, 1.0 = c2
 * Note: This is a simple index lerp, not true color blending
 */
int color_lerp(int c1, int c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;

    /* Simple linear interpolation of color indices */
    return (int)(c1 + (c2 - c1) * t);
}

/*
 * Get color from gradient based on position
 * pos: 0.0 = start, 1.0 = end
 */
int gradient_color(float pos) {
    if (pos <= 0.0f) return GRADIENT_PINK_TO_CYAN[0];
    if (pos >= 1.0f) return GRADIENT_PINK_TO_CYAN[GRADIENT_PINK_TO_CYAN_LEN - 1];

    /* Find which two colors to interpolate between */
    float scaled = pos * (GRADIENT_PINK_TO_CYAN_LEN - 1);
    int idx1 = (int)scaled;
    int idx2 = idx1 + 1;
    if (idx2 >= GRADIENT_PINK_TO_CYAN_LEN) idx2 = GRADIENT_PINK_TO_CYAN_LEN - 1;

    float t = scaled - idx1;
    return color_lerp(GRADIENT_PINK_TO_CYAN[idx1], GRADIENT_PINK_TO_CYAN[idx2], t);
}

/*
 * Print text with horizontal gradient effect
 * Each character gets a different color from the gradient
 */
void print_gradient_text(const char *text) {
    int len = (int)strlen(text);
    if (len == 0) return;

    for (int i = 0; i < len; i++) {
        float pos = (float)i / (float)(len - 1);
        int color = gradient_color(pos);
        printf(CSI "38;5;%dm%c" COL_RESET, color, text[i]);
    }
}

/*
 * Print text with specified gradient colors
 */
void print_gradient_custom(const char *text, const int *colors, int color_count) {
    int len = (int)strlen(text);
    if (len == 0 || color_count == 0) return;

    for (int i = 0; i < len; i++) {
        /* Map character position to color index */
        int color_idx = (i * (color_count - 1)) / (len > 1 ? len - 1 : 1);
        if (color_idx >= color_count) color_idx = color_count - 1;

        printf(CSI "38;5;%dm%c" COL_RESET, colors[color_idx], text[i]);
    }
}
