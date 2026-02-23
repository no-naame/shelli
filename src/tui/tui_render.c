/*
 * shelli - Educational Shell
 * tui/tui_render.c - Double-buffered rendering engine + page drawing helpers
 *
 * Core: RenderBuf (diff-based flushing, no flicker)
 * Helpers: page_draw_outer_frame, page_draw_stage_dots, page_draw_box, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tui.h"

/*
 * ============================================================================
 * Box Drawing Characters
 * ============================================================================
 */
#define BOX_TL "\342\225\255"  /* ╭ */
#define BOX_TR "\342\225\256"  /* ╮ */
#define BOX_BL "\342\225\260"  /* ╰ */
#define BOX_BR "\342\225\257"  /* ╯ */
#define BOX_H  "\342\224\200"  /* ─ */
#define BOX_V  "\342\224\202"  /* │ */

#define HEAVY_TL "\342\224\217"  /* ┏ */
#define HEAVY_TR "\342\224\223"  /* ┓ */
#define HEAVY_BL "\342\224\227"  /* ┗ */
#define HEAVY_BR "\342\224\233"  /* ┛ */
#define HEAVY_H  "\342\224\201"  /* ━ */
#define HEAVY_V  "\342\224\203"  /* ┃ */

/* Stage indicators */
#define STAGE_FILLED   "\342\227\211"  /* ◉ */
#define STAGE_EMPTY    "\342\227\216"  /* ◎ */

/* Decorative */
#define DIAMOND_EMPTY  "\342\227\207"  /* ◇ */

/*
 * ============================================================================
 * Render Buffer Implementation
 * ============================================================================
 */

static RenderBuf rb_front;
static RenderBuf rb_back;
static int render_bufs_initialized = 0;

/* Animation speed */
static AnimSpeed anim_speed = ANIM_SPEED_NORMAL;

void rbuf_init(RenderBuf *rb, int rows, int cols) {
    rb->rows = rows;
    rb->cols = cols;
    rb->cells = calloc(rows * cols, sizeof(RBCell));
    for (int i = 0; i < rows * cols; i++) {
        rb->cells[i].ch[0] = ' ';
        rb->cells[i].ch[1] = '\0';
        rb->cells[i].fg = -1;
        rb->cells[i].bg = -1;
        rb->cells[i].bold = 0;
        rb->cells[i].dim = 0;
    }
}

void rbuf_free(RenderBuf *rb) {
    free(rb->cells);
    rb->cells = NULL;
    rb->rows = 0;
    rb->cols = 0;
}

void rbuf_clear(RenderBuf *rb) {
    for (int i = 0; i < rb->rows * rb->cols; i++) {
        rb->cells[i].ch[0] = ' ';
        rb->cells[i].ch[1] = '\0';
        rb->cells[i].fg = -1;
        rb->cells[i].bg = COL_BASE;
        rb->cells[i].bold = 0;
        rb->cells[i].dim = 0;
    }
}

void rbuf_put_char(RenderBuf *rb, int row, int col, const char *ch,
                   int fg, int bg, int bold, int dim) {
    if (row < 0 || row >= rb->rows || col < 0 || col >= rb->cols) return;
    RBCell *c = &rb->cells[row * rb->cols + col];
    strncpy(c->ch, ch, 4);
    c->ch[4] = '\0';
    c->fg = fg;
    c->bg = bg;
    c->bold = bold;
    c->dim = dim;
}

void rbuf_put_str(RenderBuf *rb, int row, int col, const char *str,
                  int fg, int bg, int bold, int dim) {
    if (!str) return;
    int c = col;
    const char *p = str;
    while (*p && c < rb->cols) {
        if (row < 0 || row >= rb->rows) return;
        int len = 1;
        if ((*p & 0x80) == 0) len = 1;
        else if ((*p & 0xE0) == 0xC0) len = 2;
        else if ((*p & 0xF0) == 0xE0) len = 3;
        else if ((*p & 0xF8) == 0xF0) len = 4;
        char ch[5] = {0};
        for (int i = 0; i < len && p[i]; i++) ch[i] = p[i];
        rbuf_put_char(rb, row, c, ch, fg, bg, bold, dim);
        p += len;
        c++;
    }
}

