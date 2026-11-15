#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PlayerConfig player_cfg;
EnemyType enemy_types[MAX_ENEMY_TYPES];
int num_enemy_types = 0;
Entity player;
Entity enemies[MAX_ENEMIES];

static char status_msg[76] = "";
static uint8_t status_attr = ATTR(COL_YELLOW, COL_BLACK);
static int status_timer = 180;

static const int dir_dx[4] = {  0,  1,  0, -1 };
static const int dir_dy[4] = { -1,  0,  1,  0 };

static void set_status(const char *msg, uint8_t attr, int frames)
{
    strncpy(status_msg, msg, sizeof(status_msg) - 1);
    status_msg[sizeof(status_msg) - 1] = '\0';
    status_attr = attr;
    status_timer = frames;
}

static int iabs(int v) { return (v < 0) ? -v : v; }

static int entity_radius(const Entity *e)
{
    if (e->is_player) return player_cfg.radius > 0 ? player_cfg.radius : 2;
    if (e->type_idx >= 0 && e->type_idx < num_enemy_types) {
        return enemy_types[e->type_idx].radius > 0 ? enemy_types[e->type_idx].radius : 2;
    }
    return 2;
}

static bool entities_overlap(int cx1, int cy1, int r1, int cx2, int cy2, int r2)
{
    int min_sep = r1 + r2;
    return (iabs(cx1 - cx2) < min_sep && iabs(cy1 - cy2) < min_sep);
}

static bool can_move_to(const Entity *self, int nx, int ny)
{
    int i;
    int r_self = entity_radius(self);
    if (nx - (r_self + 1) <= ARENA_LEFT || nx + (r_self + 1) >= ARENA_RIGHT) return false;
    if (ny - (r_self + 1) <= ARENA_TOP  || ny + (r_self + 1) >= ARENA_BOTTOM) return false;

    if (!self->is_player && player.active) {
        if (entities_overlap(nx, ny, r_self, player.cx, player.cy, entity_radius(&player)))
            return false;
    }
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active || &enemies[i] == self) continue;
        if (entities_overlap(nx, ny, r_self, enemies[i].cx, enemies[i].cy, entity_radius(&enemies[i])))
            return false;
    }
    return true;
}

void game_spawn_player(int x, int y, Direction dir)
{
    memset(&player, 0, sizeof(player));
    player.active = true;
    player.is_player = true;
    player.cx = x;
    player.cy = y;
    player.dir = dir;
    player.hp = player_cfg.max_hp > 0 ? player_cfg.max_hp : 100;
    player.max_hp = player.hp;
    player.stamina = player_cfg.max_stamina > 0 ? player_cfg.max_stamina : 100;
    player.max_stamina = player.stamina;
}

void game_spawn_enemy(const char *type_id, int x, int y, Direction dir)
{
    int i, t_idx = -1;
    for (i = 0; i < num_enemy_types; i++) {
        if (strcmp(enemy_types[i].id, type_id) == 0) {
            t_idx = i;
            break;
        }
    }
    if (t_idx < 0) return;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            Entity *e = &enemies[i];
            memset(e, 0, sizeof(*e));
            e->active = true;
            e->is_player = false;
            e->type_idx = t_idx;
            e->cx = x;
            e->cy = y;
            e->dir = dir;
            e->hp = enemy_types[t_idx].max_hp;
            e->max_hp = enemy_types[t_idx].max_hp;
            e->ai_think_cd = 15 + (i * 20);
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
        set_status("Loaded data/*.lua! [J] Block/Parry  [K] Thrust  [I] Sweep  [L] Roll",
                   ATTR(COL_LIGHTCYAN, COL_BLACK), 240);
    } else {
        set_status(err_buf, ATTR(COL_WHITE, COL_RED), 600);
    }
}

