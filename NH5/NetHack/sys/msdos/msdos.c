/* $NHDT-Header: NetHack 5.0 msdos.c 2026/07/10_13:08:55 NetHack-5.0:1.32 $ */
/* Copyright (c) Michael Allison 2002 */
/* Copyright (c) NetHack PC Development Team 1990 */
/* NetHack may be freely redistributed.  See license for details.         */

/*
 *  MSDOS system functions.
 *  Many thanks to Don Kneller who originated the DOS port and
 *  contributed much to the cause.
 */

#define NEED_VARARGS
#include "hack.h"

#ifdef MSDOS
#include "pcvideo.h"
#include "wintty.h"
#include "dosmouse.h"

#include <dos.h>

/*
 * MS-DOS functions
 */
#define DIRECT_INPUT 0x07 /* Unfiltered Character Input Without Echo */
#define FATINFO 0x1B      /* Get Default Drive Data */
/* MS-DOS 2.0+: */
#define GETDTA 0x2F        /* Get DTA Address */
#define FREESPACE 0x36     /* Get Drive Allocation Info */
#define GETSWITCHAR 0x3700 /* Get Switch Character */
#define FINDFIRST 0x4E     /* Find First File */
#define FINDNEXT 0x4F      /* Find Next File */
#define SETFILETIME 0x5701 /* Set File Date & Time */
                           /*
                            * BIOS interrupts
                            */
#ifdef PC9800
#define KEYBRD_BIOS 0x18
#else
#define KEYBRD_BIOS 0x16
#endif

/*
 * Keyboard BIOS functions
 */
#define READCHAR 0x00       /* Read Character from Keyboard */
#define GETKEYFLAGS 0x02    /* Get Keyboard Flags */
/*#define KEY_DEBUG */      /* print values of unexpected key codes - devel*/

void get_cursor(int *, int *);

/* direct bios calls are used only when iflags.BIOS is set */

static char DOSgetch(void);
static char BIOSgetch(void);
/* static long freediskspace(char *path); */
unsigned long sys_random_seed(void);

#ifndef __GO32__
static char *getdta(void);
#endif
static unsigned int dos_ioctl(int, int, unsigned);
#ifdef NO_TERMS
extern boolean pckeys(unsigned char, unsigned char); /* pckeys.c */
#endif

#ifdef TTY_GRAPHICS
int
tgetch(void)
{
    char ch;
    struct dos_mouse_event mev;

#ifdef SCREEN_VESA
    if (iflags.usevesa) {
        vesa_flush_text();
    }
#endif /*SCREEN_VESA*/
/* BIOSgetch can use the numeric key pad on IBM compatibles. */
#ifdef SIMULATE_CURSOR
    if (iflags.grmode && cursor_flag)
        DrawCursor();
#endif
    if (dos_mouse_available) {
        dos_mouse_show();
        for (;;) {
            if (kbhit()) {
                if (iflags.BIOS)
                    ch = BIOSgetch();
                else
                    ch = DOSgetch();
                break;
            }
            if (dos_mouse_poll(&mev)) {
                int mcx = mev.cx;
                int mcy = mev.cy;
                int mch;

#ifdef SCREEN_VESA
                if (iflags.grmode && iflags.usevesa) {
                    vesa_mouse_to_cell(mev.px, mev.py, &mcx, &mcy);
                } else
#endif
#ifdef SCREEN_VGA
                if (iflags.grmode) {
                    mcx = mev.px / 8;
                    mcy = mev.py / 16;
                }
#endif
                mch = tty_mouse_menu_click(mcx, mcy, mev.pressed);
                if (mch > 0) {
                    ch = (char) mch;
                    break;
                } else if (mch == 0) {
                    /* Clicked non-selectable header in menu; keep waiting */
                    continue;
                }
                /* Not in a menu (mch < 0): if typing a line (getlin/askname),
                 * ignore clicks except right-click to cancel; otherwise
                 * left-click acts as space (--More-- / text window) and
                 * right-click acts as ESC. */
                if (ttyDisplay && ttyDisplay->inread > 0) {
                    if (mev.pressed == 2) {
                        ch = '\033';
                        break;
                    }
                    continue;
                }
                ch = (mev.pressed == 1) ? ' ' : '\033';
                break;
            }
        }
        dos_mouse_hide();
    } else {
        if (iflags.BIOS)
            ch = BIOSgetch();
        else
            ch = DOSgetch();
    }
#ifdef SIMULATE_CURSOR
    if (iflags.grmode && cursor_flag)
        HideCursor();
#endif
    return ((ch == '\r') ? '\n' : ch);
}
#endif /* TTY_GRAPHICS */

