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
int og_magic_resist(int index, int element, int class_bit, int modifier);
int og_magic_chance(int index, int group, int sub);
void og_magic_apply(int spell, int caster, int target, int level, int no_resist);
void og_magic_minutes(df_u32 start, df_u32 minutes);
void og_magic_restore_foe(int index);
void og_magic_remove(int target, int group);
void og_magic_hostile(void);
int og_magic_sees(int kind);
void og_magic_soul_capture(int index);
int og_magic_ready(df_u32 owned_spell);
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
