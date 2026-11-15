#include "combat.h"
#include "render.h"
#include "game.h"
#include <string.h>

static void set_local_rect(const Character *c, int fwd1, int fwd2, int right1, int right2, HitRect *out)
{
    int wx1, wy1, wx2, wy2;
    render_local_to_world(c, fwd1, right1, &wx1, &wy1);
    render_local_to_world(c, fwd2, right2, &wx2, &wy2);

    out->active = true;
    out->x1 = (wx1 < wx2) ? wx1 : wx2;
    out->x2 = (wx1 > wx2) ? wx1 : wx2;
    out->y1 = (wy1 < wy2) ? wy1 : wy2;
    out->y2 = (wy1 > wy2) ? wy1 : wy2;
}

static bool rects_intersect(const HitRect *a, const HitRect *b)
{
    if (!a->active || !b->active) return false;
    return !(a->x2 < b->x1 || a->x1 > b->x2 || a->y2 < b->y1 || a->y1 > b->y2);
}

void combat_build_hitboxes(Character *c)
{
    c->hitbox.active = false;
    c->shield_box.active = false;
    if (!c->active || !c->def) return;

    int rad = character_radius(c);

    /* 1. Shield Barrier when Blocking / Parrying */
    if (c->action == ACT_BLOCK && c->left_weapon && c->left_weapon->kind == WEAPON_SHIELD) {
        set_local_rect(c, rad - 1, rad + 6, -10, 10, &c->shield_box);
    }

    /* 2. Active Weapon Hitbox */
    const AttackDef *atk = character_get_active_attack(c);
    if (atk) {
        int active_start = atk->windup_frames + 1;
        int active_end   = atk->windup_frames + atk->active_frames;

        if (c->action_frame >= active_start && c->action_frame <= active_end) {
            int rel = c->action_frame - active_start;
            int total = (atk->active_frames > 0) ? atk->active_frames : 1;
            int reach = (atk->reach_px * (c->def->scale_pct > 0 ? c->def->scale_pct : 100)) / 100;
            int w     = (atk->width_px * (c->def->scale_pct > 0 ? c->def->scale_pct : 100)) / 100;

            if (atk->anim_type == ANIM_SLASH) {
                /* 3-stage sweeping arc from right -> center -> left */
                if (rel * 3 < total) {
                    set_local_rect(c, rad - 2, reach, -2, w, &c->hitbox);
                } else if (rel * 3 < total * 2) {
                    set_local_rect(c, rad, reach + 2, -(w * 2) / 3, (w * 2) / 3, &c->hitbox);
                } else {
                    set_local_rect(c, rad - 2, reach, -w, 2, &c->hitbox);
                }
            } else {
                /* Thrust / Cleave / Impale corridor */
                int cur_reach = (rel * 4 < total) ? (rad + (reach - rad) * 2 / 3) : reach;
                set_local_rect(c, rad - 1, cur_reach, -w, w, &c->hitbox);
            }
        }
    }
}

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

        if (fwd_dist >= 12 && fwd_dist <= 34 && side_dist <= 14) {
            if (e->action == ACT_STAGGER || character_is_behind(attacker, e)) {
                return e;
            }
        }
    }
    return NULL;
}

static bool check_hitbox_vs_body(const Character *atk, const Character *tgt)
{
    if (!atk->hitbox.active || !tgt->active) return false;
    int rad = character_radius(tgt);
    HitRect body = { true, tgt->cx - rad, tgt->cy - rad, tgt->cx + rad, tgt->cy + rad };
    return rects_intersect(&atk->hitbox, &body);
}

static bool check_hitbox_vs_shield(const Character *atk, const Character *tgt)
{
    if (!atk->hitbox.active || tgt->action != ACT_BLOCK || !tgt->shield_box.active) return false;

    if (rects_intersect(&atk->hitbox, &tgt->shield_box)) {
        return true;
    }

    /* Also block if hitbox hits body from the front hemisphere while shield is raised */
    if (check_hitbox_vs_body(atk, tgt)) {
        int rel_x = atk->cx - tgt->cx;
        int rel_y = atk->cy - tgt->cy;
        switch (tgt->dir) {
        case DIR_UP:    return (rel_y <= 4);
        case DIR_DOWN:  return (rel_y >= -4);
        case DIR_LEFT:  return (rel_x <= 4);
        case DIR_RIGHT: return (rel_x >= -4);
        }
    }
    return false;
}

static int apply_armor_mitigation(int raw_dmg, const Character *tgt, bool is_crit)
{
    /* Critical hits pierce armor; normal hits are reduced by target armor (min 4 dmg) */
    int armor = (tgt && tgt->def && !is_crit) ? tgt->def->armor : 0;
    int dmg = raw_dmg - armor;
    return (dmg < 4) ? 4 : dmg;
}

