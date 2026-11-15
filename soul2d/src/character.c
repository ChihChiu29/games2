#include "character.h"
#include <string.h>

const int dir_dx[4] = {  0,  1,  0, -1 };
const int dir_dy[4] = { -1,  0,  1,  0 };

PlayerDef player_def;
EnemyDef enemy_defs[MAX_ENEMY_TYPES];
int num_enemy_defs = 0;

Character player;
Character enemies[MAX_ENEMIES];

int iabs(int v)
{
    return (v < 0) ? -v : v;
}

int character_radius(const Character *c)
{
    if (c && c->def && c->def->radius > 0) {
        return c->def->radius;
    }
    return 8;
}

static bool chars_overlap(int cx1, int cy1, int r1, int cx2, int cy2, int r2)
{
    int min_sep = r1 + r2 + 2;
    return (iabs(cx1 - cx2) < min_sep && iabs(cy1 - cy2) < min_sep);
}

bool character_can_move_to(const Character *self, int nx, int ny)
{
    int i;
    int r_self = character_radius(self);

    if (nx - r_self <= ARENA_LEFT + 4 || nx + r_self >= ARENA_RIGHT - 4) return false;
    if (ny - r_self <= ARENA_TOP  + 6 || ny + r_self >= ARENA_BOTTOM - 4) return false;

    if (!self->is_player && player.active) {
        if (chars_overlap(nx, ny, r_self, player.cx, player.cy, character_radius(&player))) {
            return false;
        }
    }
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active || &enemies[i] == self) continue;
        if (chars_overlap(nx, ny, r_self, enemies[i].cx, enemies[i].cy, character_radius(&enemies[i]))) {
            return false;
        }
    }
    return true;
}

bool character_try_move_px(Character *c, int dx, int dy)
{
    if (dx == 0 && dy == 0) return false;

    if (character_can_move_to(c, c->cx + dx, c->cy + dy)) {
        c->cx += dx;
        c->cy += dy;
        return true;
    }
    if (dx != 0 && character_can_move_to(c, c->cx + dx, c->cy)) {
        c->cx += dx;
        return true;
    }
    if (dy != 0 && character_can_move_to(c, c->cx, c->cy + dy)) {
        c->cy += dy;
        return true;
    }
    return false;
}

void character_move_subpixel(Character *c, int vx_fp, int vy_fp)
{
    if (vx_fp == 0 && vy_fp == 0) return;

    /* Normalize diagonal speed (multiply by ~11/16 ≈ 0.707) */
    if (vx_fp != 0 && vy_fp != 0) {
        vx_fp = (vx_fp * 11) / 16;
        vy_fp = (vy_fp * 11) / 16;
    }

    c->sub_x += vx_fp;
    c->sub_y += vy_fp;

    int step_x = c->sub_x / 16;
    int step_y = c->sub_y / 16;
    c->sub_x -= step_x * 16;
    c->sub_y -= step_y * 16;

    if (step_x != 0 || step_y != 0) {
        character_try_move_px(c, step_x, step_y);
    }
    c->walk_ticks++;
}

bool character_equip(Character *c, WeaponSlot slot, const char *weapon_id)
{
    if (!c) return false;
    const WeaponDef *w = (weapon_id && weapon_id[0]) ? weapon_find(weapon_id) : NULL;
    if (slot == SLOT_LEFT_HAND) {
        c->left_weapon = w;
    } else {
        c->right_weapon = w;
    }
    return (w != NULL);
}

const WeaponDef *character_get_weapon(const Character *c, WeaponSlot slot)
{
    if (!c) return NULL;
    return (slot == SLOT_LEFT_HAND) ? c->left_weapon : c->right_weapon;
}

void character_init(Character *c, bool is_player, const CharacterDef *def,
                    const EnemyDef *edef, int x, int y, Direction dir)
{
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->is_player = is_player;
    c->def = def;
    c->enemy_def = edef;
    c->cx = x;
    c->cy = y;
    c->dir = dir;
    c->hp = (def && def->max_hp > 0) ? def->max_hp : 100;
    c->max_hp = c->hp;
    c->stamina = (def && def->max_stamina > 0) ? def->max_stamina : 100;
    c->max_stamina = c->stamina;

    if (def) {
        character_equip(c, SLOT_LEFT_HAND, def->default_left_weapon);
        character_equip(c, SLOT_RIGHT_HAND, def->default_right_weapon);
    }
}

void character_start_attack(Character *c, WeaponSlot hand, int atk_slot, int regen_cd)
{
    c->action = (atk_slot == WATK_CRIT) ? ACT_CRIT_ATTACK : ACT_ATTACK;
    c->active_hand = hand;
    c->attack_slot = atk_slot;
    c->action_frame = 1;
    c->hit_dealt = false;
    c->block_hold_frames = 0;
    if (regen_cd > 0) {
        c->stamina_regen_cd = regen_cd;
    }
}

const AttackDef *character_get_active_attack(const Character *c)
{
    if (!c || (c->action != ACT_ATTACK && c->action != ACT_CRIT_ATTACK)) return NULL;
    const WeaponDef *w = character_get_weapon(c, c->active_hand);
    return weapon_get_attack(w, c->attack_slot);
}

bool character_tick_common(Character *c)
{
    if (!c->active || !c->def) return true;

    if (c->hurt_flash > 0) c->hurt_flash--;
    if (c->turn_cd > 0) c->turn_cd--;
    if (c->ai_think_cd > 0) c->ai_think_cd--;

    if (c->action == ACT_STAGGER) {
        c->action_frame++;
        if (c->action_frame > c->def->stagger_duration) {
            c->action = ACT_NONE;
            c->action_frame = 0;
        }
        return true;
    }

    if (c->action == ACT_CRIT_HURT) {
        c->action_frame++;
        int dur = (c->def->crit_hurt_duration > 0) ? c->def->crit_hurt_duration : 48;
        if (c->action_frame > dur) {
            c->action = ACT_NONE;
            c->action_frame = 0;
            if (!c->is_player) {
                c->ai_think_cd = 25;
            }
        }
        return true;
    }

    if (c->action == ACT_ATTACK || c->action == ACT_CRIT_ATTACK) {
        const AttackDef *atk = character_get_active_attack(c);
        int total_frames = atk ? (atk->windup_frames + atk->active_frames + atk->recovery_frames) : 1;
        c->action_frame++;
        if (c->action_frame > total_frames) {
            c->action = ACT_NONE;
            c->action_frame = 0;
            if (!c->is_player) {
                c->ai_think_cd = 30;
            }
        }
        return true;
    }

    return false;
}

bool character_is_behind(const Character *attacker, const Character *target)
{
    if (!attacker || !target || !attacker->active || !target->active) return false;

    int rx = attacker->cx - target->cx;
    int ry = attacker->cy - target->cy;
    int t_fdx = dir_dx[target->dir];
    int t_fdy = dir_dy[target->dir];

    int fwd_proj = rx * t_fdx + ry * t_fdy;
    int side_dist = (t_fdx != 0) ? iabs(ry) : iabs(rx);

    if (fwd_proj < -4 && side_dist <= 14 && attacker->dir == target->dir) {
        return true;
    }
    return false;
}
