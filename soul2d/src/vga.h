#ifndef SOUL2D_VGA_H
#define SOUL2D_VGA_H

#include "platform.h"

#define VGA_W 320
#define VGA_H 200

/* Extended 256-color palette ramps */
#define PAL_OUTLINE       16
#define PAL_SHADOW        17
#define PAL_FLOOR_DARK    18
#define PAL_FLOOR_MID     19
#define PAL_FLOOR_LIGHT   20
#define PAL_WALL_DARK     21
#define PAL_WALL_MID      22
#define PAL_WALL_LIGHT    23

#define PAL_STEEL_DARK    24
#define PAL_STEEL_MID     25
#define PAL_STEEL_LIGHT   26
#define PAL_STEEL_SHINE   27

#define PAL_BLUE_DARK     28
#define PAL_BLUE_MID      29
#define PAL_BLUE_LIGHT    30
#define PAL_BLUE_SHINE    31

#define PAL_GOLD_DARK     32
#define PAL_GOLD_MID      33
#define PAL_GOLD_LIGHT    34
#define PAL_GOLD_SHINE    35

#define PAL_BONE_DARK     36
#define PAL_BONE_MID      37
#define PAL_BONE_LIGHT    38
#define PAL_BONE_SHINE    39

#define PAL_WOOD_DARK     40
#define PAL_WOOD_MID      41
#define PAL_WOOD_LIGHT    42
#define PAL_WOOD_SHINE    43

#define PAL_CRIMSON_DARK  44
#define PAL_CRIMSON_MID   45
#define PAL_CRIMSON_LIGHT 46
#define PAL_CRIMSON_HOT   47

#define PAL_ARCANE_DARK   48
#define PAL_ARCANE_MID    49
#define PAL_ARCANE_LIGHT  50
#define PAL_ARCANE_HOT    51

#define PAL_EMERALD_DARK  52
#define PAL_EMERALD_MID   53
#define PAL_EMERALD_LIGHT 54
#define PAL_EMERALD_HOT   55

extern uint8_t framebuffer[VGA_H][VGA_W];
extern uint8_t palette_rgb[256][3];

void vga_init_palette_data(void);
void vga_clear(uint8_t col);
void vga_put_pixel(int x, int y, uint8_t col);
void vga_fill_rect(int x, int y, int w, int h, uint8_t col);
void vga_draw_rect(int x, int y, int w, int h, uint8_t col);
void vga_draw_line(int x0, int y0, int x1, int y1, uint8_t col);
void vga_draw_str(int x, int y, const char *s, uint8_t fg, int bg);
void vga_present(void);

#endif /* SOUL2D_VGA_H */