void rbuf_put_str_trunc(RenderBuf *rb, int row, int col, const char *str,
                        int fg, int bg, int bold, int dim, int max_width) {
    if (!str) return;
    int c = col;
    int written = 0;
    const char *p = str;
    while (*p && c < rb->cols && written < max_width) {
        if (row < 0 || row >= rb->rows) return;
        int len = 1;
        if ((*p & 0x80) == 0) len = 1;
        else if ((*p & 0xE0) == 0xC0) len = 2;
        else if ((*p & 0xF0) == 0xE0) len = 3;
        else if ((*p & 0xF8) == 0xF0) len = 4;
        char ch[5] = {0};
        for (int i = 0; i < len && p[i]; i++) ch[i] = p[i];
        rbuf_put_char(rb, row, c, ch, fg, bg, bold, dim);
        p += len;
        c++;
        written++;
    }
}

static int cell_eq(const RBCell *a, const RBCell *b) {
    return strcmp(a->ch, b->ch) == 0 &&
           a->fg == b->fg && a->bg == b->bg &&
           a->bold == b->bold && a->dim == b->dim;
}

void rbuf_flush(RenderBuf *rb, RenderBuf *prev) {
    int last_fg = -2, last_bg = -2, last_bold = -1, last_dim = -1;

    for (int r = 0; r < rb->rows; r++) {
        int need_move = 1;
        for (int c = 0; c < rb->cols; c++) {
            RBCell *cur = &rb->cells[r * rb->cols + c];
            if (prev && prev->rows == rb->rows && prev->cols == rb->cols) {
                RBCell *old = &prev->cells[r * prev->cols + c];
                if (cell_eq(cur, old)) {
                    need_move = 1;
                    continue;
                }
            }

            if (need_move) {
                printf(CSI "%d;%dH", r + 1, c + 1);
                need_move = 0;
            }

            if (cur->bold != last_bold || cur->dim != last_dim ||
                cur->fg != last_fg || cur->bg != last_bg) {
                printf(COL_RESET);
                if (cur->bold) printf(COL_BOLD);
                if (cur->dim) printf(COL_DIM);
                if (cur->fg >= 0) printf(CSI "38;5;%dm", cur->fg);
                if (cur->bg >= 0) printf(CSI "48;5;%dm", cur->bg);
                last_fg = cur->fg;
                last_bg = cur->bg;
                last_bold = cur->bold;
                last_dim = cur->dim;
            }

            printf("%s", cur->ch);
        }
    }
    printf(COL_RESET);
    fflush(stdout);

    if (prev && prev->rows == rb->rows && prev->cols == rb->cols) {
        memcpy(prev->cells, rb->cells, rb->rows * rb->cols * sizeof(RBCell));
    }
}

void tui_render_ensure(int rows, int cols) {
    if (render_bufs_initialized &&
        rb_front.rows == rows && rb_front.cols == cols) {
        return;
    }
    if (render_bufs_initialized) {
        rbuf_free(&rb_front);
        rbuf_free(&rb_back);
    }
    rbuf_init(&rb_front, rows, cols);
    rbuf_init(&rb_back, rows, cols);
    for (int i = 0; i < rb_back.rows * rb_back.cols; i++) {
        rb_back.cells[i].fg = -99;
    }
    render_bufs_initialized = 1;
}

void tui_render_get_bufs(RenderBuf **front, RenderBuf **back) {
    *front = &rb_front;
    *back = &rb_back;
}

void tui_render_flush(void) {
    rbuf_flush(&rb_front, &rb_back);
}

/*
 * ============================================================================
 * Token Color Mapping
 * ============================================================================
 */

int token_type_color(TokenType type) {
    switch (type) {
    case TOK_WORD:      return COL_BLUE;
    case TOK_PIPE:      return COL_MAUVE;
    case TOK_REDIR_IN:  return COL_TEAL;
    case TOK_REDIR_OUT: return COL_TEAL;
    case TOK_REDIR_APP: return COL_TEAL;
    case TOK_HEREDOC:   return COL_TEAL;
    case TOK_SEMI:      return COL_PEACH;
    case TOK_AND:       return COL_PEACH;
    case TOK_OR:        return COL_PEACH;
    case TOK_BG:        return COL_PEACH;
    case TOK_EOF:       return COL_OVERLAY;
    }
    return COL_TEXT;
}

