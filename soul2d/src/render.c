#include "render.h"
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

uint8_t render_rot_tile(uint8_t ch, Direction dir)
{
    if (dir == DIR_UP) return ch;

    if (ch >= TILE_SPEAR_TIP_U && ch <= TILE_SPEAR_TIP_L) {
        int idx = (ch - TILE_SPEAR_TIP_U + (int)dir) & 3;
        return (uint8_t)(TILE_SPEAR_TIP_U + idx);
    }
    if (ch >= TILE_SWORD_TIP_U && ch <= TILE_SWORD_TIP_L) {
        int idx = (ch - TILE_SWORD_TIP_U + (int)dir) & 3;
        return (uint8_t)(TILE_SWORD_TIP_U + idx);
    }
    if (ch == TILE_SHAFT_V && (dir == DIR_RIGHT || dir == DIR_LEFT)) return TILE_SHAFT_H;
    if (ch == TILE_SHAFT_H && (dir == DIR_RIGHT || dir == DIR_LEFT)) return TILE_SHAFT_V;

    if (dir == DIR_DOWN) {
        switch (ch) {
        case '^': return 'v';
        case 'v': return '^';
        case '<': return '>';
        case '>': return '<';
        case '[': return ']';
        case ']': return '[';
        case 'A': return 'V';
        default:  return ch;
        }
    }

    if (dir == DIR_RIGHT) {
        switch (ch) {
        case '^': return '>';
        case '>': return 'v';
        case 'v': return '<';
        case '<': return '^';
        case '|': return '-';
        case '-': return '|';
        case '/': return '\\';
        case '\\': return '/';
        case '[': return '=';
        case ']': return '=';
        case 'A': return '>';
        default:  return ch;
        }
    }

    if (dir == DIR_LEFT) {
        switch (ch) {
        case '^': return '<';
        case '<': return 'v';
        case 'v': return '>';
        case '>': return '^';
        case '|': return '-';
        case '-': return '|';
        case '/': return '\\';
        case '\\': return '/';
        case '[': return '=';
        case ']': return '=';
        case 'A': return '<';
        default:  return ch;
        }
    }
    return ch;
}

static void put_local(const Character *c, int fwd, int right, uint8_t base_ch, uint8_t attr)
{
    int wx, wy;
    render_local_to_world(c, fwd, right, &wx, &wy);
    if (wx <= ARENA_LEFT || wx >= ARENA_RIGHT || wy <= ARENA_TOP || wy >= ARENA_BOTTOM) return;
    platform_put_cell(wx, wy, render_rot_tile(base_ch, c->dir), attr);
}

static void draw_cell_list(const Character *c, const SpriteCell *cells, int count,
                           bool mirror_right, int8_t override_attr)
{
    int i;
    for (i = 0; i < count; i++) {
        uint8_t at = (override_attr >= 0) ? (uint8_t)override_attr : cells[i].attr;
        int r = mirror_right ? -cells[i].right : cells[i].right;
        put_local(c, cells[i].fwd, r, cells[i].ch, at);
    }
}

static void draw_weapon_hand(const Character *c, WeaponSlot slot)
{
    const WeaponDef *w = character_get_weapon(c, slot);
    if (!w) return;

    /* If this hand is currently blocking with a shield, the active shield barrier draws it */
    if (slot == SLOT_LEFT_HAND && c->action == ACT_BLOCK && w->kind == WEAPON_SHIELD) {
        return;
    }

    bool mirror = (slot == SLOT_LEFT_HAND && !w->authored_left) ||
                  (slot == SLOT_RIGHT_HAND && w->authored_left);

    /* If this hand is currently executing an attack, draw its windup or recovery pose */
    if ((c->action == ACT_ATTACK || c->action == ACT_CRIT_ATTACK) && c->active_hand == slot) {
        const AttackDef *atk = character_get_active_attack(c);
        if (atk) {
            if (c->action_frame <= atk->windup_frames) {
                int8_t flash_override = -1;
                if (c->action == ACT_CRIT_ATTACK && (c->action_frame & 1)) {
                    flash_override = (int8_t)ATTR(COL_WHITE, COL_RED);
                } else if (!c->is_player && c->action_frame > atk->windup_frames - 8 && (c->action_frame & 1)) {
                    flash_override = (int8_t)ATTR(COL_WHITE, COL_RED);
                }
                draw_cell_list(c, atk->windup_cells, atk->num_windup_cells, mirror, flash_override);
            } else if (c->action_frame > atk->windup_frames + atk->active_frames) {
                draw_cell_list(c, atk->recovery_cells, atk->num_recovery_cells, mirror, -1);
            }
            return;
        }
    }

    /* Otherwise draw the weapon's idle pose */
    draw_cell_list(c, w->idle_cells, w->num_idle_cells, mirror, -1);
}

