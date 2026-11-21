/*
 * Real-time mode (realtime = 1): turns pass on a clock.
 *
 * The game's turn structure stays: every turn each civilization moves its
 * units, cities produce, the year advances.  What changes is who ends the
 * human player's turn: a clock.  While the game waits for orders (the
 * blinking unit, or "End of Turn") the turn time runs; when it is used up
 * the remaining moves of all your units are dropped and the turn ends by
 * itself.  Units with goto orders keep walking every turn, so with short
 * turns fast units visibly move faster than slow ones.
 *
 * Time only runs while the game waits on the map: dialogs, city screens
 * and menus pause it.
 *
 * Keys on the map: [ or -  slower,  ] or =  faster,  F12  pause,
 * Tab  next unit (the game's "wait").
 *
 * Multiplayer later: the turn clock is the one place that decides when a
 * turn ends (realtime_turn_due); a network session would drive it from a
 * host instead of the local timer.
 */
#include "native.h"
#include "runtime.h"
#include "addons.h"
#include "video.h"
#include "wide.h"
#include "mp.h"

#define UNIT(p, u) (0x90f4 + 0x600 * (p) + 12 * (u))
#define UNIT_TYPE 3
#define UNIT_MOVES 4
#define END_OF_TURN 0x80            /* unit number of the End of Turn wait */
#define KEY_ENTER 0x1C0D
#define KEY_WAIT 0x1177             /* 'w': the game's "wait" (next unit) */
#define KEY_BUILD_CITY 0x3062       /* 'b': Settlers build city */
#define KEY_AUTO_WORK 0x1E61        /* 'a': Settlers auto-improve */
#define KEY_FORTIFY 0x2166          /* 'f': fortify */
#define KEY_SENTRY 0x1F73           /* 's': sentry */
#define KEY_SKIP 0x3920             /* Space: no orders / skip */
#define MARKER_KEY 0xFFFE           /* wakes the order loop (see mapnav.c) */

static const int speeds[] = {12000, 8000, 5000, 3000, 2000, 1500, 1000, 600};
#define NSPEEDS ((int)(sizeof speeds / sizeof speeds[0]))
static int speed = -1;              /* index into speeds */
static int paused;
static uint64_t elapsed_us, last_us;
static int last_turn = -1;
static uint64_t last_push_us;

static int turn_ms(void)
{
    if (speed < 0) {                /* the configured length picks the start */
        speed = 0;
        for (int i = 0; i < NSPEEDS; i++)
            if (speeds[i] >= cfg.turn_ms)
                speed = i;
        if (cfg.turn_ms != speeds[speed])
            return cfg.turn_ms;
    }
    return speeds[speed];
}

int realtime_on(void)
{
    return cfg.realtime || mp_in_game();
}

/* fraction of the turn used, 0..1000, for the clock display */
int realtime_progress(void)
{
    if (mp_in_game())
        return mp_progress();
    uint64_t t = (uint64_t)turn_ms() * 1000;
    return elapsed_us >= t ? 1000 : (int)(elapsed_us * 1000 / t);
}

int realtime_paused(void)
{
    return paused && !mp_in_game();
}

int realtime_turn_secs10(void)
{
    if (mp_in_game())
        return mp_turn_secs10();
    return turn_ms() / 100;
}

static int turn_due(int unit)
{
    if (mp_in_game())
        return mp_turn_due(unit);   /* the host's clock, never paused */
    return !paused && elapsed_us >= (uint64_t)turn_ms() * 1000;
}

static int idle_unit = -1;
static uint64_t idle_seen_us;

