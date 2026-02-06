/*
 * shelli - Educational Shell
 * tui/tui_input.c - Line editor with history and key handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "tui.h"

/*
 * Key codes
 */
typedef enum {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_TAB,
    KEY_ESCAPE,
    KEY_CTRL_C,
    KEY_CTRL_D,
    KEY_CTRL_L,
    KEY_CTRL_A,
    KEY_CTRL_E,
    KEY_CTRL_K,
    KEY_CTRL_U,
    KEY_CTRL_W,
} KeyCode;

/*
 * Key event structure
 */
typedef struct {
    KeyCode code;
    char ch;           /* For KEY_CHAR */
} KeyEvent;

/*
 * Line editor state
 */
#define LINE_BUFFER_SIZE 4096
#define HISTORY_SIZE 100

typedef struct {
    char buf[LINE_BUFFER_SIZE];
    int len;
    int cursor;

    /* History */
    char *history[HISTORY_SIZE];
    int hist_count;
    int hist_pos;

    /* Saved line while navigating history */
    char saved_line[LINE_BUFFER_SIZE];
    int saved_len;
} LineEditor;

static LineEditor editor = {0};

/* External functions from tui_render.c */
void render_input_line(const char *line, int cursor_pos);

/*
 * Read a raw byte with timeout
 */
static int read_byte(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return (unsigned char)c;
    }
    return -1;
}

/*
 * Read a key event
 */
static KeyEvent read_key(void) {
    KeyEvent evt = {0};

    int c = read_byte();
    if (c < 0) {
        evt.code = KEY_NONE;
        return evt;
    }

    /* Control characters */
    if (c == '\r' || c == '\n') {
        evt.code = KEY_ENTER;
        return evt;
    }

    if (c == 127 || c == 8) {
        evt.code = KEY_BACKSPACE;
        return evt;
    }

    if (c == '\t') {
        evt.code = KEY_TAB;
        return evt;
    }

    if (c == 3) {  /* Ctrl+C */
        evt.code = KEY_CTRL_C;
        return evt;
    }

    if (c == 4) {  /* Ctrl+D */
        evt.code = KEY_CTRL_D;
        return evt;
    }

    if (c == 12) {  /* Ctrl+L */
        evt.code = KEY_CTRL_L;
        return evt;
    }

    if (c == 1) {  /* Ctrl+A */
        evt.code = KEY_CTRL_A;
        return evt;
    }

    if (c == 5) {  /* Ctrl+E */
        evt.code = KEY_CTRL_E;
        return evt;
    }

    if (c == 11) {  /* Ctrl+K */
        evt.code = KEY_CTRL_K;
        return evt;
    }

    if (c == 21) {  /* Ctrl+U */
        evt.code = KEY_CTRL_U;
        return evt;
    }

    if (c == 23) {  /* Ctrl+W */
        evt.code = KEY_CTRL_W;
        return evt;
    }

    /* Escape sequences */
    if (c == 27) {
        int c2 = read_byte();
        if (c2 < 0) {
            evt.code = KEY_ESCAPE;
            return evt;
        }

        if (c2 == '[') {
            int c3 = read_byte();
            if (c3 < 0) {
                evt.code = KEY_ESCAPE;
                return evt;
            }

            switch (c3) {
                case 'A': evt.code = KEY_UP; return evt;
                case 'B': evt.code = KEY_DOWN; return evt;
                case 'C': evt.code = KEY_RIGHT; return evt;
                case 'D': evt.code = KEY_LEFT; return evt;
                case 'H': evt.code = KEY_HOME; return evt;
                case 'F': evt.code = KEY_END; return evt;
                case '1':
                case '7': {
                    int c4 = read_byte();
                    if (c4 == '~') {
                        evt.code = KEY_HOME;
                        return evt;
                    }
                    break;
                }
                case '4':
                case '8': {
                    int c4 = read_byte();
                    if (c4 == '~') {
                        evt.code = KEY_END;
                        return evt;
                    }
                    break;
                }
                case '3': {
                    int c4 = read_byte();
                    if (c4 == '~') {
                        evt.code = KEY_DELETE;
                        return evt;
                    }
                    break;
                }
            }
        }

        evt.code = KEY_ESCAPE;
        return evt;
    }

    /* Regular printable character */
    if (c >= 32 && c < 127) {
        evt.code = KEY_CHAR;
        evt.ch = (char)c;
        return evt;
    }

    /* UTF-8 multibyte character - treat as char */
    if ((c & 0xC0) == 0xC0) {
        evt.code = KEY_CHAR;
        evt.ch = (char)c;
        return evt;
    }

    return evt;
}

/*
 * Insert character at cursor position
 */
static void editor_insert(char c) {
    if (editor.len >= LINE_BUFFER_SIZE - 1) return;

    /* Shift characters after cursor */
    memmove(&editor.buf[editor.cursor + 1],
            &editor.buf[editor.cursor],
            editor.len - editor.cursor);

    editor.buf[editor.cursor] = c;
    editor.cursor++;
    editor.len++;
    editor.buf[editor.len] = '\0';
}

/*
 * Delete character before cursor (backspace)
 */
static void editor_backspace(void) {
    if (editor.cursor == 0) return;

    memmove(&editor.buf[editor.cursor - 1],
            &editor.buf[editor.cursor],
            editor.len - editor.cursor);

    editor.cursor--;
    editor.len--;
    editor.buf[editor.len] = '\0';
}

/*
 * Delete character at cursor (delete key)
 */
static void editor_delete(void) {
    if (editor.cursor >= editor.len) return;

    memmove(&editor.buf[editor.cursor],
            &editor.buf[editor.cursor + 1],
            editor.len - editor.cursor - 1);

    editor.len--;
    editor.buf[editor.len] = '\0';
}