static void draw_crit_hurt_effect(const Character *c)
{
    const CharacterDef *def = c->def;
    if (!def) return;

    /* 1. Draw archetype-specific critical-hurt pose cells (e.g. shattered bones) */
    draw_cell_list(c, def->crit_hurt_cells, def->num_crit_hurt_cells, false, -1);

    /* 2. Expanding debris/spark shockwave during first 24 frames of ACT_CRIT_HURT */
    if (c->action_frame <= 24) {
        int dist = 2 + (c->action_frame / 6);
        uint8_t spark_attr = (c->action_frame & 2)
            ? ATTR(COL_YELLOW, COL_RED)
            : ATTR(COL_WHITE, COL_MAGENTA);
        uint8_t shard_attr = ATTR(COL_WHITE, COL_RED);

        put_local(c,  dist, -dist, TILE_CRIT_SPARK, spark_attr);
        put_local(c,  dist,  dist, TILE_CRIT_SPARK, spark_attr);
        put_local(c, -dist, -dist, TILE_BONE_SHARD, shard_attr);
        put_local(c, -dist,  dist, TILE_BONE_SHARD, shard_attr);
        put_local(c,  dist + 1, 0, TILE_CRIT_SPARK, spark_attr);
    }
}

void render_character(const Character *c)
{
    int i, f, r;
    if (!c || !c->active || !c->def) return;
    const CharacterDef *def = c->def;

    /* Player Dodge Roll Animation */
    if (c->is_player && c->action == ACT_ROLL) {
        bool iframe = (c->action_frame <= player_def.roll_iframes);
        uint8_t roll_attr = iframe
            ? ATTR(COL_LIGHTGREEN, COL_BLUE)
            : ATTR(COL_LIGHTCYAN, COL_BLACK);
        uint8_t spin = (c->action_frame & 2) ? TILE_SLASH_ARC : TILE_PARRY_STAR;
        for (f = -1; f <= 1; f++) {
            for (r = -1; r <= 1; r++) {
                put_local(c, f, r, (f == 0 && r == 0) ? TILE_KNIGHT_HELM : spin, roll_attr);
            }
        }
        put_local(c, 2, 0, TILE_SPEAR_TIP_U, ATTR(COL_YELLOW, COL_BLUE));
        return;
    }

    int8_t body_override = -1;
    if (c->action == ACT_CRIT_HURT) {
        if (c->action_frame & 2) {
            body_override = (int8_t)(def->crit_flash_attr ? def->crit_flash_attr : ATTR(COL_YELLOW, COL_RED));
        } else {
            body_override = (int8_t)ATTR(COL_WHITE, COL_RED);
        }
    } else if (c->hurt_flash > 0) {
        body_override = (int8_t)ATTR(COL_WHITE, COL_RED);
    } else if (c->action == ACT_STAGGER) {
        body_override = c->is_player
            ? (int8_t)ATTR(COL_YELLOW, COL_BROWN)
            : (int8_t)ATTR(COL_LIGHTCYAN, COL_BLUE);
    }

    /* 1. Base Character Body */
    draw_cell_list(c, def->body_cells, def->num_body_cells, false, body_override);

    /* 2. Special State Overlay or Equipped Weapons */
    if (c->action == ACT_CRIT_HURT) {
        draw_crit_hurt_effect(c);
    } else if (c->action == ACT_STAGGER) {
        draw_cell_list(c, def->stagger_cells, def->num_stagger_cells, false, -1);
    } else {
        draw_weapon_hand(c, SLOT_LEFT_HAND);
        draw_weapon_hand(c, SLOT_RIGHT_HAND);
    }

    /* 3. Active Shield Barrier */
    for (i = 0; i < c->num_shield; i++) {
        platform_put_cell(c->shield[i].x, c->shield[i].y, c->shield[i].ch, c->shield[i].attr);
    }

    /* 4. Active Weapon Hitbox Cells */
    for (i = 0; i < c->num_hitbox; i++) {
        platform_put_cell(c->hitbox[i].x, c->hitbox[i].y, c->hitbox[i].ch, c->hitbox[i].attr);
    }
}

