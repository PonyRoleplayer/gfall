#ifndef GFALL_DUNGEON_H
#define GFALL_DUNGEON_H

/* GFALL's creature combat and swept arrows. Entity pictures and collision
 * belong to the engine; all combat decisions are here. See docs/game.md. */
#include "game/dagger/play/combat/weapon.h"
#include "engine/scene/entity.h"

/*
 * A creature's reach and senses (ai_creature_think, equip.c). It reaches FOE_REACH_PLAYER at the player and FOE_REACH_FOE at another creature, and shoots from FOE_RANGED_REACH_MUL times
 * that when it can; it swings, shoots or casts facing within FOE_ATTACK_ARC; nothing beyond FOE_ACTIVE_RANGE is thought of. Its voice is heard within FOE_VOICE_RANGE and, closer than
 * FOE_VOICE_NEAR, is the attack's three times in four. Between swings it waits FOE_ATTACK_WAIT_MIN..MAX milliseconds, less FOE_ATTACK_WAIT_PER_LEVEL for each of the player's levels over
 * FOE_ATTACK_WAIT_LEVEL_PIVOT, a fresh creature having reflexes 0 (FOE_ATTACK_WAIT_PER_REFLEX each below normal), and FOE_ATTACK_WAIT_FALLBACK when that comes to nothing.
 * Ours, not the decomp's: the body and its clamp to the picture, the chest and the bow above the feet, the stop FOE_CLOSE_MARGIN short of the reach, the voice timer, the retarget and
 * sleeper-look timers, the hurt time, and the watch's longer memory.
 */
#define FOE_REACH_PLAYER 90
#define FOE_REACH_FOE 60
#define FOE_RANGED_REACH_MUL 4
#define FOE_ATTACK_ARC 128
/* A creature's picture types are 0..FOE_TYPES - 1 (dungeon_variant); its speed rate is a percent, FOE_RATE_NORMAL unless a place gives one. */
#define FOE_TYPES 60
#define FOE_RATE_NORMAL 100
#define FOE_ACTIVE_RANGE 2048
#define FOE_VOICE_RANGE 1024
#define FOE_VOICE_NEAR 128
#define FOE_ATTACK_WAIT_MIN 1500
#define FOE_ATTACK_WAIT_MAX 3000
#define FOE_ATTACK_WAIT_LEVEL_PIVOT 10
#define FOE_ATTACK_WAIT_PER_LEVEL 50
#define FOE_ATTACK_WAIT_PER_REFLEX 450
#define FOE_ATTACK_WAIT_FALLBACK 1500
#define FOE_BODY_RADIUS 12
#define FOE_BODY_HEIGHT 80
#define FOE_BODY_STEP 24
#define FOE_BODY_EYE 60
#define FOE_BODY_HEIGHT_MIN 24
#define FOE_BODY_HEIGHT_MAX 128
#define FOE_BODY_STEP_DIV 4
#define FOE_CHEST_HEIGHT 40
#define FOE_BOW_HEIGHT 50
#define FOE_CLOSE_MARGIN 20
#define FOE_VOICE_STAGGER_MS 4000
#define FOE_VOICE_MIN_MS 2000
#define FOE_VOICE_MAX_MS 6000
#define FOE_RETARGET_TIME 0.45f
#define FOE_RETARGET_STAGGER 0.04f
#define FOE_RETARGET_SLOTS 8
#define FOE_SLEEP_LOOK_TIME 0.2f
#define FOE_HURT_TIME 0.25f
#define WATCH_GIVE_UP 600
#define FOE_ANIM_DEFAULT_MS 100
/* The animation scripts' actions and the pictures they use (monster_set_action). */
#define ANIM_ACTION_ATTACK 8
#define ANIM_ACTION_SHOOT 24
#define ANIM_ACTION_CAST 32
#define RECORD_CAST 20
#define RECORD_SHOOT_2 25
#define FOE_CAST_PAUSE 0.5f
#define FOE_ATTACK_WAIT_FLOOR 0.1f
/* A creature in reach also needs rand() % SPD < SPD / FOE_SWING_SPEED_DIV + FOE_SWING_SPEED_ADD to swing (ai_creature_think, equip.c 996), rolled once a frame of the original, which we take as
 * FOE_THINK_HZ a second (its frame rate was not fixed; a Pentium 90 ran the game at some ten to twenty): a speed of 50 swings on one step in four. */
