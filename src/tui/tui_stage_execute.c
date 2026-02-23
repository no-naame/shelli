/*
 * shelli - Educational Shell
 * tui/tui_stage_execute.c - Stage 4: Execute page renderer
 *
 * Renders a full-screen page showing the execution trace.
 * Each ExecLine is rendered with prefix_char, indentation, and colored text.
 */

#include <stdio.h>
#include <string.h>
#include "tui.h"

void page_render_execute(RenderBuf *rb, const StageData *sd,
                         int tw, int th, int scroll) {
    int lx = 4;
    int bw = (tw >= 60) ? tw - 8 - 16 : tw - 8;

    /* Row 0: outer frame */
    page_draw_outer_frame(rb, tw, th, "EXECUTE", COL_PEACH, 4);

    /* Row 2: stage dots */
    page_draw_stage_dots(rb, 2, tw, STAGE_EXECUTE);

    /* Row 4-6: Your Input box */
    page_draw_input_box(rb, lx, 4, bw, sd->input);

    /* Row 8+: Execution Trace box */
    int exec_box_y = 8;
    int exec_content = sd->exec_count > 0 ? sd->exec_count : 1;
    int exec_box_h = exec_content + 2; /* top + bottom borders */

    /* Cap box height */
    int max_box_h = th - exec_box_y - 10;
    if (max_box_h < 4) max_box_h = 4;
    if (exec_box_h > max_box_h) exec_box_h = max_box_h;

    page_draw_box(rb, lx, exec_box_y, bw, exec_box_h,
                  "Execution Trace", COL_PEACH);

    int inner_top = exec_box_y + 1;
    int inner_bot = exec_box_y + exec_box_h - 1;
    int visible_lines = inner_bot - inner_top;

    if (sd->exec_count == 0) {
        int r = inner_top;
        if (r < inner_bot) {
            rbuf_put_str(rb, r, lx + 3,
                         "No execution trace recorded.",
                         COL_OVERLAY, COL_BASE, 0, 1);
        }
    } else {
        for (int i = 0; i < sd->exec_count; i++) {
            int r = inner_top + i - scroll;
            if (r < inner_top || r >= inner_bot) continue;

            const ExecLine *line = &sd->exec_lines[i];
            int col = lx + 3;

            /* Indent: 4 spaces per indent level */
            if (line->indent > 0) {
                for (int j = 0; j < line->indent * 4 && col < lx + bw - 2; j++) {
                    rbuf_put_char(rb, r, col, " ", -1, COL_BASE, 0, 0);
                    col++;
                }
            }

            /* Prefix character with appropriate color */
            if (line->prefix_char) {
                int prefix_color = line->color;
                /* Use distinct colors for different prefix types */
                switch (line->prefix_char) {
                case '>': prefix_color = COL_PEACH;   break; /* process */
                case '~': prefix_color = COL_TEAL;    break; /* I/O */
                case '*': prefix_color = COL_GREEN;    break; /* exec */
                case '.': prefix_color = COL_OVERLAY;  break; /* wait */
                default:  prefix_color = line->color;  break;
                }
                char pch[2] = {line->prefix_char, '\0'};
                rbuf_put_str(rb, r, col, pch,
                             prefix_color, COL_BASE, 1, 0);
                col++;

                /* Space after prefix */
                rbuf_put_char(rb, r, col, " ", -1, COL_BASE, 0, 0);
                col++;
            }

            /* Text in the line's color */
            int remain = (lx + bw - 2) - col;
            if (remain > 0 && line->text[0]) {
                rbuf_put_str_trunc(rb, r, col, line->text,
                                   line->color, COL_BASE, 0, 0,
                                   remain);
            }
        }
    }

    /* Scroll indicator */
    if (sd->exec_count > visible_lines) {
        char scroll_info[64];
        int showing_end = scroll + visible_lines;
        if (showing_end > sd->exec_count) showing_end = sd->exec_count;
        snprintf(scroll_info, sizeof(scroll_info),
                 "[%d-%d of %d]", scroll + 1, showing_end, sd->exec_count);
        int si_len = (int)strlen(scroll_info);
        rbuf_put_str(rb, exec_box_y, lx + bw - si_len - 3,
                     scroll_info, COL_OVERLAY, COL_BASE, 0, 1);
    }

    /* "What happened?" educational box */
    int info_y = exec_box_y + exec_box_h + 1;
    int info_h = 7;
    if (info_y + info_h >= th - 2) {
        info_h = th - 2 - info_y;
        if (info_h < 3) info_h = 3;
    }
    page_draw_box(rb, lx, info_y, bw, info_h, "What happened?", COL_BLUE);

    const char *lines[] = {
        "The shell used SYSTEM CALLS to execute your command:",
        "  pipe()    - create pipes for | operators",
        "  fork()    - spawn a child process for each command",
        "  dup2()    - redirect stdin/stdout to pipes or files",
        "  execvp()  - replace child process with the target program",
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
        mascot_render(rb, mascot_x, mascot_y, MOOD_WORKING, mascot_frame);
        mascot_frame = (mascot_frame + 1) % 2;
    }

    /* Hints bar */
    const char *keys[]   = {"[\xe2\x86\x90]", "[\xe2\x86\x92]",
                            "[\xe2\x86\x91\xe2\x86\x93]",
                            "[Enter]", "[Q]"};
    const char *labels[] = {"prev", "next", "scroll", "new command", "quit"};
    page_draw_hints_bar(rb, tw, th, keys, labels, 5);
}
