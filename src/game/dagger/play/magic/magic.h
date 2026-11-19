#ifndef GFALL_MAGIC_H
#define GFALL_MAGIC_H
#include "game/dagger/play/magic/spellbook.h"
#include "game/dagger/play/play.h"
#include "engine/gfx/camera.h"
/* Index -1 is the player; creature identity is converted to a persistent key. */
KeepKey magic_key(int target);
void magic_forget_transients(void);
void magic_forget_actor(int target);
int magic_index(KeepKey key);
int magic_target(int index, CharacterState *who, ClassInfo *career, int *base);
void magic_commit(int index, const CharacterState *who, int caster);
int spell_cost(const SpellDef *spell, const CharacterState *caster);
/* One effect's raw cost, the formula of its group over its cost record and numbers (spell_cost_formula_dispatch); what the spell maker stores and prices by. */
int effect_cost_raw(int group, const df_i16 *c, const int *d, const int *ch, const int *m);
/*
 * magic_resist (spfx_resist_roll): the chance starts at RESIST_BASE, immunity ends it at 0 and a critical weakness at 100, a tolerance halves it and a resistance adds half again, a Nord against
 * cold and a Breton against magic gain RESIST_RACIAL, and it is held to RESIST_MIN..RESIST_MAX. A roll over the chance is the full effect; one that is more than RESIST_NONE_BAND under it, none; between
 * them the effect is the margin times RESIST_PARTIAL_STEP percent.
 */
/* The spell cost formulas (spell_cost: spell_effect_cost_formula[type] - 1 dispatched): duration and chance; duration and magnitude; duration; chance; magnitude; the two mixed ones. */
enum { SPELL_COST_DURATION_CHANCE = 1, SPELL_COST_DURATION_MAGNITUDE, SPELL_COST_DURATION, SPELL_COST_CHANCE, SPELL_COST_MAGNITUDE, SPELL_COST_MIXED_6, SPELL_COST_MIXED_7 };
/* Our index for the trap (a link's spell, cast with no caster record) and the stats it casts with. */
#define MAGIC_INDEX_TRAP -3
#define MAGIC_TRAP_STAT 50
/* Spell icons: a lasting spell with an icon below SPELL_ICON_HELD, or SPELL_ICON_STRIKE (a weapon's strike spell), extends the same effect instead of stacking (cast_spell_on, runspell.c:184);
 * an item's held spell has icon SPELL_ICON_HELD + its slot and is cast at HELD_SPELL_LEVEL; a duration is held to ROUNDS_MASK rounds. */