static bool check_hitbox_vs_body(const Entity *atk, const Entity *tgt)
{
    int i;
    int rad = entity_radius(tgt);
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

static bool check_hitbox_vs_shield(const Entity *atk, const Entity *tgt)
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

static void update_player(void)
{
    if (!player.active) return;

    if (player.hurt_flash > 0) player.hurt_flash--;
    if (player.move_cd > 0) player.move_cd--;

    if (player.stamina_regen_cd > 0) {
        player.stamina_regen_cd--;
    } else if (player.action != ACT_ATTACK && player.action != ACT_ROLL) {
        int regen_rate = (player.action == ACT_BLOCK) ? 1 : 2;
        if ((status_timer & 1) == 0 && player.stamina < player.max_stamina) {
            player.stamina += regen_rate;
            if (player.stamina > player.max_stamina) player.stamina = player.max_stamina;
        }
    }

    if (player.action == ACT_STAGGER) {
        player.action_frame++;
        if (player.action_frame > 32) {
            player.action = ACT_NONE;
            player.action_frame = 0;
        }
        return;
    }

    if (player.action == ACT_ROLL) {
        player.action_frame++;
        if (player.action_frame <= player_cfg.roll_iframes &&
            (player.action_frame % 2 == 1 || player.action_frame <= 4)) {
            int nx = player.cx + player.roll_dx;
            int ny = player.cy + player.roll_dy;
            if (can_move_to(&player, nx, ny)) {
                player.cx = nx;
                player.cy = ny;
            }
        }
        if (player.action_frame > player_cfg.roll_frames) {
            player.action = ACT_NONE;
            player.action_frame = 0;
        }
        return;
    }

    if (player.action == ACT_ATTACK) {
        const AttackDef *atk = (player.attack_idx == 1) ? &player_cfg.heavy_atk : &player_cfg.light_atk;
        int total_frames = atk->windup_frames + atk->active_frames + atk->recovery_frames;
        player.action_frame++;
        if (player.action_frame > total_frames) {
            player.action = ACT_NONE;
            player.action_frame = 0;
        }
        return;
    }

    /* J: Left-Hand Shield Block / Parry */
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

    /* L: Roll in facing direction */
    if (platform_key_pressed(SC_L) && player.stamina >= player_cfg.roll_stamina_cost) {
        player.stamina -= player_cfg.roll_stamina_cost;
        player.stamina_regen_cd = 28;
        player.action = ACT_ROLL;
        player.action_frame = 1;
        player.block_hold_frames = 0;
        player.roll_dx = dir_dx[player.dir];
        player.roll_dy = dir_dy[player.dir];
        set_status("Dodge Roll! (Invulnerable i-frames)", ATTR(COL_LIGHTGREEN, COL_BLACK), 40);
        return;
    }

    /* K: Right-Hand Normal Attack (Fast Spear Thrust) */
    if (platform_key_pressed(SC_K) && player.stamina >= 12) {
        player.stamina -= player_cfg.light_atk.stamina_cost;
        if (player.stamina < 0) player.stamina = 0;
        player.stamina_regen_cd = 25;
        player.action = ACT_ATTACK;
        player.attack_idx = 0;
        player.action_frame = 1;
        player.hit_dealt = false;
        player.block_hold_frames = 0;
        return;
    }

    /* I: Right-Hand Special Attack (Heavy Spear Sweep) */
    if (platform_key_pressed(SC_I) && player.stamina >= 20) {
        player.stamina -= player_cfg.heavy_atk.stamina_cost;
        if (player.stamina < 0) player.stamina = 0;
        player.stamina_regen_cd = 40;
        player.action = ACT_ATTACK;
        player.attack_idx = 1;
        player.action_frame = 1;
        player.hit_dealt = false;
        player.block_hold_frames = 0;
        return;
    }

    /* U: Left-Hand Special (Unused on Shield per PRD) */
    if (platform_key_pressed(SC_U)) {
        set_status("[U] Left-hand special not used on Shield", ATTR(COL_LIGHTGRAY, COL_BLACK), 60);
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
            int nx = player.cx + move_dx;
            int ny = player.cy + move_dy;
            if (can_move_to(&player, nx, ny)) {
                player.cx = nx;
                player.cy = ny;
            } else if (move_dx != 0 && can_move_to(&player, player.cx + move_dx, player.cy)) {
                player.cx += move_dx;
            } else if (move_dy != 0 && can_move_to(&player, player.cx, player.cy + move_dy)) {
                player.cy += move_dy;
            }
            player.move_cd = (player.action == ACT_BLOCK) ? player_cfg.move_cd_block : player_cfg.move_cd_normal;
        }
    }
}

