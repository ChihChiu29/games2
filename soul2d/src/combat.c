#include "combat.h"
#include "render.h"
#include "game.h"
#include <string.h>

static void add_hitbox_local(Character *c, int fwd, int right, uint8_t base_ch, uint8_t attr)
{
    if (c->num_hitbox >= MAX_HITBOX_CELLS) return;
    int wx, wy;
    render_local_to_world(c, fwd, right, &wx, &wy);
    if (wx <= ARENA_LEFT || wx >= ARENA_RIGHT || wy <= ARENA_TOP || wy >= ARENA_BOTTOM) return;
    c->hitbox[c->num_hitbox].x = wx;
    c->hitbox[c->num_hitbox].y = wy;
    c->hitbox[c->num_hitbox].ch = render_rot_tile(base_ch, c->dir);
    c->hitbox[c->num_hitbox].attr = attr;
    c->num_hitbox++;
}

static void add_shield_local(Character *c, int fwd, int right, uint8_t base_ch, uint8_t attr)
{
    if (c->num_shield >= 16) return;
    int wx, wy;
    render_local_to_world(c, fwd, right, &wx, &wy);
    if (wx <= ARENA_LEFT || wx >= ARENA_RIGHT || wy <= ARENA_TOP || wy >= ARENA_BOTTOM) return;
    c->shield[c->num_shield].x = wx;
    c->shield[c->num_shield].y = wy;
    c->shield[c->num_shield].ch = render_rot_tile(base_ch, c->dir);
    c->shield[c->num_shield].attr = attr;
    c->num_shield++;
}

void combat_build_hitboxes(Character *c)
{
    int i, r;
    c->num_hitbox = 0;
    c->num_shield = 0;
    if (!c->active || !c->def) return;

    /* 1. Shield Barrier when Blocking / Parrying */
    if (c->action == ACT_BLOCK && c->left_weapon && c->left_weapon->kind == WEAPON_SHIELD) {
        int parry_win = (c->left_weapon->parry_window_frames > 0) ? c->left_weapon->parry_window_frames : 11;
        bool is_parry = (c->block_hold_frames <= parry_win);
        uint8_t sh_attr = is_parry
            ? ATTR(COL_YELLOW, COL_CYAN)
            : ATTR(COL_LIGHTCYAN, COL_BLUE);
        uint8_t edge_ch = is_parry ? TILE_PARRY_STAR : TILE_SHIELD_EDGE;
        uint8_t mid_ch  = is_parry ? TILE_SLASH_ARC  : TILE_SHIELD_MID;

        for (r = -2; r <= 2; r++) {
            uint8_t ch = (r == -2 || r == 2) ? edge_ch : mid_ch;
            add_shield_local(c, 2, r, ch, sh_attr);
            add_shield_local(c, 3, r, ch, sh_attr);
        }
    }

    /* 2. Active Weapon Hitboxes */
    const AttackDef *atk = character_get_active_attack(c);
    if (atk) {
        const WeaponDef *w = character_get_weapon(c, c->active_hand);
        bool mirror = w && ((c->active_hand == SLOT_LEFT_HAND && !w->authored_left) ||
                            (c->active_hand == SLOT_RIGHT_HAND && w->authored_left));
        int active_start = atk->windup_frames + 1;
        int active_end   = atk->windup_frames + atk->active_frames;

        if (c->action_frame >= active_start && c->action_frame <= active_end) {
            int rel = c->action_frame - active_start;
            int accum = 0;
            for (i = 0; i < atk->num_phases; i++) {
                const AttackPhase *ph = &atk->phases[i];
                if (rel < accum + ph->duration_frames) {
                    int cell_i;
                    for (cell_i = 0; cell_i < ph->num_cells; cell_i++) {
                        int right_coord = mirror ? -ph->cells[cell_i].right : ph->cells[cell_i].right;
                        add_hitbox_local(c, ph->cells[cell_i].fwd, right_coord,
                                         ph->cells[cell_i].ch, ph->cells[cell_i].attr);
                    }
                    break;
                }
                accum += ph->duration_frames;
            }
        }
    }
}

/* Finds an enemy in front of `attacker` eligible for an immediate Critical Attack:
 * - Enemy is stunned (`ACT_STAGGER`, e.g. parried), OR
 * - Attacker is behind the enemy's back (`character_is_behind`). */
