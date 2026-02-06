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

static volatile sig_atomic_t interrupted = 0;

static void handle_sigint(int sig) {
    (void)sig;
    interrupted = 1;
}

static void exec_logger(const char *message) {
    tui_log_exec(message);
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

        tui_show_tokens(&tokens);

        if (tui_is_debug()) {
            tui_wait_step("Tokenization complete");
        }

        /* Parse */
        char error[256] = "";
        Pipeline *pipeline = parser_parse(&tokens, error, sizeof(error));

        if (!pipeline && error[0]) {
            tui_show_error(error);
            tokenlist_free(&tokens);
            free(line);
            continue;
        }

        if (pipeline) {
            tui_show_pipeline(pipeline);

            if (tui_is_debug()) {
                tui_wait_step("Parsing complete");
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
                tui_show_result(last_exit, output_buf[0] ? output_buf : NULL);
            }

            if (tui_is_debug()) {
                tui_wait_step("Execution complete");
            }

            pipeline_free(pipeline);
        }

        tokenlist_free(&tokens);
        free(line);
    }

    /* Cleanup TUI (restores terminal) */
    tui_cleanup();

    return last_exit;
}