static void update_enemy(Entity *e)
{
    int i;
    if (!e->active || e->type_idx < 0 || e->type_idx >= num_enemy_types) return;
    const EnemyType *et = &enemy_types[e->type_idx];

    if (e->hurt_flash > 0) e->hurt_flash--;
    if (e->move_cd > 0) e->move_cd--;
    if (e->ai_think_cd > 0) e->ai_think_cd--;

    if (e->action == ACT_STAGGER) {
        e->action_frame++;
        if (e->action_frame > et->stagger_duration) {
            e->action = ACT_NONE;
            e->action_frame = 0;
        }
        return;
    }

    if (e->action == ACT_ATTACK) {
        const AttackDef *atk = &et->attacks[e->attack_idx];
        int total_frames = atk->windup_frames + atk->active_frames + atk->recovery_frames;
        e->action_frame++;
        if (e->action_frame > total_frames) {
            e->action = ACT_NONE;
            e->action_frame = 0;
            e->ai_think_cd = 25;
        }
        return;
    }

    if (!player.active) return;

    int dx = player.cx - e->cx;
    int dy = player.cy - e->cy;
    int adx = iabs(dx);
    int ady = iabs(dy);

    if (adx >= ady) {
        e->dir = (dx >= 0) ? DIR_RIGHT : DIR_LEFT;
    } else {
        e->dir = (dy >= 0) ? DIR_DOWN : DIR_UP;
    }

    int primary_dist = (adx >= ady) ? adx : ady;
    int off_axis     = (adx >= ady) ? ady : adx;

    /* Query Lua AI function if ready */
    if (e->ai_think_cd == 0) {
        const char *decision = data_lua_call_ai(et, e, primary_dist, off_axis);
        for (i = 0; i < et->num_attacks; i++) {
            if (strcmp(decision, et->attacks[i].name) == 0) {
                e->action = ACT_ATTACK;
                e->attack_idx = i;
                e->action_frame = 1;
                e->hit_dealt = false;
                return;
            }
        }
    }

    /* Approach player at enemy's configured move_cooldown */
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
            if (can_move_to(e, e->cx + step_x, e->cy + step_y)) {
                e->cx += step_x;
                e->cy += step_y;
            } else if (step_x != 0 && can_move_to(e, e->cx + step_x, e->cy)) {
                e->cx += step_x;
            } else if (step_y != 0 && can_move_to(e, e->cx, e->cy + step_y)) {
                e->cy += step_y;
            }
            e->move_cd = et->move_cooldown;
        }
    }
}

