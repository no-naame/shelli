/*
 * shelli - Educational Shell
 * tui/tui_core.c - Terminal control (raw mode, alternate buffer)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "tui.h"

/*
 * Terminal state
 */
static struct termios orig_termios;
static int raw_mode_enabled = 0;
static int alt_screen_enabled = 0;
static int term_width = 80;
static int term_height = 24;

/* Forward declarations for internal functions */
void splash_draw(int width, int height);
void splash_animate(int width, int height, int frame);

/*
 * Update terminal size from ioctl
 */
static void update_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_width = ws.ws_col;
        term_height = ws.ws_row;
    }
}

/*
 * SIGWINCH handler for terminal resize
 */
static void handle_winch(int sig) {
    (void)sig;
    update_size();
    /* Redraw frame on resize */
    tui_draw_frame();
}

/*
 * Enter raw mode
 */
static int enter_raw_mode(void) {
    if (raw_mode_enabled) return 0;

    if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
        return -1;
    }

    struct termios raw = orig_termios;

    /* Input modes: disable break, CR to NL, parity check, strip, flow control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* Output modes: disable post-processing */
    raw.c_oflag &= ~(OPOST);

    /* Control modes: 8-bit chars */
    raw.c_cflag |= (CS8);

    /* Local modes: disable echo, canonical, extended, signals */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* Control chars: set read timeout */
    raw.c_cc[VMIN] = 0;   /* No minimum chars */
    raw.c_cc[VTIME] = 1;  /* 100ms timeout */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        return -1;
    }

    raw_mode_enabled = 1;
    return 0;
}

/*
 * Exit raw mode
 */
static void exit_raw_mode(void) {
    if (!raw_mode_enabled) return;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_enabled = 0;
}

/*
 * Enter alternate screen buffer
 */
static void enter_alt_screen(void) {
    if (alt_screen_enabled) return;

    printf(ALT_SCREEN_ON);
    printf(CUR_HIDE);
    fflush(stdout);

    alt_screen_enabled = 1;
}

/*
 * Exit alternate screen buffer
 */
static void exit_alt_screen(void) {
    if (!alt_screen_enabled) return;

    printf(CUR_SHOW);
    printf(ALT_SCREEN_OFF);
    fflush(stdout);

    alt_screen_enabled = 0;
}

/*
 * Initialize the TUI system
 */
int tui_init(void) {
    /* Get initial terminal size */
    update_size();

    /* Enter alternate screen first (before raw mode) */
    enter_alt_screen();

    /* Enter raw mode for input handling */
    if (enter_raw_mode() < 0) {
        exit_alt_screen();
        return -1;
    }

    /* Set up SIGWINCH handler */
    struct sigaction sa;
    sa.sa_handler = handle_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    /* Clear screen */
    printf(BG_BASE);
    printf(SCR_CLEAR);
    printf(CUR_HOME);
    fflush(stdout);

    return 0;
}

/*
 * Cleanup and restore terminal
 */
void tui_cleanup(void) {
    /* Exit raw mode first */
    exit_raw_mode();

    /* Exit alternate screen */
    exit_alt_screen();

    /* Reset colors */
    printf(COL_RESET);
    fflush(stdout);
}

/*
 * Get terminal dimensions
 */
void tui_get_size(int *width, int *height) {
    update_size();
    if (width) *width = term_width;
    if (height) *height = term_height;
}

/*
 * Internal: get current width
 */
int term_get_width(void) {
    return term_width;
}

/*
 * Internal: get current height
 */
int term_get_height(void) {
    return term_height;
}

/*
 * Show splash screen with animation
 */
void tui_splash(void) {
    /* Simple animation: fade in over a few frames */
    for (int frame = 0; frame < 5; frame++) {
        splash_animate(term_width, term_height, frame);
        usleep(50000);  /* 50ms per frame */
    }

    /* Wait for keypress */
    printf(CUR_SHOW);
    fflush(stdout);

    /* Read any key */
    char c;
    while (read(STDIN_FILENO, &c, 1) != 1) {
        /* Wait for input */
    }

    printf(CUR_HIDE);
    fflush(stdout);
}

/*
 * Animation tick (called periodically)
 */
static int tick_counter = 0;

void tui_tick(void) {
    tick_counter++;
}

/*
 * Get spinner character for animation frame
 */
static const char *SPINNER_FRAMES[] = {
    "\342\240\213", /* ⠋ */
    "\342\240\231", /* ⠙ */
    "\342\240\271", /* ⠹ */
    "\342\240\270", /* ⠸ */
    "\342\240\274", /* ⠼ */
    "\342\240\264", /* ⠴ */
    "\342\240\246", /* ⠦ */
    "\342\240\247", /* ⠧ */
    "\342\240\207", /* ⠇ */
    "\342\240\217", /* ⠏ */
};

#define SPINNER_FRAME_COUNT 10

const char *tui_spinner_frame(int frame) {
    return SPINNER_FRAMES[frame % SPINNER_FRAME_COUNT];
}
