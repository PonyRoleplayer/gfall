#ifndef GFALL_ITEMDEFS_H
#define GFALL_ITEMDEFS_H

/* FALL.EXE's item templates, group maps, equipment slots and loot tables.
 * Values are extracted from the byte-perfect image; see tools/gfall-data.py
 * and docs/game.md. No dependency on GFALL's rules. */
#include "engine/core/df.h"

typedef struct ItemDef {
    char name[25];
    int weight, condition, value;
    int variants, draw_order, flags, dropped_image, inventory_image;
} ItemDef;


/*
 * The item groups of FALL.EXE (an item's group, with its index inside the group), in Daggerfall Unity's ItemGroups order. The ingredient groups 15..22 have no name here.
 * A name is for the groups the game's rules name; the numbers are the data's.
 */
enum {
    ITEM_GROUP_DRUGS = 0,
    ITEM_GROUP_USELESS = 1,
    ITEM_GROUP_ARMOR = 2,
    ITEM_GROUP_WEAPON = 3,
    ITEM_GROUP_MAGIC_ITEM = 4,
    ITEM_GROUP_ARTIFACT = 5,
    ITEM_GROUP_MENS_CLOTHING = 6,
    ITEM_GROUP_BOOK = 7,
    ITEM_GROUP_FURNITURE = 8,
    ITEM_GROUP_CURIO = 9,
    ITEM_GROUP_RELIGIOUS = 10,
    ITEM_GROUP_MAP = 11,
    ITEM_GROUP_WOMENS_CLOTHING = 12,
    ITEM_GROUP_PAINTING = 13,
    ITEM_GROUP_GEM = 14,
    ITEM_GROUP_TRANSPORT = 23,
    ITEM_GROUP_DEED = 24,
    ITEM_GROUP_JEWELRY = 25,
    ITEM_GROUP_QUEST_ITEM = 26,
    ITEM_GROUP_MISC = 27,
    ITEM_GROUP_CURRENCY = 28
};
/* Indexes in ITEM_GROUP_MISC: the spellbook, a soul trap, a letter of credit. In ITEM_GROUP_WEAPON: the two bows and the arrow. */
enum { MISC_SPELLBOOK = 0, MISC_SOUL_TRAP = 1, MISC_LETTER_OF_CREDIT = 2, MISC_HOUSE_DEED = 6 };
enum { WEAPON_SHORT_BOW = 16, WEAPON_LONG_BOW = 17, WEAPON_ARROW = 18 };
enum { MENS_KHAJIIT_SUIT = 15, WOMENS_KHAJIIT_SUIT = 11 };

/*
 * Making an item (item_roll_material, item_roll_armor_type, item_roll_dye, item_init_from_template; equip.c leaves the numbers inline). A material is a roll of 0..MATERIAL_ROLL_MAX, plus
 * (level - MATERIAL_LEVEL_PIVOT) times MATERIAL_LEVEL_LOW_MUL below the pivot or MATERIAL_LEVEL_HIGH_MUL above, held to 0..MATERIAL_ROLL_CLAMP, walked down the MATERIALS-long share table; the
 * item's colour is MATERIAL_COLOR_BASE + the material (ITEM_COLOR_PLAIN with none), its value times MATERIAL_VALUE_MUL times the table's, its weight and condition the table's over MATERIAL_SCALE.
 * A picture is a record of IMAGE_ARCHIVE_STRIDE in its archive; a male armour picture is ARMOR_MALE_OFFSET archives on. Armour is leather under ARMOR_LEATHER_CHANCE, chain under
 * ARMOR_CHAIN_CHANCE, else plate (ARMOR_LEATHER, ARMOR_CHAIN, ARMOR_PLATE; leather weighs half). Clothing is dyed unless a roll of 0..100 is under DYE_KEEP_CHANCE, in colours
 * DYE_COLOR_MIN..MAX; a few styles take their dye from a table (MENS_DYED_FIRST and the next, WOMENS_DYED, MENS_DYED_OTHER) and a few start one picture on (MENS_EXTRA_STYLE and the next,
 * WOMENS_EXTRA_STYLE and the next). A recipe (MISC_RECIPE) is one of RECIPES; an arrow stack holds ARROW_STACK_MIN..ARROW_STACK_ROLL_MAX; a book is worth BOOK_VALUE_MIN and up to
 * BOOK_VALUE_SPREAD - 1 more.
 */
