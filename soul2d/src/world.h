#ifndef SOUL2D_WORLD_H
#define SOUL2D_WORLD_H

#include "character.h"

#define WORLD_NUM_ENEMY_TYPES 3

typedef struct {
    unsigned int seed;
    int room_id;
    int num_prebuilt_enemies;
    char prebuilt_enemy_ids[WORLD_NUM_ENEMY_TYPES][24];
} WorldState;

extern WorldState current_world;

void enemy_generate(int difficulty, int slot_idx);
bool world_build(char *err_buf, int err_buf_len);
void world_respawn(void);

#endif /* SOUL2D_WORLD_H */
