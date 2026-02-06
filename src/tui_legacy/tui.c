/*
 * shelli - Educational Shell
 * tui.c - Terminal UI with ANSI escapes and Unicode box-drawing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "tui.h"

/* Terminal state */
static int term_width = 80;
static int term_height = 24;
static int debug_mode = 0;

/* Panel content buffers */
#define MAX_PANEL_LINES 32
#define MAX_LINE_LEN 256

static char input_content[MAX_LINE_LEN] = "";
static char tokenize_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int tokenize_count = 0;
static char parse_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int parse_count = 0;
static char exec_lines[MAX_PANEL_LINES][MAX_LINE_LEN];
static int exec_count = 0;
static char result_content[MAX_LINE_LEN] = "";

/* Box drawing characters (Unicode) */
#define BOX_TL "┌"
#define BOX_TR "┐"
#define BOX_BL "└"
#define BOX_BR "┘"
#define BOX_H  "─"
#define BOX_V  "│"
#define BOX_LT "├"
#define BOX_RT "┤"
#define BOX_TT "┬"
#define BOX_BT "┴"
#define BOX_X  "┼"

static void update_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_width = ws.ws_col;
        term_height = ws.ws_row;
    }
}

static void handle_winch(int sig) {
    (void)sig;
    update_size();
}

static void move_cursor(int row, int col) {
    printf(CSI "%d;%dH", row, col);
}

static void print_repeated(const char *str, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s", str);
    }
}

static void draw_hline(int row, int col, int width, const char *left, const char *fill, const char *right) {
    move_cursor(row, col);
    printf("%s%s", COL_FG_GRAY, left);
    print_repeated(fill, width - 2);
    printf("%s%s", right, COL_RESET);
}

static void draw_box_mid(int row, int col, int width) {
    draw_hline(row, col, width, BOX_LT, BOX_H, BOX_RT);
}

static void draw_box_split(int row, int col, int width, int split_col) {
    move_cursor(row, col);
    printf("%s%s", COL_FG_GRAY, BOX_LT);
    print_repeated(BOX_H, split_col - 2);
    printf("%s", BOX_TT);
    print_repeated(BOX_H, width - split_col - 1);
    printf("%s%s", BOX_RT, COL_RESET);
}

static void draw_box_unsplit(int row, int col, int width, int split_col) {
    move_cursor(row, col);
    printf("%s%s", COL_FG_GRAY, BOX_LT);
    print_repeated(BOX_H, split_col - 2);
    printf("%s", BOX_BT);
    print_repeated(BOX_H, width - split_col - 1);
    printf("%s%s", BOX_RT, COL_RESET);
}

static void draw_box_bottom(int row, int col, int width) {
    draw_hline(row, col, width, BOX_BL, BOX_H, BOX_BR);
}

static void draw_empty_row(int row, int col, int width) {
    move_cursor(row, col);
    printf("%s%s%s", COL_FG_GRAY, BOX_V, COL_RESET);
    printf("%*s", width - 2, "");
    printf("%s%s%s", COL_FG_GRAY, BOX_V, COL_RESET);
}

static void draw_split_row(int row, int col, int width, int split_col) {
    move_cursor(row, col);
    printf("%s%s%s", COL_FG_GRAY, BOX_V, COL_RESET);
    printf("%*s", split_col - 2, "");
    printf("%s%s%s", COL_FG_GRAY, BOX_V, COL_RESET);
    printf("%*s", width - split_col - 1, "");
    printf("%s%s%s", COL_FG_GRAY, BOX_V, COL_RESET);
}

static void draw_label(int row, int col, const char *label) {
    move_cursor(row, col);
    printf("%s%s %s %s", COL_FG_BLUE, COL_BOLD, label, COL_RESET);
}

int tui_init(void) {
    update_size();
    signal(SIGWINCH, handle_winch);

    /* Clear screen */
    printf("%s%s", SCR_CLEAR, CUR_HOME);
    fflush(stdout);

    return 0;
}

