/* File: main-ibm.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

/* Purpose: Visual Display Support for "term.c", for the IBM */

/*
 * Original code by "Billy Tanksley (wtanksle@ucsd.edu)"
 * Use "Makefile.ibm" to compile Angband using this file.
 *
 * Support for DJGPP v2 by "Scott Egashira (egashira@u.washington.edu)"
 *
 * Extensive modifications by "Ben Harrison (benh@phial.com)",
 * including "collation" of the Watcom C/C++ and DOS-286 patches.
 *
 * Watcom C/C++ changes by "David Boeren (akemi@netcom.com)"
 * Use "Makefile.wat" to compile this file with Watcom C/C++, and
 * be sure to define "USE_IBM" and "USE_WAT".
 *
 * DOS-286 (conio.h) changes by (Roland Jay Roberts (jay@map.com)
 * Use "Makefile.286" (not ready) to compile this file for DOS-286,
 * and be sure to define "USE_IBM", "USE_WAT", and "USE_286".  Also,
 * depending on your compiler, you may need to define "USE_CONIO".
 *
 * True color palette support by "Mike Marcelais (michmarc@microsoft.com)",
 * with interface to the "color_table" array by Ben Harrison.
 *
 * Both "shift" keys are treated as "identical", and all the modifier keys
 * (control, shift, alt) are ignored when used with "normal" keys, unless
 * they modify the underlying "ascii" value of the key.  You must use the
 * new "user pref files" to be able to interact with the keypad and such.
 *
 * The "lib/user/pref-ibm.prf" file contains macro definitions and possible
 * alternative color set definitions.  The "lib/user/font-ibm.prf" contains
 * attr/char mappings for walls and floors and such.
 *
 * Note the "Term_user_ibm()" function hook, which could allow the user
 * to interact with the "main-ibm.c" visual system.  Currently this hook
 * is unused, but, for example, it could allow the user to toggle "sound"
 * or "graphics" modes, or to select the number of screen rows, with the
 * extra screen rows being used for the mirror window.
 */


#include "angband.h"


#ifdef USE_IBM


/*
 * Use a "virtual" screen to "buffer" screen writes.
 */
#define USE_VIRTUAL

#ifdef USE_DOSSOCK
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <bios.h>
#include <dos.h>
#include <dir.h>

#ifdef USE_WAT

# include <conio.h>

# ifdef USE_CONIO
# else /* USE_CONIO */

# include <graph.h>

# define bioskey(C)	_bios_keybrd(C)

# endif  /* USE_CONIO */

# ifndef USE_286
# define int86(a, b, c)	int386(a, b, c)
# endif 

# define inportb(x)	inp(x)
# define outportb(x, y)	outp(x, y)

#else /* USE_WAT */

# if __DJGPP__ > 1

# include <pc.h>
# include <osfcn.h>

# else /* __DJGPP__ > 1 */
# ifdef __DJGPP__
# error "Upgrade to version 2.0 of DJGPP"
# endif  /* __DJGPP__ */
# endif  /* __DJGPP__ > 1 */

#endif /* USE_WAT */


#ifdef USE_CONIO

# include <conio.h>

/*
 * Hack -- write directly to video card
 */
extern int directvideo = 1;

/*
 * Hack -- no virtual screen
 */
# undef USE_VIRTUAL

#endif /* USE_CONIO */


/*
 * Keypress input modifier flags (hard-coded by DOS)
 */
#define K_RSHIFT	0	/* Right shift key down */
#define K_LSHIFT	1	/* Left shift key down */
#define K_CTRL		2	/* Ctrl key down */
#define K_ALT		3	/* Alt key down */
#define K_SCROLL	4	/* Scroll lock on */
#define K_NUM		5	/* Num lock on */
#define K_CAPS		6	/* Caps lock on */
#define K_INSERT	7	/* Insert on */


/*
 * Foreground color bits (hard-coded by DOS)
 */
#define VID_BLACK	0x00
#define VID_BLUE	0x01
#define VID_GREEN	0x02
#define VID_CYAN	0x03
#define VID_RED		0x04
#define VID_MAGENTA	0x05
#define VID_YELLOW	0x06
#define VID_WHITE	0x07

/*
 * Bright text (hard-coded by DOS)
 */
#define VID_BRIGHT	0x08

/*
 * Background color bits (hard-coded by DOS)
 */
#define VUD_BLACK	0x00
#define VUD_BLUE	0x10
#define VUD_GREEN	0x20
#define VUD_CYAN	0x30
#define VUD_RED		0x40
#define VUD_MAGENTA	0x50
#define VUD_YELLOW	0x60
#define VUD_WHITE	0x70

/*
 * Blinking text (hard-coded by DOS)
 */
#define VUD_BRIGHT	0x80


/*
 * Screen Size
 */
static int rows = 25;
static int cols = 80;


/*
 * Physical Screen
 */
#ifdef USE_286
# define PhysicalScreen ((byte *)MK_FP(0xB800, 0x0000))
#else
# define PhysicalScreen ((byte *)(0xB800 << 4))
#endif


#ifdef USE_VIRTUAL

/*
 * Virtual Screen Contents
 */
static byte *VirtualScreen;

#else

/*
* Physical screen access
*/
#define VirtualScreen PhysicalScreen

#endif


/*
 * Hack -- the cursor "visibility"
 */
static int saved_cur_v;
static int saved_cur_high;
static int saved_cur_low;


#ifdef USE_CONIO
#else /* USE_CONIO */

/*
* This array is used for "wiping" the screen
*/
static byte wiper[160];

#endif /* USE_CONIO */


/*
 * The main screen (currently the only screen)
 */
static term term_screen_body;


/*
 * Choose between the "complex" and "simple" color methods
 */
static byte use_color_complex = FALSE;


/*
 * The "complex" color set
 */
static long ibm_color_complex[16];


/*
 * The "simple" color set
 *
 * This table is used by the "color" code to instantiate the "approximate"
 * Angband colors using the only colors available on crappy monitors.
 *
 * The entries below are taken from the "color bits" defined above.
 *
 * Note that values from 16 to 255 are extremely ugly.
 *
 * The values below came from various sources, if you do not like them,
 * get a better monitor, or edit "pref-ibm.prf" to use different codes.
 *
 * Note that many of the choices below suck, but so do crappy monitors.
 */
static byte ibm_color_simple[16] =
{
	VID_BLACK, 			/* Dark */
	VID_WHITE, 			/* White */
	VID_CYAN, 			/* Slate XXX */
	VID_RED | VID_BRIGHT, 	/* Orange XXX */
	VID_RED, 			/* Red */
	VID_GREEN, 			/* Green */
	VID_BLUE, 			/* Blue */
	VID_YELLOW, 			/* Umber XXX */
	VID_BLACK | VID_BRIGHT, 	/* Light Dark */
	VID_CYAN | VID_BRIGHT, 	/* Light Slate XXX */
	VID_MAGENTA, 		/* Violet */
	VID_YELLOW | VID_BRIGHT, 	/* Yellow */
	VID_MAGENTA | VID_BRIGHT, 	/* Light Red XXX */
	VID_GREEN | VID_BRIGHT, 	/* Light Green */
	VID_BLUE | VID_BRIGHT, 	/* Light Blue */
	VID_YELLOW			/* Light Umber XXX */
};



/*
 * Activate the "ibm_color_complex" palette information.
 *
 * Code by Mike Marcelais, with help from "The programmer's guide
 * to the EGA and VGA video cards" [Farraro].
 *
 * On VGA cards, colors go through a double-indirection when looking
 * up the `real' color when in 16 color mode.  The color value in the
 * attribute is looked up in the EGA color registers.  Then that value
 * is looked up in the VGA color registers.  Then the color is displayed.
 * This is done for compatability.  However, the EGA registers are
 * initialized by default to 0..5, 14, 7, 38..3F and not 0..F which means
 * that unless these are reset, the VGA setpalette function will not
 * update the correct palette register!
 *
 * DJGPP's GrSetColor() does _not_ set the EGA palette list, only the
 * VGA color list.
 *
 * Note that the "traditional" method, using "int86(0x10)", is very slow
 * when called in protected mode, so we use a faster method using video
 * ports instead.
 *
 * On Watcom machines, we could simply use the special "_remapallpalette()"
 * function, which not only sets both palette lists (see below) but also
 * checks for legality of the monitor mode, but, if we are doing bitmapped
 * graphics, that function forgets to set the EGA registers for some reason.
 */
static void activate_color_complex(void)
{
	int i;
	printf("%c%c%c%c", 8, 8, 8, 8);

#if 1

	/* Edit the EGA palette */
	inportb(0x3da);

	/* Edit the colors */
	for (i = 0; i < 16; i++)
	{
		/* Set color "i" */
		outportb(0x3c0, i);

		/* To value "i" */
		outportb(0x3c0, i);
	}

	/* Use that EGA palette */
	outportb(0x3c0, 0x20);

	/* Edit VGA palette, starting at color zero */
	outportb(0x3c8, 0);

	/* Send the colors */
	for (i = 0; i < 16; i++)
	{
		/* Send the red, green, blue components */
		outportb(0x3c9, ((ibm_color_complex[i]) & 0xFF));
		outportb(0x3c9, ((ibm_color_complex[i] >> 8) & 0xFF));
		outportb(0x3c9, ((ibm_color_complex[i] >> 16) & 0xFF));
	}

	/* Disable VGA attribute blinking so all 16 background colors work cleanly */
	{
		union REGS r;
		memset(&r, 0, sizeof(r));
		r.w.ax = 0x1003;
		r.w.bx = 0x0000;
		int86(0x10, &r, &r);
	}

#else /* 1 */

	/* Set the colors */
	for (i = 0; i < 16; i++)
	{
		union REGS r;

		/* Set EGA color */
		r.h.ah = 0x10;
		r.h.al = 0x00;

		/* Set color "i" */
		r.h.bl = i;

		/* To value "i" */
		r.h.bh = i;

		/* Do it */
		int86(0x10, &r, &r);

		/* Set VGA color */
		r.h.ah = 0x10;
		r.h.al = 0x10;

		/* Set color "i" */
		r.h.bh = 0x00;
		r.h.bl = i;

		/* Use this "green" value */
		r.h.ch = ((ibm_color_complex[i] >> 8) & 0xFF);

		/* Use this "blue" value */
		r.h.cl = ((ibm_color_complex[i] >> 16) & 0xFF);

		/* Use this "red" value */
		r.h.dh = ((ibm_color_complex[i]) & 0xFF);

		/* Do it */
		int86(0x10, &r, &r);
	}

#endif /* 1 */

}


#ifdef SUPPORT_GAMMA

/*
 * When set to TRUE, indicates that we can use gamma_table
 */
static bool gamma_table_ready = FALSE;


/*
 * Build gamma correction table if requested and applicable
 */
static void setup_gamma_table(void)
{
	static u16b old_gamma_val = 0;

	/* (Re)build the table only when gamma value changes */
	if (gamma_val == old_gamma_val) return;

	/* Temporarily inactivate the table */
	gamma_table_ready = FALSE;

	/* Validate gamma_val */
	if ((gamma_val <= 0) || (gamma_val >= 256))
	{
		/* Reset */
		old_gamma_val = gamma_val = 0;

		/* Leave it inactive */
		return;
	}

	/* (Re)build the table */
	build_gamma_table(gamma_val);

	/* Remember gamma_val */
	old_gamma_val = gamma_val;

	/* Activate the table */
	gamma_table_ready = TRUE;
}

#endif /* SUPPORT_GAMMA */


/*
 * Note the use of "(x >> 2)" to convert an 8 bit value to a 6 bit value
 * without losing much precision.
 */
static int Term_xtra_ibm_react(void)
{
	int i;

	/* Complex method */
	if (use_color_complex)
	{
		long rv, gv, bv, code;

		bool change = FALSE;

#ifdef SUPPORT_GAMMA

		/* Build gamma_table */
		setup_gamma_table();

#endif /* SUPPORT_GAMMA */

		/* Save the default colors */
		for (i = 0; i < 16; i++)
		{
			/* Extract desired values */
			rv = angband_color_table[i][1];
			gv = angband_color_table[i][2];
			bv = angband_color_table[i][3];

#ifdef SUPPORT_GAMMA

			/* Hack - Gamma correction */
			if (gamma_table_ready)
			{
				rv = gamma_table[rv];
				gv = gamma_table[gv];
				bv = gamma_table[bv];
			}

#endif /* SUPPORT_GAMMA */

			/* 8 bit to 6 bit conversion */
			rv = rv >> 2;
			gv = gv >> 2;
			bv = bv >> 2;

			/* Extract a full color code */
			code = ((rv) | (gv << 8) | (bv << 16));

			/* Activate changes */
			if (ibm_color_complex[i] != code)
			{
				/* Note the change */
				change = TRUE;

				/* Apply the desired color */
				ibm_color_complex[i] = code;
			}
		}

		/* Activate the palette if needed */
		if (change) activate_color_complex();
	}

	/* Simple method */
	else
	{
		/* Save the default colors */
		for (i = 0; i < 16; i++)
		{
			/* Simply accept the desired colors */
			ibm_color_simple[i] = angband_color_table[i][0];
		}
	}

	/* Success */
	return (0);
}



/*
 * Hack -- set the cursor "visibility"
 */
