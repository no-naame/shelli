/*
 * shelli - Educational Shell
 * tui/tui.h - Unified TUI public API
 *
 * Page-based, presentation-style UI for educational shell visualization.
 * Each processing stage is a full-screen page with educational explanations.
 */

#ifndef TUI_H
#define TUI_H

#include "../lexer.h"
#include "../ast.h"

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
#define COL_SURFACE    236    /* #313244 - Box backgrounds, hint bar */
#define COL_OVERLAY    243    /* #6c7086 - Dim text, inactive items */

/* Text */
#define COL_TEXT       254    /* #cdd6f4 - Primary text */
#define COL_SUBTEXT    249    /* #a6adc8 - Secondary text */

/* Accents */
#define COL_BLUE       111    /* #89b4fa - Titles, WORD tokens */
#define COL_PINK       218    /* #f5c2e7 - Command names, keywords */
#define COL_GREEN      114    /* #a6e3a1 - String tokens, success */
#define COL_PEACH      216    /* #fab387 - Warnings, parentheses */
#define COL_RED        204    /* #f38ba8 - Errors */
#define COL_LAVENDER   147    /* #b4befe - AST connectors, accent */
#define COL_TEAL       116    /* #94e2d5 - Redirect tokens, types */
#define COL_YELLOW     221    /* #f9e2af - Variable tokens */
#define COL_MAUVE      141    /* #cba6f7 - Keyword tokens, pipes */

/* Neon Accents (for logo gradient) */
#define COL_NEON_PINK    213  /* #ff79c6 - Logo gradient start */
#define COL_NEON_CYAN    123  /* #8be9fd - Logo gradient end */
#define COL_NEON_PURPLE  141  /* #bd93f9 */
#define COL_MATRIX_GREEN 84   /* #50fa7b */

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
 * Stages (browsable processing pages)
 * ============================================================================
 */

typedef enum {
    STAGE_TOKENIZE = 0,
    STAGE_AST,
    STAGE_EXPAND,
    STAGE_EXECUTE,
    STAGE_RESULT,
    STAGE_COUNT
} TuiStage;

/*
 * ============================================================================
 * Mascot Moods
 * ============================================================================
 */

typedef enum {
    MOOD_NORMAL,
    MOOD_THINKING,
    MOOD_HAPPY,
    MOOD_SAD,
    MOOD_WORKING
} MascotMood;

/*
 * ============================================================================
 * Key Input
 * ============================================================================
 */

typedef enum {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_TAB,
    KEY_SHIFT_TAB,
    KEY_ESCAPE,
    KEY_CTRL_C,
    KEY_CTRL_D,
    KEY_CTRL_L,
    KEY_CTRL_A,
    KEY_CTRL_E,
    KEY_CTRL_K,
    KEY_CTRL_U,
    KEY_CTRL_W
} KeyCode;

typedef struct {
    KeyCode code;
    char ch;
} KeyEvent;

/*
 * ============================================================================
 * Render Buffer - Double-buffered rendering
 * ============================================================================
 */

typedef struct {
    char ch[5];     /* UTF-8 character (up to 4 bytes + null) */
    int  fg;        /* foreground color (256-color index, -1 = default) */
    int  bg;        /* background color (256-color index, -1 = default) */
    int  bold;      /* bold attribute */
    int  dim;       /* dim attribute */
} RBCell;

typedef struct {
    RBCell *cells;
    int rows, cols;
} RenderBuf;

void rbuf_init(RenderBuf *rb, int rows, int cols);
void rbuf_free(RenderBuf *rb);
void rbuf_clear(RenderBuf *rb);
void rbuf_put_char(RenderBuf *rb, int row, int col, const char *ch,
                   int fg, int bg, int bold, int dim);
void rbuf_put_str(RenderBuf *rb, int row, int col, const char *str,
                  int fg, int bg, int bold, int dim);
void rbuf_put_str_trunc(RenderBuf *rb, int row, int col, const char *str,
                        int fg, int bg, int bold, int dim, int max_width);
void rbuf_flush(RenderBuf *rb, RenderBuf *prev);

/*
 * ============================================================================
 * Stage Data - all pipeline data for one command
 * ============================================================================
 */

#define MAX_SD_TOKENS      64
#define MAX_SD_AST_LINES   128
#define MAX_SD_EXPANSIONS  32
#define MAX_SD_EXEC_LINES  64
#define MAX_SD_OUTPUT_LINES 128
#define MAX_SD_LINE        512

/* Token display data */
typedef struct {
    char value[64];
    char label[32];     /* e.g., "WORD", "PIPE", "STRING" */
    int  color;         /* Foreground color for the token value */
    int  is_command;    /* 1 if this is in command position */
} TokenDisplay;

/* AST display line */
typedef struct {
    char prefix[128];   /* Tree connectors: "  ", "| ", etc. */
    char icon[8];       /* "|", ">", "?", "@", "#", "!", "$", "f" */
    char label[32];     /* "PIPELINE", "CMD", "IF", etc. */
    char detail[256];   /* Command name, arg value, etc. */
    int  icon_color;    /* Color for icon + label */
    int  detail_color;  /* Color for detail text */
} AstDisplayLine;

