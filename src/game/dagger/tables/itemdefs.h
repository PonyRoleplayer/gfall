#ifndef OGFALL_ITEMDEFS_H
#define OGFALL_ITEMDEFS_H

/* FALL.EXE's item templates, group maps, equipment slots and loot tables.
 * Values are extracted from the byte-perfect image; see tools/ogfall-data.py
 * and docs/ogfall.md. No dependency on GFALL's rules. */
#include "engine/core/df.h"

typedef struct OgItemDef {
    char name[25];
    int weight, condition, value;
    int variants, draw_order, flags, dropped_image, inventory_image;
} OgItemDef;

const OgItemDef *og_item_def(int group, int index);
/* A template's rarity and capacity (FALL.EXE's +0x28 and +0x1E): 0 for an item that is none. */
int og_item_rarity(int group, int index);
int og_item_capacity(int group, int index);
/* The template (FALL.EXE's 288) an item of this group and index is made from, -1 for none. */
int og_item_template(int group, int index);
int og_item_kinds(int group);
int og_item_slot(int group, int index);
int og_body_shape(int race);
int og_start_spell(int klass, int index);
int og_loot_group(int category);
int og_dungeon_loot(int type);
const df_u16 *og_loot_table(int index);
int og_material_share(int index);
int og_material_value(int index);
int og_material_weight(int index);
int og_material_condition(int index);
const char *og_material_name(int index);
int og_book_id(int index);
int og_book_count(void);
int og_book_rarity(int index);
int og_group_tab(int group);
int og_weapon_sound(int index);
int og_weapon_damage(int index, int maximum);
const char *og_condition_name(int condition, int maximum);
int og_effect_text(int group, int subtype);
const char *og_effect_subtype(int group, int subtype);
int og_weapon_proficiency(int index);
int og_armor_base(int type);
int og_armor_slot(int part);
int og_shield_covers(int index, int part);
int og_material_hit(int material);
int og_item_ink(int color, int index);
int og_clothing_dye(int special, int roll);
int og_clothing_next(int group, int index, int record);
int og_clothing_first(int group, int record);
const char *og_effect_name(int group);
#endif