static void curs_set(int v)
{
	/* If needed */
	if (saved_cur_v != v)
	{
		union REGS r;

		/* Set cursor */
		r.h.ah = 1;

		/* Visible */
		if (v)
		{
			/* Use the saved values */
			r.h.ch = saved_cur_high;
			r.h.cl = saved_cur_low;
		}

		/* Invisible */
		else
		{
			/* Make it invisible */
			r.h.ch = 0x20;
			r.h.cl = 0x00;
		}

		/* Make the call */
		int86(0x10, &r, &r);

		/* Save the cursor state */
		saved_cur_v = v;
	}
}


/*
 * =========================================================================
 * DOS Mouse Interface, Software Cursor, Command Dock (Row 24),
 * Menu Click Handler & Styled Right-Click Context Menu
 * =========================================================================
 */

extern bool mouse_cmd_raw;
extern int mouse_dest_y, mouse_dest_x;
extern int mouse_find_step(int ty, int tx);
extern int ibm_target_mode, ibm_target_y, ibm_target_x;
extern int ibm_skill_mode, ibm_skill_click_y, ibm_skill_click_x, ibm_skill_click_btn;

static bool mouse_present = FALSE;
static int mouse_x = 40;
static int mouse_y = 12;
static int mouse_last_buttons = 0;

typedef struct dock_btn_type
{
	int x0, x1;
	cptr label;
	char cmd;
} dock_btn_type;

/* 12-button Command Dock spanning all 80 columns of Row 24 (y = 24) */
#define DOCK_BTN_COUNT 12
static const dock_btn_type dock_buttons[DOCK_BTN_COUNT] =
{
	{  0,  5, "[i]Inv",  'i' },
	{  6, 11, "[e]Eqp",  'e' },
	{ 12, 17, "[g]Get",  'g' },
	{ 18, 24, "[m]Cast", 'm' },
	{ 25, 30, "[z]Rod",  'z' },
	{ 31, 37, "[a]Wand", 'a' },
	{ 38, 44, "[u]Staf", 'u' },
	{ 45, 51, "[q]Quaf", 'q' },
	{ 52, 58, "[r]Read", 'r' },
	{ 59, 65, "[f]Fire", 'f' },
	{ 66, 72, "[G]Skil", 'G' },
	{ 73, 79, "[+]More", '+' }
};

/* Flush one row from VirtualScreen to physical screen, overlaying software mouse cursor */
static void ibm_flush_row(int r)
{
#ifdef USE_VIRTUAL
	byte linebuf[160];

	if (r < 0 || r >= rows || !VirtualScreen) return;

	memcpy(linebuf, VirtualScreen + ((r * cols) << 1), 160);

	if (mouse_present && r == mouse_y && mouse_x >= 0 && mouse_x < cols)
	{
		byte orig_ch = linebuf[mouse_x << 1];
		byte orig_attr = linebuf[(mouse_x << 1) + 1];
		byte orig_bg = (orig_attr >> 4) & 0x0F;
		byte orig_fg = orig_attr & 0x0F;
		byte new_attr;

		/* On blank space, draw the pointer arrow glyph (0x10) so the cursor is always visible */
		if (orig_ch == ' ' || orig_ch == 0)
		{
			linebuf[mouse_x << 1] = 0x10;
			if (use_color_complex)
			{
				byte cur_bg = (orig_bg == TERM_BLUE) ? TERM_GREEN : TERM_BLUE;
				new_attr = (byte)((cur_bg << 4) | TERM_YELLOW);
			}
			else
			{
				new_attr = 0x1E;
			}
		}
		else
		{
			if (use_color_complex)
			{
				byte cur_bg = (orig_bg == TERM_BLUE) ? TERM_GREEN : TERM_BLUE;
				byte cur_fg = (orig_fg == TERM_YELLOW) ? TERM_WHITE : TERM_YELLOW;
				new_attr = (byte)((cur_bg << 4) | cur_fg);
			}
			else
			{
				new_attr = (byte)(orig_attr ^ 0x7F);
			}
		}
		linebuf[(mouse_x << 1) + 1] = new_attr;
	}

# ifdef USE_WAT
	memcpy(PhysicalScreen + (r * 160), linebuf, 160);
# else
	ScreenUpdateLine(linebuf, r);
# endif
#endif
}

/* Render the 12-button Command Dock onto Row 24 of VirtualScreen (only when in-game) and flush */
static void ibm_draw_dock(void)
{
	int b, x;
	byte *dest;

	if (!VirtualScreen) return;
	dest = VirtualScreen + ((24 * cols) << 1);

	/* Hide the Command Dock on title, savefile, and character creation screens */
	if (!character_dungeon)
	{
		for (x = 0; x < cols; x++)
		{
			dest[x << 1] = ' ';
			dest[(x << 1) + 1] = use_color_complex ? TERM_WHITE : 0x07;
		}
		ibm_flush_row(24);
		return;
	}

	for (x = 0; x < cols; x++)
	{
		dest[x << 1] = ' ';
		dest[(x << 1) + 1] = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_WHITE) : 0x70;
	}

	for (b = 0; b < DOCK_BTN_COUNT; b++)
	{
		bool hov = (mouse_present && mouse_y == 24 &&
		            mouse_x >= dock_buttons[b].x0 && mouse_x <= dock_buttons[b].x1);
		byte bg = use_color_complex ?
		          (hov ? TERM_BLUE : ((b & 1) ? TERM_DARK : TERM_SLATE)) :
		          (hov ? 0x10 : ((b & 1) ? 0x00 : 0x70));
		cptr s = dock_buttons[b].label;
		int len = strlen(s);

		for (x = dock_buttons[b].x0; x <= dock_buttons[b].x1; x++)
		{
			int idx = x - dock_buttons[b].x0;
			char ch = (idx < len) ? s[idx] : ' ';
			byte fg;

			if (use_color_complex)
			{
				if (hov)
					fg = TERM_YELLOW;
				else if (idx <= 2)
					fg = TERM_YELLOW;
				else
					fg = TERM_WHITE;
				dest[x << 1] = (byte)ch;
				dest[(x << 1) + 1] = (byte)((bg << 4) | fg);
			}
			else
			{
				fg = (idx <= 2) ? (VID_YELLOW | VID_BRIGHT) : VID_WHITE;
				dest[x << 1] = (byte)ch;
				dest[(x << 1) + 1] = (byte)(bg | fg);
			}
		}
	}

	ibm_flush_row(24);
}

/* Initialize DOS mouse driver via INT 33h */
static void ibm_mouse_init(void)
{
	union REGS r;

	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0000;
	int86(0x33, &r, &r);
	if (r.w.ax == 0)
	{
		mouse_present = FALSE;
		return;
	}

	mouse_present = TRUE;

	/* Horizontal range 0..639 */
	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0007;
	r.w.cx = 0;
	r.w.dx = 639;
	int86(0x33, &r, &r);

	/* Vertical range 0..199 */
	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0008;
	r.w.cx = 0;
	r.w.dx = 199;
	int86(0x33, &r, &r);

	/* Keep hardware cursor hidden; we draw a clean software cursor */
	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0002;
	int86(0x33, &r, &r);

	/* Clear any pending button press counters */
	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0005;
	r.w.bx = 0;
	int86(0x33, &r, &r);
	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0005;
	r.w.bx = 1;
	int86(0x33, &r, &r);

	mouse_x = 40;
	mouse_y = 12;
	mouse_last_buttons = 0;
}

/* Poll mouse position and newly-pressed buttons (bit 0 = left, bit 1 = right) */
static int ibm_mouse_poll(int *out_x, int *out_y)
{
	union REGS r;
	int nx, ny, btns, pressed;

	if (!mouse_present) return (0);

	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0003;
	int86(0x33, &r, &r);

	btns = (int)(r.w.bx & 0x03);
	nx = (int)(r.w.cx / 8);
	ny = (int)(r.w.dx / 8);

	pressed = btns & (~mouse_last_buttons);
	mouse_last_buttons = btns;

	/* Also check latched button-press counters so fast clicks are never missed */
	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0005;
	r.w.bx = 0;
	int86(0x33, &r, &r);
	if (r.w.bx > 0)
	{
		pressed |= 1;
		nx = (int)(r.w.cx / 8);
		ny = (int)(r.w.dx / 8);
	}

	memset(&r, 0, sizeof(r));
	r.w.ax = 0x0005;
	r.w.bx = 1;
	int86(0x33, &r, &r);
	if (r.w.bx > 0)
	{
		pressed |= 2;
		nx = (int)(r.w.cx / 8);
		ny = (int)(r.w.dx / 8);
	}

	if (nx < 0) nx = 0;
	if (nx >= cols) nx = cols - 1;
	if (ny < 0) ny = 0;
	if (ny >= rows) ny = rows - 1;

	if (nx != mouse_x || ny != mouse_y)
	{
		int old_y = mouse_y;
		mouse_x = nx;
		mouse_y = ny;
		if (old_y == 24 || mouse_y == 24)
		{
			ibm_draw_dock();
		}
		if (old_y != 24) ibm_flush_row(old_y);
		if (mouse_y != old_y && mouse_y != 24) ibm_flush_row(mouse_y);
	}

	if (out_x) *out_x = mouse_x;
	if (out_y) *out_y = mouse_y;
	return (pressed);
}

/* Helper: read character on VirtualScreen at (x, y) */
static char ibm_vscr_char(int x, int y)
{
	if (!VirtualScreen || x < 0 || x >= cols || y < 0 || y >= rows) return (' ');
	return ((char)VirtualScreen[((y * cols + x) << 1)]);
}

/* Helper: read a line of text from VirtualScreen */
static void ibm_vscr_line(int y, char *buf)
{
	int x;
	for (x = 0; x < cols; x++)
	{
		char c = ibm_vscr_char(x, y);
		/* Non-ASCII font tiles map to spaces for text matching */
		buf[x] = ((unsigned char)c >= 32 && (unsigned char)c < 127) ? c : ' ';
	}
	buf[cols] = '\0';
}

/* Convert (dy, dx) into an 8-way keypad direction (1..9, never 5) */
static int ibm_dir_from_delta(int dy, int dx)
{
	int ady = (dy < 0) ? -dy : dy;
	int adx = (dx < 0) ? -dx : dx;
	int sy, sx;

	if (dy == 0 && dx == 0) return (0);

	if (ady > adx * 2) sx = 0;
	else sx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);

	if (adx > ady * 2) sy = 0;
	else sy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);

	if (sy < 0 && sx < 0) return (7);
	if (sy < 0 && sx == 0) return (8);
	if (sy < 0 && sx > 0) return (9);
	if (sy == 0 && sx < 0) return (4);
	if (sy == 0 && sx > 0) return (6);
	if (sy > 0 && sx < 0) return (1);
	if (sy > 0 && sx == 0) return (2);
	if (sy > 0 && sx > 0) return (3);
	return (0);
}

/* Check if grid (y, x) has a chest object */
static bool ibm_has_chest(int y, int x)
{
	s16b o_idx;
	if (!in_bounds(y, x)) return (FALSE);
	for (o_idx = cave[y][x].o_idx; o_idx; o_idx = o_list[o_idx].next_o_idx)
	{
		if (o_list[o_idx].tval == TV_CHEST) return (TRUE);
	}
	return (FALSE);
}

/* Enqueue a sequence of keys into Term */
static void ibm_send_keys(cptr s)
{
	while (*s)
	{
		Term_keypress((unsigned char)(*s));
		s++;
	}
}

/* Trigger school spellcasting directly if the player has learned school spells */
static void ibm_cast_or_skill(void)
{
	if (exec_lua("return fill_learned_spells(-1)") > 0)
	{
		command_arg = MKEY_SCHOOL;
	}
	mouse_cmd_raw = TRUE;
	Term_keypress('m');
}

/* Context menu action IDs */
#define CTX_NONE          0
#define CTX_RAW_CMD       1
#define CTX_DIR_CMD       2
#define CTX_AIM_CMD       3
#define CTX_BFS_TRAVEL    4
#define CTX_REST_HEAL     5
#define CTX_CAST_SPELL    6
#define CTX_TARGET_MON    7
#define CTX_TARGET_LOC    8
#define CTX_MON_LORE      9
#define CTX_EXAMINE_TILE 10

typedef struct ibm_ctx_item
{
	char label[44];
	int action;
	char cmd;
	int dir;
} ibm_ctx_item;

static void ibm_add_ctx(ibm_ctx_item *items, int *cnt, cptr label, int action, char cmd, int dir)
{
	if (*cnt >= 20) return;
	strncpy(items[*cnt].label, label, 43);
	items[*cnt].label[43] = '\0';
	items[*cnt].action = action;
	items[*cnt].cmd = cmd;
	items[*cnt].dir = dir;
	(*cnt)++;
}

/* Draw a styled string into VirtualScreen at (x, y) */
static void ibm_vscr_putstr(int x, int y, cptr s, byte attr)
{
	int i;
	if (!VirtualScreen || y < 0 || y >= rows) return;
	for (i = 0; s[i] && (x + i) < cols; i++)
	{
		if (x + i < 0) continue;
		VirtualScreen[((y * cols + x + i) << 1)] = (byte)s[i];
		VirtualScreen[((y * cols + x + i) << 1) + 1] = attr;
	}
}

