/*
 * shelli - Educational Shell
 * tui/tui_input.c - Line editor with history, key handling, syntax highlighting
 *
 * Full-screen input page: centered input box, syntax highlighting, mascot.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../builtins.h"
#include "../lexer.h"
#include "tui.h"

/* Box drawing */
#define BOX_TL "\342\225\255"  /* ╭ */
#define BOX_TR "\342\225\256"  /* ╮ */
#define BOX_BL "\342\225\260"  /* ╰ */
#define BOX_BR "\342\225\257"  /* ╯ */
#define BOX_H  "\342\224\200"  /* ─ */
#define BOX_V  "\342\224\202"  /* │ */

/* Prompt character */
#define PROMPT_CHAR "\342\235\257"  /* ❯ */

/*
 * Line editor state
 */
#define LINE_BUFFER_SIZE 4096
#define HISTORY_SIZE 100

typedef struct {
    char buf[LINE_BUFFER_SIZE];
    int len;
    int cursor;

    char *history[HISTORY_SIZE];
    int hist_count;
    int hist_pos;

    char saved_line[LINE_BUFFER_SIZE];
    int saved_len;
} LineEditor;

static LineEditor editor = {0};

/* Suggestion / "Try These" state */
static SuggestState suggest = {0};
static TryTheseState try_these = {.selected = 0, .visible = 1, .scroll_offset = 0};

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
 * Read a key event (exported as tui_read_key)
 */
KeyEvent tui_read_key(void) {
    KeyEvent evt = {0};

    int c = read_byte();
    if (c < 0) {
        evt.code = KEY_NONE;
        return evt;
    }

    if (c == '\r' || c == '\n') { evt.code = KEY_ENTER; return evt; }
    if (c == 127 || c == 8) { evt.code = KEY_BACKSPACE; return evt; }
    if (c == '\t') { evt.code = KEY_TAB; return evt; }
    if (c == 3) { evt.code = KEY_CTRL_C; return evt; }
    if (c == 4) { evt.code = KEY_CTRL_D; return evt; }
    if (c == 12) { evt.code = KEY_CTRL_L; return evt; }
    if (c == 1) { evt.code = KEY_CTRL_A; return evt; }
    if (c == 5) { evt.code = KEY_CTRL_E; return evt; }
    if (c == 11) { evt.code = KEY_CTRL_K; return evt; }
    if (c == 21) { evt.code = KEY_CTRL_U; return evt; }
    if (c == 23) { evt.code = KEY_CTRL_W; return evt; }

    if (c == 27) {
        int c2 = read_byte();
        if (c2 < 0) { evt.code = KEY_ESCAPE; return evt; }

        if (c2 == '[') {
            int c3 = read_byte();
            if (c3 < 0) { evt.code = KEY_ESCAPE; return evt; }

            switch (c3) {
                case 'A': evt.code = KEY_UP; return evt;
                case 'B': evt.code = KEY_DOWN; return evt;
                case 'C': evt.code = KEY_RIGHT; return evt;
                case 'D': evt.code = KEY_LEFT; return evt;
                case 'H': evt.code = KEY_HOME; return evt;
                case 'F': evt.code = KEY_END; return evt;
                case 'Z': evt.code = KEY_SHIFT_TAB; return evt;
                case '1': case '7': {
                    int c4 = read_byte();
                    if (c4 == '~') { evt.code = KEY_HOME; return evt; }
                    break;
                }
                case '4': case '8': {
                    int c4 = read_byte();
                    if (c4 == '~') { evt.code = KEY_END; return evt; }
                    break;
                }
                case '3': {
                    int c4 = read_byte();
                    if (c4 == '~') { evt.code = KEY_DELETE; return evt; }
                    break;
                }
            }
        }

        evt.code = KEY_ESCAPE;
        return evt;
    }

    if (c >= 32 && c < 127) {
        evt.code = KEY_CHAR;
        evt.ch = (char)c;
        return evt;
    }

    if ((c & 0xC0) == 0xC0) {
        evt.code = KEY_CHAR;
        evt.ch = (char)c;
        return evt;
    }

    return evt;
}

/*
 * Editor operations
 */