/*
 *  Keyboard translation tables.
 */
#ifdef PC9800
#define KEYPADLO 0x38
#define KEYPADHI 0x50
#else
#define KEYPADLO 0x47
#define KEYPADHI 0x53
#endif

#define PADKEYS (KEYPADHI - KEYPADLO + 1)
#define iskeypad(x) (KEYPADLO <= (x) && (x) <= KEYPADHI)

/*
 * Keypad keys are translated to the normal values below.
 * When iflags.BIOS is active, shifted keypad keys are translated to the
 *    shift values below.
 */
static const struct pad {
    char normal, shift, cntrl;
} keypad[PADKEYS] =
    {
#ifdef PC9800
      { '>', '>', '>' },       /* Ins */
      { '<', '<', '<' },       /* Del */
      { 'k', 'K', C('k') },    /* Up */
      { 'h', 'H', C('h') },    /* Left */
      { 'l', 'L', C('l') },    /* Right */
      { 'j', 'J', C('j') },    /* Down */
      { 0, 0, 0 },             /* HomeClr */
      { '?', '?', '?' },       /* Help */
      { 'm', C('p'), C('p') }, /* - */
      { '/', '/', '/' },       /* / */
      { 'y', 'Y', C('y') },    /* 7 */
      { 'k', 'K', C('k') },    /* 8 */
      { 'u', 'U', C('u') },    /* 9 */
      { '*', '*', '*' },       /* * */
      { 'h', 'H', C('h') },    /* 4 */
      { 'g', 'g', 'g' },       /* 5 */
      { 'l', 'L', C('l') },    /* 6 */
      { 'p', 'P', C('p') },    /* + */
      { 'b', 'B', C('b') },    /* 1 */
      { 'j', 'J', C('j') },    /* 2 */
      { 'n', 'N', C('n') },    /* 3 */
      { '=', '=', '=' },       /* = */
      { 'i', 'I', C('i') },    /* 0 */
      { ',', ':', ':' },       /* , */
      { '.', '.', '.' }        /* . */
#else
      { 'y', 'Y', C('y') },    /* 7 */
      { 'k', 'K', C('k') },    /* 8 */
      { 'u', 'U', C('u') },    /* 9 */
      { 'm', C('p'), C('p') }, /* - */
      { 'h', 'H', C('h') },    /* 4 */
      { 'g', 'g', 'g' },       /* 5 */
      { 'l', 'L', C('l') },    /* 6 */
      { 'p', 'P', C('p') },    /* + */
      { 'b', 'B', C('b') },    /* 1 */
      { 'j', 'J', C('j') },    /* 2 */
      { 'n', 'N', C('n') },    /* 3 */
      { 'i', 'I', C('i') },    /* Ins */
      { '.', ':', ':' }        /* Del */
#endif
    },
  numpad[PADKEYS] = {
#ifdef PC9800
      { '>', '>', '>' },       /* Ins */
      { '<', '<', '<' },       /* Del */
      { '8', M('8'), '8' },    /* Up */
      { '4', M('4'), '4' },    /* Left */
      { '6', M('6'), '6' },    /* Right */
      { '2', M('2'), '2' },    /* Down */
      { 0, 0, 0 },             /* HomeClr */
      { '?', '?', '?' },       /* Help */
      { 'm', C('p'), C('p') }, /* - */
      { '/', '/', '/' },       /* / */
      { '7', M('7'), '7' },    /* 7 */
      { '8', M('8'), '8' },    /* 8 */
      { '9', M('9'), '9' },    /* 9 */
      { '*', '*', '*' },       /* * */
      { '4', M('4'), '4' },    /* 4 */
      { 'g', 'G', 'g' },       /* 5 */
      { '6', M('6'), '6' },    /* 6 */
      { 'p', 'P', C('p') },    /* + */
      { '1', M('1'), '1' },    /* 1 */
      { '2', M('2'), '2' },    /* 2 */
      { '3', M('3'), '3' },    /* 3 */
      { '=', '=', '=' },       /* = */
      { 'i', 'I', C('i') },    /* 0 */
      { ',', ':', ':' },       /* , */
      { '.', '.', '.' }        /* . */
#else
      { '7', M('7'), '7' },    /* 7 */
      { '8', M('8'), '8' },    /* 8 */
      { '9', M('9'), '9' },    /* 9 */
      { 'm', C('p'), C('p') }, /* - */
      { '4', M('4'), '4' },    /* 4 */
      { '5', M('5'), '5' },    /* 5 */
      { '6', M('6'), '6' },    /* 6 */
      { 'p', 'P', C('p') },    /* + */
      { '1', M('1'), '1' },    /* 1 */
      { '2', M('2'), '2' },    /* 2 */
      { '3', M('3'), '3' },    /* 3 */
      { '0', M('0'), '0' },    /* Ins */
      { '.', ':', ':' }        /* Del */
#endif
  };

