/*
 * shelli - Educational Shell
 * tui/tui_stage_ast.c - Stage 2: Abstract Syntax Tree page renderer
 *
 * Renders a full-screen page showing the parser's AST output.
 * Each AstDisplayLine is rendered with tree-style prefix, colored icon,
 * label, and detail text.
 */

#include <stdio.h>
#include <string.h>
#include "tui.h"

void page_render_ast(RenderBuf *rb, const StageData *sd,
                     int tw, int th, int scroll) {
    int lx = 4;
    int bw = (tw >= 60) ? tw - 8 - 16 : tw - 8;

    /* Row 0: outer frame */
    page_draw_outer_frame(rb, tw, th, "ABSTRACT SYNTAX TREE", COL_BLUE, 2);

    /* Row 2: stage dots */
    page_draw_stage_dots(rb, 2, tw, STAGE_AST);

    /* Row 4-6: Your Input box */
    page_draw_input_box(rb, lx, 4, bw, sd->input);

    /* Row 8+: AST box */
    int ast_box_y = 8;
    int ast_content_lines = sd->ast_count > 0 ? sd->ast_count : 1;
    int ast_box_h = ast_content_lines + 2; /* top + bottom borders */

    /* Cap box height so it doesn't overflow */
    int max_box_h = th - ast_box_y - 10;
    if (max_box_h < 5) max_box_h = 5;
    if (ast_box_h > max_box_h) ast_box_h = max_box_h;

    page_draw_box(rb, lx, ast_box_y, bw, ast_box_h,
                  "Abstract Syntax Tree", COL_LAVENDER);

    /* Render AST lines inside the box */
    int inner_top = ast_box_y + 1;
    int inner_bot = ast_box_y + ast_box_h - 1;
    int visible_lines = inner_bot - inner_top;

    for (int i = 0; i < sd->ast_count; i++) {
        int r = inner_top + i - scroll;
        if (r < inner_top || r >= inner_bot) continue;

        const AstDisplayLine *line = &sd->ast_lines[i];
        int col = lx + 2;

        /* Prefix (tree connectors) in overlay color */
        if (line->prefix[0]) {
            rbuf_put_str_trunc(rb, r, col, line->prefix,
                               COL_OVERLAY, COL_BASE, 0, 0,
                               bw - 4);
            col += (int)strlen(line->prefix);
        }

        /* Icon in its color */
        if (line->icon[0]) {
            rbuf_put_str(rb, r, col, line->icon,
                         line->icon_color, COL_BASE, 1, 0);
            col += (int)strlen(line->icon);
        }

        /* Space + Label in icon color */
        rbuf_put_str(rb, r, col, " ", -1, COL_BASE, 0, 0);
        col++;
        if (line->label[0]) {
            rbuf_put_str(rb, r, col, line->label,
                         line->icon_color, COL_BASE, 1, 0);
            col += (int)strlen(line->label);
        }

        /* Two spaces + Detail in detail color */
        if (line->detail[0]) {
            rbuf_put_str(rb, r, col, "  ", -1, COL_BASE, 0, 0);
            col += 2;
            int remain = (lx + bw - 2) - col;
            if (remain > 0) {
                rbuf_put_str_trunc(rb, r, col, line->detail,
                                   line->detail_color, COL_BASE, 0, 0,
                                   remain);
            }
        }
    }

    if (sd->ast_count == 0) {
        int r = inner_top;
        if (r < inner_bot) {
            rbuf_put_str(rb, r, lx + 3,
                         "No AST nodes produced.",
                         COL_OVERLAY, COL_BASE, 0, 1);
        }
    }

    /* Scroll indicator */
    if (sd->ast_count > visible_lines) {
        char scroll_info[64];
        int showing_end = scroll + visible_lines;
        if (showing_end > sd->ast_count) showing_end = sd->ast_count;
        snprintf(scroll_info, sizeof(scroll_info),
                 "[%d-%d of %d]", scroll + 1, showing_end, sd->ast_count);
        int si_len = (int)strlen(scroll_info);
        rbuf_put_str(rb, ast_box_y, lx + bw - si_len - 3,
                     scroll_info, COL_OVERLAY, COL_BASE, 0, 1);
    }

    /* "What happened?" educational box */
    int info_y = ast_box_y + ast_box_h + 1;
    int info_h = 6;
    if (info_y + info_h >= th - 2) {
        info_h = th - 2 - info_y;
        if (info_h < 3) info_h = 3;
    }
    page_draw_box(rb, lx, info_y, bw, info_h, "What happened?", COL_BLUE);

    const char *lines[] = {
        "The PARSER consumed the token stream and built an Abstract Syntax",
        "Tree (AST).  Each node represents a structural element: a pipeline,",
        "a simple command, an argument, a redirection, or a control-flow",
        "construct (if/while/for).  The shell walks this tree to execute."
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