Character *combat_find_crit_target(const Character *attacker)
{
    int i;
    if (!attacker || !attacker->active) return NULL;

    int fdx = dir_dx[attacker->dir];
    int fdy = dir_dy[attacker->dir];

    for (i = 0; i < MAX_ENEMIES; i++) {
        Character *e = &enemies[i];
        if (!e->active) continue;

        int rx = e->cx - attacker->cx;
        int ry = e->cy - attacker->cy;
        int fwd_dist = rx * fdx + ry * fdy;
        int side_dist = (fdx != 0) ? iabs(ry) : iabs(rx);

        if (fwd_dist >= 4 && fwd_dist <= 8 && side_dist <= 3) {
            if (e->action == ACT_STAGGER || character_is_behind(attacker, e)) {
                return e;
            }
        }
    }
    return NULL;
}

static bool check_hitbox_vs_body(const Character *atk, const Character *tgt)
{
    int i;
    int rad = character_radius(tgt);
    for (i = 0; i < atk->num_hitbox; i++) {
        int hx = atk->hitbox[i].x;
        int hy = atk->hitbox[i].y;
        if (hx >= tgt->cx - rad && hx <= tgt->cx + rad &&
            hy >= tgt->cy - rad && hy <= tgt->cy + rad) {
            return true;
        }
    }
    return false;
}

static bool check_hitbox_vs_shield(const Character *atk, const Character *tgt)
{
    int i, j;
    if (tgt->action != ACT_BLOCK) return false;

    for (i = 0; i < atk->num_hitbox; i++) {
        int hx = atk->hitbox[i].x;
        int hy = atk->hitbox[i].y;
        for (j = 0; j < tgt->num_shield; j++) {
            if (iabs(hx - tgt->shield[j].x) <= 1 && iabs(hy - tgt->shield[j].y) <= 1) {
                return true;
            }
        }
    }

    int rel_x = atk->cx - tgt->cx;
    int rel_y = atk->cy - tgt->cy;
    switch (tgt->dir) {
    case DIR_UP:    return (rel_y <= 1);
    case DIR_DOWN:  return (rel_y >= -1);
    case DIR_LEFT:  return (rel_x <= 1);
    case DIR_RIGHT: return (rel_x >= -1);
    }
    return false;
}

