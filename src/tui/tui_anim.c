/*
 * shelli - Educational Shell
 * tui/tui_anim.c - Animation system with easing functions and effects
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "tui.h"

/*
 * Animation types
 */
typedef enum {
    ANIM_NONE = 0,
    ANIM_FADE_IN,      /* Glow: ░ -> ▒ -> ▓ -> █ */
    ANIM_TYPEWRITER,   /* Character by character reveal */
    ANIM_SLIDE_IN,     /* Slide in from direction */
    ANIM_PULSE,        /* Brightness oscillation */
    ANIM_GLOW,         /* Glow border effect */
} AnimType;

/*
 * Glow effect characters
 */
static const char *GLOW_CHARS[] = {
    " ",               /* 0: Empty */
    "\342\226\221",    /* 1: ░ Light shade */
    "\342\226\222",    /* 2: ▒ Medium shade */
    "\342\226\223",    /* 3: ▓ Dark shade */
    "\342\226\210",    /* 4: █ Full block */
};
#define GLOW_LEVELS 5

/*
 * Animation state structure
 */
typedef struct {
    AnimType type;
    int frame;
    int total_frames;
    int complete;
    char *content;
    int content_len;
    int x;
    int y;
} Animation;

/*
 * Global animation state for reuse
 */
#define MAX_ANIMATIONS 8
static Animation animations[MAX_ANIMATIONS];
static int anim_count = 0;

/*
 * Easing function: ease out cubic
 * Fast start, slow end
 */
float ease_out_cubic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

/*
 * Easing function: ease in out quad
 * Slow start, fast middle, slow end
 */
float ease_in_out_quad(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    if (t < 0.5f) {
        return 2.0f * t * t;
    } else {
        return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
    }
}

/*
 * Easing function: ease out elastic
 * Overshoots then settles
 */
float ease_out_elastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    float c4 = (2.0f * 3.14159265f) / 3.0f;
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
}

/*
 * Easing function: linear
 */
float ease_linear(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t;
}

/*
 * Create a new animation
 * type values: 0=NONE, 1=FADE_IN, 2=TYPEWRITER, 3=SLIDE_IN, 4=PULSE, 5=GLOW
 */
int anim_create(int type, const char *content, int x, int y, int frames) {
    if (anim_count >= MAX_ANIMATIONS) {
        return -1;  /* No room for more animations */
    }

    Animation *a = &animations[anim_count];
    a->type = type;
    a->frame = 0;
    a->total_frames = frames > 0 ? frames : 10;
    a->complete = 0;
    a->x = x;
    a->y = y;

    if (content) {
        a->content = strdup(content);
        a->content_len = (int)strlen(content);
    } else {
        a->content = NULL;
        a->content_len = 0;
    }

    return anim_count++;
}

/*
 * Start/reset an animation
 */
void anim_start(int anim_id) {
    if (anim_id < 0 || anim_id >= anim_count) return;
    animations[anim_id].frame = 0;
    animations[anim_id].complete = 0;
}

/*
 * Tick an animation forward one frame
 * Returns 1 when animation is complete
 */
int anim_tick(int anim_id) {
    if (anim_id < 0 || anim_id >= anim_count) return 1;

    Animation *a = &animations[anim_id];
    if (a->complete) return 1;

    a->frame++;
    if (a->frame >= a->total_frames) {
        a->complete = 1;
        return 1;
    }
    return 0;
}

/*
 * Get animation progress (0.0 to 1.0)
 */
float anim_progress(int anim_id) {
    if (anim_id < 0 || anim_id >= anim_count) return 1.0f;
    Animation *a = &animations[anim_id];
    if (a->total_frames <= 0) return 1.0f;
    return (float)a->frame / (float)a->total_frames;
}

/*
 * Render fade-in animation at current frame
 */
static void render_fade_in(Animation *a) {
    float progress = ease_out_cubic((float)a->frame / (float)a->total_frames);
    int glow_level = (int)(progress * (GLOW_LEVELS - 1));
    if (glow_level >= GLOW_LEVELS) glow_level = GLOW_LEVELS - 1;

    printf(CSI "%d;%dH", a->y, a->x);
    if (a->content) {
        /* Reveal content based on glow level */
        if (glow_level == GLOW_LEVELS - 1) {
            printf("%s", a->content);
        } else {
            /* Show glow placeholder */
            for (int i = 0; i < a->content_len; i++) {
                printf("%s", GLOW_CHARS[glow_level]);
            }
        }
    }
}