#define FOE_SWING_SPEED_DIV 8
#define FOE_SWING_SPEED_ADD 6
#define FOE_THINK_HZ 20

#define FOES 128
#define FOE_ACTORS (FOES + 16)
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
int dungeon_loaded_marker(int index);

enum { TEAM_PLAYER, TEAM_HOSTILE };
enum { TARGET_NONE = -2, TARGET_PLAYER = -1 };
int dungeon_team(int index);
void dungeon_set_team(int index, int team);
int dungeon_target(int index);
int dungeon_is_pacified(int index);
void dungeon_set_pacified(int index, int passive);
/* Same human variant as the walking art; no sprite pointers enter saves. */
void dungeon_variant(int index, int texture_kind);
int dungeon_nearby(const Camera *camera, float reach);
/* How many creatures are loaded and alive, of any team and temper: the original's creature_count, which a quest's courier (op 76) waits on. */
int dungeon_alive(void);
/* A creature thrown (damage_knockback) by force and amount the way yaw points, and hurt: an area spell's (radius * AREA_KNOCKBACK_MUL, radius). */
/* monster_wake_all: every creature the markers keep standing is awake and acts. */
void dungeon_wake_sleepers(void);
/* The creatures added from now are summoned by an artifact (until it is called with 0): they give up a target scoring under SUMMON_GIVE_UP_SCORE and vanish when alone. */
#define SUMMON_GIVE_UP_SCORE 8
void dungeon_next_summoned(int on);
/* The Wabbajack: creatures added from now are its work (not changed again); whether one is, and a creature's most health. */
void dungeon_next_transformed(int on);
int dungeon_transformed(int index);
int dungeon_max_health(int index);
/* The index of the creature added last (the one a summon or a placement made). */
int dungeon_last_added(void);
/* The nearest live creature the ray (from a point, along a unit direction) passes within AIM_RADIUS_MIN (or its own radius) of, within reach; -1 for none. A pickpocket's target (click_creature). */
#define AIM_RADIUS_MIN 32.0f
int dungeon_aim(float x, float y, float z, float dx, float dy, float dz, float reach);
/* A creature's level (a pickpocket weighs it against the player's). */
int dungeon_level(int index);
void dungeon_knockback(int index, int force, int amount, float yaw);
/* Source is a struck creature index, or -1 for a door/noise. */
void dungeon_alert(int source);
int dungeon_count(void);
/* 1 when entity is one of the dungeon's creatures (it has a mind). */
int dungeon_has(const Entity *entity);
/* guards_are_present: a living guard of the City Watch (kind 146) in the place. */
int dungeon_guards(void);
Entity *dungeon_entity(int index);
/* Attach a creature whose Entity remains valid until release or place close. */
int dungeon_add(Entity *entity, int kind, int passive, int rate);
void dungeon_rebind(Entity *entities, int count);
int dungeon_kind(int index);
void dungeon_enchant_damage(int index, int damage, int source);
int dungeon_enchant_heal(int index, int amount);
int dungeon_spell_target(int index, CharacterState *who, ClassInfo *career, int *base);
void dungeon_spell_commit(int index, const CharacterState *who, int caster);
void dungeon_pacify(int index);
/* A creature made awake, knowing where the player is: the City Watch called in a town (a give-up three times a creature's). */
void dungeon_wake(int index);
/* The index of the creature on this entity, -1 for none. */
int dungeon_index(const Entity *e);
/* A creature taken out of the place (a court sent the watch away): not live, no health, not counted. */
void dungeon_remove(int index);
/* Forget a pool actor before the owner frees its Entity. */
void dungeon_release_actor(int index);
/*
 * The street (src/hand/func_000733AD.c 96..110): a swing's hit frame in a town also strikes the people of the street, none of the rolls of a creature: whoever stands within 90 units
 * and 200 of the view is killed. The game does it (hit(cam) returns how many it struck); a creature's death is told to the game (died(kind, entity)) so it can lay the body.
 */
void dungeon_set_street(int (*hit)(const Camera *cam), void (*died)(int kind, Entity *entity));

#endif