static void editor_insert(char c) {
    if (editor.len >= LINE_BUFFER_SIZE - 1) return;
    memmove(&editor.buf[editor.cursor + 1],
            &editor.buf[editor.cursor],
            editor.len - editor.cursor);
    editor.buf[editor.cursor] = c;
    editor.cursor++;
    editor.len++;
    editor.buf[editor.len] = '\0';
}

static void editor_backspace(void) {
    if (editor.cursor == 0) return;
    memmove(&editor.buf[editor.cursor - 1],
            &editor.buf[editor.cursor],
            editor.len - editor.cursor);
    editor.cursor--;
    editor.len--;
    editor.buf[editor.len] = '\0';
}

static void editor_delete(void) {
    if (editor.cursor >= editor.len) return;
    memmove(&editor.buf[editor.cursor],
            &editor.buf[editor.cursor + 1],
            editor.len - editor.cursor - 1);
    editor.len--;
    editor.buf[editor.len] = '\0';
}

static void editor_move(int delta) {
    int new_pos = editor.cursor + delta;
    if (new_pos < 0) new_pos = 0;
    if (new_pos > editor.len) new_pos = editor.len;
    editor.cursor = new_pos;
}

static void editor_home(void) { editor.cursor = 0; }
static void editor_end(void) { editor.cursor = editor.len; }

static void editor_kill_to_end(void) {
    editor.buf[editor.cursor] = '\0';
    editor.len = editor.cursor;
}

static void editor_kill_to_start(void) {
    memmove(editor.buf, &editor.buf[editor.cursor], editor.len - editor.cursor);
    editor.len -= editor.cursor;
    editor.cursor = 0;
    editor.buf[editor.len] = '\0';
}

static void editor_kill_word(void) {
    if (editor.cursor == 0) return;
    int end = editor.cursor;
    while (editor.cursor > 0 && editor.buf[editor.cursor - 1] == ' ')
        editor.cursor--;
    while (editor.cursor > 0 && editor.buf[editor.cursor - 1] != ' ')
        editor.cursor--;
    int deleted = end - editor.cursor;
    memmove(&editor.buf[editor.cursor], &editor.buf[end], editor.len - end);
    editor.len -= deleted;
    editor.buf[editor.len] = '\0';
}

static void editor_set(const char *s) {
    strncpy(editor.buf, s, LINE_BUFFER_SIZE - 1);
    editor.buf[LINE_BUFFER_SIZE - 1] = '\0';
    editor.len = (int)strlen(editor.buf);
    editor.cursor = editor.len;
}

static void editor_clear(void) {
    editor.buf[0] = '\0';
    editor.len = 0;
    editor.cursor = 0;
}

/* Replace entire editor buffer with given text */
static void editor_replace_all(const char *text) {
    editor_set(text);
}

/*
 * History
 */
static void history_add(const char *line) {
    if (!line || !line[0]) return;
    if (editor.hist_count > 0 &&
        strcmp(editor.history[editor.hist_count - 1], line) == 0)
        return;
    if (editor.hist_count >= HISTORY_SIZE) {
        free(editor.history[0]);
        memmove(editor.history, &editor.history[1],
                (HISTORY_SIZE - 1) * sizeof(char *));
        editor.hist_count--;
    }
    editor.history[editor.hist_count] = strdup(line);
    editor.hist_count++;
}

void tui_history_load(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.shelli_history", home);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len > 0) history_add(line);
    }
    fclose(f);
}

void tui_history_save(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.shelli_history", home);
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < editor.hist_count; i++)
        fprintf(f, "%s\n", editor.history[i]);
    fclose(f);
}

const char *tui_history_get(int n) {
    if (n < 1 || n > editor.hist_count) return NULL;
    return editor.history[n - 1];
}

int tui_history_count(void) { return editor.hist_count; }

void tui_history_print(void) {
    for (int i = 0; i < editor.hist_count; i++)
        printf("%4d  %s\n", i + 1, editor.history[i]);
}

