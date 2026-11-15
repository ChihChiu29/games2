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

    /* 3. Disable blink bit so all 16 background colors work */
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x1003;
    r.h.bl = 0x00;
    int86(0x10, &r, &r);

    /* 4. Hide hardware text cursor */
    memset(&r, 0, sizeof(r));
    r.h.ah = 0x01;
    r.w.cx = 0x2000;
    int86(0x10, &r, &r);

    /* 5. Lock memory and install INT 9 hardware keyboard handler */
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

void platform_put_cell(int x, int y, char ch, uint8_t attr)
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
            screen_buf[y][x].ch = *s;
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
            vga_raw[idx++] = (uint8_t)screen_buf[y][x].ch;
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