void combat_resolve_all(void)
{
    int i, k;

    combat_build_hitboxes(&player);
    for (i = 0; i < MAX_ENEMIES; i++) {
        combat_build_hitboxes(&enemies[i]);
    }

    /* 1. Player Weapon Hitbox vs Enemies */
    if (player.active && player.hitbox.active && !player.hit_dealt) {
        const AttackDef *patk = character_get_active_attack(&player);
        const WeaponDef *pwep = character_get_weapon(&player, player.active_hand);

        for (i = 0; i < MAX_ENEMIES && patk; i++) {
            Character *sk = &enemies[i];
            if (!sk->active) continue;

            if (check_hitbox_vs_body(&player, sk)) {
                bool is_backstab = character_is_behind(&player, sk);
                bool is_riposte  = (sk->action == ACT_STAGGER);
                bool is_crit     = (player.action == ACT_CRIT_ATTACK) || is_backstab || is_riposte;

                int dmg = weapon_compute_damage(pwep, patk);
                if (is_crit) {
                    const AttackDef *catk = weapon_get_attack(pwep, WATK_CRIT);
                    dmg = catk ? weapon_compute_damage(pwep, catk) : (dmg * 2);
                }
                dmg = apply_armor_mitigation(dmg, sk, is_crit);

                sk->hp -= dmg;
                sk->hurt_flash = 14;
                player.hit_dealt = true;

                /* Pixel knockback */
                int kb_px = is_crit ? 10 : 5;
                for (k = 0; k < kb_px; k++) {
                    character_try_move_px(sk, dir_dx[player.dir], dir_dy[player.dir]);
                }

                if (is_crit) {
                    sk->action = ACT_CRIT_HURT;
                    sk->action_frame = 1;
                    sk->hitbox.active = false;
                } else if (patk->causes_stagger && sk->action != ACT_STAGGER) {
                    sk->action = ACT_STAGGER;
                    sk->action_frame = 35;
                    sk->hitbox.active = false;
                }

                if (sk->hp <= 0) {
                    sk->hp = 0;
                    sk->active = false;
                    game_set_status("ENEMY FELLED! ([R] Reload Lua, [TAB] Swap Weapon)",
                                    COL_LIGHTGREEN, 180);
                } else if (is_backstab) {
                    game_set_status("*** CRITICAL BACKSTAB! *** Shattered enemy from behind!",
                                    COL_YELLOW, 110);
                } else if (is_riposte || is_crit) {
                    game_set_status("*** CRITICAL RIPOSTE! *** Devastating strike on stunned enemy!",
                                    COL_YELLOW, 110);
                } else if (player.attack_slot == WATK_SPECIAL) {
                    game_set_status("Heavy Special Attack hits! (Enemy staggered!)",
                                    COL_LIGHTMAGENTA, 75);
                } else {
                    game_set_status("Normal Attack hits!",
                                    COL_LIGHTCYAN, 60);
                }
            }
        }
    }

    /* 2. Enemy Weapon Hitboxes vs Player */
    for (i = 0; i < MAX_ENEMIES; i++) {
        Character *sk = &enemies[i];
        if (!sk->active || !sk->hitbox.active || sk->hit_dealt || !player.active) continue;
        const AttackDef *eatk = character_get_active_attack(sk);
        if (!eatk) continue;

        bool hit_shield = check_hitbox_vs_shield(sk, &player);
        bool hit_body   = check_hitbox_vs_body(sk, &player);

        if (hit_shield || hit_body) {
            if (player.action == ACT_ROLL && player.action_frame <= player_def.roll_iframes) {
                sk->hit_dealt = true;
                game_set_status("EVADED! Rolled through enemy attack!",
                                COL_LIGHTGREEN, 75);
                continue;
            }

            sk->hit_dealt = true;

            if (hit_shield) {
                int parry_win = (player.left_weapon && player.left_weapon->parry_window_frames > 0)
                    ? player.left_weapon->parry_window_frames : 11;
                if (player.block_hold_frames <= parry_win) {
                    sk->action = ACT_STAGGER;
                    sk->action_frame = 1;
                    sk->hitbox.active = false;
                    player.stamina += 25;
                    if (player.stamina > player.max_stamina) player.stamina = player.max_stamina;
                    game_set_status("*** PARRY! *** Enemy stunned! Press [K] for Critical Riposte!",
                                    COL_YELLOW, 120);
                } else {
                    player.hp -= eatk->chip_damage;
                    player.stamina -= eatk->stamina_damage;
                    player.stamina_regen_cd = 35;
                    for (k = 0; k < 3; k++) {
                        character_try_move_px(&player, dir_dx[sk->dir], dir_dy[sk->dir]);
                    }
                    if (player.stamina <= 0) {
                        player.stamina = 0;
                        player.action = ACT_STAGGER;
                        player.action_frame = 1;
                        game_set_status("GUARD BROKEN! Out of Stamina!", COL_LIGHTRED, 90);
                    } else {
                        game_set_status("BLOCKED with Left Shield! (Hold [J] blocks, Tap [J] parries)",
                                        COL_LIGHTCYAN, 75);
                    }
                }
            } else {
                bool backstabbed_player = character_is_behind(sk, &player);
                int base_dmg = weapon_compute_damage(character_get_weapon(sk, sk->active_hand), eatk);
                int raw_dmg = backstabbed_player ? (base_dmg + base_dmg / 2) : base_dmg;
                int dmg = apply_armor_mitigation(raw_dmg, &player, backstabbed_player);
                player.hp -= dmg;
                player.hurt_flash = 14;
                for (k = 0; k < 5; k++) {
                    character_try_move_px(&player, dir_dx[sk->dir], dir_dy[sk->dir]);
                }
                if (backstabbed_player) {
                    player.action = ACT_CRIT_HURT;
                    player.action_frame = 1;
                    game_set_status("CRITICAL HIT FROM BEHIND! Don't turn your back on enemies!",
                                    COL_YELLOW, 100);
                } else {
                    game_set_status("OUCH! Direct hit! Use [J] Shield or [L] Roll!",
                                    COL_LIGHTRED, 90);
                }
            }

            if (player.hp <= 0) {
                player.hp = 0;
                player.active = false;
                game_set_status("YOU DIED! Press [R] to Reload & Rise Again or [ESC] to Quit",
                                COL_YELLOW, 600);
            }
        }
    }
}