/*
 * Unlike Ctrl-letter, the Alt-letter keystrokes have no specific ASCII
 * meaning unless assigned one by a keyboard conversion table, so the
 * keyboard BIOS normally does not return a character code when Alt-letter
 * is pressed.  So, to interpret unassigned Alt-letters, we must use a
 * scan code table to translate the scan code into a letter, then set the
 * "meta" bit for it.  -3.
 */
#ifdef PC9800
#define SCANLO 0x5
#else
#define SCANLO 0x10
#endif /* PC9800 */

static const char scanmap[] = {
    /* ... */
#ifdef PC9800
    0, 0, 0, 0, 0, 0, '-', '^', '\\', '\b', '\t', 'q', 'w', 'e', 'r', 't',
    'y', 'u', 'i', 'o', 'p', '@', '[', '\n', 'a', 's', 'd', 'f', 'g', 'h',
    'j', 'k', 'l', ';', ':', ']', 'z', 'x', 'c', 'v', 'b', 'N', 'm', ',', '.',
    '/' /* ... */
#else
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a',
    's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x',
    'c', 'v', 'b', 'n', 'm', ',', '.', '?' /* ... */
#endif /* PC9800 */
};

#define inmap(x) (SCANLO <= (x) && (x) < SCANLO + SIZE(scanmap))

#ifdef NEW_ALT
#define NUMERIC_SCANLO 0x78
static const char numeric_scanmap[] = { /* ... */
                                        '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', '0', '-', '='
};
#define in_numericmap(x) \
    (NUMERIC_SCANLO <= (x) && (x) < NUMERIC_SCANLO + SIZE(numeric_scanmap))
#endif

/*
 * BIOSgetch gets keys directly with a BIOS call.
 */
#ifdef PC9800
#define SHIFT 0x1
#define KANA 0x4
#define GRPH 0x8
#define CTRL 0x10
#else
#define SHIFT (0x1 | 0x2)
#define CTRL 0x4
#define ALT 0x8
#endif /* PC9800 */

