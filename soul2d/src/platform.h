#ifndef SOUL2D_PLATFORM_H
#define SOUL2D_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/* VGA Mode 13h resolution: 320x200, 256 colors */
#define SCREEN_W 320
#define SCREEN_H 200

/* Standard 16 base palette indices (0..15) */
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

/* Extended 256-color palette ramps initialized in platform_init() */
#define PAL_OUTLINE      16  /* Pitch black sprite outline */
#define PAL_SHADOW       17  /* Dark floor drop-shadow */
#define PAL_FLOOR_DARK   18
#define PAL_FLOOR_MID    19
#define PAL_FLOOR_LIGHT  20
#define PAL_WALL_DARK    21
#define PAL_WALL_MID     22
#define PAL_WALL_LIGHT   23

/* Steel / Silver Armor Ramp (24..27: dark -> mid -> light -> shine) */
#define PAL_STEEL_DARK   24
#define PAL_STEEL_MID    25
#define PAL_STEEL_LIGHT  26
#define PAL_STEEL_SHINE  27

/* Knight Royal Blue Ramp (28..31) */
#define PAL_BLUE_DARK    28
#define PAL_BLUE_MID     29
#define PAL_BLUE_LIGHT   30
#define PAL_BLUE_SHINE   31

/* Gold / Brass Ramp (32..35) */
#define PAL_GOLD_DARK    32
#define PAL_GOLD_MID     33
#define PAL_GOLD_LIGHT   34
#define PAL_GOLD_SHINE   35

/* Bone / Skeleton Ramp (36..39) */
#define PAL_BONE_DARK    36
#define PAL_BONE_MID     37
#define PAL_BONE_LIGHT   38
#define PAL_BONE_SHINE   39

/* Wood / Leather Ramp (40..43) */
#define PAL_WOOD_DARK    40
#define PAL_WOOD_MID     41
#define PAL_WOOD_LIGHT   42
#define PAL_WOOD_SHINE   43

/* Crimson / Hitbox / Plume Ramp (44..47) */
#define PAL_CRIMSON_DARK  44
#define PAL_CRIMSON_MID   45
#define PAL_CRIMSON_LIGHT 46
#define PAL_CRIMSON_HOT   47

/* Arcane / Heavy Attack Ramp (48..51) */
#define PAL_ARCANE_DARK   48
#define PAL_ARCANE_MID    49
#define PAL_ARCANE_LIGHT  50
#define PAL_ARCANE_HOT    51

/* Emerald / Stamina / Roll i-frame Ramp (52..55) */
#define PAL_EMERALD_DARK  52
#define PAL_EMERALD_MID   53
#define PAL_EMERALD_LIGHT 54
#define PAL_EMERALD_HOT   55

#define ATTR(fg, bg) ((uint8_t)(((bg) << 4) | ((fg) & 0x0F)))

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

extern uint8_t framebuffer[SCREEN_H][SCREEN_W];
extern uint8_t palette_rgb[256][3]; /* 0..255 R, G, B per color index */

void platform_init_palette_data(void);
void platform_init(void);
void platform_shutdown(void);
void platform_poll_input(void);
bool platform_key_down(int scancode);
bool platform_key_pressed(int scancode);

void platform_clear(uint8_t col);
void platform_put_pixel(int x, int y, uint8_t col);
void platform_fill_rect(int x, int y, int w, int h, uint8_t col);
void platform_draw_rect(int x, int y, int w, int h, uint8_t col);
void platform_draw_line(int x0, int y0, int x1, int y1, uint8_t col);
void platform_draw_str(int x, int y, const char *s, uint8_t fg, int bg);

void platform_present(void);
void platform_wait_frame(void);

#endif /* SOUL2D_PLATFORM_H */
