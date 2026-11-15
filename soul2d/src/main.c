#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char status_msg[76] = "";
static uint8_t status_attr = ATTR(COL_YELLOW, COL_BLACK);
static int status_timer = 180;

void game_set_status(const char *msg, uint8_t attr, int frames)
{
    strncpy(status_msg, msg, sizeof(status_msg) - 1);
    status_msg[sizeof(status_msg) - 1] = '\0';
    status_attr = attr;
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
            enemies[i].ai_think_cd = 15 + (i * 20);
            break;
        }
    }
}

static void reload_and_reset(void)
{
    char err_buf[76];
    memset(&player, 0, sizeof(player));
    memset(enemies, 0, sizeof(enemies));

    if (data_lua_reload(err_buf, sizeof(err_buf))) {
        game_set_status("Loaded Lua! [K] Atk/Crit [I] Special [J] Shield/Parry [TAB] Swap Weapon",
                        ATTR(COL_LIGHTCYAN, COL_BLACK), 240);
    } else {
        game_set_status(err_buf, ATTR(COL_WHITE, COL_RED), 600);
    }
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
        snprintf(buf, sizeof(buf), "Equipped Right Hand: %s! ([K] %s, [I] %s)",
                 player.right_weapon->name,
                 player.right_weapon->attacks[WATK_NORMAL].name,
                 player.right_weapon->attacks[WATK_SPECIAL].name);
        game_set_status(buf, ATTR(COL_YELLOW, COL_BLUE), 120);
    }
}

static void trigger_hand_action(WeaponSlot slot, int atk_slot)
{
    const WeaponDef *w = character_get_weapon(&player, slot);
    if (!w) return;

    if (w->kind == WEAPON_SHIELD) {
        if (atk_slot == WATK_SPECIAL) {
            game_set_status("[U] Left-hand special not used on Shield", ATTR(COL_LIGHTGRAY, COL_BLACK), 60);
        }
        return;
    }

    /* Check if a Normal Attack [K] is eligible to upgrade into a Critical Attack
     * (Riposte on a stunned enemy or Backstab from behind an enemy)! */
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

    int regen_cd = (atk_slot == WATK_SPECIAL) ? 40 : 25;
    character_start_attack(&player, slot, atk_slot, regen_cd);

    if (atk_slot == WATK_CRIT) {
        game_set_status("EXECUTING CRITICAL ATTACK!", ATTR(COL_YELLOW, COL_RED), 60);
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
        if (player.action_frame <= player_def.roll_iframes &&
            (player.action_frame % 2 == 1 || player.action_frame <= 4)) {
            character_try_move(&player, player.roll_dx, player.roll_dy);
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

    /* U: Left-Hand Special Use */
    if (platform_key_pressed(SC_U)) {
        trigger_hand_action(SLOT_LEFT_HAND, WATK_SPECIAL);
        if (player.action != ACT_NONE && player.action != ACT_BLOCK) return;
    }

    /* L: Dodge Roll in facing direction */
    if (platform_key_pressed(SC_L) && player.stamina >= player_def.roll_stamina_cost) {
        player.stamina -= player_def.roll_stamina_cost;
        player.stamina_regen_cd = 28;
        player.action = ACT_ROLL;
        player.action_frame = 1;
        player.block_hold_frames = 0;
        player.roll_dx = dir_dx[player.dir];
        player.roll_dy = dir_dy[player.dir];
        game_set_status("Dodge Roll! (Invulnerable i-frames)", ATTR(COL_LIGHTGREEN, COL_BLACK), 40);
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

    /* WASD Movement & 4-Direction Facing */
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
        if (player.move_cd == 0) {
            character_try_move(&player, move_dx, move_dy);
            int block_cd = (player.left_weapon && player.left_weapon->block_move_cd > 0)
                ? player.left_weapon->block_move_cd : 5;
            player.move_cd = (player.action == ACT_BLOCK)
                ? block_cd
                : player_def.base.move_cooldown;
        }
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

    /* Enemies only turn to face the player when their move/turn cooldown allows,
     * giving the player a window to roll behind their back for a Backstab! */
    if (e->move_cd == 0) {
        if (adx >= ady) {
            e->dir = (dx >= 0) ? DIR_RIGHT : DIR_LEFT;
        } else {
            e->dir = (dy >= 0) ? DIR_DOWN : DIR_UP;
        }
    }

    int primary_dist = (adx >= ady) ? adx : ady;
    int off_axis     = (adx >= ady) ? ady : adx;

    if (e->ai_think_cd == 0 && e->right_weapon) {
        const char *decision = data_lua_call_ai(e->enemy_def, e, primary_dist, off_axis);
        if (weapon_find_attack_by_name(e->right_weapon, decision, &atk_slot)) {
            character_start_attack(e, SLOT_RIGHT_HAND, atk_slot, 0);
            return;
        }
    }

    if (e->move_cd == 0) {
        int step_x = 0, step_y = 0;
        if (primary_dist > 6) {
            if (dx > 0) step_x = 1;
            else if (dx < 0) step_x = -1;
            if (dy > 0) step_y = 1;
            else if (dy < 0) step_y = -1;
        } else if (off_axis > 1) {
            if (adx >= ady) {
                step_y = (dy > 0) ? 1 : -1;
            } else {
                step_x = (dx > 0) ? 1 : -1;
            }
        }

        if (step_x != 0 || step_y != 0) {
            character_try_move(e, step_x, step_y);
            e->move_cd = e->def->move_cooldown;
        }
    }
}

int main(void)
{
    int i;
    srand(42);
    platform_init();
    data_lua_init();
    reload_and_reset();

    while (true) {
        platform_poll_input();

        if (platform_key_pressed(SC_ESC)) {
            break;
        }
        if (platform_key_pressed(SC_R)) {
            reload_and_reset();
        }

        if (status_timer > 0) {
            status_timer--;
        }

        update_player();
        for (i = 0; i < MAX_ENEMIES; i++) {
            update_enemy(&enemies[i]);
        }
        combat_resolve_all();

        render_scene(status_msg, status_attr);
        platform_wait_frame();
    }

    data_lua_shutdown();
    platform_shutdown();
    return 0;
}
