#ifndef GFALL_MAGIC_H
#define GFALL_MAGIC_H
#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/play.h"
#include "engine/gfx/camera.h"
/* Index -1 is the player; creature identity is converted to a persistent key. */
KeepKey magic_key(int target);
int magic_index(KeepKey key);
int magic_target(int index, CharacterState *who, ClassInfo *career, int *base);
void magic_commit(int index, const CharacterState *who, int caster);
int spell_cost(const SpellDef *spell, const CharacterState *caster);
/* One effect's raw cost, the formula of its group over its cost record and numbers (spell_cost_formula_dispatch); what the spell maker stores and prices by. */
int effect_cost_raw(int group, const df_i16 *c, const int *d, const int *ch, const int *m);
int magic_resist(int index, int element, int class_bit, int modifier);
int magic_chance(int index, int group, int sub);
void magic_apply(int spell, int caster, int target, int level, int no_resist);
/* A spell held by an enchanted item (Cast when held): a lasting effect on the player with icon 200 + the item's slot, ended by magic_release (items.md of the decomp, docs/game-enchantments.md). */
void magic_hold(int spell, int slot);
/* The same spell with a definition of the caller's (an item's strike spell is cast with icon 250). */
void magic_apply_def(const SpellDef *def, int spell, int caster, int target, int level, int no_resist);
/* damage_weapon_strike_effects, type 2: the spell cast through a weapon that struck the creature (the index of the dungeon's creatures): a spell on a touch or a range at the creature, an area one bursting at the creature (and not at the wielder, 9.4 fixed). */
void magic_strike(int spell, int victim);
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