/* called while the game waits for orders on the map (hook_map_idle) */
void realtime_idle(void)
{
    idle_unit = (int16_t)RW(SS, (uint16_t)(BP + 8));   /* f_0860_0aa8(player, unit) */
    idle_seen_us = now_us();
    if (!cfg.realtime && !mp_in_game())
        return;
    mp_note_idle();
    uint64_t now = now_us();
    int turn = (int16_t)GW(0x90f2);
    if (turn != last_turn) {
        last_turn = turn;
        elapsed_us = 0;
    } else if (!paused && now - last_us < 2000000) {
        /* only time spent waiting here counts (the blink loop comes by
           every 0.3-0.8 s): a longer gap means a dialog, a city screen
           or the other civilizations' moves */
        elapsed_us += now - last_us;
    }
    last_us = now;
    int unit = (int16_t)RW(SS, (uint16_t)(BP + 8));   /* f_0860_0aa8(player, unit) */
    if (!turn_due(unit))
        return;
    uint16_t pending;
    if (bios_key_peek(&pending) && now - last_push_us < 1000000)
        return;                     /* the last push is not read yet */
    last_push_us = now;
    int player = (int16_t)GW(0xe710);
    if (unit == END_OF_TURN) {
        while (bios_key_peek(&pending))
            bios_key_wait();        /* keys typed into "End of Turn" are void */
        bios_key_push(KEY_ENTER);
        return;
    }
    /* time is up: no unit has moves left, the order loop runs out */
    for (int u = 0; u < 128; u++)
        if (GB(UNIT(player, u) + UNIT_TYPE) != 0xff)
            WB(DS, (uint16_t)(UNIT(player, u) + UNIT_MOVES), 0);
    bios_key_push(MARKER_KEY);
}

/* hook before 0402:3e31: the human player's units are all done.  Always
   wait at "End of Turn" (for the player in turn-based mode, or for the
   clock in real-time mode) so turns never auto-end unexpectedly. */
void hook_realtime_end_of_turn(void)
{
    mp_units_done();
    WW(SS, (uint16_t)(BP - 6), 0);
}

/* ---- selecting a unit by clicking it ----
   The order loop of f_0402_000a works through the units by index
   ([BP-0x32]); clicking one of your units with moves left makes the loop
   continue with that unit.  Its standing orders (sentry, fortify,
   settler work, goto) are cancelled, as it now waits for new ones. */
static int order_unit = -1, order_goto_mode, select_unit = -1;

/* hook_unit_await (0402:0645), BP = order loop frame */
void realtime_order_wait(void)
{
    order_unit = (int16_t)RW(SS, (uint16_t)(BP - 0x32));
    order_goto_mode = RW(SS, (uint16_t)(BP - 0x36)) != 0;
    idle_unit = order_unit;
    idle_seen_us = now_us();
}

static int resolve_shortcuts(const char **lbl1, uint8_t *col1, uint16_t *key1,
                             const char **lbl2, uint8_t *col2, uint16_t *key2)
{
    if (order_goto_mode || now_us() - idle_seen_us > 1200000ULL)
        return 0;
    int au = idle_unit;
    if (au == END_OF_TURN) {
        if (lbl1) *lbl1 = "NEXT TURN";
        if (col1) *col1 = 10;
        if (key1) *key1 = KEY_ENTER;
        return 1;
    }
    if (au < 0 || au >= 128)
        return 0;
    int player = (int16_t)GW(0xe710);
    if (player < 0 || player >= 8)
        return 0;
    uint16_t r = UNIT(player, au);
    uint8_t t = GB(r + UNIT_TYPE);
    if (t == 0xff || GB(r + UNIT_MOVES) == 0)
        return 0;
    if (t == 0) {                   /* Settlers */
        if (lbl1) *lbl1 = "CITY";
        if (col1) *col1 = 10;
        if (key1) *key1 = KEY_BUILD_CITY;
        if (lbl2) *lbl2 = "AUTO";
        if (col2) *col2 = 9;
        if (key2) *key2 = KEY_AUTO_WORK;
        return 2;
    }
    if (t <= 13) {                  /* Land military */
        if (lbl1) *lbl1 = "FORT";
        if (col1) *col1 = 9;
        if (key1) *key1 = KEY_FORTIFY;
        if (lbl2) *lbl2 = "WAIT";
        if (col2) *col2 = 9;
        if (key2) *key2 = KEY_WAIT;
        return 2;
    }
    if (t <= 25) {                  /* Air / Naval / Nuclear */
        if (lbl1) *lbl1 = "SENTRY";
        if (col1) *col1 = 9;
        if (key1) *key1 = KEY_SENTRY;
        if (lbl2) *lbl2 = "WAIT";
        if (col2) *col2 = 9;
        if (key2) *key2 = KEY_WAIT;
        return 2;
    }
    /* Diplomat / Caravan */
    if (lbl1) *lbl1 = "WAIT";
    if (col1) *col1 = 9;
    if (key1) *key1 = KEY_WAIT;
    if (lbl2) *lbl2 = "SKIP";
    if (col2) *col2 = 9;
    if (key2) *key2 = KEY_SKIP;
    return 2;
}

