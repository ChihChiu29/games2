#include "weapon.h"
#include <string.h>

static WeaponDef weapon_types[MAX_WEAPON_TYPES];
static int num_weapon_types = 0;

void weapon_registry_clear(void)
{
    memset(weapon_types, 0, sizeof(weapon_types));
    num_weapon_types = 0;
}

WeaponDef *weapon_register(const char *id)
{
    int i;
    if (!id || !id[0]) return NULL;

    for (i = 0; i < num_weapon_types; i++) {
        if (strcmp(weapon_types[i].id, id) == 0) {
            memset(&weapon_types[i], 0, sizeof(WeaponDef));
            weapon_types[i].used = true;
            strncpy(weapon_types[i].id, id, sizeof(weapon_types[i].id) - 1);
            return &weapon_types[i];
        }
    }

    if (num_weapon_types >= MAX_WEAPON_TYPES) return NULL;
    WeaponDef *w = &weapon_types[num_weapon_types++];
    memset(w, 0, sizeof(*w));
    w->used = true;
    w->level = 1;
    strncpy(w->id, id, sizeof(w->id) - 1);
    return w;
}

int weapon_compute_damage(const WeaponDef *w, const AttackDef *atk)
{
    if (!atk) return 0;
    int lvl = (w && w->level > 1) ? w->level : 1;
    /* +15% damage per weapon level above 1 */
    return atk->damage + ((atk->damage * (lvl - 1) * 15) / 100);
}

const WeaponDef *weapon_find(const char *id)
{
    int i;
    if (!id || !id[0]) return NULL;
    for (i = 0; i < num_weapon_types; i++) {
        if (weapon_types[i].used && strcmp(weapon_types[i].id, id) == 0) {
            return &weapon_types[i];
        }
    }
    return NULL;
}

const AttackDef *weapon_get_attack(const WeaponDef *w, int atk_slot)
{
    if (!w || w->kind != WEAPON_MELEE) return NULL;
    if (atk_slot < 0 || atk_slot >= WATK_COUNT) return NULL;
    if (!w->attacks[atk_slot].valid) return NULL;
    return &w->attacks[atk_slot];
}

const AttackDef *weapon_find_attack_by_name(const WeaponDef *w, const char *name, int *out_slot)
{
    int i;
    if (!w || w->kind != WEAPON_MELEE || !name || !name[0]) return NULL;

    if (strcmp(name, "normal") == 0 && w->attacks[WATK_NORMAL].valid) {
        if (out_slot) *out_slot = WATK_NORMAL;
        return &w->attacks[WATK_NORMAL];
    }
    if (strcmp(name, "special") == 0 && w->attacks[WATK_SPECIAL].valid) {
        if (out_slot) *out_slot = WATK_SPECIAL;
        return &w->attacks[WATK_SPECIAL];
    }
    if (strcmp(name, "crit") == 0 && w->attacks[WATK_CRIT].valid) {
        if (out_slot) *out_slot = WATK_CRIT;
        return &w->attacks[WATK_CRIT];
    }

    for (i = 0; i < WATK_COUNT; i++) {
        if (w->attacks[i].valid && strcmp(w->attacks[i].name, name) == 0) {
            if (out_slot) *out_slot = i;
            return &w->attacks[i];
        }
    }
    return NULL;
}