/* Display and run the interactive popup Context Menu for map grid (gy, gx) */
static bool ibm_show_context_menu(int gy, int gx, int scr_x, int scr_y)
{
	ibm_ctx_item items[20];
	int cnt = 0;
	char title[48];
	cave_type *c_ptr = NULL;
	monster_type *m_ptr = NULL;
	bool has_mon = FALSE;
	int dist = 0;
	int dir = 0;
	byte *saved_vscr;
	int box_w, box_h, box_x, box_y;
	int i, r, c;
	int chosen = -1;

	if (!p_ptr->wild_mode && !in_bounds(gy, gx)) return (FALSE);

	if (!p_ptr->wild_mode)
	{
		c_ptr = &cave[gy][gx];
		if (c_ptr->m_idx > 0 && m_list[c_ptr->m_idx].ml)
		{
			m_ptr = &m_list[c_ptr->m_idx];
			has_mon = TRUE;
		}
		dist = distance(p_ptr->py, p_ptr->px, gy, gx);
		dir = ibm_dir_from_delta(gy - p_ptr->py, gx - p_ptr->px);
	}
	else
	{
		dist = distance(p_ptr->py, p_ptr->px, gy, gx);
		dir = ibm_dir_from_delta(gy - p_ptr->py, gx - p_ptr->px);
	}

	/* Build title and context-sensitive menu items */
	if (gy == p_ptr->py && gx == p_ptr->px)
	{
		strcpy(title, "Player Actions (@)");
		if (p_ptr->wild_mode)
		{
			ibm_add_ctx(items, &cnt, "Enter local area (>)", CTX_RAW_CMD, '>', 0);
		}
		else
		{
			if (c_ptr->o_idx)
				ibm_add_ctx(items, &cnt, "Pick up item (g)", CTX_RAW_CMD, 'g', 0);
			if (c_ptr->feat == FEAT_LESS || c_ptr->feat == FEAT_WAY_LESS ||
			    c_ptr->feat == FEAT_SHAFT_UP || c_ptr->feat == FEAT_QUEST_UP || dun_level == 0)
				ibm_add_ctx(items, &cnt, (dun_level == 0) ? "World wilderness map (<)" : "Ascend stairs (<)", CTX_RAW_CMD, '<', 0);
			if (c_ptr->feat == FEAT_MORE || c_ptr->feat == FEAT_WAY_MORE ||
			    c_ptr->feat == FEAT_SHAFT_DOWN || c_ptr->feat == FEAT_QUEST_DOWN ||
			    c_ptr->feat == FEAT_QUEST_ENTER || c_ptr->feat == FEAT_QUEST_EXIT)
				ibm_add_ctx(items, &cnt, "Descend / Enter (>)", CTX_RAW_CMD, '>', 0);
			if (c_ptr->feat == FEAT_FOUNTAIN)
				ibm_add_ctx(items, &cnt, "Drink from fountain (H)", CTX_RAW_CMD, 'H', 0);
			if (c_ptr->feat >= FEAT_ALTAR_HEAD && c_ptr->feat <= FEAT_ALTAR_TAIL)
				ibm_add_ctx(items, &cnt, "Pray / Sacrifice (O)", CTX_RAW_CMD, 'O', 0);
		}
		ibm_add_ctx(items, &cnt, "Wait 1 turn (,)", CTX_RAW_CMD, ',', 0);
		ibm_add_ctx(items, &cnt, "Rest until healed (R)", CTX_REST_HEAL, 'R', 0);
		ibm_add_ctx(items, &cnt, "Search surroundings (s)", CTX_RAW_CMD, 's', 0);
		ibm_add_ctx(items, &cnt, "Look around (l)", CTX_RAW_CMD, 'l', 0);
		ibm_add_ctx(items, &cnt, "Wear / Wield equipment (w)", CTX_RAW_CMD, 'w', 0);
		ibm_add_ctx(items, &cnt, "Take off equipment (t)", CTX_RAW_CMD, 't', 0);
		ibm_add_ctx(items, &cnt, "Drop item (d)", CTX_RAW_CMD, 'd', 0);
		ibm_add_ctx(items, &cnt, "Eat food (E)", CTX_RAW_CMD, 'E', 0);
		ibm_add_ctx(items, &cnt, "Activate artifact (A)", CTX_RAW_CMD, 'A', 0);
		ibm_add_ctx(items, &cnt, "Racial / Class power (U)", CTX_RAW_CMD, 'U', 0);
		ibm_add_ctx(items, &cnt, "Character sheet (C)", CTX_RAW_CMD, 'C', 0);
		ibm_add_ctx(items, &cnt, "Game options (=)", CTX_RAW_CMD, '=', 0);
		ibm_add_ctx(items, &cnt, "Save game (Ctrl-S)", CTX_RAW_CMD, KTRL('S'), 0);
	}
	else if (has_mon)
	{
		char m_name[48];
		monster_desc(m_name, m_ptr, 0x08);
		m_name[0] = toupper((unsigned char)m_name[0]);
		strncpy(title, m_name, 42);
		title[42] = '\0';

		if (dist == 1)
			ibm_add_ctx(items, &cnt, "Attack / Interact", CTX_DIR_CMD, ';', dir);
		ibm_add_ctx(items, &cnt, "Cast spell at monster", CTX_AIM_CMD, 'm', 5);
		ibm_add_ctx(items, &cnt, "Fire ranged weapon (f)", CTX_AIM_CMD, 'f', 5);
		ibm_add_ctx(items, &cnt, "Zap rod at monster (z)", CTX_AIM_CMD, 'z', 5);
		ibm_add_ctx(items, &cnt, "Aim wand at monster (a)", CTX_AIM_CMD, 'a', 5);
		ibm_add_ctx(items, &cnt, "Throw item at monster (v)", CTX_AIM_CMD, 'v', 5);
		if (dist == 1)
			ibm_add_ctx(items, &cnt, "Steal from monster (Z)", CTX_DIR_CMD, 'Z', dir);
		ibm_add_ctx(items, &cnt, "Target monster (*)", CTX_TARGET_MON, 0, 0);
		ibm_add_ctx(items, &cnt, "Monster lore & stats", CTX_MON_LORE, 0, 0);
		if (dist > 1)
			ibm_add_ctx(items, &cnt, "Walk toward monster", CTX_DIR_CMD, ';', dir);
	}
	else if (!p_ptr->wild_mode && dist == 1)
	{
		if (c_ptr->o_idx && o_list[c_ptr->o_idx].marked)
		{
			object_desc(title, &o_list[c_ptr->o_idx], TRUE, 0);
			title[42] = '\0';
		}
		else if (c_ptr->info & (CAVE_MARK | CAVE_SEEN))
		{
			strncpy(title, f_name + f_info[c_ptr->feat].name, 42);
			title[42] = '\0';
		}
		else
		{
			strcpy(title, "Adjacent Tile");
		}

		if ((c_ptr->feat >= FEAT_DOOR_HEAD && c_ptr->feat <= FEAT_DOOR_TAIL) || ibm_has_chest(gy, gx))
			ibm_add_ctx(items, &cnt, "Open door / chest (o)", CTX_DIR_CMD, 'o', dir);
		if (c_ptr->feat >= FEAT_DOOR_HEAD && c_ptr->feat <= FEAT_DOOR_TAIL)
		{
			ibm_add_ctx(items, &cnt, "Bash door (B)", CTX_DIR_CMD, 'B', dir);
			ibm_add_ctx(items, &cnt, "Spike door (j)", CTX_DIR_CMD, 'j', dir);
		}
		if (c_ptr->feat == FEAT_OPEN || c_ptr->feat == FEAT_BROKEN)
			ibm_add_ctx(items, &cnt, "Close door (c)", CTX_DIR_CMD, 'c', dir);
		if ((c_ptr->info & CAVE_TRDT) && c_ptr->t_idx != 0)
			ibm_add_ctx(items, &cnt, "Disarm trap (D)", CTX_DIR_CMD, 'D', dir);
		if (cave_floor_bold(gy, gx) || c_ptr->feat == FEAT_TREES || c_ptr->feat == FEAT_SHOP)
		{
			ibm_add_ctx(items, &cnt, "Step here (;)", CTX_DIR_CMD, ';', dir);
			ibm_add_ctx(items, &cnt, "Run this way (.)", CTX_DIR_CMD, '.', dir);
		}
		if (f_info[c_ptr->feat].flags1 & FF1_TUNNELABLE)
			ibm_add_ctx(items, &cnt, "Tunnel / Dig (T)", CTX_DIR_CMD, 'T', dir);
		ibm_add_ctx(items, &cnt, "Alter / Attack tile (+)", CTX_DIR_CMD, '+', dir);
		ibm_add_ctx(items, &cnt, "Examine tile", CTX_EXAMINE_TILE, 0, 0);
	}
	else
	{
		if (p_ptr->wild_mode)
		{
			strcpy(title, "Wilderness Tile");
			ibm_add_ctx(items, &cnt, "Walk toward here", CTX_DIR_CMD, ';', dir);
			ibm_add_ctx(items, &cnt, "Enter local area (>)", CTX_RAW_CMD, '>', 0);
		}
		else
		{
			if (c_ptr->o_idx && o_list[c_ptr->o_idx].marked)
			{
				object_desc(title, &o_list[c_ptr->o_idx], TRUE, 0);
				title[42] = '\0';
			}
			else if (c_ptr->info & (CAVE_MARK | CAVE_SEEN))
			{
				strncpy(title, f_name + f_info[c_ptr->feat].name, 42);
				title[42] = '\0';
			}
			else
			{
				strcpy(title, "Dungeon Location");
			}

			if (mouse_find_step(gy, gx) > 0)
				ibm_add_ctx(items, &cnt, "Travel here (Shortest path)", CTX_BFS_TRAVEL, '.', 0);
			ibm_add_ctx(items, &cnt, "Run in this direction (.)", CTX_DIR_CMD, '.', dir);
			ibm_add_ctx(items, &cnt, "Step in this direction (;)", CTX_DIR_CMD, ';', dir);
			ibm_add_ctx(items, &cnt, "Cast spell at tile (m)", CTX_AIM_CMD, 'm', 5);
			ibm_add_ctx(items, &cnt, "Fire / Shoot at tile (f)", CTX_AIM_CMD, 'f', 5);
			ibm_add_ctx(items, &cnt, "Target this location (*)", CTX_TARGET_LOC, 0, 0);
			ibm_add_ctx(items, &cnt, "View dungeon map (M)", CTX_RAW_CMD, 'M', 0);
			ibm_add_ctx(items, &cnt, "Examine tile", CTX_EXAMINE_TILE, 0, 0);
		}
	}

	if (cnt == 0) return (FALSE);

	/* Compute menu box dimensions */
	box_w = strlen(title) + 6;
	for (i = 0; i < cnt; i++)
	{
		int w = strlen(items[i].label) + 7;
		if (w > box_w) box_w = w;
	}
	if (box_w > 48) box_w = 48;
	box_h = cnt + 3;

	box_x = scr_x + 1;
	box_y = scr_y;
	if (box_x + box_w + 1 >= cols) box_x = scr_x - box_w;
	if (box_x < 1) box_x = 1;
	if (box_y + box_h >= 24) box_y = 23 - box_h;
	if (box_y < 1) box_y = 1;

	/* Save VirtualScreen */
	C_MAKE(saved_vscr, rows * cols * 2, byte);
	memcpy(saved_vscr, VirtualScreen, rows * cols * 2);

	/* Modal interaction loop */
	while (TRUE)
	{
		int hovered = -1;
		int pressed, mx, my;
		byte border_attr = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_WHITE) : 0x7F;
		byte title_attr  = use_color_complex ? (byte)((TERM_BLUE << 4) | TERM_YELLOW) : 0x1E;
		byte body_attr   = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_WHITE) : 0x70;
		byte key_attr    = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_YELLOW) : 0x7E;
		byte sel_attr    = use_color_complex ? (byte)((TERM_BLUE << 4) | TERM_YELLOW) : 0x1F;
		byte shadow_attr = use_color_complex ? (byte)((TERM_DARK << 4) | TERM_SLATE) : 0x08;

		if (mouse_y >= box_y + 2 && mouse_y < box_y + 2 + cnt &&
		    mouse_x >= box_x && mouse_x < box_x + box_w)
		{
			hovered = mouse_y - (box_y + 2);
		}

		/* Draw menu box into VirtualScreen */
		for (r = 0; r < box_h; r++)
		{
			int sy = box_y + r;
			for (c = 0; c < box_w; c++)
			{
				int sx = box_x + c;
				char ch = ' ';
				byte at = body_attr;

				if (r == 0 || r == box_h - 1)
				{
					at = border_attr;
					ch = (c == 0 || c == box_w - 1) ? '+' : '-';
				}
				else if (c == 0 || c == box_w - 1)
				{
					at = border_attr;
					ch = '|';
				}
				else if (r == 1)
				{
					at = title_attr;
					ch = ' ';
				}

				VirtualScreen[((sy * cols + sx) << 1)] = (byte)ch;
				VirtualScreen[((sy * cols + sx) << 1) + 1] = at;
			}
			/* Right-edge drop shadow */
			if (box_x + box_w < cols && r > 0)
			{
				VirtualScreen[((sy * cols + box_x + box_w) << 1) + 1] = shadow_attr;
			}
		}
		/* Bottom-edge drop shadow */
		if (box_y + box_h < 24)
		{
			for (c = 1; c <= box_w && (box_x + c) < cols; c++)
			{
				VirtualScreen[(((box_y + box_h) * cols + box_x + c) << 1) + 1] = shadow_attr;
			}
		}

		/* Title text */
		ibm_vscr_putstr(box_x + 2, box_y + 1, title, title_attr);

		/* Menu items */
		for (i = 0; i < cnt; i++)
		{
			int sy = box_y + 2 + i;
			bool is_hov = (i == hovered);
			byte row_at = is_hov ? sel_attr : body_attr;
			byte badge_at = is_hov ? sel_attr : key_attr;
			char badge[6];

			for (c = 1; c < box_w - 1; c++)
			{
				VirtualScreen[((sy * cols + box_x + c) << 1)] = ' ';
				VirtualScreen[((sy * cols + box_x + c) << 1) + 1] = row_at;
			}

			sprintf(badge, "[%c] ", 'a' + i);
			ibm_vscr_putstr(box_x + 2, sy, badge, badge_at);
			ibm_vscr_putstr(box_x + 6, sy, items[i].label, row_at);
		}

		for (r = box_y; r <= box_y + box_h && r < 24; r++)
		{
			ibm_flush_row(r);
		}

		/* Check keyboard */
		if (bioskey(1))
		{
			int k = bioskey(0x10) & 0xFF;
			if (k == ESCAPE || k == ' ')
			{
				chosen = -1;
				break;
			}
			else if (k >= 'a' && k < 'a' + cnt)
			{
				chosen = k - 'a';
				break;
			}
			else if (k >= 'A' && k < 'A' + cnt)
			{
				chosen = k - 'A';
				break;
			}
			else if (k >= '1' && k <= '9' && (k - '1') < cnt)
			{
				chosen = k - '1';
				break;
			}
		}

		/* Check mouse */
		pressed = ibm_mouse_poll(&mx, &my);
		if (pressed & 1)
		{
			if (my >= box_y + 2 && my < box_y + 2 + cnt &&
			    mx >= box_x && mx < box_x + box_w)
			{
				chosen = my - (box_y + 2);
			}
			break;
		}
		else if (pressed & 2)
		{
			chosen = -1;
			break;
		}

		delay(10);
	}

	/* Restore VirtualScreen and flush */
	memcpy(VirtualScreen, saved_vscr, rows * cols * 2);
	C_FREE(saved_vscr, rows * cols * 2, byte);
	for (r = 0; r < 24; r++) ibm_flush_row(r);
	ibm_draw_dock();

	if (chosen < 0 || chosen >= cnt) return (FALSE);

	/* Execute selected context menu action */
	switch (items[chosen].action)
	{
	case CTX_RAW_CMD:
		mouse_cmd_raw = TRUE;
		Term_keypress(items[chosen].cmd);
		return (TRUE);

	case CTX_DIR_CMD:
		command_dir = items[chosen].dir;
		mouse_cmd_raw = TRUE;
		Term_keypress(items[chosen].cmd);
		return (TRUE);

	case CTX_AIM_CMD:
		if (has_mon)
		{
			target_who = c_ptr->m_idx;
			target_row = gy;
			target_col = gx;
			health_track(target_who);
		}
		else
		{
			target_who = -1;
			target_row = gy;
			target_col = gx;
		}
		command_dir = 5;
		if (items[chosen].cmd == 'm')
		{
			ibm_cast_or_skill();
		}
		else
		{
			mouse_cmd_raw = TRUE;
			Term_keypress(items[chosen].cmd);
		}
		return (TRUE);

	case CTX_BFS_TRAVEL:
		{
			int step_dir = mouse_find_step(gy, gx);
			if (step_dir > 0)
			{
				mouse_dest_y = gy;
				mouse_dest_x = gx;
				command_dir = step_dir;
				mouse_cmd_raw = TRUE;
				Term_keypress('.');
				return (TRUE);
			}
			return (FALSE);
		}

	case CTX_REST_HEAL:
		mouse_cmd_raw = TRUE;
		ibm_send_keys("R*\r");
		return (TRUE);

	case CTX_CAST_SPELL:
		ibm_cast_or_skill();
		return (TRUE);

	case CTX_TARGET_MON:
		if (has_mon)
		{
			char m_name[80];
			target_who = c_ptr->m_idx;
			target_row = gy;
			target_col = gx;
			health_track(target_who);
			monster_desc(m_name, m_ptr, 0x00);
			msg_format("Targeted %s.", m_name);
		}
		return (FALSE);

	case CTX_TARGET_LOC:
		target_who = -1;
		target_row = gy;
		target_col = gx;
		msg_format("Targeted location (%d, %d).", gy, gx);
		return (FALSE);

	case CTX_MON_LORE:
		if (has_mon)
		{
			int mx2, my2;
			character_icky = TRUE;
			Term_save();
			screen_roff(m_ptr->r_idx, m_ptr->ego, 0);
			Term_fresh();
			while (TRUE)
			{
				if (bioskey(1))
				{
					(void)bioskey(0x10);
					break;
				}
				if (ibm_mouse_poll(&mx2, &my2)) break;
				delay(10);
			}
			Term_load();
			character_icky = FALSE;
			Term_fresh();
		}
		return (FALSE);

	case CTX_EXAMINE_TILE:
		if (!p_ptr->wild_mode && c_ptr)
		{
			if (c_ptr->o_idx && o_list[c_ptr->o_idx].marked)
			{
				char o_name[80];
				object_desc(o_name, &o_list[c_ptr->o_idx], TRUE, 3);
				msg_format("You see %s (%s).", o_name, f_name + f_info[c_ptr->feat].name);
			}
			else if (c_ptr->info & (CAVE_MARK | CAVE_SEEN))
			{
				msg_format("You see %s.", f_name + f_info[c_ptr->feat].name);
			}
			else
			{
				msg_print("You see nothing there.");
			}
		}
		return (FALSE);
	}

	return (FALSE);
}

