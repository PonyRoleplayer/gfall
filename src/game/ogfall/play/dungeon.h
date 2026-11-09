#ifndef OGFALL_DUNGEON_H
#define OGFALL_DUNGEON_H

/* OGFALL's creature combat and swept arrows. Entity pictures and collision
 * belong to the engine; all combat decisions are here. See docs/ogfall.md. */
#include "game/ogfall/play/weapon.h"
#include "engine/scene/entity.h"

#define OG_FOES 128
#define OG_ARROWS 16
typedef struct OgArrow {
    int live, source;
    df_u32 weapon;
    float at[3], direction[3], distance;
    int index, material;
} OgArrow;

void og_dungeon_open(const Scene *scene, const int *kinds, const int *passive, const int *rates);
void og_dungeon_close(void);
void og_dungeon_step(const Scene *scene, const Camera *cam, float dt);
void og_dungeon_strike(const Scene *scene, const Camera *cam, const OgStrike *strike);
int og_dungeon_health(int index);
int og_dungeon_loot_table(int index);
df_u32 og_dungeon_container(int index);
const OgArrow *og_dungeon_arrow(int index);
/* Sprite slots, loaded by the adapter with the place. */
void og_dungeon_blood(int red_slot, int green_slot);
int og_dungeon_loaded(int index);
/* Same human variant as the walking art; no sprite pointers enter saves. */
void og_dungeon_variant(int index, int texture_kind);
int og_dungeon_nearby(const Camera *camera, float reach);

#endif
