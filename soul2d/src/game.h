#ifndef SOUL2D_GAME_H
#define SOUL2D_GAME_H

#include "platform.h"

/* Playfield bounds (Rows 0..3 are HUD, Rows 4..49 are Arena) */
#define ARENA_TOP    4
#define ARENA_BOTTOM 49
#define ARENA_LEFT   0
#define ARENA_RIGHT  79

#define MAX_HITBOX_CELLS  48
#define MAX_SPRITE_CELLS  64
#define MAX_POSE_CELLS    16
#define MAX_ATTACK_PHASES  4
#define MAX_ATTACK_DEFS    4
#define MAX_ENEMY_TYPES    8
#define MAX_ENEMIES        8

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT = 1,
    DIR_DOWN = 2,
    DIR_LEFT = 3
} Direction;

typedef enum {
    ACT_NONE = 0,
    ACT_BLOCK,
    ACT_ATTACK,
    ACT_ROLL,
    ACT_STAGGER
} ActionType;

/* Relative cell in DIR_UP local coordinates (fwd > 0 is front, right > 0 is right hand) */
typedef struct {
    int8_t fwd;
    int8_t right;
    char ch;
    uint8_t attr;
} SpriteCell;

/* One active hitbox phase within an attack animation */
typedef struct {
    int duration_frames;
    int num_cells;
    SpriteCell cells[MAX_HITBOX_CELLS];
} AttackPhase;

/* Complete data-driven attack definition (used by both Player weapons and Enemy weapons) */
typedef struct {
    char name[24];
    int windup_frames;
    int active_frames;     /* Automatically computed as sum of phases[i].duration_frames */
    int recovery_frames;
    int damage;
    int riposte_damage;
    int chip_damage;
    int stamina_cost;
    int stamina_damage;
    bool causes_stagger;

    int num_windup_cells;
    SpriteCell windup_cells[MAX_POSE_CELLS];

    int num_recovery_cells;
    SpriteCell recovery_cells[MAX_POSE_CELLS];

    int num_phases;
    AttackPhase phases[MAX_ATTACK_PHASES];
} AttackDef;

/* Data-driven Enemy Archetype loaded from data/enemies/ Lua scripts */
typedef struct {
    bool used;
    char id[24];
    char name[32];
    int radius;            /* 2 for 5x5 (-2..+2), 3 for 7x7 (-3..+3) */
    int max_hp;
    int move_cooldown;
    int stagger_duration;

    int num_body_cells;
    SpriteCell body_cells[MAX_SPRITE_CELLS];

    int num_idle_weapon;
    SpriteCell idle_weapon[MAX_POSE_CELLS];

    int num_stagger_cells;
    SpriteCell stagger_cells[MAX_POSE_CELLS];

    int num_attacks;
    AttackDef attacks[MAX_ATTACK_DEFS];

    char ai_func[32];      /* Lua AI function name */
} EnemyType;

/* Data-driven Player Archetype loaded from data/player.lua */
typedef struct {
    int radius;
    int max_hp;
    int max_stamina;
    int move_cd_normal;
    int move_cd_block;
    int parry_window_frames;
    int roll_frames;
    int roll_iframes;
    int roll_stamina_cost;

    int num_body_cells;
    SpriteCell body_cells[MAX_SPRITE_CELLS];

    int num_idle_shield;
    SpriteCell idle_shield[MAX_POSE_CELLS];

    int num_idle_weapon;
    SpriteCell idle_weapon[MAX_POSE_CELLS];

    AttackDef light_atk;   /* K key */
    AttackDef heavy_atk;   /* I key */
} PlayerConfig;

typedef struct {
    int x, y;
    char ch;
    uint8_t attr;
} WorldCell;

typedef struct {
    bool active;
    bool is_player;
    int type_idx;          /* Index into enemy_types[] for enemies */
    int cx, cy;
    Direction dir;

    int hp, max_hp;
    int stamina, max_stamina;
    int stamina_regen_cd;

    ActionType action;
    int attack_idx;        /* 0 = light_atk, 1 = heavy_atk for player; index in type->attacks[] for enemy */
    int action_frame;
    int block_hold_frames;
    bool hit_dealt;

    int move_cd;
    int roll_dx, roll_dy;
    int hurt_flash;
    int ai_think_cd;

    int num_hitbox;
    WorldCell hitbox[MAX_HITBOX_CELLS];

    int num_shield;
    WorldCell shield[16];
} Entity;

extern PlayerConfig player_cfg;
extern EnemyType enemy_types[MAX_ENEMY_TYPES];
extern int num_enemy_types;
extern Entity player;
extern Entity enemies[MAX_ENEMIES];

/* Lua Data & AI interface (src/data_lua.c) */
void data_lua_init(void);
void data_lua_shutdown(void);
bool data_lua_reload(char *err_buf, int err_buf_len);
const char *data_lua_call_ai(const EnemyType *et, const Entity *e, int primary_dist, int off_axis);

/* Sprite & Hitbox builder (src/sprites.c) */
void build_entity_hitboxes(Entity *e);
void draw_entity_sprite(const Entity *e);

/* Spawn helpers called by Lua (src/main.c) */
void game_spawn_player(int x, int y, Direction dir);
void game_spawn_enemy(const char *type_id, int x, int y, Direction dir);

#endif /* SOUL2D_GAME_H */
