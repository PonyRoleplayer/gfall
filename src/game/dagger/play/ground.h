#ifndef OGFALL_GROUND_H
#define OGFALL_GROUND_H
#include "game/dagger/play/state.h"
#include "engine/scene/scene.h"
/* Dropped items outside use a map pixel plus a small position within it.
 * Neither their keys nor their saved coordinates depend on the stream origin. */
/* The pile at a point, made with a loot table's roll if it is new (a killed guard's things); -1 is the empty pile of a drop. */
df_u32 og_ground_pile(float x, float y, float z, int table);
df_u32 og_ground_container(float x, float y, float z);
int og_ground_position(const OgObject *container, float *x, float *y, float *z);
void og_ground_open(const Scene *scene, int sprite_slot);
void og_ground_close(void);
void og_ground_frame(const Camera *camera);
df_u32 og_ground_aim(float x, float y, float z, float dx, float dy, float dz, float reach);
#endif
