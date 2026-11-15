#include "game.h"
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lua_State *L = NULL;
static char last_lua_error[128] = "";

static int lua_alert_handler(lua_State *state)
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

static uint8_t parse_tile_name(const char *s, uint8_t def_tile)
{
    if (!s || !s[0]) return def_tile;
    if (strcmp(s, "WALL") == 0)         return TILE_WALL;
    if (strcmp(s, "FLOOR_DOT") == 0)    return TILE_FLOOR_DOT;
    if (strcmp(s, "BLOCK_FULL") == 0)   return TILE_BLOCK_FULL;
    if (strcmp(s, "BLOCK_TOP") == 0)    return TILE_BLOCK_TOP;
    if (strcmp(s, "BLOCK_BOT") == 0)    return TILE_BLOCK_BOT;
    if (strcmp(s, "KNIGHT_HELM") == 0)  return TILE_KNIGHT_HELM;
    if (strcmp(s, "KNIGHT_ARMOR") == 0) return TILE_KNIGHT_ARMOR;
    if (strcmp(s, "KNIGHT_BOOT") == 0)  return TILE_KNIGHT_BOOT;
    if (strcmp(s, "SHIELD_MID") == 0)   return TILE_SHIELD_MID;
    if (strcmp(s, "SHIELD_EDGE") == 0)  return TILE_SHIELD_EDGE;
    if (strcmp(s, "SPEAR_TIP_U") == 0)  return TILE_SPEAR_TIP_U;
    if (strcmp(s, "SPEAR_TIP_R") == 0)  return TILE_SPEAR_TIP_R;
    if (strcmp(s, "SPEAR_TIP_D") == 0)  return TILE_SPEAR_TIP_D;
    if (strcmp(s, "SPEAR_TIP_L") == 0)  return TILE_SPEAR_TIP_L;
    if (strcmp(s, "SHAFT_V") == 0)      return TILE_SHAFT_V;
    if (strcmp(s, "SHAFT_H") == 0)      return TILE_SHAFT_H;
    if (strcmp(s, "SKEL_SKULL") == 0)   return TILE_SKEL_SKULL;
    if (strcmp(s, "SKEL_RIBS") == 0)    return TILE_SKEL_RIBS;
    if (strcmp(s, "SKEL_PELVIS") == 0)  return TILE_SKEL_PELVIS;
    if (strcmp(s, "SKEL_LIMB") == 0)    return TILE_SKEL_LIMB;
    if (strcmp(s, "SWORD_TIP_U") == 0)  return TILE_SWORD_TIP_U;
    if (strcmp(s, "SWORD_TIP_R") == 0)  return TILE_SWORD_TIP_R;
    if (strcmp(s, "SWORD_TIP_D") == 0)  return TILE_SWORD_TIP_D;
    if (strcmp(s, "SWORD_TIP_L") == 0)  return TILE_SWORD_TIP_L;
    if (strcmp(s, "SWORD_HILT") == 0)   return TILE_SWORD_HILT;
    if (strcmp(s, "SLASH_ARC") == 0)    return TILE_SLASH_ARC;
    if (strcmp(s, "PARRY_STAR") == 0)   return TILE_PARRY_STAR;
    if (strcmp(s, "CRIT_SPARK") == 0)   return TILE_CRIT_SPARK;
    if (strcmp(s, "BONE_SHARD") == 0)   return TILE_BONE_SHARD;
    return def_tile;
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

static uint8_t parse_glyph_fields(lua_State *state, int tbl_idx, const char *tile_key, const char *ch_key, uint8_t def_ch)
{
    char tile_str[32], ch_str[8];
    get_str_field(state, tbl_idx, tile_key, tile_str, sizeof(tile_str), "");
    if (tile_str[0]) {
        return parse_tile_name(tile_str, def_ch);
    }
    get_str_field(state, tbl_idx, ch_key, ch_str, sizeof(ch_str), "");
    if (ch_str[0]) {
        return (uint8_t)ch_str[0];
    }
    return def_ch;
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
                char fg_str[24], bg_str[24];
                int fwd = get_int_field(state, c_idx, "fwd", 0);
                int right = get_int_field(state, c_idx, "right", 0);
                uint8_t glyph = parse_glyph_fields(state, c_idx, "tile", "ch", '*');
                get_str_field(state, c_idx, "fg", fg_str, sizeof(fg_str), "WHITE");
                get_str_field(state, c_idx, "bg", bg_str, sizeof(bg_str), "BLACK");

                out_cells[count].fwd = (int8_t)fwd;
                out_cells[count].right = (int8_t)right;
                out_cells[count].ch = glyph;
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

static void parse_attack_def(lua_State *state, int atk_idx, AttackDef *atk, bool is_crit)
{
    int i, f, r;
    memset(atk, 0, sizeof(*atk));
    atk->valid           = true;
    atk->is_critical     = is_crit;
    get_str_field(state, atk_idx, "name", atk->name, sizeof(atk->name), is_crit ? "CRITICAL" : "ATTACK");
    atk->windup_frames   = get_int_field(state, atk_idx, "windup_frames", 12);
    atk->recovery_frames = get_int_field(state, atk_idx, "recovery_frames", 12);
    atk->damage          = get_int_field(state, atk_idx, "damage", 15);
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

                /* Concise rectangular hitbox regions ("rect") */
                lua_pushstring(state, "rect");
                lua_gettable(state, p_idx);
                if (lua_istable(state, -1)) {
                    int r_idx = lua_gettop(state);
                    int f1 = get_int_field(state, r_idx, "fwd1", 2);
                    int f2 = get_int_field(state, r_idx, "fwd2", 4);
                    int r1 = get_int_field(state, r_idx, "right1", -1);
                    int r2 = get_int_field(state, r_idx, "right2", 1);
                    uint8_t base_ch = parse_glyph_fields(state, r_idx, "tile", "ch", '*');
                    uint8_t tip_ch  = parse_glyph_fields(state, r_idx, "tip_tile", "tip_ch", base_ch);
                    char fg_s[24], bg_s[24], tip_fg_s[24], tip_bg_s[24];
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
                                ph->cells[ph->num_cells].ch = is_tip ? tip_ch : base_ch;
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

/* Lua: register_weapon("spear", { ... }) */
static int l_register_weapon(lua_State *state)
{
    const char *id = luaL_check_string(state, 1);
    if (!lua_istable(state, 2)) return 0;
    int t = 2;

    WeaponDef *w = weapon_register(id);
    if (!w) return 0;

    char kind_s[24], hand_s[16];
    get_str_field(state, t, "name", w->name, sizeof(w->name), id);
    get_str_field(state, t, "kind", kind_s, sizeof(kind_s), "MELEE");
    get_str_field(state, t, "hand", hand_s, sizeof(hand_s), "RIGHT");

    w->kind = (strcmp(kind_s, "SHIELD") == 0) ? WEAPON_SHIELD : WEAPON_MELEE;
    w->authored_left = (strcmp(hand_s, "LEFT") == 0);
    w->block_move_cd = get_int_field(state, t, "block_move_cd", 5);
    w->parry_window_frames = get_int_field(state, t, "parry_window_frames", 11);

    w->num_idle_cells = parse_cell_list(state, t, "idle_pose", w->idle_cells, MAX_POSE_CELLS);

    lua_pushstring(state, "normal_attack");
    lua_gettable(state, t);
    if (lua_istable(state, -1)) {
        parse_attack_def(state, lua_gettop(state), &w->attacks[WATK_NORMAL], false);
    }
    lua_pop(state, 1);

    lua_pushstring(state, "special_attack");
    lua_gettable(state, t);
    if (lua_istable(state, -1)) {
        parse_attack_def(state, lua_gettop(state), &w->attacks[WATK_SPECIAL], false);
    }
    lua_pop(state, 1);

    lua_pushstring(state, "crit_attack");
    lua_gettable(state, t);
    if (lua_istable(state, -1)) {
        parse_attack_def(state, lua_gettop(state), &w->attacks[WATK_CRIT], true);
    }
    lua_pop(state, 1);

    return 0;
}

static void parse_character_def_common(lua_State *state, int t, CharacterDef *def, const char *id, const char *def_name)
{
    char crit_fg[24], crit_bg[24];
    strncpy(def->id, id, sizeof(def->id) - 1);
    def->id[sizeof(def->id) - 1] = '\0';
    get_str_field(state, t, "name", def->name, sizeof(def->name), def_name);
    def->radius             = get_int_field(state, t, "radius", 2);
    def->max_hp             = get_int_field(state, t, "max_hp", 100);
    def->max_stamina        = get_int_field(state, t, "max_stamina", 100);
    def->move_cooldown      = get_int_field(state, t, "move_cooldown", 5);
    def->stagger_duration   = get_int_field(state, t, "stagger_duration", 32);
    def->crit_hurt_duration = get_int_field(state, t, "crit_hurt_duration", 45);

    get_str_field(state, t, "left_weapon",  def->default_left_weapon,  sizeof(def->default_left_weapon),  "");
    get_str_field(state, t, "right_weapon", def->default_right_weapon, sizeof(def->default_right_weapon), "");

    get_str_field(state, t, "crit_flash_fg", crit_fg, sizeof(crit_fg), "YELLOW");
    get_str_field(state, t, "crit_flash_bg", crit_bg, sizeof(crit_bg), "RED");
    def->crit_flash_attr = ATTR(parse_color_name(crit_fg, COL_YELLOW), parse_color_name(crit_bg, COL_RED));

    def->num_body_cells      = parse_cell_list(state, t, "body", def->body_cells, MAX_SPRITE_CELLS);
    def->num_stagger_cells   = parse_cell_list(state, t, "stagger_pose", def->stagger_cells, MAX_POSE_CELLS);
    def->num_crit_hurt_cells = parse_cell_list(state, t, "crit_hurt_pose", def->crit_hurt_cells, MAX_POSE_CELLS);
}

/* Lua: configure_player({ ... }) */
static int l_configure_player(lua_State *state)
{
    if (!lua_istable(state, 1)) return 0;
    int t = 1;

    memset(&player_def, 0, sizeof(player_def));
    parse_character_def_common(state, t, &player_def.base, "player", "Knight");

    player_def.base.move_cooldown = get_int_field(state, t, "move_cd_normal", player_def.base.move_cooldown);
    player_def.roll_frames        = get_int_field(state, t, "roll_frames", 14);
    player_def.roll_iframes       = get_int_field(state, t, "roll_iframes", 10);
    player_def.roll_stamina_cost  = get_int_field(state, t, "roll_stamina_cost", 22);
    get_str_field(state, t, "alt_right_weapon", player_def.alt_right_weapon, sizeof(player_def.alt_right_weapon), "");

    return 0;
}

/* Lua: register_enemy("skeleton", { ... }) */
static int l_register_enemy(lua_State *state)
{
    const char *id = luaL_check_string(state, 1);
    if (!lua_istable(state, 2) || num_enemy_defs >= MAX_ENEMY_TYPES) return 0;

    int t = 2;
    EnemyDef *ed = &enemy_defs[num_enemy_defs++];
    memset(ed, 0, sizeof(*ed));
    ed->used = true;

    parse_character_def_common(state, t, &ed->base, id, id);
    get_str_field(state, t, "ai_func", ed->ai_func, sizeof(ed->ai_func), "");

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
    lua_register(L, "register_weapon", l_register_weapon);
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
    weapon_registry_clear();
    num_enemy_defs = 0;
    memset(enemy_defs, 0, sizeof(enemy_defs));
    memset(&player_def, 0, sizeof(player_def));

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

const char *data_lua_call_ai(const EnemyDef *edef, const Character *c, int primary_dist, int off_axis)
{
    static char decision[32];
    decision[0] = '\0';

    if (!L || !edef || !edef->ai_func[0]) return "approach";

    lua_getglobal(L, edef->ai_func);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return "approach";
    }

    lua_pushnumber(L, c->hp);
    lua_pushnumber(L, c->max_hp);
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
