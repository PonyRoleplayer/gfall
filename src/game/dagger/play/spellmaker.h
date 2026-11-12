#ifndef GFALL_SPELLMAKER_H
#define GFALL_SPELLMAKER_H

/*
 * The spell maker (spells.c, func_00037014, func_00038228, func_00038911 and func_000383CC of the decompilation): a spell made of up to three
 * effects, a target, an element, an icon and a name, priced in gold and added to the spellbook as a spell of its own. This is the rules; the
 * screen and its clicks are possessions.c and seam.c. The spell is the 89-byte record SPELLS.STD keeps: effect type and subtype pairs at 0, the
 * element at 6, the target at 7, three stored effect costs (shorts) at 8, durations at 14 (3 a slot), chances at 23 (3), magnitudes at 32 (5), the
 * name at 47 (25), the icon at 72.
 *
 * Kept as the original has it: the cost an effect is stored at is worked over the cost record of the last effect in the spell (the casting cost line
 * leaves that one behind), so editing an earlier effect of a spell of several prices it by a later one's numbers; a kind-0 effect has no cost
 * stored until its row is clicked. Made proper: the icon's previous arrow wraps (the original's byte goes to 255).
 */
#include "engine/core/df.h"

/* The spell's bytes. */
#define MAKER_ICONS 69
#define MAKER_NAME_AT 47
#define MAKER_NAME_LEN 24

typedef struct Maker {
    df_u8 spell[89];
    /* spell_effect_slot, and spellmaker_settings_kind: 0 for no panel, else the bits 1 duration, 2 chance, 4 magnitude the effect's numbers use. */
    int slot, kind;
} Maker;

const Maker *maker_state(void);
/* spellmaker_new: everything 1, the effects empty (255), the element magic (4) and the target the caster (0), no name. */
void maker_new(void);

/* spellmaker_allowed_targets: 2 when any target will do, 0 when the effects want the caster alone, 1 when they want touch and beyond. */
int maker_targets(void);
/* func_00037AB7: the greatest elemental level among the effects; elements other than magic are allowed above 0. */
int maker_elements(void);
/* The first empty effect slot, -1 when all three are used. */
int maker_free_slot(void);
/* spellmaker_add_effect: the effect types the targets allowed can add, by index into the effects' names; how many. */
int maker_effect_list(int *types, int cap);
/* spellmaker_pick_effect_cb: the effect goes into the first free slot and the element and target are put to what it allows. The subtypes it offers
 * (names) are counted by maker_subtype_count; with none the panel for its numbers opens (maker_state()->kind). Returns the slot, -1 when none is free. */
int maker_pick_effect(int type);
int maker_subtype_count(int type);
/* spellmaker_pick_subtype_cb: the subtype of the slot being made; its panel opens. */
void maker_pick_subtype(int sub);
/* A click on an effect's row, to edit its numbers (the panel opens; an effect without any gets its cost worked) or to delete it. */
/* The effect a row's choice is about (before edit or delete). */
void maker_select(int slot);
void maker_edit(int slot);
void maker_delete(int slot);
/* The panel's close button. */
void maker_close_panel(void);
/* spellmaker_adjust_value through its eleven handlers: 0..2 duration base, plus, per level; 3..5 chance; 6..10 magnitude base low and high, plus low and
 * high, per level. delta is +1 or -1. The spinner only answers when the effect's kind allows (as the original's test does). */
int maker_spin_allowed(int index);
void maker_adjust(int index, int delta);
/* The value of handler index for the slot being set. */
int maker_value(int index);
/* The effect cost stored for the slot being set, worked again (the panel does it every frame while open). */
void maker_recompute(void);

/* The buttons: element 0..4 (fire, frost, poison, shock, magic), target 0..4 (caster, touch, distance, area, area at distance), the icon's step. */
void maker_set_element(int element);
void maker_set_target(int target);
void maker_icon(int step);
void maker_set_name(const char *name);

/* The three numbers the window shows (gold, and spell points by spell_cost) and the price in gold: (sum of stored costs) * the target's factor * 2. */
int maker_price(void);
int maker_shown(void);
int maker_spell_points(void);

/* spellmaker_buy: the text id of what came of it (TEXT.RSC 1704 no name, 1702 not enough gold, 1709 no room, 1705 made), 0 for an empty spell, and -1 for no
 * spellbook ("You do not have a spellbook!"). A spell made is in the book and the maker begins again. */
int maker_buy(void);

#endif