static void history_up(void) {
    if (editor.hist_count == 0) return;
    if (editor.hist_pos == editor.hist_count) {
        strncpy(editor.saved_line, editor.buf, LINE_BUFFER_SIZE);
        editor.saved_len = editor.len;
    }
    if (editor.hist_pos > 0) {
        editor.hist_pos--;
        editor_set(editor.history[editor.hist_pos]);
    }
}

static void history_down(void) {
    if (editor.hist_pos >= editor.hist_count) return;
    editor.hist_pos++;
    if (editor.hist_pos == editor.hist_count) {
        editor_set(editor.saved_line);
    } else {
        editor_set(editor.history[editor.hist_pos]);
    }
}

/*
 * Syntax highlighting
 */
static void highlight_input(const char *buf, int len, int *colors) {
    for (int i = 0; i < len; i++)
        colors[i] = COL_TEXT;
    if (len == 0) return;

    TokenList tokens;
    char error_buf[256] = "";
    int result = lexer_tokenize(buf, &tokens, error_buf, sizeof(error_buf));

    if (result < 0) {
        for (int i = 0; i < len; i++) colors[i] = COL_RED;
        return;
    }

    int pos = 0;
    for (int t = 0; t < tokens.count; t++) {
        Token *tok = &tokens.tokens[t];

        while (pos < len && (buf[pos] == ' ' || buf[pos] == '\t')) {
            colors[pos] = COL_OVERLAY;
            pos++;
        }
        if (pos >= len) break;

        int color;
        if (tok->type == TOK_WORD && tok->value && is_shell_keyword(tok->value)) {
            color = COL_PINK;
        } else {
            color = token_type_color(tok->type);
        }

        if (tok->value) {
            if (pos < len && (buf[pos] == '\'' || buf[pos] == '"')) {
                char quote = buf[pos];
                colors[pos] = COL_GREEN;
                pos++;
                while (pos < len && buf[pos] != quote) {
                    if (buf[pos] == '\\' && quote == '"' && pos + 1 < len) {
                        colors[pos] = COL_PEACH;
                        pos++;
                    }
                    colors[pos] = COL_GREEN;
                    pos++;
                }
                if (pos < len) { colors[pos] = COL_GREEN; pos++; }
            } else {
                int tok_val_len = (int)strlen(tok->value);
                for (int i = 0; i < tok_val_len && pos < len; i++) {
                    colors[pos] = color;
                    pos++;
                }
            }
        } else {
            int op_len = 0;
            switch (tok->type) {
            case TOK_PIPE:      op_len = 1; break;
            case TOK_SEMI:      op_len = 1; break;
            case TOK_AND:       op_len = 2; break;
            case TOK_OR:        op_len = 2; break;
            case TOK_BG:        op_len = 1; break;
            case TOK_REDIR_IN:  op_len = 1; break;
            case TOK_REDIR_OUT: op_len = 1; break;
            case TOK_REDIR_APP: op_len = 2; break;
            case TOK_HEREDOC:   op_len = 2; break;
            case TOK_EOF:       op_len = 0; break;
            default:            op_len = 0; break;
            }
            for (int i = 0; i < op_len && pos < len; i++) {
                colors[pos] = color;
                pos++;
            }
        }
    }
    tokenlist_free(&tokens);
}

/*
 * Tab completion
 */
#define MAX_COMPLETIONS 256
#define MAX_COMPLETION_LEN 256

static int find_word_start(const char *buf, int cursor) {
    int pos = cursor;
    while (pos > 0 && buf[pos - 1] != ' ') pos--;
    return pos;
}

static int is_command_position(const char *buf, int word_start) {
    int i = word_start - 1;
    while (i >= 0 && buf[i] == ' ') i--;
    if (i < 0) return 1;
    if (buf[i] == '|' || buf[i] == ';') return 1;
    if (i > 0 && buf[i] == '&' && buf[i-1] == '&') return 1;
    return 0;
}

