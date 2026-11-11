#ifndef OGFALL_MAGIC_H
#define OGFALL_MAGIC_H
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/play.h"
#include "engine/gfx/camera.h"
/* Index -1 is the player; creature identity is converted to a persistent key. */
KeepKey og_magic_key(int target);
int og_magic_index(KeepKey key);
int og_magic_target(int index, OgCharacter *who, OgClass *career, int *base);
void og_magic_commit(int index, const OgCharacter *who, int caster);
int og_spell_cost(const OgSpellDef *spell, const OgCharacter *caster);
/* One effect's raw cost, the formula of its group over its cost record and numbers (spell_cost_formula_dispatch); what the spell maker stores and prices by. */
int og_effect_cost_raw(int group, const df_i16 *c, const int *d, const int *ch, const int *m);
int og_magic_resist(int index, int element, int class_bit, int modifier);
int og_magic_chance(int index, int group, int sub);
void og_magic_apply(int spell, int caster, int target, int level, int no_resist);
/* A spell held by an enchanted item (Cast when held): a lasting effect on the player with icon 200 + the item's slot, ended by og_magic_release (items.md of the decomp, docs/ogfall-enchantments.md). */
void og_magic_hold(int spell, int slot);
/* The same spell with a definition of the caller's (an item's strike spell is cast with icon 250). */
void og_magic_apply_def(const OgSpellDef *def, int spell, int caster, int target, int level, int no_resist);
/* damage_weapon_strike_effects, type 2: the spell cast through a weapon that struck the creature (the index of the dungeon's creatures): a spell on a touch or a range at the creature, an area one bursting at the creature (and not at the wielder, 9.4 fixed). */
void og_magic_strike(int spell, int victim);
void og_magic_release(int spell, int slot);
void og_magic_minutes(df_u32 start, df_u32 minutes);
void og_magic_restore_foe(int index);
void og_magic_remove(int target, int group);
void og_magic_hostile(void);
int og_magic_sees(int kind);
void og_magic_soul_capture(int index);
int og_magic_ready(df_u32 owned_spell);
/* An enchanted item's Cast when used (spell id): cast at once on the caster, else readied for the attack button; 0 when it could not be. */
int og_magic_cast_item(int spell);
int og_magic_recast(void);
void og_magic_abort(void);
int og_magic_pending(void);
int og_magic_visible(void);
int og_magic_absorb(int index, int damage);
void og_magic_recall_prompt(void);
void og_magic_open(const Scene *scene);
/* Logical projectile positions; the engine shifts their spawned art itself. */
void og_magic_shift(float dx, float dz);
void og_magic_close(void);
void og_magic_frame(const Scene *scene, const Camera *camera, const OgIn *input, int blocked);
int og_magic_creature(int index, const Camera *camera);
void og_magic_trap(int spell, float x, float y, float z);
void og_magic_draw(void);
void og_magic_set_draw(void (*draw)(int element, int frame));
void og_magic_set_hud(void (*draw)(const OgMagicState *state));
void og_magic_light_sprite(int slot, int width, int height);
void og_magic_slots(const int slots[5][2]);
int og_magic_valid(const OgMagicState *state);
/* Portrait indicators: 1 enchanted, 2 symptomatic poison, 4 disease. */
int og_magic_indicators(void);
void og_magic_status(void);
void og_magic_commands(void);
#endif