/*
 * Render typewriter animation at current frame
 */
static void render_typewriter(Animation *a) {
    if (!a->content) return;

    float progress = ease_out_cubic((float)a->frame / (float)a->total_frames);
    int chars_to_show = (int)(progress * a->content_len);
    if (chars_to_show > a->content_len) chars_to_show = a->content_len;

    printf(CSI "%d;%dH", a->y, a->x);
    for (int i = 0; i < chars_to_show; i++) {
        printf("%c", a->content[i]);
    }

    /* Cursor effect at end */
    if (chars_to_show < a->content_len && (a->frame % 2) == 0) {
        printf("_");
    }
}

/*
 * Render pulse animation (brightness oscillation)
 */
static void render_pulse(Animation *a) {
    if (!a->content) return;

    /* Create pulsing effect using sin wave */
    float t = (float)a->frame / (float)a->total_frames;
    float pulse = (sinf(t * 3.14159265f * 4.0f) + 1.0f) / 2.0f;  /* 0 to 1 */

    /* Map pulse to color brightness (between dim and bright) */
    int color = 243 + (int)(pulse * 12);  /* Range 243-255 */
    if (color > 255) color = 255;

    printf(CSI "%d;%dH", a->y, a->x);
    printf(CSI "38;5;%dm%s" COL_RESET, color, a->content);
}

/*
 * Render animation at current frame
 */
void anim_render(int anim_id) {
    if (anim_id < 0 || anim_id >= anim_count) return;

    Animation *a = &animations[anim_id];

    switch (a->type) {
        case ANIM_FADE_IN:
            render_fade_in(a);
            break;
        case ANIM_TYPEWRITER:
            render_typewriter(a);
            break;
        case ANIM_PULSE:
            render_pulse(a);
            break;
        default:
            /* Unsupported animation type - just draw content */
            if (a->content) {
                printf(CSI "%d;%dH%s", a->y, a->x, a->content);
            }
            break;
    }
    fflush(stdout);
}

/*
 * Check if animation is complete
 */
int anim_is_complete(int anim_id) {
    if (anim_id < 0 || anim_id >= anim_count) return 1;
    return animations[anim_id].complete;
}

/*
 * Destroy an animation and free resources
 */
void anim_destroy(int anim_id) {
    if (anim_id < 0 || anim_id >= anim_count) return;

    Animation *a = &animations[anim_id];
    if (a->content) {
        free(a->content);
        a->content = NULL;
    }
    a->type = ANIM_NONE;
}

/*
 * Clear all animations
 */
void anim_clear_all(void) {
    for (int i = 0; i < anim_count; i++) {
        anim_destroy(i);
    }
    anim_count = 0;
}

/*
 * Run a simple fade-in animation blocking
 * Convenience function for quick effects
 */
void anim_fade_in_blocking(int x, int y, const char *content, int duration_ms) {
    int frames = duration_ms / 16;  /* ~60fps */
    if (frames < 5) frames = 5;

    int anim_id = anim_create(ANIM_FADE_IN, content, x, y, frames);
    if (anim_id < 0) {
        /* Fallback: just print content */
        printf(CSI "%d;%dH%s", y, x, content);
        fflush(stdout);
        return;
    }

    while (!anim_is_complete(anim_id)) {
        anim_render(anim_id);
        anim_tick(anim_id);
        usleep(16000);  /* ~60fps */
    }

    /* Final render */
    printf(CSI "%d;%dH%s", y, x, content);
    fflush(stdout);

    anim_destroy(anim_id);
}

/*
 * Run a typewriter animation blocking
 */
void anim_typewriter_blocking(int x, int y, const char *content, int duration_ms) {
    int frames = duration_ms / 16;
    if (frames < 5) frames = 5;

    int anim_id = anim_create(ANIM_TYPEWRITER, content, x, y, frames);
    if (anim_id < 0) {
        printf(CSI "%d;%dH%s", y, x, content);
        fflush(stdout);
        return;
    }

    while (!anim_is_complete(anim_id)) {
        anim_render(anim_id);
        anim_tick(anim_id);
        usleep(16000);
    }

    /* Final render */
    printf(CSI "%d;%dH%s", y, x, content);
    fflush(stdout);

    anim_destroy(anim_id);
}
