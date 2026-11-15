#define _POSIX_C_SOURCE 199309L
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

Cell screen_buf[SCREEN_H][SCREEN_W];

static struct termios orig_termios;
static bool term_initialized = false;

static int key_hold_timer[128];
static uint8_t curr_key_down[128];
static uint8_t curr_key_pressed[128];

/* Map DOS VGA color index (0..15) to ANSI SGR foreground & background codes */
static const int vga_to_ansi_fg[16] = {
    30, /* 0x00 COL_BLACK        */
    34, /* 0x01 COL_BLUE         */
    32, /* 0x02 COL_GREEN        */
    36, /* 0x03 COL_CYAN         */
    31, /* 0x04 COL_RED          */
    35, /* 0x05 COL_MAGENTA      */
    33, /* 0x06 COL_BROWN        */
    37, /* 0x07 COL_LIGHTGRAY    */
    90, /* 0x08 COL_DARKGRAY     */
    94, /* 0x09 COL_LIGHTBLUE    */
    92, /* 0x0A COL_LIGHTGREEN   */
    96, /* 0x0B COL_LIGHTCYAN    */
    91, /* 0x0C COL_LIGHTRED     */
    95, /* 0x0D COL_LIGHTMAGENTA */
    93, /* 0x0E COL_YELLOW       */
    97  /* 0x0F COL_WHITE        */
};

static const int vga_to_ansi_bg[16] = {
    40,  /* 0x00 COL_BLACK        */
    44,  /* 0x01 COL_BLUE         */
    42,  /* 0x02 COL_GREEN        */
    46,  /* 0x03 COL_CYAN         */
    41,  /* 0x04 COL_RED          */
    45,  /* 0x05 COL_MAGENTA      */
    43,  /* 0x06 COL_BROWN        */
    47,  /* 0x07 COL_LIGHTGRAY    */
    100, /* 0x08 COL_DARKGRAY     */
    104, /* 0x09 COL_LIGHTBLUE    */
    102, /* 0x0A COL_LIGHTGREEN   */
    106, /* 0x0B COL_LIGHTCYAN    */
    101, /* 0x0C COL_LIGHTRED     */
    105, /* 0x0D COL_LIGHTMAGENTA */
    103, /* 0x0E COL_YELLOW       */
    107  /* 0x0F COL_WHITE        */
};

void platform_shutdown(void)
{
    if (term_initialized) {
        const char *restore_seq = "\033[0m\033[?25h\033[?1049l";
        if (write(STDOUT_FILENO, restore_seq, strlen(restore_seq)) < 0) {
            /* ignore write error on shutdown */
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

    /* Enter alternate buffer, hide cursor, clear screen, request 80x50 size */
    const char *init_seq = "\033[?1049h\033[8;50;80t\033[?25l\033[2J";
    if (write(STDOUT_FILENO, init_seq, strlen(init_seq)) < 0) {
        /* ignore */
    }
}

static void trigger_key(int sc, int hold_frames)
{
    if (sc < 0 || sc >= 128) return;

    /* Clear opposing movement keys immediately when changing direction */
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
                /* Check for arrow key escape sequence \033[A..D */
                if (i + 2 < (int)n && buf[i + 1] == '[') {
                    uint8_t seq = buf[i + 2];
                    if (seq == 'A') trigger_key(SC_UP, 6);
                    else if (seq == 'B') trigger_key(SC_DOWN, 6);
                    else if (seq == 'C') trigger_key(SC_RIGHT, 6);
                    else if (seq == 'D') trigger_key(SC_LEFT, 6);
                    i += 2;
                } else {
                    trigger_key(SC_ESC, 2);
                }
                continue;
            }

            switch (c) {
            case 'w': case 'W': trigger_key(SC_W, 6); break;
            case 's': case 'S': trigger_key(SC_S, 6); break;
            case 'a': case 'A': trigger_key(SC_A, 6); break;
            case 'd': case 'D': trigger_key(SC_D, 6); break;
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

void platform_clear(uint8_t attr)
{
    int y, x;
    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            screen_buf[y][x].ch = ' ';
            screen_buf[y][x].attr = attr;
        }
    }
}

void platform_put_cell(int x, int y, uint8_t ch, uint8_t attr)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    screen_buf[y][x].ch = ch;
    screen_buf[y][x].attr = attr;
}

