/*
 * shelli - Educational Shell
 * tui/tui_suggest.c - Autocomplete recommendation engine + "Try These" presets
 *
 * Provides ranked suggestions as the user types, using history frequency,
 * Levenshtein fuzzy matching, and a curated preset command database.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "tui.h"

/* Box drawing */
#define BOX_TL "\342\225\255"  /* ╭ */
#define BOX_TR "\342\225\256"  /* ╮ */
#define BOX_BL "\342\225\260"  /* ╰ */
#define BOX_BR "\342\225\257"  /* ╯ */
#define BOX_H  "\342\224\200"  /* ─ */
#define BOX_V  "\342\224\202"  /* │ */

/* Selection indicator */
#define SEL_MARKER "\342\226\270"  /* ▸ */

/*
 * ============================================================================
 * Preset Command Database
 * ============================================================================
 */

static const PresetCommand presets[] = {
    /* Basics */
    {"echo hello world",          "Print text to stdout",       "Basics",       COL_GREEN},
    {"echo $HOME",                "Variable expansion",         "Basics",       COL_GREEN},
    {"pwd",                       "Print working directory",    "Basics",       COL_GREEN},
    /* Pipes */
    {"ls -la | grep .c",          "Filter output with pipes",   "Pipes",        COL_MAUVE},
    /* Redirection */
    {"echo hello > /tmp/test",    "Redirect output to file",    "Redirection",  COL_TEAL},
    /* Control Flow */
    {"if true; then echo yes; fi","If-then conditional",        "Control Flow", COL_PINK},
    {"for i in 1 2 3; do echo $i; done", "For loop iteration", "Control Flow", COL_PINK},
    /* Operators */
    {"true && echo success",      "AND operator",               "Operators",    COL_PEACH},
    {"false || echo fallback",    "OR operator",                "Operators",    COL_PEACH},
    /* Expansion */
    {"echo $((2 + 3 * 4))",       "Arithmetic expansion",      "Expansion",    COL_YELLOW},
    {"echo ~",                    "Tilde expansion",            "Expansion",    COL_YELLOW},
    {"echo *.c",                  "Glob expansion",             "Expansion",    COL_YELLOW},
    /* Advanced */
    {"echo hello | tr a-z A-Z",   "Transform with pipe",        "Advanced",     COL_BLUE},
};

static const int preset_count = sizeof(presets) / sizeof(presets[0]);

const PresetCommand *suggest_get_presets(int *count) {
    if (count) *count = preset_count;
    return presets;
}

/*
 * ============================================================================
 * Frequency Table (djb2 hash, chained buckets)
 * ============================================================================
 */

typedef struct FreqEntry {
    char *word;
    int count;
    struct FreqEntry *next;
} FreqEntry;

static FreqEntry *freq_table[FREQ_TABLE_SIZE];

static unsigned int djb2_hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

void suggest_init(void) {
    memset(freq_table, 0, sizeof(freq_table));
}

void suggest_cleanup(void) {
    for (int i = 0; i < FREQ_TABLE_SIZE; i++) {
        FreqEntry *e = freq_table[i];
        while (e) {
            FreqEntry *next = e->next;
            free(e->word);
            free(e);
            e = next;
        }
        freq_table[i] = NULL;
    }
}

static void freq_increment(const char *word) {
    unsigned int h = djb2_hash(word) % FREQ_TABLE_SIZE;
    FreqEntry *e = freq_table[h];
    while (e) {
        if (strcmp(e->word, word) == 0) {
            e->count++;
            return;
        }
        e = e->next;
    }
    /* New entry */
    e = malloc(sizeof(FreqEntry));
    if (!e) return;
    e->word = strdup(word);
    e->count = 1;
    e->next = freq_table[h];
    freq_table[h] = e;
}

int suggest_get_frequency(const char *word) {
    unsigned int h = djb2_hash(word) % FREQ_TABLE_SIZE;
    FreqEntry *e = freq_table[h];
    while (e) {
        if (strcmp(e->word, word) == 0) return e->count;
        e = e->next;
    }
    return 0;
}

