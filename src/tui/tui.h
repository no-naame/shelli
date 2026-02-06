/*
 * shelli - Educational Shell
 * tui/tui.h - Unified TUI public API
 *
 * Professional-grade terminal UI inspired by LazyVim and Charm
 */

#ifndef TUI_H
#define TUI_H

#include "../lexer.h"
#include "../parser.h"

/*
 * ============================================================================
 * ANSI Escape Codes
 * ============================================================================
 */

#define ESC "\033"
#define CSI ESC "["

/* Cursor control */
#define CUR_HIDE        CSI "?25l"
#define CUR_SHOW        CSI "?25h"
#define CUR_HOME        CSI "H"
#define CUR_SAVE        CSI "s"
#define CUR_RESTORE     CSI "u"

/* Screen control */
#define SCR_CLEAR       CSI "2J"
#define SCR_CLEAR_LINE  CSI "2K"
#define ALT_SCREEN_ON   CSI "?1049h"
#define ALT_SCREEN_OFF  CSI "?1049l"

/*
 * ============================================================================
 * Catppuccin Mocha Color Palette (256-color approximations)
 * ============================================================================
 */

/* Backgrounds */
#define COL_BASE       234    /* #1e1e2e - Main background */
#define COL_SURFACE    236    /* #313244 - Panel backgrounds */
#define COL_OVERLAY    243    /* #6c7086 - Dim text */

/* Text */
#define COL_TEXT       254    /* #cdd6f4 - Primary text */
#define COL_SUBTEXT    249    /* #a6adc8 - Secondary text */

/* Accents */
#define COL_BLUE       111    /* #89b4fa - Primary accent, titles */
#define COL_PINK       218    /* #f5c2e7 - Keywords, operators */
#define COL_GREEN      114    /* #a6e3a1 - Success, strings */
#define COL_PEACH      216    /* #fab387 - Warnings, numbers */
#define COL_RED        204    /* #f38ba8 - Errors */
#define COL_LAVENDER   147    /* #b4befe - Secondary accent */
#define COL_TEAL       116    /* #94e2d5 - Types, special */
#define COL_YELLOW     221    /* #f9e2af - Highlights */

/* Neon Accents (for cyberpunk glow effects) */
#define COL_NEON_PINK    213  /* #ff79c6 - Hot pink glow */
#define COL_NEON_CYAN    123  /* #8be9fd - Electric cyan */
#define COL_NEON_PURPLE  141  /* #bd93f9 - Soft purple */
#define COL_MATRIX_GREEN 84   /* #50fa7b - Matrix green accent */

/* ANSI color macros */
#define FG(c)          CSI "38;5;" #c "m"
#define BG(c)          CSI "48;5;" #c "m"
#define COL_RESET      CSI "0m"
#define COL_BOLD       CSI "1m"
#define COL_DIM        CSI "2m"

/* Named foreground colors for convenience */
#define FG_BASE        CSI "38;5;234m"
#define FG_SURFACE     CSI "38;5;236m"
#define FG_OVERLAY     CSI "38;5;243m"
#define FG_TEXT        CSI "38;5;254m"
#define FG_SUBTEXT     CSI "38;5;249m"
#define FG_BLUE        CSI "38;5;111m"
#define FG_PINK        CSI "38;5;218m"
#define FG_GREEN       CSI "38;5;114m"
#define FG_PEACH       CSI "38;5;216m"
#define FG_RED         CSI "38;5;204m"
#define FG_LAVENDER    CSI "38;5;147m"
#define FG_TEAL        CSI "38;5;116m"
#define FG_YELLOW      CSI "38;5;221m"

/* Named background colors */
#define BG_BASE        CSI "48;5;234m"
#define BG_SURFACE     CSI "48;5;236m"

/*
 * ============================================================================
 * Execution Stages
 * ============================================================================
 */

typedef enum {
    STAGE_INPUT = 0,
    STAGE_TOKENIZE,
    STAGE_PARSE,
    STAGE_EXECUTE,
    STAGE_RESULT,
    STAGE_COUNT
} TuiStage;

/*
 * ============================================================================
 * Panel Identifiers (for compatibility)
 * ============================================================================
 */

typedef enum {
    PANEL_INPUT,
    PANEL_TOKENIZE,
    PANEL_PARSE,
    PANEL_EXECUTE,
    PANEL_RESULT
} PanelId;

