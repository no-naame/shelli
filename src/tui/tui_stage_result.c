/*
 * shelli - Educational Shell
 * tui/tui_stage_result.c - Stage 5: Result page renderer
 *
 * Renders a full-screen page showing command output and exit code.
 * Displays error box (red) on failure, or output + exit code on success.
 */

#include <stdio.h>
#include <string.h>
#include "tui.h"

void page_render_result(RenderBuf *rb, const StageData *sd,
                        int tw, int th, int scroll) {
    int lx = 4;
    int bw = (tw >= 60) ? tw - 8 - 16 : tw - 8;

    /* Row 0: outer frame */
    page_draw_outer_frame(rb, tw, th, "RESULT", COL_GREEN, 5);

    /* Row 2: stage dots (all filled at final stage) */
    page_draw_stage_dots(rb, 2, tw, STAGE_RESULT);

    /* Row 4-6: Your Input box */
    page_draw_input_box(rb, lx, 4, bw, sd->input);

    int content_y = 8;

    if (sd->has_error) {
        /* Error box */
        int err_h = 4;
        page_draw_box(rb, lx, content_y, bw, err_h, "Error", COL_RED);

        /* Error icon + message */
        int r = content_y + 1;
        rbuf_put_str(rb, r, lx + 3,
                     "\xe2\x9c\x97", COL_RED, COL_BASE, 1, 0); /* ✗ */
        rbuf_put_str(rb, r, lx + 5, " ", -1, COL_BASE, 0, 0);
        rbuf_put_str_trunc(rb, r, lx + 6, sd->error_msg,
                           COL_RED, COL_BASE, 0, 0, bw - 10);

        /* Exit code line */
        r = content_y + 2;
        char exit_str[64];
        snprintf(exit_str, sizeof(exit_str), "exit code: %d", sd->exit_code);
        rbuf_put_str(rb, r, lx + 5, exit_str,
                     COL_OVERLAY, COL_BASE, 0, 1);

        content_y += err_h + 1;
    } else {
        /* Output box */
        int out_content = sd->output_count > 0 ? sd->output_count : 1;
        int out_box_h = out_content + 2; /* top + bottom borders */

        /* Cap box height */
        int max_out_h = th - content_y - 14;
        if (max_out_h < 4) max_out_h = 4;
        if (out_box_h > max_out_h) out_box_h = max_out_h;

        page_draw_box(rb, lx, content_y, bw, out_box_h, "Output", COL_GREEN);

        int inner_top = content_y + 1;
        int inner_bot = content_y + out_box_h - 1;
        int visible_lines = inner_bot - inner_top;

        if (sd->output_count == 0) {
            int r = inner_top;
            if (r < inner_bot) {
                rbuf_put_str(rb, r, lx + 3,
                             "(no output)",
                             COL_OVERLAY, COL_BASE, 0, 1);
            }
        } else {
            for (int i = 0; i < sd->output_count; i++) {
                int r = inner_top + i - scroll;
                if (r < inner_top || r >= inner_bot) continue;
                rbuf_put_str_trunc(rb, r, lx + 3, sd->output_lines[i],
                                   COL_TEXT, COL_BASE, 0, 0, bw - 6);
            }
        }

        /* Scroll indicator for output */
        if (sd->output_count > visible_lines) {
            char scroll_info[64];
            int showing_end = scroll + visible_lines;
            if (showing_end > sd->output_count) showing_end = sd->output_count;
            snprintf(scroll_info, sizeof(scroll_info),
                     "[%d-%d of %d]", scroll + 1, showing_end,
                     sd->output_count);
            int si_len = (int)strlen(scroll_info);
            rbuf_put_str(rb, content_y, lx + bw - si_len - 3,
                         scroll_info, COL_OVERLAY, COL_BASE, 0, 1);
        }

        content_y += out_box_h + 1;

        /* Exit Code box */
        int ec_box_h = 3;
        page_draw_box(rb, lx, content_y, bw, ec_box_h, "Exit Code", COL_OVERLAY);

        int r = content_y + 1;
        if (sd->exit_code == 0) {
            /* Success: green checkmark */
            rbuf_put_str(rb, r, lx + 3,
                         "\xe2\x9c\x93", COL_GREEN, COL_BASE, 1, 0); /* ✓ */
            rbuf_put_str(rb, r, lx + 5,
                         " exit code: 0  (success)",
                         COL_GREEN, COL_BASE, 0, 0);
        } else {
            /* Failure: red X with code */
            rbuf_put_str(rb, r, lx + 3,
                         "\xe2\x9c\x97", COL_RED, COL_BASE, 1, 0); /* ✗ */
            char ec_str[128];
            const char *desc = "";
            switch (sd->exit_code) {
            case 1:   desc = "general error"; break;
            case 2:   desc = "misuse of builtin"; break;
            case 126:  desc = "permission denied"; break;
            case 127:  desc = "command not found"; break;
            case 130:  desc = "interrupted (SIGINT)"; break;
            case 137:  desc = "killed (SIGKILL)"; break;
            case 139:  desc = "segmentation fault"; break;
            case 141:  desc = "broken pipe (SIGPIPE)"; break;
            case 143:  desc = "terminated (SIGTERM)"; break;
            default:
                if (sd->exit_code > 128 && sd->exit_code < 192)
                    desc = "killed by signal";
                else
                    desc = "failure";
                break;
            }
            snprintf(ec_str, sizeof(ec_str),
                     " exit code: %d  (%s)", sd->exit_code, desc);
            rbuf_put_str_trunc(rb, r, lx + 5, ec_str,
                               COL_RED, COL_BASE, 0, 0, bw - 10);
        }

        content_y += ec_box_h + 1;
    }

    /* "What happened?" educational box */
    int info_y = content_y;
    int info_h = 6;
    if (info_y + info_h >= th - 2) {
        info_h = th - 2 - info_y;
        if (info_h < 3) info_h = 3;
    }

    if (info_h >= 3 && info_y < th - 2) {
        page_draw_box(rb, lx, info_y, bw, info_h, "What happened?", COL_BLUE);

        const char *lines[] = {
            "Every process returns an EXIT CODE when it finishes:",
            "  0        = success (the command did what you asked)",
            "  1-125    = command-specific error codes",
            "  127      = command not found in PATH",
        };
        int nlines = 4;
        for (int i = 0; i < nlines; i++) {
            int r = info_y + 1 + i;
            if (r < info_y + info_h - 1 && r < th - 1) {
                rbuf_put_str_trunc(rb, r, lx + 3, lines[i],
                                   COL_SUBTEXT, COL_BASE, 0, 0, bw - 6);
            }
        }
    }

    /* Mascot (top-right, fixed position) */
    if (tw >= 60) {
        static int mascot_frame = 0;
        MascotMood mood = (sd->exit_code == 0 && !sd->has_error)
                          ? MOOD_HAPPY : MOOD_SAD;
        int mascot_x = tw - 20;
        int mascot_y = 4;
        mascot_render(rb, mascot_x, mascot_y, mood, mascot_frame);
        mascot_frame = (mascot_frame + 1) % 2;
    }

    /* Hints bar */
    const char *keys[]   = {"[\xe2\x86\x90]", "[\xe2\x86\x92]",
                            "[\xe2\x86\x91\xe2\x86\x93]",
                            "[Enter]", "[Q]"};
    const char *labels[] = {"prev", "next", "scroll", "new command", "quit"};
    page_draw_hints_bar(rb, tw, th, keys, labels, 5);
}