/* Extract first word from a command string */
static void extract_first_word(const char *cmd, char *out, int out_size) {
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    int i = 0;
    while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && i < out_size - 1) {
        out[i] = cmd[i];
        i++;
    }
    out[i] = '\0';
}

void suggest_record_usage(const char *command) {
    if (!command || !command[0]) return;
    char word[MAX_SUGGEST_LEN];
    extract_first_word(command, word, MAX_SUGGEST_LEN);
    if (word[0]) freq_increment(word);
}

void suggest_build_freq_table(void) {
    int count = tui_history_count();
    for (int i = 1; i <= count; i++) {
        const char *entry = tui_history_get(i);
        if (entry) suggest_record_usage(entry);
    }
}

/*
 * ============================================================================
 * Levenshtein Distance (bounded single-row DP)
 * ============================================================================
 */

int suggest_levenshtein(const char *a, const char *b, int max_dist) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);

    if (abs(la - lb) > max_dist) return max_dist + 1;

    /* Use shorter string as rows for less memory */
    if (la > lb) {
        const char *tmp = a; a = b; b = tmp;
        int t = la; la = lb; lb = t;
    }

    int row[MAX_SUGGEST_LEN + 1];
    if (la + 1 > MAX_SUGGEST_LEN + 1) return max_dist + 1;

    for (int i = 0; i <= la; i++) row[i] = i;

    for (int j = 1; j <= lb; j++) {
        int prev = row[0];
        row[0] = j;
        int row_min = row[0];

        for (int i = 1; i <= la; i++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int val = prev + cost;
            if (row[i] + 1 < val) val = row[i] + 1;
            if (row[i - 1] + 1 < val) val = row[i - 1] + 1;
            prev = row[i];
            row[i] = val;
            if (val < row_min) row_min = val;
        }

        /* Early termination: entire row exceeds threshold */
        if (row_min > max_dist) return max_dist + 1;
    }

    return row[la];
}

/*
 * ============================================================================
 * Scoring
 * ============================================================================
 */

int suggest_compute_score(const char *candidate, const char *prefix,
                          SuggestSource source, int frequency, int recency) {
    int score = 0;
    int prefix_len = (int)strlen(prefix);

    /* Prefix match bonus */
    if (prefix_len > 0 && strncmp(candidate, prefix, prefix_len) == 0) {
        score += 1000;
    } else if (prefix_len >= 2) {
        /* Fuzzy match penalty */
        /* Compare first word of candidate with prefix */
        char cand_word[MAX_SUGGEST_LEN];
        extract_first_word(candidate, cand_word, MAX_SUGGEST_LEN);
        int dist = suggest_levenshtein(cand_word, prefix, 2);
        if (dist <= 2) {
            score -= dist * 100;
        } else {
            return -10000; /* No match */
        }
    } else {
        return -10000; /* No match for single char without prefix match */
    }

    /* Frequency bonus */
    score += frequency * 50;

    /* Recency bonus: recency is rank from most recent (0 = most recent) */
    if (recency >= 0) {
        score += 200 - (recency * 5);
        if (score < 0) score = 0; /* Don't let recency go too negative */
    }

    /* Source bonus */
    switch (source) {
    case SRC_HISTORY:    score += 300; break;
    case SRC_PRESET:     score += 200; break;
    case SRC_BUILTIN:    score += 150; break;
    case SRC_KEYWORD:    score += 100; break;
    case SRC_EXECUTABLE: score += 50;  break;
    }

    return score;
}

/*
 * ============================================================================
 * Suggestion Generation
 * ============================================================================
 */

/* Duplicate of helpers from tui_input.c (avoids exposing statics) */
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

/* Check if suggestion already exists in list */
static int suggestion_exists(const SuggestState *state, const char *text) {
    for (int i = 0; i < state->count; i++) {
        if (strcmp(state->items[i].text, text) == 0) return 1;
    }
    return 0;
}

