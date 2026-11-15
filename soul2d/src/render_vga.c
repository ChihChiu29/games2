#include "render.h"
#include "vga.h"
#include <stdio.h>
#include <stdlib.h>
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

/* =========================================================================
 * Offscreen 48x48 Sprite Canvas with Automatic 1px Silhouette Outliner
 * ========================================================================= */

#define CANV_SZ 48
#define CANV_C  24

static uint8_t canv[CANV_SZ][CANV_SZ];
static uint8_t canv_out[CANV_SZ][CANV_SZ];

static void canv_clear(void)
{
    memset(canv, 0, sizeof(canv));
}

static void canv_put(int x, int y, uint8_t col)
{
    int cx = CANV_C + x;
    int cy = CANV_C + y;
    if ((unsigned)cx < CANV_SZ && (unsigned)cy < CANV_SZ) {
        canv[cy][cx] = col;
    }
}

static void canv_rect(int x, int y, int w, int h, uint8_t col)
{
    int py, px;
    for (py = y; py < y + h; py++) {
        for (px = x; px < x + w; px++) {
            canv_put(px, py, col);
        }
    }
}

static void canv_shaded_box(int x, int y, int w, int h, uint8_t dark, uint8_t mid, uint8_t light)
{
    int py, px;
    for (py = 0; py < h; py++) {
        for (px = 0; px < w; px++) {
            uint8_t c = mid;
            if (py == 0 || px == 0) c = light;
            else if (py == h - 1 || px == w - 1) c = dark;
            canv_put(x + px, y + py, c);
        }
    }
}

