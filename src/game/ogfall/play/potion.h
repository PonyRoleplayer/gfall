#ifndef OGFALL_POTION_H
#define OGFALL_POTION_H

#include "engine/core/df.h"

/*
 * Potions. The recipes FALL.EXE keeps (potion_recipes: twenty of them, each its ingredients, its value and the spell it casts), the potion item, drinking it
 * (potion_drink, src/lifted/potions.c 298) and a shop's shelf of them (shop_stock_potions, equip.c 843). Slice 10 of people and law.
 *
 * A potion is a glass bottle (group 1, index 1) whose price is its recipe's value and whose recipe is kept in the item's message field, one more than the
 * recipe's number (a bottle with none is empty). Each recipe's spell is registered with the spell table under 1000 + its number, so drinking is the ordinary
 * casting of a spell on the player at the player's own level.
 */

#define OG_POTION_SPELL 1000
#define OG_POTION_GROUP 1
#define OG_POTION_INDEX 1

/* Registers every recipe's spell; after the spells are loaded. */
void og_potions_register(void);
int og_potion_count(void);
/* The recipe's name (its spell's), its value, the spell's id. */
const char *og_potion_name(int recipe);
int og_potion_value(int recipe);
int og_potion_spell(int recipe);
/* Its ingredients: how many, and the group and index of the n-th (the original's -2 ends the list). */
int og_potion_ingredients(int recipe, int *group, int *index, int cap);

/* A bottle of the recipe in a container; 0 when there is no room. */
df_u32 og_potion_create(df_u32 container, int recipe);
/* The recipe a bottle holds, -1 for an item that is no potion. */
int og_potion_of(df_u32 item);
/* inventory use of a bottle: the spell is cast on the player and the bottle gone. 1 when drunk. */
int og_potion_drink(df_u32 item);
/* shop_stock_potions: quality + 1 bottles of random recipes into a container. */
void og_potion_stock(df_u32 container, int quality);

/*
 * shop_stock_soul_traps (equip.c 495): a hall's quality / 2 + 1 soul traps at 5000, a quarter of them (a roll under 25) holding a soul of a random kind that has a
 * value, which is then added to the price. A soul is a child of the trap (OG_RECORD_SOUL, its kind the index).
 */
void og_soul_trap_stock(df_u32 container, int quality);
/* What a creature's soul is worth, by kind; 0 for none. */
int og_soul_value(int kind);

#endif