/* Expansion display data */
typedef struct {
    char original[256];
    char expanded[512];
    char type_label[64]; /* e.g., "variable expansion" */
} ExpansionDisplay;

/* Execution trace line */
typedef struct {
    char text[MAX_SD_LINE];
    char prefix_char;   /* '>' process, '~' I/O, '*' exec, '.' wait */
    int  color;
    int  indent;        /* 0=top level, 1=child process */
} ExecLine;

/* Complete stage data for one command */
typedef struct {
    /* Input */
    char input[MAX_SD_LINE];

    /* Tokens */
    TokenDisplay tokens[MAX_SD_TOKENS];
    int token_count;

    /* AST */
    AstDisplayLine ast_lines[MAX_SD_AST_LINES];
    int ast_count;

    /* Expansions */
    ExpansionDisplay expansions[MAX_SD_EXPANSIONS];
    int expansion_count;

    /* Execution trace */
    ExecLine exec_lines[MAX_SD_EXEC_LINES];
    int exec_count;

    /* Result */
    char output_lines[MAX_SD_OUTPUT_LINES][MAX_SD_LINE];
    int output_count;
    int exit_code;

    /* Error state */
    char error_msg[MAX_SD_LINE];
    int has_error;
} StageData;

/*
 * ============================================================================
 * Animation Speed
 * ============================================================================
 */

typedef enum {
    ANIM_SPEED_NONE = 0,
    ANIM_SPEED_FAST,
    ANIM_SPEED_NORMAL,
    ANIM_SPEED_SLOW
} AnimSpeed;

/*
 * ============================================================================
 * Public API - Core
 * ============================================================================
 */

int  tui_init(void);
void tui_cleanup(void);
void tui_suspend_raw(void);
void tui_resume_raw(void);
void tui_get_size(int *width, int *height);

/*
 * ============================================================================
 * Public API - Pages
 * ============================================================================
 */

/* Show welcome page. Returns 0 to continue, 1 to quit. */
int  tui_welcome_page(void);

/* Interactive stage browser. Returns 0=new command, 1=quit. */
int  tui_stage_browser(StageData *data);

/*
 * ============================================================================
 * Public API - Input
 * ============================================================================
 */

/* Read a line of input (renders full-screen input page) */
char *tui_read_line(void);

/* Read a single key event (exported for use by pages) */
KeyEvent tui_read_key(void);

/* History */
void tui_history_load(void);
void tui_history_save(void);
const char *tui_history_get(int n);
int  tui_history_count(void);
void tui_history_print(void);

/*
 * ============================================================================
 * Public API - Stage Data Helpers
 * ============================================================================
 */

void stage_data_init(StageData *sd);
void stage_data_set_input(StageData *sd, const char *input);
void stage_data_add_token(StageData *sd, const char *value,
                          const char *label, int color, int is_cmd);
void stage_data_add_ast_line(StageData *sd, const char *prefix,
                             const char *icon, const char *label,
                             const char *detail, int icon_color,
                             int detail_color);
void stage_data_add_expansion(StageData *sd, const char *original,
                              const char *expanded, const char *type_label);
void stage_data_add_exec(StageData *sd, const char *text, char prefix_char,
                         int color, int indent);
void stage_data_set_output(StageData *sd, const char *output);
void stage_data_set_result(StageData *sd, int exit_code);
void stage_data_set_error(StageData *sd, const char *error);

/* Populate stage data from parsed structures */
void stage_data_populate_tokens(StageData *sd, TokenList *tokens);
void stage_data_populate_ast(StageData *sd, AstNode *ast);

/*
 * ============================================================================
 * Public API - Page Rendering Helpers (shared between page renderers)
 * ============================================================================
 */

/* Draw outer heavy frame with title bar */
void page_draw_outer_frame(RenderBuf *rb, int tw, int th,
                           const char *stage_name, int stage_color,
                           int stage_num);

/* Draw stage progress dots on a row */
void page_draw_stage_dots(RenderBuf *rb, int row, int tw, TuiStage current);

/* Draw a rounded box with title */
void page_draw_box(RenderBuf *rb, int x, int y, int w, int h,
                   const char *title, int border_color);

/* Draw "Your Input" box showing the command */
void page_draw_input_box(RenderBuf *rb, int x, int y, int w,
                         const char *input);

/* Draw bottom hint bar */
void page_draw_hints_bar(RenderBuf *rb, int tw, int th,
                         const char **keys, const char **labels,
                         int count);

/*
 * ============================================================================
 * Public API - Mascot
 * ============================================================================
 */

void mascot_render(RenderBuf *rb, int x, int y, MascotMood mood, int frame);
MascotMood mascot_mood_for_stage(TuiStage stage, int exit_code);

/*
 * ============================================================================
 * Public API - Stage Page Renderers
 * ============================================================================
 */

void page_render_tokenize(RenderBuf *rb, const StageData *sd,
                          int tw, int th, int scroll);
