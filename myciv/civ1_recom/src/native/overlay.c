/*
 * On-screen overlay for features added to the game: short messages and
 * the city screen's "GOLD" button.  Drawn onto the presented frame only,
 * never into game memory, so the game's own drawing is not disturbed.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "native.h"
#include "runtime.h"
#include "addons.h"
#include "video.h"
#include "mp.h"

/* 5x7 font, one byte per row (bit 4 = leftmost pixel) */
static const struct { char c; uint8_t row[7]; } font[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 2, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 17, 25, 21, 19, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {31, 2, 4, 2, 1, 17, 14}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 30, 1, 1, 17, 14}},
    {'6', {6, 8, 16, 30, 17, 17, 14}},   {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 2, 12}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},       {'+', {0, 4, 4, 31, 4, 4, 0}},
    {':', {0, 12, 12, 0, 12, 12, 0}},    {'.', {0, 0, 0, 0, 0, 12, 12}},
    {'!', {4, 4, 4, 4, 4, 0, 4}},        {'(', {2, 4, 8, 8, 8, 4, 2}},
    {')', {8, 4, 2, 2, 2, 4, 8}},        {'/', {1, 1, 2, 4, 8, 16, 16}},
    {'?', {14, 17, 1, 2, 4, 0, 4}},      {',', {0, 0, 0, 0, 12, 4, 8}},
};

static const uint8_t *glyph(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (size_t i = 0; i < sizeof font / sizeof font[0]; i++)
        if (font[i].c == c)
            return font[i].row;
    return NULL;
}

/* palette index closest to an RGB colour (6 bit components) */
static uint8_t nearest(int r, int g, int b)
{
    int best = 0, bd = 1 << 30;
    for (int i = 0; i < 256; i++) {
        int dr = vga_dac[i][0] - r, dg = vga_dac[i][1] - g, db = vga_dac[i][2] - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return (uint8_t)best;
}

static void fill(uint8_t *buf, int W, int H, int x, int y, int w, int h, uint8_t c)
{
    for (int r = y; r < y + h; r++)
        for (int k = x; k < x + w; k++)
            if (r >= 0 && r < H && k >= 0 && k < W)
                buf[r * W + k] = c;
}

static void text(uint8_t *buf, int W, int H, int x, int y, const char *s, uint8_t c)
{
    for (; *s; s++, x += 6) {
        const uint8_t *g = glyph(*s);
        if (!g)
            continue;
        for (int r = 0; r < 7; r++)
            for (int k = 0; k < 5; k++)
                if ((g[r] >> (4 - k)) & 1) {
                    int px = x + k, py = y + r;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        buf[py * W + px] = c;
                }
    }
}

int text_width(const char *s)
{
    return (int)strlen(s) * 6 - 1;
}

/* ---------------------------------------------------------------- */

static char msg[96];
static uint64_t msg_until;

void ovl_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    msg_until = now_us() + 3500000;
    logmsg("%s", msg);
}

int native_overlay_active(void)
{
    return city_screen_city() >= 0 || (msg[0] && now_us() < msg_until) ||
           video_is_map() || mp_overlay_active();
}

static void shadow_text(uint8_t *buf, int W, int H, int x, int y,
                        const char *s, uint8_t fg, uint8_t sh)
{
    text(buf, W, H, x + 1, y + 1, s, sh);
    text(buf, W, H, x, y, s, fg);
}

/* Draw a classic Civ1 3D beveled button (matching f_0d07_703b):
   1px outer dark frame, top/left highlight bevel, bottom/right shadow bevel,
   and centered text with 1px drop shadow. */
static void civ_button(uint8_t *buf, int W, int H, int x, int y, int w, int h,
                       uint8_t face, uint8_t hi, uint8_t lo, uint8_t border,
                       const char *label, uint8_t fg, uint8_t sh)
{
    fill(buf, W, H, x, y, w, h, face);
    /* top and left bevel */
    fill(buf, W, H, x, y, w, 1, hi);
    fill(buf, W, H, x, y, 1, h, hi);
    /* bottom and right bevel */
    fill(buf, W, H, x + 1, y + h - 1, w - 1, 1, lo);
    fill(buf, W, H, x + w - 1, y + 1, 1, h - 1, lo);
    /* subtle dark outer bottom/right drop line */
    fill(buf, W, H, x + 1, y + h, w, 1, border);
    fill(buf, W, H, x + w, y + 1, 1, h, border);
    if (label && label[0]) {
        int tx = x + (w - text_width(label)) / 2;
        int ty = y + (h - 7) / 2;
        if (sh != fg)
            shadow_text(buf, W, H, tx, ty, label, fg, sh);
        else
            text(buf, W, H, tx, ty, label, fg);
    }
}