/* Display and run the comprehensive 2-column [+]More Commands menu (excludes dock shortcuts) */
static bool ibm_show_more_menu(void)
{
	typedef struct
	{
		char key;
		cptr label;
		char cmd;
		bool is_rest;
	} more_item_type;

	static const more_item_type more_items[30] =
	{
		/* Left column (15 items) */
		{ 'a', "Rest until healed (R)",    'R',     TRUE  },
		{ 'b', "Wait 1 turn (,)",          ',',     FALSE },
		{ 'c', "Search surroundings (s)",  's',     FALSE },
		{ 'd', "Toggle search mode (S)",   'S',     FALSE },
		{ 'e', "Look around (l)",          'l',     FALSE },
		{ 'f', "Full dungeon map (M)",     'M',     FALSE },
		{ '?', "Help / Tutorial (?)",      '?',     FALSE },
		{ 'h', "Wear / Wield item (w)",    'w',     FALSE },
		{ 'i', "Take off item (t)",        't',     FALSE },
		{ 'j', "Drop item (d)",            'd',     FALSE },
		{ 'k', "Destroy item (k)",         'k',     FALSE },
		{ 'l', "Eat food (E)",             'E',     FALSE },
		{ 'm', "Fuel light source (F)",    'F',     FALSE },
		{ 'n', "Throw item (v)",           'v',     FALSE },
		{ 'o', "Inspect item (I)",         'I',     FALSE },
		/* Right column (15 items) */
		{ 'p', "Use skill / power (m)",    'm',     FALSE },
		{ 'q', "Racial / Class power (U)", 'U',     FALSE },
		{ 'r', "Activate artifact (A)",    'A',     FALSE },
		{ 's', "Special abilities (N)",    'N',     FALSE },
		{ 't', "Browse spellbooks (b)",    'b',     FALSE },
		{ 'u', "Pray to god (p)",          'p',     FALSE },
		{ 'v', "Pet commands (P)",         'P',     FALSE },
		{ 'w', "Chat / Interact (Y)",      'Y',     FALSE },
		{ 'x', "Character sheet (C)",      'C',     FALSE },
		{ 'y', "Quest status (^Q)",        KTRL('Q'), FALSE },
		{ 'z', "Knowledge / Lore (~)",     '~',     FALSE },
		{ '1', "Message log (^P)",         KTRL('P'), FALSE },
		{ '2', "Game options (=)",         '=',     FALSE },
		{ '3', "Save game (^S)",           KTRL('S'), FALSE },
		{ '4', "Save & Quit (^X)",         KTRL('X'), FALSE }
	};

	const int rows_per_col = 15;
	const int box_x = 4, box_y = 4, box_w = 72, box_h = 18;
	const int col_w = 34;
	byte *saved_vscr;
	int r, c, i;
	int chosen = -1;

	C_MAKE(saved_vscr, rows * cols * 2, byte);
	memcpy(saved_vscr, VirtualScreen, rows * cols * 2);

	while (TRUE)
	{
		int hovered = -1;
		int pressed, mx, my;
		byte border_attr = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_WHITE) : 0x7F;
		byte title_attr  = use_color_complex ? (byte)((TERM_BLUE << 4) | TERM_YELLOW) : 0x1E;
		byte body_attr   = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_WHITE) : 0x70;
		byte key_attr    = use_color_complex ? (byte)((TERM_SLATE << 4) | TERM_YELLOW) : 0x7E;
		byte sel_attr    = use_color_complex ? (byte)((TERM_BLUE << 4) | TERM_YELLOW) : 0x1F;
		byte shadow_attr = use_color_complex ? (byte)((TERM_DARK << 4) | TERM_SLATE) : 0x08;

		if (mouse_y >= box_y + 2 && mouse_y < box_y + 2 + rows_per_col &&
		    mouse_x > box_x && mouse_x < box_x + box_w - 1)
		{
			int row_idx = mouse_y - (box_y + 2);
			int col_idx = (mouse_x >= box_x + 2 + col_w) ? 1 : 0;
			hovered = col_idx * rows_per_col + row_idx;
		}

		for (r = 0; r < box_h; r++)
		{
			int sy = box_y + r;
			for (c = 0; c < box_w; c++)
			{
				int sx = box_x + c;
				char ch = ' ';
				byte at = body_attr;

				if (r == 0 || r == box_h - 1)
				{
					at = border_attr;
					ch = (c == 0 || c == box_w - 1) ? '+' : '-';
				}
				else if (c == 0 || c == box_w - 1)
				{
					at = border_attr;
					ch = '|';
				}
				else if (r == 1)
				{
					at = title_attr;
					ch = ' ';
				}
				VirtualScreen[((sy * cols + sx) << 1)] = (byte)ch;
				VirtualScreen[((sy * cols + sx) << 1) + 1] = at;
			}
			if (box_x + box_w < cols && r > 0)
				VirtualScreen[((sy * cols + box_x + box_w) << 1) + 1] = shadow_attr;
		}
		if (box_y + box_h < 24)
		{
			for (c = 1; c <= box_w && (box_x + c) < cols; c++)
				VirtualScreen[(((box_y + box_h) * cols + box_x + c) << 1) + 1] = shadow_attr;
		}

		ibm_vscr_putstr(box_x + 2, box_y + 1, "More Commands (not on bottom dock)  -  [a-z, 1-4, ESC=cancel]", title_attr);

		for (i = 0; i < 30; i++)
		{
			int col_idx = i / rows_per_col;
			int row_idx = i % rows_per_col;
			int sx = box_x + 2 + col_idx * (col_w + 1);
			int sy = box_y + 2 + row_idx;
			bool is_hov = (i == hovered);
			byte row_at = is_hov ? sel_attr : body_attr;
			byte badge_at = is_hov ? sel_attr : key_attr;
			char badge[6];

			for (c = 0; c < col_w; c++)
			{
				VirtualScreen[((sy * cols + sx + c) << 1)] = ' ';
				VirtualScreen[((sy * cols + sx + c) << 1) + 1] = row_at;
			}
			sprintf(badge, "[%c] ", more_items[i].key);
			ibm_vscr_putstr(sx, sy, badge, badge_at);
			ibm_vscr_putstr(sx + 4, sy, more_items[i].label, row_at);
		}

		for (r = box_y; r <= box_y + box_h && r < 24; r++)
			ibm_flush_row(r);

		if (bioskey(1))
		{
			int k = bioskey(0x10) & 0xFF;
			if (k == ESCAPE || k == ' ')
			{
				chosen = -1;
				break;
			}
			for (i = 0; i < 30; i++)
			{
				if (tolower(k) == more_items[i].key)
				{
					chosen = i;
					break;
				}
			}
			if (chosen >= 0) break;
		}

		pressed = ibm_mouse_poll(&mx, &my);
		if (pressed & 1)
		{
			if (my >= box_y + 2 && my < box_y + 2 + rows_per_col &&
			    mx > box_x && mx < box_x + box_w - 1)
			{
				int row_idx = my - (box_y + 2);
				int col_idx = (mx >= box_x + 2 + col_w) ? 1 : 0;
				chosen = col_idx * rows_per_col + row_idx;
			}
			break;
		}
		else if (pressed & 2)
		{
			chosen = -1;
			break;
		}

		delay(10);
	}

	memcpy(VirtualScreen, saved_vscr, rows * cols * 2);
	C_FREE(saved_vscr, rows * cols * 2, byte);
	for (r = 0; r < 24; r++) ibm_flush_row(r);
	ibm_draw_dock();

	if (chosen < 0 || chosen >= 30) return (FALSE);

	mouse_cmd_raw = TRUE;
	if (more_items[chosen].is_rest)
	{
		ibm_send_keys("R*\r");
	}
	else
	{
		Term_keypress(more_items[chosen].cmd);
	}
	return (TRUE);
}

