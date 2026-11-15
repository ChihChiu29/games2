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

/* Custom 8x8 Tile IDs (0x80..0x9A):
 * - DOS renders custom 8x8 VGA bitmap glyphs loaded into font RAM.
 * - Linux terminal renders clean UTF-8 / ASCII equivalents. */
#define TILE_BASE         0x80
#define TILE_WALL         0x80
#define TILE_FLOOR_DOT    0x81
#define TILE_BLOCK_FULL   0x82
#define TILE_BLOCK_TOP    0x83
#define TILE_BLOCK_BOT    0x84
#define TILE_KNIGHT_HELM  0x85
#define TILE_KNIGHT_ARMOR 0x86
#define TILE_KNIGHT_BOOT  0x87
#define TILE_SHIELD_MID   0x88
#define TILE_SHIELD_EDGE  0x89
#define TILE_SPEAR_TIP_U  0x8A
#define TILE_SPEAR_TIP_R  0x8B
#define TILE_SPEAR_TIP_D  0x8C
#define TILE_SPEAR_TIP_L  0x8D
#define TILE_SHAFT_V      0x8E
#define TILE_SHAFT_H      0x8F
#define TILE_SKEL_SKULL   0x90
#define TILE_SKEL_RIBS    0x91
#define TILE_SKEL_PELVIS  0x92
#define TILE_SKEL_LIMB    0x93
#define TILE_SWORD_TIP_U  0x94
#define TILE_SWORD_TIP_R  0x95
#define TILE_SWORD_TIP_D  0x96
#define TILE_SWORD_TIP_L  0x97
#define TILE_SWORD_HILT   0x98
#define TILE_SLASH_ARC    0x99
#define TILE_PARRY_STAR   0x9A
#define TILE_CRIT_SPARK   0x9B
#define TILE_BONE_SHARD   0x9C
#define TILE_COUNT        29

/* PC XT Set-1 Scancodes */
#define SC_ESC   0x01
#define SC_TAB   0x0F
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
    uint8_t ch;
    uint8_t attr;
} Cell;

extern Cell screen_buf[SCREEN_H][SCREEN_W];

void platform_init(void);
void platform_shutdown(void);
void platform_poll_input(void);
bool platform_key_down(int scancode);
bool platform_key_pressed(int scancode);
void platform_clear(uint8_t attr);
void platform_put_cell(int x, int y, uint8_t ch, uint8_t attr);
void platform_put_str(int x, int y, const char *s, uint8_t attr);
void platform_present(void);
void platform_wait_frame(void);

#endif /* SOUL2D_PLATFORM_H */
