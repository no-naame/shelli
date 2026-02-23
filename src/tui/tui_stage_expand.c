/*
 * shelli - Educational Shell
 * tui/tui_stage_expand.c - Stage 3: Expand page renderer
 *
 * Renders a full-screen page showing word expansion results.
 * Each expansion is displayed as: original -> expanded (type label).
 */

#include <stdio.h>
#include <string.h>
#include "tui.h"

void page_render_expand(RenderBuf *rb, const StageData *sd,
                        int tw, int th, int scroll) {
    int lx = 4;
    int bw = (tw >= 60) ? tw - 8 - 16 : tw - 8;

    /* Row 0: outer frame */
    page_draw_outer_frame(rb, tw, th, "EXPAND", COL_YELLOW, 3);

    /* Row 2: stage dots */
    page_draw_stage_dots(rb, 2, tw, STAGE_EXPAND);

    /* Row 4-6: Your Input box */
    page_draw_input_box(rb, lx, 4, bw, sd->input);

    /* Row 8+: Word Expansions box */
    int exp_box_y = 8;
    int exp_content;
    if (sd->expansion_count == 0) {
        exp_content = 1;
    } else {
        /* Each expansion takes 2 lines: original->expanded, then type label */
        exp_content = sd->expansion_count * 2;
    }
    int exp_box_h = exp_content + 2; /* top + bottom borders */

    /* Cap box height */
    int max_box_h = th - exp_box_y - 10;
    if (max_box_h < 4) max_box_h = 4;
    if (exp_box_h > max_box_h) exp_box_h = max_box_h;

    page_draw_box(rb, lx, exp_box_y, bw, exp_box_h,
                  "Word Expansions", COL_YELLOW);

    int inner_top = exp_box_y + 1;
    int inner_bot = exp_box_y + exp_box_h - 1;

    if (sd->expansion_count == 0) {
        int r = inner_top;
        if (r < inner_bot) {
            rbuf_put_str(rb, r, lx + 3,
                         "No expansions were needed for this command.",
                         COL_OVERLAY, COL_BASE, 0, 1);
        }
    } else {
        for (int i = 0; i < sd->expansion_count; i++) {
            const ExpansionDisplay *exp = &sd->expansions[i];

            /* Line 1: original  -->  expanded */
            int line_idx = i * 2;
            int r1 = inner_top + line_idx - scroll;

            if (r1 >= inner_top && r1 < inner_bot) {
                int col = lx + 3;

                /* Original word in yellow */
                rbuf_put_str_trunc(rb, r1, col, exp->original,
                                   COL_YELLOW, COL_BASE, 0, 0,
                                   bw / 3);
                col += (int)strlen(exp->original);
                if (col > lx + bw / 3) col = lx + bw / 3;

                /* Arrow in teal: "  └──→  " */
                const char *arrow = "  \xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x86\x92  ";
                rbuf_put_str(rb, r1, col, arrow,
                             COL_TEAL, COL_BASE, 0, 0);
                col += 10; /* approximate width of arrow string */

                /* Expanded value in green */
                int remain = (lx + bw - 2) - col;
                if (remain > 0) {
                    rbuf_put_str_trunc(rb, r1, col, exp->expanded,
                                       COL_GREEN, COL_BASE, 0, 0,
                                       remain);
                }
            }

            /* Line 2: type label (indented, dim) */
            int r2 = inner_top + line_idx + 1 - scroll;
            if (r2 >= inner_top && r2 < inner_bot) {
                rbuf_put_str_trunc(rb, r2, lx + 5, exp->type_label,
                                   COL_OVERLAY, COL_BASE, 0, 1,
                                   bw - 8);
            }
        }
    }

    /* "What happened?" educational box */
    int info_y = exp_box_y + exp_box_h + 1;
    int info_h = 7;
    if (info_y + info_h >= th - 2) {
        info_h = th - 2 - info_y;
        if (info_h < 3) info_h = 3;
    }
    page_draw_box(rb, lx, info_y, bw, info_h, "What happened?", COL_BLUE);

    const char *lines[] = {
        "The shell performed WORD EXPANSION on each argument before",
        "executing the command.  Expansions happen in this order:",
        "  1. Brace expansion    {a,b}  ->  a b",
        "  2. Tilde expansion    ~  ->  /home/user",
        "  3. Parameter/variable expansion   $VAR  ->  value",
    };
    int nlines = 5;
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
