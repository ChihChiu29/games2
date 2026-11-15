#include "game.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lua_State *L = NULL;
static char last_lua_error[128] = "";

static int lua_alert_handler(lua_State * state)
{
    const char *msg = lua_tostring(state, 1);
    if (msg) {
        strncpy(last_lua_error, msg, sizeof(last_lua_error) - 1);
        last_lua_error[sizeof(last_lua_error) - 1] = '\0';
    }
    return 0;
}

static int l_irand(lua_State *state)
{
    int n = (int)luaL_check_number(state, 1);
    if (n <= 1) {
        lua_pushnumber(state, 1);
    } else {
        lua_pushnumber(state, (rand() % n) + 1);
    }
    return 1;
}

static uint8_t parse_color_name(const char *s, uint8_t def_col)
{
    if (!s || !s[0]) return def_col;
    if (strcmp(s, "BLACK") == 0)        return COL_BLACK;
    if (strcmp(s, "BLUE") == 0)         return COL_BLUE;
    if (strcmp(s, "GREEN") == 0)        return COL_GREEN;
    if (strcmp(s, "CYAN") == 0)         return COL_CYAN;
    if (strcmp(s, "RED") == 0)          return COL_RED;
    if (strcmp(s, "MAGENTA") == 0)      return COL_MAGENTA;
    if (strcmp(s, "BROWN") == 0)        return COL_BROWN;
    if (strcmp(s, "LIGHTGRAY") == 0)    return COL_LIGHTGRAY;
    if (strcmp(s, "DARKGRAY") == 0)     return COL_DARKGRAY;
    if (strcmp(s, "LIGHTBLUE") == 0)    return COL_LIGHTBLUE;
    if (strcmp(s, "LIGHTGREEN") == 0)   return COL_LIGHTGREEN;
    if (strcmp(s, "LIGHTCYAN") == 0)    return COL_LIGHTCYAN;
    if (strcmp(s, "LIGHTRED") == 0)     return COL_LIGHTRED;
    if (strcmp(s, "LIGHTMAGENTA") == 0) return COL_LIGHTMAGENTA;
    if (strcmp(s, "YELLOW") == 0)       return COL_YELLOW;
    if (strcmp(s, "WHITE") == 0)        return COL_WHITE;
    return def_col;
}

static Direction parse_dir_name(const char *s)
{
    if (!s) return DIR_RIGHT;
    if (strcmp(s, "UP") == 0)    return DIR_UP;
    if (strcmp(s, "RIGHT") == 0) return DIR_RIGHT;
    if (strcmp(s, "DOWN") == 0)  return DIR_DOWN;
    if (strcmp(s, "LEFT") == 0)  return DIR_LEFT;
    return DIR_RIGHT;
}

static int get_int_field(lua_State *state, int tbl_idx, const char *key, int def_val)
{
    int val = def_val;
    lua_pushstring(state, key);
    lua_gettable(state, tbl_idx);
    if (lua_isnumber(state, -1)) {
        val = (int)lua_tonumber(state, -1);
    }
    lua_pop(state, 1);
    return val;
}

static void get_str_field(lua_State *state, int tbl_idx, const char *key, char *out, int max_len, const char *def_val)
{
    lua_pushstring(state, key);
    lua_gettable(state, tbl_idx);
    if (lua_isstring(state, -1)) {
        strncpy(out, lua_tostring(state, -1), max_len - 1);
        out[max_len - 1] = '\0';
    } else if (def_val) {
        strncpy(out, def_val, max_len - 1);
        out[max_len - 1] = '\0';
    } else {
        out[0] = '\0';
    }
    lua_pop(state, 1);
}