/* Scan a screen row for a selectable menu letter like "a) ", "  a - 2) ", or "[a] " */
static char ibm_scan_row_choice(int my, int mx, int btn)
{
	char line[81];
	char row0[81];
	int pos[16];
	char chs[16];
	int count = 0;
	int x, i, best;

	if (my < 1 || my >= 24) return (0);
	ibm_vscr_line(my, line);
	ibm_vscr_line(0, row0);

	/* Special store/building footer keywords on rows 18..23 */
	if (my >= 18)
	{
		if (strstr(line, "ESC)") && mx < 16) return (ESCAPE);
		if (strstr(line, "SPACE)") && mx < 20) return (' ');
		if (strstr(line, "-more-")) return (' ');
	}

	/* Collect all selectable option tokens on this row (handles both 1-col and multi-col grids) */
	for (x = 0; x < 77 && count < 16; x++)
	{
		char c = line[x];
		if ((isalnum((unsigned char)c)) &&
		    (x == 0 || line[x - 1] == ' ' || line[x - 1] == '(' || line[x - 1] == '['))
		{
			if ((line[x + 1] == ')' && line[x + 2] == ' ') ||
			    (x > 0 && line[x - 1] == '[' && line[x + 1] == ']' && line[x + 2] == ' ') ||
			    (line[x + 1] == ' ' && line[x + 2] == '-' && line[x + 3] == ' '))
			{
				pos[count] = x;
				chs[count] = c;
				count++;
			}
		}
	}

	if (count == 0) return (0);

	if (count == 1)
	{
		char c = chs[0];
		if (mx >= pos[0] - 2 || pos[0] <= 16)
		{
			if (btn == 2 && strstr(row0, "Descs") && islower((unsigned char)c))
				return ((char)toupper((unsigned char)c));
			return (c);
		}
		return (0);
	}

	/* Multi-column row: find the column containing mx */
	best = 0;
	for (i = 0; i < count; i++)
	{
		if (mx >= pos[i] - 1) best = i;
	}
	if (mx - pos[best] <= 22)
	{
		char c = chs[best];
		if (btn == 2 && strstr(row0, "Descs") && islower((unsigned char)c))
			return ((char)toupper((unsigned char)c));
		return (c);
	}

	return (0);
}

