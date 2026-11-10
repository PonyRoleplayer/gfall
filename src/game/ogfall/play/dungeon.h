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
void og_dungeon_shift(float dx, float dz);
/* Sprite slots, loaded by the adapter with the place. */
void og_dungeon_blood(int red_slot, int green_slot);
int og_dungeon_loaded(int index);
/* Same human variant as the walking art; no sprite pointers enter saves. */
void og_dungeon_variant(int index, int texture_kind);
int og_dungeon_nearby(const Camera *camera, float reach);
/* Source is a struck creature index, or -1 for a door/noise. */
void og_dungeon_alert(int source);
int og_dungeon_count(void);
/* guards_are_present: a living guard of the City Watch (kind 146) in the place. */
int og_dungeon_guards(void);
Entity *og_dungeon_entity(int index);
/* Append a runtime creature; its Entity has a stable address until close. */
int og_dungeon_add(Entity *entity, int kind, int passive, int rate);
void og_dungeon_rebind(Entity *entities, int count);
int og_dungeon_kind(int index);
int og_dungeon_spell_target(int index, OgCharacter *who, OgClass *career, int *base);
void og_dungeon_spell_commit(int index, const OgCharacter *who, int caster);
void og_dungeon_pacify(int index);

#endif