int is_shell_keyword(const char *word) {
    static const char *keywords[] = {
        "if", "then", "elif", "else", "fi",
        "while", "until", "do", "done",
        "for", "in", "case", "esac",
        NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

/*
 * ============================================================================
 * Animation Speed
 * ============================================================================
 */

void tui_set_anim_speed(AnimSpeed speed) {
    anim_speed = speed;
}

int tui_get_anim_delay(void) {
    switch (anim_speed) {
    case ANIM_SPEED_NONE:   return 0;
    case ANIM_SPEED_FAST:   return 50000;
    case ANIM_SPEED_NORMAL: return 100000;
    case ANIM_SPEED_SLOW:   return 200000;
    }
    return 100000;
}

/*
 * ============================================================================
 * Page Drawing Helpers
 * ============================================================================
 */

void page_draw_outer_frame(RenderBuf *rb, int tw, int th,
                           const char *stage_name, int stage_color,
                           int stage_num) {
    /* Row 0: top border */
    rbuf_put_char(rb, 0, 0, HEAVY_TL, COL_OVERLAY, COL_BASE, 0, 0);

    int col;

    if (stage_name && stage_name[0]) {
        /* Stage pages: no "shelli", just border then stage name */
        for (int c = 1; c < 3; c++)
            rbuf_put_char(rb, 0, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
        col = 3;
        rbuf_put_char(rb, 0, col, DIAMOND_EMPTY, COL_OVERLAY, COL_BASE, 0, 0);
        col++;
        rbuf_put_str(rb, 0, col, " ", -1, COL_BASE, 0, 0);
        col++;
        rbuf_put_str(rb, 0, col, stage_name, stage_color, COL_BASE, 1, 0);
        col += (int)strlen(stage_name);
        rbuf_put_str(rb, 0, col, " ", -1, COL_BASE, 0, 0);
        col++;
    } else {
        /* Input page: gradient "shelli" in top border */
        for (int c = 1; c < 4; c++)
            rbuf_put_char(rb, 0, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
        const int title_colors[] = {COL_NEON_PINK, COL_NEON_PURPLE, COL_LAVENDER,
                                    COL_BLUE, COL_NEON_CYAN, COL_TEAL};
        const char *title = "shelli";
        rbuf_put_str(rb, 0, 4, " ", -1, COL_BASE, 0, 0);
        for (int i = 0; i < 6; i++) {
            char ch[2] = {title[i], '\0'};
            rbuf_put_char(rb, 0, 5 + i, ch, title_colors[i], COL_BASE, 1, 0);
        }
        rbuf_put_str(rb, 0, 11, " ", -1, COL_BASE, 0, 0);
        col = 12;
    }

    /* Fill rest of top border */
    for (int c = col; c < tw - 1; c++)
        rbuf_put_char(rb, 0, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);

    /* Stage N of 5 on the right */
    if (stage_num > 0) {
        char sn[32];
        snprintf(sn, sizeof(sn), " Stage %d of 5 ", stage_num);
        int sn_len = (int)strlen(sn);
        int sn_x = tw - sn_len - 2;
        if (sn_x > col) {
            rbuf_put_str(rb, 0, sn_x, sn, COL_SUBTEXT, COL_BASE, 0, 0);
        }
    }

    rbuf_put_char(rb, 0, tw - 1, HEAVY_TR, COL_OVERLAY, COL_BASE, 0, 0);

    /* Left/right heavy borders */
    for (int r = 1; r < th - 1; r++) {
        rbuf_put_char(rb, r, 0, HEAVY_V, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_char(rb, r, tw - 1, HEAVY_V, COL_OVERLAY, COL_BASE, 0, 0);
    }

    /* Bottom border */
    int bot = th - 1;
    rbuf_put_char(rb, bot, 0, HEAVY_BL, COL_OVERLAY, COL_BASE, 0, 0);
    for (int c = 1; c < tw - 1; c++)
        rbuf_put_char(rb, bot, c, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_char(rb, bot, tw - 1, HEAVY_BR, COL_OVERLAY, COL_BASE, 0, 0);
}

void page_draw_stage_dots(RenderBuf *rb, int row, int tw, TuiStage current) {
    const char *names[] = {"Tokenize", "AST", "Expand", "Execute", "Result"};
    const int colors[] = {COL_PINK, COL_BLUE, COL_YELLOW, COL_PEACH, COL_GREEN};

    /* Calculate total width for centering */
    int total_w = 0;
    for (int i = 0; i < STAGE_COUNT; i++) {
        total_w += 1 + 1 + (int)strlen(names[i]); /* dot + space + name */
        if (i < STAGE_COUNT - 1) total_w += 6; /* " ━━━━ " */
    }

    int start = (tw - total_w) / 2;
    if (start < 4) start = 4;
    int col = start;

    for (int i = 0; i < STAGE_COUNT; i++) {
        int is_past = (i < (int)current);
        int is_current = (i == (int)current);

        int dot_color, name_color;
        int bold = 0;

        if (is_past || is_current) {
            dot_color = colors[i];
            name_color = colors[i];
            if (is_current) bold = 1;
        } else {
            dot_color = COL_OVERLAY;
            name_color = COL_OVERLAY;
        }

        const char *sym = (is_past || is_current) ? STAGE_FILLED : STAGE_EMPTY;
        rbuf_put_char(rb, row, col, sym, dot_color, COL_BASE, bold, 0);
        col++;
        rbuf_put_str(rb, row, col, " ", -1, COL_BASE, 0, 0);
        col++;
        rbuf_put_str(rb, row, col, names[i], name_color, COL_BASE, bold, 0);
        col += (int)strlen(names[i]);

        if (i < STAGE_COUNT - 1) {
            rbuf_put_str(rb, row, col, " ", -1, COL_BASE, 0, 0);
            col++;
            for (int j = 0; j < 4; j++) {
                rbuf_put_char(rb, row, col, HEAVY_H, COL_OVERLAY, COL_BASE, 0, 0);
                col++;
            }
            rbuf_put_str(rb, row, col, " ", -1, COL_BASE, 0, 0);
            col++;
        }
    }
}

void page_draw_box(RenderBuf *rb, int x, int y, int w, int h,
                   const char *title, int border_color) {
    if (h < 2 || w < 4) return;

    /* Top border */
    rbuf_put_char(rb, y, x, BOX_TL, border_color, COL_BASE, 0, 0);
    rbuf_put_char(rb, y, x + 1, BOX_H, border_color, COL_BASE, 0, 0);

    if (title && title[0]) {
        rbuf_put_str(rb, y, x + 2, " ", -1, COL_BASE, 0, 0);
        rbuf_put_str(rb, y, x + 3, title, border_color, COL_BASE, 0, 0);
        int tlen = (int)strlen(title);
        rbuf_put_str(rb, y, x + 3 + tlen, " ", -1, COL_BASE, 0, 0);
        int fill_start = x + 4 + tlen;
        for (int c = fill_start; c < x + w - 1; c++)
            rbuf_put_char(rb, y, c, BOX_H, border_color, COL_BASE, 0, 0);
    } else {
        for (int c = x + 2; c < x + w - 1; c++)
            rbuf_put_char(rb, y, c, BOX_H, border_color, COL_BASE, 0, 0);
    }
    rbuf_put_char(rb, y, x + w - 1, BOX_TR, border_color, COL_BASE, 0, 0);

    /* Side borders */
    for (int r = y + 1; r < y + h - 1; r++) {
        rbuf_put_char(rb, r, x, BOX_V, border_color, COL_BASE, 0, 0);
        rbuf_put_char(rb, r, x + w - 1, BOX_V, border_color, COL_BASE, 0, 0);
    }

    /* Bottom border */
    int bot = y + h - 1;
    rbuf_put_char(rb, bot, x, BOX_BL, border_color, COL_BASE, 0, 0);
    for (int c = x + 1; c < x + w - 1; c++)
        rbuf_put_char(rb, bot, c, BOX_H, border_color, COL_BASE, 0, 0);
    rbuf_put_char(rb, bot, x + w - 1, BOX_BR, border_color, COL_BASE, 0, 0);
}

void page_draw_input_box(RenderBuf *rb, int x, int y, int w,
                         const char *input) {
    page_draw_box(rb, x, y, w, 3, "Your Input", COL_OVERLAY);
    if (input && input[0]) {
        rbuf_put_str_trunc(rb, y + 1, x + 3, input,
                           COL_TEXT, COL_BASE, 0, 0, w - 6);
    }
}

void page_draw_hints_bar(RenderBuf *rb, int tw, int th,
                         const char **keys, const char **labels,
                         int count) {
    int row = th - 1;
    /* Draw surface background on bottom row */
    for (int c = 0; c < tw; c++) {
        rbuf_put_char(rb, row, c, " ", -1, COL_SURFACE, 0, 0);
    }
    /* Overwrite border chars */
    rbuf_put_char(rb, row, 0, HEAVY_BL, COL_OVERLAY, COL_SURFACE, 0, 0);
    rbuf_put_char(rb, row, tw - 1, HEAVY_BR, COL_OVERLAY, COL_SURFACE, 0, 0);

    int col = 2;
    for (int i = 0; i < count && col < tw - 4; i++) {
        rbuf_put_str(rb, row, col, keys[i], COL_TEXT, COL_SURFACE, 0, 0);
        col += (int)strlen(keys[i]);
        rbuf_put_str(rb, row, col, " ", -1, COL_SURFACE, 0, 0);
        col++;
        rbuf_put_str(rb, row, col, labels[i], COL_OVERLAY, COL_SURFACE, 0, 0);
        col += (int)strlen(labels[i]);
        col += 2;
    }
}
