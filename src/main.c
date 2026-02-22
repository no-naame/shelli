/*
 * shelli - Educational Shell
 * main.c - REPL loop with professional TUI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "tui/tui.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "builtins.h"
#include "expand.h"

static volatile sig_atomic_t interrupted = 0;

static void handle_sigint(int sig) {
    (void)sig;
    interrupted = 1;
}

static void exec_logger(const char *message) {
    tui_log_exec(message);
}

/*
 * For any command in the pipeline that has a heredoc_delim, collect lines
 * from the user until the delimiter line is seen, then write them into a pipe
 * and leave the read-end in cmd->heredoc_fd.
 *
 * We temporarily leave raw mode so fgets() works normally.
 * Returns 0 on success, -1 on error.
 */
static int collect_heredocs(Pipeline *pipeline) {
    int found = 0;
    for (Command *cmd = pipeline->first; cmd; cmd = cmd->next) {
        if (cmd->heredoc_delim) { found = 1; break; }
    }
    if (!found) return 0;

    /* Leave raw mode so the terminal is in normal line-reading mode */
    tui_suspend_raw();

    for (Command *cmd = pipeline->first; cmd; cmd = cmd->next) {
        if (!cmd->heredoc_delim) continue;

        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            tui_resume_raw();
            return -1;
        }

        /* Prompt and collect lines until delimiter */
        char linebuf[4096];
        while (1) {
            printf("heredoc> ");
            fflush(stdout);

            if (!fgets(linebuf, sizeof(linebuf), stdin)) {
                /* EOF — stop reading */
                break;
            }

            /* Strip trailing newline for delimiter comparison */
            int len = (int)strlen(linebuf);
            if (len > 0 && linebuf[len - 1] == '\n') {
                linebuf[len - 1] = '\0';
                len--;
            }

            if (strcmp(linebuf, cmd->heredoc_delim) == 0) {
                break;  /* Delimiter line reached */
            }

            /* Write the line (with newline) to the pipe */
            linebuf[len] = '\n';
            linebuf[len + 1] = '\0';
            write(pipefd[1], linebuf, len + 1);
        }

        close(pipefd[1]);  /* Signal EOF to the reader */
        cmd->heredoc_fd = pipefd[0];
    }

    tui_resume_raw();
    return 0;
}

/*
 * Expand history references in a line:
 *   !!   -> last command
 *   !n   -> command number n (1-based)
 *
 * Returns a newly allocated string (caller must free), or NULL on error.
 * If no expansion needed, returns strdup of the original line.
 */
static char *expand_history(const char *line) {
    if (line[0] != '!') {
        return strdup(line);
    }

    const char *ref = line + 1;
    const char *replacement = NULL;
    int count = tui_history_count();

    if (ref[0] == '!') {
        /* !! -> last command */
        if (count == 0) {
            fprintf(stderr, "shelli: !!: no previous command\n");
            return NULL;
        }
        replacement = tui_history_get(count);
    } else {
        /* !n -> command number n */
        char *endptr;
        long n = strtol(ref, &endptr, 10);
        if (*endptr != '\0' || n < 1) {
            fprintf(stderr, "shelli: !%s: unknown history reference\n", ref);
            return NULL;
        }
        replacement = tui_history_get((int)n);
        if (!replacement) {
            fprintf(stderr, "shelli: !%ld: event not found\n", n);
            return NULL;
        }
    }

    /* Echo the expanded command so the user sees what runs */
    printf("%s\n", replacement);
    fflush(stdout);

    return strdup(replacement);
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  --debug    Enable step-by-step execution mode\n");
    printf("  --help     Show this help message\n");
    printf("\n");
    printf("shelli is an educational shell that visualizes how shells work.\n");
}

/*
 * Split a token list into segments separated by ;, &&, ||.
 * Each segment is a pipeline. The separator type tells how to
 * chain them (unconditional, and, or).
 *
 * Returns an array of (start_index, end_index, separator_after) triples.
 */
typedef struct {
    int start;      /* First token index of this segment */
    int end;        /* One past last token index */
    TokenType sep;  /* Separator AFTER this segment (TOK_SEMI, TOK_AND, TOK_OR, TOK_EOF) */
} Segment;

static int split_segments(TokenList *tokens, Segment *segs, int max_segs) {
    int count = 0;
    int start = 0;

    for (int i = 0; i < tokens->count; i++) {
        TokenType t = tokens->tokens[i].type;
        if (t == TOK_SEMI || t == TOK_AND || t == TOK_OR || t == TOK_EOF) {
            if (i > start) {
                /* Non-empty segment */
                if (count >= max_segs) return count;
                segs[count].start = start;
                segs[count].end = i;
                segs[count].sep = t;
                count++;
            }
            start = i + 1;
            if (t == TOK_EOF) break;
        }
    }

    return count;
}

/*
 * Create a sub-token-list from a range of tokens (shallow copy, no strdup).
 * Adds a TOK_EOF at the end. Caller must free the tokens array but NOT the values.
 */
static TokenList make_sub_tokens(TokenList *src, int start, int end) {
    TokenList sub;
    int count = end - start;
    sub.count = count + 1; /* +1 for EOF */
    sub.capacity = sub.count;
    sub.tokens = malloc(sub.count * sizeof(Token));

    for (int i = 0; i < count; i++) {
        sub.tokens[i].type = src->tokens[start + i].type;
        sub.tokens[i].value = src->tokens[start + i].value; /* shallow */
    }
    sub.tokens[count].type = TOK_EOF;
    sub.tokens[count].value = NULL;

    return sub;
}

