#ifndef GFALL_DUNGEON_H
#define GFALL_DUNGEON_H

/* GFALL's creature combat and swept arrows. Entity pictures and collision
 * belong to the engine; all combat decisions are here. See docs/gfall.md. */
#include "game/dagger/play/weapon.h"
#include "engine/scene/entity.h"

#define FOES 128
#define ARROWS 16
typedef struct ArrowInfo {
    int live, source;
    df_u32 weapon;
    float at[3], direction[3], distance;
    int index, material;
} ArrowInfo;

void dungeon_open(const Scene *scene, const int *kinds, const int *passive, const int *rates);
void dungeon_close(void);
/* A town streamed round the player: no creature is the scene's own (its entities are the street's people, not foes), but the watch may be made of them. */
void dungeon_open_street(const Scene *scene);
void dungeon_step(const Scene *scene, const Camera *cam, float dt);
void dungeon_strike(const Scene *scene, const Camera *cam, const StrikeInfo *strike);
int dungeon_health(int index);
int dungeon_loot_list(int index);
df_u32 dungeon_container(int index);
void dungeon_gear_to(int index, df_u32 container);
/* The creatures round a point for the enchantments' ticks: the nearest's distance (-1 for none), one of a category within reach, and a hurt to the nearest within reach. */
int dungeon_nearest_distance(const Camera *cam);
int dungeon_category_within(const Camera *cam, int category, int reach);
int dungeon_hurt_nearest(const Camera *cam, float reach, int damage);
const ArrowInfo *dungeon_arrow(int index);
void dungeon_shift(float dx, float dz);
/* Sprite slots, loaded by the adapter with the place. */
void dungeon_blood(int red_slot, int green_slot);
int dungeon_loaded(int index);
/* Same human variant as the walking art; no sprite pointers enter saves. */
void dungeon_variant(int index, int texture_kind);
int dungeon_nearby(const Camera *camera, float reach);
/* Source is a struck creature index, or -1 for a door/noise. */
void dungeon_alert(int source);
int dungeon_count(void);
/* 1 when entity is one of the dungeon's creatures (it has a mind). */
int dungeon_has(const Entity *entity);
/* guards_are_present: a living guard of the City Watch (kind 146) in the place. */
int dungeon_guards(void);
Entity *dungeon_entity(int index);
/* Append a runtime creature; its Entity has a stable address until close. */
int dungeon_add(Entity *entity, int kind, int passive, int rate);
void dungeon_rebind(Entity *entities, int count);
int dungeon_kind(int index);
int dungeon_spell_target(int index, CharacterState *who, ClassInfo *career, int *base);
void dungeon_spell_commit(int index, const CharacterState *who, int caster);
void dungeon_pacify(int index);
/* A creature made awake, knowing where the player is: the City Watch called in a town (a give-up three times a creature's). */
void dungeon_wake(int index);
/* The index of the creature on this entity, -1 for none. */
int dungeon_index(const Entity *e);
/* A creature taken out of the place (a court sent the watch away): not live, no health, not counted. */
void dungeon_remove(int index);
/*
 * The street (src/hand/func_000733AD.c 96..110): a swing's hit frame in a town also strikes the people of the street, none of the rolls of a creature: whoever stands within 90 units
 * and 200 of the view is killed. The game does it (hit(cam) returns how many it struck); a creature's death is told to the game (died(kind, entity)) so it can lay the body.
 */
void dungeon_set_street(int (*hit)(const Camera *cam), void (*died)(int kind, Entity *entity));

#endif