void page_render_ast(RenderBuf *rb, const StageData *sd,
                     int tw, int th, int scroll);
void page_render_expand(RenderBuf *rb, const StageData *sd,
                        int tw, int th, int scroll);
void page_render_execute(RenderBuf *rb, const StageData *sd,
                         int tw, int th, int scroll);
void page_render_result(RenderBuf *rb, const StageData *sd,
                        int tw, int th, int scroll);

/*
 * ============================================================================
 * Public API - Render Internals (shared between TUI modules)
 * ============================================================================
 */

void tui_render_get_bufs(RenderBuf **front, RenderBuf **back);
void tui_render_ensure(int rows, int cols);
void tui_render_flush(void);

int  tui_resize_pending(void);
void tui_handle_resize(void);
int  term_get_width(void);
int  term_get_height(void);

/* Token color mapping for syntax highlighting */
int  token_type_color(TokenType type);
int  is_shell_keyword(const char *word);

/* Animation speed */
void tui_set_anim_speed(AnimSpeed speed);
int  tui_get_anim_delay(void);

/* Animation tick */
void tui_tick(void);
const char *tui_spinner_frame(int frame);

/*
 * ============================================================================
 * Public API - Enhanced Animation System (tui_anim.c)
 * ============================================================================
 */

float ease_out_cubic(float t);
float ease_in_out_quad(float t);
float ease_out_elastic(float t);
float ease_linear(float t);

int  anim_create(int type, const char *content, int x, int y, int frames);
void anim_start(int anim_id);
int  anim_tick(int anim_id);
void anim_render(int anim_id);
int  anim_is_complete(int anim_id);
void anim_destroy(int anim_id);
void anim_clear_all(void);

void anim_fade_in_blocking(int x, int y, const char *content, int duration_ms);
void anim_typewriter_blocking(int x, int y, const char *content, int duration_ms);

/*
 * ============================================================================
 * Public API - Icons (tui_icons.c)
 * ============================================================================
 */

void icons_set_nerd_font(int enabled);
int  icons_nerd_font_enabled(void);
const char *icon_get(const char *name);
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
void icon_print(const char *name, int color);
void icon_print_label(const char *name, const char *label, int icon_color, int label_color);

/*
 * ============================================================================
 * Public API - Theme Enhancements (tui_theme.c)
 * ============================================================================
 */

int  theme_neon_pink(void);
int  theme_neon_cyan(void);
int  theme_neon_purple(void);
int  theme_matrix_green(void);
int  color_lerp(int c1, int c2, float t);
int  gradient_color(float pos);
void print_gradient_text(const char *text);
void print_gradient_custom(const char *text, const int *colors, int color_count);

/*
 * ============================================================================
 * Suggestion / Autocomplete Types
 * ============================================================================
 */

#define MAX_SUGGESTIONS    12
#define MAX_SUGGEST_LEN   256
#define FREQ_TABLE_SIZE   128

typedef enum {
    SRC_HISTORY,
    SRC_BUILTIN,
    SRC_KEYWORD,
    SRC_EXECUTABLE,
    SRC_PRESET
} SuggestSource;

typedef struct {
    char text[MAX_SUGGEST_LEN];
    char description[MAX_SUGGEST_LEN];
    SuggestSource source;
    int score;
    int frequency;
} Suggestion;

typedef struct {
    Suggestion items[MAX_SUGGESTIONS];
    int count;
    int selected;           /* -1 = none selected */
    int visible;
    char last_prefix[MAX_SUGGEST_LEN];
} SuggestState;

typedef struct {
    const char *command;
    const char *description;
    const char *category;
    int color;
} PresetCommand;

typedef struct {
    int selected;           /* index into preset array */
    int visible;
    int scroll_offset;
} TryTheseState;

/*
 * ============================================================================
 * Public API - Suggestions (tui_suggest.c)
 * ============================================================================
 */

void suggest_init(void);
void suggest_cleanup(void);
void suggest_build_freq_table(void);
void suggest_record_usage(const char *command);
int  suggest_get_frequency(const char *word);

int  suggest_levenshtein(const char *a, const char *b, int max_dist);
int  suggest_compute_score(const char *candidate, const char *prefix,
                           SuggestSource source, int frequency, int recency);
void suggest_generate(SuggestState *state, const char *prefix);

const PresetCommand *suggest_get_presets(int *count);

void suggest_render_dropdown(RenderBuf *rb, const SuggestState *state,
                             int x, int y, int w);
void suggest_render_try_these(RenderBuf *rb, const TryTheseState *state,
                              int x, int y, int w, int max_h);

void suggest_move(SuggestState *state, int delta);
void try_these_move(TryTheseState *state, int delta);
const char *suggest_accept(SuggestState *state);
const char *try_these_accept(TryTheseState *state);
void suggest_dismiss(SuggestState *state);
void try_these_dismiss(TryTheseState *state);

/*
 * ============================================================================
 * Public API - Logo (tui_logo.c)
 * ============================================================================
 */

const char **logo_get_lines(void);
int  logo_get_height(void);

#endif /* TUI_H */