int unit_shortcuts_get(const char **lbl1, uint8_t *col1,
                       const char **lbl2, uint8_t *col2)
{
    return resolve_shortcuts(lbl1, col1, NULL, lbl2, col2, NULL);
}

int realtime_active_unit(void)
{
    return order_unit;
}

static void realtime_cycle(void)
{
    if (mp_in_game())
        return;
    if (!cfg.realtime) {
        cfg.realtime = 1;
        speed = 3;                  /* 3000 ms (1x) */
        paused = 0;
        elapsed_us = 0;
        last_us = now_us();
        ovl_msg("Real-Time: 1x (3.0s)");
    } else if (turn_ms() > 1500) {
        speed = 5;                  /* 1500 ms (2x) */
        paused = 0;
        elapsed_us = 0;
        last_us = now_us();
        ovl_msg("Real-Time: 2x (1.5s)");
    } else {
        cfg.realtime = 0;
        paused = 0;
        elapsed_us = 0;
        ovl_msg("Turn-Based Mode");
    }
}

/* Called from 0402:0c23 when left-clicking a map tile containing our own
   unit(s).  Returns unit index >= 0 to immediately wake & select it, or -2
   if handled without switching (already active or all units have 0 moves). */
int map_click_select_unit(void)
{
    int player = (int16_t)RW(SS, (uint16_t)(BP + 6));
    int cur = (int16_t)RW(SS, (uint16_t)(BP - 0x32));
    int sx = (int16_t)RW(SS, (uint16_t)(BP - 0x1c));
    int sy = (int16_t)RW(SS, (uint16_t)(BP - 0x26));
    int best = -1, cur_here = 0, woke_zero = 0;

    /* Scan starting after cur so clicking a stack cycles through units */
    int start = (cur >= 0 && cur < 128) ? (cur + 1) & 127 : 0;
    for (int i = 0; i < 128; i++) {
        int u = (start + i) & 127;
        uint16_t r = UNIT(player, u);
        if (GB(r + UNIT_TYPE) == 0xff || GB(r + 1) != sx || GB(r + 2) != sy)
            continue;
        if (u == cur) {
            cur_here = 1;
            continue;
        }
        if (GB(r + UNIT_MOVES) > 0) {
            best = u;
            break;
        }
    }
    if (best >= 0) {
        uint16_t r = UNIT(player, best);
        WB(DS, r, GB(r) & ~0xcf);        /* clear sentry, fortify, settler work */
        WB(DS, (uint16_t)(r + 6), 0xff); /* clear goto */
        autowork_cancel(best);
        return best;
    }
    if (cur_here)
        return -2;
    /* All friendly units on this tile have 0 moves left: wake standing orders */
    int cancelled_auto = 0;
    for (int u = 0; u < 128; u++) {
        uint16_t r = UNIT(player, u);
        if (GB(r + UNIT_TYPE) == 0xff || GB(r + 1) != sx || GB(r + 2) != sy)
            continue;
        if (autowork_cancel(u))
            cancelled_auto = 1;
        if ((GB(r) & 0xcf) || GB(r + 6) != 0xff) {
            WB(DS, r, GB(r) & ~0xcf);
            WB(DS, (uint16_t)(r + 6), 0xff);
            woke_zero = 1;
        }
    }
    if (!cancelled_auto)
        ovl_msg(woke_zero ? "Unit awakened (0 moves left)" : "No moves left this turn");
    return -2;
}

