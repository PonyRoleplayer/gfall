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