/* Process a mouse click (btn: 1 = Left, 2 = Right) at screen (mx, my). Returns TRUE if key(s) queued */
static bool ibm_handle_mouse_click(int mx, int my, int btn)
{
	char row0[81];
	char line[81];
	char line23[81];
	int b;

	/* 1. If running, resting, or repeating a command, any click aborts it */
	if (running || resting || command_rep)
	{
		Term_keypress(' ');
		return (TRUE);
	}

	/* 2. If "-more-" message prompt is active, any click advances it */
	if (msg_flag)
	{
		Term_keypress(' ');
		return (TRUE);
	}

	ibm_vscr_line(0, row0);
	ibm_vscr_line(my, line);
	ibm_vscr_line(23, line23);

	/* 3. Yes/No confirmation prompt on Row 0 or on the clicked row (e.g. "Use quick start (y/n)?") */
	if (strstr(row0, "[y/n]") || strstr(row0, "(y/n)") ||
	    strstr(row0, "[Y/N]") || strstr(row0, "(Y/N)") ||
	    strstr(line, "[y/n]") || strstr(line, "(y/n)") ||
	    strstr(line, "[Y/N]") || strstr(line, "(Y/N)"))
	{
		const char *yn = strstr(line, "(y/n)");
		if (!yn) yn = strstr(line, "[y/n]");
		if (!yn) yn = strstr(line, "(Y/N)");
		if (!yn) yn = strstr(line, "[Y/N]");
		if (!yn && my == 0)
		{
			yn = strstr(row0, "(y/n)");
			if (!yn) yn = strstr(row0, "[y/n]");
			if (!yn) yn = strstr(row0, "(Y/N)");
			if (!yn) yn = strstr(row0, "[Y/N]");
		}
		if (btn == 2)
		{
			Term_keypress('n');
			return (TRUE);
		}
		if (yn)
		{
			int yn_x = (int)(yn - ((my == 0 && yn >= row0 && yn < row0 + 81) ? row0 : line));
			if (mx >= yn_x + 3 && mx <= yn_x + 4)
			{
				Term_keypress('n');
				return (TRUE);
			}
		}
		Term_keypress('y');
		return (TRUE);
	}

	/* 4. Skill tree screen (G) */
	if (ibm_skill_mode == 1)
	{
		char line_mid[81];
		ibm_vscr_line(12, line_mid);
		if (strstr(line_mid, "(y/n)"))
		{
			Term_keypress((btn == 1) ? 'y' : 'n');
			return (TRUE);
		}
		if (my >= 7 && my < 24)
		{
			ibm_skill_click_y = my;
			ibm_skill_click_x = mx;
			ibm_skill_click_btn = btn;
			Term_keypress(' ');
			return (TRUE);
		}
		if (btn == 2 || my == 0 || my == 24)
		{
			Term_keypress(ESCAPE);
			return (TRUE);
		}
		return (FALSE);
	}

	/* 5. Targeting / Aiming / Direction prompts (ibm_target_mode 1..4) */
	if (ibm_target_mode > 0)
	{
		if (btn == 2)
		{
			Term_keypress(ESCAPE);
			return (TRUE);
		}
		if (my >= 1 && my <= 22 && mx >= 13 && mx <= 78)
		{
			int gy = panel_row_min + (my - 1);
			int gx = panel_col_min + (mx - 13);

			if (ibm_target_mode == 1)
			{
				ibm_target_y = gy;
				ibm_target_x = gx;
				Term_keypress('t');
				return (TRUE);
			}
			else if (ibm_target_mode == 2)
			{
				if (in_bounds(gy, gx) && cave[gy][gx].m_idx > 0 && target_able(cave[gy][gx].m_idx))
				{
					target_who = cave[gy][gx].m_idx;
					target_row = gy;
					target_col = gx;
					health_track(target_who);
					Term_keypress('5');
					return (TRUE);
				}
				else if (distance(p_ptr->py, p_ptr->px, gy, gx) == 1)
				{
					int d = ibm_dir_from_delta(gy - p_ptr->py, gx - p_ptr->px);
					if (d > 0)
					{
						Term_keypress('0' + d);
						return (TRUE);
					}
				}
				else if (in_bounds(gy, gx))
				{
					target_who = -1;
					target_row = gy;
					target_col = gx;
					Term_keypress('5');
					return (TRUE);
				}
			}
			else if (ibm_target_mode == 3)
			{
				int d = ibm_dir_from_delta(gy - p_ptr->py, gx - p_ptr->px);
				if (d > 0)
				{
					Term_keypress('0' + d);
					return (TRUE);
				}
			}
			else if (ibm_target_mode == 4)
			{
				ibm_target_y = gy;
				ibm_target_x = gx;
				Term_keypress(' ');
				return (TRUE);
			}
		}
		return (FALSE);
	}

	/* 6. Command Dock on Row 24 (only active when in-game) */
	if (my == 24)
	{
		if (!character_dungeon) return (FALSE);
		for (b = 0; b < DOCK_BTN_COUNT; b++)
		{
			if (mx >= dock_buttons[b].x0 && mx <= dock_buttons[b].x1)
			{
				/* If a modal menu/list is currently open, close it first */
				if (!inkey_flag || character_icky || command_see)
				{
					Term_keypress(ESCAPE);
					return (TRUE);
				}
				if (dock_buttons[b].cmd == '+')
				{
					return (ibm_show_more_menu());
				}
				else if (dock_buttons[b].cmd == 'm')
				{
					ibm_cast_or_skill();
					return (TRUE);
				}
				else
				{
					mouse_cmd_raw = TRUE;
					Term_keypress(dock_buttons[b].cmd);
					return (TRUE);
				}
			}
		}
		return (FALSE);
	}

	/* 7. Menus, Item Selection, Spell Lists, Stores, Options, Character Birth */
	if (!inkey_flag || character_icky || command_see || !character_dungeon)
	{
		char ch;
		char line11[81], line15[81], line20[81];
		ibm_vscr_line(11, line11);
		ibm_vscr_line(15, line15);
		ibm_vscr_line(20, line20);

		/* 7a. Options toggle screen ("RET to advance, y/n to set") */
		if (strstr(row0, "RET to advance"))
		{
			if (btn == 2)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
			if (my == 0)
			{
				const char *p_esc = strstr(row0, "ESC");
				const char *p_yn  = strstr(row0, "y/n");
				if (p_esc && mx >= (int)(p_esc - row0) - 2)
				{
					Term_keypress(ESCAPE);
					return (TRUE);
				}
				if (p_yn && mx >= (int)(p_yn - row0) && mx <= (int)(p_yn - row0) + 10)
				{
					Term_keypress((mx >= (int)(p_yn - row0) + 2) ? 'n' : 'y');
					return (TRUE);
				}
				Term_keypress('\r');
				return (TRUE);
			}
			if (my >= 2 && my <= 22 && line[48] == ':')
			{
				int cur_r = -1, n_opt = 0, r_idx;
				for (r_idx = 2; r_idx <= 22; r_idx++)
				{
					char rline[81];
					ibm_vscr_line(r_idx, rline);
					if (rline[48] == ':')
					{
						if ((VirtualScreen[((r_idx * cols) << 1) + 1] & 0x0F) == TERM_L_BLUE)
							cur_r = r_idx;
						n_opt++;
					}
				}
				if (cur_r >= 2 && n_opt > 0)
				{
					int steps = (my - cur_r + n_opt) % n_opt;
					int s_i;
					for (s_i = 0; s_i < steps; s_i++) Term_keypress('2');
					Term_keypress((line[50] == 'y') ? 'n' : 'y');
					Term_keypress('8');
					return (TRUE);
				}
			}
			if (my == 5 && strstr(line, "frequency"))
			{
				Term_keypress('f');
				return (TRUE);
			}
			return (FALSE);
		}

		/* 7b. Point-based stat allocation screen ("Total Cost xx/48") */
		if (strstr(row0, "Total Cost "))
		{
			if (my == 0)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
			if (my == 23)
			{
				const char *p_q   = strstr(line23, "'Q'");
				const char *p_s   = strstr(line23, "'S'");
				const char *p_esc = strstr(line23, "ESC");
				if (p_esc && mx >= (int)(p_esc - line23) - 3)
					Term_keypress(ESCAPE);
				else if (p_s && mx >= (int)(p_s - line23) - 1 && mx <= (int)(p_s - line23) + 16)
					Term_keypress('S');
				else if (p_q && mx >= (int)(p_q - line23) - 1 && mx <= (int)(p_q - line23) + 12)
					Term_keypress('Q');
				else
					Term_keypress(ESCAPE);
				return (TRUE);
			}
			if (my >= 2 && my <= 7)
			{
				int cur_stat = Term->scr->cy - 2;
				int target_stat = my - 2;
				if (cur_stat >= 0 && cur_stat <= 5)
				{
					int steps = (target_stat - cur_stat + 6) % 6;
					int s_i;
					for (s_i = 0; s_i < steps; s_i++) Term_keypress('2');
					Term_keypress((btn == 2) ? '4' : '6');
					return (TRUE);
				}
			}
			if (btn == 2)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
			return (FALSE);
		}

		/* 7c. Autoroller / Stat Roll screen ("['r' to reroll, ... or ESC to accept]") */
		if (strstr(line11, "Hit ESC to abort"))
		{
			Term_keypress(ESCAPE);
			return (TRUE);
		}
		if (strstr(line23, "to reroll"))
		{
			if (btn == 2)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
			if (my == 23)
			{
				const char *p_prev = strstr(line23, "'p'");
				const char *p_hist = strstr(line23, "'h'");
				const char *p_esc  = strstr(line23, "ESC");
				if (p_esc && mx >= (int)(p_esc - line23) - 3)
					Term_keypress(ESCAPE);
				else if (p_hist && mx >= (int)(p_hist - line23) - 1)
					Term_keypress('h');
				else if (p_prev && mx >= (int)(p_prev - line23) - 1)
					Term_keypress('p');
				else
					Term_keypress('r');
				return (TRUE);
			}
			if (my >= 15 && my <= 21)
			{
				Term_keypress('h');
				return (TRUE);
			}
			return (FALSE);
		}

		/* 7d. Character Background Edit Mode */
		if (strstr(line15, "Character Background - Edit Mode"))
		{
			Term_keypress((btn == 2) ? ESCAPE : '\r');
			return (TRUE);
		}

		/* 7e. Text / Number input prompts (askfor_aux / get_name) */
		if (ibm_numeric_input && ibm_numeric_bar_y >= 0)
		{
			if (btn == 2)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
			if (my == ibm_numeric_bar_y)
			{
				if (mx >= 2 && mx <= 6)        { ibm_numeric_action = 1; Term_keypress('\x1f'); }
				else if (mx >= 8 && mx <= 12)  { ibm_numeric_action = 2; Term_keypress('\x1f'); }
				else if (mx >= 14 && mx <= 17) { ibm_numeric_action = 3; Term_keypress('\x1f'); }
				else if (mx >= 19 && mx <= 22) { ibm_numeric_action = 4; Term_keypress('\x1f'); }
				else if (mx >= 24 && mx <= 28) { ibm_numeric_action = 5; Term_keypress('\x1f'); }
				else if (mx >= 30 && mx <= 34) { ibm_numeric_action = 6; Term_keypress('\x1f'); }
				else if (mx >= 36 && mx <= 39) { Term_keypress('\r'); }
				else if (mx >= 41 && mx <= 45) { Term_keypress(ESCAPE); }
				return (TRUE);
			}
			Term_keypress('\r');
			return (TRUE);
		}
		if (strstr(line23, "Enter the name of") ||
		    strstr(line23, "Enter your player's name") ||
		    strstr(line20, "Number of quests?") ||
		    strstr(row0, "Quantity ("))
		{
			Term_keypress((btn == 2) ? ESCAPE : '\r');
			return (TRUE);
		}

		/* 7f. Final character confirmation prompt ("['Q' to suicide, 'S' to start over, or ESC to continue]") */
		if (strstr(line23, "ESC to continue") || strstr(line23, "to start over"))
		{
			if (btn == 2 || my != 23)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
			{
				const char *p_q   = strstr(line23, "'Q'");
				const char *p_s   = strstr(line23, "'S'");
				const char *p_esc = strstr(line23, "ESC");
				if (p_esc && mx >= (int)(p_esc - line23) - 3)
					Term_keypress(ESCAPE);
				else if (p_s && mx >= (int)(p_s - line23) - 1)
					Term_keypress('S');
				else if (p_q && mx <= (int)(p_q - line23) + 15)
					Term_keypress('Q');
				else
					Term_keypress(ESCAPE);
				return (TRUE);
			}
		}

		/* 7g. Right-click in menus (unless right-clicking a spell for its description) cancels/exits */
		if (btn == 2)
		{
			ch = ibm_scan_row_choice(my, mx, 2);
			if (ch && isupper((unsigned char)ch) && strstr(row0, "Descs"))
			{
				Term_keypress(ch);
				return (TRUE);
			}
			Term_keypress(ESCAPE);
			return (TRUE);
		}

		/* 7h. Inline keywords on clicked row ('* for random', '= for options', '? for help', 'Q' to quit, 'S' to restart, etc.) */
		{
			const char *p_rand = strstr(line, "* for");
			const char *p_opt  = strstr(line, "= for");
			const char *p_help = strstr(line, "'?' for help");
			const char *p_quit = strstr(line, "'Q' to quit");
			const char *p_rest = strstr(line, "'S' to restart");
			const char *p_ret  = strstr(line, "Return to select");
			const char *p_bs   = strstr(line, "Backspace to delete");
			const char *p_escq = strstr(line, "Esc to quit");

			if (p_opt && mx >= (int)(p_opt - line) - 1 && mx <= (int)(p_opt - line) + 14)
			{
				Term_keypress('=');
				return (TRUE);
			}
			if (p_rand && mx >= (int)(p_rand - line) - 1 && mx <= (int)(p_rand - line) + 18)
			{
				Term_keypress('*');
				return (TRUE);
			}
			if (p_help && mx >= (int)(p_help - line) - 1 && mx <= (int)(p_help - line) + 13)
			{
				Term_keypress('?');
				return (TRUE);
			}
			if (p_quit && mx >= (int)(p_quit - line) - 1 && mx <= (int)(p_quit - line) + 12)
			{
				Term_keypress('Q');
				return (TRUE);
			}
			if (p_rest && mx >= (int)(p_rest - line) - 1 && mx <= (int)(p_rest - line) + 15)
			{
				Term_keypress('S');
				return (TRUE);
			}
			if (p_ret && mx >= (int)(p_ret - line) && mx <= (int)(p_ret - line) + 16)
			{
				Term_keypress('\r');
				return (TRUE);
			}
			if (p_bs && mx >= (int)(p_bs - line) && mx <= (int)(p_bs - line) + 19)
			{
				Term_keypress('\010');
				return (TRUE);
			}
			if (p_escq)
			{
				Term_keypress(ESCAPE);
				return (TRUE);
			}
		}

		/* Row 0 prompt shortcuts ('/', '-', '*', '?', 'ESC') */
		if (my == 0)
		{
			char c0 = ibm_vscr_char(mx, 0);
			const char *p_star = strchr(row0, '*');
			const char *p_esc  = strstr(row0, "ESC");

			if (c0 == '/' || c0 == '-' || c0 == '*' || c0 == '?')
			{
				Term_keypress(c0);
				return (TRUE);
			}
			if (p_star)
			{
				int sx = (int)(p_star - row0);
				if (mx >= sx - 1 && mx <= sx + 8)
				{
					Term_keypress('*');
					return (TRUE);
				}
			}
			if (p_esc)
			{
				int ex = (int)(p_esc - row0);
				if (mx >= ex - 1 && mx <= ex + 8)
				{
					Term_keypress(ESCAPE);
					return (TRUE);
				}
			}
		}

		ch = ibm_scan_row_choice(my, mx, btn);
		if (ch)
		{
			/* If inside a store at top-level command prompt and clicking a store stock row (6..17), buy/get it */
			if (character_icky && my >= 6 && my <= 17 && islower((unsigned char)ch))
			{
				char line22[81];
				ibm_vscr_line(22, line22);
				if (strstr(line22, "Purchase an item"))
				{
					mouse_cmd_raw = TRUE;
					Term_keypress('p');
					Term_keypress(ch);
					return (TRUE);
				}
				if (strstr(line22, "Get an item"))
				{
					mouse_cmd_raw = TRUE;
					Term_keypress('g');
					Term_keypress(ch);
					return (TRUE);
				}
			}
			Term_keypress(ch);
			return (TRUE);
		}

		/* Click on "[Press any key to continue]" or initial news/title screen advances */
		{
			char line1[81], line6[81];
			ibm_vscr_line(1, line1);
			ibm_vscr_line(6, line6);
			if (strstr(line23, "Press any key") || strstr(row0, "Press any key") ||
			    strstr(line, "Press any key") || strstr(line, "-more-"))
			{
				Term_keypress(' ');
				return (TRUE);
			}
			/* Title/News splash screen (before begin_screen or player_birth) */
			if (!character_dungeon && !strstr(line1, "Welcome to") && !strstr(line6, "New Character"))
			{
				char line2[81];
				ibm_vscr_line(2, line2);
				if (!strstr(line2, "Name  :"))
				{
					Term_keypress(' ');
					return (TRUE);
				}
			}
		}

		return (FALSE);
	}

	/* 8. Normal Dungeon / Town / Wilderness Map Mode (inkey_flag == TRUE) */
	if (my >= 1 && my <= 22 && mx < 13)
	{
		/* Sidebar click: left-click top half opens Character Sheet, otherwise Player Context Menu */
		if (btn == 1 && my <= 12)
		{
			mouse_cmd_raw = TRUE;
			Term_keypress('C');
			return (TRUE);
		}
		return (ibm_show_context_menu(p_ptr->py, p_ptr->px, mx, my));
	}

	if (my >= 1 && my <= 22 && mx >= 13 && mx <= 78)
	{
		int gy = panel_row_min + (my - 1);
		int gx = panel_col_min + (mx - 13);
		int dist, dir;
		cave_type *c_ptr;

		/* Right-click on map always opens the styled Context Menu */
		if (btn == 2)
		{
			return (ibm_show_context_menu(gy, gx, mx, my));
		}

		/* Left-click on map in Wilderness Mode */
		if (p_ptr->wild_mode)
		{
			if (gy == p_ptr->py && gx == p_ptr->px)
			{
				mouse_cmd_raw = TRUE;
				Term_keypress('>');
				return (TRUE);
			}
			dir = ibm_dir_from_delta(gy - p_ptr->py, gx - p_ptr->px);
			if (dir > 0)
			{
				command_dir = dir;
				mouse_cmd_raw = TRUE;
				Term_keypress(';');
				return (TRUE);
			}
			return (FALSE);
		}

		if (!in_bounds(gy, gx)) return (FALSE);
		c_ptr = &cave[gy][gx];
		dist = distance(p_ptr->py, p_ptr->px, gy, gx);
		dir = ibm_dir_from_delta(gy - p_ptr->py, gx - p_ptr->px);

		/* Left-click on Player's own tile */
		if (dist == 0)
		{
			if (c_ptr->o_idx)
			{
				mouse_cmd_raw = TRUE;
				Term_keypress('g');
				return (TRUE);
			}
			if (c_ptr->feat == FEAT_LESS || c_ptr->feat == FEAT_WAY_LESS ||
			    c_ptr->feat == FEAT_SHAFT_UP || c_ptr->feat == FEAT_QUEST_UP)
			{
				mouse_cmd_raw = TRUE;
				Term_keypress('<');
				return (TRUE);
			}
			if (c_ptr->feat == FEAT_MORE || c_ptr->feat == FEAT_WAY_MORE ||
			    c_ptr->feat == FEAT_SHAFT_DOWN || c_ptr->feat == FEAT_QUEST_DOWN ||
			    c_ptr->feat == FEAT_QUEST_ENTER || c_ptr->feat == FEAT_QUEST_EXIT)
			{
				mouse_cmd_raw = TRUE;
				Term_keypress('>');
				return (TRUE);
			}
			if (c_ptr->feat == FEAT_FOUNTAIN)
			{
				mouse_cmd_raw = TRUE;
				Term_keypress('H');
				return (TRUE);
			}
			if (c_ptr->feat >= FEAT_ALTAR_HEAD && c_ptr->feat <= FEAT_ALTAR_TAIL)
			{
				return (ibm_show_context_menu(gy, gx, mx, my));
			}
			mouse_cmd_raw = TRUE;
			Term_keypress(',');
			return (TRUE);
		}

		/* Left-click on an Adjacent Tile (dist == 1) */
		if (dist == 1)
		{
			/* Monster: attack/interact */
			if (c_ptr->m_idx > 0 && m_list[c_ptr->m_idx].ml)
			{
				command_dir = dir;
				mouse_cmd_raw = TRUE;
				Term_keypress(';');
				return (TRUE);
			}
			/* Closed door or chest: open */
			if ((c_ptr->feat >= FEAT_DOOR_HEAD && c_ptr->feat <= FEAT_DOOR_TAIL) || ibm_has_chest(gy, gx))
			{
				command_dir = dir;
				mouse_cmd_raw = TRUE;
				Term_keypress('o');
				return (TRUE);
			}
			/* Known trap: disarm */
			if ((c_ptr->info & CAVE_TRDT) && c_ptr->t_idx != 0)
			{
				command_dir = dir;
				mouse_cmd_raw = TRUE;
				Term_keypress('D');
				return (TRUE);
			}
			/* Passable floor / trees / shop: step */
			if (cave_floor_bold(gy, gx) || c_ptr->feat == FEAT_TREES || c_ptr->feat == FEAT_SHOP)
			{
				command_dir = dir;
				mouse_cmd_raw = TRUE;
				Term_keypress(';');
				return (TRUE);
			}
			/* Tunnelable wall / rubble: tunnel */
			if (f_info[c_ptr->feat].flags1 & FF1_TUNNELABLE)
			{
				command_dir = dir;
				mouse_cmd_raw = TRUE;
				Term_keypress('T');
				return (TRUE);
			}
			/* Fallback: open Context Menu */
			return (ibm_show_context_menu(gy, gx, mx, my));
		}

		/* Left-click on a Distant Tile (dist > 1) */
		if (c_ptr->m_idx > 0 && m_list[c_ptr->m_idx].ml)
		{
			/* Distant visible monster: open Monster Context Menu */
			return (ibm_show_context_menu(gy, gx, mx, my));
		}

		/* Try BFS pathfinding to (gy, gx) */
		{
			int step_dir = mouse_find_step(gy, gx);
			if (step_dir > 0)
			{
				mouse_dest_y = gy;
				mouse_dest_x = gx;
				command_dir = step_dir;
				mouse_cmd_raw = TRUE;
				Term_keypress('.');
				return (TRUE);
			}
		}

		/* If unreachable / unexplored wall, open Context Menu */
		return (ibm_show_context_menu(gy, gx, mx, my));
	}

	return (FALSE);
}



