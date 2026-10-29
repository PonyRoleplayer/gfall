#ifndef DF_ITEMS_TABLE_H
#define DF_ITEMS_TABLE_H

#include "engine/assets/recfile.h"

/*
 * What items are, as records (docs/data.md), classic's tables from
 * inside FALL.EXE as Daggerfall Unity carries them:
 *
 *   ITEM: an item template, by its index (DFU's ItemTemplates): name,
 *         weight, value, condition, pictures. The weapons, 113..131, for
 *         now; armour and the rest join with the inventory (P5).
 *   WEAP: what a weapon template adds: its damage, its skill, and the
 *         WEAPON##.CIF its swing is drawn from.
 *   MATL: the ten materials, iron to daedric: the damage and to-hit they
 *         add, and what they do to weight, value and condition.
 *
 * Read docs/game.md (Combat) before editing.
 */

/* Template indices the code names. */
#define ITEM_DAGGER 113
#define ITEM_LONGSWORD 120
#define ITEM_ARROW 131

/* Materials (DFU's WeaponMaterialTypes). */
enum {
    MATERIAL_IRON,
    MATERIAL_STEEL,
    MATERIAL_SILVER,
    MATERIAL_ELVEN,
    MATERIAL_DWARVEN,
    MATERIAL_MITHRIL,
    MATERIAL_ADAMANTIUM,
    MATERIAL_EBONY,
    MATERIAL_ORCISH,
    MATERIAL_DAEDRIC,
    MATERIAL_COUNT
};

typedef struct ItemTemplate {
    df_u16 id;
    char name[24];
    /* Kilograms; gold. */
    float weight;
    df_u16 price;
    df_u16 hit_points;
    /* Its picture on the ground (TEXTURE.###) and in the paper doll's hand (its archive, record). */
    df_u16 world_archive;
    df_u16 world_record;
    df_u16 player_archive;
    df_u16 player_record;
    df_u8 one_handed;
    df_u8 blunt;
} ItemTemplate;

typedef struct WeaponTemplate {
    df_u16 id;
    df_u8 damage_min;
    df_u8 damage_max;
    /* SKILL_* the swing trains and is rolled with. */
    df_u8 skill;
    /* Its swing: WEAPON##.CIF's number. */
    df_u8 cif;
} WeaponTemplate;

typedef struct Material {
    df_u16 id;
    char name[12];
    /* Added to a weapon's damage; ten times it to its chance to hit. */
    signed char modifier;
    /* Weight times weight_q / 4; value times 3 * value_q; condition times condition_q / 4. */
    df_u8 weight_q;
    df_u16 value_q;
    df_u8 condition_q;
} Material;

extern const RecSchema item_schema;
extern const RecSchema weapon_schema;
extern const RecSchema material_schema;
RecTable *item_table(void);
RecTable *weapon_table(void);
RecTable *material_table(void);
const ItemTemplate *item_template(int id);
const WeaponTemplate *weapon_template(int id);
const Material *material(int id);

#endif
