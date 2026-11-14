/* dosmouse.c - DOS INT 33h mouse driver support for NetHack
 * Copyright (c) 2026 - NetHack modification
 * NetHack may be freely redistributed.  See license for details.
 *
 * Provides mouse input support for the MSDOS/DJGPP build via
 * the standard INT 33h mouse driver interface. The driver polls
 * for mouse button presses and translates pixel coordinates to
 * character cell coordinates for the game's tty windowing system.
 */

#include "hack.h"

#ifdef MSDOS

#include <dos.h>
#include "pcvideo.h"
#include "dosmouse.h"

/* INT 33h function numbers */
#define MOUSE_INT       0x33
#define MOUSE_RESET     0x0000  /* Reset and detect */
#define MOUSE_SHOW      0x0001  /* Show mouse cursor */
#define MOUSE_HIDE      0x0002  /* Hide mouse cursor */
#define MOUSE_GETPOS    0x0003  /* Get position and button status */
#define MOUSE_SETPOS    0x0004  /* Set position */
#define MOUSE_GETPRESS  0x0005  /* Get button press info */
#define MOUSE_SETXRANGE 0x0007  /* Set horizontal range */
#define MOUSE_SETYRANGE 0x0008  /* Set vertical range */

int dos_mouse_available = 0;
static int mouse_visible = 0;

/*
 * Initialize the mouse driver via INT 33h function 0.
 * Returns 1 if a mouse driver is present and a mouse is connected.
 */
int
dos_mouse_init(void)
{
    union REGS regs;

    regs.x.ax = MOUSE_RESET;
    int86(MOUSE_INT, &regs, &regs);

    /* AX = 0xFFFF if driver installed, BX = number of buttons */
    if ((short) regs.x.ax == -1) {
        dos_mouse_available = 1;
        mouse_visible = 0; /* MOUSE_RESET leaves cursor hidden (counter = 1) */
        return 1;
    }

    dos_mouse_available = 0;
    mouse_visible = 0;
    return 0;
}

/*
 * Show the mouse cursor (INT 33h function 1).
 * In text mode this is typically a colored block;
 * in graphics mode it's a small arrow.
 */
void
dos_mouse_show(void)
{
    union REGS regs;

    if (!dos_mouse_available || mouse_visible)
        return;

#ifdef SCREEN_VESA
    if (iflags.grmode && iflags.usevesa) {
        regs.x.ax = MOUSE_GETPOS;
        int86(MOUSE_INT, &regs, &regs);
        vesa_draw_mouse_cursor((int) regs.x.cx, (int) regs.x.dx);
        mouse_visible = 1;
        return;
    }
#endif
    regs.x.ax = MOUSE_SHOW;
    int86(MOUSE_INT, &regs, &regs);
    mouse_visible = 1;
}

/*
 * Hide the mouse cursor (INT 33h function 2).
 * Call before drawing to avoid cursor artifacts.
 */
void
dos_mouse_hide(void)
{
    union REGS regs;

    if (!dos_mouse_available || !mouse_visible)
        return;

#ifdef SCREEN_VESA
    if (iflags.grmode && iflags.usevesa) {
        vesa_hide_mouse_cursor();
        mouse_visible = 0;
        return;
    }
#endif
    regs.x.ax = MOUSE_HIDE;
    int86(MOUSE_INT, &regs, &regs);
    mouse_visible = 0;
}

/*
 * Set the mouse movement range (INT 33h functions 7 and 8).
 * max_x, max_y are in pixel coordinates.
 */
void
dos_mouse_set_range(int max_x, int max_y)
{
    union REGS regs;

    if (!dos_mouse_available)
        return;

    /* Set horizontal range: 0 to max_x */
    regs.x.ax = MOUSE_SETXRANGE;
    regs.x.cx = 0;
    regs.x.dx = max_x;
    int86(MOUSE_INT, &regs, &regs);

    /* Set vertical range: 0 to max_y */
    regs.x.ax = MOUSE_SETYRANGE;
    regs.x.cx = 0;
    regs.x.dx = max_y;
    int86(MOUSE_INT, &regs, &regs);
}

/*
 * Poll for a mouse button press event using INT 33h function 5.
 * Uses function 5 (get button press info) for each button to detect
 * new presses since last call. This avoids missing quick clicks.
 *
 * Returns 1 if a button was pressed, filling in *ev.
 * Returns 0 if no button press occurred.
 */
int
dos_mouse_poll(struct dos_mouse_event *ev)
{
    union REGS regs;
    int btn;

    if (!dos_mouse_available)
        return 0;

#ifdef SCREEN_VESA
    if (mouse_visible && iflags.grmode && iflags.usevesa) {
        regs.x.ax = MOUSE_GETPOS;
        int86(MOUSE_INT, &regs, &regs);
        vesa_draw_mouse_cursor((int) regs.x.cx, (int) regs.x.dx);
    }
#endif

    /* Check left button (button 0) press count */
    for (btn = 0; btn <= 1; btn++) {
        regs.x.ax = MOUSE_GETPRESS;
        regs.x.bx = btn;  /* 0=left, 1=right */
        int86(MOUSE_INT, &regs, &regs);

        if (regs.x.bx > 0) {
            /* A press was detected. regs.cx = X pixel, regs.dx = Y pixel */
            ev->buttons = regs.x.ax;  /* current button state */
            ev->px = regs.x.cx;
            ev->py = regs.x.dx;
            /* In text mode, INT 33h reports virtual pixel coords where
             * each character cell is 8x8 pixels. Convert to cell coords. */
            ev->cx = ev->px / 8;
            ev->cy = ev->py / 8;
            ev->pressed = btn + 1;  /* 1=left, 2=right */
            return 1;
        }
    }

    return 0;
}

#endif /* MSDOS */
