#include "world.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

WorldState current_world;

/* Procedural Enemy Generator:
 * Given a single `difficulty` factor (1, 2, ...), synthesizes a complete EnemyDef
 * (HP, armor, move speed, scale, reach, weapon, AI pattern, and fixed art). */
void enemy_generate(int difficulty, int slot_idx)
{
    if (num_enemy_defs >= MAX_ENEMY_TYPES) return;
    if (difficulty < 1) difficulty = 1;

    EnemyDef *ed = &enemy_defs[num_enemy_defs++];
    memset(ed, 0, sizeof(*ed));
    ed->used = true;
    ed->difficulty = difficulty;

    CharacterDef *b = &ed->base;
    snprintf(b->id, sizeof(b->id), "gen_lv%d_%d", difficulty, slot_idx);

    /* Choose an enemy archetype & body style weighted by difficulty */
    int archetype = rand() % 3; /* 0 = Skirmisher, 1 = Soldier, 2 = Heavy Brute */
    if (difficulty >= 2) {
        b->body_style = (archetype == 0) ? BSTYLE_KNIGHT : BSTYLE_SKELETON_LORD;
    } else {
        b->body_style = (archetype == 2 && (rand() % 2 == 0)) ? BSTYLE_KNIGHT : BSTYLE_SKELETON;
    }

    static const char *lv1_names[3] = { "L1 SKIRM", "L1 SKEL", "L1 GUARD" };
    static const char *lv2_names[3] = { "L2 DUEL",  "L2 LORD", "L2 BRUTE" };
    const char *base_name = (difficulty >= 2) ? lv2_names[archetype] : lv1_names[archetype];
    snprintf(b->name, sizeof(b->name), "%s", base_name);

    /* Scale & Collision Radius */
    if (difficulty >= 2) {
        b->scale_pct = 122 + (archetype == 2 ? 15 : (rand() % 10));
    } else {
        b->scale_pct = 95 + (rand() % 12);
    }
    b->radius = (8 * b->scale_pct) / 100;
    if (b->radius < 7) b->radius = 7;

    /* Difficulty-driven Combat Budget: HP, Armor, Move Speed, Reach, AI Cadence */
    int base_hp = 55 + difficulty * 40 + (rand() % 20);
    if (archetype == 2) base_hp += 20;
    if (archetype == 0) base_hp -= 10;
    b->max_hp = base_hp;
    b->max_stamina = 100;

    b->armor = (difficulty - 1) * 3 + (archetype == 2 ? 3 : (rand() % 2));
    b->move_speed_fp = (archetype == 0 ? 9 : (archetype == 2 ? 6 : 7)) + (difficulty - 1);
    b->turn_cooldown = (difficulty >= 2) ? 20 : 26;
    b->stagger_duration = (difficulty >= 2) ? 85 : 95;
    b->crit_hurt_duration = 48;

    ed->ai_aggression = 45 + difficulty * 18 + (archetype == 0 ? 15 : 0);
    ed->ai_special_pct = 20 + difficulty * 15 + (archetype == 2 ? 20 : 0);
    ed->attack_range_bonus = (difficulty - 1) * 4 + (archetype == 0 ? 3 : 0);

    /* Equip procedurally chosen melee weapon */
    const char *wep = ((rand() % 2) == 0) ? "broadsword" : "spear";
    strncpy(b->default_right_weapon, wep, sizeof(b->default_right_weapon) - 1);
    b->default_left_weapon[0] = '\0';

    /* Generate & lock appearance for this enemy type */
    b->has_fixed_appearance = true;
    character_generate_appearance(&b->fixed_appearance, b->body_style);
}

void world_respawn(void)
{
    static const int spawn_x[WORLD_NUM_ENEMY_TYPES] = { 190, 230, 255 };
    static const int spawn_y[WORLD_NUM_ENEMY_TYPES] = {  72, 152, 110 };
    int i;

    memset(&player, 0, sizeof(player));
    memset(enemies, 0, sizeof(enemies));

    /* Spawn player with the world's fixed appearance */
    game_spawn_player(72, 112, DIR_RIGHT);

    /* Spawn 1 enemy of each pre-built type (2 of level 1, 1 of level 2) */
    for (i = 0; i < current_world.num_prebuilt_enemies; i++) {
        game_spawn_enemy(current_world.prebuilt_enemy_ids[i],
                         spawn_x[i], spawn_y[i], DIR_LEFT);
    }
}

bool world_build(char *err_buf, int err_buf_len)
{
    int i;
    static const int init_difficulties[WORLD_NUM_ENEMY_TYPES] = { 1, 1, 2 };

    if (!data_lua_reload(err_buf, err_buf_len)) {
        return false;
    }

    current_world.seed = (unsigned int)rand();
    current_world.room_id = 1;

    /* 1. Generate and lock the Player's appearance for this entire world */
    player_def.base.has_fixed_appearance = true;
    character_generate_appearance(&player_def.base.fixed_appearance, player_def.base.body_style);

    /* 2. Pre-build N = 3 enemy types (2x Level 1, 1x Level 2) */
    num_enemy_defs = 0;
    memset(enemy_defs, 0, sizeof(enemy_defs));
    current_world.num_prebuilt_enemies = WORLD_NUM_ENEMY_TYPES;

    for (i = 0; i < WORLD_NUM_ENEMY_TYPES; i++) {
        enemy_generate(init_difficulties[i], i + 1);
        strncpy(current_world.prebuilt_enemy_ids[i],
                enemy_defs[i].base.id,
                sizeof(current_world.prebuilt_enemy_ids[i]) - 1);
    }

    /* 3. Spawn Player & 1 of each pre-built Enemy Type */
    world_respawn();
    return true;
}
