#ifndef SOUL2D_RENDER_H
#define SOUL2D_RENDER_H

#include "character.h"

void render_local_to_world(const Character *c, int fwd, int right, int *wx, int *wy);
void render_character(const Character *c);
void render_scene(const char *status_msg, uint8_t status_col);

#endif /* SOUL2D_RENDER_H */