static char
BIOSgetch(void)
{
    unsigned char scan, shift, ch = 0;
    const struct pad *kpad;
    union REGS regs;

    do {
        /* Get scan code.
         */
        regs.h.ah = READCHAR;
        int86(KEYBRD_BIOS, &regs, &regs);
        ch = regs.h.al;
        scan = regs.h.ah;
        /* Get shift status.
         */
        regs.h.ah = GETKEYFLAGS;
        int86(KEYBRD_BIOS, &regs, &regs);
        shift = regs.h.al;

        /* Translate keypad keys */
        if (iskeypad(scan)) {
            kpad = iflags.num_pad ? numpad : keypad;
            if (shift & SHIFT)
                ch = kpad[scan - KEYPADLO].shift;
            else if (shift & CTRL)
                ch = kpad[scan - KEYPADLO].cntrl;
            else
                ch = kpad[scan - KEYPADLO].normal;
        }
#ifdef NO_TERMS
        /* Check for special interface manipulation keys */
        if (pckeys(scan, shift)) {
            ch = 0xFF;
            continue;
        }
#endif
/* Translate unassigned Alt-letters */
#ifdef PC9800
        if (shift & KANA)
            return 0;
        if ((shift & GRPH) && (ch >= 0x80)) {
#else
        if ((shift & ALT) && !ch) {
#endif
#if 0
            pline("Scan code: %d 0x%03X", scan, scan);
#endif
            if (inmap(scan))
                ch = scanmap[scan - SCANLO];
#ifdef NEW_ALT
            else if (in_numericmap(scan))
                ch = numeric_scanmap[scan - NUMERIC_SCANLO];
#endif
            return (isprint(ch) ? M(ch) : ch);
        }
    } while (ch == 0xFF);
    return ch;
}

static char
DOSgetch(void)
{
    union REGS regs;
    char ch;
    struct pad(*kpad)[PADKEYS];

    regs.h.ah = DIRECT_INPUT;
    intdos(&regs, &regs);
    ch = regs.h.al;

#ifdef PC9800
    if (ch < 0) /* KANA letters and GRPH-shifted letters(?) */
        ch = 0; /* munch it */
#else
    /*
     * The extended codes for Alt-shifted letters, and unshifted keypad
     * and function keys, correspond to the scan codes.  So we can still
     * translate the unshifted cursor keys and Alt-letters.  -3.
     */
    if (ch == 0) { /* an extended key */
        regs.h.ah = DIRECT_INPUT;
        intdos(&regs, &regs); /* get the extended key code */
        ch = regs.h.al;

        if (iskeypad(ch)) { /* unshifted keypad keys */
            kpad = (void *) (iflags.num_pad ? numpad : keypad);
            ch = (*kpad)[ch - KEYPADLO].normal;
        } else if (inmap(ch)) { /* Alt-letters */
            ch = scanmap[ch - SCANLO];
            if (isprint(ch))
                ch = M(ch);
        } else
            ch = 0; /* munch it */
    }
#endif
    return (ch);
}

#ifdef TTY_GRAPHICS
/*
 * dos_poskey: combined keyboard+mouse input for DOS.
 * Returns the character if keyboard input, or 0 if a mouse click occurred.
 * When returning 0, *x and *y contain map coordinates and *mod contains
 * CLICK_1 (left) or CLICK_2 (right).
 *
 * Uses kbhit() to check for keyboard readiness without blocking,
 * then polls INT 33h for mouse clicks.  If neither is ready, it busy-waits.
 * Unlike tgetch()/BIOSgetch(), this does NOT block on special keys consumed
 * by pckeys() — it returns to the polling loop to keep checking the mouse.
 */
int
dos_poskey(coordxy *x, coordxy *y, int *mod)
{
    struct dos_mouse_event mev;

    /* Flush VESA text buffer before waiting for input */
#ifdef SCREEN_VESA
    if (iflags.usevesa) {
        vesa_flush_text();
    }
#endif

#ifdef SIMULATE_CURSOR
    if (iflags.grmode && cursor_flag)
        DrawCursor();
#endif

    if (dos_mouse_available)
        dos_mouse_show();

    for (;;) {
        /* Check keyboard first (non-blocking) */
        if (kbhit()) {
            unsigned char scan, shift, ch;
            const struct pad *kpad;
            union REGS regs;

            if (iflags.BIOS) {
                /* Inline non-blocking version of BIOSgetch():
                 * Read one key event and return, even if pckeys() eats it.
                 * The outer for(;;) loop will retry. */
                regs.h.ah = READCHAR;
                int86(KEYBRD_BIOS, &regs, &regs);
                ch = regs.h.al;
                scan = regs.h.ah;

                regs.h.ah = GETKEYFLAGS;
                int86(KEYBRD_BIOS, &regs, &regs);
                shift = regs.h.al;

                /* Translate keypad keys */
                if (iskeypad(scan)) {
                    kpad = iflags.num_pad ? numpad : keypad;
                    if (shift & SHIFT)
                        ch = kpad[scan - KEYPADLO].shift;
                    else if (shift & CTRL)
                        ch = kpad[scan - KEYPADLO].cntrl;
                    else
                        ch = kpad[scan - KEYPADLO].normal;
                }
#ifdef NO_TERMS
                if (pckeys(scan, shift)) {
                    continue;  /* consumed by pckeys; try again */
                }
#endif
                /* Alt-letters */
#ifndef PC9800
                if ((shift & ALT) && !ch) {
                    if (inmap(scan))
                        ch = scanmap[scan - SCANLO];
#ifdef NEW_ALT
                    else if (in_numericmap(scan))
                        ch = numeric_scanmap[scan - NUMERIC_SCANLO];
#endif
                    ch = isprint(ch) ? M(ch) : ch;
                }
#endif
                if (ch) {
                    if (dos_mouse_available)
                        dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                    if (iflags.grmode && cursor_flag)
                        HideCursor();
#endif
                    return ((ch == '\r') ? '\n' : (int) ch);
                }
                continue;  /* ch==0, no valid key; keep polling */
            } else {
                /* DOSgetch path — simpler, can use as-is since it's
                 * a single read without an internal retry loop */
                int c = DOSgetch();

                if (dos_mouse_available)
                    dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                if (iflags.grmode && cursor_flag)
                    HideCursor();
#endif
                return ((c == '\r') ? '\n' : c);
            }
        }

        /* Check mouse (non-blocking poll) */
        if (dos_mouse_available && dos_mouse_poll(&mev)) {
            /*
             * Convert screen cell coordinates to map coordinates.
             * The map window starts at screen row 1 (row 0 is message line)
             * and screen column 0 corresponds to map column 1 (1..COLNO-1).
             */
            int map_x, map_y;
            int mcx = mev.cx, mcy = mev.cy;

#ifdef SCREEN_VESA
            if (iflags.grmode && iflags.usevesa) {
                vesa_mouse_to_cell(mev.px, mev.py, &mcx, &mcy);
                vesa_mouse_to_map(mev.px, mev.py, &map_x, &map_y);
            } else
#endif
#ifdef SCREEN_VGA
            if (iflags.grmode) {
                mcx = mev.px / 8;
                mcy = mev.py / 16;
                vga_mouse_to_map(mev.px, mev.py, &map_x, &map_y);
            } else
#endif
            {
                /* Text mode: cells are 8x8 (virtual pixel coords) */
                map_x = mev.cx + 1;
                map_y = mev.cy - 1;  /* subtract 1 for message line */
#ifdef CLIPPING
                if (clipping) {
                    map_x += clipx;
                    map_y += clipy;
                }
#endif
            }

            /* Dismiss --More-- prompt if active */
            if (ttyDisplay && ttyDisplay->toplin == TOPLINE_NEED_MORE) {
                dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                if (iflags.grmode && cursor_flag)
                    HideCursor();
#endif
                return (mev.pressed == 1) ? ' ' : '\033';
            }

            /* Direction prompt ("In what direction?"): convert clicked tile
             * to a direction character relative to the hero */
            if (program_state.input_state == getdirInp) {
                if (map_x >= 1 && map_x < COLNO
                    && map_y >= 0 && map_y < ROWNO) {
                    int dch;

                    if (map_x == u.ux && map_y == u.uy) {
                        dch = '.';
                    } else {
                        int dir = xytodir(sgn(map_x - u.ux),
                                          sgn(map_y - u.uy));
                        dch = (int) (unsigned char) gc.Cmd.dirchars[dir];
                    }
                    dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                    if (iflags.grmode && cursor_flag)
                        HideCursor();
#endif
                    return dch;
                }
                if (mev.pressed == 2) {
                    dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                    if (iflags.grmode && cursor_flag)
                        HideCursor();
#endif
                    return '\033';
                }
                continue;
            }

            /* Yes/No or single-char prompt via yn_function() */
            if (ttyDisplay && ttyDisplay->inread > 0) {
                dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                if (iflags.grmode && cursor_flag)
                    HideCursor();
#endif
                return (mev.pressed == 1) ? ' ' : '\033';
            }

            /* Normal map click (commandInp or getposInp) */
            if (map_x >= 1 && map_x < COLNO
                && map_y >= 0 && map_y < ROWNO) {
                *x = (coordxy) map_x;
                *y = (coordxy) map_y;
                *mod = (mev.pressed == 1) ? CLICK_1 : CLICK_2;
                dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                if (iflags.grmode && cursor_flag)
                    HideCursor();
#endif
                return 0;  /* 0 signals mouse click to readchar_core() */
            }

            /* Click on position bar in graphics mode -> pan map viewport */
            if (iflags.grmode && program_state.input_state == commandInp) {
                boolean panned = FALSE;

                dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                if (cursor_flag)
                    HideCursor();
#endif
#ifdef SCREEN_VESA
                if (iflags.usevesa)
                    panned = vesa_pan_to_pixel(mev.px, mev.py);
                else
#endif
#ifdef SCREEN_VGA
                if (iflags.usevga)
                    panned = vga_pan_to_pixel(mev.px, mev.py);
#endif
#ifdef SIMULATE_CURSOR
                if (cursor_flag)
                    DrawCursor();
#endif
                dos_mouse_show();
                if (panned)
                    continue;
            }

            /* Click on message line (row 0) in command mode -> prev messages */
            if (mcy == 0 && program_state.input_state == commandInp) {
                dos_mouse_hide();
#ifdef SIMULATE_CURSOR
                if (iflags.grmode && cursor_flag)
                    HideCursor();
#endif
                return C('p');
            }
        }
    }
    /* NOTREACHED */
}
#endif /* TTY_GRAPHICS */

#if 0
static long
freediskspace(char *path)
{
    union REGS regs;

    regs.h.ah = FREESPACE;
    if (path[0] && path[1] == ':')
        regs.h.dl = (toupper(path[0]) - 'A') + 1;
    else
        regs.h.dl = 0;
    intdos(&regs, &regs);
    if (regs.x.ax == 0xFFFF)
        return -1L; /* bad drive number */
    else
        return ((long) regs.x.bx * regs.x.cx * regs.x.ax);
}
#endif /* 0 */

#ifndef __GO32__
/*
 * Functions to get filenames using wildcards
 */
int
findfirst_file(char *path)
{
    union REGS regs;
    struct SREGS sregs;

    regs.h.ah = FINDFIRST;
    regs.x.cx = 0; /* attribute: normal files */
    regs.x.dx = FP_OFF(path);
    sregs.ds = FP_SEG(path);
    intdosx(&regs, &regs, &sregs);
    return !regs.x.cflag;
}

int
findnext_file(void)
{
    union REGS regs;

    regs.h.ah = FINDNEXT;
    intdos(&regs, &regs);
    return !regs.x.cflag;
}

char *
foundfile_buffer(void)
{
    return (getdta() + 30);
}

/* Get disk transfer area */
static char *
getdta(void)
{
    union REGS regs;
    struct SREGS sregs;
    char *ret;

    regs.h.ah = GETDTA;
    intdosx(&regs, &regs, &sregs);
#ifdef MK_FP
    ret = (char *) MK_FP(sregs.es, regs.x.bx);
#else
    FP_OFF(ret) = regs.x.bx;
    FP_SEG(ret) = sregs.es;
#endif
    return ret;
}

long
filesize_nh(char *file)
{
    char *dta;

    if (findfirst_file(file)) {
        dta = getdta();
        return (*(long *) (dta + 26));
    } else
        return -1L;
}

#endif /* __GO32__ */

/*
 * Chdrive() changes the default drive.
 */
void
chdrive(const char *str)
{
#define SELECTDISK 0x0E
    char *ptr;
    union REGS inregs;
    char drive;

    if ((ptr = strchr(str, ':')) != (char *) 0) {
        drive = toupper(*(ptr - 1));
        inregs.h.ah = SELECTDISK;
        inregs.h.dl = drive - 'A';
        intdos(&inregs, &inregs);
    }
    return;
}

/* Use the IOCTL DOS function call to change stdin and stdout to raw
 * mode.  For stdin, this prevents MSDOS from trapping ^P, thus
 * freeing us of ^P toggling 'echo to printer'.
 * Thanks to Mark Zbikowski (markz@microsoft.UUCP).
 */

#define DEVICE 0x80
#define RAW 0x20
#define IOCTL 0x44
#define STDIN fileno(stdin)
#define STDOUT fileno(stdout)
#define GETBITS 0
#define SETBITS 1

static unsigned int old_stdin, old_stdout;

void
disable_ctrlP(void)
{
    if (!iflags.rawio)
        return;

    old_stdin = dos_ioctl(STDIN, GETBITS, 0);
    old_stdout = dos_ioctl(STDOUT, GETBITS, 0);
    if (old_stdin & DEVICE)
        dos_ioctl(STDIN, SETBITS, old_stdin | RAW);
    if (old_stdout & DEVICE)
        dos_ioctl(STDOUT, SETBITS, old_stdout | RAW);
    return;
}

void
enable_ctrlP(void)
{
    if (!iflags.rawio)
        return;
    if (old_stdin)
        (void) dos_ioctl(STDIN, SETBITS, old_stdin);
    if (old_stdout)
        (void) dos_ioctl(STDOUT, SETBITS, old_stdout);
    return;
}

static unsigned int
dos_ioctl(int handle, int mode, unsigned setvalue)
{
    union REGS regs;

    regs.h.ah = IOCTL;
    regs.h.al = mode;
    regs.x.bx = handle;
    regs.h.dl = setvalue;
    regs.h.dh = 0; /* Zero out dh */
    intdos(&regs, &regs);
    return (regs.x.dx);
}

unsigned long
sys_random_seed(void)
{
    unsigned long ourseed = 0UL;
    time_t datetime = 0;

    (void) time(&datetime);
    ourseed = (unsigned long) datetime;
    return ourseed;
}

#endif /* MSDOS */