#define SPELL_ICON_HELD 200
#define SPELL_ICON_STRIKE 250
#define HELD_SPELL_LEVEL 8
#define ROUNDS_MASK 65535
/* A dispel or a killing effect's chance is the effect's plus (level difference) * DISPEL_PER_LEVEL, held to SPELL_CHANCE_MIN..SPELL_CHANCE_MAX (the banish rule's shape). */
#define DISPEL_PER_LEVEL 5
#define SPELL_CHANCE_MIN 5
#define SPELL_CHANCE_MAX 95
/* The class absorption flags: by day, by night; and the effect settings bit "magnitude by level". Azura's Star (artifact 9) traps a soul without the chance roll. */
#define CLASS_ABSORB_DAY 2
#define CLASS_ABSORB_NIGHT 4
#define EFFECT_SETTING_MAGNITUDE 4
#define ARTIFACT_AZURAS_STAR 9
/* A spell has SPELL_EFFECTS effects; the last group that sets a condition bit is GROUP_LAST_CONDITION, and the groups 39 and 42 have bits of their own. The message when nothing is active is WORDS_NO_EFFECTS. */
/* The sub-effects an effect type lists at most. */
#define SUB_EFFECTS_MOST 12
#define SPELL_EFFECTS 3
#define GROUP_LAST_CONDITION 46
#define GROUP_39 39
#define GROUP_42 42
#define WORDS_NO_EFFECTS 18
/* The cast animation is CAST_FRAMES frames at BIOS_HZ, done after CAST_TICKS ticks. */
#define CAST_FRAMES 6
#define CAST_TICKS 7
/* spell_area_effect: an area's radius is AREA_STRIKE_RADIUS for a strike (icon ICON_STRIKE and up), else level * AREA_PER_LEVEL + AREA_BASE, times AREA_MUL. Each creature caught is also thrown away from the burst, force radius * AREA_KNOCKBACK_MUL and amount radius (dungeon_knockback). */
#define ICON_STRIKE 250
#define AREA_STRIKE_RADIUS 168
#define AREA_PER_LEVEL 4
#define AREA_BASE 64
#define AREA_MUL 3
#define AREA_KNOCKBACK_MUL 30
/* The missile's and the burst's sprite sizes and rate (ours), the cast light's place (ahead, down). */
#define MISSILE_SIZE 48
#define BURST_SIZE 128
#define MISSILE_RATE 12
#define MISSILE_DROP 24
#define CAST_LIGHT_AHEAD 192
#define CAST_LIGHT_DOWN 18
/* spell_missile_update (func_0005C448.c): a missile flies MISSILE_SPEED a second, is launched MISSILE_LAUNCH ahead, bursts at MISSILE_RANGE from the player, hits a body within MISSILE_HIT_RADIUS and bursts for BURST_MS ms;. */
#define MISSILE_SPEED 400.0f
#define MISSILE_LAUNCH 110.0f
#define MISSILE_RANGE 2048
#define MISSILE_HIT_RADIUS 60
#define BURST_MS 600
/* The touch target: within TOUCH_REACH, TOUCH_ACROSS off the aim. */
#define TOUCH_REACH 160
#define TOUCH_ACROSS 60
/* A creature casts at range when farther than CAST_RANGED_REACH_MUL times its reach (a body 60, the player 90) and CAST_RANGED_MIN, nearer than CAST_RANGED_MAX, one frame in CAST_RANGED_ODDS (equip.c:966..983). */
#define CREATURE_REACH_BODY 60
#define CREATURE_REACH_PLAYER 90
#define CAST_RANGED_REACH_MUL 4
#define CAST_RANGED_MIN 256
#define CAST_RANGED_MAX 2048
#define CAST_RANGED_ODDS 40
#define RESIST_FULL 100
#define RESIST_BASE 50
#define RESIST_RACIAL 30
#define RESIST_MIN 5
#define RESIST_MAX 95
#define RESIST_NONE_BAND 20
#define RESIST_PARTIAL_STEP 5
/* magic_sees: a creature sees through a chameleon or shadow one time in SEE_CHANCE (SEE_SHADOW_CHANCE for a shadow), never through invisibility. */
#define SEE_CHANCE 4
#define SEE_SHADOW_CHANCE 8
/* spell_cost: each effect costs (SPELL_COST_SKILL_CEILING - the school's skill) percent of its raw cost; the target's factor halves the sum; a spell costs SPELL_COST_MIN at least. */
#define SPELL_COST_SKILL_CEILING 110
#define SPELL_COST_MIN 5
int magic_resist(int index, int element, int class_bit, int modifier);
int magic_chance(int index, int group, int sub);
void magic_apply(int spell, int caster, int target, int level, int no_resist);
/* A spell held by an enchanted item (Cast when held): a lasting effect on the player with icon 200 + the item's slot, ended by magic_release (items.md of the decomp, docs/game-enchantments.md). */
void magic_hold(int spell, int slot);
/* The same spell with a definition of the caller's (an item's strike spell is cast with icon 250). */
void magic_apply_def(const SpellDef *def, int spell, int caster, int target, int level, int no_resist);
/* damage_weapon_strike_effects, type 2: the spell cast through a weapon that struck the creature (the index of the dungeon's creatures): a spell on a touch or a range at the creature, an area one bursting at the creature (and not at the wielder, 9.4 fixed). */
void magic_strike(int spell, int victim);
void magic_creature_item_strike(int spell, int attacker, int target, int level);
void magic_release(int spell, int slot);
void magic_minutes(df_u32 start, df_u32 minutes);
void magic_restore_foe(int index);
void magic_remove(int target, int group);
void magic_hostile(void);
int magic_sees(int kind);
void magic_soul_capture(int index);
int magic_ready(df_u32 owned_spell);
/* An enchanted item's Cast when used (spell id): cast at once on the caster, else readied for the attack button; 0 when it could not be. */
int magic_cast_item(int spell);
int magic_recast(void);
void magic_abort(void);
int magic_pending(void);
int magic_visible(void);
int magic_absorb(int index, int damage);
void magic_recall_prompt(void);
void magic_open(const Scene *scene);
/* Logical projectile positions; the engine shifts their spawned art itself. */
void magic_shift(float dx, float dz);
void magic_close(void);
void magic_frame(const Scene *scene, const Camera *camera, const PlayIn *input, int blocked);
int magic_creature(int index, const Camera *camera);
int magic_creature_at(int index, int target, const Camera *camera);
void magic_trap(int spell, float x, float y, float z);
void magic_draw(void);
void magic_set_draw(void (*draw)(int element, int frame));
void magic_set_hud(void (*draw)(const MagicState *state));
void magic_light_sprite(int slot, int width, int height);
void magic_slots(const int slots[5][2]);
int magic_valid(const MagicState *state);
/* Portrait indicators: 1 enchanted, 2 symptomatic poison, 4 disease. */
int magic_indicators(void);
void magic_status(void);
void magic_commands(void);
#endif
