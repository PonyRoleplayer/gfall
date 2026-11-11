#ifndef OGFALL_ITEMMAKER_H
#define OGFALL_ITEMMAKER_H

/*
 * The item maker's rules (itemmakr.c, func_000576FF, func_00057147, func_00057F8B and func_00058A31 of the decompilation; docs/item-making-math.md there,
 * docs/ogfall-itemmaker.md here for which of the original's bugs are fixed): ten slots of powers and side effects on one item, what each costs in the item's
 * enchantment points and in gold, what an item may be given, and the item made. The screen is stage 2.
 *
 * A slot is a power (kind 0, type 0..14) or a side effect (kind 1, type 0..10, the item's type 15..25). A parameter is a list index (the three spell lists, the
 * labels), a creature id (a soul) or -1 for none. Each slot carries five exclusion pairs {type, parameter} in the item's numbering (powers 0..14, side
 * effects 15..25; parameter 255 for any), and rows 10 and 11 hold a soul's rule-outs of spells (Cast when used, Cast when held).
 */
#include "game/ogfall/play/state.h"

typedef struct OgItemMaker {
    df_u32 item;
    int kind[10], type[10], param[10], soul[10];
    df_u8 pair[12][5][2];
    /* The slot waiting for its parameter, -1 for none. */
    int pending;
} OgItemMaker;

/* TEXT.RSC ids the rules answer with (a negative number is a text: 1650 not enough gold, 1651 too many points, 1652 enchanted, 1653 no item, 1657 and 1658 no free slot, 1659 no points, 1660 enchanted already). */
#define OG_IM_NO_GOLD 1650
#define OG_IM_NO_POINTS 1651
#define OG_IM_DONE 1652
#define OG_IM_NO_ITEM 1653
#define OG_IM_NO_POWER_SLOT 1657
#define OG_IM_NO_SIDE_SLOT 1658
#define OG_IM_CANNOT 1659
#define OG_IM_ENCHANTED 1660

const OgItemMaker *og_itemmaker(void);
void og_itemmaker_reset(void);
/* The player's captured souls as creature ids, at most 63 (the soul traps; set by the game). */
void og_itemmaker_set_souls(int (*list)(int *creatures, int cap));

/* An item's enchantment points: its template's, scaled by its material when it rolled one (weapons but arrows, plate armour). 0 for an item that cannot be enchanted. */
int og_itemmaker_capacity(df_u32 item);
/* The row of an item's enchantments, 0 for an item without. */
const OgItemEnchant *og_itemmaker_row(df_u32 item);
/* itemmaker_pick_item: 0 when the item is taken (the slots are cleared if it is another item), else the text id (1659 no points, 1660 enchanted already). */
int og_itemmaker_pick(df_u32 item);

int og_itemmaker_free_slots(void);
/* The powers (side 0, 0..14) or side effects (side 1, 0..10) that can be added now: itemmaker_show_list's rules, and a kind with no parameter left is not offered. */
int og_itemmaker_kinds(int side, int *kinds, int cap);
/* A kind's parameter kind: 0 none, 1..3 spells, 4 souls, 100 and over labels; the count of what it has; and the label of a parameter. */
int og_itemmaker_param_kind(int side, int kind);
int og_itemmaker_param_count(int side, int kind);
const char *og_itemmaker_name(int side, int kind);
/* The name of a parameter as a slot shows it (spell, label, creature). */
const char *og_itemmaker_param_name(int side, int kind, int param);

/* itemmaker_add_power_cb / itemmaker_add_side_effect_cb: the kind goes into the first free slot (returned) or a negative text id when there is none. A kind with a parameter
 * waits (pending) for og_itemmaker_choose. */
int og_itemmaker_begin(int side, int kind);
/* The parameters the pending slot can take (itemmaker_show_param_list: its pairs, the groups of reputation), as list indexes or, for a soul, creature ids. */
int og_itemmaker_params(int *params, int cap);
/* The parameter chosen: the pairs are written, and a soul's powers are added. 0, or a negative number when a soul's powers do not fit (the soul stays bound with none). */
int og_itemmaker_choose(int param);
/* Escape on the parameters: the half-made slot is taken out again. */
void og_itemmaker_cancel(void);
/* itemmaker_remove_slot: a slot taken out; a soul-granted one only goes with its soul. */
void og_itemmaker_remove(int slot);

/* The cost of a slot kind and type and parameter, in points (as_points) or as gold is taken (the sum of the powers', times 10); the totals. */
int og_itemmaker_points_used(void);
int og_itemmaker_gold(void);
/* itemmaker_enchant: the text id of what came of it (1653 no item, 1651 too many points, 1650 not enough gold, 1652 done, 0 nothing chosen). The gold is paid, the item's
 * enchantments kept, the soul bound consumed through consume(creature), and the maker cleared. */
int og_itemmaker_enchant(void (*consume)(int creature));

/* enchant_item_value: what an item's powers cost, 0 for an item with none. */
int og_itemmaker_value(df_u32 item);
/* enchant_powers_text: "Powers unknown." or a line for each enchantment. */
int og_itemmaker_text(df_u32 item, char *out, int cap);

/* Whether an item holds enchantments: one made at the item maker or a guild's magic item. */
int og_item_enchanted(df_u32 item);
/* The enchantments of any enchanted item as lines (enchant_powers_text), for the item's second box (TEXT.RSC 1016). */
int og_item_powers_text(df_u32 item, char *out, int cap);
/* inv_refresh_magic_values: the value of each item made at the item maker is its powers' cost, worked again when the inventory opens. */
void og_itemmaker_refresh_values(void);

#endif
