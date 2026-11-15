#ifndef SOUL2D_CHARACTER_H
#define SOUL2D_CHARACTER_H

#include "weapon.h"

/* Playfield bounds in 320x200 pixel coordinates (Rows 0..25 are HUD) */
#define ARENA_TOP    28
#define ARENA_BOTTOM 195
#define ARENA_LEFT   6
#define ARENA_RIGHT  313

#define MAX_ENEMY_TYPES 8
#define MAX_ENEMIES     8

typedef enum {
    BSTYLE_KNIGHT = 0,
    BSTYLE_SKELETON,
    BSTYLE_SKELETON_LORD
} BodyStyle;

typedef enum {
    ACT_NONE = 0,
    ACT_BLOCK,
    ACT_ATTACK,
    ACT_CRIT_ATTACK, /* Executing a Critical Attack (Riposte or Backstab) */
    ACT_ROLL,
    ACT_STAGGER,     /* Stunned (e.g. parried or guard-broken) */
    ACT_CRIT_HURT    /* Critically struck (backstabbed or riposted; plays special effect) */
} ActionType;

/* Axis-aligned or directional bounding box in world pixel space */
typedef struct {
    bool active;
    int x1, y1, x2, y2;
} HitRect;

typedef enum {
    HEAD_GREATHELM = 0,
    HEAD_SALLET,
    HEAD_HORNED_HELM,
    HEAD_SKULL,
    HEAD_CROWNED_SKULL,
    HEAD_HOOD
} HeadStyle;

typedef enum {
    CREST_NONE = 0,
    CREST_PLUME,
    CREST_HORNS,
    CREST_SPIKES
} CrestStyle;

typedef enum {
    TORSO_PLATE = 0,
    TORSO_SURCOAT,
    TORSO_RIBCAGE,
    TORSO_ROBE
} TorsoStyle;

typedef enum {
    PAULDRON_NONE = 0,
    PAULDRON_ROUND,
    PAULDRON_SPIKED
} PauldronStyle;

typedef enum {
    CAPE_NONE = 0,
    CAPE_FLOWING,
    CAPE_TATTERED
} CapeStyle;

/* Procedurally generated visual traits for any Biped character */
typedef struct {
    HeadStyle head;
    CrestStyle crest;
    TorsoStyle torso;
    PauldronStyle pauldron;
    CapeStyle cape;

    uint8_t primary_ramp;  /* Base palette index of 4-shade ramp (dark, mid, light, shine) */
    uint8_t cloth_ramp;    /* Base palette index of 4-shade cloth/cape ramp */
    uint8_t trim_col;      /* Belt / crown / buckle highlight */
    uint8_t eye_col;       /* Eye / visor glow (0 = dark visor) */
    uint8_t crest_col;     /* Plume / horn color */

    int8_t shoulder_half_w;/* 3..6 */
    int8_t torso_h;        /* 6..8 */
    int8_t leg_len;        /* 4..6 */
} BipedAppearance;

/* Base Moving Character Class Definition (shared by Player and all Enemy Classes) */
typedef struct {
    char id[24];
    char name[32];
    BodyStyle body_style;
    int scale_pct;         /* 100 = 1.0x normal, 135 = 1.35x elite/boss */
    int radius;            /* Collision bounding radius in pixels (default 8) */
    int max_hp;
    int max_stamina;
    int armor;             /* Flat damage mitigation (0..12) */
    int move_speed_fp;     /* Walk speed in 1/16th pixels per frame (Dark Souls 1 pace) */
    int turn_cooldown;     /* Frames before an enemy can snap-turn to a new direction */
    int stagger_duration;
    int crit_hurt_duration;

    char default_left_weapon[24];
    char default_right_weapon[24];

    bool has_fixed_appearance;
    BipedAppearance fixed_appearance;
} CharacterDef;

/* Enemy Class Definition (extends CharacterDef with AI & Difficulty parameters) */
typedef struct {
    bool used;
    CharacterDef base;
    int difficulty;        /* 1, 2, ... */
    int ai_aggression;     /* 1..100: attack cadence */
    int ai_special_pct;    /* 0..100: probability of special/heavy attack */
    int attack_range_bonus;/* Extra engagement reach in pixels */
    char ai_func[32];
} EnemyDef;

/* Player Class Definition (extends CharacterDef with dodge roll & weapon loadout) */
typedef struct {
    CharacterDef base;
    int roll_frames;
    int roll_iframes;
    int roll_speed_fp;     /* Roll speed in 1/16th pixels per frame */
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
    BipedAppearance appearance;

    int cx, cy;            /* World pixel coordinates */
    int sub_x, sub_y;      /* 1/16th pixel sub-pixel accumulators */
    Direction dir;
    int walk_ticks;        /* Increments when moving to drive procedural walk cycle */
    int turn_cd;

    int hp, max_hp;
    int stamina, max_stamina;
    int stamina_regen_cd;
    int skill_cd;          /* Weapon skill cooldown ([U]) */

    ActionType action;
    WeaponSlot active_hand;
    int attack_slot;       /* WATK_NORMAL, WATK_SPECIAL, WATK_CRIT */
    int action_frame;
    int block_hold_frames;
    bool hit_dealt;

    int roll_dx, roll_dy;
    int hurt_flash;
    int ai_think_cd;

    HitRect hitbox;
    HitRect shield_box;
} Character;

extern const int dir_dx[4];
extern const int dir_dy[4];

extern PlayerDef player_def;
extern EnemyDef enemy_defs[MAX_ENEMY_TYPES];
extern int num_enemy_defs;

extern Character player;
extern Character enemies[MAX_ENEMIES];

int iabs(int v);
void character_generate_appearance(BipedAppearance *a, BodyStyle style);
void character_init(Character *c, bool is_player, const CharacterDef *def,
                    const EnemyDef *edef, int x, int y, Direction dir);
bool character_equip(Character *c, WeaponSlot slot, const char *weapon_id);
const WeaponDef *character_get_weapon(const Character *c, WeaponSlot slot);
const AttackDef *character_get_active_attack(const Character *c);

int character_radius(const Character *c);
bool character_can_move_to(const Character *self, int nx, int ny);
bool character_try_move_px(Character *c, int dx, int dy);
void character_move_subpixel(Character *c, int vx_fp, int vy_fp);
void character_start_attack(Character *c, WeaponSlot hand, int atk_slot, int regen_cd);
bool character_tick_common(Character *c);
bool character_is_behind(const Character *attacker, const Character *target);

#endif /* SOUL2D_CHARACTER_H */