/* Add a candidate to the suggestion list */
static void add_candidate(SuggestState *state, const char *text,
                           const char *desc, SuggestSource source,
                           int score, int frequency) {
    if (state->count >= MAX_SUGGESTIONS) {
        /* Replace lowest-scoring item if this one is better */
        int min_idx = 0;
        for (int i = 1; i < state->count; i++) {
            if (state->items[i].score < state->items[min_idx].score)
                min_idx = i;
        }
        if (score <= state->items[min_idx].score) return;
        /* Replace */
        strncpy(state->items[min_idx].text, text, MAX_SUGGEST_LEN - 1);
        state->items[min_idx].text[MAX_SUGGEST_LEN - 1] = '\0';
        strncpy(state->items[min_idx].description, desc ? desc : "",
                MAX_SUGGEST_LEN - 1);
        state->items[min_idx].description[MAX_SUGGEST_LEN - 1] = '\0';
        state->items[min_idx].source = source;
        state->items[min_idx].score = score;
        state->items[min_idx].frequency = frequency;
        return;
    }
    Suggestion *s = &state->items[state->count];
    strncpy(s->text, text, MAX_SUGGEST_LEN - 1);
    s->text[MAX_SUGGEST_LEN - 1] = '\0';
    strncpy(s->description, desc ? desc : "", MAX_SUGGEST_LEN - 1);
    s->description[MAX_SUGGEST_LEN - 1] = '\0';
    s->source = source;
    s->score = score;
    s->frequency = frequency;
    state->count++;
}

/* Sort suggestions by score descending */
static int cmp_suggestions(const void *a, const void *b) {
    const Suggestion *sa = (const Suggestion *)a;
    const Suggestion *sb = (const Suggestion *)b;
    return sb->score - sa->score;
}