static void resolve_combat(void)
{
    int i;

    build_entity_hitboxes(&player);
    for (i = 0; i < MAX_ENEMIES; i++) {
        build_entity_hitboxes(&enemies[i]);
    }

    /* Player Spear Hitbox vs Enemies */
    if (player.active && player.num_hitbox > 0 && !player.hit_dealt) {
        const AttackDef *patk = (player.attack_idx == 1) ? &player_cfg.heavy_atk : &player_cfg.light_atk;
        for (i = 0; i < MAX_ENEMIES; i++) {
            Entity *sk = &enemies[i];
            if (!sk->active) continue;
            if (check_hitbox_vs_body(&player, sk)) {
                bool riposte = (sk->action == ACT_STAGGER);
                int dmg = riposte ? patk->riposte_damage : patk->damage;

                sk->hp -= dmg;
                sk->hurt_flash = 10;
                player.hit_dealt = true;

                int kx = sk->cx + dir_dx[player.dir];
                int ky = sk->cy + dir_dy[player.dir];
                if (can_move_to(sk, kx, ky)) {
                    sk->cx = kx;
                    sk->cy = ky;
                }

                if (patk->causes_stagger && sk->action != ACT_STAGGER) {
                    sk->action = ACT_STAGGER;
                    sk->action_frame = 55;
                }

                if (sk->hp <= 0) {
                    sk->hp = 0;
                    sk->active = false;
                    set_status("ENEMY FELLED! (Press [R] anytime to reload Lua & respawn)",
                               ATTR(COL_LIGHTGREEN, COL_BLACK), 180);
                } else if (riposte) {
                    set_status("CRITICAL RIPOSTE HIT! Massive damage on staggered enemy!",
                               ATTR(COL_YELLOW, COL_RED), 90);
                } else if (player.attack_idx == 1) {
                    set_status("Heavy Spear Sweep hits! (Staggered enemy)",
                               ATTR(COL_LIGHTMAGENTA, COL_BLACK), 75);
                } else {
                    set_status("Spear Thrust hits!",
                               ATTR(COL_LIGHTCYAN, COL_BLACK), 60);
                }
            }
        }
    }

    /* Enemy Weapon Hitboxes vs Player */
    for (i = 0; i < MAX_ENEMIES; i++) {
        Entity *sk = &enemies[i];
        if (!sk->active || sk->num_hitbox == 0 || sk->hit_dealt || !player.active) continue;
        const EnemyType *et = &enemy_types[sk->type_idx];
        const AttackDef *eatk = &et->attacks[sk->attack_idx];

        bool hit_shield = check_hitbox_vs_shield(sk, &player);
        bool hit_body   = check_hitbox_vs_body(sk, &player);

        if (hit_shield || hit_body) {
            if (player.action == ACT_ROLL && player.action_frame <= player_cfg.roll_iframes) {
                sk->hit_dealt = true;
                set_status("EVADED! Rolled through enemy attack!",
                           ATTR(COL_LIGHTGREEN, COL_BLACK), 75);
                continue;
            }

            sk->hit_dealt = true;

            if (hit_shield) {
                if (player.block_hold_frames <= player_cfg.parry_window_frames) {
                    sk->action = ACT_STAGGER;
                    sk->action_frame = 1;
                    sk->num_hitbox = 0;
                    player.stamina += 20;
                    if (player.stamina > player.max_stamina) player.stamina = player.max_stamina;
                    set_status("*** PARRY! *** Attack deflected & enemy staggered! Counterattack!",
                               ATTR(COL_YELLOW, COL_BLUE), 120);
                } else {
                    player.hp -= eatk->chip_damage;
                    player.stamina -= eatk->stamina_damage;
                    player.stamina_regen_cd = 30;
                    if (player.stamina <= 0) {
                        player.stamina = 0;
                        player.action = ACT_STAGGER;
                        player.action_frame = 1;
                        set_status("GUARD BROKEN! Out of Stamina!", ATTR(COL_WHITE, COL_RED), 90);
                    } else {
                        set_status("BLOCKED with Left Shield! (Hold [J] blocks, Tap [J] parries)",
                                   ATTR(COL_LIGHTCYAN, COL_BLUE), 75);
                    }
                }
            } else {
                player.hp -= eatk->damage;
                player.hurt_flash = 12;
                set_status("OUCH! Direct hit! Use [J] Shield or [L] Roll!",
                           ATTR(COL_WHITE, COL_RED), 90);
            }

            if (player.hp <= 0) {
                player.hp = 0;
                player.active = false;
                set_status("YOU DIED! Press [R] to Reload & Rise Again or [ESC] to Quit",
                           ATTR(COL_YELLOW, COL_RED), 600);
            }
        }
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

static void render_scene(void)
{
    int x, y, i;
    char buf[80];

    platform_clear(ATTR(COL_DARKGRAY, COL_BLACK));

    for (y = ARENA_TOP; y <= ARENA_BOTTOM; y++) {
        for (x = ARENA_LEFT; x <= ARENA_RIGHT; x++) {
            if (y == ARENA_TOP || y == ARENA_BOTTOM || x == ARENA_LEFT || x == ARENA_RIGHT) {
                platform_put_cell(x, y, '#', ATTR(COL_LIGHTGRAY, COL_DARKGRAY));
            } else if (((x + y * 3) % 11 == 0) && ((x * 5 + y) % 7 == 0)) {
                platform_put_cell(x, y, '.', ATTR(COL_DARKGRAY, COL_BLACK));
            }
        }
    }

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        draw_entity_sprite(&enemies[i]);

        int bar_y = enemies[i].cy - (entity_radius(&enemies[i]) + 2);
        if (bar_y > ARENA_TOP) {
            draw_bar(enemies[i].cx - 3, bar_y, 7, enemies[i].hp, enemies[i].max_hp,
                     ATTR(COL_WHITE, COL_RED), ATTR(COL_DARKGRAY, COL_BLACK));
        }
    }

    if (player.active) {
        draw_entity_sprite(&player);
    }

    /* HUD (Rows 0..3) */
    for (y = 0; y < ARENA_TOP; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            platform_put_cell(x, y, ' ', ATTR(COL_WHITE, COL_BLACK));
        }
    }

    snprintf(buf, sizeof(buf), "HP %3d/%3d", player.hp, player.max_hp);
    platform_put_str(1, 0, buf, ATTR(COL_LIGHTRED, COL_BLACK));
    draw_bar(12, 0, 14, player.hp, player.max_hp, ATTR(COL_WHITE, COL_RED), ATTR(COL_RED, COL_BLACK));

    snprintf(buf, sizeof(buf), "STAM %3d/%3d", player.stamina, player.max_stamina);
    platform_put_str(28, 0, buf, ATTR(COL_LIGHTGREEN, COL_BLACK));
    draw_bar(41, 0, 14, player.stamina, player.max_stamina,
             ATTR(COL_BLACK, COL_LIGHTGREEN), ATTR(COL_GREEN, COL_BLACK));

    const char *stance = "READY";
    uint8_t stance_attr = ATTR(COL_LIGHTGRAY, COL_BLACK);
    if (!player.active) {
        stance = "DEAD [R=Retry]";
        stance_attr = ATTR(COL_WHITE, COL_RED);
    } else if (player.action == ACT_BLOCK) {
        if (player.block_hold_frames <= player_cfg.parry_window_frames) {
            stance = "*PARRY WINDOW*";
            stance_attr = ATTR(COL_YELLOW, COL_CYAN);
        } else {
            stance = "[SHIELD BLOCK]";
            stance_attr = ATTR(COL_LIGHTCYAN, COL_BLUE);
        }
    } else if (player.action == ACT_ATTACK) {
        stance = (player.attack_idx == 1) ? "HEAVY SWEEP!" : "SPEAR THRUST!";
        stance_attr = (player.attack_idx == 1) ? ATTR(COL_WHITE, COL_MAGENTA) : ATTR(COL_YELLOW, COL_RED);
    } else if (player.action == ACT_ROLL) {
        stance = "DODGE ROLL";
        stance_attr = ATTR(COL_LIGHTGREEN, COL_BLUE);
    } else if (player.action == ACT_STAGGER) {
        stance = "STAGGERED!";
        stance_attr = ATTR(COL_YELLOW, COL_RED);
    }
    platform_put_str(57, 0, "STATE:", ATTR(COL_LIGHTGRAY, COL_BLACK));
    platform_put_str(64, 0, stance, stance_attr);

    for (i = 0; i < 2; i++) {
        int col = (i == 0) ? 1 : 40;
        const char *ename = (enemies[i].type_idx >= 0 && enemies[i].type_idx < num_enemy_types)
            ? enemy_types[enemies[i].type_idx].name : "Enemy";
        if (!enemies[i].active) {
            snprintf(buf, sizeof(buf), "%s #%d: DEFEATED", ename, i + 1);
            platform_put_str(col, 1, buf, ATTR(COL_DARKGRAY, COL_BLACK));
        } else {
            const EnemyType *et = &enemy_types[enemies[i].type_idx];
            const char *estate = "Patrol";
            uint8_t eattr = ATTR(COL_LIGHTGRAY, COL_BLACK);
            if (enemies[i].action == ACT_STAGGER) {
                estate = "STAGGERED!";
                eattr = ATTR(COL_YELLOW, COL_BLUE);
            } else if (enemies[i].action == ACT_ATTACK) {
                const AttackDef *atk = &et->attacks[enemies[i].attack_idx];
                bool winding = (enemies[i].action_frame <= atk->windup_frames);
                estate = winding ? "WINDUP!" : atk->name;
                eattr = winding ? ATTR(COL_YELLOW, COL_BROWN) : ATTR(COL_WHITE, COL_RED);
            }
            snprintf(buf, sizeof(buf), "%s#%d HP:%2d/%2d [%s]",
                     ename, i + 1, enemies[i].hp, enemies[i].max_hp, estate);
            platform_put_str(col, 1, buf, eattr);
        }
    }

    platform_put_str(1, 2, status_msg, status_attr);
    platform_put_str(1, 3,
        "WASD:Move/Face  J:Shield(Tap=Parry)  K:FastSpear  I:HeavySpear  L:Roll  R:Reload",
        ATTR(COL_LIGHTCYAN, COL_BLACK));

    platform_present();
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
        resolve_combat();

        render_scene();
        platform_wait_frame();
    }

    data_lua_shutdown();
    platform_shutdown();
    return 0;
}
