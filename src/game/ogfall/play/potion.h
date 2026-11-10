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
/* An unknown brew's spell is made fresh into this id each time one is drunk. */
#define OG_BREW_SPELL 2999
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

/*
 * The potion maker (potionmaker_*, potion_match_recipe, potion_mix_unknown: src/lifted/potions.c, src/hand/func_0008FE48.c). Ingredients are the pack's items whose
 * template has bit 0 of its flags; each carries, from its template, a key (variants) the recipes match by, a potency (rarity), and the effect it lends an unknown brew
 * (draw_order the effect, capacity the subtype). The cauldron holds eight, each different. Mixing tries the twenty recipes in order (every ingredient of the recipe matched by
 * key, and the potency left after the unmatched ones are taken off, less the recipe's own, halved and plus 5, at least 1); failing them, the effects the ingredients lend are
 * merged by effect, the strongest three kept and the weak ones dropped, and the potion is of "Unknown Powers" worth nothing.
 */
#define OG_CAULDRON 8
typedef struct OgCauldron {
    df_u32 item[OG_CAULDRON];
    /* The key (254 for an empty cell), potency, effect and subtype of each, as the original keeps them in arrays by cell: a removed cell leaves its old numbers behind. */
    unsigned char key[OG_CAULDRON], potency[OG_CAULDRON], type[OG_CAULDRON], subtype[OG_CAULDRON];
} OgCauldron;

const OgCauldron *og_cauldron(void);
void og_cauldron_clear(void);
/* An item that can go in the cauldron (a template flagged an ingredient). */
int og_ingredient(df_u32 item);
/* potionmaker_add_ingredient: into the first free cell, refused when full or when an ingredient of its kind is in already. */
int og_cauldron_add(df_u32 item);
int og_cauldron_remove(int cell);
int og_cauldron_count(void);

/* The pack's ingredients not in the cauldron, in the order the original lists them; how many (ids at most cap). */
int og_ingredients(df_u32 *ids, int cap);

/* potion_match_recipe for a recipe against a cauldron: 1 when it makes it. */
int og_potion_match(int recipe, const OgCauldron *cauldron);

enum { OG_BREW_NOTHING, OG_BREW_RECIPE, OG_BREW_UNKNOWN };
/*
 * potionmaker_mix: the first recipe the cauldron makes, else an unknown brew, else nothing. A potion is made into the pack, the ingredients used up and the cauldron emptied;
 * *recipe is the recipe made, or -1 for an unknown brew.
 */
int og_brew(int *recipe, df_u32 *potion);
/* The unknown brew's effects as potion_mix_unknown merges them, from a cauldron: how many (0 for none); each effect's type and subtype. */
int og_brew_effects(const OgCauldron *cauldron, int *type, int *subtype);

/* Recipes the player knows: items "Potion Recipe" (group 27, index 4) in the pack, the recipe a recipe item holds read as the potions' are. */
df_u32 og_recipe_create(df_u32 container, int recipe);
int og_recipe_of(df_u32 item);
int og_recipes_known(df_u32 *ids, int cap);
/* potion_have_recipe_ingredients and potion_load_recipe: the cauldron emptied and filled with the pack's ingredients for a recipe. 0 when one is missing. */
int og_recipe_load(int recipe);

#endif
