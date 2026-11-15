#include "platform.h"
#include <string.h>
#include <stdlib.h>

uint8_t framebuffer[SCREEN_H][SCREEN_W];
uint8_t palette_rgb[256][3];

static void set_pal(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    palette_rgb[idx][0] = r;
    palette_rgb[idx][1] = g;
    palette_rgb[idx][2] = b;
}

void platform_init_palette_data(void)
{
    memset(palette_rgb, 0, sizeof(palette_rgb));

    /* Standard 16 VGA colors */
    set_pal(COL_BLACK,        10,  10,  14);
    set_pal(COL_BLUE,         28,  56, 148);
    set_pal(COL_GREEN,        32, 140,  56);
    set_pal(COL_CYAN,         36, 144, 156);
    set_pal(COL_RED,         168,  32,  36);
    set_pal(COL_MAGENTA,     148,  36, 148);
    set_pal(COL_BROWN,       148,  88,  32);
    set_pal(COL_LIGHTGRAY,   172, 176, 184);
    set_pal(COL_DARKGRAY,     76,  80,  92);
    set_pal(COL_LIGHTBLUE,    72, 128, 232);
    set_pal(COL_LIGHTGREEN,   80, 220,  96);
    set_pal(COL_LIGHTCYAN,    88, 224, 236);
    set_pal(COL_LIGHTRED,    240,  76,  76);
    set_pal(COL_LIGHTMAGENTA,232,  88, 232);
    set_pal(COL_YELLOW,      248, 224,  72);
    set_pal(COL_WHITE,       248, 248, 252);

    /* Dungeon Floor & Wall Ramps */
    set_pal(PAL_OUTLINE,       6,   6,  10);
    set_pal(PAL_SHADOW,       14,  14,  20);
    set_pal(PAL_FLOOR_DARK,   22,  24,  32);
    set_pal(PAL_FLOOR_MID,    28,  31,  40);
    set_pal(PAL_FLOOR_LIGHT,  38,  42,  54);
    set_pal(PAL_WALL_DARK,    44,  46,  58);
    set_pal(PAL_WALL_MID,     68,  72,  88);
    set_pal(PAL_WALL_LIGHT,   98, 104, 124);

    /* Steel / Armor Ramp */
    set_pal(PAL_STEEL_DARK,   64,  74,  92);
    set_pal(PAL_STEEL_MID,   116, 130, 156);
    set_pal(PAL_STEEL_LIGHT, 176, 192, 216);
    set_pal(PAL_STEEL_SHINE, 236, 244, 255);

    /* Knight Royal Blue Ramp */
    set_pal(PAL_BLUE_DARK,    24,  44, 104);
    set_pal(PAL_BLUE_MID,     44,  84, 176);
    set_pal(PAL_BLUE_LIGHT,   76, 136, 236);
    set_pal(PAL_BLUE_SHINE,  144, 196, 255);

    /* Gold / Brass Ramp */
    set_pal(PAL_GOLD_DARK,   124,  80,  20);
    set_pal(PAL_GOLD_MID,    188, 136,  36);
    set_pal(PAL_GOLD_LIGHT,  244, 198,  64);
    set_pal(PAL_GOLD_SHINE,  255, 240, 148);

    /* Bone / Skeleton Ramp */
    set_pal(PAL_BONE_DARK,    92,  86,  76);
    set_pal(PAL_BONE_MID,    148, 142, 128);
    set_pal(PAL_BONE_LIGHT,  208, 202, 186);
    set_pal(PAL_BONE_SHINE,  248, 244, 232);

    /* Wood / Shaft Ramp */
    set_pal(PAL_WOOD_DARK,    72,  42,  20);
    set_pal(PAL_WOOD_MID,    116,  68,  32);
    set_pal(PAL_WOOD_LIGHT,  164, 102,  50);
    set_pal(PAL_WOOD_SHINE,  208, 142,  80);

    /* Crimson / Hitbox / Plume Ramp */
    set_pal(PAL_CRIMSON_DARK,  96,  16,  24);
    set_pal(PAL_CRIMSON_MID,  168,  28,  36);
    set_pal(PAL_CRIMSON_LIGHT,236,  64,  56);
    set_pal(PAL_CRIMSON_HOT,  255, 164,  92);

    /* Arcane / Heavy Sweep Ramp */
    set_pal(PAL_ARCANE_DARK,   76,  20,  96);
    set_pal(PAL_ARCANE_MID,   140,  40, 168);
    set_pal(PAL_ARCANE_LIGHT, 212,  84, 240);
    set_pal(PAL_ARCANE_HOT,   252, 176, 255);

    /* Emerald / Stamina / Roll Ramp */
    set_pal(PAL_EMERALD_DARK,  20,  84,  48);
    set_pal(PAL_EMERALD_MID,   36, 148,  76);
    set_pal(PAL_EMERALD_LIGHT, 76, 220, 116);
    set_pal(PAL_EMERALD_HOT,  168, 255, 192);
}

