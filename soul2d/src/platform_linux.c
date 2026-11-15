#define _POSIX_C_SOURCE 199309L
#include "platform.h"
#include "term_ascii.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

typedef struct {
    char ch;
    uint8_t fg;
    uint8_t bg;
} TermCell;

static TermCell term_buf[TERM_H][TERM_W];

static struct termios orig_termios;
static bool term_initialized = false;

static int key_hold_timer[128];
static uint8_t curr_key_down[128];
static uint8_t curr_key_pressed[128];

static const int col_to_ansi_fg[16] = {
    30, 34, 32, 36, 31, 35, 33, 37,
    90, 94, 92, 96, 91, 95, 93, 97
};

static const int col_to_ansi_bg[16] = {
    40, 44, 42, 46, 41, 45, 43, 47,
    100, 104, 102, 106, 101, 105, 103, 107
};

void platform_shutdown(void)
{
    if (term_initialized) {
        const char *restore_seq = "\033[0m\033[?25h\033[?1049l";
        if (write(STDOUT_FILENO, restore_seq, strlen(restore_seq)) < 0) {
        }
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        term_initialized = false;
    }
}

static void handle_sig(int sig)
{
    (void)sig;
    platform_shutdown();
    _exit(0);
}

void platform_init(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        struct termios raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        term_initialized = true;
    }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);
    atexit(platform_shutdown);

    memset(key_hold_timer, 0, sizeof(key_hold_timer));
    memset(curr_key_down, 0, sizeof(curr_key_down));
    memset(curr_key_pressed, 0, sizeof(curr_key_pressed));

    const char *init_seq = "\033[?1049h\033[?25l\033[2J";
    if (write(STDOUT_FILENO, init_seq, strlen(init_seq)) < 0) {
    }
}

static void trigger_key(int sc, int hold_frames)
{
    if (sc < 0 || sc >= 128) return;

    if (sc == SC_W || sc == SC_UP) {
        key_hold_timer[SC_S] = 0;
        key_hold_timer[SC_DOWN] = 0;
    } else if (sc == SC_S || sc == SC_DOWN) {
        key_hold_timer[SC_W] = 0;
        key_hold_timer[SC_UP] = 0;
    } else if (sc == SC_A || sc == SC_LEFT) {
        key_hold_timer[SC_D] = 0;
        key_hold_timer[SC_RIGHT] = 0;
    } else if (sc == SC_D || sc == SC_RIGHT) {
        key_hold_timer[SC_A] = 0;
        key_hold_timer[SC_LEFT] = 0;
    }

    if (key_hold_timer[sc] == 0) {
        curr_key_pressed[sc] = 1;
    }
    key_hold_timer[sc] = hold_frames;
}

void platform_poll_input(void)
{
    uint8_t buf[64];
    ssize_t n;
    int i;

    memset(curr_key_pressed, 0, sizeof(curr_key_pressed));

    for (i = 0; i < 128; i++) {
        if (key_hold_timer[i] > 0) {
            key_hold_timer[i]--;
        }
    }

    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (i = 0; i < (int)n; i++) {
            uint8_t c = buf[i];
            if (c == 27) {
                if (i + 2 < (int)n && buf[i + 1] == '[') {
                    uint8_t seq = buf[i + 2];
                    if (seq == 'A') trigger_key(SC_UP, 8);
                    else if (seq == 'B') trigger_key(SC_DOWN, 8);
                    else if (seq == 'C') trigger_key(SC_RIGHT, 8);
                    else if (seq == 'D') trigger_key(SC_LEFT, 8);
                    i += 2;
                } else {
                    trigger_key(SC_ESC, 2);
                }
                continue;
            }

            switch (c) {
            case 'w': case 'W': trigger_key(SC_W, 8); break;
            case 's': case 'S': trigger_key(SC_S, 8); break;
            case 'a': case 'A': trigger_key(SC_A, 8); break;
            case 'd': case 'D': trigger_key(SC_D, 8); break;
            case 'j': case 'J': trigger_key(SC_J, 22); break;
            case 'k': case 'K':
                curr_key_pressed[SC_K] = 1;
                key_hold_timer[SC_K] = 2;
                break;
            case 'i': case 'I':
                curr_key_pressed[SC_I] = 1;
                key_hold_timer[SC_I] = 2;
                break;
            case 'l': case 'L':
            case ' ':
                curr_key_pressed[SC_L] = 1;
                key_hold_timer[SC_L] = 2;
                break;
            case 'u': case 'U':
                curr_key_pressed[SC_U] = 1;
                key_hold_timer[SC_U] = 2;
                break;
            case '\t':
                curr_key_pressed[SC_TAB] = 1;
                key_hold_timer[SC_TAB] = 2;
                break;
            case 'b': case 'B':
                curr_key_pressed[SC_B] = 1;
                key_hold_timer[SC_B] = 2;
                break;
            case 'r': case 'R':
                curr_key_pressed[SC_R] = 1;
                key_hold_timer[SC_R] = 2;
                break;
            case 'q': case 'Q':
                curr_key_pressed[SC_ESC] = 1;
                key_hold_timer[SC_ESC] = 2;
                break;
            default:
                break;
            }
        }
    }

    for (i = 0; i < 128; i++) {
        curr_key_down[i] = (key_hold_timer[i] > 0) ? 1 : 0;
    }
}