static int find_command_completions(const char *prefix,
                                    char completions[][MAX_COMPLETION_LEN],
                                    int max) {
    int count = 0;
    int prefix_len = (int)strlen(prefix);

    const char *builtin_names[] = {
        "cd", "pwd", "exit", "help", "export", "unset", "history",
        "test", "echo", "type", "true", "false",
        "local", "return", "source",
        "jobs", "fg", "bg", NULL
    };
    for (int i = 0; builtin_names[i] && count < max; i++) {
        if (strncmp(builtin_names[i], prefix, prefix_len) == 0) {
            strncpy(completions[count], builtin_names[i], MAX_COMPLETION_LEN - 1);
            completions[count][MAX_COMPLETION_LEN - 1] = '\0';
            count++;
        }
    }

    const char *keywords[] = {
        "if", "then", "elif", "else", "fi",
        "while", "until", "do", "done",
        "for", "in", "case", "esac", NULL
    };
    for (int i = 0; keywords[i] && count < max; i++) {
        if (strncmp(keywords[i], prefix, prefix_len) == 0) {
            strncpy(completions[count], keywords[i], MAX_COMPLETION_LEN - 1);
            completions[count][MAX_COMPLETION_LEN - 1] = '\0';
            count++;
        }
    }

    const char *path_env = getenv("PATH");
    if (!path_env) return count;

    char *path_copy = strdup(path_env);
    char *dir = strtok(path_copy, ":");

    while (dir && count < max) {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL && count < max) {
                if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
                    char full_path[4096];
                    snprintf(full_path, sizeof(full_path), "%s/%s",
                             dir, entry->d_name);
                    struct stat st;
                    if (stat(full_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
                        int dup = 0;
                        for (int j = 0; j < count; j++) {
                            if (strcmp(completions[j], entry->d_name) == 0) {
                                dup = 1; break;
                            }
                        }
                        if (!dup) {
                            strncpy(completions[count], entry->d_name,
                                    MAX_COMPLETION_LEN - 1);
                            completions[count][MAX_COMPLETION_LEN - 1] = '\0';
                            count++;
                        }
                    }
                }
            }
            closedir(d);
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return count;
}

static int find_file_completions(const char *prefix,
                                 char completions[][MAX_COMPLETION_LEN],
                                 int max) {
    int count = 0;
    char dir_path[4096] = ".";
    const char *file_prefix = prefix;
    int prefix_len;

    const char *last_slash = strrchr(prefix, '/');
    if (last_slash) {
        int dlen = (int)(last_slash - prefix);
        if (dlen == 0) strcpy(dir_path, "/");
        else {
            if (dlen >= (int)sizeof(dir_path)) dlen = sizeof(dir_path) - 1;
            memcpy(dir_path, prefix, dlen);
            dir_path[dlen] = '\0';
        }
        file_prefix = last_slash + 1;
    }

    char expanded_dir[4096];
    if (dir_path[0] == '~') {
        const char *home = getenv("HOME");
        if (home) snprintf(expanded_dir, sizeof(expanded_dir), "%s%s",
                           home, dir_path + 1);
        else strncpy(expanded_dir, dir_path, sizeof(expanded_dir) - 1);
    } else {
        strncpy(expanded_dir, dir_path, sizeof(expanded_dir) - 1);
    }
    expanded_dir[sizeof(expanded_dir) - 1] = '\0';

    prefix_len = (int)strlen(file_prefix);

    DIR *d = opendir(expanded_dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < max) {
        if (entry->d_name[0] == '.' && file_prefix[0] != '.') continue;
        if (strncmp(entry->d_name, file_prefix, prefix_len) == 0) {
            char completion[MAX_COMPLETION_LEN];
            if (last_slash) {
                int dl = (int)(last_slash - prefix) + 1;
                memcpy(completion, prefix, dl);
                completion[dl] = '\0';
                strncat(completion, entry->d_name, MAX_COMPLETION_LEN - dl - 1);
            } else {
                strncpy(completion, entry->d_name, MAX_COMPLETION_LEN - 1);
                completion[MAX_COMPLETION_LEN - 1] = '\0';
            }

            char full_path[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s",
                     expanded_dir, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                int clen = (int)strlen(completion);
                if (clen < MAX_COMPLETION_LEN - 2) {
                    completion[clen] = '/';
                    completion[clen + 1] = '\0';
                }
            }

            strncpy(completions[count], completion, MAX_COMPLETION_LEN - 1);
            completions[count][MAX_COMPLETION_LEN - 1] = '\0';
            count++;
        }
    }

    closedir(d);
    return count;
}