/*
 * Process an event (check for a keypress)
 *
 * The keypress processing code is often the most system dependant part
 * of Angband, since sometimes even the choice of compiler is important.
 *
 * For the IBM, we divide all keypresses into two categories, first, the
 * "normal" keys, including all keys required to play Angband, and second,
 * the "special" keys, such as keypad keys, function keys, and various keys
 * used in combination with various modifier keys.
 *
 * To simplify this file, we use Angband's "macro processing" ability, in
 * combination with a specialized "pref-ibm.prf" file, to handle most of the
 * "special" keys, instead of attempting to fully analyze them here.  This
 * file only has to determine when a "special" key has been pressed, and
 * translate it into a simple string which signals the use of a "special"
 * key, the set of modifiers used, if any, and the hardware scan code of
 * the actual key which was pressed.  To simplify life for the user, we
 * treat both "shift" keys as identical modifiers.
 *
 * The final encoding is "^_MMMxSS\r", where "MMM" encodes the modifiers
 * ("C" for control, "S" for shift, "A" for alt, or any ordered combination),
 * and "SS" encodes the keypress (as the two "digit" hexadecimal encoding of
 * the scan code of the key that was pressed), and the "^_" and "x" and "\r"
 * delimit the encoding for recognition by the macro processing code.
 *
 * Some important facts about scan codes follow.  All "normal" keys use
 * scan codes from 1-58.  The "function" keys use 59-68 (and 133-134).
 * The "keypad" keys use 69-83.  Escape uses 1.  Enter uses 28.  Control
 * uses 29.  Left Shift uses 42.  Right Shift uses 54.  PrtScrn uses 55.
 * Alt uses 56.  Space uses 57.  CapsLock uses 58.  NumLock uses 69.
 * ScrollLock uses 70.  The "keypad" keys which use scan codes 71-83
 * are ordered KP7,KP8,KP9,KP-,KP4,KP5,KP6,KP+,KP1,KP2,KP3,INS,DEL.
 *
 * Using "bioskey(0x10)" instead of "bioskey(0)" apparently provides more
 * information, including better access to the keypad keys in combination
 * with various modifiers, but only works on "PC's after 6/1/86", and there
 * is no way to determine if the function is provided on a machine.  I have
 * been told that without it you cannot detect, for example, control-left.
 * The basic scan code + ascii value pairs returned by the keypad follow,
 * with values in parentheses only available to "bioskey(0x10)".
 *
 *         /      *      -      +      1      2      3      4
 * Norm:  352f   372a   4a2d   4e2b   4f00   5000   5100   4b00
 * Shft:  352f   372a   4a2d   4e2b   4f31   5032   5133   4b34
 * Ctrl: (9500) (9600) (8e00) (9000)  7500  (9100)  7600   7300
 *
 *         5      6      7      8      9      0      .     Enter
 * Norm: (4c00)  4d00   4700   4800   4900   5200   5300  (e00d)
 * Shft:  4c35   4d36   4737   4838   4939   5230   532e  (e00d)
 * Ctrl: (8f00)  7400   7700  (8d00)  8400  (9200) (9300) (e00a)
 *
 * See "pref-ibm.prf" for the "standard" macros for various keys.
 *
 * Certain "bizarre" keypad keys (such as "enter") return a "scan code"
 * of "0xE0", and a "usable" ascii value.  These keys should be treated
 * like the normal keys, see below.  XXX XXX XXX Note that these "special"
 * keys could be prefixed with an optional "ctrl-^" which would allow them
 * to be used in macros without hurting their use in normal situations.
 */
static errr Term_xtra_ibm_event(int v)
{
	int i, k, s;

	bool mc = FALSE;
	bool ms = FALSE;
	bool ma = FALSE;


	/* Non-blocking check (e.g. during running/resting/macro scan) */
	if (!v)
	{
		if (!bioskey(1))
		{
			if (mouse_present)
			{
				int mx, my;
				int pressed = ibm_mouse_poll(&mx, &my);
				if ((pressed & 3) && (running || resting || command_rep))
				{
					Term_keypress(' ');
					return (0);
				}
			}
			return (1);
		}
	}
	else
	{
		/* Blocking wait for keyboard or mouse event */
		ibm_draw_dock();
		if (mouse_present && mouse_y != 24)
		{
			ibm_flush_row(mouse_y);
		}
		while (!bioskey(1))
		{
			if (mouse_present)
			{
				int mx, my;
				int pressed = ibm_mouse_poll(&mx, &my);
				if (pressed & 1)
				{
					if (ibm_handle_mouse_click(mx, my, 1)) return (0);
				}
				else if (pressed & 2)
				{
					if (ibm_handle_mouse_click(mx, my, 2)) return (0);
				}
			}
			delay(5);
		}
	}

	/* Wait for a keypress */
	k = bioskey(0x10);

	/* Access the "modifiers" */
	i = bioskey(2);

	/* Extract the "scan code" */
	s = ((k >> 8) & 0xFF);

	/* Extract the "ascii value" */
	k = (k & 0xFF);

	/* Process "normal" keys */
	if ((s <= 58) || (s == 0xE0))
	{
		/* Enqueue it */
		if (k) Term_keypress(k);

		/* Success */
		return (0);
	}

	/* Extract the modifier flags */
	if (i & (1 << K_CTRL)) mc = TRUE;
	if (i & (1 << K_LSHIFT)) ms = TRUE;
	if (i & (1 << K_RSHIFT)) ms = TRUE;
	if (i & (1 << K_ALT)) ma = TRUE;


	/* Begin a "macro trigger" */
	Term_keypress(31);

	/* Hack -- Send the modifiers */
	if (mc) Term_keypress('C');
	if (ms) Term_keypress('S');
	if (ma) Term_keypress('A');

	/* Introduce the hexidecimal scan code */
	Term_keypress('x');

	/* Encode the hexidecimal scan code */
	Term_keypress(hexsym[s / 16]);
	Term_keypress(hexsym[s % 16]);

	/* End the "macro trigger" */
	Term_keypress(13);

	/* Success */
	return (0);
}


/*
 * Handle a "special request"
 *
 * The given parameters are "valid".
 */
static errr Term_xtra_ibm(int n, int v)
{
	int i;

	/* Analyze the request */
	switch (n)
	{
		/* Make a "bell" noise */
	case TERM_XTRA_NOISE:
		{
			/* Make a bell noise */
			(void)write(1, "\007", 1);

			/* Success */
			return (0);
		}

		/* Set the cursor shape */
	case TERM_XTRA_SHAPE:
		{
			/* Set cursor shape */
			curs_set(v);

			/* Success */
			return (0);
		}

		/* Flush one line of output */
	case TERM_XTRA_FROSH:
		{

#ifdef USE_VIRTUAL

			/* Apply the virtual screen line (with mouse cursor overlay) to the physical screen */
			ibm_flush_row(v);

#endif /* USE_VIRTUAL */

			/* Success */
			return (0);
		}

		/* Clear the screen */
	case TERM_XTRA_CLEAR:
		{

#ifdef USE_CONIO

			/* Clear the screen */
			clrscr();

#else /* USE_CONIO */

			/* Clear each line (virtual or physical) */
			for (i = 0; i < rows; i++)
			{
				/* Clear the line */
				memcpy((VirtualScreen + ((i*cols) << 1)), wiper, (cols << 1));
			}

# ifdef USE_VIRTUAL

# ifdef USE_WAT

			/* Copy the virtual screen to the physical screen */
			memcpy(PhysicalScreen, VirtualScreen, 25*80*2);

# else /* USE_WAT */

			/* Erase the physical screen */
			ScreenClear();

# endif  /* USE_WAT */

			/* Keep the Command Dock on Row 24 visible (if in-game) */
			ibm_draw_dock();

			/* Keep mouse cursor visible */
			if (mouse_present && mouse_y != 24)
			{
				ibm_flush_row(mouse_y);
			}

# endif  /* USE_VIRTUAL */

#endif /* USE_CONIO */

			/* Success */
			return (0);
		}

		/* Process events */
	case TERM_XTRA_EVENT:
		{
			irc_poll();

			/* Process one event */
			return (Term_xtra_ibm_event(v));
		}

		/* Flush events */
	case TERM_XTRA_FLUSH:
		{
			/* Strip keyboard events */
			while (bioskey(1)) (void)bioskey(0x10);

			/* Sync mouse button state without triggering actions */
			if (mouse_present)
			{
				int mx, my;
				(void)ibm_mouse_poll(&mx, &my);
			}

			/* Success */
			return (0);
		}

		/* React to global changes */
	case TERM_XTRA_REACT:
		{
			/* React to "color_table" changes */
			return (Term_xtra_ibm_react());
		}

		/* Delay for some milliseconds */
	case TERM_XTRA_DELAY:
		{
			/* Delay if needed */
			if (v > 0) delay(v);

			/* Success */
			return (0);
		}

		/*
		 * Scans for subdirectories in a directory "scansubdir_dir"
		 * and place teh result in "scansubdir_result/scansubdir_max"
		 */
	case TERM_XTRA_SCANSUBDIR:
		{
			struct ffblk f;
			int done;

			done = findfirst(format("%s\\*", scansubdir_dir), &f, FA_DIREC);

			scansubdir_max = 0;
			while ((!done) && (scansubdir_max < 255))
			{
				if ((f.ff_attrib & FA_DIREC) && (strcmp(f.ff_name, ".")) && (strcmp(f.ff_name, "..")))
				{
					string_free(scansubdir_result[scansubdir_max]);
					scansubdir_result[scansubdir_max] = string_make(f.ff_name);
					scansubdir_max++;
				}

				done = findnext(&f);
			}

			return 0;
		}
	}

	/* Unknown request */
	return (1);
}



/*
 * Move the cursor
 *
 * The given parameters are "valid".
 */
static errr Term_curs_ibm(int x, int y)
{

#ifdef USE_WAT

# ifdef USE_CONIO

	/* Place the cursor */
	gotoxy(x + 1, y + 1);

# else /* USE_CONIO */

	union REGS r;

	r.h.ah = 2;
	r.h.bh = 0;
	r.h.dl = x;
	r.h.dh = y;

	/* Place the cursor */
	int86(0x10, &r, &r);

# endif  /* USE_CONIO */

#else /* USE_WAT */

	/* Move the cursor */
	ScreenSetCursor(y, x);

#endif /* USE_WAT */

	/* Success */
	return (0);
}


/*
 * Erase a block of the screen
 *
 * The given parameters are "valid".
 */
static errr Term_wipe_ibm(int x, int y, int n)
{

#ifdef USE_CONIO

	/* Wipe the region */
	window(x + 1, y + 1, x + n, y + 1);
	clrscr();
	window(1, 1, cols, rows);

#else /* USE_CONIO */

	/* Wipe part of the virtual (or physical) screen */
	memcpy(VirtualScreen + ((cols*y + x) << 1), wiper, n << 1);

#endif /* USE_CONIO */

	/* Success */
	return (0);
}


/*
 * Place some text on the screen using an attribute
 *
 * The given parameters are "valid".  Be careful with "a".
 *
 * The string "cp" has length "n" and is NOT null-terminated.
 */
static errr Term_text_ibm(int x, int y, int n, byte a, const char *cp)
{
	register int i;
	register byte attr;
	register byte *dest;


	/* Handle "complex" color */
	if (use_color_complex)
	{
		/* Extract a color index */
		attr = (a & 0x0F);
	}

	/* Handle "simple" color */
	else
	{
		/* Extract a color value */
		attr = ibm_color_simple[a & 0x0F];
	}

#ifdef USE_CONIO

	/* Place the cursor */
	gotoxy(x + 1, y + 1);

	/* Set the attribute */
	textattr(attr);

	/* Dump the text */
	for (i = 0; i < n; i++) putch(cp[i]);

#else /* USE_CONIO */

	/* Access the virtual (or physical) screen */
	dest = VirtualScreen + (((cols * y) + x) << 1);

	/* Save the data */
	for (i = 0; i < n; i++)
	{
		/* Apply */
		*dest++ = cp[i];
		*dest++ = attr;
	}

#endif /* USE_CONIO */

	/* Success */
	return (0);
}


/*
 * Place some attr/char pairs on the screen
 *
 * The given parameters are "valid".
 */
#ifdef USE_TRANSPARENCY
static errr Term_pict_ibm(int x, int y, int n, const byte *ap, const char *cp, const byte *tap, const char *tcp)
#else /* USE_TRANSPARENCY */
static errr Term_pict_ibm(int x, int y, int n, const byte *ap, const char *cp)
#endif /* USE_TRANSPARENCY */
{
	register int i;
	register byte attr;
	register byte *dest;


#ifdef USE_CONIO

	/* Place the cursor */
	gotoxy(x + 1, y + 1);

	/* Dump the text */
	for (i = 0; i < n; i++)
	{
		/* Handle "complex" color */
		if (use_color_complex)
		{
			/* Extract a color index */
			attr = (ap[i] & 0x0F);
		}

		/* Handle "simple" color */
		else
		{
			/* Extract a color value */
			attr = ibm_color_simple[ap[i] & 0x0F];
		}

		/* Set the attribute */
		textattr(attr);

		/* Dump the char */
		putch(cp[i]);
	}

#else /* USE_CONIO */

	/* Access the virtual (or physical) screen */
	dest = VirtualScreen + (((cols * y) + x) << 1);

	/* Save the data */
	for (i = 0; i < n; i++)
	{
		/* Handle "complex" color */
		if (use_color_complex)
		{
			/* Extract a color index */
			attr = (ap[i] & 0x0F);
		}

		/* Handle "simple" color */
		else
		{
			/* Extract a color value */
			attr = ibm_color_simple[ap[i] & 0x0F];
		}

		/* Apply */
		*dest++ = cp[i];
		*dest++ = attr;
	}

#endif /* USE_CONIO */

	/* Success */
	return (0);
}


/*
 * Init a Term
 */
static void Term_init_ibm(term *t)
{
	/* XXX Nothing */
}


/*
 * Nuke a Term
 */
