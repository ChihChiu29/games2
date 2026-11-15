#ifndef SOUL2D_PLATFORM_H
#define SOUL2D_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/* Standard 16 base color indices shared by both VGA and ASCII presentation layers */
#define COL_BLACK        0
#define COL_BLUE         1
#define COL_GREEN        2
#define COL_CYAN         3
#define COL_RED          4
#define COL_MAGENTA      5
#define COL_BROWN        6
#define COL_LIGHTGRAY    7
#define COL_DARKGRAY     8
#define COL_LIGHTBLUE    9
#define COL_LIGHTGREEN   10
#define COL_LIGHTCYAN    11
#define COL_LIGHTRED     12
#define COL_LIGHTMAGENTA 13
#define COL_YELLOW       14
#define COL_WHITE        15

/* PC XT Set-1 Scancodes */
#define SC_ESC   0x01
#define SC_TAB   0x0F
#define SC_W     0x11
#define SC_A     0x1E
#define SC_S     0x1F
#define SC_D     0x20
#define SC_B     0x30
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

void platform_init(void);
void platform_shutdown(void);
void platform_poll_input(void);
bool platform_key_down(int scancode);
bool platform_key_pressed(int scancode);
void platform_wait_frame(void);

#endif /* SOUL2D_PLATFORM_H */
