#ifndef SOUL2D_RENDER_H
#define SOUL2D_RENDER_H

#include "character.h"

void render_local_to_world(const Character *c, int fwd, int right, int *wx, int *wy);
uint8_t render_rot_tile(uint8_t ch, Direction dir);
void render_character(const Character *c);
void render_scene(const char *status_msg, uint8_t status_attr);

#endif /* SOUL2D_RENDER_H */
