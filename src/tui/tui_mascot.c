/*
 * shelli - Educational Shell
 * tui/tui_mascot.c - ASCII art mascot rendering
 *
 * Dense "Double Frame" block-character mascot with mood variants
 * and a 2-frame breathing animation (legs apart / legs together).
 *
 * 10 columns wide, 6 rows tall.
 */

#include <stdio.h>
#include <string.h>
#include "tui.h"

/* ── UTF-8 encoded box-drawing / block characters ── */

/* Double-line frame */
#define DBL_TL  "\xe2\x95\x94"  /* ╔ */
#define DBL_TR  "\xe2\x95\x97"  /* ╗ */
#define DBL_BL  "\xe2\x95\x9a"  /* ╚ */
#define DBL_BR  "\xe2\x95\x9d"  /* ╝ */
#define DBL_H   "\xe2\x95\x90"  /* ═ */
#define DBL_V   "\xe2\x95\x91"  /* ║ */
#define DBL_BT  "\xe2\x95\xa6"  /* ╦ */
#define DBL_TT  "\xe2\x95\xa9"  /* ╩ */

/* Fill / decoration */
#define SHADE   "\xe2\x96\x91"  /* ░ */
#define DASH    "\xe2\x94\x88"  /* ┈ */

/* Eyes per mood */
#define EYE_NORMAL   "\xe2\x96\x80"  /* ▀ */
#define EYE_THINKING "\xe2\x96\x93"  /* ▓ */
#define EYE_HAPPY    "\xe2\x96\x80"  /* ▀ */
#define EYE_SAD      "\xe2\x96\x84"  /* ▄ */
#define EYE_WORKING  "\xe2\x96\x92"  /* ▒ */

/* Mouth per mood */
#define MOUTH_NORMAL   "\xe2\x96\xbd"  /* ▽ */
#define MOUTH_THINKING "\xe2\x94\x80"  /* ─ */
#define MOUTH_HAPPY    "\xe2\x96\xbf"  /* ▿ */
#define MOUTH_SAD      "\xe2\x96\xb5"  /* ▵ */
#define MOUTH_WORKING  "\xe2\x95\x90"  /* ═ */

/* Chest symbols per mood */
#define CHEST_HEART  "\xe2\x99\xa5"    /* ♥ */
#define CHEST_STAR   "\xe2\x9c\xa6"    /* ✦ */
#define CHEST_BOLT   "\xe2\x9a\xa1"    /* ⚡ */

/* Per-mood face data */
typedef struct {
    const char *eye;
    const char *mouth;
    const char *chest;
    int chest_color;
} MascotFace;

static const MascotFace faces[] = {
    [MOOD_NORMAL]   = { EYE_NORMAL,   MOUTH_NORMAL,   CHEST_HEART, COL_PINK },
    [MOOD_THINKING] = { EYE_THINKING, MOUTH_THINKING, CHEST_STAR,  COL_NEON_CYAN },
    [MOOD_HAPPY]    = { EYE_HAPPY,    MOUTH_HAPPY,    CHEST_HEART, COL_PINK },
    [MOOD_SAD]      = { EYE_SAD,      MOUTH_SAD,      SHADE,       COL_OVERLAY },
    [MOOD_WORKING]  = { EYE_WORKING,  MOUTH_WORKING,  CHEST_BOLT,  COL_YELLOW },
};

/*
 * mascot_render - Draw the "Double Frame" mascot into a RenderBuf
 *
 * Layout (10 wide, 6 tall):
 *
 *   col: 0123456789
 *   r0:  ╔════════╗
 *   r1:  ║░░▀░▀░░║
 *   r2:  ║░░░▽░░░║
 *   r3:  ║░░♥♥♥░░║
 *   r4:  ╚══╦══╦══╝   (frame A) / ╚═╦════╦═╝ (frame B)
 *   r5:  ┈┈┈╩┈┈╩┈┈   (frame A) / ┈┈╩┈┈┈┈╩┈┈ (frame B)
 */
