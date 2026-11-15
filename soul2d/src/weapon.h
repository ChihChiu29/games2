#ifndef SOUL2D_WEAPON_H
#define SOUL2D_WEAPON_H

#include "platform.h"

#define MAX_WEAPON_TYPES 16

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
    WSTYLE_SPEAR = 0,
    WSTYLE_SWORD,
    WSTYLE_GREATSWORD,
    WSTYLE_SHIELD
} WeaponStyle;

typedef enum {
    ANIM_THRUST = 0,
    ANIM_SLASH,
    ANIM_CLEAVE,
    ANIM_IMPALE
} AttackAnimType;

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

/* Complete data-driven attack animation & hitbox geometry (in 320x200 pixel space) */
typedef struct {
    bool valid;
    char name[24];
    AttackAnimType anim_type;
    int windup_frames;
    int active_frames;
    int recovery_frames;

    int reach_px;          /* Forward reach from character center (pixels) */
    int width_px;          /* Half-width of hitbox perpendicular to facing (pixels) */

    int damage;
    int chip_damage;
    int stamina_cost;
    int stamina_damage;
    bool causes_stagger;
    bool is_critical;
} AttackDef;

/* Base Weapon / Equipment Class Definition (registered in data/weapons/) */
typedef struct {
    bool used;
    char id[24];
    char name[32];
    WeaponKind kind;
    WeaponStyle style;

    int length_px;         /* Total weapon length in pixels */
    int blade_width_px;    /* Blade width in pixels */

    /* Shield properties (when kind == WEAPON_SHIELD) */
    int block_speed_fp;    /* Walk speed while blocking (1/16 px per frame) */
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
