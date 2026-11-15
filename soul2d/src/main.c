#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char status_msg[76] = "";
static uint8_t status_col = COL_YELLOW;
static int status_timer = 180;

void game_set_status(const char *msg, uint8_t col, int frames)
{
    strncpy(status_msg, msg, sizeof(status_msg) - 1);
    status_msg[sizeof(status_msg) - 1] = '\0';
    status_col = col;
    status_timer = frames;
}

void game_spawn_player(int x, int y, Direction dir)
{
    character_init(&player, true, &player_def.base, NULL, x, y, dir);
}

void game_spawn_enemy(const char *type_id, int x, int y, Direction dir)
{
    int i;
    const EnemyDef *edef = NULL;
    for (i = 0; i < num_enemy_defs; i++) {
        if (strcmp(enemy_defs[i].base.id, type_id) == 0) {
            edef = &enemy_defs[i];
            break;
        }
    }
    if (!edef) return;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            character_init(&enemies[i], false, &edef->base, edef, x, y, dir);
            enemies[i].ai_think_cd = 20 + (i * 25);
            break;
        }
    }
}

static void build_new_world(void)
{
    char err_buf[76];
    if (world_build(err_buf, sizeof(err_buf))) {
        game_set_status("BUILT WORLD! [B]NewWorld [R]Respawn [U]Skill [TAB]Swap [K/I]Atk [J]Parry",
                        COL_LIGHTCYAN, 300);
    } else {
        game_set_status(err_buf, COL_LIGHTRED, 600);
    }
}

static void respawn_current_world(void)
{
    world_respawn();
    game_set_status("WORLD RESPAWNED! (Same Player & 3 Enemy Types) [B] Build New World",
                    COL_LIGHTGREEN, 200);
}

static void swap_player_right_weapon(void)
{
    if (!player.active || player.action != ACT_NONE) return;
    if (!player_def.alt_right_weapon[0]) return;

    const char *curr_id = player.right_weapon ? player.right_weapon->id : "";
    const char *next_id = (strcmp(curr_id, player_def.base.default_right_weapon) == 0)
        ? player_def.alt_right_weapon
        : player_def.base.default_right_weapon;

    if (character_equip(&player, SLOT_RIGHT_HAND, next_id) && player.right_weapon) {
        char buf[128];
        snprintf(buf, sizeof(buf), "EQUIPPED %s! ([K] %s, [I] %s)",
                 player.right_weapon->name,
                 player.right_weapon->attacks[WATK_NORMAL].name,
                 player.right_weapon->attacks[WATK_SPECIAL].name);
        game_set_status(buf, COL_YELLOW, 120);
    }
}

static void trigger_weapon_skill(void)
{
    WeaponSlot slot = SLOT_LEFT_HAND;
    const WeaponDef *w = character_get_weapon(&player, SLOT_LEFT_HAND);
    if (!w || !w->skill.valid) {
        slot = SLOT_RIGHT_HAND;
        w = character_get_weapon(&player, SLOT_RIGHT_HAND);
    }
    if (!w || !w->skill.valid) {
        game_set_status("[U] No Weapon Skill on equipped gear", COL_LIGHTGRAY, 60);
        return;
    }
    if (player.skill_cd > 0) {
        game_set_status("[U] Weapon Skill recharging...", COL_LIGHTGRAY, 45);
        return;
    }
    if (player.stamina < w->skill.stamina_cost) {
        game_set_status("Not enough Stamina for Weapon Skill!", COL_LIGHTRED, 50);
        return;
    }

    player.stamina -= w->skill.stamina_cost;
    player.skill_cd = w->skill.cooldown_frames;
    character_start_attack(&player, slot, w->skill.atk_slot, 45);

    char msg[76];
    snprintf(msg, sizeof(msg), "WEAPON SKILL: %s!", w->skill.name);
    game_set_status(msg, COL_YELLOW, 90);
}

