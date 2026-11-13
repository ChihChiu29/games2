/* dosmouse.h - DOS INT 33h mouse driver support for NetHack
 * Copyright (c) 2026 - NetHack modification
 * NetHack may be freely redistributed.  See license for details.
 */

#ifndef DOSMOUSE_H
#define DOSMOUSE_H

#ifdef MSDOS

/* Mouse button masks from INT 33h */
#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

/* Mouse event structure */
struct dos_mouse_event {
    int buttons;   /* button state bitmask */
    int px, py;    /* pixel coordinates */
    int cx, cy;    /* character cell coordinates (px/8, py/8 for text mode) */
    int pressed;   /* which button was newly pressed (0=none,1=left,2=right) */
};

/* Function prototypes */
extern int  dos_mouse_init(void);      /* returns 1 if mouse driver found */
extern void dos_mouse_show(void);      /* show mouse cursor */
extern void dos_mouse_hide(void);      /* hide mouse cursor */
extern int  dos_mouse_poll(struct dos_mouse_event *ev);
                                       /* poll for click; returns 1 if event */
extern void dos_mouse_set_range(int max_x, int max_y);
                                       /* set mouse movement range */

extern int dos_mouse_available;        /* 1 if mouse driver is present */

#endif /* MSDOS */

#endif /* DOSMOUSE_H */
