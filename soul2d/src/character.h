#ifndef SOUL2D_CHARACTER_H
#define SOUL2D_CHARACTER_H

#include "weapon.h"

/* Playfield bounds (Rows 0..3 are HUD, Rows 4..49 are Arena) */
#define ARENA_TOP    4
#define ARENA_BOTTOM 49
#define ARENA_LEFT   0
#define ARENA_RIGHT  79

#define MAX_SPRITE_CELLS  64
#define MAX_ENEMY_TYPES    8
#define MAX_ENEMIES        8

typedef enum {
    ACT_NONE = 0,
    ACT_BLOCK,
    ACT_ATTACK,
    ACT_CRIT_ATTACK, /* Executing a Critical Attack (Riposte or Backstab) */
    ACT_ROLL,
    ACT_STAGGER,     /* Stunned (e.g. parried or guard-broken) */
    ACT_CRIT_HURT    /* Critically struck (backstabbed or riposted; plays special effect) */
} ActionType;

/* Base Moving Character Class Definition (shared by Player and all Enemy Classes) */
typedef struct {
    char id[24];
    char name[32];
    int radius;            /* 2 for 5x5 (-2..+2), 3 for 7x7 (-3..+3) */
    int max_hp;
    int max_stamina;
    int move_cooldown;
    int stagger_duration;
    int crit_hurt_duration;

    char default_left_weapon[24];
    char default_right_weapon[24];

    int num_body_cells;
    SpriteCell body_cells[MAX_SPRITE_CELLS];

    int num_stagger_cells;
    SpriteCell stagger_cells[MAX_POSE_CELLS];

    int num_crit_hurt_cells;
    SpriteCell crit_hurt_cells[MAX_POSE_CELLS];
    uint8_t crit_flash_attr;
} CharacterDef;

/* Enemy Class Definition (extends CharacterDef with AI & critical-hit special effect) */
typedef struct {
    bool used;
    CharacterDef base;
    char ai_func[32];
} EnemyDef;

/* Player Class Definition (extends CharacterDef with dodge roll & weapon loadout) */
typedef struct {
    CharacterDef base;
    int roll_frames;
    int roll_iframes;
    int roll_stamina_cost;
    char alt_right_weapon[24];
} PlayerDef;

/* Runtime Moving Character Instance */
typedef struct {
    bool active;
    bool is_player;
    const CharacterDef *def;
    const EnemyDef *enemy_def;

    const WeaponDef *left_weapon;
    const WeaponDef *right_weapon;

    int cx, cy;
    Direction dir;

    int hp, max_hp;
    int stamina, max_stamina;
    int stamina_regen_cd;

    ActionType action;
    WeaponSlot active_hand;
    int attack_slot;       /* WATK_NORMAL, WATK_SPECIAL, WATK_CRIT */
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
} Character;

extern const int dir_dx[4];
extern const int dir_dy[4];

extern PlayerDef player_def;
extern EnemyDef enemy_defs[MAX_ENEMY_TYPES];
extern int num_enemy_defs;

extern Character player;
extern Character enemies[MAX_ENEMIES];

int iabs(int v);
void character_init(Character *c, bool is_player, const CharacterDef *def,
                    const EnemyDef *edef, int x, int y, Direction dir);
bool character_equip(Character *c, WeaponSlot slot, const char *weapon_id);
const WeaponDef *character_get_weapon(const Character *c, WeaponSlot slot);
const AttackDef *character_get_active_attack(const Character *c);

int character_radius(const Character *c);
bool character_can_move_to(const Character *self, int nx, int ny);
bool character_try_move(Character *c, int dx, int dy);
void character_start_attack(Character *c, WeaponSlot hand, int atk_slot, int regen_cd);
bool character_tick_common(Character *c);
bool character_is_behind(const Character *attacker, const Character *target);

#endif /* SOUL2D_CHARACTER_H */
