#ifndef DF_SPELLS_PLAY_H
#define DF_SPELLS_PLAY_H

#include "game/dagger/play/character.h"
#include "game/dagger/tables/magic.h"

/*
 * A spell, what it costs, and the book that holds the player's. Read
 * docs/game.md (Magic) before editing.
 *
 * A spell is up to three effects, one target, one element, a name and an
 * icon (ICON00I0.IMG's number). An effect is classic's group and subgroup
 * (magic.h's EFCT) with the settings of its duration, chance and magnitude.
 * The 89 standard spells (magic.h's SPEL) become spells by
 * spell_standard; the spell maker (play/maker.c) makes the others.
 *
 * Cost, as Daggerfall Unity works it (FormulaHelper.CalculateTotalEffectCosts,
 * docs/game.md, The cost): each supported parameter has a cost of
 * its effect's EFCT numbers, the effect's gold their sum, its spell points the
 * gold times (110 - the school's skill) / 400; a spell's cost is the sum of
 * its effects' times the target's multiplier (1, 1, 1.5, 2, 2.5), the spell
 * points at least 5. An effect that supports nothing (Teleport) costs as one
 * of 1 / 1 / 1 of (60, 100, 160).
 *
 * The book holds SPELLBOOK_MAX spells, no two alike (the same name), kept
 * as the player adds them and as they sort. Plain C: hostcheck drives it.
 */

#define SPELL_NAME MAGIC_SPELL_NAME
#define SPELLBOOK_MAX 128

typedef struct Spell {
    char name[SPELL_NAME];
    df_u8 icon;
    df_u8 element;
    df_u8 target;
    EffectSettings effect[MAGIC_SLOTS];
} Spell;

typedef struct Spellbook {
    Spell spell[SPELLBOOK_MAX];
    int count;
} Spellbook;

/* A slot with no effect: its id is MAGIC_ID(255, 255). */
#define SPELL_NO_EFFECT MAGIC_ID(255, 255)

/* A standard spell as the player's spell. */
void spell_standard(Spell *out, const MagicSpell *std);
/* How many of its slots hold an effect. */
int spell_effects(const Spell *s);

/*
 * One effect's gold, and its spell points at skill (the level of its school), by the sums above; 0 for a slot with no
 * effect or one MAIN.DAT lacks. points may be 0.
 */
int spell_effect_gold(const EffectSettings *e, int skill, int *points);
/*
 * A spell's cost for the character pc (their skill in each effect's school): its spell points,
 * and its gold in *gold (the spell maker's fee, no skill in it), either to 0 or not given.
 */
int spell_cost(const Spell *s, const Character *pc, int *gold);
/* The target's multiplier of a cost: 1, 1, 1.5, 2, 2.5 for TARGET_* in order. */
int spell_target_cost(int cost, int target);
/* 1 when the effect is one of the spell's, whatever its settings. */
int spell_has_effect(const Spell *s, int effect_id);

/* The effect's name in a line, "Damage Health": its group and its subgroup. cap bytes; the id's own number if MAIN.DAT lacks it. */
void spell_effect_name(int effect_id, char *out, int cap);

/* One more spell at the end; 0 when the book is full or one of that name is in it. */
int spellbook_add(Spellbook *b, const Spell *s);
/* The spell at i taken out, the ones after it closing up. */
void spellbook_remove(Spellbook *b, int i);
/* The index of the spell of that name, or -1. */
int spellbook_find(const Spellbook *b, const char *name);
/* Sorted by name (case folded), which classic's Sort button does. */
void spellbook_sort(Spellbook *b);
/*
 * What a new character starts with (DFU's StartingSpells): by class, the Mage and
 * the Sorcerer five, the Spellsword three and so on; a custom class with any
 * magic skill primary or major the Spellsword's. Added to b as it is.
 */
void spellbook_starting(Spellbook *b, const Character *pc);

#endif
