#ifndef DF_ITEMINFO_H
#define DF_ITEMINFO_H

#include "game/dagger/play/inventory.h"

/*
 * The inventory's info box for an item, in classic's own lines: TEXT.RSC's records with their item
 * macros filled in (Daggerfall Unity's ItemHelper.GetItemInfo and MacroHelper). Read docs/game.md
 * (Loot and the inventory) before editing.
 *
 *   1000 armour           "%mat %arm" / "%mod to armor rating" / "Condition: %qua" / "Weight: %kg kilograms"
 *   1014 a helm or shield (the same without the material, as classic and DFU's default do)
 *   1001 a weapon         "%mat %wep" / "%wdm points of damage" / Condition / Weight
 *   1011 an arrow         "%wep" / Weight
 *   1007 a letter of credit  "%it" / "Worth: %wth gold" / Weight
 *   1003 anything else (clothing, a horse, gold, a parcel): "%it" / Condition / Worth / Weight
 *
 * The macros: %mat the material, %arm %wep %it the template's name, %mod the armour a piece gives (a shield's
 * own, else its material's), %wdm the damage as "low-high" with the material's modifier, %qua the condition in
 * words, %kg the weight of all of a stack (trailing zeros dropped), %wth its worth. A thing with no condition (gold, a horse)
 * has no Condition line. The condition's words are ours (new, almost new, slightly used, used, worn, battered,
 * useless, by a fifth of what it was made with); classic's thresholds were not found.
 *
 * Plain C over inventory.h and textrsc.h: hostcheck runs it.
 */

/* The TEXT.RSC record that is item it's info. */
int iteminfo_record(const Item *it);
/*
 * Its lines into out, each line_chars bytes (a line cut to fit), at most max of them: how many. A record that does
 * not read leaves the name and the weight, as before TEXT.RSC.
 */
int iteminfo_lines(const Item *it, char *out, int line_chars, int max);

#endif
