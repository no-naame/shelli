/*
 * shelli - Educational Shell
 * tui.h - Terminal UI interface
 */

#ifndef TUI_H
#define TUI_H

#include "lexer.h"
#include "parser.h"

/* ANSI escape codes */
#define ESC "\033"
#define CSI ESC "["

/* Colors (256-color mode) */
#define COL_RESET       CSI "0m"
#define COL_BOLD        CSI "1m"
#define COL_DIM         CSI "2m"

/* Foreground colors */
#define COL_FG_BLACK    CSI "38;5;0m"
#define COL_FG_RED      CSI "38;5;203m"
#define COL_FG_GREEN    CSI "38;5;114m"
#define COL_FG_YELLOW   CSI "38;5;221m"
#define COL_FG_BLUE     CSI "38;5;75m"
#define COL_FG_MAGENTA  CSI "38;5;176m"
#define COL_FG_CYAN     CSI "38;5;81m"
#define COL_FG_WHITE    CSI "38;5;255m"
#define COL_FG_GRAY     CSI "38;5;243m"
#define COL_FG_ORANGE   CSI "38;5;209m"
#define COL_FG_PINK     CSI "38;5;212m"

/* Background colors */
#define COL_BG_BLACK    CSI "48;5;235m"
#define COL_BG_DARK     CSI "48;5;236m"

/* Cursor control */
#define CUR_HIDE        CSI "?25l"
#define CUR_SHOW        CSI "?25h"
#define CUR_HOME        CSI "H"
#define CUR_SAVE        CSI "s"
#define CUR_RESTORE     CSI "u"

/* Screen control */
#define SCR_CLEAR       CSI "2J"
#define SCR_CLEAR_LINE  CSI "2K"

/* Panel identifiers */
typedef enum {
    PANEL_INPUT,
    PANEL_TOKENIZE,
    PANEL_PARSE,
    PANEL_EXECUTE,
    PANEL_RESULT
} PanelId;

/* Initialize the TUI, returns 0 on success */
int tui_init(void);

/* Cleanup and restore terminal */
void tui_cleanup(void);

/* Get terminal dimensions */
void tui_get_size(int *width, int *height);

/* Draw the complete frame */
void tui_draw_frame(void);

/* Update a specific panel with content */
void tui_update_panel(PanelId panel, const char *content);

/* Clear a panel */
void tui_clear_panel(PanelId panel);

/* Read a line of input with prompt, returns dynamically allocated string */
char *tui_read_line(void);

/* Display tokenization results */
void tui_show_tokens(TokenList *tokens);

/* Display parse results */
void tui_show_pipeline(Pipeline *pipeline);

/* Add a line to the execute log */
void tui_log_exec(const char *message);

/* Show result with exit code */
void tui_show_result(int exit_code, const char *output);

/* Show error message */
void tui_show_error(const char *message);

/* Wait for keypress in debug mode */
void tui_wait_step(const char *step_name);

/* Check if debug mode is enabled */
int tui_is_debug(void);

/* Set debug mode */
void tui_set_debug(int enabled);

#endif /* TUI_H */