void suggest_generate(SuggestState *state, const char *prefix) {
    state->count = 0;
    state->selected = -1;

    if (!prefix || !prefix[0]) {
        state->visible = 0;
        state->last_prefix[0] = '\0';
        return;
    }

    /* Cache check: skip if prefix unchanged */
    if (strcmp(state->last_prefix, prefix) == 0 && state->visible) return;

    strncpy(state->last_prefix, prefix, MAX_SUGGEST_LEN - 1);
    state->last_prefix[MAX_SUGGEST_LEN - 1] = '\0';

    int prefix_len = (int)strlen(prefix);

    /* Extract the current word being typed */
    int word_start = find_word_start(prefix, prefix_len);
    const char *word_prefix = prefix + word_start;
    int word_len = prefix_len - word_start;
    int in_cmd_pos = is_command_position(prefix, word_start);

    if (word_len == 0) {
        state->visible = 0;
        return;
    }

    /* 1. History candidates (full command matches) */
    int hist_count = tui_history_count();
    for (int i = hist_count; i >= 1; i--) {
        const char *entry = tui_history_get(i);
        if (!entry) continue;

        int recency = hist_count - i;
        char first_word[MAX_SUGGEST_LEN];
        extract_first_word(entry, first_word, MAX_SUGGEST_LEN);
        int freq = suggest_get_frequency(first_word);

        /* Match full prefix against history entry */
        if (strncmp(entry, prefix, prefix_len) == 0 && strlen(entry) > (size_t)prefix_len) {
            int score = suggest_compute_score(entry, prefix, SRC_HISTORY,
                                              freq, recency);
            if (score > -10000 && !suggestion_exists(state, entry)) {
                add_candidate(state, entry, NULL, SRC_HISTORY, score, freq);
            }
        }
        /* Also match just the current word for command-position suggestions */
        else if (in_cmd_pos && word_start == 0) {
            if (strncmp(first_word, word_prefix, word_len) == 0 &&
                strlen(first_word) > (size_t)word_len) {
                int score = suggest_compute_score(first_word, word_prefix,
                                                  SRC_HISTORY, freq, recency);
                if (score > -10000 && !suggestion_exists(state, first_word)) {
                    add_candidate(state, first_word, NULL, SRC_HISTORY,
                                  score, freq);
                }
            }
        }
    }

    /* 2. Builtin commands (only in command position) */
    if (in_cmd_pos) {
        static const char *builtin_names[] = {
            "cd", "pwd", "exit", "help", "export", "unset", "history",
            "test", "echo", "type", "true", "false",
            "local", "return", "source",
            "jobs", "fg", "bg", NULL
        };
        static const char *builtin_descs[] = {
            "Change directory", "Print working directory", "Exit shell",
            "Show help", "Export variable", "Unset variable", "Show history",
            "Evaluate expression", "Print arguments", "Show command type",
            "Return true", "Return false",
            "Declare local variable", "Return from function", "Execute script",
            "List background jobs", "Bring job to foreground",
            "Send job to background", NULL
        };
        for (int i = 0; builtin_names[i]; i++) {
            int freq = suggest_get_frequency(builtin_names[i]);
            int score = suggest_compute_score(builtin_names[i], word_prefix,
                                              SRC_BUILTIN, freq, -1);
            if (score > -10000 && !suggestion_exists(state, builtin_names[i])) {
                add_candidate(state, builtin_names[i], builtin_descs[i],
                              SRC_BUILTIN, score, freq);
            }
        }
    }

    /* 3. Keywords (only in command position) */
    if (in_cmd_pos) {
        static const char *keywords[] = {
            "if", "then", "elif", "else", "fi",
            "while", "until", "do", "done",
            "for", "in", "case", "esac", NULL
        };
        for (int i = 0; keywords[i]; i++) {
            int score = suggest_compute_score(keywords[i], word_prefix,
                                              SRC_KEYWORD, 0, -1);
            if (score > -10000 && !suggestion_exists(state, keywords[i])) {
                add_candidate(state, keywords[i], NULL, SRC_KEYWORD, score, 0);
            }
        }
    }

    /* 4. Preset commands (match full prefix against preset command text) */
    for (int i = 0; i < preset_count; i++) {
        if (strncmp(presets[i].command, prefix, prefix_len) == 0 &&
            strlen(presets[i].command) > (size_t)prefix_len) {
            int score = suggest_compute_score(presets[i].command, prefix,
                                              SRC_PRESET, 0, -1);
            if (score > -10000 && !suggestion_exists(state, presets[i].command)) {
                add_candidate(state, presets[i].command, presets[i].description,
                              SRC_PRESET, score, 0);
            }
        }
    }

    /* 5. PATH executables (only in command position, prefix >= 2 chars) */
    if (in_cmd_pos && word_len >= 2) {
        const char *path_env = getenv("PATH");
        if (path_env) {
            char *path_copy = strdup(path_env);
            char *dir = strtok(path_copy, ":");
            int exec_added = 0;
            while (dir && exec_added < 20) {
                DIR *d = opendir(dir);
                if (d) {
                    struct dirent *entry;
                    while ((entry = readdir(d)) != NULL && exec_added < 20) {
                        if (strncmp(entry->d_name, word_prefix, word_len) == 0 &&
                            strlen(entry->d_name) > (size_t)word_len) {
                            char full_path[4096];
                            snprintf(full_path, sizeof(full_path), "%s/%s",
                                     dir, entry->d_name);
                            struct stat st;
                            if (stat(full_path, &st) == 0 &&
                                (st.st_mode & S_IXUSR) &&
                                !suggestion_exists(state, entry->d_name)) {
                                int freq = suggest_get_frequency(entry->d_name);
                                int score = suggest_compute_score(
                                    entry->d_name, word_prefix, SRC_EXECUTABLE,
                                    freq, -1);
                                if (score > -10000) {
                                    add_candidate(state, entry->d_name, NULL,
                                                  SRC_EXECUTABLE, score, freq);
                                    exec_added++;
                                }
                            }
                        }
                    }
                    closedir(d);
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    /* 6. Fuzzy matching from history (only if few prefix matches and word >= 2) */
    if (state->count < 3 && word_len >= 2 && in_cmd_pos) {
        for (int i = hist_count; i >= 1 && state->count < MAX_SUGGESTIONS; i--) {
            const char *entry = tui_history_get(i);
            if (!entry) continue;

            char first_word[MAX_SUGGEST_LEN];
            extract_first_word(entry, first_word, MAX_SUGGEST_LEN);

            if (suggestion_exists(state, first_word)) continue;

            int dist = suggest_levenshtein(first_word, word_prefix, 2);
            if (dist > 0 && dist <= 2) {
                int freq = suggest_get_frequency(first_word);
                int recency = hist_count - i;
                /* Score with fuzzy penalty already applied */
                int score = suggest_compute_score(first_word, word_prefix,
                                                  SRC_HISTORY, freq, recency);
                /* Manually fix: compute_score returned -10000 for non-prefix,
                   but we already know edit distance is ok */
                score = -(dist * 100) + freq * 50 + 300;
                if (recency >= 0) score += 200 - (recency * 5);
                add_candidate(state, first_word, NULL, SRC_HISTORY, score, freq);
            }
        }
    }

    /* Sort by score */
    if (state->count > 1)
        qsort(state->items, state->count, sizeof(Suggestion), cmp_suggestions);

    state->visible = (state->count > 0) ? 1 : 0;
}

/*
 * ============================================================================
 * Navigation
 * ============================================================================
 */

void suggest_move(SuggestState *state, int delta) {
    if (state->count == 0) return;
    if (state->selected == -1) {
        state->selected = (delta > 0) ? 0 : state->count - 1;
    } else {
        state->selected += delta;
        if (state->selected < 0) state->selected = state->count - 1;
        if (state->selected >= state->count) state->selected = 0;
    }
}

void try_these_move(TryTheseState *state, int delta) {
    state->selected += delta;
    if (state->selected < 0) state->selected = preset_count - 1;
    if (state->selected >= preset_count) state->selected = 0;
}

const char *suggest_accept(SuggestState *state) {
    if (state->selected < 0 || state->selected >= state->count)
        return NULL;
    return state->items[state->selected].text;
}

const char *try_these_accept(TryTheseState *state) {
    if (state->selected < 0 || state->selected >= preset_count)
        return NULL;
    return presets[state->selected].command;
}

void suggest_dismiss(SuggestState *state) {
    state->visible = 0;
    state->selected = -1;
    state->count = 0;
    state->last_prefix[0] = '\0';
}

void try_these_dismiss(TryTheseState *state) {
    state->visible = 0;
}

/*
 * ============================================================================
 * Rendering - Suggestion Dropdown
 * ============================================================================
 */

static int source_color(SuggestSource src) {
    switch (src) {
    case SRC_HISTORY:    return COL_YELLOW;
    case SRC_BUILTIN:    return COL_PINK;
    case SRC_KEYWORD:    return COL_MAUVE;
    case SRC_EXECUTABLE: return COL_BLUE;
    case SRC_PRESET:     return COL_GREEN;
    }
    return COL_TEXT;
}

static const char *source_label(SuggestSource src) {
    switch (src) {
    case SRC_HISTORY:    return "history";
    case SRC_BUILTIN:    return "builtin";
    case SRC_KEYWORD:    return "keyword";
    case SRC_EXECUTABLE: return "exec";
    case SRC_PRESET:     return "preset";
    }
    return "";
}

void suggest_render_dropdown(RenderBuf *rb, const SuggestState *state,
                             int x, int y, int w) {
    if (!state->visible || state->count == 0) return;

    int h = state->count + 2; /* border top + items + border bottom */

    /* Top border with title */
    rbuf_put_str(rb, y, x, BOX_TL, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_str(rb, y, x + 1, BOX_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_str(rb, y, x + 2, " Suggestions ", COL_OVERLAY, COL_BASE, 0, 1);
    for (int c = x + 15; c < x + w - 1; c++)
        rbuf_put_str(rb, y, c, BOX_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_str(rb, y, x + w - 1, BOX_TR, COL_OVERLAY, COL_BASE, 0, 0);

    /* Items */
    for (int i = 0; i < state->count; i++) {
        int row = y + 1 + i;
        const Suggestion *s = &state->items[i];
        int selected = (i == state->selected);
        int bg = selected ? COL_SURFACE : COL_BASE;

        /* Left border */
        rbuf_put_str(rb, row, x, BOX_V, COL_OVERLAY, COL_BASE, 0, 0);

        /* Fill background */
        for (int c = x + 1; c < x + w - 1; c++)
            rbuf_put_char(rb, row, c, " ", -1, bg, 0, 0);

        /* Selection indicator */
        if (selected) {
            rbuf_put_str(rb, row, x + 2, SEL_MARKER, COL_NEON_CYAN, bg, 0, 0);
        }

        /* Command text */
        int text_col = x + 4;
        int max_text = w / 2 - 4;
        if (max_text < 8) max_text = 8;
        rbuf_put_str_trunc(rb, row, text_col, s->text,
                           selected ? COL_TEXT : COL_SUBTEXT, bg,
                           selected ? 1 : 0, 0, max_text);

        /* Source tag */
        int tag_col = x + 4 + max_text + 1;
        rbuf_put_str_trunc(rb, row, tag_col, source_label(s->source),
                           source_color(s->source), bg, 0, 0, 8);

        /* Description or frequency */
        int desc_col = tag_col + 9;
        int desc_max = (x + w - 2) - desc_col;
        if (desc_max > 0) {
            if (s->description[0]) {
                rbuf_put_str_trunc(rb, row, desc_col, s->description,
                                   COL_OVERLAY, bg, 0, 1, desc_max);
            } else if (s->frequency > 0) {
                char freq_str[16];
                snprintf(freq_str, sizeof(freq_str), "\303\227%d", s->frequency);
                rbuf_put_str_trunc(rb, row, desc_col, freq_str,
                                   COL_OVERLAY, bg, 0, 1, desc_max);
            }
        }

        /* Right border */
        rbuf_put_str(rb, row, x + w - 1, BOX_V, COL_OVERLAY, COL_BASE, 0, 0);
    }

    /* Bottom border */
    int bot = y + h - 1;
    rbuf_put_str(rb, bot, x, BOX_BL, COL_OVERLAY, COL_BASE, 0, 0);
    for (int c = x + 1; c < x + w - 1; c++)
        rbuf_put_str(rb, bot, c, BOX_H, COL_OVERLAY, COL_BASE, 0, 0);
    rbuf_put_str(rb, bot, x + w - 1, BOX_BR, COL_OVERLAY, COL_BASE, 0, 0);
}

/*
 * ============================================================================
 * Rendering - "Try These" Panel
 * ============================================================================
 */

void suggest_render_try_these(RenderBuf *rb, const TryTheseState *state,
                              int x, int y, int w, int max_h) {
    if (!state->visible) return;

    /* Calculate total rows needed: category headers + items + spacing */
    int total_rows = 0;
    const char *last_cat = NULL;
    for (int i = 0; i < preset_count; i++) {
        if (!last_cat || strcmp(presets[i].category, last_cat) != 0) {
            if (last_cat) total_rows++; /* blank line between categories */
            total_rows++; /* category header */
            last_cat = presets[i].category;
        }
        total_rows++; /* item */
    }

    int content_h = total_rows;
    if (content_h > max_h - 2) content_h = max_h - 2;
    int h = content_h + 2; /* borders */

    /* Top border with title */
    rbuf_put_str(rb, y, x, BOX_TL, COL_LAVENDER, COL_BASE, 0, 0);
    rbuf_put_str(rb, y, x + 1, BOX_H, COL_LAVENDER, COL_BASE, 0, 0);
    rbuf_put_str(rb, y, x + 2, " Try These ", COL_LAVENDER, COL_BASE, 1, 0);
    for (int c = x + 13; c < x + w - 1; c++)
        rbuf_put_str(rb, y, c, BOX_H, COL_LAVENDER, COL_BASE, 0, 0);
    rbuf_put_str(rb, y, x + w - 1, BOX_TR, COL_LAVENDER, COL_BASE, 0, 0);

    /* Determine scroll window */
    int scroll = state->scroll_offset;

    /* Render items */
    int row_idx = 0; /* logical row */
    int draw_row = 0; /* rows drawn in content area */
    last_cat = NULL;

    for (int i = 0; i < preset_count && draw_row < content_h; i++) {
        /* Category header */
        if (!last_cat || strcmp(presets[i].category, last_cat) != 0) {
            /* Blank separator between categories */
            if (last_cat) {
                if (row_idx >= scroll && draw_row < content_h) {
                    int r = y + 1 + draw_row;
                    rbuf_put_str(rb, r, x, BOX_V, COL_LAVENDER, COL_BASE, 0, 0);
                    for (int c = x + 1; c < x + w - 1; c++)
                        rbuf_put_char(rb, r, c, " ", -1, COL_BASE, 0, 0);
                    rbuf_put_str(rb, r, x + w - 1, BOX_V, COL_LAVENDER,
                                 COL_BASE, 0, 0);
                    draw_row++;
                }
                row_idx++;
            }

            /* Category header row */
            if (row_idx >= scroll && draw_row < content_h) {
                int r = y + 1 + draw_row;
                rbuf_put_str(rb, r, x, BOX_V, COL_LAVENDER, COL_BASE, 0, 0);
                for (int c = x + 1; c < x + w - 1; c++)
                    rbuf_put_char(rb, r, c, " ", -1, COL_BASE, 0, 0);
                rbuf_put_str(rb, r, x + 3, presets[i].category,
                             presets[i].color, COL_BASE, 1, 0);
                rbuf_put_str(rb, r, x + w - 1, BOX_V, COL_LAVENDER,
                             COL_BASE, 0, 0);
                draw_row++;
            }
            row_idx++;
            last_cat = presets[i].category;
        }

        /* Preset item row */
        if (row_idx >= scroll && draw_row < content_h) {
            int r = y + 1 + draw_row;
            int selected = (i == state->selected);
            int bg = selected ? COL_SURFACE : COL_BASE;

            rbuf_put_str(rb, r, x, BOX_V, COL_LAVENDER, COL_BASE, 0, 0);
            for (int c = x + 1; c < x + w - 1; c++)
                rbuf_put_char(rb, r, c, " ", -1, bg, 0, 0);

            /* Selection indicator */
            if (selected) {
                rbuf_put_str(rb, r, x + 3, SEL_MARKER, COL_NEON_CYAN, bg, 0, 0);
            }

            /* Command text */
            int cmd_col = x + 5;
            int max_cmd = w / 2 - 4;
            if (max_cmd < 10) max_cmd = 10;
            rbuf_put_str_trunc(rb, r, cmd_col, presets[i].command,
                               COL_TEXT, bg, selected ? 1 : 0, 0, max_cmd);

            /* Description */
            int desc_col = cmd_col + max_cmd + 2;
            int desc_max = (x + w - 2) - desc_col;
            if (desc_max > 0) {
                rbuf_put_str_trunc(rb, r, desc_col, presets[i].description,
                                   COL_OVERLAY, bg, 0, 1, desc_max);
            }

            rbuf_put_str(rb, r, x + w - 1, BOX_V, COL_LAVENDER, COL_BASE, 0, 0);
            draw_row++;
        }
        row_idx++;
    }

    /* Fill remaining rows if content is shorter than panel */
    while (draw_row < content_h) {
        int r = y + 1 + draw_row;
        rbuf_put_str(rb, r, x, BOX_V, COL_LAVENDER, COL_BASE, 0, 0);
        for (int c = x + 1; c < x + w - 1; c++)
            rbuf_put_char(rb, r, c, " ", -1, COL_BASE, 0, 0);
        rbuf_put_str(rb, r, x + w - 1, BOX_V, COL_LAVENDER, COL_BASE, 0, 0);
        draw_row++;
    }

    /* Bottom border */
    int bot = y + h - 1;
    rbuf_put_str(rb, bot, x, BOX_BL, COL_LAVENDER, COL_BASE, 0, 0);
    for (int c = x + 1; c < x + w - 1; c++)
        rbuf_put_str(rb, bot, c, BOX_H, COL_LAVENDER, COL_BASE, 0, 0);
    rbuf_put_str(rb, bot, x + w - 1, BOX_BR, COL_LAVENDER, COL_BASE, 0, 0);
}