void tui_cleanup(void) {
    printf("%s%s", COL_RESET, CUR_SHOW);
    printf(CSI "%d;1H", term_height);
    fflush(stdout);
}

void tui_get_size(int *width, int *height) {
    update_size();
    *width = term_width;
    *height = term_height;
}

void tui_draw_frame(void) {
    int w = term_width;
    int split_col = w / 2;

    /* Row layout:
     * 1: top border + title
     * 2: INPUT label
     * 3: INPUT content
     * 4: split border (TOKENIZE | PARSE)
     * 5: TOKENIZE/PARSE labels
     * 6-10: content (5 lines)
     * 11: unsplit border
     * 12: EXECUTE label
     * 13-17: content (5 lines)
     * 18: mid border
     * 19: RESULT label
     * 20: RESULT content
     * 21: bottom border
     */

    printf("%s", SCR_CLEAR);

    /* Row 1: Top border with title */
    move_cursor(1, 1);
    printf("%s%s", COL_FG_GRAY, BOX_TL);
    print_repeated(BOX_H, 2);
    printf("%s%s shelli %s%s", COL_RESET, COL_FG_BLUE, COL_FG_GRAY, COL_RESET);
    printf("%s", COL_FG_GRAY);
    print_repeated(BOX_H, w - 12 - 10);
    printf("%s[?] help%s", COL_FG_CYAN, COL_FG_GRAY);
    print_repeated(BOX_H, 1);
    printf("%s%s", BOX_TR, COL_RESET);

    /* Row 2: INPUT label */
    draw_empty_row(2, 1, w);
    draw_label(2, 2, "INPUT");

    /* Row 3: INPUT content */
    draw_empty_row(3, 1, w);

    /* Row 4: Split border */
    draw_box_split(4, 1, w, split_col);

    /* Row 5: TOKENIZE / PARSE labels */
    draw_split_row(5, 1, w, split_col);
    draw_label(5, 2, "TOKENIZE");
    draw_label(5, split_col + 1, "PARSE");

    /* Rows 6-10: TOKENIZE / PARSE content */
    for (int r = 6; r <= 10; r++) {
        draw_split_row(r, 1, w, split_col);
    }

    /* Row 11: Unsplit border */
    draw_box_unsplit(11, 1, w, split_col);

    /* Row 12: EXECUTE label */
    draw_empty_row(12, 1, w);
    draw_label(12, 2, "EXECUTE");

    /* Rows 13-17: EXECUTE content */
    for (int r = 13; r <= 17; r++) {
        draw_empty_row(r, 1, w);
    }

    /* Row 18: Mid border */
    draw_box_mid(18, 1, w);

    /* Row 19: RESULT label */
    draw_empty_row(19, 1, w);
    draw_label(19, 2, "RESULT");

    /* Row 20: RESULT content */
    draw_empty_row(20, 1, w);

    /* Row 21: Bottom border */
    draw_box_bottom(21, 1, w);

    fflush(stdout);
}

void tui_clear_panel(PanelId panel) {
    int w = term_width;
    int split_col = w / 2;

    switch (panel) {
    case PANEL_INPUT:
        input_content[0] = '\0';
        draw_empty_row(3, 1, w);
        break;

    case PANEL_TOKENIZE:
        tokenize_count = 0;
        for (int r = 6; r <= 10; r++) {
            draw_split_row(r, 1, w, split_col);
        }
        break;

    case PANEL_PARSE:
        parse_count = 0;
        for (int r = 6; r <= 10; r++) {
            draw_split_row(r, 1, w, split_col);
        }
        break;

    case PANEL_EXECUTE:
        exec_count = 0;
        for (int r = 13; r <= 17; r++) {
            draw_empty_row(r, 1, w);
        }
        break;

    case PANEL_RESULT:
        result_content[0] = '\0';
        draw_empty_row(20, 1, w);
        break;
    }
    fflush(stdout);
}