static void Term_nuke_ibm(term *t)
{

#ifdef USE_WAT

	/* Nothing */

#else /* USE_WAT */

	union REGS r;

#endif /* USE_WAT */

	/* Move the cursor to the bottom of the screen */
	Term_curs_ibm(0, rows - 1);

#ifdef USE_WAT

	/* Restore the original video mode */
	_setvideomode(_DEFAULTMODE);

#else /* USE_WAT */

	/* Restore the original video mode */
	r.h.ah = 0x00;
	r.h.al = 0x03;
	int86(0x10, &r, &r);

#endif /* USE_WAT */

	/* Make the cursor visible */
	curs_set(1);
}



#ifdef USE_GRAPHICS

#ifdef USE_286

/*
 * In 286 mode we don't need to worry about translating from a 32bit
 * pointer to a 16 bit pointer so we just call the interrupt function
 *
 * Note the use of "intr()" instead of "int86()" so we can pass
 * segment registers.
 */
void enable_graphic_font(void *font)
{
	union REGPACK regs =
		        {0};

	regs.h.ah = 0x11;            /* Text font function */
	regs.h.bh = 0x10;            /* Size of a character -- 16 bytes */
	regs.h.cl = 0xFF;            /* Last character in font */
	regs.x.es = FP_SEG(font);    /* Pointer to font */
	regs.x.bp = FP_OFF(font);
	intr(0x10, &regs);
}

#else /* USE_286 */

#ifdef USE_WAT

/*
* This structure is used by the DMPI function to hold registers when
* doing a real mode interrupt call.  (Stolen from the DJGPP <dpmi.h>
                                      * header file).
*/

typedef union
{
	struct
	{
		unsigned long edi;
		unsigned long esi;
		unsigned long ebp;
		unsigned long res;
		unsigned long ebx;
		unsigned long edx;
		unsigned long ecx;
		unsigned long eax;
	}
	d;
	struct
	{
		unsigned short di, di_hi;
		unsigned short si, si_hi;
		unsigned short bp, bp_hi;
		unsigned short res, res_hi;
		unsigned short bx, bx_hi;
		unsigned short dx, dx_hi;
		unsigned short cx, cx_hi;
		unsigned short ax, ax_hi;
		unsigned short flags;
		unsigned short es;
		unsigned short ds;
		unsigned short fs;
		unsigned short gs;
		unsigned short ip;
		unsigned short cs;
		unsigned short sp;
		unsigned short ss;
	}
	x;
	struct
	{
		unsigned char edi[4];
		unsigned char esi[4];
		unsigned char ebp[4];
		unsigned char res[4];
		unsigned char bl, bh, ebx_b2, ebx_b3;
		unsigned char dl, dh, edx_b2, edx_b3;
		unsigned char cl, ch, ecx_b2, ecx_b3;
		unsigned char al, ah, eax_b2, eax_b3;
	}
	h;
} __dpmi_regs;

unsigned __dpmi_allocate_dos_memory(int size, unsigned *selector)
{
	union REGPACK regs =
		        {0};

	regs.w.ax = 0x100;    /* DPMI function -- allocate low memory */
	regs.w.bx = size;     /* Number of Paragraphs to allocate */
	intr(0x31, &regs);     /* DPMI interface */

	*selector = regs.w.dx;
	return (regs.w.ax);
}

void __dpmi_free_dos_memory(unsigned sel)
{
	union REGPACK regs =
		        {0};

	regs.w.ax = 0x101;       /* DPMI function -- free low memory */
	regs.x.edx = sel;         /* PM selector for memory block */
	intr(0x31, &regs);        /* DPMI interface */
}

void __dpmi_int(int intno, __dpmi_regs *dblock)
{
	union REGPACK regs =
		        {0};

	regs.w.ax = 0x300;            /* DPMI function -- real mode interrupt */
	regs.h.bl = intno;            /* interrupt 0x10 */
	regs.x.edi = FP_OFF(dblock);   /* Pointer to dblock (offset and segment) */
	regs.x.es = FP_SEG(dblock);
	intr(0x31, &regs);             /* DPMI interface */
}

unsigned short __dpmi_sel = 0x0000;
#define _farsetsel(x) __dpmi_sel=(x)
extern void _farnspokeb(unsigned long offset, unsigned char value);
#pragma aux _farnspokeb = \
"push   fs" \
"mov    fs,__dpmi_sel" \
"mov    fs:[eax],bl" \
"pop    fs" \
parm [eax] [bl]; 

#else /* USE_WAT */

#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>

#endif /* USE_WAT */


/*
* Since you cannot send 32bit pointers to a 16bit interrupt handler
* and the video BIOS wants a (16bit) pointer to the font, we have
* to allocate a block of dos memory, copy the font into it, then
* translate a 32bit pointer into a 16bit pointer to that block.
*
* DPMI - Dos Protected Mode Interface provides functions that let
*        us do that.
*/
void enable_graphic_font(const char *font)
{
	__dpmi_regs dblock = {{0}};

	unsigned seg, sel, i;

	/*
	* Allocate a block of memory 4096 bytes big in `low memory' so a real
	* mode interrupt can access it.  Real mode pointer is returned as seg:0
	* Protected mode pointer is sel:0.
	*/
	seg = __dpmi_allocate_dos_memory(256, &sel);

	/* Copy the information into low memory buffer, by copying one byte at
	* a time.  According to the info in <sys/farptr.h>, the functions
	* _farsetsel() and _farnspokeb() will optimise away completely
	*/
	_farsetsel(sel);                /* Set the selector to write to */
	for (i = 0; i < 4096; i++)
	{
		_farnspokeb(i, *font++);       /* Copy 1 byte into low (far) memory */
	}

	/*
	* Now we use DPMI as a jumper to call the real mode interrupt.  This
	* is needed because loading `es' while in protected mode with a real
	* mode pointer will cause an Protection Fault and calling the interrupt
	* directly using the protected mode pointer will result in garbage
	* being received by the interrupt routine
	*/
	dblock.d.eax = 0x1100;          /* BIOS function -- set font */
	dblock.d.ebx = 0x1000;          /* bh = size of a letter; bl = 0 (reserved) */
	dblock.d.ecx = 0x0100;          /* Number of characters in font (all 256) */
	dblock.x.es = seg;             /* Pointer to font segment */
	dblock.d.ebp = 0x0000;          /* Pointer to font offset */

	__dpmi_int(0x10, &dblock);

	/* We're done with the low memory, free it */
	__dpmi_free_dos_memory(sel);
}

#endif /* USE_286 */

#endif /* ALLOW_GRAPH */

#ifdef USE_DOSSOCK
void *zsock_connect(char *hos, short port)
{
	struct hostent *host;
	struct sockaddr_in sin;
	int *client;

	MAKE(client, int);
	*client = socket(AF_INET, SOCK_STREAM, 0);

	host = gethostbyname(hos);

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = ((struct in_addr *)(host->h_addr))->s_addr;
	sin.sin_port = htons(port);

	if (connect(*client, (struct sockaddr*)&sin, sizeof sin) == -1)
	{
		/* could not connect to server */
		return NULL;
	}

	return client;
}

bool zsock_can_read(void *client)
{
	struct timeval t;
	fd_set rd;
	int *c = client;

	FD_ZERO(&rd);
	FD_SET(*c, &rd);
	t.tv_sec = 0;
	t.tv_usec = 0;
	select(*c + 1, &rd, NULL, NULL, &t);
	if (FD_ISSET(*c, &rd)) return TRUE;
	else return (FALSE);
}

bool zsock_wait(void *client)
{
	struct timeval t;
	fd_set rd;
	int *c = client;

	t.tv_sec = 30;
	t.tv_usec = 0;

	FD_ZERO(&rd);
	FD_SET(*c, &rd);
	select(*c + 1, &rd, NULL, NULL, &t);
	if (FD_ISSET(*c, &rd)) return TRUE;
	else return (FALSE);
}

void zsock_disconnect(void *client)
{
	close(*(int*)client);
	FREE(client, int);
}

void zsock_send(void *sock, char *str)
{
	send(*(int*)sock, str, strlen(str), 0);
}

void zsock_recv(void *sock, char *str, int len)
{
	char c;
	int l = 0;

	while ((l < len) && zsock_can_read(sock))
	{
		if (!recv(*(int*)sock, &c, 1, 0))
		{
			irc_disconnect();
			break;
		}
		if (c == '\r') continue;
		if (c == '\n') break;
		str[l++] = c;
	}
	str[l] = '\0';
}
#endif


/*
 * Initialize the IBM "visual module"
 *
 * Hack -- we assume that "blank space" should be "white space"
 * (and not "black space" which might make more sense).
 *
 * Note the use of "((x << 2) | (x >> 4))" to "expand" a 6 bit value
 * into an 8 bit value, without losing much precision, by using the 2
 * most significant bits as the least significant bits in the new value.
 */
errr init_ibm(void)
{
	int i;
	int mode;

	term *t = &term_screen_body;

	union REGS r;

	/* Check for "Windows" */
	if (getenv("windir"))
	{
		r.h.ah = 0x16;            /* Windows API Call -- Set device focus */
		r.h.al = 0x8B;            /* Causes Dos boxes to become fullscreen */
		r.h.bh = r.h.bl = 0x00;   /* 0x0000 = current Dos box */
		int86(0x2F, &r, &r);        /* Call the Windows API */
	}

	/* Initialize "color_table" */
	for (i = 0; i < 16; i++)
	{
		long rv, gv, bv;

		/* Extract desired values */
		rv = angband_color_table[i][1] >> 2;
		gv = angband_color_table[i][2] >> 2;
		bv = angband_color_table[i][3] >> 2;

		/* Extract the "complex" codes */
		ibm_color_complex[i] = ((rv) | (gv << 8) | (bv << 16));

		/* Save the "simple" codes */
		angband_color_table[i][0] = ibm_color_simple[i];
	}

#ifdef USE_WAT

	/* Set the video mode */
	if (_setvideomode(_VRES16COLOR))
	{
		mode = 0x13;
	}

	/* Wimpy monitor */
	else
	{
		mode = 0x03;
	}

	/* Force 25 line mode */
	_setvideomode(_TEXTC80);
	_settextrows(25);

#else /* USE_WAT */

	/* Set video mode */
	r.h.ah = 0x00;
	r.h.al = 0x13;  /* VGA only mode */
	int86(0x10, &r, &r);

	/* Get video mode */
	r.h.ah = 0x0F;
	int86(0x10, &r, &r);
	mode = r.h.al;

	/* Set video mode */
	r.h.ah = 0x00;
	r.h.al = 0x03;  /* Color text mode */
	int86(0x10, &r, &r);

#endif /* USE_WAT */

	/* Check video mode */
	if (mode == 0x13)
	{
		/* Remember the mode */
		use_color_complex = TRUE;

		/* Instantiate the color set */
		activate_color_complex();
	}

#ifdef USE_GRAPHICS

	/* Try to activate bitmap graphics */
	if (arg_graphics && use_color_complex)
	{
		FILE *f;

		char buf[4096];

		/* Build the filename */
		path_build(buf, 1024, ANGBAND_DIR_XTRA, "angband.fnt");

		/* Open the file */
		f = fopen(buf, "rb");

		/* Okay */
		if (f)
		{
			/* Load the bitmap data */
			if (fread(buf, 1, 4096, f) != 4096)
			{
				quit("Corrupt 'angband.fnt' file");
			}

			/* Close the file */
			fclose(f);

			/* Enable graphics */
			enable_graphic_font(buf);

			/* Enable colors (again) */
			activate_color_complex();

			/* Use graphics */
			use_graphics = TRUE;
		}
	}

#endif

#ifdef USE_CONIO
#else /* USE_CONIO */

	/* Build a "wiper line" */
	for (i = 0; i < 80; i++)
	{
		/* Space */
		wiper[2*i] = ' ';

		/* Black */
		wiper[2*i + 1] = TERM_WHITE;
	}

#endif /* USE_CONIO */


#ifdef USE_VIRTUAL

	/* Make the virtual screen */
	C_MAKE(VirtualScreen, rows * cols * 2, byte);

#endif /* USE_VIRTUAL */

	/* Initialize the DOS mouse driver and Command Dock */
	ibm_mouse_init();


	/* Erase the screen */
	Term_xtra_ibm(TERM_XTRA_CLEAR, 0);


	/* Place the cursor */
	Term_curs_ibm(0, 0);


	/* Access the "default" cursor info */
	r.h.ah = 3;
	r.h.bh = 0;

	/* Make the call */
	int86(0x10, &r, &r);

	/* Extract the standard cursor info */
	saved_cur_v = 1;
	saved_cur_high = r.h.ch;
	saved_cur_low = r.h.cl;


	/* Initialize the term */
	term_init(t, 80, 24, 256);

#ifdef USE_CONIO
#else /* USE_CONIO */

	/* Always use "Term_pict()" */
	t->always_pict = TRUE;

#endif /* USE_CONIO */

	/* Use "white space" to erase */
	t->attr_blank = TERM_WHITE;
	t->char_blank = ' ';

	/* Prepare the init/nuke hooks */
	t->init_hook = Term_init_ibm;
	t->nuke_hook = Term_nuke_ibm;

	/* Connect the hooks */
	t->xtra_hook = Term_xtra_ibm;
	t->curs_hook = Term_curs_ibm;
	t->wipe_hook = Term_wipe_ibm;
	t->text_hook = Term_text_ibm;
	t->pict_hook = Term_pict_ibm;

	/* Save it */
	term_screen = t;

	/* Activate it */
	Term_activate(term_screen);

	/* Success */
	return 0;
}


#endif /* USE_IBM */