static void canv_line(int x0, int y0, int x1, int y1, uint8_t col)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        canv_put(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void canv_blit_outlined(int world_cx, int world_cy, int scale_pct, uint8_t tint_col)
{
    int y, x;
    memcpy(canv_out, canv, sizeof(canv));

    for (y = 1; y < CANV_SZ - 1; y++) {
        for (x = 1; x < CANV_SZ - 1; x++) {
            if (canv[y][x] == 0) {
                if (canv[y - 1][x] != 0 || canv[y + 1][x] != 0 ||
                    canv[y][x - 1] != 0 || canv[y][x + 1] != 0) {
                    canv_out[y][x] = PAL_OUTLINE;
                }
            }
        }
    }

    if (scale_pct <= 0) scale_pct = 100;
    int dest_half = (CANV_C * scale_pct) / 100;

    for (y = -dest_half; y < dest_half; y++) {
        int sy = CANV_C + (y * 100) / scale_pct;
        if ((unsigned)sy >= CANV_SZ) continue;
        for (x = -dest_half; x < dest_half; x++) {
            int sx = CANV_C + (x * 100) / scale_pct;
            if ((unsigned)sx >= CANV_SZ) continue;
            uint8_t col = canv_out[sy][sx];
            if (col != 0) {
                if (tint_col != 0 && col != PAL_OUTLINE) {
                    col = tint_col;
                }
                vga_put_pixel(world_cx + x, world_cy + y, col);
            }
        }
    }
}

static void draw_floor_shadow(int cx, int cy, int rx, int ry)
{
    int y, x;
    for (y = -ry; y <= ry; y++) {
        for (x = -rx; x <= rx; x++) {
            if ((x * x * 100) / (rx * rx) + (y * y * 100) / (ry * ry) <= 100) {
                if (((cx + x + cy + y) & 1) == 0) {
                    vga_put_pixel(cx + x, cy + y, PAL_SHADOW);
                }
            }
        }
    }
}

/* =========================================================================
 * Procedural Articulated Character Rig (Knight, Skeleton, Skeleton Lord)
 * ========================================================================= */

static void generate_knight_rig(const Character *c, int stride, int bob)
{
    Direction dir = c->dir;

    if (dir == DIR_DOWN) {
        canv_shaded_box(-5, -2 + bob, 10, 9, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
    } else if (dir == DIR_RIGHT) {
        canv_shaded_box(-6, -2 + bob, 5, 9, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
    } else if (dir == DIR_LEFT) {
        canv_shaded_box(1, -2 + bob, 5, 9, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
    }

    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_shaded_box(-4, 4 + stride, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_shaded_box( 1, 4 - stride, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
    } else {
        canv_shaded_box(-2 + stride, 4, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_shaded_box(-1 - stride, 4, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
    }

    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_shaded_box(-4, -3 + bob, 8, 7, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_rect(-3, 1 + bob, 6, 3, PAL_BLUE_MID);
        canv_rect(-3, 1 + bob, 6, 1, PAL_GOLD_MID);
        canv_shaded_box(-7, -3 + bob, 3, 4, PAL_STEEL_DARK, PAL_STEEL_LIGHT, PAL_STEEL_SHINE);
        canv_shaded_box( 4, -3 + bob, 3, 4, PAL_STEEL_DARK, PAL_STEEL_MID,   PAL_STEEL_LIGHT);
        canv_put(-6, -3 + bob, PAL_GOLD_LIGHT);
        canv_put( 5, -3 + bob, PAL_GOLD_LIGHT);
        if (dir == DIR_UP) {
            canv_shaded_box(-4, -2 + bob, 8, 8, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
            canv_rect(-3, -2 + bob, 6, 1, PAL_GOLD_MID);
        }
    } else {
        canv_shaded_box(-3, -3 + bob, 6, 7, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_rect(-3, 1 + bob, 6, 2, PAL_BLUE_MID);
        canv_shaded_box(-2, -2 + bob, 4, 4, PAL_STEEL_MID, PAL_STEEL_LIGHT, PAL_STEEL_SHINE);
        canv_put(0, -2 + bob, PAL_GOLD_LIGHT);
    }

    int hy = -9 + bob;
    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_rect(-1, hy - 3, 3, 3, PAL_CRIMSON_LIGHT);
        canv_put(0, hy - 4, PAL_CRIMSON_HOT);
    } else if (dir == DIR_RIGHT) {
        canv_rect(-4, hy - 2, 4, 2, PAL_CRIMSON_LIGHT);
        canv_rect(-5, hy - 1, 2, 3, PAL_CRIMSON_MID);
    } else {
        canv_rect(1, hy - 2, 4, 2, PAL_CRIMSON_LIGHT);
        canv_rect(4, hy - 1, 2, 3, PAL_CRIMSON_MID);
    }

    canv_shaded_box(-3, hy, 7, 6, PAL_STEEL_DARK, PAL_STEEL_LIGHT, PAL_STEEL_SHINE);
    if (dir == DIR_DOWN) {
        canv_rect(-2, hy + 2, 5, 1, PAL_GOLD_LIGHT);
        canv_rect(-2, hy + 3, 5, 1, PAL_OUTLINE);
        canv_rect( 0, hy + 3, 1, 3, PAL_OUTLINE);
    } else if (dir == DIR_RIGHT) {
        canv_rect( 1, hy + 2, 3, 1, PAL_GOLD_LIGHT);
        canv_rect( 1, hy + 3, 3, 1, PAL_OUTLINE);
        canv_rect( 2, hy + 3, 1, 2, PAL_OUTLINE);
    } else if (dir == DIR_LEFT) {
        canv_rect(-3, hy + 2, 3, 1, PAL_GOLD_LIGHT);
        canv_rect(-3, hy + 3, 3, 1, PAL_OUTLINE);
        canv_rect(-2, hy + 3, 1, 2, PAL_OUTLINE);
    } else {
        canv_rect(-2, hy + 4, 5, 1, PAL_GOLD_MID);
    }
}

static void generate_skeleton_rig(const Character *c, int stride, int bob, bool is_lord)
{
    Direction dir = c->dir;
    int recoil = (c->action == ACT_STAGGER || c->action == ACT_CRIT_HURT) ? 2 : 0;

    if (is_lord) {
        canv_shaded_box(-5, -3 + bob, 11, 8, PAL_CRIMSON_DARK, PAL_CRIMSON_MID, PAL_CRIMSON_LIGHT);
    }

    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_line(-3, 4, -3, 8 + stride, PAL_BONE_MID);
        canv_put(-3, 6 + stride / 2, PAL_BONE_SHINE);
        canv_line( 3, 4,  3, 8 - stride, PAL_BONE_MID);
        canv_put( 3, 6 - stride / 2, PAL_BONE_SHINE);
    } else {
        canv_line(-1 + stride, 4, -2 + stride, 8, PAL_BONE_MID);
        canv_line( 1 - stride, 4,  2 - stride, 8, PAL_BONE_LIGHT);
    }

    canv_rect(-3, 3 + bob, 7, 2, PAL_BONE_MID);
    canv_line(0, -3 + bob, 0, 3 + bob, PAL_BONE_DARK);

    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_rect(-4, -3 + bob, 9, 1, PAL_BONE_SHINE);
        canv_rect(-4, -1 + bob, 9, 1, PAL_BONE_LIGHT);
        canv_rect(-3,  1 + bob, 7, 1, PAL_BONE_MID);
        if (dir == DIR_DOWN) {
            canv_put(0, -2 + bob, PAL_BONE_MID);
            canv_put(0,  0 + bob, PAL_BONE_MID);
        } else {
            canv_line(0, -4 + bob, 0, 3 + bob, PAL_BONE_SHINE);
        }
        canv_line(-5, -3 + bob, -6 - recoil, 2 + bob - recoil, PAL_BONE_LIGHT);
        canv_line( 5, -3 + bob,  6 + recoil, 2 + bob - recoil, PAL_BONE_LIGHT);
    } else {
        canv_rect(-2, -3 + bob, 5, 1, PAL_BONE_SHINE);
        canv_rect(-2, -1 + bob, 5, 1, PAL_BONE_LIGHT);
        canv_rect(-2,  1 + bob, 4, 1, PAL_BONE_MID);
        canv_line(0, -2 + bob, (dir == DIR_RIGHT ? 3 : -3), 2 + bob, PAL_BONE_SHINE);
    }

    if (is_lord) {
        canv_shaded_box(-7, -4 + bob, 3, 3, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_GOLD_LIGHT);
        canv_shaded_box( 5, -4 + bob, 3, 3, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_GOLD_LIGHT);
    }

    int hy = -9 + bob - recoil;
    canv_shaded_box(-3, hy, 7, 5, PAL_BONE_MID, PAL_BONE_LIGHT, PAL_BONE_SHINE);
    canv_rect(-2, hy + 5, 5, 2, PAL_BONE_MID);

    if (dir == DIR_DOWN) {
        canv_rect(-2, hy + 2, 2, 2, PAL_OUTLINE);
        canv_rect( 1, hy + 2, 2, 2, PAL_OUTLINE);
        canv_put(-2, hy + 2, PAL_CRIMSON_HOT);
        canv_put( 1, hy + 2, PAL_CRIMSON_HOT);
        canv_put(0, hy + 4, PAL_OUTLINE);
        canv_put(-1, hy + 6, PAL_OUTLINE);
        canv_put( 1, hy + 6, PAL_OUTLINE);
    } else if (dir == DIR_RIGHT) {
        canv_rect(1, hy + 2, 2, 2, PAL_OUTLINE);
        canv_put(2, hy + 2, PAL_CRIMSON_HOT);
        canv_put(1, hy + 6, PAL_OUTLINE);
    } else if (dir == DIR_LEFT) {
        canv_rect(-2, hy + 2, 2, 2, PAL_OUTLINE);
        canv_put(-2, hy + 2, PAL_CRIMSON_HOT);
        canv_put(-1, hy + 6, PAL_OUTLINE);
    }

    if (is_lord) {
        canv_rect(-3, hy - 1, 7, 1, PAL_GOLD_MID);
        canv_put(-3, hy - 2, PAL_GOLD_SHINE);
        canv_put( 0, hy - 3, PAL_GOLD_SHINE);
        canv_put( 3, hy - 2, PAL_GOLD_SHINE);
    }
}

/* =========================================================================
 * Pixel-Art Kite Shield & Curved Slash/Thrust Trails (No Debug Rectangles!)
 * ========================================================================= */

static void draw_outlined_world_line(int x0, int y0, int x1, int y1, uint8_t col)
{
    vga_draw_line(x0 - 1, y0, x1 - 1, y1, PAL_OUTLINE);
    vga_draw_line(x0 + 1, y0, x1 + 1, y1, PAL_OUTLINE);
    vga_draw_line(x0, y0 - 1, x1, y1 - 1, PAL_OUTLINE);
    vga_draw_line(x0, y0 + 1, x1, y1 + 1, PAL_OUTLINE);
    vga_draw_line(x0, y0, x1, y1, col);
}

/* Draws a proper pixel-art Kite/Heater Shield on the 48x48 outlined canvas */
static void draw_procedural_shield(const Character *c)
{
    if (!c->left_weapon || c->left_weapon->kind != WEAPON_SHIELD) return;

    bool blocking = (c->action == ACT_BLOCK);
    int parry_win = (c->left_weapon->parry_window_frames > 0) ? c->left_weapon->parry_window_frames : 11;
    bool is_parry = blocking && (c->block_hold_frames <= parry_win);

    int sx, sy;
    if (blocking) {
        render_local_to_world(c, 8, 0, &sx, &sy);
    } else {
        render_local_to_world(c, 1, -7, &sx, &sy);
    }

    canv_clear();
    uint8_t rim  = is_parry ? PAL_GOLD_SHINE : PAL_STEEL_LIGHT;
    uint8_t face = is_parry ? PAL_GOLD_LIGHT : PAL_BLUE_MID;
    uint8_t boss = is_parry ? COL_WHITE      : PAL_GOLD_LIGHT;

    if (blocking) {
        /* Large raised Kite Shield (11px tall, tapered point at bottom) */
        canv_rect(-4, -5, 9, 6, rim);
        canv_rect(-3, -4, 7, 5, face);
        canv_rect(-3,  1, 7, 2, rim);
        canv_rect(-2,  1, 5, 2, face);
        canv_rect(-1,  3, 3, 2, rim);
        canv_put(  0,  3, face);
        canv_put(  0,  5, rim);
        /* Gold Cross Emblem */
        canv_line(0, -3, 0, 2, boss);
        canv_line(-2, -1, 2, -1, boss);
    } else {
        /* Compact idle Heater Shield on left arm */
        canv_rect(-3, -3, 6, 4, rim);
        canv_rect(-2, -2, 4, 3, face);
        canv_rect(-2,  1, 4, 1, rim);
        canv_rect(-1,  2, 2, 1, rim);
        canv_put(  0, -1, boss);
    }

    canv_blit_outlined(sx, sy, 100, 0);
}

static void draw_procedural_melee_weapon(const Character *c, const WeaponDef *w,
                                         int hand_fwd, int hand_right, int tip_fwd, int tip_right,
                                         bool windup_glint)
{
    int hx, hy, tx, ty;
    render_local_to_world(c, hand_fwd, hand_right, &hx, &hy);
    render_local_to_world(c, tip_fwd, tip_right, &tx, &ty);

    if (w->style == WSTYLE_SPEAR) {
        int mx = hx + ((tx - hx) * 7) / 10;
        int my = hy + ((ty - hy) * 7) / 10;
        draw_outlined_world_line(hx, hy, mx, my, PAL_WOOD_LIGHT);
        draw_outlined_world_line(mx, my, tx, ty, windup_glint ? PAL_GOLD_SHINE : PAL_STEEL_SHINE);
    } else {
        int gx = hx + ((tx - hx) * 2) / 10;
        int gy = hy + ((ty - hy) * 2) / 10;
        draw_outlined_world_line(hx, hy, gx, gy, PAL_WOOD_LIGHT);
        draw_outlined_world_line(gx, gy, tx, ty, windup_glint ? PAL_GOLD_SHINE : PAL_STEEL_SHINE);
        int px = -(ty - hy) / 5;
        int py =  (tx - hx) / 5;
        draw_outlined_world_line(gx - px, gy - py, gx + px, gy + py, PAL_GOLD_LIGHT);
    }

    if (windup_glint) {
        vga_fill_rect(tx - 1, ty - 1, 3, 3, PAL_GOLD_SHINE);
        vga_put_pixel(tx, ty, COL_WHITE);
    }
}

/* Draws a curved pixel-art slash swoosh or piercing thrust streak (never a rectangle!) */
static void draw_weapon_motion_trail(const Character *c, const AttackDef *atk,
                                     int rel, int total, int reach, int width)
{
    bool is_crit = (c->action == ACT_CRIT_ATTACK) || atk->is_critical;
    bool is_spec = (c->attack_slot == WATK_SPECIAL);

    uint8_t col_outer = is_crit ? COL_WHITE       : (is_spec ? PAL_ARCANE_HOT   : PAL_GOLD_SHINE);
    uint8_t col_mid   = is_crit ? PAL_GOLD_LIGHT  : (is_spec ? PAL_ARCANE_LIGHT : PAL_CRIMSON_LIGHT);
    uint8_t col_tail  = is_crit ? PAL_CRIMSON_MID : (is_spec ? PAL_ARCANE_MID   : PAL_CRIMSON_DARK);

    if (atk->anim_type == ANIM_SLASH) {
        int cur_r = width - (rel * width * 2) / total;
        int prev_r1 = cur_r + width / 3;
        int prev_r2 = cur_r + (width * 2) / 3;
        if (prev_r1 > width) prev_r1 = width;
        if (prev_r2 > width) prev_r2 = width;

        int f0 = reach - iabs(cur_r) / 3;
        int f1 = reach - iabs(prev_r1) / 3;
        int f2 = reach - iabs(prev_r2) / 3;

        int x0, y0, x1, y1, x2, y2;
        render_local_to_world(c, f0, cur_r,   &x0, &y0);
        render_local_to_world(c, f1, prev_r1, &x1, &y1);
        render_local_to_world(c, f2, prev_r2, &x2, &y2);

        /* Outer bright curved arc + inner fading smear */
        vga_draw_line(x2, y2, x1, y1, col_tail);
        vga_draw_line(x1, y1, x0, y0, col_mid);
        vga_draw_line(x1 + 1, y1, x0 + 1, y0, col_outer);

        int ix0, iy0, ix1, iy1;
        render_local_to_world(c, f0 - 3, cur_r,   &ix0, &iy0);
        render_local_to_world(c, f1 - 3, prev_r1, &ix1, &iy1);
        vga_draw_line(ix1, iy1, ix0, iy0, col_tail);
    } else {
        /* Piercing thrust / cleave speed-lines along the blade axis */
        int sx, sy, ex, ey;
        render_local_to_world(c, 6, -2, &sx, &sy);
        render_local_to_world(c, reach + 2, -2, &ex, &ey);
        vga_draw_line(sx, sy, ex, ey, col_tail);

        render_local_to_world(c, 8, 3, &sx, &sy);
        render_local_to_world(c, reach + 2, 3, &ex, &ey);
        vga_draw_line(sx, sy, ex, ey, col_mid);

        render_local_to_world(c, reach, 1, &ex, &ey);
        vga_fill_rect(ex - 1, ey - 1, 3, 3, col_outer);
    }
}

static void draw_character_weapon_anim(const Character *c)
{
    const WeaponDef *w = c->right_weapon;
    if (!w || w->kind != WEAPON_MELEE) return;

    int scale = (c->def && c->def->scale_pct > 0) ? c->def->scale_pct : 100;
    int len = (w->length_px * scale) / 100;
    if (len < 12) len = 16;

    if (c->action == ACT_STAGGER || c->action == ACT_CRIT_HURT) {
        draw_procedural_melee_weapon(c, w, -2, 8, -6, 8 + len / 2, false);
        return;
    }

    const AttackDef *atk = character_get_active_attack(c);
    if (!atk) {
        draw_procedural_melee_weapon(c, w, -2, 7, len - 4, 8, false);
        return;
    }

    int reach = (atk->reach_px * scale) / 100;
    int width = (atk->width_px * scale) / 100;
    int active_start = atk->windup_frames + 1;
    int active_end   = atk->windup_frames + atk->active_frames;

    if (c->action_frame <= atk->windup_frames) {
        bool glint = (c->action_frame > atk->windup_frames - 10) && ((c->action_frame & 1) == 0);
        if (atk->anim_type == ANIM_SLASH) {
            draw_procedural_melee_weapon(c, w, -2, 8, 2, 8 + len, glint);
        } else {
            draw_procedural_melee_weapon(c, w, -6, 6, len - 8, 6, glint);
        }
    } else if (c->action_frame <= active_end) {
        int rel = c->action_frame - active_start;
        int total = (atk->active_frames > 1) ? (atk->active_frames - 1) : 1;

        draw_weapon_motion_trail(c, atk, rel, total, reach, width);

        if (atk->anim_type == ANIM_SLASH) {
            int tip_r = width - (rel * width * 2) / total;
            int tip_f = reach - (iabs(tip_r) / 3);
            draw_procedural_melee_weapon(c, w, 2, tip_r / 4, tip_f, tip_r, true);
        } else {
            draw_procedural_melee_weapon(c, w, 4, 1, reach, 1, (c->action == ACT_CRIT_ATTACK));
        }
    } else {
        if (atk->anim_type == ANIM_SLASH) {
            draw_procedural_melee_weapon(c, w, 1, -6, 4, -6 - len / 2, false);
        } else {
            draw_procedural_melee_weapon(c, w, 2, 4, len, 3, false);
        }
    }
}

static void draw_crit_shatter_particles(const Character *c)
{
    static const int8_t dir_vx[8] = { -3,  3, -4,  4, -2,  2,  0,  0 };
    static const int8_t dir_vy[8] = { -3, -3,  1,  1,  3,  3, -4,  4 };
    int i;
    int t = c->action_frame;
    if (t > 32) return;

    for (i = 0; i < 8; i++) {
        int px = c->cx + (dir_vx[i] * t) / 4;
        int py = c->cy + (dir_vy[i] * t) / 4;
        uint8_t col = (i & 1) ? PAL_BONE_SHINE : ((t & 2) ? PAL_GOLD_SHINE : PAL_CRIMSON_HOT);
        vga_fill_rect(px - 1, py - 1, 2, 2, col);
        vga_put_pixel(px + 1, py, PAL_OUTLINE);
    }
}

/* Draws a 4-phase tucked Knight somersault with trailing speed-dust (no block!) */
static void draw_knight_roll_rig(const Character *c)
{
    int phase = (c->action_frame / 3) & 3;
    bool iframe = (c->action_frame <= player_def.roll_iframes);
    uint8_t armor_c = iframe ? PAL_STEEL_SHINE : PAL_STEEL_MID;
    uint8_t cape_c  = iframe ? PAL_EMERALD_LIGHT : PAL_BLUE_MID;
    uint8_t trim_c  = iframe ? PAL_EMERALD_HOT : PAL_GOLD_LIGHT;

    /* Trailing wind/dust streaks behind the roll direction */
    int i;
    for (i = 1; i <= 3; i++) {
        int tx = c->cx - c->roll_dx * (5 + i * 3);
        int ty = c->cy - c->roll_dy * (5 + i * 3) + (i & 1 ? -2 : 2);
        vga_draw_line(tx, ty, tx - c->roll_dx * 4, ty - c->roll_dy * 4,
                      iframe ? PAL_EMERALD_MID : PAL_FLOOR_LIGHT);
    }

    canv_clear();

    /* Rounded tucked torso silhouette (chamfered circle, never a square) */
    canv_rect(-4, -5, 9, 10, armor_c);
    canv_rect(-5, -4, 11, 8, cape_c);
    canv_rect(-3, -6, 7, 12, PAL_STEEL_LIGHT);
    canv_rect(-6, -2, 13, 5, cape_c);

    /* Rotating helmet visor, crimson plume, and armored boots by somersault phase */
    if (phase == 0) {
        /* Tucking forward: helmet down-front, boots trailing back */
        canv_rect(-2, -5, 5, 4, PAL_STEEL_SHINE);
        canv_rect(-1, -6, 3, 1, PAL_CRIMSON_LIGHT);
        canv_rect(-2, -3, 4, 1, PAL_OUTLINE);
        canv_rect(-3,  2, 6, 3, trim_c);
    } else if (phase == 1) {
        /* Mid-roll 90 deg: curled tight with spinning plume & gold belt */
        canv_rect( 1, -3, 4, 5, PAL_STEEL_SHINE);
        canv_rect( 3, -2, 2, 3, PAL_CRIMSON_LIGHT);
        canv_rect(-4, -2, 3, 5, PAL_STEEL_DARK);
        canv_line(-1, -5, -1, 5, trim_c);
    } else if (phase == 2) {
        /* Inverted apex: boots on top, helmet tucked underneath */
        canv_rect(-3, -5, 6, 3, PAL_STEEL_DARK);
        canv_rect(-2,  1, 5, 4, PAL_STEEL_SHINE);
        canv_rect(-1,  5, 3, 1, PAL_CRIMSON_LIGHT);
        canv_line(-4, 0, 4, 0, trim_c);
    } else {
        /* Uncurling into landing */
        canv_rect(-4, -3, 4, 5, PAL_STEEL_SHINE);
        canv_rect(-5, -2, 2, 3, PAL_CRIMSON_LIGHT);
        canv_rect( 1,  1, 4, 4, PAL_STEEL_DARK);
        canv_line( 0, -5, 0, 5, trim_c);
    }

    canv_blit_outlined(c->cx, c->cy, 100, 0);
}

static void render_character_vga(const Character *c)
{
    if (!c || !c->active || !c->def) return;

    int scale = (c->def->scale_pct > 0) ? c->def->scale_pct : 100;
    int rad   = character_radius(c);

    draw_floor_shadow(c->cx, c->cy + (8 * scale) / 100, rad, rad / 2 + 1);

    if (c->is_player && c->action == ACT_ROLL) {
        draw_knight_roll_rig(c);
        return;
    }

    static const int8_t stride_tbl[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
    int phase  = (c->walk_ticks / 4) & 7;
    int stride = stride_tbl[phase];
    int bob    = (stride != 0) ? -1 : 0;

    canv_clear();
    if (c->def->body_style == BSTYLE_KNIGHT) {
        generate_knight_rig(c, stride, bob);
    } else if (c->def->body_style == BSTYLE_SKELETON_LORD) {
        generate_skeleton_rig(c, stride, bob, true);
    } else {
        generate_skeleton_rig(c, stride, bob, false);
    }

    uint8_t tint = 0;
    if (c->action == ACT_CRIT_HURT && (c->action_frame & 2)) {
        tint = PAL_GOLD_SHINE;
    } else if (c->hurt_flash > 0 && (c->hurt_flash & 1)) {
        tint = PAL_CRIMSON_LIGHT;
    }

    canv_blit_outlined(c->cx, c->cy, scale, tint);

    draw_procedural_shield(c);
    draw_character_weapon_anim(c);

    if (c->action == ACT_STAGGER) {
        int sy = c->cy - (13 * scale) / 100;
        int offset = ((c->action_frame / 4) & 1) ? 4 : -4;
        vga_fill_rect(c->cx - 5, sy, 2, 2, PAL_GOLD_SHINE);
        vga_fill_rect(c->cx + 4, sy, 2, 2, PAL_GOLD_SHINE);
        vga_put_pixel(c->cx + offset, sy - 2, COL_WHITE);
    }

    if (c->action == ACT_CRIT_HURT) {
        draw_crit_shatter_particles(c);
    }
}

/* =========================================================================
 * Dungeon Arena & Graphical HUD Renderer
 * ========================================================================= */

static void draw_dungeon_arena(void)
{
    int y, x;
    vga_fill_rect(0, 0, VGA_W, ARENA_TOP, COL_BLACK);

    for (y = ARENA_TOP; y < VGA_H; y++) {
        for (x = 0; x < VGA_W; x++) {
            uint8_t col = PAL_FLOOR_MID;
            int ty = y - ARENA_TOP;
            int row_idx = ty >> 4;
            int gx = (x + (row_idx & 1) * 8) & 15;
            int gy = ty & 15;
            if (gx == 0 || gy == 0) {
                col = PAL_FLOOR_DARK;
            } else if ((gx == 1 || gy == 1) && ((x + y) & 3) == 0) {
                col = PAL_FLOOR_LIGHT;
            }
            framebuffer[y][x] = col;
        }
    }

    vga_fill_rect(0, ARENA_TOP, VGA_W, 4, PAL_WALL_MID);
    vga_fill_rect(0, ARENA_BOTTOM, VGA_W, VGA_H - ARENA_BOTTOM, PAL_WALL_MID);
    vga_fill_rect(0, ARENA_TOP, ARENA_LEFT, VGA_H - ARENA_TOP, PAL_WALL_MID);
    vga_fill_rect(ARENA_RIGHT, ARENA_TOP, VGA_W - ARENA_RIGHT, VGA_H - ARENA_TOP, PAL_WALL_MID);

    vga_draw_rect(ARENA_LEFT, ARENA_TOP + 3,
                  ARENA_RIGHT - ARENA_LEFT,
                  ARENA_BOTTOM - ARENA_TOP - 3, PAL_WALL_LIGHT);
}

static void draw_hud_bar(int x, int y, int w, int h, int val, int max_val, uint8_t fill_col, uint8_t shine_col)
{
    vga_fill_rect(x, y, w, h, PAL_FLOOR_DARK);
    if (max_val > 0 && val > 0) {
        int fw = (val * w) / max_val;
        if (fw == 0) fw = 1;
        if (fw > w) fw = w;
        vga_fill_rect(x, y, fw, h, fill_col);
        vga_fill_rect(x, y, fw, 1, shine_col);
    }
    vga_draw_rect(x - 1, y - 1, w + 2, h + 2, PAL_STEEL_MID);
}

void render_scene(const char *status_msg, uint8_t status_col)
{
    int i;
    char buf[80];

    draw_dungeon_arena();

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        render_character_vga(&enemies[i]);

        int scale = (enemies[i].def && enemies[i].def->scale_pct > 0) ? enemies[i].def->scale_pct : 100;
        int bar_y = enemies[i].cy - (16 * scale) / 100;
        if (bar_y > ARENA_TOP + 4) {
            draw_hud_bar(enemies[i].cx - 10, bar_y, 20, 3,
                         enemies[i].hp, enemies[i].max_hp, PAL_CRIMSON_MID, PAL_CRIMSON_LIGHT);
        }
    }

    if (player.active) {
        render_character_vga(&player);
    }

    vga_draw_str(4, 2, "HP", COL_LIGHTRED, -1);
    draw_hud_bar(16, 2, 64, 5, player.hp, player.max_hp, PAL_CRIMSON_MID, PAL_CRIMSON_LIGHT);

    vga_draw_str(4, 10, "ST", COL_LIGHTGREEN, -1);
    draw_hud_bar(16, 10, 64, 5, player.stamina, player.max_stamina, PAL_EMERALD_MID, PAL_EMERALD_LIGHT);

    const char *r_wep = player.right_weapon ? player.right_weapon->name : "NONE";
    snprintf(buf, sizeof(buf), "WEP:%s [TAB]", r_wep);
    vga_draw_str(88, 2, buf, PAL_GOLD_LIGHT, -1);

    const char *stance = "READY";
    uint8_t stance_col = COL_LIGHTGRAY;
    int parry_win = (player.left_weapon && player.left_weapon->parry_window_frames > 0)
        ? player.left_weapon->parry_window_frames : 11;

    if (!player.active) {
        stance = "YOU DIED [R]";
        stance_col = COL_LIGHTRED;
    } else if (player.action == ACT_BLOCK) {
        stance = (player.block_hold_frames <= parry_win) ? "*PARRY*" : "BLOCKING";
        stance_col = (player.block_hold_frames <= parry_win) ? PAL_GOLD_SHINE : COL_LIGHTCYAN;
    } else if (player.action == ACT_CRIT_ATTACK) {
        stance = "**CRITICAL!**";
        stance_col = PAL_GOLD_SHINE;
    } else if (player.action == ACT_ATTACK) {
        const AttackDef *atk = character_get_active_attack(&player);
        stance = atk ? atk->name : "ATTACK";
        stance_col = COL_YELLOW;
    } else if (player.action == ACT_ROLL) {
        stance = "DODGE ROLL";
        stance_col = COL_LIGHTGREEN;
    } else if (player.action == ACT_STAGGER || player.action == ACT_CRIT_HURT) {
        stance = "STAGGERED!";
        stance_col = COL_LIGHTRED;
    }
    snprintf(buf, sizeof(buf), "STATE:%s", stance);
    vga_draw_str(88, 10, buf, stance_col, -1);

    for (i = 0; i < 2; i++) {
        int y = (i == 0) ? 2 : 10;
        const char *ename = enemies[i].def ? enemies[i].def->name : "ENEMY";
        if (!enemies[i].active) {
            snprintf(buf, sizeof(buf), "%s:SLAIN", ename);
            vga_draw_str(196, y, buf, COL_DARKGRAY, -1);
        } else {
            const char *est = "OK";
            uint8_t ecol = COL_LIGHTGRAY;
            if (enemies[i].action == ACT_CRIT_HURT) {
                est = "CRIT!";
                ecol = PAL_GOLD_SHINE;
            } else if (enemies[i].action == ACT_STAGGER) {
                est = "STUN!";
                ecol = COL_LIGHTCYAN;
            } else if (enemies[i].action == ACT_ATTACK) {
                const AttackDef *atk = character_get_active_attack(&enemies[i]);
                bool wind = atk && (enemies[i].action_frame <= atk->windup_frames);
                est = wind ? "WIND!" : "ATK!";
                ecol = wind ? PAL_GOLD_LIGHT : COL_LIGHTRED;
            }
            snprintf(buf, sizeof(buf), "%s:%d/%d %s", ename, enemies[i].hp, enemies[i].max_hp, est);
            vga_draw_str(196, y, buf, ecol, -1);
        }
    }

    vga_draw_str(4, 19, status_msg, status_col, -1);
    vga_present();
}