static void trigger_hand_action(WeaponSlot slot, int atk_slot)
{
    const WeaponDef *w = character_get_weapon(&player, slot);
    if (!w) return;

    if (w->kind == WEAPON_SHIELD) {
        if (atk_slot == WATK_SPECIAL) {
            game_set_status("[U] LEFT SPECIAL UNUSED ON SHIELD", COL_LIGHTGRAY, 60);
        }
        return;
    }

    if (atk_slot == WATK_NORMAL && weapon_get_attack(w, WATK_CRIT)) {
        Character *crit_tgt = combat_find_crit_target(&player);
        if (crit_tgt) {
            atk_slot = WATK_CRIT;
        }
    }

    const AttackDef *atk = weapon_get_attack(w, atk_slot);
    if (!atk) return;

    int min_stam = (atk_slot == WATK_SPECIAL) ? 20 : 12;
    if (player.stamina < min_stam) return;

    player.stamina -= atk->stamina_cost;
    if (player.stamina < 0) player.stamina = 0;

    int regen_cd = (atk_slot == WATK_SPECIAL) ? 45 : 28;
    character_start_attack(&player, slot, atk_slot, regen_cd);

    if (atk_slot == WATK_CRIT) {
        game_set_status("EXECUTING CRITICAL ATTACK!", COL_YELLOW, 60);
    }
}

static void update_player(void)
{
    if (!player.active) return;

    if (player.stamina_regen_cd > 0) {
        player.stamina_regen_cd--;
    } else if (player.action != ACT_ATTACK && player.action != ACT_CRIT_ATTACK && player.action != ACT_ROLL) {
        int regen_rate = (player.action == ACT_BLOCK) ? 1 : 2;
        if ((status_timer & 1) == 0 && player.stamina < player.max_stamina) {
            player.stamina += regen_rate;
            if (player.stamina > player.max_stamina) player.stamina = player.max_stamina;
        }
    }

    if (character_tick_common(&player)) {
        return;
    }

    if (player.action == ACT_ROLL) {
        player.action_frame++;
        if (player.action_frame <= player_def.roll_iframes) {
            character_move_subpixel(&player,
                                    player.roll_dx * player_def.roll_speed_fp,
                                    player.roll_dy * player_def.roll_speed_fp);
        }
        if (player.action_frame > player_def.roll_frames) {
            player.action = ACT_NONE;
            player.action_frame = 0;
        }
        return;
    }

    if (platform_key_pressed(SC_TAB)) {
        swap_player_right_weapon();
    }

    /* J: Left-Hand Normal Use (Shield Block/Parry or Left Melee) */
    if (player.left_weapon && player.left_weapon->kind == WEAPON_SHIELD) {
        if (platform_key_down(SC_J)) {
            if (player.action != ACT_BLOCK) {
                player.action = ACT_BLOCK;
                player.block_hold_frames = 1;
            } else {
                player.block_hold_frames++;
            }
        } else if (player.action == ACT_BLOCK) {
            player.action = ACT_NONE;
            player.block_hold_frames = 0;
        }
    } else if (platform_key_pressed(SC_J)) {
        trigger_hand_action(SLOT_LEFT_HAND, WATK_NORMAL);
        if (player.action != ACT_NONE) return;
    }

    /* U: Left-Hand Special Skill (or Right-Hand Skill if Left has none) */
    if (platform_key_pressed(SC_U)) {
        trigger_weapon_skill();
        if (player.action != ACT_NONE && player.action != ACT_BLOCK) return;
    }

    /* L: Dodge Roll in facing direction */
    if (platform_key_pressed(SC_L) && player.stamina >= player_def.roll_stamina_cost) {
        player.stamina -= player_def.roll_stamina_cost;
        player.stamina_regen_cd = 32;
        player.action = ACT_ROLL;
        player.action_frame = 1;
        player.block_hold_frames = 0;
        player.roll_dx = dir_dx[player.dir];
        player.roll_dy = dir_dy[player.dir];
        game_set_status("DODGE ROLL! (INVULNERABLE I-FRAMES)", COL_LIGHTGREEN, 40);
        return;
    }

    /* K: Right-Hand Normal Attack (or Critical Attack when behind/riposting!) */
    if (platform_key_pressed(SC_K)) {
        trigger_hand_action(SLOT_RIGHT_HAND, WATK_NORMAL);
        if (player.action != ACT_NONE) return;
    }

    /* I: Right-Hand Special Attack */
    if (platform_key_pressed(SC_I)) {
        trigger_hand_action(SLOT_RIGHT_HAND, WATK_SPECIAL);
        if (player.action != ACT_NONE) return;
    }

    /* WASD Movement & 4-Direction Facing (Dark Souls 1 deliberate sub-pixel speed) */
    int move_dx = 0, move_dy = 0;
    Direction new_dir = player.dir;
    bool dir_input = false;

    if (platform_key_down(SC_W) || platform_key_down(SC_UP)) {
        move_dy = -1;
        new_dir = DIR_UP;
        dir_input = true;
    } else if (platform_key_down(SC_S) || platform_key_down(SC_DOWN)) {
        move_dy = 1;
        new_dir = DIR_DOWN;
        dir_input = true;
    }

    if (platform_key_down(SC_A) || platform_key_down(SC_LEFT)) {
        move_dx = -1;
        new_dir = DIR_LEFT;
        dir_input = true;
    } else if (platform_key_down(SC_D) || platform_key_down(SC_RIGHT)) {
        move_dx = 1;
        new_dir = DIR_RIGHT;
        dir_input = true;
    }

    if (dir_input) {
        player.dir = new_dir;
        int spd = player_def.base.move_speed_fp;
        if (player.action == ACT_BLOCK && player.left_weapon && player.left_weapon->block_speed_fp > 0) {
            spd = player.left_weapon->block_speed_fp;
        }
        character_move_subpixel(&player, move_dx * spd, move_dy * spd);
    }
}