int main(int argc, char *argv[]) {
    int debug_mode = 0;
    int show_splash = 1;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[i], "--no-splash") == 0) {
            show_splash = 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Initialize TUI (enters raw mode, alt screen) */
    if (tui_init() < 0) {
        fprintf(stderr, "Failed to initialize TUI\n");
        return 1;
    }

    /* Load history from disk */
    tui_history_load();

    /* Set up signal handling (after TUI init to ensure cleanup works) */
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    tui_set_debug(debug_mode);
    executor_set_logger(exec_logger);

    /* Show splash screen */
    if (show_splash) {
        tui_splash();
    }

    /* Draw initial frame */
    tui_draw_frame();

    /* Main REPL loop */
    int last_exit = 0;
    int should_exit = 0;

    while (!should_exit) {
        interrupted = 0;

        /* Begin input stage (panels from previous command remain visible) */
        tui_stage_begin(STAGE_INPUT);

        /* Read input */
        char *line = tui_read_line();
        if (!line) {
            /* EOF (Ctrl+D on empty line) */
            break;
        }

        /* Skip empty lines */
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        /* Expand history references: !! and !n */
        if (line[0] == '!') {
            char *expanded = expand_history(line);
            free(line);
            if (!expanded) {
                /* Error already printed; show it in TUI too */
                continue;
            }
            line = expanded;
        }

        /* Clear all processing panels NOW - after user entered a new command */
        tui_clear_all_panels();

        /* Update input panel with the command */
        tui_update_panel(PANEL_INPUT, line);

        /* Mark input stage complete */
        tui_stage_end(STAGE_INPUT);

        if (tui_is_debug()) {
            tui_wait_step("Input received");
        }

        /* Tokenize */
        TokenList tokens;
        if (lexer_tokenize(line, &tokens) < 0) {
            tui_show_error("Tokenization error (unterminated quote?)");
            free(line);
            continue;
        }

        /* Expand words: tilde, variables, globs */
        expand_set_exit_code(last_exit);
        if (expand_words(&tokens) < 0) {
            tui_show_error("Expansion error");
            tokenlist_free(&tokens);
            free(line);
            continue;
        }

        tui_show_tokens(&tokens);

        if (tui_is_debug()) {
            tui_wait_step("Tokenization complete");
        }

        /* Split into segments by ; && || */
        Segment segs[64];
        int seg_count = split_segments(&tokens, segs, 64);

        if (seg_count == 0) {
            tokenlist_free(&tokens);
            free(line);
            continue;
        }

        /* Execute each segment based on separator logic */
        for (int s = 0; s < seg_count && !should_exit; s++) {
            /* Check chaining condition from PREVIOUS separator */
            if (s > 0) {
                TokenType prev_sep = segs[s - 1].sep;
                if (prev_sep == TOK_AND && last_exit != 0) {
                    /* && but previous failed: skip */
                    tui_log_exec("&& skip (previous failed)");
                    continue;
                }
                if (prev_sep == TOK_OR && last_exit == 0) {
                    /* || but previous succeeded: skip */
                    tui_log_exec("|| skip (previous succeeded)");
                    continue;
                }
            }

            /* Build a sub token list for this segment */
            TokenList sub = make_sub_tokens(&tokens, segs[s].start, segs[s].end);

            /* Parse this segment */
            char error[256] = "";
            Pipeline *pipeline = parser_parse(&sub, error, sizeof(error));

            if (!pipeline && error[0]) {
                tui_show_error(error);
                free(sub.tokens);
                break;
            }

            if (pipeline) {
                tui_show_pipeline(pipeline);

                if (tui_is_debug()) {
                    tui_wait_step("Parsing complete");
                }

                /* Collect heredoc body from user if any << operator was used */
                if (collect_heredocs(pipeline) < 0) {
                    pipeline_free(pipeline);
                    free(sub.tokens);
                    break;
                }

                /* Check if first command is exit (only for single command) */
                if (pipeline->cmd_count == 1 &&
                    pipeline->first->argc > 0 &&
                    strcmp(pipeline->first->argv[0], "exit") == 0) {

                    int dummy;
                    last_exit = builtin_execute(pipeline->first, &dummy);
                    should_exit = 1;
                    tui_log_exec("builtin: exit");
                    tui_show_result(last_exit, "Goodbye!");
                } else {
                    /* Execute with output capture */
                    tui_stage_begin(STAGE_EXECUTE);
                    char output_buf[1024] = "";
                    last_exit = executor_run_capture(pipeline, output_buf, sizeof(output_buf));
                    expand_set_exit_code(last_exit);
                    tui_show_result(last_exit, output_buf[0] ? output_buf : NULL);
                }

                if (tui_is_debug()) {
                    tui_wait_step("Execution complete");
                }

                pipeline_free(pipeline);
            }

            free(sub.tokens); /* shallow copy, don't free values */
        }

        tokenlist_free(&tokens);
        free(line);
    }

    /* Save history to disk before exit */
    tui_history_save();

    /* Cleanup TUI (restores terminal) */
    tui_cleanup();

    return last_exit;
}
