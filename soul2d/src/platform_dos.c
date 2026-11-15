#include "platform.h"
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/movedata.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

Cell screen_buf[SCREEN_H][SCREEN_W];
static uint8_t vga_raw[SCREEN_H * SCREEN_W * 2];

static volatile uint8_t raw_key_down[128];
static volatile uint8_t raw_key_latch[128];
static uint8_t prev_key_down[128];
static uint8_t curr_key_down[128];
static uint8_t curr_key_pressed[128];

static _go32_dpmi_seginfo old_kbd_isr;
static _go32_dpmi_seginfo new_kbd_isr;
static bool kbd_installed = false;

/* Custom 8x8 VGA bitmap tileset for 0x80..0x9A (27 glyphs * 8 bytes) */
static const uint8_t custom_tiles_8x8[TILE_COUNT][8] = {
    /* 0x80 TILE_WALL: Brick stone wall */
    { 0xFE, 0xFE, 0xFE, 0x00, 0xEF, 0xEF, 0xEF, 0x00 },
    /* 0x81 TILE_FLOOR_DOT: Subtle stone pebble/crack */
    { 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00 },
    /* 0x82 TILE_BLOCK_FULL: Solid block */
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    /* 0x83 TILE_BLOCK_TOP: Upper half block */
    { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 },
    /* 0x84 TILE_BLOCK_BOT: Lower half block */
    { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF },
    /* 0x85 TILE_KNIGHT_HELM: Knight helmet with T-visor & crest */
    { 0x18, 0x3C, 0x7E, 0xFF, 0xDB, 0xE7, 0xE7, 0x7E },
    /* 0x86 TILE_KNIGHT_ARMOR: Plate breastplate with pauldrons */
    { 0x7E, 0xFF, 0xFF, 0xDB, 0xFF, 0x7E, 0x7E, 0x3C },
    /* 0x87 TILE_KNIGHT_BOOT: Armored greave/boot */
    { 0x3C, 0x3C, 0x3C, 0x3C, 0x7E, 0x7E, 0xFF, 0x00 },
    /* 0x88 TILE_SHIELD_MID: Tower shield with cross emblem */
    { 0xFF, 0xDB, 0xDB, 0x81, 0xDB, 0xDB, 0x7E, 0x3C },
    /* 0x89 TILE_SHIELD_EDGE: Shield rim */
    { 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00 },
    /* 0x8A TILE_SPEAR_TIP_U: Spearhead pointing UP */
    { 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x18, 0x18 },
    /* 0x8B TILE_SPEAR_TIP_R: Spearhead pointing RIGHT */
    { 0x00, 0x30, 0x78, 0xFE, 0xFE, 0x78, 0x30, 0x00 },
    /* 0x8C TILE_SPEAR_TIP_D: Spearhead pointing DOWN */
    { 0x18, 0x18, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18 },
    /* 0x8D TILE_SPEAR_TIP_L: Spearhead pointing LEFT */
    { 0x00, 0x0C, 0x1E, 0x7F, 0x7F, 0x1E, 0x0C, 0x00 },
    /* 0x8E TILE_SHAFT_V: Vertical shaft */
    { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 },
    /* 0x8F TILE_SHAFT_H: Horizontal shaft */
    { 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00 },
    /* 0x90 TILE_SKEL_SKULL: Skeleton skull with eye sockets & teeth */
    { 0x3C, 0x7E, 0xFF, 0x99, 0xFF, 0xE7, 0x7E, 0x54 },
    /* 0x91 TILE_SKEL_RIBS: Skeleton spine + curved ribcage */
    { 0x18, 0x7E, 0x18, 0x7E, 0x18, 0x7E, 0x18, 0x3C },
    /* 0x92 TILE_SKEL_PELVIS: Skeleton pelvis */
    { 0x00, 0x7E, 0xFF, 0xE7, 0x66, 0x00, 0x00, 0x00 },
    /* 0x93 TILE_SKEL_LIMB: Bone limb with joint knobs */
    { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C },
    /* 0x94 TILE_SWORD_TIP_U: Broadsword tip UP */
    { 0x18, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C },
    /* 0x95 TILE_SWORD_TIP_R: Broadsword tip RIGHT */
    { 0x00, 0x00, 0xFC, 0xFE, 0xFE, 0xFC, 0x00, 0x00 },
    /* 0x96 TILE_SWORD_TIP_D: Broadsword tip DOWN */
    { 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x18 },
    /* 0x97 TILE_SWORD_TIP_L: Broadsword tip LEFT */
    { 0x00, 0x00, 0x3F, 0x7F, 0x7F, 0x3F, 0x00, 0x00 },
    /* 0x98 TILE_SWORD_HILT: Sword crossguard & pommel */
    { 0x18, 0xFF, 0xFF, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    /* 0x99 TILE_SLASH_ARC: Bright slash spark */
    { 0x81, 0x42, 0x3C, 0x7E, 0x7E, 0x3C, 0x42, 0x81 },
    /* 0x9A TILE_PARRY_STAR: 4-point parry/stagger star */
    { 0x18, 0x18, 0x3C, 0xFF, 0xFF, 0x3C, 0x18, 0x18 },
    /* 0x9B TILE_CRIT_SPARK: Critical impact burst */
    { 0x99, 0x5A, 0x3C, 0xFF, 0xFF, 0x3C, 0x5A, 0x99 },
    /* 0x9C TILE_BONE_SHARD: Flying bone fragment */
    { 0x60, 0xF0, 0x78, 0x3C, 0x1E, 0x0F, 0x06, 0x00 }
};

static void load_custom_vga_tiles(void)
{
    __dpmi_regs r;
    dosmemput(custom_tiles_8x8, sizeof(custom_tiles_8x8), __tb);

    memset(&r, 0, sizeof(r));
    r.x.ax = 0x1100;
    r.h.bh = 8;
    r.h.bl = 0;
    r.x.cx = TILE_COUNT;
    r.x.dx = TILE_BASE;
    r.x.es = (unsigned short)(__tb >> 4);
    r.x.bp = (unsigned short)(__tb & 0x0F);
    __dpmi_int(0x10, &r);
}

static void kbd_isr(void)
{
    uint8_t sc = inportb(0x60);
    if (sc != 0xE0 && sc != 0xE1) {
        uint8_t code = sc & 0x7F;
        if (sc & 0x80) {
            raw_key_down[code] = 0;
        } else {
            if (!raw_key_down[code]) {
                raw_key_latch[code] = 1;
            }
            raw_key_down[code] = 1;
        }
    }
    /* Toggle keyboard controller acknowledge bit (XT/AT compatibility) */
    uint8_t ctrl = inportb(0x61);
    outportb(0x61, ctrl | 0x80);
    outportb(0x61, ctrl);
    /* Send EOI to Master PIC */
    outportb(0x20, 0x20);
}
static void kbd_isr_end(void) {}

void platform_shutdown(void)
{
    if (kbd_installed) {
        _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd_isr);
        _go32_dpmi_free_iret_wrapper(&new_kbd_isr);
        kbd_installed = false;
    }

    /* Restore standard 80x25 text mode */
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0003;
    int86(0x10, &r, &r);
}