void native_overlay_draw(uint8_t *buf, int W, int H, int ox, int oy)
{
    uint16_t saved_ds = DS;
    DS = DGROUP;
    uint8_t white = nearest(63, 63, 63), black = nearest(0, 0, 0);
    uint8_t yellow = nearest(63, 55, 0), grey = nearest(22, 22, 22);
    uint8_t lgrey = nearest(42, 42, 42), dgrey = nearest(14, 14, 14);
    uint8_t blue = nearest(16, 20, 56), dblue = nearest(0, 0, 32);
    uint8_t lblue = nearest(34, 42, 63);
    uint8_t green = nearest(0, 36, 0), lgreen = nearest(20, 60, 20);
    uint8_t gold = nearest(46, 34, 0);

    int city = city_screen_city();
    if (city >= 0) {
        int gon = city_has_governor(city);
        civ_button(buf, W, H, ox + GOV_BTN_X, oy + GOV_BTN_Y,
                   GOV_BTN_W, GOV_BTN_H,
                   gon ? green : blue,
                   gon ? lgreen : lblue,
                   gon ? black : dblue,
                   black,
                   gon ? "GOV!" : "GOV",
                   gon ? yellow : white, black);

        int on = city_produces_gold(city);
        civ_button(buf, W, H, ox + GOLD_BTN_X, oy + GOLD_BTN_Y,
                   GOLD_BTN_W, GOLD_BTN_H,
                   on ? gold : blue,
                   on ? yellow : lblue,
                   on ? black : dblue,
                   black,
                   on ? "GOLD!" : "GOLD",
                   on ? yellow : white, black);

        if (on && !city_menu_open()) {
            /* cover old item name/icon between Buy and Change buttons */
            fill(buf, W, H, ox + 231, oy + 98, 85, 8, dblue);
            fill(buf, W, H, ox + 264, oy + 100, 29, 16, dblue);
            shadow_text(buf, W, H, ox + 267, oy + 105, "GOLD", yellow, black);
            /* classic beveled plaque inside the production shields box */
            fill(buf, W, H, ox + 232, oy + 120, 82, 24, dblue);
            fill(buf, W, H, ox + 232, oy + 120, 82, 1, blue);
            fill(buf, W, H, ox + 232, oy + 120, 1, 24, blue);
            fill(buf, W, H, ox + 233, oy + 143, 81, 1, black);
            fill(buf, W, H, ox + 313, oy + 121, 1, 23, black);
            shadow_text(buf, W, H, ox + 232 + (82 - text_width("PRODUCING")) / 2,
                        oy + 123, "PRODUCING", white, black);
            shadow_text(buf, W, H, ox + 232 + (82 - text_width("GOLD")) / 2,
                        oy + 133, "GOLD", yellow, black);
        }
    }
    if (video_is_map() && city < 0) {
        /* active unit XP / Hero badge right above the mode toggle button */
        int au = realtime_active_unit();
        if (au >= 0 && au < 128) {
            char badge[24];
            int is_hero = 0;
            int has = unit_xp_badge((int16_t)GW(0xe710), au, badge, sizeof badge, &is_hero);
            if (has) {
                int bx = ox + RT_BTN_X, by = oy + RT_BTN_Y(H) - 12;
                civ_button(buf, W, H, bx, by, RT_BTN_W, 10,
                           is_hero ? gold : dblue,
                           is_hero ? yellow : lblue,
                           black, black,
                           badge, is_hero ? yellow : white, black);
            }
        }
        /* clickable mode toggle / turn clock at the bottom of the status panel */
        int x = ox + RT_BTN_X, y = oy + RT_BTN_Y(H);
        int w = RT_BTN_W, h = RT_BTN_H;
        if (!realtime_on()) {
            civ_button(buf, W, H, x, y, w, h,
                       lgrey, white, dgrey, black,
                       "TURN-BASED", black, black);
        } else {
            /* sunken bevel frame matching Civ1's stone side panel */
            fill(buf, W, H, x, y, w, h, dblue);
            fill(buf, W, H, x, y, w, 1, dgrey);
            fill(buf, W, H, x, y, 1, h, dgrey);
            fill(buf, W, H, x + 1, y + h - 1, w - 1, 1, white);
            fill(buf, W, H, x + w - 1, y + 1, 1, h - 1, white);
            char t[16];
            int s10 = realtime_turn_secs10();
            if (realtime_paused())
                snprintf(t, sizeof t, "PAUSE");
            else if (s10 == 30)
                snprintf(t, sizeof t, "1X");
            else if (s10 == 15)
                snprintf(t, sizeof t, "2X");
            else
                snprintf(t, sizeof t, "%d.%ds", s10 / 10, s10 % 10);
            int tw = text_width(t) + 4;
            int bw = w - tw - 3;
            int p = realtime_progress() * bw / 1000;
            if (p > 0) {
                fill(buf, W, H, x + 2, y + 2, p, h - 4,
                     realtime_paused() ? grey : yellow);
                fill(buf, W, H, x + 2, y + h - 3, p, 1,
                     realtime_paused() ? dgrey : gold);
            }
            shadow_text(buf, W, H, x + w - tw + 1, y + 2, t, white, black);
        }
    }
    mp_overlay_draw(buf, W, H, ox, oy, fill, text, white, black, yellow, grey);
    if (msg[0] && now_us() < msg_until) {
        int w = text_width(msg) + 10;
        int x = (W - w) / 2, y = 10;
        civ_button(buf, W, H, x, y, w, 11,
                   dblue, lblue, black, black,
                   msg, yellow, black);
    }
    DS = saved_ds;
}