/*
 * Move cursor
 */
static void editor_move(int delta) {
    int new_pos = editor.cursor + delta;
    if (new_pos < 0) new_pos = 0;
    if (new_pos > editor.len) new_pos = editor.len;
    editor.cursor = new_pos;
}

/*
 * Move to start of line
 */
static void editor_home(void) {
    editor.cursor = 0;
}

/*
 * Move to end of line
 */
static void editor_end(void) {
    editor.cursor = editor.len;
}

/*
 * Delete from cursor to end of line
 */
static void editor_kill_to_end(void) {
    editor.buf[editor.cursor] = '\0';
    editor.len = editor.cursor;
}

/*
 * Delete from start to cursor
 */
static void editor_kill_to_start(void) {
    memmove(editor.buf, &editor.buf[editor.cursor], editor.len - editor.cursor);
    editor.len -= editor.cursor;
    editor.cursor = 0;
    editor.buf[editor.len] = '\0';
}

/*
 * Delete previous word
 */
static void editor_kill_word(void) {
    if (editor.cursor == 0) return;

    int end = editor.cursor;

    /* Skip trailing spaces */
    while (editor.cursor > 0 && editor.buf[editor.cursor - 1] == ' ') {
        editor.cursor--;
    }

    /* Skip word */
    while (editor.cursor > 0 && editor.buf[editor.cursor - 1] != ' ') {
        editor.cursor--;
    }

    /* Delete from cursor to end */
    int deleted = end - editor.cursor;
    memmove(&editor.buf[editor.cursor], &editor.buf[end], editor.len - end);
    editor.len -= deleted;
    editor.buf[editor.len] = '\0';
}

/*
 * Set editor content from string
 */
static void editor_set(const char *s) {
    strncpy(editor.buf, s, LINE_BUFFER_SIZE - 1);
    editor.buf[LINE_BUFFER_SIZE - 1] = '\0';
    editor.len = (int)strlen(editor.buf);
    editor.cursor = editor.len;
}

/*
 * Clear editor
 */
static void editor_clear(void) {
    editor.buf[0] = '\0';
    editor.len = 0;
    editor.cursor = 0;
}

/*
 * Add current line to history
 */
static void history_add(const char *line) {
    if (!line || !line[0]) return;

    /* Don't add duplicate of last entry */
    if (editor.hist_count > 0) {
        if (strcmp(editor.history[editor.hist_count - 1], line) == 0) {
            return;
        }
    }

    /* Free oldest if at capacity */
    if (editor.hist_count >= HISTORY_SIZE) {
        free(editor.history[0]);
        memmove(editor.history, &editor.history[1],
                (HISTORY_SIZE - 1) * sizeof(char *));
        editor.hist_count--;
    }

    editor.history[editor.hist_count] = strdup(line);
    editor.hist_count++;
}

/*
 * Navigate history up
 */
static void history_up(void) {
    if (editor.hist_count == 0) return;

    /* Save current line if at bottom */
    if (editor.hist_pos == editor.hist_count) {
        strncpy(editor.saved_line, editor.buf, LINE_BUFFER_SIZE);
        editor.saved_len = editor.len;
    }

    if (editor.hist_pos > 0) {
        editor.hist_pos--;
        editor_set(editor.history[editor.hist_pos]);
    }
}

/*
 * Navigate history down
 */
static void history_down(void) {
    if (editor.hist_pos >= editor.hist_count) return;

    editor.hist_pos++;

    if (editor.hist_pos == editor.hist_count) {
        /* Restore saved line */
        editor_set(editor.saved_line);
    } else {
        editor_set(editor.history[editor.hist_pos]);
    }
}

/*
 * Read a line of input with editing support
 */
char *tui_read_line(void) {
    /* Reset editor state */
    editor_clear();
    editor.hist_pos = editor.hist_count;

    /* Show cursor */
    printf(CUR_SHOW);
    fflush(stdout);

    /* Initial render */
    render_input_line(editor.buf, editor.cursor);

    while (1) {
        KeyEvent key = read_key();

        switch (key.code) {
            case KEY_NONE:
                /* No input, continue waiting */
                continue;

            case KEY_ENTER:
                /* Add to history and return */
                if (editor.len > 0) {
                    history_add(editor.buf);
                }
                printf(CUR_HIDE);
                fflush(stdout);
                return strdup(editor.buf);

            case KEY_CTRL_D:
                if (editor.len == 0) {
                    /* EOF on empty line */
                    printf(CUR_HIDE);
                    fflush(stdout);
                    return NULL;
                }
                break;

            case KEY_CTRL_C:
                /* Clear line */
                editor_clear();
                break;

            case KEY_CHAR:
                editor_insert(key.ch);
                break;

            case KEY_BACKSPACE:
                editor_backspace();
                break;

            case KEY_DELETE:
                editor_delete();
                break;

            case KEY_LEFT:
                editor_move(-1);
                break;

            case KEY_RIGHT:
                editor_move(1);
                break;

            case KEY_UP:
                history_up();
                break;

            case KEY_DOWN:
                history_down();
                break;

            case KEY_HOME:
            case KEY_CTRL_A:
                editor_home();
                break;

            case KEY_END:
            case KEY_CTRL_E:
                editor_end();
                break;

            case KEY_CTRL_K:
                editor_kill_to_end();
                break;

            case KEY_CTRL_U:
                editor_kill_to_start();
                break;

            case KEY_CTRL_W:
                editor_kill_word();
                break;

            case KEY_CTRL_L:
                /* Redraw screen */
                tui_draw_frame();
                break;

            case KEY_TAB:
            case KEY_ESCAPE:
                /* Ignore for now */
                break;
        }

        /* Re-render input line */
        render_input_line(editor.buf, editor.cursor);
    }
}
