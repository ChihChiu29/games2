#include "render.h"
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

/* Shaded box with top-left highlight and bottom-right shadow */
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

/* Generates a 1px PAL_OUTLINE border around all non-zero pixels and blits
 * to the main 320x200 framebuffer with optional scaling & hurt/crit tint. */
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
                platform_put_pixel(world_cx + x, world_cy + y, col);
            }
        }
    }
}

/* Draws a dithered oval drop shadow on the dungeon floor beneath a character */
static void draw_floor_shadow(int cx, int cy, int rx, int ry)
{
    int y, x;
    for (y = -ry; y <= ry; y++) {
        for (x = -rx; x <= rx; x++) {
            if ((x * x * 100) / (rx * rx) + (y * y * 100) / (ry * ry) <= 100) {
                if (((cx + x + cy + y) & 1) == 0) {
                    platform_put_pixel(cx + x, cy + y, PAL_SHADOW);
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

    /* 1. Back Cape when facing DOWN, LEFT, or RIGHT */
    if (dir == DIR_DOWN) {
        canv_shaded_box(-5, -2 + bob, 10, 9, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
    } else if (dir == DIR_RIGHT) {
        canv_shaded_box(-6, -2 + bob, 5, 9, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
    } else if (dir == DIR_LEFT) {
        canv_shaded_box(1, -2 + bob, 5, 9, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
    }

    /* 2. Articulated Armored Legs / Boots */
    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_shaded_box(-4, 4 + stride, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_shaded_box( 1, 4 - stride, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
    } else {
        canv_shaded_box(-2 + stride, 4, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_shaded_box(-1 - stride, 4, 3, 5, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
    }

    /* 3. Armored Breastplate, Surcoat & Shoulder Pauldrons */
    if (dir == DIR_DOWN || dir == DIR_UP) {
        /* Breastplate */
        canv_shaded_box(-4, -3 + bob, 8, 7, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        /* Royal blue surcoat belt & tabard */
        canv_rect(-3, 1 + bob, 6, 3, PAL_BLUE_MID);
        canv_rect(-3, 1 + bob, 6, 1, PAL_GOLD_MID);
        /* Left & Right Pauldrons */
        canv_shaded_box(-7, -3 + bob, 3, 4, PAL_STEEL_DARK, PAL_STEEL_LIGHT, PAL_STEEL_SHINE);
        canv_shaded_box( 4, -3 + bob, 3, 4, PAL_STEEL_DARK, PAL_STEEL_MID,   PAL_STEEL_LIGHT);
        canv_put(-6, -3 + bob, PAL_GOLD_LIGHT);
        canv_put( 5, -3 + bob, PAL_GOLD_LIGHT);
        /* Flowing Cape over back when facing UP */
        if (dir == DIR_UP) {
            canv_shaded_box(-4, -2 + bob, 8, 8, PAL_BLUE_DARK, PAL_BLUE_MID, PAL_BLUE_LIGHT);
            canv_rect(-3, -2 + bob, 6, 1, PAL_GOLD_MID);
        }
    } else {
        /* Side profile torso & pauldron */
        canv_shaded_box(-3, -3 + bob, 6, 7, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_STEEL_LIGHT);
        canv_rect(-3, 1 + bob, 6, 2, PAL_BLUE_MID);
        canv_shaded_box(-2, -2 + bob, 4, 4, PAL_STEEL_MID, PAL_STEEL_LIGHT, PAL_STEEL_SHINE);
        canv_put(0, -2 + bob, PAL_GOLD_LIGHT);
    }

    /* 4. Great-Helm, Visor & Crimson Plume */
    int hy = -9 + bob;
    /* Plume curving backward */
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

    /* Helmet Dome */
    canv_shaded_box(-3, hy, 7, 6, PAL_STEEL_DARK, PAL_STEEL_LIGHT, PAL_STEEL_SHINE);
    if (dir == DIR_DOWN) {
        /* Gold cross-trim & dark T-visor */
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
        /* Back of helmet */
        canv_rect(-2, hy + 4, 5, 1, PAL_GOLD_MID);
    }
}

static void generate_skeleton_rig(const Character *c, int stride, int bob, bool is_lord)
{
    Direction dir = c->dir;
    int recoil = (c->action == ACT_STAGGER || c->action == ACT_CRIT_HURT) ? 2 : 0;

    /* 1. Tattered Crimson Mantle for Skeleton Lord */
    if (is_lord) {
        canv_shaded_box(-5, -3 + bob, 11, 8, PAL_CRIMSON_DARK, PAL_CRIMSON_MID, PAL_CRIMSON_LIGHT);
    }

    /* 2. Bony Legs (Femur + Knee Knob + Tibia) */
    if (dir == DIR_DOWN || dir == DIR_UP) {
        canv_line(-3, 4, -3, 8 + stride, PAL_BONE_MID);
        canv_put(-3, 6 + stride / 2, PAL_BONE_SHINE);
        canv_line( 3, 4,  3, 8 - stride, PAL_BONE_MID);
        canv_put( 3, 6 - stride / 2, PAL_BONE_SHINE);
    } else {
        canv_line(-1 + stride, 4, -2 + stride, 8, PAL_BONE_MID);
        canv_line( 1 - stride, 4,  2 - stride, 8, PAL_BONE_LIGHT);
    }

    /* 3. Bone Pelvis, Spine & Ribcage */
    canv_rect(-3, 3 + bob, 7, 2, PAL_BONE_MID);
    canv_line(0, -3 + bob, 0, 3 + bob, PAL_BONE_DARK);

    if (dir == DIR_DOWN || dir == DIR_UP) {
        /* 3 Distinct Curved Ribs with dark intercostal gaps */
        canv_rect(-4, -3 + bob, 9, 1, PAL_BONE_SHINE);
        canv_rect(-4, -1 + bob, 9, 1, PAL_BONE_LIGHT);
        canv_rect(-3,  1 + bob, 7, 1, PAL_BONE_MID);
        if (dir == DIR_DOWN) {
            canv_put(0, -2 + bob, PAL_BONE_MID);
            canv_put(0,  0 + bob, PAL_BONE_MID);
        } else {
            /* Prominent vertebral column on back */
            canv_line(0, -4 + bob, 0, 3 + bob, PAL_BONE_SHINE);
        }
        /* Bony Arms */
        canv_line(-5, -3 + bob, -6 - recoil, 2 + bob - recoil, PAL_BONE_LIGHT);
        canv_line( 5, -3 + bob,  6 + recoil, 2 + bob - recoil, PAL_BONE_LIGHT);
    } else {
        /* Side profile ribcage */
        canv_rect(-2, -3 + bob, 5, 1, PAL_BONE_SHINE);
        canv_rect(-2, -1 + bob, 5, 1, PAL_BONE_LIGHT);
        canv_rect(-2,  1 + bob, 4, 1, PAL_BONE_MID);
        canv_line(0, -2 + bob, (dir == DIR_RIGHT ? 3 : -3), 2 + bob, PAL_BONE_SHINE);
    }

    if (is_lord) {
        /* Dark iron & gold pauldrons on Skeleton Lord */
        canv_shaded_box(-7, -4 + bob, 3, 3, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_GOLD_LIGHT);
        canv_shaded_box( 5, -4 + bob, 3, 3, PAL_STEEL_DARK, PAL_STEEL_MID, PAL_GOLD_LIGHT);
    }

    /* 4. Skull, Jaw & Glowing Red Eye Sockets */
    int hy = -9 + bob - recoil;
    /* Rounded cranium */
    canv_shaded_box(-3, hy, 7, 5, PAL_BONE_MID, PAL_BONE_LIGHT, PAL_BONE_SHINE);
    /* Narrower jawbone */
    canv_rect(-2, hy + 5, 5, 2, PAL_BONE_MID);

    if (dir == DIR_DOWN) {
        /* Two dark eye sockets with glowing red pupils */
        canv_rect(-2, hy + 2, 2, 2, PAL_OUTLINE);
        canv_rect( 1, hy + 2, 2, 2, PAL_OUTLINE);
        canv_put(-2, hy + 2, PAL_CRIMSON_HOT);
        canv_put( 1, hy + 2, PAL_CRIMSON_HOT);
        /* Nasal cavity & teeth */
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

    /* Jagged Gold Crown on Skeleton Lord */
    if (is_lord) {
        canv_rect(-3, hy - 1, 7, 1, PAL_GOLD_MID);
        canv_put(-3, hy - 2, PAL_GOLD_SHINE);
        canv_put( 0, hy - 3, PAL_GOLD_SHINE);
        canv_put( 3, hy - 2, PAL_GOLD_SHINE);
    }
}

/* =========================================================================
 * Procedural Weapon, Shield & Hitbox Arc Renderer
 * ========================================================================= */

static void draw_outlined_world_line(int x0, int y0, int x1, int y1, uint8_t col)
{
    platform_draw_line(x0 - 1, y0, x1 - 1, y1, PAL_OUTLINE);
    platform_draw_line(x0 + 1, y0, x1 + 1, y1, PAL_OUTLINE);
    platform_draw_line(x0, y0 - 1, x1, y1 - 1, PAL_OUTLINE);
    platform_draw_line(x0, y0 + 1, x1, y1 + 1, PAL_OUTLINE);
    platform_draw_line(x0, y0, x1, y1, col);
}

/* Renders an articulated melee weapon from local (hand_fwd, hand_right) to (tip_fwd, tip_right) */
static void draw_procedural_melee_weapon(const Character *c, const WeaponDef *w,
                                         int hand_fwd, int hand_right, int tip_fwd, int tip_right,
                                         bool windup_glint)
{
    int hx, hy, tx, ty;
    render_local_to_world(c, hand_fwd, hand_right, &hx, &hy);
    render_local_to_world(c, tip_fwd, tip_right, &tx, &ty);

    if (w->style == WSTYLE_SPEAR) {
        /* Wooden shaft from hand to 70% toward tip, gleaming steel spearhead for last 30% */
        int mx = hx + ((tx - hx) * 7) / 10;
        int my = hy + ((ty - hy) * 7) / 10;
        draw_outlined_world_line(hx, hy, mx, my, PAL_WOOD_LIGHT);
        draw_outlined_world_line(mx, my, tx, ty, windup_glint ? PAL_GOLD_SHINE : PAL_STEEL_SHINE);
    } else {
        /* Sword / Greatsword: Pommel/Hilt + Gold Crossguard + Broad Steel Blade */
        int gx = hx + ((tx - hx) * 2) / 10;
        int gy = hy + ((ty - hy) * 2) / 10;
        draw_outlined_world_line(hx, hy, gx, gy, PAL_WOOD_LIGHT);
        draw_outlined_world_line(gx, gy, tx, ty, windup_glint ? PAL_GOLD_SHINE : PAL_STEEL_SHINE);
        /* Crossguard perpendicular to blade */
        int px = -(ty - hy) / 5;
        int py =  (tx - hx) / 5;
        draw_outlined_world_line(gx - px, gy - py, gx + px, gy + py, PAL_GOLD_LIGHT);
    }

    if (windup_glint) {
        platform_fill_rect(tx - 1, ty - 1, 3, 3, PAL_GOLD_SHINE);
        platform_put_pixel(tx, ty, COL_WHITE);
    }
}

static void draw_procedural_shield(const Character *c)
{
    if (!c->left_weapon || c->left_weapon->kind != WEAPON_SHIELD) return;

    if (c->action == ACT_BLOCK && c->shield_box.active) {
        int parry_win = (c->left_weapon->parry_window_frames > 0) ? c->left_weapon->parry_window_frames : 11;
        bool is_parry = (c->block_hold_frames <= parry_win);
        uint8_t fill_c = is_parry ? PAL_GOLD_LIGHT : PAL_BLUE_MID;
        uint8_t rim_c  = is_parry ? PAL_GOLD_SHINE : PAL_STEEL_SHINE;

        int w = c->shield_box.x2 - c->shield_box.x1 + 1;
        int h = c->shield_box.y2 - c->shield_box.y1 + 1;
        platform_fill_rect(c->shield_box.x1 - 1, c->shield_box.y1 - 1, w + 2, h + 2, PAL_OUTLINE);
        platform_fill_rect(c->shield_box.x1, c->shield_box.y1, w, h, fill_c);
        platform_draw_rect(c->shield_box.x1, c->shield_box.y1, w, h, rim_c);
    } else {
        /* Idle Kite Shield held on Left Arm (right = -7) */
        int sx, sy;
        render_local_to_world(c, 1, -7, &sx, &sy);
        platform_fill_rect(sx - 3, sy - 4, 6, 8, PAL_OUTLINE);
        platform_fill_rect(sx - 2, sy - 3, 4, 6, PAL_BLUE_MID);
        platform_draw_rect(sx - 2, sy - 3, 4, 6, PAL_STEEL_LIGHT);
        platform_put_pixel(sx - 1, sy, PAL_GOLD_LIGHT);
        platform_put_pixel(sx,     sy, PAL_GOLD_LIGHT);
    }
}

static void draw_active_hitbox_zone(const Character *c)
{
    if (!c->hitbox.active) return;

    const AttackDef *atk = character_get_active_attack(c);
    bool is_crit = (c->action == ACT_CRIT_ATTACK) || (atk && atk->is_critical);
    bool is_spec = (c->attack_slot == WATK_SPECIAL);

    uint8_t fill_a = is_crit ? PAL_GOLD_LIGHT  : (is_spec ? PAL_ARCANE_MID   : PAL_CRIMSON_MID);
    uint8_t fill_b = is_crit ? PAL_CRIMSON_HOT : (is_spec ? PAL_ARCANE_LIGHT : PAL_CRIMSON_LIGHT);
    uint8_t border = is_crit ? COL_WHITE       : (is_spec ? PAL_ARCANE_HOT   : PAL_GOLD_LIGHT);

    int x, y;
    for (y = c->hitbox.y1; y <= c->hitbox.y2; y++) {
        if ((unsigned)y >= SCREEN_H) continue;
        for (x = c->hitbox.x1; x <= c->hitbox.x2; x++) {
            if ((unsigned)x >= SCREEN_W) continue;
            /* Translucent/dithered pattern so floor & characters remain visible underneath */
            if (((x + y) & 1) == 0) {
                framebuffer[y][x] = ((x ^ y) & 2) ? fill_b : fill_a;
            }
        }
    }
    platform_draw_rect(c->hitbox.x1, c->hitbox.y1,
                       c->hitbox.x2 - c->hitbox.x1 + 1,
                       c->hitbox.y2 - c->hitbox.y1 + 1, border);
}

static void draw_character_weapon_anim(const Character *c)
{
    const WeaponDef *w = c->right_weapon;
    if (!w || w->kind != WEAPON_MELEE) return;

    int scale = (c->def && c->def->scale_pct > 0) ? c->def->scale_pct : 100;
    int len = (w->length_px * scale) / 100;
    if (len < 12) len = 16;

    if (c->action == ACT_STAGGER || c->action == ACT_CRIT_HURT) {
        /* Stunned: Weapon flung wide open to the side! */
        draw_procedural_melee_weapon(c, w, -2, 8, -6, 8 + len / 2, false);
        return;
    }

    const AttackDef *atk = character_get_active_attack(c);
    if (!atk) {
        /* Idle Pose: held in right hand angled slightly forward */
        draw_procedural_melee_weapon(c, w, -2, 7, len - 4, 8, false);
        return;
    }

    int reach = (atk->reach_px * scale) / 100;
    int width = (atk->width_px * scale) / 100;
    int active_start = atk->windup_frames + 1;
    int active_end   = atk->windup_frames + atk->active_frames;

    if (c->action_frame <= atk->windup_frames) {
        /* Windup Pose: pulled back or cocked wide with warning glint! */
        bool glint = (c->action_frame > atk->windup_frames - 10) && ((c->action_frame & 1) == 0);
        if (atk->anim_type == ANIM_SLASH) {
            draw_procedural_melee_weapon(c, w, -2, 8, 2, 8 + len, glint);
        } else {
            draw_procedural_melee_weapon(c, w, -6, 6, len - 8, 6, glint);
        }
    } else if (c->action_frame <= active_end) {
        /* Active Attack Pose: smoothly interpolates across slash arc or thrust extension! */
        int rel = c->action_frame - active_start;
        int total = (atk->active_frames > 1) ? (atk->active_frames - 1) : 1;
        if (atk->anim_type == ANIM_SLASH) {
            int tip_r = width - (rel * width * 2) / total;
            int tip_f = reach - (iabs(tip_r) / 3);
            draw_procedural_melee_weapon(c, w, 2, tip_r / 4, tip_f, tip_r, true);
        } else {
            draw_procedural_melee_weapon(c, w, 4, 1, reach, 1, (c->action == ACT_CRIT_ATTACK));
        }
    } else {
        /* Recovery Pose */
        if (atk->anim_type == ANIM_SLASH) {
            draw_procedural_melee_weapon(c, w, 1, -6, 4, -6 - len / 2, false);
        } else {
            draw_procedural_melee_weapon(c, w, 2, 4, len, 3, false);
        }
    }
}

/* Draws flying bone shards & crimson/gold sparks when critically struck */
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
        platform_fill_rect(px - 1, py - 1, 2, 2, col);
        platform_put_pixel(px + 1, py, PAL_OUTLINE);
    }
}

void render_character(const Character *c)
{
    if (!c || !c->active || !c->def) return;

    int scale = (c->def->scale_pct > 0) ? c->def->scale_pct : 100;
    int rad   = character_radius(c);

    /* 1. Draw Active Hitbox Zone on the floor/air first so characters & blades draw cleanly on top */
    draw_active_hitbox_zone(c);

    /* 2. Floor Drop Shadow */
    draw_floor_shadow(c->cx, c->cy + (8 * scale) / 100, rad, rad / 2 + 1);

    /* 3. Dodge Roll Animation */
    if (c->is_player && c->action == ACT_ROLL) {
        canv_clear();
        uint8_t roll_c = (c->action_frame <= player_def.roll_iframes) ? PAL_EMERALD_LIGHT : PAL_BLUE_LIGHT;
        canv_shaded_box(-5, -5, 10, 10, PAL_BLUE_DARK, roll_c, PAL_STEEL_SHINE);
        canv_blit_outlined(c->cx, c->cy, scale, 0);
        return;
    }

    /* 4. Compute Procedural Walk Cycle Stride & Torso Bob */
    static const int8_t stride_tbl[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
    int phase  = (c->walk_ticks / 4) & 7;
    int stride = stride_tbl[phase];
    int bob    = (stride != 0) ? -1 : 0;

    /* 5. Generate Articulated Body Rig on 48x48 Canvas */
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

    /* 6. Draw Equipped Shield & Melee Weapon */
    draw_procedural_shield(c);
    draw_character_weapon_anim(c);

    /* 7. Stunned Stars above Head (`ACT_STAGGER`) */
    if (c->action == ACT_STAGGER) {
        int sy = c->cy - (13 * scale) / 100;
        int offset = ((c->action_frame / 4) & 1) ? 4 : -4;
        platform_fill_rect(c->cx - 5, sy, 2, 2, PAL_GOLD_SHINE);
        platform_fill_rect(c->cx + 4, sy, 2, 2, PAL_GOLD_SHINE);
        platform_put_pixel(c->cx + offset, sy - 2, COL_WHITE);
    }

    /* 8. Critical-Attacked Bone/Spark Burst (`ACT_CRIT_HURT`) */
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

    /* Fill HUD background */
    platform_fill_rect(0, 0, SCREEN_W, ARENA_TOP, COL_BLACK);

    /* Fill Dungeon Stone Floor with subtle flagstone tile pattern */
    for (y = ARENA_TOP; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            uint8_t col = PAL_FLOOR_MID;
            int ty = y - ARENA_TOP;
            /* 16x16 stone tiles with staggered mortar joints */
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

    /* Stone Brick Perimeter Walls */
    platform_fill_rect(0, ARENA_TOP, SCREEN_W, 4, PAL_WALL_MID);
    platform_fill_rect(0, ARENA_BOTTOM, SCREEN_W, SCREEN_H - ARENA_BOTTOM, PAL_WALL_MID);
    platform_fill_rect(0, ARENA_TOP, ARENA_LEFT, SCREEN_H - ARENA_TOP, PAL_WALL_MID);
    platform_fill_rect(ARENA_RIGHT, ARENA_TOP, SCREEN_W - ARENA_RIGHT, SCREEN_H - ARENA_TOP, PAL_WALL_MID);

    platform_draw_rect(ARENA_LEFT, ARENA_TOP + 3,
                       ARENA_RIGHT - ARENA_LEFT,
                       ARENA_BOTTOM - ARENA_TOP - 3, PAL_WALL_LIGHT);
}

static void draw_hud_bar(int x, int y, int w, int h, int val, int max_val, uint8_t fill_col, uint8_t shine_col)
{
    platform_fill_rect(x, y, w, h, PAL_FLOOR_DARK);
    if (max_val > 0 && val > 0) {
        int fw = (val * w) / max_val;
        if (fw == 0) fw = 1;
        if (fw > w) fw = w;
        platform_fill_rect(x, y, fw, h, fill_col);
        platform_fill_rect(x, y, fw, 1, shine_col);
    }
    platform_draw_rect(x - 1, y - 1, w + 2, h + 2, PAL_STEEL_MID);
}

void render_scene(const char *status_msg, uint8_t status_col)
{
    int i;
    char buf[80];

    draw_dungeon_arena();

    /* Draw Enemies & Overhead HP Bars */
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        render_character(&enemies[i]);

        int scale = (enemies[i].def && enemies[i].def->scale_pct > 0) ? enemies[i].def->scale_pct : 100;
        int bar_y = enemies[i].cy - (16 * scale) / 100;
        if (bar_y > ARENA_TOP + 4) {
            draw_hud_bar(enemies[i].cx - 10, bar_y, 20, 3,
                         enemies[i].hp, enemies[i].max_hp, PAL_CRIMSON_MID, PAL_CRIMSON_LIGHT);
        }
    }

    if (player.active) {
        render_character(&player);
    }

    /* Top Graphical HUD (Rows 0..27) */
    platform_draw_str(4, 2, "HP", COL_LIGHTRED, -1);
    draw_hud_bar(16, 2, 64, 5, player.hp, player.max_hp, PAL_CRIMSON_MID, PAL_CRIMSON_LIGHT);

    platform_draw_str(4, 10, "ST", COL_LIGHTGREEN, -1);
    draw_hud_bar(16, 10, 64, 5, player.stamina, player.max_stamina, PAL_EMERALD_MID, PAL_EMERALD_LIGHT);

    const char *r_wep = player.right_weapon ? player.right_weapon->name : "NONE";
    snprintf(buf, sizeof(buf), "WEP:%s [TAB]", r_wep);
    platform_draw_str(88, 2, buf, PAL_GOLD_LIGHT, -1);

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
    platform_draw_str(88, 10, buf, stance_col, -1);

    /* Enemy status readout on top-right */
    for (i = 0; i < 2; i++) {
        int y = (i == 0) ? 2 : 10;
        const char *ename = enemies[i].def ? enemies[i].def->name : "ENEMY";
        if (!enemies[i].active) {
            snprintf(buf, sizeof(buf), "%s:SLAIN", ename);
            platform_draw_str(196, y, buf, COL_DARKGRAY, -1);
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
            platform_draw_str(196, y, buf, ecol, -1);
        }
    }

    /* Status message bar at row 19 */
    platform_draw_str(4, 19, status_msg, status_col, -1);

    platform_present();
}
