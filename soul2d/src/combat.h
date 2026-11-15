#ifndef SOUL2D_COMBAT_H
#define SOUL2D_COMBAT_H

#include "character.h"

Character *combat_find_crit_target(const Character *attacker);
void combat_build_hitboxes(Character *c);
void combat_resolve_all(void);

#endif /* SOUL2D_COMBAT_H */