/*
 * ============================================================================
 * Public API - Core Functions
 * ============================================================================
 */

/* Initialize the TUI system (enters alt screen, raw mode) */
int tui_init(void);

/* Cleanup and restore terminal state */
void tui_cleanup(void);

/* Get terminal dimensions */
void tui_get_size(int *width, int *height);

/* Show splash screen with logo animation */
void tui_splash(void);

/* Draw/redraw the main frame */
void tui_draw_frame(void);

/*
 * ============================================================================
 * Public API - Input
 * ============================================================================
 */

/* Read a line of input with full editing support */
char *tui_read_line(void);

/*
 * ============================================================================
 * Public API - Stage Visualization
 * ============================================================================
 */

/* Set the current stage (updates stage indicator) */
void tui_stage_begin(TuiStage stage);

/* Mark a stage as complete */
void tui_stage_end(TuiStage stage);

/* Display tokenization results in TOKENIZE panel */
void tui_show_tokens(TokenList *tokens);

/* Display parse results in PARSE panel */
void tui_show_pipeline(Pipeline *pipeline);

/* Add a log line to EXECUTE panel */
void tui_log_exec(const char *message);

/* Show final result with exit code */
void tui_show_result(int exit_code, const char *output);

/* Show error message */
void tui_show_error(const char *message);

/*
 * ============================================================================
 * Public API - Panel Management
 * ============================================================================
 */

/* Update panel content */
void tui_update_panel(PanelId panel, const char *content);

/* Clear a specific panel */
void tui_clear_panel(PanelId panel);

/* Clear all processing panels (TOKENIZE, PARSE, EXECUTE, RESULT) at once */
void tui_clear_all_panels(void);

/*
 * ============================================================================
 * Public API - Debug Mode
 * ============================================================================
 */

/* Wait for user input in debug mode */
void tui_wait_step(const char *step_name);

/* Check if debug mode is enabled */
int tui_is_debug(void);

/* Set debug mode */
void tui_set_debug(int enabled);

/*
 * ============================================================================
 * Public API - Animation
 * ============================================================================
 */

/* Process one frame of animation (call every ~16ms) */
void tui_tick(void);

/* Get spinner character for current frame */
const char *tui_spinner_frame(int frame);

/*
 * ============================================================================
 * Public API - Enhanced Animation System (tui_anim.c)
 * ============================================================================
 */

/* Easing functions for smooth animations */
float ease_out_cubic(float t);
float ease_in_out_quad(float t);
float ease_out_elastic(float t);
float ease_linear(float t);

/* Animation lifecycle */
int anim_create(int type, const char *content, int x, int y, int frames);
void anim_start(int anim_id);
int anim_tick(int anim_id);
void anim_render(int anim_id);
int anim_is_complete(int anim_id);
void anim_destroy(int anim_id);
void anim_clear_all(void);

/* Convenience blocking animations */
void anim_fade_in_blocking(int x, int y, const char *content, int duration_ms);
void anim_typewriter_blocking(int x, int y, const char *content, int duration_ms);

/*
 * ============================================================================
 * Public API - Icons (tui_icons.c)
 * ============================================================================
 */

/* Icon mode control */
void icons_set_nerd_font(int enabled);
int icons_nerd_font_enabled(void);

/* Get icon by name */
const char *icon_get(const char *name);

/* Common icon accessors */
const char *icon_terminal(void);
const char *icon_keyword(void);
const char *icon_struct(void);
const char *icon_play(void);
const char *icon_check(void);
const char *icon_cog(void);
const char *icon_folder(void);
const char *icon_file(void);
const char *icon_git(void);
const char *icon_arrow_right(void);
const char *icon_pipe(void);
const char *icon_error(void);
const char *icon_success(void);
const char *icon_diamond(void);

/* Print icon with color */
void icon_print(const char *name, int color);
void icon_print_label(const char *name, const char *label, int icon_color, int label_color);

/*
 * ============================================================================
 * Public API - Theme Enhancements (tui_theme.c)
 * ============================================================================
 */

/* Get neon accent colors */
int theme_neon_pink(void);
int theme_neon_cyan(void);
int theme_neon_purple(void);
int theme_matrix_green(void);

/* Gradient helpers */
int color_lerp(int c1, int c2, float t);
int gradient_color(float pos);
void print_gradient_text(const char *text);
void print_gradient_custom(const char *text, const int *colors, int color_count);

#endif /* TUI_H */
