#ifndef DF_ITEMS_TABLE_H
#define DF_ITEMS_TABLE_H

#include "engine/assets/recfile.h"

/*
 * What items are, as records (docs/data.md), classic's tables from
 * inside FALL.EXE as Daggerfall Unity carries them:
 *
 *   ITEM: an item template, by its index (DFU's ItemTemplates): name,
 *         weight, value, condition, pictures. The armour 102..112, the
 *         weapons 113..131 and gold 276 for now; clothing, ingredients
 *         and the rest join when something uses them.
 *   WEAP: what a weapon template adds: its damage, its skill, and the
 *         WEAPON##.CIF its swing is drawn from.
 *   ARMR: what an armour template adds: the body part it covers or the
 *         parts a shield covers, and which paper doll variants it can wear.
 *   MATL: the twelve materials, iron to daedric and then leather and
 *         chain, which only armour comes in: the damage and to-hit they
 *         add, the armour they give, and what they do to weight, value
 *         and condition.
 *   LOOT: classic's loot tables, by the letter a creature names (ENEM's
 *         loot): gold, and each kind of thing's chance.
 *   LDUN: which of those a dungeon of each type fills its treasure from.
 *
 * Read docs/game.md (Combat) before editing.
 */

/* Template indices the code names. */
#define ITEM_CUIRASS 102
#define ITEM_GAUNTLETS 103
#define ITEM_GREAVES 104
#define ITEM_LEFT_PAULDRON 105
#define ITEM_RIGHT_PAULDRON 106
#define ITEM_HELM 107
#define ITEM_BOOTS 108
#define ITEM_BUCKLER 109
#define ITEM_ROUND_SHIELD 110
#define ITEM_KITE_SHIELD 111
#define ITEM_TOWER_SHIELD 112
#define ITEM_DAGGER 113
#define ITEM_LONGSWORD 120
#define ITEM_ARROW 131
#define ITEM_LETTER 275
#define ITEM_GOLD 276

/*
 * A body's seven parts, each with its own armour value, by DFU's BodyParts.
 * A blow lands on one of them; a piece of armour covers one.
 */
enum {
    BODY_HEAD,
    BODY_RIGHT_ARM,
    BODY_LEFT_ARM,
    BODY_CHEST,
    BODY_HANDS,
    BODY_LEGS,
    BODY_FEET,
    BODY_PARTS
};

/*
 * Materials: DFU's WeaponMaterialTypes in its order, then the two only
 * armour comes in (its ArmorMaterialTypes' Leather and Chain).
 */
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
    MATERIAL_LEATHER,
    MATERIAL_CHAIN,
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
    /* Paper doll layers go on lowest first (DFU's drawOrderOrEffect). */
    df_u8 draw_order;
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

/*
 * What an armour template adds. A shield's armour comes from its own
 * value, whatever its material (classic's rule), over every part in its
 * mask; any other piece's comes from its material, over its one part.
 *
 * The variant ranges say which paper doll pictures a piece can show in
 * each class of material: a piece's picture is its template's
 * player_record plus its variant. A range is low..high, both in.
 */
typedef struct ArmorTemplate {
    df_u16 id;
    /* BODY_* it covers; a shield's is ignored. */
    df_u8 part;
    /* A shield's armour, 0 for any other piece, and the parts it covers, a bit each (1 << BODY_*). */
    df_u8 shield;
    df_u8 shield_parts;
    df_u8 plate_low;
    df_u8 plate_high;
    df_u8 leather_low;
    df_u8 leather_high;
    df_u8 chain_low;
    df_u8 chain_high;
} ArmorTemplate;

typedef struct Material {
    df_u16 id;
    char name[12];
    /* Added to a weapon's damage; ten times it to its chance to hit. */
    signed char modifier;
    /* Weight times weight_q / 4; value times value_mul * value_q; condition times condition_q / 4. */
    df_u8 weight_q;
    df_u16 value_q;
    df_u8 condition_q;
    /* 3 for a metal, 1 for leather, 2 for chain (ItemBuilder.ApplyArmorMaterial). */
    df_u8 value_mul;
    /* What a piece of armour in it gives (GetMaterialArmorValue): a body part's value goes down five times this. */
    df_u8 armor;
} Material;

/* Which loot table a dungeon of one type fills its treasure from (LootTables' keys by location). */
typedef struct DungeonLoot {
    df_u16 id;
    /* Its table's letter. */
    df_u8 letter;
} DungeonLoot;

/*
 * One loot table (LootTables.DefaultLootTables). Gold is a roll between
 * the two, times the player's level. The rest are percent chances: each
 * success rolls again at half the chance. Only armour and weapons are
 * rolled yet; the other columns wait for the items they make.
 */
typedef struct LootChance {
    /* The table's letter, 'A'..'U'. */
    df_u16 id;
    df_u8 gold_min;
    df_u8 gold_max;
    df_u8 armor;
    df_u8 weapon;
    df_u8 magic;
    df_u8 clothing;
    df_u8 book;
    df_u8 plant1;
    df_u8 plant2;
    df_u8 creature1;
    df_u8 creature2;
    df_u8 creature3;
    df_u8 misc1;
    df_u8 misc2;
    df_u8 religious;
} LootChance;

extern const RecSchema item_schema;
extern const RecSchema weapon_schema;
extern const RecSchema armor_schema;
extern const RecSchema material_schema;
extern const RecSchema loot_schema;
extern const RecSchema dungeon_loot_schema;
RecTable *item_table(void);
RecTable *weapon_table(void);
RecTable *armor_table(void);
RecTable *material_table(void);
RecTable *loot_table(void);
RecTable *dungeon_loot_table(void);
const ItemTemplate *item_template(int id);
const WeaponTemplate *weapon_template(int id);
const ArmorTemplate *armor_template(int id);
const Material *material(int id);
const LootChance *loot_chance(int letter);
/* The loot table a dungeon of this type (Dungeon.dungeon_type) fills its treasure from, or 0. */
const LootChance *dungeon_loot(int dungeon_type);

#endif