static int parse_cell_list(lua_State *state, int tbl_idx, const char *key, SpriteCell *out_cells, int max_cells)
{
    int count = 0;
    lua_pushstring(state, key);
    lua_gettable(state, tbl_idx);
    if (lua_istable(state, -1)) {
        int list_idx = lua_gettop(state);
        int n = lua_getn(state, list_idx);
        int i;
        for (i = 1; i <= n && count < max_cells; i++) {
            lua_rawgeti(state, list_idx, i);
            if (lua_istable(state, -1)) {
                int c_idx = lua_gettop(state);
                char ch_str[8], fg_str[24], bg_str[24];
                int fwd = get_int_field(state, c_idx, "fwd", 0);
                int right = get_int_field(state, c_idx, "right", 0);
                get_str_field(state, c_idx, "ch", ch_str, sizeof(ch_str), "*");
                get_str_field(state, c_idx, "fg", fg_str, sizeof(fg_str), "WHITE");
                get_str_field(state, c_idx, "bg", bg_str, sizeof(bg_str), "BLACK");

                out_cells[count].fwd = (int8_t)fwd;
                out_cells[count].right = (int8_t)right;
                out_cells[count].ch = ch_str[0] ? ch_str[0] : ' ';
                out_cells[count].attr = ATTR(
                    parse_color_name(fg_str, COL_WHITE),
                    parse_color_name(bg_str, COL_BLACK)
                );
                count++;
            }
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 1);
    return count;
}

static void parse_attack_def(lua_State *state, int atk_idx, AttackDef *atk)
{
    int i, f, r;
    memset(atk, 0, sizeof(*atk));
    get_str_field(state, atk_idx, "name", atk->name, sizeof(atk->name), "attack");
    atk->windup_frames   = get_int_field(state, atk_idx, "windup_frames", 12);
    atk->recovery_frames = get_int_field(state, atk_idx, "recovery_frames", 12);
    atk->damage          = get_int_field(state, atk_idx, "damage", 15);
    atk->riposte_damage  = get_int_field(state, atk_idx, "riposte_damage", atk->damage * 2);
    atk->chip_damage     = get_int_field(state, atk_idx, "chip_damage", 3);
    atk->stamina_cost    = get_int_field(state, atk_idx, "stamina_cost", 18);
    atk->stamina_damage  = get_int_field(state, atk_idx, "stamina_damage", 24);
    atk->causes_stagger  = (get_int_field(state, atk_idx, "causes_stagger", 0) != 0);

    atk->num_windup_cells   = parse_cell_list(state, atk_idx, "windup_pose", atk->windup_cells, MAX_POSE_CELLS);
    atk->num_recovery_cells = parse_cell_list(state, atk_idx, "recovery_pose", atk->recovery_cells, MAX_POSE_CELLS);

    atk->active_frames = 0;
    atk->num_phases = 0;

    lua_pushstring(state, "phases");
    lua_gettable(state, atk_idx);
    if (lua_istable(state, -1)) {
        int plist_idx = lua_gettop(state);
        int n = lua_getn(state, plist_idx);
        for (i = 1; i <= n && atk->num_phases < MAX_ATTACK_PHASES; i++) {
            lua_rawgeti(state, plist_idx, i);
            if (lua_istable(state, -1)) {
                int p_idx = lua_gettop(state);
                AttackPhase *ph = &atk->phases[atk->num_phases];
                ph->duration_frames = get_int_field(state, p_idx, "frames", 6);
                ph->num_cells = parse_cell_list(state, p_idx, "cells", ph->cells, MAX_HITBOX_CELLS);

                /* Also support concise rectangular hitbox regions ("rect") */
                lua_pushstring(state, "rect");
                lua_gettable(state, p_idx);
                if (lua_istable(state, -1)) {
                    int r_idx = lua_gettop(state);
                    int f1 = get_int_field(state, r_idx, "fwd1", 2);
                    int f2 = get_int_field(state, r_idx, "fwd2", 4);
                    int r1 = get_int_field(state, r_idx, "right1", -1);
                    int r2 = get_int_field(state, r_idx, "right2", 1);
                    char ch_s[8], tip_s[8], fg_s[24], bg_s[24], tip_fg_s[24], tip_bg_s[24];
                    get_str_field(state, r_idx, "ch", ch_s, sizeof(ch_s), "*");
                    get_str_field(state, r_idx, "tip_ch", tip_s, sizeof(tip_s), ch_s);
                    get_str_field(state, r_idx, "fg", fg_s, sizeof(fg_s), "YELLOW");
                    get_str_field(state, r_idx, "bg", bg_s, sizeof(bg_s), "RED");
                    get_str_field(state, r_idx, "tip_fg", tip_fg_s, sizeof(tip_fg_s), "YELLOW");
                    get_str_field(state, r_idx, "tip_bg", tip_bg_s, sizeof(tip_bg_s), "RED");

                    uint8_t base_at = ATTR(parse_color_name(fg_s, COL_YELLOW), parse_color_name(bg_s, COL_RED));
                    uint8_t tip_at  = ATTR(parse_color_name(tip_fg_s, COL_YELLOW), parse_color_name(tip_bg_s, COL_RED));

                    for (f = f1; f <= f2; f++) {
                        for (r = r1; r <= r2; r++) {
                            if (ph->num_cells < MAX_HITBOX_CELLS) {
                                bool is_tip = (f == f2);
                                ph->cells[ph->num_cells].fwd = (int8_t)f;
                                ph->cells[ph->num_cells].right = (int8_t)r;
                                ph->cells[ph->num_cells].ch = is_tip ? tip_s[0] : ch_s[0];
                                ph->cells[ph->num_cells].attr = is_tip ? tip_at : base_at;
                                ph->num_cells++;
                            }
                        }
                    }
                }
                lua_pop(state, 1);

                atk->active_frames += ph->duration_frames;
                atk->num_phases++;
            }
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 1);
}

/* Lua: configure_player({ ... }) */
static int l_configure_player(lua_State *state)
{
    if (!lua_istable(state, 1)) return 0;
    int t = 1;

    player_cfg.radius              = get_int_field(state, t, "radius", 2);
    player_cfg.max_hp              = get_int_field(state, t, "max_hp", 100);
    player_cfg.max_stamina         = get_int_field(state, t, "max_stamina", 100);
    player_cfg.move_cd_normal      = get_int_field(state, t, "move_cd_normal", 3);
    player_cfg.move_cd_block       = get_int_field(state, t, "move_cd_block", 5);
    player_cfg.parry_window_frames = get_int_field(state, t, "parry_window_frames", 11);
    player_cfg.roll_frames         = get_int_field(state, t, "roll_frames", 14);
    player_cfg.roll_iframes        = get_int_field(state, t, "roll_iframes", 10);
    player_cfg.roll_stamina_cost   = get_int_field(state, t, "roll_stamina_cost", 22);

    player_cfg.num_body_cells  = parse_cell_list(state, t, "body", player_cfg.body_cells, MAX_SPRITE_CELLS);
    player_cfg.num_idle_shield = parse_cell_list(state, t, "idle_shield", player_cfg.idle_shield, MAX_POSE_CELLS);
    player_cfg.num_idle_weapon = parse_cell_list(state, t, "idle_weapon", player_cfg.idle_weapon, MAX_POSE_CELLS);

    lua_pushstring(state, "light_attack");
    lua_gettable(state, t);
    if (lua_istable(state, -1)) {
        parse_attack_def(state, lua_gettop(state), &player_cfg.light_atk);
    }
    lua_pop(state, 1);

    lua_pushstring(state, "heavy_attack");
    lua_gettable(state, t);
    if (lua_istable(state, -1)) {
        parse_attack_def(state, lua_gettop(state), &player_cfg.heavy_atk);
    }
    lua_pop(state, 1);

    return 0;
}

/* Lua: register_enemy("skeleton", { ... }) */
static int l_register_enemy(lua_State *state)
{
    int i;
    const char *id = luaL_check_string(state, 1);
    if (!lua_istable(state, 2) || num_enemy_types >= MAX_ENEMY_TYPES) return 0;

    int t = 2;
    EnemyType *et = &enemy_types[num_enemy_types++];
    memset(et, 0, sizeof(*et));
    et->used = true;
    strncpy(et->id, id, sizeof(et->id) - 1);

    get_str_field(state, t, "name", et->name, sizeof(et->name), id);
    et->radius           = get_int_field(state, t, "radius", 2);
    et->max_hp           = get_int_field(state, t, "max_hp", 90);
    et->move_cooldown    = get_int_field(state, t, "move_cooldown", 8);
    et->stagger_duration = get_int_field(state, t, "stagger_duration", 95);
    get_str_field(state, t, "ai_func", et->ai_func, sizeof(et->ai_func), "");

    et->num_body_cells    = parse_cell_list(state, t, "body", et->body_cells, MAX_SPRITE_CELLS);
    et->num_idle_weapon   = parse_cell_list(state, t, "idle_weapon", et->idle_weapon, MAX_POSE_CELLS);
    et->num_stagger_cells = parse_cell_list(state, t, "stagger_pose", et->stagger_cells, MAX_POSE_CELLS);

    lua_pushstring(state, "attacks");
    lua_gettable(state, t);
    if (lua_istable(state, -1)) {
        int alist = lua_gettop(state);
        int n = lua_getn(state, alist);
        for (i = 1; i <= n && et->num_attacks < MAX_ATTACK_DEFS; i++) {
            lua_rawgeti(state, alist, i);
            if (lua_istable(state, -1)) {
                parse_attack_def(state, lua_gettop(state), &et->attacks[et->num_attacks]);
                et->num_attacks++;
            }
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 1);

    return 0;
}

/* Lua: spawn_player(x, y, "RIGHT") */
static int l_spawn_player(lua_State *state)
{
    int x = (int)luaL_check_number(state, 1);
    int y = (int)luaL_check_number(state, 2);
    const char *dir_s = luaL_opt_string(state, 3, "RIGHT");
    game_spawn_player(x, y, parse_dir_name(dir_s));
    return 0;
}

/* Lua: spawn_enemy("skeleton", x, y, "LEFT") */
static int l_spawn_enemy(lua_State *state)
{
    const char *type_id = luaL_check_string(state, 1);
    int x = (int)luaL_check_number(state, 2);
    int y = (int)luaL_check_number(state, 3);
    const char *dir_s = luaL_opt_string(state, 4, "LEFT");
    game_spawn_enemy(type_id, x, y, parse_dir_name(dir_s));
    return 0;
}

void data_lua_init(void)
{
    if (L) lua_close(L);
    L = lua_open(0);
    lua_baselibopen(L);
    lua_strlibopen(L);
    lua_iolibopen(L);

    lua_register(L, "_ALERT", lua_alert_handler);
    lua_register(L, "irand", l_irand);
    lua_register(L, "configure_player", l_configure_player);
    lua_register(L, "register_enemy", l_register_enemy);
    lua_register(L, "spawn_player", l_spawn_player);
    lua_register(L, "spawn_enemy", l_spawn_enemy);
}

void data_lua_shutdown(void)
{
    if (L) {
        lua_close(L);
        L = NULL;
    }
}

bool data_lua_reload(char *err_buf, int err_buf_len)
{
    last_lua_error[0] = '\0';
    num_enemy_types = 0;
    memset(enemy_types, 0, sizeof(enemy_types));
    memset(&player_cfg, 0, sizeof(player_cfg));

    if (!L) data_lua_init();

    int status = lua_dofile(L, "data/init.lua");
    if (status != 0) {
        if (err_buf && err_buf_len > 0) {
            snprintf(err_buf, err_buf_len, "Lua Error: %s",
                     last_lua_error[0] ? last_lua_error : "Failed to load data/init.lua");
        }
        return false;
    }
    return true;
}

const char *data_lua_call_ai(const EnemyType *et, const Entity *e, int primary_dist, int off_axis)
{
    static char decision[32];
    decision[0] = '\0';

    if (!L || !et || !et->ai_func[0]) return "approach";

    lua_getglobal(L, et->ai_func);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return "approach";
    }

    lua_pushnumber(L, e->hp);
    lua_pushnumber(L, e->max_hp);
    lua_pushnumber(L, primary_dist);
    lua_pushnumber(L, off_axis);

    if (lua_call(L, 4, 1) == 0) {
        if (lua_isstring(L, -1)) {
            strncpy(decision, lua_tostring(L, -1), sizeof(decision) - 1);
            decision[sizeof(decision) - 1] = '\0';
        }
        lua_pop(L, 1);
    }

    return decision[0] ? decision : "approach";
}
