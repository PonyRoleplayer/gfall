#ifndef OGFALL_DUNGEON_H
#define OGFALL_DUNGEON_H

/* OGFALL's creature combat and swept arrows. Entity pictures and collision
 * belong to the engine; all combat decisions are here. See docs/ogfall.md. */
#include "game/dagger/play/weapon.h"
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
/* A town streamed round the player: no creature is the scene's own (its entities are the street's people, not foes), but the watch may be made of them. */
void og_dungeon_open_street(const Scene *scene);
void og_dungeon_step(const Scene *scene, const Camera *cam, float dt);
void og_dungeon_strike(const Scene *scene, const Camera *cam, const OgStrike *strike);
int og_dungeon_health(int index);
int og_dungeon_loot_table(int index);
df_u32 og_dungeon_container(int index);
void og_dungeon_gear_to(int index, df_u32 container);
/* The creatures round a point for the enchantments' ticks: the nearest's distance (-1 for none), one of a category within reach, and a hurt to the nearest within reach. */
int og_dungeon_nearest_distance(const Camera *cam);
int og_dungeon_category_within(const Camera *cam, int category, int reach);
int og_dungeon_hurt_nearest(const Camera *cam, float reach, int damage);
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
/* 1 when entity is one of the dungeon's creatures (it has a mind). */
int og_dungeon_has(const Entity *entity);
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
/* A creature made awake, knowing where the player is: the City Watch called in a town (a give-up three times a creature's). */
void og_dungeon_wake(int index);
/* The index of the creature on this entity, -1 for none. */
int og_dungeon_index(const Entity *e);
/* A creature taken out of the place (a court sent the watch away): not live, no health, not counted. */
void og_dungeon_remove(int index);
/*
 * The street (src/hand/func_000733AD.c 96..110): a swing's hit frame in a town also strikes the people of the street, none of the rolls of a creature: whoever stands within 90 units
 * and 200 of the view is killed. The game does it (hit(cam) returns how many it struck); a creature's death is told to the game (died(kind, entity)) so it can lay the body.
 */
void og_dungeon_set_street(int (*hit)(const Camera *cam), void (*died)(int kind, Entity *entity));

#endif
