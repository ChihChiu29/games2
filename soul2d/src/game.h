#ifndef SOUL2D_GAME_H
#define SOUL2D_GAME_H

#include "platform.h"
#include "weapon.h"
#include "character.h"
#include "combat.h"
#include "render.h"

/* Lua Data & AI interface (src/data_lua.c) */
void data_lua_init(void);
void data_lua_shutdown(void);
bool data_lua_reload(char *err_buf, int err_buf_len);
const char *data_lua_call_ai(const EnemyDef *edef, const Character *c, int primary_dist, int off_axis);

/* Status & Spawn helpers (src/main.c) */
void game_set_status(const char *msg, uint8_t attr, int frames);
void game_spawn_player(int x, int y, Direction dir);
void game_spawn_enemy(const char *type_id, int x, int y, Direction dir);

#endif /* SOUL2D_GAME_H */