static int common_prefix_len(char completions[][MAX_COMPLETION_LEN], int count) {
    if (count <= 0) return 0;
    if (count == 1) return (int)strlen(completions[0]);
    int len = (int)strlen(completions[0]);
    for (int i = 1; i < count; i++) {
        int j = 0;
        while (j < len && completions[i][j] == completions[0][j]) j++;
        len = j;
    }
    return len;
}

static void handle_tab(void) {
    int word_start = find_word_start(editor.buf, editor.cursor);
    int word_len = editor.cursor - word_start;

    char prefix[MAX_COMPLETION_LEN];
    if (word_len >= MAX_COMPLETION_LEN) word_len = MAX_COMPLETION_LEN - 1;
    memcpy(prefix, editor.buf + word_start, word_len);
    prefix[word_len] = '\0';

    char completions[MAX_COMPLETIONS][MAX_COMPLETION_LEN];
    int count;

    if (is_command_position(editor.buf, word_start))
        count = find_command_completions(prefix, completions, MAX_COMPLETIONS);
    else
        count = find_file_completions(prefix, completions, MAX_COMPLETIONS);

    if (count == 0) return;

    if (count == 1) {
        const char *completion = completions[0];
        int comp_len = (int)strlen(completion);
        memmove(editor.buf + word_start,
                editor.buf + editor.cursor,
                editor.len - editor.cursor + 1);
        editor.len -= word_len;
        editor.cursor = word_start;
        int add_space = (comp_len > 0 && completion[comp_len - 1] != '/') ? 1 : 0;
        if (editor.len + comp_len + add_space < LINE_BUFFER_SIZE - 1) {
            memmove(editor.buf + word_start + comp_len + add_space,
                    editor.buf + word_start,
                    editor.len - word_start + 1);
            memcpy(editor.buf + word_start, completion, comp_len);
            if (add_space) editor.buf[word_start + comp_len] = ' ';
            editor.len += comp_len + add_space;
            editor.cursor = word_start + comp_len + add_space;
            editor.buf[editor.len] = '\0';
        }
    } else {
        int cp_len = common_prefix_len(completions, count);
        if (cp_len > word_len) {
            memmove(editor.buf + word_start,
                    editor.buf + editor.cursor,
                    editor.len - editor.cursor + 1);
            editor.len -= word_len;
            editor.cursor = word_start;
            if (editor.len + cp_len < LINE_BUFFER_SIZE - 1) {
                memmove(editor.buf + word_start + cp_len,
                        editor.buf + word_start,
                        editor.len - word_start + 1);
                memcpy(editor.buf + word_start, completions[0], cp_len);
                editor.len += cp_len;
                editor.cursor = word_start + cp_len;
                editor.buf[editor.len] = '\0';
            }
        }
    }
}

/*
 * Render the full-screen input page
 */