#define MATERIALS 10
#define MATERIAL_ROLL_MAX 255
#define MATERIAL_LEVEL_PIVOT 10
#define MATERIAL_LEVEL_LOW_MUL 4
#define MATERIAL_LEVEL_HIGH_MUL 2
#define MATERIAL_ROLL_CLAMP 256
#define MATERIAL_COLOR_BASE 16
#define MATERIAL_VALUE_MUL 3
#define MATERIAL_SCALE 4
#define ITEM_COLOR_PLAIN 18
#define IMAGE_ARCHIVE_STRIDE 128
#define ARMOR_MALE_OFFSET 4
#define ARMOR_LEATHER_CHANCE 70
#define ARMOR_CHAIN_CHANCE 90
/* The armour pieces' indexes in ITEM_GROUP_ARMOR; from ARMOR_SHIELD_FIRST the shields (buckler, round, kite, tower). */
enum { ARMOR_CUIRASS = 0, ARMOR_GAUNTLETS = 1, ARMOR_GREAVES = 2, ARMOR_LEFT_PAULDRON = 3, ARMOR_RIGHT_PAULDRON = 4, ARMOR_HELM = 5, ARMOR_BOOTS = 6, ARMOR_SHIELD_FIRST = 7 };
#define ARMOR_PIECES 7
#define ARMOR_LEATHER 0
#define ARMOR_CHAIN 1
#define ARMOR_PLATE 2
#define DYE_KEEP_CHANCE 25
#define DYE_COLOR_MIN 1
#define DYE_COLOR_MAX 10
#define MENS_DYED_FIRST 10
#define WOMENS_DYED 8
#define MENS_DYED_OTHER 4
#define MENS_EXTRA_STYLE 13
#define WOMENS_EXTRA_STYLE 9
#define MISC_RECIPE 4
#define RECIPES 20
#define ARROW_STACK_MIN 1
#define ARROW_STACK_ROLL_MAX 20
#define BOOK_VALUE_MIN 300
#define BOOK_VALUE_SPREAD 501

const ItemDef *item_def(int group, int index);
/* A template's rarity and capacity (FALL.EXE's +0x28 and +0x1E): 0 for an item that is none. */
int item_rarity(int group, int index);
int item_capacity(int group, int index);
/* The template (FALL.EXE's 288) an item of this group and index is made from, -1 for none. */
int template_index(int group, int index);
int item_kinds(int group);
int item_slot(int group, int index);
int body_shape(int race);
int start_spell(int klass, int index);
int loot_group(int category);
int dungeon_loot_kind(int type);
const df_u16 *item_loot_list(int index);
int material_share(int index);
int material_value(int index);
int material_weight(int index);
int material_condition(int index);
const char *material_name(int index);
int book_id(int index);
int book_count(void);
int book_rarity(int index);
int group_tab(int group);
int weapon_sound(int index);
int weapon_damage(int index, int maximum);
const char *condition_name(int condition, int maximum);
int effect_text(int group, int subtype);
const char *effect_subtype(int group, int subtype);
int weapon_proficiency(int index);
int armor_base(int type);
int armor_slot(int part);
int shield_covers(int index, int part);
int material_hit(int material);
int item_ink(int color, int index);
int clothing_dye(int special, int roll);
int clothing_next(int group, int index, int record);
int clothing_first(int group, int record);
const char *effect_name(int group);
#endif
