/*
 * shelli - Educational Shell
 * tui/tui_stage_tokenize.c - Stage 1: Tokenize page renderer
 *
 * Renders a full-screen page showing the lexer's tokenization results.
 * Token chips flow horizontally with wrapping, each showing value + label.
 */

#include <stdio.h>
#include <string.h>
#include "tui.h"

void page_render_tokenize(RenderBuf *rb, const StageData *sd,
                          int tw, int th, int scroll) {
    int lx = 4;
    int bw = (tw >= 60) ? tw - 8 - 16 : tw - 8;

    /* Row 0: outer frame */
    page_draw_outer_frame(rb, tw, th, "TOKENIZE", COL_PINK, 1);

    /* Row 2: stage dots */
    page_draw_stage_dots(rb, 2, tw, STAGE_TOKENIZE);

    /* Row 4-6: Your Input box */
    page_draw_input_box(rb, lx, 4, bw, sd->input);

    /* Row 8+: Tokens Found box */
    /* First compute how many rows the token chips need */
    int chip_area_x = lx + 3;           /* left padding inside box */
    int chip_area_w = bw - 6;           /* usable width inside box */
    int chip_col = 0;
    int chip_rows = 1;

    /* Each chip is: [value] + 2 spaces separator
     * We need to pre-calculate the row count to size the box.
     * Chip width = max(strlen(value), strlen(label)) + 2 padding */
    typedef struct { int w; } ChipInfo;
    ChipInfo chips[64];
    for (int i = 0; i < sd->token_count && i < 64; i++) {
        int vlen = (int)strlen(sd->tokens[i].value);
        int llen = (int)strlen(sd->tokens[i].label);
        int cw = (vlen > llen ? vlen : llen) + 2; /* 1 pad each side */
        if (cw < 6) cw = 6;
        chips[i].w = cw;
    }

    /* Calculate rows needed */
    chip_col = 0;
    chip_rows = 1;
    for (int i = 0; i < sd->token_count; i++) {
        int cw = chips[i].w;
        if (chip_col > 0 && chip_col + cw > chip_area_w) {
            chip_rows++;
            chip_col = 0;
        }
        chip_col += cw + 2; /* chip width + 2 spaces separator */
    }
    if (sd->token_count == 0) chip_rows = 1;

    /* Each chip takes 2 display rows (value + label), plus 1 blank between chip rows */
    int content_rows = chip_rows * 3;
    int token_box_h = content_rows + 3; /* top border + 1 pad + content + bottom border */
    if (sd->token_count == 0) token_box_h = 4;

    /* Title for the token box */
    char token_title[64];
    snprintf(token_title, sizeof(token_title), "Tokens Found: %d", sd->token_count);

    int token_box_y = 8;
    page_draw_box(rb, lx, token_box_y, bw, token_box_h, token_title, COL_PINK);

    /* Render token chips inside the box */
    int base_row = token_box_y + 1;
    int cur_row = 0;
    chip_col = 0;

    for (int i = 0; i < sd->token_count; i++) {
        int cw = chips[i].w;

        /* Wrap to next row if needed */
        if (chip_col > 0 && chip_col + cw > chip_area_w) {
            cur_row++;
            chip_col = 0;
        }

        /* Apply scroll offset */
        int draw_row_val = base_row + cur_row * 3 - scroll;
        int draw_row_lbl = draw_row_val + 1;
        int draw_col = chip_area_x + chip_col;

        /* Draw value line (colored per token) */
        if (draw_row_val > token_box_y && draw_row_val < token_box_y + token_box_h - 1) {
            /* Center the value within the chip width */
            int vlen = (int)strlen(sd->tokens[i].value);
            int pad = (cw - vlen) / 2;
            if (pad < 0) pad = 0;
            int fg = sd->tokens[i].color;
            int bold = sd->tokens[i].is_command;
            rbuf_put_str_trunc(rb, draw_row_val, draw_col + pad,
                               sd->tokens[i].value,
                               fg, COL_BASE, bold, 0, cw);
        }

        /* Draw label line (dim, in overlay color) */
        if (draw_row_lbl > token_box_y && draw_row_lbl < token_box_y + token_box_h - 1) {
            int llen = (int)strlen(sd->tokens[i].label);
            int pad = (cw - llen) / 2;
            if (pad < 0) pad = 0;
            rbuf_put_str_trunc(rb, draw_row_lbl, draw_col + pad,
                               sd->tokens[i].label,
                               COL_OVERLAY, COL_BASE, 0, 1, cw);
        }

        chip_col += cw + 2;
    }

    if (sd->token_count == 0) {
        int r = base_row + 1 - scroll;
        if (r > token_box_y && r < token_box_y + token_box_h - 1) {
            rbuf_put_str(rb, r, chip_area_x,
                         "No tokens produced.",
                         COL_OVERLAY, COL_BASE, 0, 1);
        }
    }

    /* "What happened?" educational box */
    int info_y = token_box_y + token_box_h + 1;
    int info_h = 6;
    if (info_y + info_h >= th - 2) {
        info_h = th - 2 - info_y;
        if (info_h < 3) info_h = 3;
    }
    page_draw_box(rb, lx, info_y, bw, info_h, "What happened?", COL_BLUE);

    const char *lines[] = {
        "The LEXER scanned your input character by character and broke it",
        "into tokens -- the smallest meaningful units of shell syntax.",
        "Each token has a type (WORD, PIPE, REDIRECT, ...) that tells the",
        "parser what role it plays in your command."
    };
    int nlines = 4;
    for (int i = 0; i < nlines; i++) {
        int r = info_y + 1 + i;
        if (r < info_y + info_h - 1 && r < th - 1) {
            rbuf_put_str_trunc(rb, r, lx + 3, lines[i],
                               COL_SUBTEXT, COL_BASE, 0, 0, bw - 6);
        }
    }

    /* Mascot (top-right, fixed position) */
    if (tw >= 60) {
        static int mascot_frame = 0;
        int mascot_x = tw - 20;
        int mascot_y = 4;
        mascot_render(rb, mascot_x, mascot_y, MOOD_THINKING, mascot_frame);
        mascot_frame = (mascot_frame + 1) % 2;
    }

    /* Hints bar */
    const char *keys[]   = {"[\xe2\x86\x90]", "[\xe2\x86\x92]",
                            "[\xe2\x86\x91\xe2\x86\x93]",
                            "[Enter]", "[Q]"};
    const char *labels[] = {"prev", "next", "scroll", "new command", "quit"};
    page_draw_hints_bar(rb, tw, th, keys, labels, 5);
}