void platform_init(void)
{
    union REGS r;

    /* 1. Set 80x25 16-color text mode */
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0003;
    int86(0x10, &r, &r);

    /* 2. Load 8x8 ROM font into character generator to enable 80x50 square-cell mode */
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x1112;
    r.h.bl = 0x00;
    int86(0x10, &r, &r);

    /* 3. Load custom 8x8 pixel-art tileset into slots 0x80..0x9A */
    load_custom_vga_tiles();

    /* 4. Disable blink bit so all 16 background colors work */
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x1003;
    r.h.bl = 0x00;
    int86(0x10, &r, &r);

    /* 5. Hide hardware text cursor */
    memset(&r, 0, sizeof(r));
    r.h.ah = 0x01;
    r.w.cx = 0x2000;
    int86(0x10, &r, &r);

    /* 6. Lock memory and install INT 9 hardware keyboard handler */
    memset((void *)raw_key_down, 0, sizeof(raw_key_down));
    memset((void *)raw_key_latch, 0, sizeof(raw_key_latch));
    memset(prev_key_down, 0, sizeof(prev_key_down));
    memset(curr_key_down, 0, sizeof(curr_key_down));
    memset(curr_key_pressed, 0, sizeof(curr_key_pressed));

    _go32_dpmi_lock_data((void *)raw_key_down, sizeof(raw_key_down));
    _go32_dpmi_lock_data((void *)raw_key_latch, sizeof(raw_key_latch));
    _go32_dpmi_lock_code(kbd_isr, (unsigned long)((char *)kbd_isr_end - (char *)kbd_isr));

    _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd_isr);
    new_kbd_isr.pm_offset = (unsigned long)kbd_isr;
    new_kbd_isr.pm_selector = _go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&new_kbd_isr);
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &new_kbd_isr);
    kbd_installed = true;

    atexit(platform_shutdown);
}

void platform_poll_input(void)
{
    int i;
    for (i = 0; i < 128; i++) {
        uint8_t down = raw_key_down[i];
        uint8_t latched = raw_key_latch[i];
        raw_key_latch[i] = 0;

        curr_key_down[i] = down | latched;
        curr_key_pressed[i] = (down && !prev_key_down[i]) || latched;
        prev_key_down[i] = down;
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

void platform_present(void)
{
    int y, x, idx = 0;
    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            vga_raw[idx++] = screen_buf[y][x].ch;
            vga_raw[idx++] = screen_buf[y][x].attr;
        }
    }
    dosmemput(vga_raw, sizeof(vga_raw), 0xB8000);
}

void platform_wait_frame(void)
{
    /* Wait for VGA vertical retrace (~60Hz..70Hz) */
    while (inportb(0x3DA) & 0x08) {
        /* Wait for current retrace to end */
    }
    while (!(inportb(0x3DA) & 0x08)) {
        /* Wait for next vertical retrace to begin */
    }
}
