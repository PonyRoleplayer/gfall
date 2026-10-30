#ifndef DF_INVENTORY_H
#define DF_INVENTORY_H

#include "engine/core/df.h"

/*
 * What someone carries: items made from MAIN.DAT's templates (ITEM, WEAP,
 * ARMR) in a material (MATL), what of them is worn, and gold. Read
 * docs/game.md (Loot and the inventory) before editing.
 *
 * An item is its template and material with what the material made of
 * them, by classic's rules as Daggerfall Unity has them (ItemBuilder's
 * SetItemPropertiesByMaterial and ApplyArmorMaterial): the value times the
 * material's value_mul and value_q; the weight in quarter kilograms times
 * weight_q / 4, rounded half to even, back to kilograms; the condition
 * times condition_q / 4.
 *
 * This is not the engine's generic pack (scene/pack.h). A Daggerfall item's
 * weight and value belong to the one item, not to its kind, its armour
 * goes by body part, and it has more places to be worn than a pack has
 * slots.
 *
 * Weapons, armour and gold, for now. A one-handed weapon goes in the first
 * free hand, and one hand or the other is the one in use (DFU's WeaponManager):
 * a shield in the left keeps the right in use. Clothing, jewellery and the
 * rest come when they mean something.
 */

#define INVENTORY_MAX 64

/* Where a thing is worn: one each of these. */
enum {
    EQUIP_HEAD,
    EQUIP_RIGHT_ARM,
    EQUIP_LEFT_ARM,
    EQUIP_CHEST,
    EQUIP_GLOVES,
    EQUIP_RIGHT_HAND,
    EQUIP_LEFT_HAND,
    EQUIP_LEGS,
    EQUIP_FEET,
    EQUIP_COUNT
};

typedef struct Item {
    df_u16 template_id;
    df_u8 material;
    /* Which of its template's paper doll pictures it shows. */
    df_u8 variant;
    /* Kilograms, each. */
    float weight;
    df_u32 value;
    df_u16 condition;
    df_u16 condition_max;
    /* How many: arrows and a body's gold come in stacks, anything else is 1. */
    df_u16 count;
} Item;

typedef struct Inventory {
    Item item[INVENTORY_MAX];
    int count;
    /* Each place's item, or -1. */
    int worn[EQUIP_COUNT];
    df_u32 gold;
    /* 1 when the left hand is the one in use. */
    int use_left;
} Inventory;

/* An item of template in material, new. 0 when MAIN.DAT has no such template or material, or the template does not come in it. */
int item_make(Item *it, int template_id, int material_id);
/* "Iron Longsword": its material's name and its template's; gold and arrows by their template's alone. */
void item_name(const Item *it, char *out, int cap);
/* A weapon template by name, any case, spaces and dashes ignored ("dai katana"); -1 when none. */
int item_find_weapon(const char *name);
/* A weapon or armour template by name, as item_find_weapon; -1 when none. */
int item_find_gear(const char *name);
/* A material by name, as item_find_weapon; -1 when none. */
int item_find_material(const char *name);
/* The lowest and highest variant this armour can show in its material; 0, 0 for anything else. */
void item_variants(const Item *it, int *low, int *high);
/*
 * The TEXTURE.### record that shows item it on a paper doll and in a
 * list. Armour goes by who wears it, its archive by sex and body shape
 * (ItemBuilder.ApplyArmorSettings). A thing with no such picture, gold and
 * arrows, shows its picture on the ground. 0 when there is none.
 */
int item_picture(const Item *it, int race, int female, int *archive, int *record);

/* The metal its picture is recoloured to (cif_metal), or -1: leather, chain, gold and arrows keep theirs. */
int item_metal(const Item *it);

/* Empty, wearing nothing, no gold. */
void inventory_clear(Inventory *inv);
/* Its index, or -1 when the pack is full. */
int inventory_add(Inventory *inv, const Item *it);
/* Takes item i out; what is worn stays worn, and item i comes off first if it was. */
void inventory_remove(Inventory *inv, int i);
/* Item i in hand (a weapon), or -1 for bare hands. 0 when i is no weapon. */
int inventory_equip(Inventory *inv, int i);
/*
 * Item i worn where it goes, whatever was there coming off. A two-handed
 * weapon takes the left hand's shield off, and a shield a two-handed
 * weapon. 0 when it cannot be worn, a broken thing among them.
 */
int inventory_wear(Inventory *inv, int i);
/* Off whatever is worn there, into the pack. */
void inventory_take_off(Inventory *inv, int place);
/* Where item i is worn (EQUIP_*), or -1. */
int inventory_worn_at(const Inventory *inv, int i);
/* The other hand in use. 0, with nothing changed, when a shield in the left keeps it to the right. */
int inventory_swap_hands(Inventory *inv);
/* The place of the hand in use: EQUIP_RIGHT_HAND or EQUIP_LEFT_HAND. */
int inventory_hand_place(const Inventory *inv);
/* The weapon in the hand in use, or 0: bare hands. */
const Item *inventory_weapon(const Inventory *inv);
/* The same, to change: its condition wearing down. */
Item *inventory_hand(Inventory *inv);
/*
 * Condition lost to a blow of this much damage (FormulaHelper's
 * ApplyConditionDamageThroughPhysicalHit): (10 x damage + 50) / 100, and with
 * none, a roll of 0..99 under 20 is 1. 1 when the item is broken now.
 */
int item_wear_down(Item *it, int damage, int roll);
/* 1 when it has worn out: condition gone from what had some. Broken things cannot be worn. */
int item_broken(const Item *it);
/* What takes a blow at a body part (BODY_*): a shield worn that covers it, else the armour on it. Its place (EQUIP_*), or -1. */
int inventory_armor_at_part(const Inventory *inv, int part);
/* The pack's stack of arrows, or -1. */
int inventory_arrows(const Inventory *inv);
/* One arrow gone, the stack with it when it was the last. 0 when there were none. */
int inventory_spend_arrow(Inventory *inv);
/* Every item's weight together and the gold's, kilograms. */
float inventory_weight(const Inventory *inv);
/*
 * Each body part's armour value from what is worn: 100 bare, lower is
 * better. Armour takes five times its material's armour off its part; a
 * shield five times its own off each part it covers (DaggerfallEntity's
 * UpdateEquippedArmorValues). out has BODY_PARTS values.
 */
void inventory_armor(const Inventory *inv, int *out);

#endif
