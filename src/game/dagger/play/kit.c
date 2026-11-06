#include "game/dagger/play/kit.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/play/combat.h"

/* See kit.h. */

#define START_GOLD 100u
#define CLASS_ARCHER 13
#define ARCHER_ARROWS 24
/* ItemTemplates' numbers for the weapons classes start with. */
#define TANTO 114
#define STAFF 115
#define SHORTSWORD 116
#define BROADSWORD 118
#define SABER 119
#define MACE 124
#define WARHAMMER 126
#define BATTLE_AXE 127
#define LONG_BOW 130

typedef struct ClassWeapon {
    short weapon;
    short material;
} ClassWeapon;

/* By class number, the CLASS##.CFG's. */
static const ClassWeapon g_class_weapon[18] = {
    { SHORTSWORD, MATERIAL_STEEL },    /* Mage */
    { SABER, MATERIAL_IRON },          /* Spellsword */
    { SABER, MATERIAL_IRON },          /* Battlemage */
    { SHORTSWORD, MATERIAL_STEEL },    /* Sorcerer */
    { MACE, MATERIAL_IRON },           /* Healer */
    { SHORTSWORD, MATERIAL_IRON },     /* Nightblade */
    { SHORTSWORD, MATERIAL_IRON },     /* Bard */
    { TANTO, MATERIAL_STEEL },         /* Burglar */
    { SABER, MATERIAL_IRON },          /* Rogue */
    { SHORTSWORD, MATERIAL_IRON },     /* Acrobat */
    { SHORTSWORD, MATERIAL_IRON },     /* Thief */
    { ITEM_LONGSWORD, MATERIAL_IRON }, /* Assassin */
    { STAFF, MATERIAL_STEEL },         /* Monk */
    { LONG_BOW, MATERIAL_IRON },       /* Archer */
    { BATTLE_AXE, MATERIAL_STEEL },    /* Ranger */
    { WARHAMMER, MATERIAL_IRON },      /* Barbarian */
    { BROADSWORD, MATERIAL_IRON },     /* Warrior */
    { ITEM_LONGSWORD, MATERIAL_IRON }  /* Knight */
};

/* The shirt and pants a new character is dressed in, worn. */
static void dress(int female, Inventory *pack)
{
    Item it;
    int shirt;
    int pants;
    int look;

    shirt = female ? ITEM_WOMENS_SHIRT : ITEM_MENS_SHIRT;
    pants = female ? ITEM_WOMENS_PANTS : ITEM_MENS_PANTS;
    if (item_make_clothing(&it, shirt, 0)) {
        inventory_wear(pack, inventory_add(pack, &it));
    }
    /* The pants' look is rolled, among as many as the template has. */
    look = item_template(pants) && item_template(pants)->variants ? combat_roll(item_template(pants)->variants) : 0;
    if (item_make_clothing(&it, pants, look)) {
        inventory_wear(pack, inventory_add(pack, &it));
    }
}

void kit_give(int klass, int female, Inventory *pack)
{
    Item it;

    inventory_clear(pack);
    pack->gold = START_GOLD;
    /* A class of the player's own: an iron longsword (AssignStartingGear). */
    if (klass < 0 || klass >= 18) {
        if (item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON)) {
            inventory_equip(pack, inventory_add(pack, &it));
        }
        dress(female, pack);
        return;
    }
    if (item_make(&it, g_class_weapon[klass].weapon, g_class_weapon[klass].material)) {
        inventory_equip(pack, inventory_add(pack, &it));
    }
    if (klass == CLASS_ARCHER) {
        if (item_make(&it, BATTLE_AXE, MATERIAL_STEEL)) {
            inventory_add(pack, &it);
        }
        if (item_make(&it, ITEM_ARROW, MATERIAL_IRON)) {
            it.count = ARCHER_ARROWS;
            inventory_add(pack, &it);
        }
    }
    dress(female, pack);
}
