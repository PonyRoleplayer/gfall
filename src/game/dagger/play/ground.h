#ifndef GFALL_GROUND_H
#define GFALL_GROUND_H
#include "game/dagger/play/state.h"
#include "engine/scene/scene.h"
/* Dropped items outside use a map pixel plus a small position within it.
 * Neither their keys nor their saved coordinates depend on the stream origin. */
/* The pile at a point, made with a loot table's roll if it is new (a killed guard's things); -1 is the empty pile of a drop. */
df_u32 ground_pile(float x, float y, float z, int table);
df_u32 ground_container(float x, float y, float z);
int ground_position(const ObjectInfo *container, float *x, float *y, float *z);
void ground_open(const Scene *scene, int sprite_slot);
void ground_close(void);
void ground_frame(const Camera *camera);
df_u32 ground_aim(float x, float y, float z, float dx, float dy, float dz, float reach);
#endif