static void render_input(void) {
    move_cursor(3, 3);
    printf("%s❯%s %s%s", COL_FG_CYAN, COL_RESET, input_content, SCR_CLEAR_LINE);
}

static void render_tokenize(void) {
    int w = term_width;
    int split_col = w / 2;
    int max_lines = 5;

    for (int i = 0; i < max_lines; i++) {
        int row = 6 + i;
        move_cursor(row, 3);
        if (i < tokenize_count) {
            printf("%s%s", tokenize_lines[i], COL_RESET);
        }
        /* Clear to the border */
        printf(CSI "%dG", split_col);
    }
}

static void render_parse(void) {
    int w = term_width;
    int split_col = w / 2;
    int max_lines = 5;

    for (int i = 0; i < max_lines; i++) {
        int row = 6 + i;
        move_cursor(row, split_col + 2);
        if (i < parse_count) {
            printf("%s%s", parse_lines[i], COL_RESET);
        }
        /* Clear to the border */
        printf(CSI "%dG", w);
    }
}

static void render_execute(void) {
    int w = term_width;
    int max_lines = 5;

    for (int i = 0; i < max_lines; i++) {
        int row = 13 + i;
        move_cursor(row, 3);
        if (i < exec_count) {
            printf("%s%s", exec_lines[i], COL_RESET);
        }
        /* Clear to the border */
        printf(CSI "%dG", w);
    }
}

static void render_result(void) {
    int w = term_width;
    move_cursor(20, 3);
    printf("%s%s", result_content, COL_RESET);
    /* Clear to the border */
    printf(CSI "%dG", w);
}

void tui_update_panel(PanelId panel, const char *content) {
    switch (panel) {
    case PANEL_INPUT:
        strncpy(input_content, content, MAX_LINE_LEN - 1);
        input_content[MAX_LINE_LEN - 1] = '\0';
        render_input();
        break;

    case PANEL_TOKENIZE:
        if (tokenize_count < MAX_PANEL_LINES) {
            strncpy(tokenize_lines[tokenize_count], content, MAX_LINE_LEN - 1);
            tokenize_lines[tokenize_count][MAX_LINE_LEN - 1] = '\0';
            tokenize_count++;
        }
        render_tokenize();
        break;

    case PANEL_PARSE:
        if (parse_count < MAX_PANEL_LINES) {
            strncpy(parse_lines[parse_count], content, MAX_LINE_LEN - 1);
            parse_lines[parse_count][MAX_LINE_LEN - 1] = '\0';
            parse_count++;
        }
        render_parse();
        break;

    case PANEL_EXECUTE:
        if (exec_count < MAX_PANEL_LINES) {
            strncpy(exec_lines[exec_count], content, MAX_LINE_LEN - 1);
            exec_lines[exec_count][MAX_LINE_LEN - 1] = '\0';
            exec_count++;
        }
        render_execute();
        break;

    case PANEL_RESULT:
        strncpy(result_content, content, MAX_LINE_LEN - 1);
        result_content[MAX_LINE_LEN - 1] = '\0';
        render_result();
        break;
    }
    fflush(stdout);
}

void tui_show_tokens(TokenList *tokens) {
    tokenize_count = 0;
    for (int i = 0; i < tokens->count && tokenize_count < MAX_PANEL_LINES; i++) {
        Token *tok = &tokens->tokens[i];
        char buf[MAX_LINE_LEN];

        if (tok->value) {
            snprintf(buf, sizeof(buf), "%s[%s]%s \"%s%s%s\"",
                     COL_FG_PINK, token_type_str(tok->type), COL_RESET,
                     COL_FG_GREEN, tok->value, COL_RESET);
        } else {
            snprintf(buf, sizeof(buf), "%s[%s]%s",
                     COL_FG_PINK, token_type_str(tok->type), COL_RESET);
        }

        strncpy(tokenize_lines[tokenize_count], buf, MAX_LINE_LEN - 1);
        tokenize_lines[tokenize_count][MAX_LINE_LEN - 1] = '\0';
        tokenize_count++;
    }
    render_tokenize();
    fflush(stdout);
}