void mascot_render(RenderBuf *rb, int x, int y, MascotMood mood, int frame)
{
    if (!rb) return;
    if (mood < MOOD_NORMAL || mood > MOOD_WORKING)
        mood = MOOD_NORMAL;

    const MascotFace *f = &faces[mood];
    int body  = COL_LAVENDER;
    int skin  = COL_OVERLAY;
    int eye   = COL_TEAL;
    int mouth = COL_TEAL;
    int bg    = COL_BASE;

    /* Row 0: ╔════════╗ */
    rbuf_put_char(rb, y, x,     DBL_TL, body, bg, 0, 0);
    for (int c = 1; c <= 8; c++)
        rbuf_put_char(rb, y, x + c, DBL_H, body, bg, 0, 0);
    rbuf_put_char(rb, y, x + 9, DBL_TR, body, bg, 0, 0);

    /* Row 1: ║░░▀░▀░░║  (eyes row) */
    rbuf_put_char(rb, y + 1, x,     DBL_V, body, bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 1, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 2, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 3, f->eye, eye, bg, 1, 0);
    rbuf_put_char(rb, y + 1, x + 4, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 5, f->eye, eye, bg, 1, 0);
    rbuf_put_char(rb, y + 1, x + 6, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 7, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 8, " ",   -1,  bg, 0, 0);
    rbuf_put_char(rb, y + 1, x + 9, DBL_V, body, bg, 0, 0);

    /* Row 2: ║░░░▽░░░║  (mouth row) */
    rbuf_put_char(rb, y + 2, x,     DBL_V, body, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 1, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 2, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 3, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 4, f->mouth, mouth, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 5, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 6, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 7, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 8, " ",   -1,  bg, 0, 0);
    rbuf_put_char(rb, y + 2, x + 9, DBL_V, body, bg, 0, 0);

    /* Row 3: ║░░♥♥♥░░║  (chest row) */
    rbuf_put_char(rb, y + 3, x,     DBL_V, body, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 1, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 2, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 3, f->chest, f->chest_color, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 4, f->chest, f->chest_color, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 5, f->chest, f->chest_color, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 6, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 7, SHADE, skin, bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 8, " ",   -1,  bg, 0, 0);
    rbuf_put_char(rb, y + 3, x + 9, DBL_V, body, bg, 0, 0);

    /* Row 4 + 5: legs (breathing animation) */
    if (frame == 0) {
        /* Frame A (inhale): ╚══╦══╦══╝ */
        rbuf_put_char(rb, y + 4, x,     DBL_BL, body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 1, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 2, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 3, DBL_BT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 4, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 5, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 6, DBL_BT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 7, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 8, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 9, DBL_BR, body, bg, 0, 0);

        /* Frame A ground: ┈┈┈╩┈┈╩┈┈  (pad to 10) */
        rbuf_put_char(rb, y + 5, x,     DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 1, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 2, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 3, DBL_TT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 5, x + 4, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 5, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 6, DBL_TT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 5, x + 7, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 8, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 9, " ",    -1,   bg, 0, 0);
    } else {
        /* Frame B (exhale): ╚═╦════╦═╝ */
        rbuf_put_char(rb, y + 4, x,     DBL_BL, body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 1, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 2, DBL_BT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 3, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 4, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 5, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 6, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 7, DBL_BT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 8, DBL_H,  body, bg, 0, 0);
        rbuf_put_char(rb, y + 4, x + 9, DBL_BR, body, bg, 0, 0);

        /* Frame B ground: ┈┈╩┈┈┈┈╩┈┈ */
        rbuf_put_char(rb, y + 5, x,     DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 1, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 2, DBL_TT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 5, x + 3, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 4, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 5, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 6, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 7, DBL_TT, body, bg, 0, 0);
        rbuf_put_char(rb, y + 5, x + 8, DASH,   skin, bg, 0, 1);
        rbuf_put_char(rb, y + 5, x + 9, DASH,   skin, bg, 0, 1);
    }
}

/*
 * mascot_mood_for_stage - Determine mascot mood from pipeline stage
 *
 * Mapping:
 *   STAGE_TOKENIZE, STAGE_AST, STAGE_EXPAND  -> MOOD_THINKING
 *   STAGE_EXECUTE                              -> MOOD_WORKING
 *   STAGE_RESULT                               -> MOOD_HAPPY (exit 0)
 *                                                 MOOD_SAD   (exit != 0)
 */
MascotMood mascot_mood_for_stage(TuiStage stage, int exit_code)
{
    switch (stage) {
    case STAGE_TOKENIZE:
    case STAGE_AST:
    case STAGE_EXPAND:
        return MOOD_THINKING;

    case STAGE_EXECUTE:
        return MOOD_WORKING;

    case STAGE_RESULT:
        return (exit_code == 0) ? MOOD_HAPPY : MOOD_SAD;

    default:
        return MOOD_NORMAL;
    }
}
