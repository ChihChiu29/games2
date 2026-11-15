#ifndef SOUL2D_PLATFORM_H
#define SOUL2D_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 80
#define SCREEN_H 50

/* Standard 16 VGA text colors */
#define COL_BLACK        0x00
#define COL_BLUE         0x01
#define COL_GREEN        0x02
#define COL_CYAN         0x03
#define COL_RED          0x04
#define COL_MAGENTA      0x05
#define COL_BROWN        0x06
#define COL_LIGHTGRAY    0x07
#define COL_DARKGRAY     0x08
#define COL_LIGHTBLUE    0x09
#define COL_LIGHTGREEN   0x0A
#define COL_LIGHTCYAN    0x0B
#define COL_LIGHTRED     0x0C
#define COL_LIGHTMAGENTA 0x0D
#define COL_YELLOW       0x0E
#define COL_WHITE        0x0F

#define ATTR(fg, bg) ((uint8_t)(((bg) << 4) | ((fg) & 0x0F)))

/* PC XT Set-1 Scancodes */
#define SC_ESC   0x01
#define SC_W     0x11
#define SC_A     0x1E
#define SC_S     0x1F
#define SC_D     0x20
#define SC_R     0x13
#define SC_U     0x16
#define SC_I     0x17
#define SC_J     0x24
#define SC_K     0x25
#define SC_L     0x26
#define SC_SPACE 0x39
#define SC_UP    0x48
#define SC_LEFT  0x4B
#define SC_RIGHT 0x4D
#define SC_DOWN  0x50

typedef struct {
    char ch;
    uint8_t attr;
} Cell;

extern Cell screen_buf[SCREEN_H][SCREEN_W];

void platform_init(void);
void platform_shutdown(void);
void platform_poll_input(void);
bool platform_key_down(int scancode);
bool platform_key_pressed(int scancode);
void platform_clear(uint8_t attr);
void platform_put_cell(int x, int y, char ch, uint8_t attr);
void platform_put_str(int x, int y, const char *s, uint8_t attr);
void platform_present(void);
void platform_wait_frame(void);

#endif /* SOUL2D_PLATFORM_H */
