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
enum { MISC_SPELLBOOK = 0, MISC_SOUL_TRAP = 1, MISC_LETTER_OF_CREDIT = 2 };
enum { WEAPON_SHORT_BOW = 16, WEAPON_LONG_BOW = 17, WEAPON_ARROW = 18 };

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
