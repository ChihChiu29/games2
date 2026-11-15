#include "render.h"
#include "term_ascii.h"
#include <stdio.h>
#include <string.h>

void render_local_to_world(const Character *c, int fwd, int right, int *wx, int *wy)
{
    int dx = 0, dy = 0;
    switch (c->dir) {
    case DIR_UP:
        dx = right;
        dy = -fwd;
        break;
    case DIR_DOWN:
        dx = -right;
        dy = fwd;
        break;
    case DIR_RIGHT:
        dx = fwd;
        dy = right;
        break;
    case DIR_LEFT:
        dx = -fwd;
        dy = -right;
        break;
    }
    *wx = c->cx + dx;
    *wy = c->cy + dy;
}

/* Map 320x200 world pixel coordinates into 80x24 Linux terminal ASCII grid */
static int px_to_tx(int px)
{
    int tx = (px * (TERM_W - 2)) / 320 + 1;
    if (tx < 1) tx = 1;
    if (tx > TERM_W - 2) tx = TERM_W - 2;
    return tx;
}

static int py_to_ty(int py)
{
    int ty = 3 + ((py - ARENA_TOP) * (TERM_H - 5)) / (ARENA_BOTTOM - ARENA_TOP);
    if (ty < 4) ty = 4;
    if (ty > TERM_H - 2) ty = TERM_H - 2;
    return ty;
}

static char dir_glyph(Direction d)
{
    switch (d) {
    case DIR_UP:    return '^';
    case DIR_RIGHT: return '>';
    case DIR_DOWN:  return 'v';
    case DIR_LEFT:  return '<';
    }
    return '@';
}

static void render_character_ascii(const Character *c)
{
    if (!c || !c->active || !c->def) return;

    int tx = px_to_tx(c->cx);
    int ty = py_to_ty(c->cy);
    int fdx = dir_dx[c->dir];
    int fdy = dir_dy[c->dir];

    /* 1. Active Attack Trail / Hitbox */
    const AttackDef *atk = character_get_active_attack(c);
    if (atk) {
        int active_start = atk->windup_frames + 1;
        int active_end   = atk->windup_frames + atk->active_frames;
        if (c->action_frame >= active_start && c->action_frame <= active_end) {
            uint8_t col = (c->action == ACT_CRIT_ATTACK) ? COL_YELLOW : COL_LIGHTRED;
            char wch = (atk->anim_type == ANIM_SLASH) ? '*' : ((fdx != 0) ? '-' : '|');
            int reach_cells = (c->def->scale_pct > 110) ? 5 : 4;
            int s;
            for (s = 2; s <= reach_cells; s++) {
                term_put_cell(tx + fdx * s, ty + fdy * s, wch, col, COL_RED);
            }
        } else if (c->action_frame <= atk->windup_frames) {
            term_put_cell(tx, ty - 1, '!', COL_YELLOW, COL_BROWN);
        }
    }

    /* 2. Shield Indicator */
    if (c->action == ACT_BLOCK) {
        int parry_win = (c->left_weapon && c->left_weapon->parry_window_frames > 0)
            ? c->left_weapon->parry_window_frames : 11;
        bool is_parry = (c->block_hold_frames <= parry_win);
        char sch = (fdx != 0) ? '|' : '=';
        term_put_cell(tx + fdx * 2, ty + fdy * 2, sch,
                      is_parry ? COL_YELLOW : COL_LIGHTCYAN,
                      is_parry ? COL_CYAN : COL_BLUE);
    }

    /* 3. Character Core & Facing Indicator */
    char body_ch = '@';
    uint8_t fg = COL_WHITE, bg = COL_BLUE;
    if (!c->is_player) {
        body_ch = (c->def->body_style == BSTYLE_SKELETON_LORD) ? 'L' : 'S';
        fg = (c->def->body_style == BSTYLE_SKELETON_LORD) ? COL_YELLOW : COL_LIGHTGRAY;
        bg = COL_BLACK;
    }
    if (c->action == ACT_CRIT_HURT) {
        bg = COL_RED;
        fg = COL_YELLOW;
        term_put_cell(tx - 1, ty - 1, '*', COL_YELLOW, COL_RED);
        term_put_cell(tx + 1, ty + 1, '*', COL_YELLOW, COL_RED);
    } else if (c->action == ACT_STAGGER) {
        bg = COL_BLUE;
        fg = COL_YELLOW;
    } else if (c->hurt_flash > 0) {
        bg = COL_RED;
    } else if (c->is_player && c->action == ACT_ROLL) {
        static const char spin_ch[4] = { '/', '-', '\\', '|' };
        body_ch = spin_ch[(c->action_frame / 2) & 3];
        fg = (c->action_frame <= player_def.roll_iframes) ? COL_LIGHTGREEN : COL_LIGHTCYAN;
        bg = COL_BLUE;
        term_put_cell(tx - c->roll_dx, ty - c->roll_dy, '.', COL_DARKGRAY, COL_BLACK);
    }

    term_put_cell(tx, ty, body_ch, fg, bg);
    term_put_cell(tx + fdx, ty + fdy, dir_glyph(c->dir), COL_YELLOW, bg);
}

void render_scene(const char *status_msg, uint8_t status_col)
{
    int x, y, i;
    char buf[160];

    term_clear(COL_DARKGRAY, COL_BLACK);

    /* Arena Border (Rows 3..23) */
    for (y = 3; y < TERM_H; y++) {
        for (x = 0; x < TERM_W; x++) {
            if (y == 3 || y == TERM_H - 1 || x == 0 || x == TERM_W - 1) {
                term_put_cell(x, y, '#', COL_LIGHTGRAY, COL_DARKGRAY);
            } else if (((x + y * 3) % 11 == 0) && ((x * 5 + y) % 7 == 0)) {
                term_put_cell(x, y, '.', COL_DARKGRAY, COL_BLACK);
            }
        }
    }

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) render_character_ascii(&enemies[i]);
    }
    if (player.active) {
        render_character_ascii(&player);
    }

    /* Clean 3-row ASCII HUD (Rows 0..2) */
    const char *wep = player.right_weapon ? player.right_weapon->name : "None";
    snprintf(buf, sizeof(buf), "HP:%3d/%3d  STM:%3d/%3d  WEP:%-10s [TAB]",
             player.hp, player.max_hp, player.stamina, player.max_stamina, wep);
    term_put_str(1, 0, buf, COL_LIGHTGREEN, COL_BLACK);

    const char *e0 = enemies[0].active ? enemies[0].def->name : "SLAIN";
    const char *e1 = enemies[1].active ? enemies[1].def->name : "SLAIN";
    snprintf(buf, sizeof(buf), "%s:%d/%d   %s:%d/%d   [R]Reload [Q]Quit",
             e0, enemies[0].hp, enemies[0].max_hp,
             e1, enemies[1].hp, enemies[1].max_hp);
    term_put_str(1, 1, buf, COL_LIGHTGRAY, COL_BLACK);

    term_put_str(1, 2, status_msg, status_col, COL_BLACK);
    term_present();
}