bool platform_key_down(int scancode)
{
    if (scancode < 0 || scancode >= 128) return false;
    return curr_key_down[scancode] != 0;
}

bool platform_key_pressed(int scancode)
{
    if (scancode < 0 || scancode >= 128) return false;
    return curr_key_pressed[scancode] != 0;
}

void platform_wait_frame(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 16666667L;
    nanosleep(&ts, NULL);
}

void term_clear(uint8_t fg, uint8_t bg)
{
    int y, x;
    for (y = 0; y < TERM_H; y++) {
        for (x = 0; x < TERM_W; x++) {
            term_buf[y][x].ch = ' ';
            term_buf[y][x].fg = fg;
            term_buf[y][x].bg = bg;
        }
    }
}

void term_put_cell(int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    if ((unsigned)x >= TERM_W || (unsigned)y >= TERM_H) return;
    term_buf[y][x].ch = ch;
    term_buf[y][x].fg = fg;
    term_buf[y][x].bg = bg;
}

void term_put_str(int x, int y, const char *s, uint8_t fg, uint8_t bg)
{
    if ((unsigned)y >= TERM_H || !s) return;
    while (*s && x < TERM_W) {
        if (x >= 0) {
            term_buf[y][x].ch = *s;
            term_buf[y][x].fg = fg;
            term_buf[y][x].bg = bg;
        }
        x++;
        s++;
    }
}

void term_present(void)
{
    static char out_buf[TERM_H * TERM_W * 16 + 512];
    int pos = 0;
    int y, x;
    int last_fg = -1, last_bg = -1;

    pos += snprintf(out_buf + pos, sizeof(out_buf) - pos, "\033[H");

    for (y = 0; y < TERM_H; y++) {
        if (y > 0) {
            out_buf[pos++] = '\r';
            out_buf[pos++] = '\n';
        }
        for (x = 0; x < TERM_W; x++) {
            uint8_t fg = term_buf[y][x].fg & 0x0F;
            uint8_t bg = term_buf[y][x].bg & 0x0F;
            char ch = term_buf[y][x].ch;
            if ((unsigned char)ch < 32 || (unsigned char)ch > 126) ch = ' ';
            if ((int)fg != last_fg || (int)bg != last_bg) {
                pos += snprintf(out_buf + pos, sizeof(out_buf) - pos,
                                "\033[%d;%dm", col_to_ansi_fg[fg], col_to_ansi_bg[bg]);
                last_fg = (int)fg;
                last_bg = (int)bg;
            }
            out_buf[pos++] = ch;
        }
    }
    pos += snprintf(out_buf + pos, sizeof(out_buf) - pos, "\033[0m");

    if (write(STDOUT_FILENO, out_buf, (size_t)pos) < 0) {
    }
}
