/* Features added to the game (src/native). */
#pragma once
#include <stdint.h>

/* city screen "GOV" and "GOLD" buttons (33x9 at y=190, matching native EXIT/CHANGE) */
#define GOV_BTN_X 214
#define GOV_BTN_Y 190
#define GOV_BTN_W 33
#define GOV_BTN_H 9

#define GOLD_BTN_X 249
#define GOLD_BTN_Y 190
#define GOLD_BTN_W 33
#define GOLD_BTN_H 9

/* map screen real-time mode toggle button (bottom of left status panel) */
#define RT_BTN_X 2
#define RT_BTN_W 76
#define RT_BTN_H 10
#define RT_BTN_Y(h) ((h) - 11)

/* map screen context-sensitive unit shortcut bar (above XP badge & mode toggle) */
#define SC_BTN_Y(h) ((h) - 34)
#define SC_BTN_H 9
#define SC_BTN1_X 2
#define SC_BTN1_W 37
#define SC_BTN2_X 41
#define SC_BTN2_W 37
#define SC_FULL_X 2
#define SC_FULL_W 76

int city_screen_city(void);          /* city shown on the city screen or -1 */
int city_menu_open(void);            /* 1 while "What shall we build?" is open */
int city_produces_gold(int city);
int city_has_governor(int city);
void city_set_gold(int city, int on);
void ovl_msg(const char *fmt, ...);
int native_overlay_active(void);
void native_overlay_draw(uint8_t *buf, int W, int H, int ox, int oy);

/* input filters: return 1 if the event was used by a feature */
int native_key_filter(uint16_t key);           /* BIOS key word scan<<8|ascii */
int native_mouse_filter(int button, int x, int y);

/* real-time mode (realtime.c) */
int realtime_on(void);
void realtime_idle(void);
int realtime_key(uint16_t key);
int realtime_progress(void);         /* 0..1000 of the turn used */
int realtime_paused(void);
int realtime_turn_secs10(void);      /* turn length in 1/10 s */
void realtime_order_wait(void);
int realtime_active_unit(void);
int realtime_click(int x, int y);
void realtime_apply_select(void);
int unit_shortcuts_get(const char **lbl1, uint8_t *col1,
                       const char **lbl2, uint8_t *col2);

/* map navigation (mapnav.c) */
int mapnav_button(int b, int down, int x, int y);
void mapnav_motion(int x, int y);
void mapnav_wheel(int steps);
int mapnav_is_marker(uint16_t key);
void mouse_deliver_button(int b, int down);
void mouse_deliver_click(int b);     /* press now, release a moment later */
void mouse_screen_resized(void);

void native_state_dump(void);
void native_state_dump_to(const char *path);

/* save games: extra state stored in CIVILn.NAT next to CIVILn.SVE */
/* f is a FILE * (kept opaque so this header needs no system headers) */
void native_save_state(void *f);
void native_load_state(void *f);
int autowork_cancel(int unit);
void autowork_save(void *f);
void autowork_load(void *f);

/* unit XP & Hero upgrades (unitxp.c) */
int unit_xp_atk(int p, int u);
int unit_xp_def(int p, int u);
int unit_xp_mov(int p, int u);
void unit_xp_clear(int p, int u);
int unit_xp_consume_life(int p, int u);
void unit_xp_on_win(int p, int u);
int unit_xp_badge(int p, int u, char *buf, int bufsz, int *is_hero);
void unit_xp_save(void *f);
void unit_xp_load(void *f);