void platform_clear(uint8_t col)
{
    memset(framebuffer, col, sizeof(framebuffer));
}

void platform_put_pixel(int x, int y, uint8_t col)
{
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) {
        framebuffer[y][x] = col;
    }
}

void platform_fill_rect(int x, int y, int w, int h, uint8_t col)
{
    int py, px;
    for (py = y; py < y + h; py++) {
        if ((unsigned)py >= SCREEN_H) continue;
        for (px = x; px < x + w; px++) {
            if ((unsigned)px < SCREEN_W) {
                framebuffer[py][px] = col;
            }
        }
    }
}

void platform_draw_rect(int x, int y, int w, int h, uint8_t col)
{
    int i;
    for (i = 0; i < w; i++) {
        platform_put_pixel(x + i, y, col);
        platform_put_pixel(x + i, y + h - 1, col);
    }
    for (i = 0; i < h; i++) {
        platform_put_pixel(x, y + i, col);
        platform_put_pixel(x + w - 1, y + i, col);
    }
}

void platform_draw_line(int x0, int y0, int x1, int y1, uint8_t col)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        platform_put_pixel(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Compact 4x5 pixel font (packed 20 bits per ASCII char 32..95) */
static const uint32_t font4x5[64] = {
    0x00000, /* ' ' */ 0x44404, /* '!' */ 0xA0000, /* '"' */ 0xAEAEA, /* '#' */
    0x6C64C, /* '$' */ 0x92490, /* '%' */ 0x4A4A6, /* '&' */ 0x40000, /* '\''*/
    0x24442, /* '(' */ 0x42224, /* ')' */ 0xA4E4A, /* '*' */ 0x04E40, /* '+' */
    0x00048, /* ',' */ 0x00E00, /* '-' */ 0x00004, /* '.' */ 0x22488, /* '/' */
    0x69996, /* '0' */ 0x4C44E, /* '1' */ 0xE268E, /* '2' */ 0xE262E, /* '3' */
    0xAAEA2, /* '4' */ 0xE8E2E, /* '5' */ 0x68E96, /* '6' */ 0xE2444, /* '7' */
    0x69696, /* '8' */ 0x69726, /* '9' */ 0x04040, /* ':' */ 0x04048, /* ';' */
    0x24842, /* '<' */ 0x0E0E0, /* '=' */ 0x84248, /* '>' */ 0xE2404, /* '?' */
    0x69B86, /* '@' */ 0x69F99, /* 'A' */ 0xE9E9E, /* 'B' */ 0x69896, /* 'C' */
    0xE999E, /* 'D' */ 0xF8E8F, /* 'E' */ 0xF8E88, /* 'F' */ 0x68B96, /* 'G' */
    0x99F99, /* 'H' */ 0xE444E, /* 'I' */ 0x722A4, /* 'J' */ 0x9AC99, /* 'K' */
    0x8888F, /* 'L' */ 0x9F999, /* 'M' */ 0x9DDB9, /* 'N' */ 0x69996, /* 'O' */
    0xE9E88, /* 'P' */ 0x699B7, /* 'Q' */ 0xE9EA9, /* 'R' */ 0x7861E, /* 'S' */
    0xE4444, /* 'T' */ 0x99996, /* 'U' */ 0x999A4, /* 'V' */ 0x999F9, /* 'W' */
    0x99699, /* 'X' */ 0x99644, /* 'Y' */ 0xF248F, /* 'Z' */ 0x64446, /* '[' */
    0x88422, /* '\\'*/ 0x62226, /* ']' */ 0x4A000, /* '^' */ 0x0000F  /* '_' */
};

static void draw_glyph(int x, int y, char c, uint8_t fg, int bg)
{
    int row, col;
    unsigned char uc = (unsigned char)c;
    if (uc >= 'a' && uc <= 'z') uc = (unsigned char)(uc - 32);
    if (uc < 32 || uc > 95) uc = 32;

    uint32_t bits = font4x5[uc - 32];
    if (bg >= 0) {
        platform_fill_rect(x, y, 4, 6, (uint8_t)bg);
    }
    for (row = 0; row < 5; row++) {
        int shift = (4 - row) * 4;
        uint8_t nibble = (uint8_t)((bits >> shift) & 0x0F);
        for (col = 0; col < 4; col++) {
            if (nibble & (0x08 >> col)) {
                platform_put_pixel(x + col, y + row, fg);
            }
        }
    }
}

void platform_draw_str(int x, int y, const char *s, uint8_t fg, int bg)
{
    if (!s) return;
    while (*s) {
        draw_glyph(x, y, *s, fg, bg);
        x += 4;
        s++;
    }
}
