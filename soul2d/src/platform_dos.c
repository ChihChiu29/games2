#include "platform.h"
#include "vga.h"
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/movedata.h>
#include <stdlib.h>
#include <string.h>

static volatile uint8_t raw_key_down[128];
static volatile uint8_t raw_key_latch[128];
static uint8_t prev_key_down[128];
static uint8_t curr_key_down[128];
static uint8_t curr_key_pressed[128];

static _go32_dpmi_seginfo old_kbd_isr;
static _go32_dpmi_seginfo new_kbd_isr;
static bool kbd_installed = false;

static void apply_vga_palette(void)
{
    int i;
    outportb(0x3C8, 0);
    for (i = 0; i < 256; i++) {
        /* VGA DAC uses 6-bit (0..63) RGB components */
        outportb(0x3C9, palette_rgb[i][0] >> 2);
        outportb(0x3C9, palette_rgb[i][1] >> 2);
        outportb(0x3C9, palette_rgb[i][2] >> 2);
    }
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
    uint8_t ctrl = inportb(0x61);
    outportb(0x61, ctrl | 0x80);
    outportb(0x61, ctrl);
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

    vga_init_palette_data();

    /* 1. Enter VGA Mode 13h: 320x200 256-color linear framebuffer at 0xA0000 */
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0013;
    int86(0x10, &r, &r);

    /* 2. Program 256-color VGA palette */
    apply_vga_palette();

    /* 3. Lock memory and install INT 9 hardware keyboard handler */
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

void vga_present(void)
{
    dosmemput(framebuffer, sizeof(framebuffer), 0xA0000);
}

void platform_wait_frame(void)
{
    while (inportb(0x3DA) & 0x08) {
    }
    while (!(inportb(0x3DA) & 0x08)) {
    }
}