void platform_put_str(int x, int y, const char *s, uint8_t attr)
{
    if (y < 0 || y >= SCREEN_H || !s) return;
    while (*s && x < SCREEN_W) {
        if (x >= 0) {
            screen_buf[y][x].ch = (uint8_t)*s;
            screen_buf[y][x].attr = attr;
        }
        x++;
        s++;
    }
}

/* UTF-8 / ASCII fallback for custom DOS 8x8 tiles (0x80..0x9A) */
static const char *linux_tile_fallback(uint8_t ch)
{
    switch (ch) {
    case TILE_WALL:         return "#";
    case TILE_FLOOR_DOT:    return ".";
    case TILE_BLOCK_FULL:   return "\xE2\x96\x88"; /* █ */
    case TILE_BLOCK_TOP:    return "\xE2\x96\x80"; /* ▀ */
    case TILE_BLOCK_BOT:    return "\xE2\x96\x84"; /* ▄ */
    case TILE_KNIGHT_HELM:  return "@";
    case TILE_KNIGHT_ARMOR: return "#";
    case TILE_KNIGHT_BOOT:  return "m";
    case TILE_SHIELD_MID:   return "[";
    case TILE_SHIELD_EDGE:  return "|";
    case TILE_SPEAR_TIP_U:  return "^";
    case TILE_SPEAR_TIP_R:  return ">";
    case TILE_SPEAR_TIP_D:  return "v";
    case TILE_SPEAR_TIP_L:  return "<";
    case TILE_SHAFT_V:      return "|";
    case TILE_SHAFT_H:      return "-";
    case TILE_SKEL_SKULL:   return "O";
    case TILE_SKEL_RIBS:    return "X";
    case TILE_SKEL_PELVIS:  return "=";
    case TILE_SKEL_LIMB:    return "|";
    case TILE_SWORD_TIP_U:  return "^";
    case TILE_SWORD_TIP_R:  return ">";
    case TILE_SWORD_TIP_D:  return "v";
    case TILE_SWORD_TIP_L:  return "<";
    case TILE_SWORD_HILT:   return "+";
    case TILE_SLASH_ARC:    return "*";
    case TILE_PARRY_STAR:   return "*";
    case TILE_CRIT_SPARK:   return "X";
    case TILE_BONE_SHARD:   return "~";
    default:                return " ";
    }
}

void platform_present(void)
{
    static char out_buf[SCREEN_H * SCREEN_W * 16 + 1024];
    int pos = 0;
    int y, x;
    int last_attr = -1;

    pos += snprintf(out_buf + pos, sizeof(out_buf) - pos, "\033[H");

    for (y = 0; y < SCREEN_H; y++) {
        if (y > 0) {
            out_buf[pos++] = '\r';
            out_buf[pos++] = '\n';
        }
        for (x = 0; x < SCREEN_W; x++) {
            uint8_t attr = screen_buf[y][x].attr;
            uint8_t ch = screen_buf[y][x].ch;
            if ((int)attr != last_attr) {
                int fg = vga_to_ansi_fg[attr & 0x0F];
                int bg = vga_to_ansi_bg[(attr >> 4) & 0x0F];
                pos += snprintf(out_buf + pos, sizeof(out_buf) - pos, "\033[%d;%dm", fg, bg);
                last_attr = (int)attr;
            }
            if (ch >= TILE_BASE && ch < TILE_BASE + TILE_COUNT) {
                const char *u = linux_tile_fallback(ch);
                while (*u) {
                    out_buf[pos++] = *u++;
                }
            } else if (ch < 32 || ch > 126) {
                out_buf[pos++] = ' ';
            } else {
                out_buf[pos++] = (char)ch;
            }
        }
    }
    pos += snprintf(out_buf + pos, sizeof(out_buf) - pos, "\033[0m");

    if (write(STDOUT_FILENO, out_buf, (size_t)pos) < 0) {
        /* ignore */
    }
}

void platform_wait_frame(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 16666667L; /* ~60 FPS (16.67 ms) */
    nanosleep(&ts, NULL);
}
