/*
 * shelli - Educational Shell
 * tui/tui_icons.c - Nerd Font icons with ASCII fallbacks
 *
 * Requires a Nerd Font for best experience:
 * https://www.nerdfonts.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tui.h"

/*
 * Icon mode: use Nerd Font icons or ASCII fallbacks
 */
static int use_nerd_font = 1;  /* Default to Nerd Font */

/*
 * Nerd Font Icons (requires patched font)
 *
 * These use the private use area of Unicode where Nerd Fonts
 * patches in additional glyphs.
 */

/* Terminal/Shell icons */
#define NF_TERMINAL     "\357\200\244"   /* nf-fa-terminal  (U+F0124) */
#define NF_SHELL        "\357\204\274"   /* nf-fa-code      (U+F013C) */
#define NF_PROMPT       "\357\220\201"   /* nf-mdi-chevron_right */

/* Code/Language icons */
#define NF_KEYWORD      "\357\200\207"   /* nf-fa-asterisk (U+F0007) */
#define NF_STRUCT       "\357\204\213"   /* nf-fa-sitemap   (U+F010B) */
#define NF_FUNCTION     "\357\206\225"   /* nf-fa-cube      (U+F0195) */
#define NF_VARIABLE     "\357\213\200"   /* nf-fa-tag       (U+F02C0) */

/* Action icons */
#define NF_PLAY         "\357\200\213"   /* nf-fa-play      (U+F000B) */
#define NF_PAUSE        "\357\200\214"   /* nf-fa-pause     (U+F000C) */
#define NF_STOP         "\357\200\215"   /* nf-fa-stop      (U+F000D) */
#define NF_CHECK        "\357\200\214"   /* nf-fa-check     (U+F000C) */
#define NF_TIMES        "\357\200\215"   /* nf-fa-times     (U+F000D) */
#define NF_COG          "\357\200\223"   /* nf-fa-cog       (U+F0013) */

/* File icons */
#define NF_FOLDER       "\357\200\266"   /* nf-fa-folder    (U+F0036) */
#define NF_FOLDER_OPEN  "\357\200\267"   /* nf-fa-folder_open (U+F0037) */
#define NF_FILE         "\357\200\265"   /* nf-fa-file      (U+F0035) */
#define NF_FILE_CODE    "\357\203\203"   /* nf-fa-file_code (U+F00C3) */

/* Git icons */
#define NF_GIT          "\357\204\226"   /* nf-dev-git      (U+F0116) */
#define NF_BRANCH       "\357\204\231"   /* nf-dev-git_branch (U+F0119) */
#define NF_COMMIT       "\357\204\227"   /* nf-dev-git_commit (U+F0117) */

/* Arrow/Flow icons */
#define NF_ARROW_RIGHT  "\357\200\241"   /* nf-fa-arrow_right (U+F0021) */
#define NF_ARROW_DOWN   "\357\200\243"   /* nf-fa-arrow_down  (U+F0023) */
#define NF_PIPE         "\357\211\264"   /* nf-oct-workflow   (U+F0274) */

/* Status icons */
#define NF_INFO         "\357\200\251"   /* nf-fa-info_circle (U+F0029) */
#define NF_WARNING      "\357\200\252"   /* nf-fa-exclamation_triangle */
#define NF_ERROR        "\357\200\207"   /* nf-fa-times_circle */
#define NF_SUCCESS      "\357\200\214"   /* nf-fa-check_circle */

/* Decorative */
#define NF_DIAMOND      "\357\203\246"   /* nf-fa-diamond    (U+F00E6) */
#define NF_STAR         "\357\200\211"   /* nf-fa-star       (U+F0009) */
#define NF_HEART        "\357\200\204"   /* nf-fa-heart      (U+F0004) */

/*
 * ASCII Fallback Icons (for terminals without Nerd Fonts)
 */
#define ASCII_TERMINAL  ">"
#define ASCII_SHELL     "$"
#define ASCII_PROMPT    ">"
#define ASCII_KEYWORD   "#"
#define ASCII_STRUCT    "{"
#define ASCII_FUNCTION  "f"
#define ASCII_VARIABLE  "x"
#define ASCII_PLAY      ">"
#define ASCII_PAUSE     "||"
#define ASCII_STOP      "[]"
#define ASCII_CHECK     "+"
#define ASCII_TIMES     "x"
#define ASCII_COG       "*"
#define ASCII_FOLDER    "D"
#define ASCII_FOLDER_OPEN "[D]"
#define ASCII_FILE      "-"
#define ASCII_FILE_CODE "<>"
#define ASCII_GIT       "G"
#define ASCII_BRANCH    "Y"
#define ASCII_COMMIT    "o"
#define ASCII_ARROW_R   "->"
#define ASCII_ARROW_D   "v"
#define ASCII_PIPE      "|"
#define ASCII_INFO      "(i)"
#define ASCII_WARNING   "(!)"
#define ASCII_ERROR     "(x)"
#define ASCII_SUCCESS   "(+)"
#define ASCII_DIAMOND   "<>"
#define ASCII_STAR      "*"
#define ASCII_HEART     "<3"

/*
 * Icon lookup structure
 */
typedef struct {
    const char *name;
    const char *nerd_font;
    const char *ascii;
} IconDef;