static void render_input_page(void) {
    int tw, th;
    tui_get_size(&tw, &th);
    tui_render_ensure(th, tw);

    RenderBuf *rb, *prev;
    tui_render_get_bufs(&rb, &prev);
    rbuf_clear(rb);

    /* Outer frame with "shelli" title (no stage name) */
    page_draw_outer_frame(rb, tw, th, NULL, 0, 0);

    /* Input box positioned below the logo */
    int lx = 7;
    int bw = tw - 14;
    if (bw < 30) { lx = 2; bw = tw - 4; }

    /* Logo goes above the input box */
    int logo_h = logo_get_height();
    int box_y = th / 2 - 2;
    if (box_y < logo_h + 4) box_y = logo_h + 4;

    /* Render the ASCII art logo centered above the input box via printf.
     * The logo contains embedded ANSI escapes, so it must bypass the
     * render buffer.  Flush the frame first, then printf the logo. */
    {
        const char **logo = logo_get_lines();
        int logo_start = box_y - logo_h - 3;
        if (logo_start < 2) logo_start = 2;

        tui_render_flush();

        for (int i = 0; i < logo_h && logo[i]; i++) {
            int vlen = 0;
            const char *s = logo[i];
            int in_esc = 0;
            while (*s) {
                if (*s == '\033') in_esc = 1;
                else if (in_esc) { if (*s == 'm') in_esc = 0; }
                else if ((*s & 0xC0) != 0x80) vlen++;
                s++;
            }
            int cx = (tw - vlen) / 2;
            if (cx < 1) cx = 1;
            printf(CSI "%d;%dH%s" COL_RESET, logo_start + i + 1, cx, logo[i]);
        }
        fflush(stdout);

        /* Second pass: re-populate the render buffer so the diff-based
         * flush can correctly manage all subsequent content. */
        rbuf_clear(rb);
        page_draw_outer_frame(rb, tw, th, NULL, 0, 0);
    }

    /* Draw input box */
    page_draw_box(rb, lx, box_y, bw, 5, NULL, COL_LAVENDER);

    /* Prompt */
    rbuf_put_str(rb, box_y + 2, lx + 3, PROMPT_CHAR, COL_NEON_CYAN, COL_BASE, 0, 0);
    rbuf_put_str(rb, box_y + 2, lx + 4, " ", -1, COL_BASE, 0, 0);

    /* Input text with syntax highlighting */
    int input_col = lx + 5;
    int max_input = bw - 8;

    if (editor.len > 0) {
        int colors[LINE_BUFFER_SIZE];
        highlight_input(editor.buf, editor.len, colors);

        /* Determine visible window */
        int view_start = 0;
        if (editor.cursor > max_input - 2) {
            view_start = editor.cursor - max_input + 2;
        }

        for (int i = view_start; i < editor.len && (i - view_start) < max_input; i++) {
            char ch[2] = {editor.buf[i], '\0'};
            rbuf_put_char(rb, box_y + 2, input_col + (i - view_start),
                         ch, colors[i], COL_BASE, 0, 0);
        }

        /* Show cursor position */
        int cursor_screen = input_col + (editor.cursor - view_start);
        if (cursor_screen < input_col + max_input) {
            if (editor.cursor < editor.len) {
                char ch[2] = {editor.buf[editor.cursor], '\0'};
                rbuf_put_char(rb, box_y + 2, cursor_screen,
                             ch, COL_BASE, COL_NEON_CYAN, 1, 0);
            } else {
                rbuf_put_char(rb, box_y + 2, cursor_screen,
                             " ", COL_BASE, COL_NEON_CYAN, 0, 0);
            }
        }
    } else {
        /* Empty - just show cursor block */
        rbuf_put_char(rb, box_y + 2, input_col,
                     " ", COL_BASE, COL_NEON_CYAN, 0, 0);
    }

    /* Below-input area: "Try These", suggestions, or fallback help */
    int panel_y = box_y + 6;
    int panel_max_h = th - panel_y - 3;
    if (panel_max_h < 4) panel_max_h = 4;

    if (editor.len == 0 && try_these.visible) {
        /* "Try These" panel when input is empty */
        suggest_render_try_these(rb, &try_these, lx, panel_y, bw, panel_max_h);

        /* Hint bar for "Try These" mode */
        const char *keys[] = {"[Enter]", "[" "\342\206\221\342\206\223" "]",
                              "[Tab]", "[Ctrl+D]"};
        const char *labels[] = {"try it", "browse", "insert", "quit"};
        page_draw_hints_bar(rb, tw, th, keys, labels, 4);
    } else if (editor.len > 0 && suggest.visible && suggest.count > 0) {
        /* Suggestion dropdown when typing */
        suggest_render_dropdown(rb, &suggest, lx, panel_y, bw);

        /* Hint bar for suggestions mode */
        const char *keys[] = {"[Enter]", "[" "\342\206\221\342\206\223" "]",
                              "[Tab]", "[Esc]"};
        const char *labels[] = {"run", "select", "accept", "dismiss"};
        page_draw_hints_bar(rb, tw, th, keys, labels, 4);
    } else {
        /* Default help text + mascot */
        int help_y = panel_y;
        const char *help1 = "Type a command and press Enter to see what happens.";
        const char *help2 = "Use up/down for history. Tab for completion.";
        int h1x = (tw - (int)strlen(help1)) / 2;
        int h2x = (tw - (int)strlen(help2)) / 2;
        if (h1x < 4) h1x = 4;
        if (h2x < 4) h2x = 4;
        rbuf_put_str(rb, help_y, h1x, help1, COL_OVERLAY, COL_BASE, 0, 0);
        rbuf_put_str(rb, help_y + 1, h2x, help2, COL_OVERLAY, COL_BASE, 0, 0);

        /* Mascot */
        int mascot_x = tw / 2 - 4;
        int mascot_y = help_y + 3;
        if (mascot_y + 6 < th - 2) {
            static int input_mascot_frame = 0;
            mascot_render(rb, mascot_x, mascot_y, MOOD_NORMAL, input_mascot_frame);
            input_mascot_frame = (input_mascot_frame + 1) % 2;
        }

        /* Hint bar */
        const char *keys[] = {"[Enter]", "[" "\342\206\221\342\206\223" "]",
                              "[Tab]", "[Ctrl+D]"};
        const char *labels[] = {"run", "history", "complete", "quit"};
        page_draw_hints_bar(rb, tw, th, keys, labels, 4);
    }

    tui_render_flush();
}