static void draw_bar(int x, int y, int width, int val, int max_val, uint8_t fill_attr, uint8_t empty_attr)
{
    int i;
    int filled = (max_val > 0) ? ((val * width) / max_val) : 0;
    if (val > 0 && filled == 0) filled = 1;
    for (i = 0; i < width; i++) {
        platform_put_cell(x + i, y, (i < filled) ? '=' : '-', (i < filled) ? fill_attr : empty_attr);
    }
}

void render_scene(const char *status_msg, uint8_t status_attr)
{
    int x, y, i;
    char buf[80];

    platform_clear(ATTR(COL_DARKGRAY, COL_BLACK));

    for (y = ARENA_TOP; y <= ARENA_BOTTOM; y++) {
        for (x = ARENA_LEFT; x <= ARENA_RIGHT; x++) {
            if (y == ARENA_TOP || y == ARENA_BOTTOM || x == ARENA_LEFT || x == ARENA_RIGHT) {
                platform_put_cell(x, y, TILE_WALL, ATTR(COL_LIGHTGRAY, COL_DARKGRAY));
            } else if (((x + y * 3) % 11 == 0) && ((x * 5 + y) % 7 == 0)) {
                platform_put_cell(x, y, TILE_FLOOR_DOT, ATTR(COL_DARKGRAY, COL_BLACK));
            }
        }
    }

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        render_character(&enemies[i]);

        int bar_y = enemies[i].cy - (character_radius(&enemies[i]) + 2);
        if (bar_y > ARENA_TOP) {
            draw_bar(enemies[i].cx - 3, bar_y, 7, enemies[i].hp, enemies[i].max_hp,
                     ATTR(COL_WHITE, COL_RED), ATTR(COL_DARKGRAY, COL_BLACK));
        }
    }

    if (player.active) {
        render_character(&player);
    }

    /* HUD (Rows 0..3) */
    for (y = 0; y < ARENA_TOP; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            platform_put_cell(x, y, ' ', ATTR(COL_WHITE, COL_BLACK));
        }
    }

    snprintf(buf, sizeof(buf), "HP %3d/%3d", player.hp, player.max_hp);
    platform_put_str(1, 0, buf, ATTR(COL_LIGHTRED, COL_BLACK));
    draw_bar(12, 0, 12, player.hp, player.max_hp, ATTR(COL_WHITE, COL_RED), ATTR(COL_RED, COL_BLACK));

    snprintf(buf, sizeof(buf), "STM %3d/%3d", player.stamina, player.max_stamina);
    platform_put_str(25, 0, buf, ATTR(COL_LIGHTGREEN, COL_BLACK));
    draw_bar(37, 0, 12, player.stamina, player.max_stamina,
             ATTR(COL_BLACK, COL_LIGHTGREEN), ATTR(COL_GREEN, COL_BLACK));

    const char *r_wep = player.right_weapon ? player.right_weapon->name : "None";
    snprintf(buf, sizeof(buf), "R:%s", r_wep);
    platform_put_str(50, 0, buf, ATTR(COL_YELLOW, COL_BLACK));

    const char *stance = "READY";
    uint8_t stance_attr = ATTR(COL_LIGHTGRAY, COL_BLACK);
    int parry_win = (player.left_weapon && player.left_weapon->parry_window_frames > 0)
        ? player.left_weapon->parry_window_frames : 11;

    if (!player.active) {
        stance = "DEAD [R=Retry]";
        stance_attr = ATTR(COL_WHITE, COL_RED);
    } else if (player.action == ACT_BLOCK) {
        if (player.block_hold_frames <= parry_win) {
            stance = "*PARRY WINDOW*";
            stance_attr = ATTR(COL_YELLOW, COL_CYAN);
        } else {
            stance = "[SHIELD BLOCK]";
            stance_attr = ATTR(COL_LIGHTCYAN, COL_BLUE);
        }
    } else if (player.action == ACT_CRIT_ATTACK) {
        stance = "**CRITICAL!**";
        stance_attr = ATTR(COL_YELLOW, COL_RED);
    } else if (player.action == ACT_ATTACK) {
        const AttackDef *atk = character_get_active_attack(&player);
        stance = atk ? atk->name : "ATTACK!";
        stance_attr = (player.attack_slot == WATK_SPECIAL)
            ? ATTR(COL_WHITE, COL_MAGENTA)
            : ATTR(COL_YELLOW, COL_RED);
    } else if (player.action == ACT_ROLL) {
        stance = "DODGE ROLL";
        stance_attr = ATTR(COL_LIGHTGREEN, COL_BLUE);
    } else if (player.action == ACT_STAGGER || player.action == ACT_CRIT_HURT) {
        stance = "STAGGERED!";
        stance_attr = ATTR(COL_YELLOW, COL_RED);
    }
    platform_put_str(64, 0, stance, stance_attr);

    for (i = 0; i < 2; i++) {
        int col = (i == 0) ? 1 : 40;
        const char *ename = enemies[i].def ? enemies[i].def->name : "Enemy";
        if (!enemies[i].active) {
            snprintf(buf, sizeof(buf), "%s #%d: DEFEATED", ename, i + 1);
            platform_put_str(col, 1, buf, ATTR(COL_DARKGRAY, COL_BLACK));
        } else {
            const char *estate = "Patrol";
            uint8_t eattr = ATTR(COL_LIGHTGRAY, COL_BLACK);
            if (enemies[i].action == ACT_CRIT_HURT) {
                estate = "*CRITICALLY HIT!*";
                eattr = ATTR(COL_YELLOW, COL_RED);
            } else if (enemies[i].action == ACT_STAGGER) {
                estate = "STUNNED (Riposte!)";
                eattr = ATTR(COL_YELLOW, COL_BLUE);
            } else if (enemies[i].action == ACT_ATTACK || enemies[i].action == ACT_CRIT_ATTACK) {
                const AttackDef *atk = character_get_active_attack(&enemies[i]);
                bool winding = atk && (enemies[i].action_frame <= atk->windup_frames);
                estate = winding ? "WINDUP!" : (atk ? atk->name : "ATTACK");
                eattr = winding ? ATTR(COL_YELLOW, COL_BROWN) : ATTR(COL_WHITE, COL_RED);
            }
            snprintf(buf, sizeof(buf), "%s#%d HP:%2d/%2d [%s]",
                     ename, i + 1, enemies[i].hp, enemies[i].max_hp, estate);
            platform_put_str(col, 1, buf, eattr);
        }
    }

    platform_put_str(1, 2, status_msg, status_attr);
    platform_put_str(1, 3,
        "WASD:Move J:Shield/Parry K:Atk/Crit I:Special L:Roll TAB:SwapWep R:Reload",
        ATTR(COL_LIGHTCYAN, COL_BLACK));

    platform_present();
}
