#ifndef DF_INVENTORY_H
#define DF_INVENTORY_H

#include "engine/core/df.h"

/*
 * What the player carries: items made from MAIN.DAT's templates (ITEM,
 * WEAP) in a material (MATL), and the one weapon in hand. Read
 * docs/game.md (Combat) before editing.
 *
 * An item is its template and material with what the material made of
 * them, by classic's rules as Daggerfall Unity has them (ItemBuilder's
 * SetItemPropertiesByMaterial): the value times 3 times the material's
 * value_q; the weight in quarter kilograms times weight_q / 4, rounded
 * half to even, back to kilograms; the condition times condition_q / 4.
 *
 * Weapons only, for now; armour, clothing, gold and the rest come with
 * the inventory screen (P5).
 */

#define INVENTORY_MAX 64

typedef struct Item {
    df_u16 template_id;
    df_u8 material;
    /* Kilograms. */
    float weight;
    df_u32 value;
    df_u16 condition;
    df_u16 condition_max;
} Item;

typedef struct Inventory {
    Item item[INVENTORY_MAX];
    int count;
    /* The item in hand, or -1: bare hands. */
    int weapon;
} Inventory;

/* An item of template in material, new. 0 when MAIN.DAT has no such template or material. */
int item_make(Item *it, int template_id, int material_id);
/* "Iron Longsword": its material's name and its template's. */
void item_name(const Item *it, char *out, int cap);
/* A weapon template by name, any case, spaces and dashes ignored ("dai katana"); -1 when none. */
int item_find_weapon(const char *name);
/* A material by name, as item_find_weapon; -1 when none. */
int item_find_material(const char *name);

/* Empty, bare handed. */
void inventory_clear(Inventory *inv);
/* Its index, or -1 when the pack is full. */
int inventory_add(Inventory *inv, const Item *it);
/* Takes item i out; the hand follows the item it held, or is emptied. */
void inventory_remove(Inventory *inv, int i);
/* Item i in hand (a weapon), or -1 for bare hands. 0 when i is no weapon. */
int inventory_equip(Inventory *inv, int i);
/* The item in hand, or 0. */
const Item *inventory_weapon(const Inventory *inv);
/* Every item's weight together, kilograms. */
float inventory_weight(const Inventory *inv);

#endif