static const IconDef ICONS[] = {
    {"terminal",    NF_TERMINAL,    ASCII_TERMINAL},
    {"shell",       NF_SHELL,       ASCII_SHELL},
    {"prompt",      NF_PROMPT,      ASCII_PROMPT},
    {"keyword",     NF_KEYWORD,     ASCII_KEYWORD},
    {"struct",      NF_STRUCT,      ASCII_STRUCT},
    {"function",    NF_FUNCTION,    ASCII_FUNCTION},
    {"variable",    NF_VARIABLE,    ASCII_VARIABLE},
    {"play",        NF_PLAY,        ASCII_PLAY},
    {"pause",       NF_PAUSE,       ASCII_PAUSE},
    {"stop",        NF_STOP,        ASCII_STOP},
    {"check",       NF_CHECK,       ASCII_CHECK},
    {"times",       NF_TIMES,       ASCII_TIMES},
    {"cog",         NF_COG,         ASCII_COG},
    {"folder",      NF_FOLDER,      ASCII_FOLDER},
    {"folder_open", NF_FOLDER_OPEN, ASCII_FOLDER_OPEN},
    {"file",        NF_FILE,        ASCII_FILE},
    {"file_code",   NF_FILE_CODE,   ASCII_FILE_CODE},
    {"git",         NF_GIT,         ASCII_GIT},
    {"branch",      NF_BRANCH,      ASCII_BRANCH},
    {"commit",      NF_COMMIT,      ASCII_COMMIT},
    {"arrow_right", NF_ARROW_RIGHT, ASCII_ARROW_R},
    {"arrow_down",  NF_ARROW_DOWN,  ASCII_ARROW_D},
    {"pipe",        NF_PIPE,        ASCII_PIPE},
    {"info",        NF_INFO,        ASCII_INFO},
    {"warning",     NF_WARNING,     ASCII_WARNING},
    {"error",       NF_ERROR,       ASCII_ERROR},
    {"success",     NF_SUCCESS,     ASCII_SUCCESS},
    {"diamond",     NF_DIAMOND,     ASCII_DIAMOND},
    {"star",        NF_STAR,        ASCII_STAR},
    {"heart",       NF_HEART,       ASCII_HEART},
    {NULL,          NULL,           NULL}  /* Sentinel */
};

/*
 * Enable or disable Nerd Font icons
 */
void icons_set_nerd_font(int enabled) {
    use_nerd_font = enabled;
}

/*
 * Check if Nerd Font mode is enabled
 */
int icons_nerd_font_enabled(void) {
    return use_nerd_font;
}

/*
 * Get icon by name
 * Returns the appropriate icon (Nerd Font or ASCII) based on mode
 */
const char *icon_get(const char *name) {
    for (int i = 0; ICONS[i].name != NULL; i++) {
        if (strcmp(ICONS[i].name, name) == 0) {
            return use_nerd_font ? ICONS[i].nerd_font : ICONS[i].ascii;
        }
    }
    return "?";  /* Unknown icon */
}

/*
 * Get terminal icon
 */
const char *icon_terminal(void) {
    return use_nerd_font ? NF_TERMINAL : ASCII_TERMINAL;
}

/*
 * Get keyword icon
 */
const char *icon_keyword(void) {
    return use_nerd_font ? NF_KEYWORD : ASCII_KEYWORD;
}

/*
 * Get structure icon
 */
const char *icon_struct(void) {
    return use_nerd_font ? NF_STRUCT : ASCII_STRUCT;
}

/*
 * Get play icon
 */
const char *icon_play(void) {
    return use_nerd_font ? NF_PLAY : ASCII_PLAY;
}

/*
 * Get check icon
 */
const char *icon_check(void) {
    return use_nerd_font ? NF_CHECK : ASCII_CHECK;
}

/*
 * Get cog/gear icon
 */
const char *icon_cog(void) {
    return use_nerd_font ? NF_COG : ASCII_COG;
}

/*
 * Get folder icon
 */
const char *icon_folder(void) {
    return use_nerd_font ? NF_FOLDER : ASCII_FOLDER;
}

/*
 * Get file icon
 */
const char *icon_file(void) {
    return use_nerd_font ? NF_FILE : ASCII_FILE;
}

/*
 * Get git icon
 */
const char *icon_git(void) {
    return use_nerd_font ? NF_GIT : ASCII_GIT;
}

/*
 * Get arrow right icon
 */
const char *icon_arrow_right(void) {
    return use_nerd_font ? NF_ARROW_RIGHT : ASCII_ARROW_R;
}

/*
 * Get pipe/workflow icon
 */
const char *icon_pipe(void) {
    return use_nerd_font ? NF_PIPE : ASCII_PIPE;
}

/*
 * Get error icon
 */
const char *icon_error(void) {
    return use_nerd_font ? NF_ERROR : ASCII_ERROR;
}

/*
 * Get success icon
 */
const char *icon_success(void) {
    return use_nerd_font ? NF_SUCCESS : ASCII_SUCCESS;
}

/*
 * Get diamond icon
 */
const char *icon_diamond(void) {
    return use_nerd_font ? NF_DIAMOND : ASCII_DIAMOND;
}

/*
 * Print an icon with color
 */
void icon_print(const char *name, int color) {
    const char *icon = icon_get(name);
    if (color >= 0) {
        printf(CSI "38;5;%dm%s" COL_RESET, color, icon);
    } else {
        printf("%s", icon);
    }
}

/*
 * Print an icon with a label
 */
void icon_print_label(const char *name, const char *label, int icon_color, int label_color) {
    const char *icon = icon_get(name);
    printf(CSI "38;5;%dm%s" COL_RESET " ", icon_color, icon);
    printf(CSI "38;5;%dm%s" COL_RESET, label_color, label);
}
