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

static Direction parse_dir_name(const char *s)
{
    if (!s) return DIR_RIGHT;
    if (strcmp(s, "UP") == 0)    return DIR_UP;
    if (strcmp(s, "RIGHT") == 0) return DIR_RIGHT;
    if (strcmp(s, "DOWN") == 0)  return DIR_DOWN;
    if (strcmp(s, "LEFT") == 0)  return DIR_LEFT;
    return DIR_RIGHT;
}

static WeaponStyle parse_weapon_style(const char *s)
{
    if (!s) return WSTYLE_SWORD;
    if (strcmp(s, "SPEAR") == 0)      return WSTYLE_SPEAR;
    if (strcmp(s, "GREATSWORD") == 0) return WSTYLE_GREATSWORD;
    if (strcmp(s, "SHIELD") == 0)     return WSTYLE_SHIELD;
    return WSTYLE_SWORD;
}

static AttackAnimType parse_anim_type(const char *s)
{
    if (!s) return ANIM_THRUST;
    if (strcmp(s, "SLASH") == 0)  return ANIM_SLASH;
    if (strcmp(s, "CLEAVE") == 0) return ANIM_CLEAVE;
    if (strcmp(s, "IMPALE") == 0) return ANIM_IMPALE;
    return ANIM_THRUST;
}

static BodyStyle parse_body_style(const char *s)
{
    if (!s) return BSTYLE_SKELETON;
    if (strcmp(s, "KNIGHT") == 0)        return BSTYLE_KNIGHT;
    if (strcmp(s, "SKELETON_LORD") == 0) return BSTYLE_SKELETON_LORD;
    return BSTYLE_SKELETON;
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

static void parse_attack_def(lua_State *state, int atk_idx, AttackDef *atk, bool is_crit)
{
    char anim_s[24];
    memset(atk, 0, sizeof(*atk));
    atk->valid           = true;
    atk->is_critical     = is_crit;
    get_str_field(state, atk_idx, "name", atk->name, sizeof(atk->name), is_crit ? "CRITICAL" : "ATTACK");
    get_str_field(state, atk_idx, "anim", anim_s, sizeof(anim_s), "THRUST");
    atk->anim_type       = parse_anim_type(anim_s);

    atk->windup_frames   = get_int_field(state, atk_idx, "windup_frames", 18);
    atk->active_frames   = get_int_field(state, atk_idx, "active_frames", 12);
    atk->recovery_frames = get_int_field(state, atk_idx, "recovery_frames", 16);
    atk->reach_px        = get_int_field(state, atk_idx, "reach_px", 24);
    atk->width_px        = get_int_field(state, atk_idx, "width_px", 8);

    atk->damage          = get_int_field(state, atk_idx, "damage", 18);
    atk->chip_damage     = get_int_field(state, atk_idx, "chip_damage", 3);
    atk->stamina_cost    = get_int_field(state, atk_idx, "stamina_cost", 18);
    atk->stamina_damage  = get_int_field(state, atk_idx, "stamina_damage", 24);
    atk->causes_stagger  = (get_int_field(state, atk_idx, "causes_stagger", 0) != 0);
}

/* Lua: register_weapon("spear", { ... }) */
static int l_register_weapon(lua_State *state)
{
    const char *id = luaL_check_string(state, 1);
    if (!lua_istable(state, 2)) return 0;
    int t = 2;

    WeaponDef *w = weapon_register(id);
    if (!w) return 0;

    char kind_s[24], style_s[24];
    get_str_field(state, t, "name", w->name, sizeof(w->name), id);
    get_str_field(state, t, "kind", kind_s, sizeof(kind_s), "MELEE");
    get_str_field(state, t, "style", style_s, sizeof(style_s), "SWORD");

    w->kind = (strcmp(kind_s, "SHIELD") == 0) ? WEAPON_SHIELD : WEAPON_MELEE;
    w->style = parse_weapon_style(style_s);
    w->length_px = get_int_field(state, t, "length_px", 18);
    w->blade_width_px = get_int_field(state, t, "blade_width_px", 3);

    w->block_speed_fp = get_int_field(state, t, "block_speed_fp", 8);
    w->parry_window_frames = get_int_field(state, t, "parry_window_frames", 12);

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
    char bstyle_s[32];
    strncpy(def->id, id, sizeof(def->id) - 1);
    def->id[sizeof(def->id) - 1] = '\0';
    get_str_field(state, t, "name", def->name, sizeof(def->name), def_name);
    get_str_field(state, t, "body_style", bstyle_s, sizeof(bstyle_s), "SKELETON");
    def->body_style         = parse_body_style(bstyle_s);
    def->scale_pct          = get_int_field(state, t, "scale_pct", 100);
    def->radius             = get_int_field(state, t, "radius", (8 * def->scale_pct) / 100);
    def->max_hp             = get_int_field(state, t, "max_hp", 100);
    def->max_stamina        = get_int_field(state, t, "max_stamina", 100);
    def->move_speed_fp      = get_int_field(state, t, "move_speed_fp", 12);
    def->turn_cooldown      = get_int_field(state, t, "turn_cooldown", 20);
    def->stagger_duration   = get_int_field(state, t, "stagger_duration", 45);
    def->crit_hurt_duration = get_int_field(state, t, "crit_hurt_duration", 50);

    get_str_field(state, t, "left_weapon",  def->default_left_weapon,  sizeof(def->default_left_weapon),  "");
    get_str_field(state, t, "right_weapon", def->default_right_weapon, sizeof(def->default_right_weapon), "");
}

/* Lua: configure_player({ ... }) */
static int l_configure_player(lua_State *state)
{
    if (!lua_istable(state, 1)) return 0;
    int t = 1;

    memset(&player_def, 0, sizeof(player_def));
    parse_character_def_common(state, t, &player_def.base, "player", "Knight");

    player_def.roll_frames       = get_int_field(state, t, "roll_frames", 18);
    player_def.roll_iframes      = get_int_field(state, t, "roll_iframes", 12);
    player_def.roll_speed_fp     = get_int_field(state, t, "roll_speed_fp", 32);
    player_def.roll_stamina_cost = get_int_field(state, t, "roll_stamina_cost", 22);
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