static void update_enemy(Character *e)
{
    int atk_slot = 0;
    if (character_tick_common(e)) return;
    if (!player.active || !e->enemy_def) return;

    int dx = player.cx - e->cx;
    int dy = player.cy - e->cy;
    int adx = iabs(dx);
    int ady = iabs(dy);

    /* Deliberate turning cooldown so the player can roll behind an enemy's back */
    if (e->turn_cd == 0) {
        Direction desired = (adx >= ady)
            ? ((dx >= 0) ? DIR_RIGHT : DIR_LEFT)
            : ((dy >= 0) ? DIR_DOWN  : DIR_UP);
        if (desired != e->dir) {
            e->dir = desired;
            e->turn_cd = e->def->turn_cooldown;
        }
    }

    int primary_dist = (adx >= ady) ? adx : ady;
    int off_axis     = (adx >= ady) ? ady : adx;

    if (e->ai_think_cd == 0 && e->right_weapon) {
        if (e->enemy_def->ai_func[0]) {
            const char *decision = data_lua_call_ai(e->enemy_def, e, primary_dist, off_axis);
            if (weapon_find_attack_by_name(e->right_weapon, decision, &atk_slot)) {
                character_start_attack(e, SLOT_RIGHT_HAND, atk_slot, 0);
                return;
            }
        } else {
            int engage_dist = 28 + e->enemy_def->attack_range_bonus;
            if (primary_dist <= engage_dist && off_axis <= 14) {
                if ((rand() % 100) < e->enemy_def->ai_aggression) {
                    int slot = ((rand() % 100) < e->enemy_def->ai_special_pct)
                        ? WATK_SPECIAL : WATK_NORMAL;
                    if (weapon_get_attack(e->right_weapon, slot)) {
                        character_start_attack(e, SLOT_RIGHT_HAND, slot, 0);
                        return;
                    }
                }
                e->ai_think_cd = 12;
            }
        }
    }

    int spd = e->def->move_speed_fp;
    int step_x = 0, step_y = 0;
    if (primary_dist > 22) {
        if (dx > 2) step_x = 1;
        else if (dx < -2) step_x = -1;
        if (dy > 2) step_y = 1;
        else if (dy < -2) step_y = -1;
    } else if (off_axis > 5) {
        if (adx >= ady) {
            step_y = (dy > 0) ? 1 : -1;
        } else {
            step_x = (dx > 0) ? 1 : -1;
        }
    }

    if (step_x != 0 || step_y != 0) {
        character_move_subpixel(e, step_x * spd, step_y * spd);
    }
}

int main(void)
{
    int i;
    srand((unsigned int)time(NULL));
    platform_init();
    data_lua_init();
    build_new_world();

    while (true) {
        platform_poll_input();

        if (platform_key_pressed(SC_ESC)) {
            break;
        }
        if (platform_key_pressed(SC_B)) {
            build_new_world();
        }
        if (platform_key_pressed(SC_R)) {
            respawn_current_world();
        }

        if (status_timer > 0) {
            status_timer--;
        }

        update_player();
        for (i = 0; i < MAX_ENEMIES; i++) {
            update_enemy(&enemies[i]);
        }
        combat_resolve_all();

        render_scene(status_msg, status_col);
        platform_wait_frame();
    }

    data_lua_shutdown();
    platform_shutdown();
    return 0;
}