void combat_resolve_all(void)
{
    int i;

    combat_build_hitboxes(&player);
    for (i = 0; i < MAX_ENEMIES; i++) {
        combat_build_hitboxes(&enemies[i]);
    }

    /* 1. Player Weapon Hitbox vs Enemies */
    if (player.active && player.num_hitbox > 0 && !player.hit_dealt) {
        const AttackDef *patk = character_get_active_attack(&player);
        const WeaponDef *pwep = character_get_weapon(&player, player.active_hand);

        for (i = 0; i < MAX_ENEMIES && patk; i++) {
            Character *sk = &enemies[i];
            if (!sk->active) continue;

            if (check_hitbox_vs_body(&player, sk)) {
                bool is_backstab = character_is_behind(&player, sk);
                bool is_riposte  = (sk->action == ACT_STAGGER);
                bool is_crit     = (player.action == ACT_CRIT_ATTACK) || is_backstab || is_riposte;

                int dmg = patk->damage;
                if (is_crit) {
                    const AttackDef *catk = weapon_get_attack(pwep, WATK_CRIT);
                    dmg = catk ? catk->damage : (patk->damage * 2);
                }

                sk->hp -= dmg;
                sk->hurt_flash = 12;
                player.hit_dealt = true;

                /* Knockback (2 cells on critical hit, 1 cell on normal hit) */
                character_try_move(sk, dir_dx[player.dir], dir_dy[player.dir]);
                if (is_crit) {
                    character_try_move(sk, dir_dx[player.dir], dir_dy[player.dir]);
                }

                if (is_crit) {
                    sk->action = ACT_CRIT_HURT;
                    sk->action_frame = 1;
                    sk->num_hitbox = 0;
                } else if (patk->causes_stagger && sk->action != ACT_STAGGER) {
                    sk->action = ACT_STAGGER;
                    sk->action_frame = 45;
                    sk->num_hitbox = 0;
                }

                if (sk->hp <= 0) {
                    sk->hp = 0;
                    sk->active = false;
                    game_set_status("ENEMY FELLED! (Press [R] to reload Lua & respawn, [TAB] to swap weapon)",
                                    ATTR(COL_LIGHTGREEN, COL_BLACK), 180);
                } else if (is_backstab) {
                    game_set_status("*** CRITICAL BACKSTAB! *** Struck enemy from behind! Bone-shatter!",
                                    ATTR(COL_YELLOW, COL_RED), 110);
                } else if (is_riposte || is_crit) {
                    game_set_status("*** CRITICAL RIPOSTE! *** Devastating strike on stunned enemy!",
                                    ATTR(COL_YELLOW, COL_RED), 110);
                } else if (player.attack_slot == WATK_SPECIAL) {
                    game_set_status("Heavy Special Attack hits! (Enemy staggered!)",
                                    ATTR(COL_LIGHTMAGENTA, COL_BLACK), 75);
                } else {
                    game_set_status("Normal Attack hits!",
                                    ATTR(COL_LIGHTCYAN, COL_BLACK), 60);
                }
            }
        }
    }

    /* 2. Enemy Weapon Hitboxes vs Player */
    for (i = 0; i < MAX_ENEMIES; i++) {
        Character *sk = &enemies[i];
        if (!sk->active || sk->num_hitbox == 0 || sk->hit_dealt || !player.active) continue;
        const AttackDef *eatk = character_get_active_attack(sk);
        if (!eatk) continue;

        bool hit_shield = check_hitbox_vs_shield(sk, &player);
        bool hit_body   = check_hitbox_vs_body(sk, &player);

        if (hit_shield || hit_body) {
            if (player.action == ACT_ROLL && player.action_frame <= player_def.roll_iframes) {
                sk->hit_dealt = true;
                game_set_status("EVADED! Rolled through enemy attack!",
                                ATTR(COL_LIGHTGREEN, COL_BLACK), 75);
                continue;
            }

            sk->hit_dealt = true;

            if (hit_shield) {
                int parry_win = (player.left_weapon && player.left_weapon->parry_window_frames > 0)
                    ? player.left_weapon->parry_window_frames : 11;
                if (player.block_hold_frames <= parry_win) {
                    sk->action = ACT_STAGGER;
                    sk->action_frame = 1;
                    sk->num_hitbox = 0;
                    player.stamina += 25;
                    if (player.stamina > player.max_stamina) player.stamina = player.max_stamina;
                    game_set_status("*** PARRY! *** Enemy stunned! Press [K] for Critical Riposte!",
                                    ATTR(COL_YELLOW, COL_BLUE), 120);
                } else {
                    player.hp -= eatk->chip_damage;
                    player.stamina -= eatk->stamina_damage;
                    player.stamina_regen_cd = 30;
                    if (player.stamina <= 0) {
                        player.stamina = 0;
                        player.action = ACT_STAGGER;
                        player.action_frame = 1;
                        game_set_status("GUARD BROKEN! Out of Stamina!", ATTR(COL_WHITE, COL_RED), 90);
                    } else {
                        game_set_status("BLOCKED with Left Shield! (Hold [J] blocks, Tap [J] parries)",
                                        ATTR(COL_LIGHTCYAN, COL_BLUE), 75);
                    }
                }
            } else {
                bool backstabbed_player = character_is_behind(sk, &player);
                int dmg = backstabbed_player ? (eatk->damage + eatk->damage / 2) : eatk->damage;
                player.hp -= dmg;
                player.hurt_flash = 12;
                if (backstabbed_player) {
                    player.action = ACT_CRIT_HURT;
                    player.action_frame = 1;
                    game_set_status("CRITICAL HIT FROM BEHIND! Don't turn your back on enemies!",
                                    ATTR(COL_YELLOW, COL_RED), 100);
                } else {
                    game_set_status("OUCH! Direct hit! Use [J] Shield or [L] Roll!",
                                    ATTR(COL_WHITE, COL_RED), 90);
                }
            }

            if (player.hp <= 0) {
                player.hp = 0;
                player.active = false;
                game_set_status("YOU DIED! Press [R] to Reload & Rise Again or [ESC] to Quit",
                                ATTR(COL_YELLOW, COL_RED), 600);
            }
        }
    }
}
