#ifndef GFALL_POTION_H
#define GFALL_POTION_H

#include "engine/core/df.h"

/*
 * Potions. The recipes FALL.EXE keeps (potion_recipes: twenty of them, each its ingredients, its value and the spell it casts), the potion item, drinking it
 * (potion_drink, src/lifted/potions.c 298) and a shop's shelf of them (shop_stock_potions, equip.c 843). Slice 10 of people and law.
 *
 * A potion is a glass bottle (group 1, index 1) whose price is its recipe's value and whose recipe is kept in the item's message field, one more than the
 * recipe's number (a bottle with none is empty). Each recipe's spell is registered with the spell table under 1000 + its number, so drinking is the ordinary
 * casting of a spell on the player at the player's own level.
 */

/* A recipe record (109 bytes in the original): its value, the 16-bit word at RECIPE_VALUE_OFFSET; its spell RECIPE_SPELL_OFFSET bytes in (our copy); RECIPE_INGREDIENTS indexes from RECIPE_INGREDIENTS_OFFSET
 * (RECIPE_NO_INGREDIENT empty) with their groups from RECIPE_GROUPS_OFFSET. In the cauldron an empty cell is CAULDRON_EMPTY and a recipe ingredient already matched RECIPE_USED. */
#define RECIPE_VALUE_OFFSET 8
#define RECIPE_SPELL_OFFSET 20
#define RECIPE_INGREDIENTS 8
#define RECIPE_NO_INGREDIENT -2
#define RECIPE_GROUPS_OFFSET 10
#define CAULDRON_EMPTY 254
#define RECIPE_USED 255
/* A recipe brews when ((potency - rarities) >> 1) + POTION_BREW_BASE >= 1; an unknown mix keeps MIX_EFFECTS_MAX effects at most. */
#define POTION_BREW_BASE 5
#define MIX_EFFECTS_MAX 3
/* The spell an unknown brew makes: element magic on the caster, a chance of MIX_CHANCE and a magnitude MIX_MAGNITUDE (the original's own odd values); the sound of mixing is SOUND_POTION_MIX. */
#define MIX_CHANCE 70
#define MIX_MAGNITUDE 10
/* A soul trap in a shop is worth SOUL_TRAP_VALUE (plus its soul's); SOUL_TRAP_FILLED_CHANCE percent hold the soul of one of SOUL_CREATURES kinds. */
#define SOUL_TRAP_VALUE 5000
#define SOUL_TRAP_FILLED_CHANCE 25
#define SOUL_CREATURES 43
/* Our packing of three effects into a potion's message: PACKED_EFFECT_BITS bits each, a type + 1 above a 4-bit subtype. */
#define PACKED_EFFECT_BITS 10
#define POTION_SPELL 1000
/* An unknown brew's spell is made fresh into this id each time one is drunk. */
#define BREW_SPELL 2999
#define POTION_GROUP 1
#define POTION_INDEX 1

/* Registers every recipe's spell; after the spells are loaded. */
void potions_register(void);
int potion_count(void);
/* The recipe's name (its spell's), its value, the spell's id. */
const char *potion_name(int recipe);
int potion_value(int recipe);
int potion_spell(int recipe);
/* Its ingredients: how many, and the group and index of the n-th (the original's -2 ends the list). */
int potion_ingredients(int recipe, int *group, int *index, int cap);

/* A bottle of the recipe in a container; 0 when there is no room. */
df_u32 potion_create(df_u32 container, int recipe);
/* The recipe a bottle holds, -1 for an item that is no potion. */
int potion_of(df_u32 item);
/* inventory use of a bottle: the spell is cast on the player and the bottle gone. 1 when drunk. */
int potion_drink(df_u32 item);
/* shop_stock_potions: quality + 1 bottles of random recipes into a container. */
void potion_stock(df_u32 container, int quality);

/*
 * shop_stock_soul_traps (equip.c 495): a hall's quality / 2 + 1 soul traps at 5000, a quarter of them (a roll under 25) holding a soul of a random kind that has a
 * value, which is then added to the price. A soul is a child of the trap (RECORD_SOUL, its kind the index).
 */
void soul_trap_stock(df_u32 container, int quality);
/* What a creature's soul is worth, by kind; 0 for none. */
int soul_value(int kind);

/*
 * The potion maker (potionmaker_*, potion_match_recipe, potion_mix_unknown: src/lifted/potions.c, src/hand/func_0008FE48.c). Ingredients are the pack's items whose
 * template has bit 0 of its flags; each carries, from its template, a key (variants) the recipes match by, a potency (rarity), and the effect it lends an unknown brew
 * (draw_order the effect, capacity the subtype). The cauldron holds eight, each different. Mixing tries the twenty recipes in order (every ingredient of the recipe matched by
 * key, and the potency left after the unmatched ones are taken off, less the recipe's own, halved and plus 5, at least 1); failing them, the effects the ingredients lend are
 * merged by effect, the strongest three kept and the weak ones dropped, and the potion is of "Unknown Powers" worth nothing.
 */
#define CAULDRON 8
typedef struct Cauldron {
    df_u32 item[CAULDRON];
    /* The key (254 for an empty cell), potency, effect and subtype of each, as the original keeps them in arrays by cell: a removed cell leaves its old numbers behind. */
    unsigned char key[CAULDRON], potency[CAULDRON], type[CAULDRON], subtype[CAULDRON];
} Cauldron;

const Cauldron *cauldron_state(void);
void cauldron_clear(void);
/* An item that can go in the cauldron (a template flagged an ingredient). */
int ingredient_at(df_u32 item);
/* potionmaker_add_ingredient: into the first free cell, refused when full or when an ingredient of its kind is in already. */
int cauldron_add(df_u32 item);
int cauldron_remove(int cell);
int cauldron_count(void);

/* The pack's ingredients not in the cauldron, in the order the original lists them; how many (ids at most cap). */
int ingredient_list(df_u32 *ids, int cap);

/* potion_match_recipe for a recipe against a cauldron: 1 when it makes it. */
int potion_match(int recipe, const Cauldron *cauldron);

enum { BREW_NOTHING, BREW_RECIPE, BREW_UNKNOWN };
/*
 * potionmaker_mix: the first recipe the cauldron makes, else an unknown brew, else nothing. A potion is made into the pack, the ingredients used up and the cauldron emptied;
 * *recipe is the recipe made, or -1 for an unknown brew.
 */
int brew_state(int *recipe, df_u32 *potion);
/* The unknown brew's effects as potion_mix_unknown merges them, from a cauldron: how many (0 for none); each effect's type and subtype. */
int brew_effects(const Cauldron *cauldron, int *type, int *subtype);

/* Recipes the player knows: items "Potion Recipe" (group 27, index 4) in the pack, the recipe a recipe item holds read as the potions' are. */
df_u32 recipe_create(df_u32 container, int recipe);
int recipe_of(df_u32 item);
int recipes_known(df_u32 *ids, int cap);
/* potion_have_recipe_ingredients and potion_load_recipe: the cauldron emptied and filled with the pack's ingredients for a recipe. 0 when one is missing. */
int recipe_load(int recipe);

#endif