/*
 * Clear the printf'd logo artifacts before leaving the input page.
 * The logo bypasses the render buffer, so we must clear the terminal
 * and invalidate the prev buffer to force a full redraw on the next page.
 */
static void input_page_cleanup(void) {
    printf(SCR_CLEAR CUR_HOME);
    fflush(stdout);
    RenderBuf *f, *b;
    tui_render_get_bufs(&f, &b);
    for (int i = 0; i < b->rows * b->cols; i++)
        b->cells[i].fg = -99;
}

/*
 * Read a line of input with full-screen editing
 */
char *tui_read_line(void) {
    editor_clear();
    editor.hist_pos = editor.hist_count;

    /* Reset suggestion state */
    suggest_dismiss(&suggest);
    try_these.selected = 0;
    try_these.visible = 1;
    try_these.scroll_offset = 0;

    printf(CUR_HIDE);
    fflush(stdout);

    render_input_page();

    while (1) {
        if (tui_resize_pending()) {
            tui_handle_resize();
            /* Force full redraw */
            RenderBuf *f, *b;
            tui_render_get_bufs(&f, &b);
            for (int i = 0; i < b->rows * b->cols; i++)
                b->cells[i].fg = -99;
            render_input_page();
        }

        KeyEvent key = tui_read_key();

        switch (key.code) {
            case KEY_NONE:
                continue;

            case KEY_ENTER:
                /* If input is empty and a preset is selected, insert + submit */
                if (editor.len == 0 && try_these.visible) {
                    const char *preset = try_these_accept(&try_these);
                    if (preset) {
                        editor_replace_all(preset);
                        history_add(editor.buf);
                        try_these.visible = 0;
                        input_page_cleanup();
                        return strdup(editor.buf);
                    }
                }
                if (editor.len > 0) {
                    suggest_dismiss(&suggest);
                    history_add(editor.buf);
                }
                input_page_cleanup();
                return strdup(editor.buf);

            case KEY_CTRL_D:
                if (editor.len == 0) {
                    suggest_dismiss(&suggest);
                    input_page_cleanup();
                    return NULL;
                }
                break;

            case KEY_CTRL_C:
                suggest_dismiss(&suggest);
                try_these.visible = 1;
                try_these.selected = 0;
                editor_clear();
                break;

            case KEY_CHAR:
                editor_insert(key.ch);
                try_these.visible = 0;
                /* Auto-generate suggestions */
                suggest_generate(&suggest, editor.buf);
                break;

            case KEY_BACKSPACE:
                editor_backspace();
                if (editor.len == 0) {
                    /* Input now empty: show "Try These" */
                    suggest_dismiss(&suggest);
                    try_these.visible = 1;
                    try_these.selected = 0;
                } else {
                    /* Regenerate suggestions */
                    suggest.last_prefix[0] = '\0'; /* force regeneration */
                    suggest_generate(&suggest, editor.buf);
                }
                break;

            case KEY_DELETE:
                editor_delete();
                if (editor.len == 0) {
                    suggest_dismiss(&suggest);
                    try_these.visible = 1;
                    try_these.selected = 0;
                } else {
                    suggest.last_prefix[0] = '\0';
                    suggest_generate(&suggest, editor.buf);
                }
                break;

            case KEY_LEFT:
                editor_move(-1);
                break;

            case KEY_RIGHT:
                editor_move(1);
                break;

            case KEY_UP:
                if (suggest.visible && suggest.count > 0) {
                    suggest_move(&suggest, -1);
                } else if (editor.len == 0 && try_these.visible) {
                    try_these_move(&try_these, -1);
                } else {
                    history_up();
                    /* Update suggestions for history content */
                    if (editor.len > 0) {
                        suggest.last_prefix[0] = '\0';
                        suggest_generate(&suggest, editor.buf);
                    } else {
                        suggest_dismiss(&suggest);
                    }
                }
                break;

            case KEY_DOWN:
                if (suggest.visible && suggest.count > 0) {
                    suggest_move(&suggest, 1);
                } else if (editor.len == 0 && try_these.visible) {
                    try_these_move(&try_these, 1);
                } else {
                    history_down();
                    if (editor.len > 0) {
                        suggest.last_prefix[0] = '\0';
                        suggest_generate(&suggest, editor.buf);
                    } else {
                        suggest_dismiss(&suggest);
                    }
                }
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
                if (editor.len == 0) {
                    suggest_dismiss(&suggest);
                    try_these.visible = 1;
                } else {
                    suggest.last_prefix[0] = '\0';
                    suggest_generate(&suggest, editor.buf);
                }
                break;

            case KEY_CTRL_U:
                editor_kill_to_start();
                if (editor.len == 0) {
                    suggest_dismiss(&suggest);
                    try_these.visible = 1;
                } else {
                    suggest.last_prefix[0] = '\0';
                    suggest_generate(&suggest, editor.buf);
                }
                break;

            case KEY_CTRL_W:
                editor_kill_word();
                if (editor.len == 0) {
                    suggest_dismiss(&suggest);
                    try_these.visible = 1;
                } else {
                    suggest.last_prefix[0] = '\0';
                    suggest_generate(&suggest, editor.buf);
                }
                break;

            case KEY_CTRL_L:
                /* Force full redraw */
                {
                    RenderBuf *f, *b;
                    tui_render_get_bufs(&f, &b);
                    for (int i = 0; i < b->rows * b->cols; i++)
                        b->cells[i].fg = -99;
                }
                break;

            case KEY_TAB:
                if (editor.len == 0 && try_these.visible) {
                    /* Insert selected preset into input buffer */
                    const char *preset = try_these_accept(&try_these);
                    if (preset) {
                        editor_replace_all(preset);
                        try_these.visible = 0;
                        suggest.last_prefix[0] = '\0';
                        suggest_generate(&suggest, editor.buf);
                    }
                } else if (editor.len > 0 && suggest.visible &&
                           suggest.count > 0) {
                    if (suggest.selected == -1) {
                        /* Select first suggestion */
                        suggest.selected = 0;
                    } else {
                        /* Accept selected suggestion */
                        const char *text = suggest_accept(&suggest);
                        if (text) {
                            editor_replace_all(text);
                            suggest_dismiss(&suggest);
                            /* Regenerate with new content */
                            suggest.last_prefix[0] = '\0';
                            suggest_generate(&suggest, editor.buf);
                        }
                    }
                } else if (editor.len > 0 && !suggest.visible) {
                    /* Try generating suggestions first */
                    suggest.last_prefix[0] = '\0';
                    suggest_generate(&suggest, editor.buf);
                    if (!suggest.visible) {
                        /* Fall back to tab completion */
                        handle_tab();
                    }
                } else {
                    handle_tab();
                }
                break;

            case KEY_ESCAPE:
                if (suggest.visible) {
                    suggest_dismiss(&suggest);
                } else if (try_these.visible && editor.len == 0) {
                    try_these_dismiss(&try_these);
                }
                break;

            default:
                break;
        }

        render_input_page();
    }
}