int realtime_click(int x, int y)
{
    if (!video_is_map())
        return 0;
    if (!mp_in_game() &&
        x >= RT_BTN_X && x < RT_BTN_X + RT_BTN_W &&
        y >= RT_BTN_Y(g_scr_h) && y < RT_BTN_Y(g_scr_h) + RT_BTN_H) {
        realtime_cycle();
        return 1;
    }
    int sy = SC_BTN_Y(g_scr_h);
    if (y >= sy && y < sy + SC_BTN_H) {
        uint16_t k1 = 0, k2 = 0;
        int n = resolve_shortcuts(NULL, NULL, &k1, NULL, NULL, &k2);
        if (n == 1 && x >= SC_FULL_X && x < SC_FULL_X + SC_FULL_W) {
            idle_seen_us = 0;
            bios_key_push(k1);
            return 1;
        }
        if (n == 2) {
            if (x >= SC_BTN1_X && x < SC_BTN1_X + SC_BTN1_W) {
                idle_seen_us = 0;
                bios_key_push(k1);
                return 1;
            }
            if (x >= SC_BTN2_X && x < SC_BTN2_X + SC_BTN2_W) {
                idle_seen_us = 0;
                bios_key_push(k2);
                return 1;
            }
        }
    }
    return 0;
}

/* hook_unit_order_key with the marker key, BP = order loop frame */
void realtime_apply_select(void)
{
    int u = select_unit;
    select_unit = -1;
    if (u < 0)
        return;
    uint16_t r = UNIT((int16_t)GW(0xe710), u);
    WB(DS, r, GB(r) & ~0xcf);       /* sentry, fortify, settler work */
    WB(DS, (uint16_t)(r + 6), 0xff); /* goto */
    WW(SS, (uint16_t)(BP - 0x32), (uint16_t)u);
}

int realtime_key(uint16_t key)
{
    if (!realtime_on() || !video_is_map())
        return 0;
    uint8_t ascii = (uint8_t)key, scan = (uint8_t)(key >> 8);
    if (mp_in_game() && scan != 0x0F)
        return 0;                   /* the host sets the pace */
    if (ascii == '[' || ascii == '-' || ascii == ']' || ascii == '=') {
        turn_ms();
        int faster = ascii == ']' || ascii == '=';
        speed += faster ? 1 : -1;
        if (speed < 0)
            speed = 0;
        if (speed >= NSPEEDS)
            speed = NSPEEDS - 1;
        ovl_msg("Turn: %d.%d seconds", speeds[speed] / 1000, speeds[speed] % 1000 / 100);
        return 1;
    }
    if (scan == 0x58 && ascii == 0) {   /* F12 */
        paused = !paused;
        ovl_msg(paused ? "Paused (F12)" : "Running");
        return 1;
    }
    if (scan == 0x0F) {                 /* Tab: next unit */
        bios_key_push(KEY_WAIT);
        return 1;
    }
    return 0;
}

/* ---- quick announcements (real time, multiplayer) ----
   f_0ad5_1569: "Our wise men discover the secret of ..." is a picture or a
   dialog that waits for a key; in real time it is a short message instead. */

/* 0ad5:16d6: [BP-0x42] = 1 shows the picture */
static int discovery_box;            /* the next message box is the discovery */

void hook_fast_discovery(void)
{
    if (realtime_on()) {
        WW(SS, (uint16_t)(BP - 0x42), 0);
        discovery_box = 1;
    }
}

/* 0ad5:1996: the announcement is over (the advance is given next) */
void hook_fast_discovery_end(void)
{
    discovery_box = 0;
}

/* ovl21_0000(-1): the game's message box with the text in DS:C926 (used
   for many messages; only the discovery is shortened) */
void ovl21_0000(void)
{
    if ((int16_t)FARG(0) != -1 || !discovery_box) {
        lifted_ovl21_0000();
        return;
    }
    char t[96];
    int n = 0;
    for (int i = 0; n < 95 && GB((uint16_t)(0xc926 + i)); i++) {
        char c = (char)GB((uint16_t)(0xc926 + i));
        if (c == '\n')
            c = ' ';
        if (c != ' ' || (n && t[n - 1] != ' '))
            t[n++] = c;
    }
    t[n] = 0;
    ovl_msg("%s", t);
    RET_FAR(0);
}

/* ovl12_09d6: the historians' report of the greatest civilizations */
void ovl12_09d6(void)
{
    if (!cfg.rankings || mp_in_game()) {
        RET_FAR(0);
        return;
    }
    lifted_ovl12_09d6();
}
