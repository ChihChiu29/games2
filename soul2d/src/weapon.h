#ifndef SOUL2D_WEAPON_H
#define SOUL2D_WEAPON_H

#include "platform.h"

#define MAX_HITBOX_CELLS  48
#define MAX_POSE_CELLS    16
#define MAX_ATTACK_PHASES  4
#define MAX_WEAPON_TYPES  16

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT = 1,
    DIR_DOWN = 2,
    DIR_LEFT = 3
} Direction;

typedef enum {
    WEAPON_NONE = 0,
    WEAPON_MELEE,
    WEAPON_SHIELD
} WeaponKind;

typedef enum {
    SLOT_LEFT_HAND = 0,
    SLOT_RIGHT_HAND = 1
} WeaponSlot;

/* Standard attack slots on any melee weapon */
typedef enum {
    WATK_NORMAL  = 0, /* Light / Normal attack (K or J) */
    WATK_SPECIAL = 1, /* Heavy / Special attack (I or U) */
    WATK_CRIT    = 2, /* Critical attack: Riposte (on stunned) or Backstab (from behind) */
    WATK_COUNT   = 3
} WeaponAttackSlot;

/* Relative cell in DIR_UP local coordinates (fwd > 0 is front, right > 0 is right hand) */
typedef struct {
    int8_t fwd;
    int8_t right;
    uint8_t ch;
    uint8_t attr;
} SpriteCell;

typedef struct {
    int x, y;
    uint8_t ch;
    uint8_t attr;
} WorldCell;

/* One active hitbox phase within an attack animation */
typedef struct {
    int duration_frames;
    int num_cells;
    SpriteCell cells[MAX_HITBOX_CELLS];
} AttackPhase;

/* Complete data-driven attack animation & combat properties */
typedef struct {
    bool valid;
    char name[24];
    int windup_frames;
    int active_frames;     /* Sum of phases[i].duration_frames */
    int recovery_frames;
    int damage;
    int chip_damage;
    int stamina_cost;
    int stamina_damage;
    bool causes_stagger;
    bool is_critical;

    int num_windup_cells;
    SpriteCell windup_cells[MAX_POSE_CELLS];

    int num_recovery_cells;
    SpriteCell recovery_cells[MAX_POSE_CELLS];

    int num_phases;
    AttackPhase phases[MAX_ATTACK_PHASES];
} AttackDef;

/* Base Weapon / Equipment Class Definition (registered in data/weapons/) */
typedef struct {
    bool used;
    char id[24];
    char name[32];
    WeaponKind kind;
    bool authored_left;    /* true if idle_cells were authored for left hand (right < 0) */

    int num_idle_cells;
    SpriteCell idle_cells[MAX_POSE_CELLS];

    /* Shield properties (when kind == WEAPON_SHIELD) */
    int block_move_cd;
    int parry_window_frames;

    /* Melee attack animations: Normal, Special, and Critical (Riposte/Backstab) */
    AttackDef attacks[WATK_COUNT];
} WeaponDef;

void weapon_registry_clear(void);
WeaponDef *weapon_register(const char *id);
const WeaponDef *weapon_find(const char *id);
const AttackDef *weapon_get_attack(const WeaponDef *w, int atk_slot);
const AttackDef *weapon_find_attack_by_name(const WeaponDef *w, const char *name, int *out_slot);

#endif /* SOUL2D_WEAPON_H */