void tui_show_pipeline(Pipeline *pipeline) {
    parse_count = 0;
    if (!pipeline) return;

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
        snprintf(buf, sizeof(buf), "%scmd[%d]:%s %s",
                 COL_FG_ORANGE, idx, COL_RESET, args);

        strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
        parse_lines[parse_count][MAX_LINE_LEN - 1] = '\0';
        parse_count++;

        /* Show redirects */
        if (cmd->redir_in.type && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "   %s◄%s %s",
                     COL_FG_YELLOW, COL_RESET, cmd->redir_in.filename);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;
        }
        if (cmd->redir_out.type && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "   %s►%s %s %s",
                     COL_FG_YELLOW, COL_RESET,
                     cmd->redir_out.type == REDIR_APPEND ? ">>" : ">",
                     cmd->redir_out.filename);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;
        }

        /* Show pipe indicator */
        if (cmd->next && parse_count < MAX_PANEL_LINES) {
            snprintf(buf, sizeof(buf), "   %s↓ pipe%s", COL_FG_CYAN, COL_RESET);
            strncpy(parse_lines[parse_count], buf, MAX_LINE_LEN - 1);
            parse_count++;
        }

        cmd = cmd->next;
        idx++;
    }

    render_parse();
    fflush(stdout);
}

void tui_log_exec(const char *message) {
    if (exec_count < MAX_PANEL_LINES) {
        strncpy(exec_lines[exec_count], message, MAX_LINE_LEN - 1);
        exec_lines[exec_count][MAX_LINE_LEN - 1] = '\0';
        exec_count++;
    }
    render_execute();
    fflush(stdout);
}

void tui_show_result(int exit_code, const char *output) {
    char buf[MAX_LINE_LEN];
    if (output && output[0]) {
        snprintf(buf, sizeof(buf), "%s%s                                           exit: %s%d%s",
                 output, COL_FG_GRAY, exit_code == 0 ? COL_FG_GREEN : COL_FG_RED,
                 exit_code, COL_RESET);
    } else {
        snprintf(buf, sizeof(buf), "%s                                                     exit: %s%d%s",
                 COL_FG_GRAY, exit_code == 0 ? COL_FG_GREEN : COL_FG_RED,
                 exit_code, COL_RESET);
    }
    tui_update_panel(PANEL_RESULT, buf);
}

void tui_show_error(const char *message) {
    char buf[MAX_LINE_LEN];
    snprintf(buf, sizeof(buf), "%s%s%s", COL_FG_RED, message, COL_RESET);
    tui_update_panel(PANEL_RESULT, buf);
}

char *tui_read_line(void) {
    static char line[1024];
    int pos = 0;

    move_cursor(3, 3);
    printf("%s❯%s ", COL_FG_CYAN, COL_RESET);
    printf("%s", CUR_SHOW);
    fflush(stdout);

    while (1) {
        int c = getchar();
        if (c == EOF || c == '\n') {
            break;
        } else if (c == 127 || c == 8) { /* Backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c >= 32 && pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
            putchar(c);
            fflush(stdout);
        }
    }

    line[pos] = '\0';
    printf("%s", CUR_HIDE);

    /* Update input panel */
    tui_update_panel(PANEL_INPUT, line);

    return strdup(line);
}

void tui_wait_step(const char *step_name) {
    if (!debug_mode) return;

    move_cursor(22, 1);
    printf("%s[DEBUG]%s %s - Press Enter to continue...",
           COL_FG_YELLOW, COL_RESET, step_name);
    fflush(stdout);

    getchar();

    move_cursor(22, 1);
    printf("%s", SCR_CLEAR_LINE);
    fflush(stdout);
}

int tui_is_debug(void) {
    return debug_mode;
}

void tui_set_debug(int enabled) {
    debug_mode = enabled;
}
