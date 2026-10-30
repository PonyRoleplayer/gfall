#include "game/dagger/tables/items.h"

/* See items.h and docs/data.md. */

#define I(name, type, member) REC_FIELD(name, type, ItemTemplate, member)
static const RecField g_item_fields[] = {
    I("id", REC_UINT, id),
    I("name", REC_TEXT, name),
    I("weight", REC_FLOAT, weight),
    I("price", REC_UINT, price),
    I("hit_points", REC_UINT, hit_points),
    I("world_archive", REC_UINT, world_archive),
    I("world_record", REC_UINT, world_record),
    I("player_archive", REC_UINT, player_archive),
    I("player_record", REC_UINT, player_record),
    I("one_handed", REC_UINT, one_handed),
    I("blunt", REC_UINT, blunt),
    I("draw_order", REC_UINT, draw_order)
};
#undef I
const RecSchema item_schema = {
    "ITEM", "item", g_item_fields, (int)(sizeof(g_item_fields) / sizeof(g_item_fields[0])),
    (df_u16)sizeof(ItemTemplate), 0
};

static const RecField g_weapon_fields[] = {
    REC_FIELD("id", REC_UINT, WeaponTemplate, id),
    REC_FIELD("damage_min", REC_UINT, WeaponTemplate, damage_min),
    REC_FIELD("damage_max", REC_UINT, WeaponTemplate, damage_max),
    REC_FIELD("skill", REC_UINT, WeaponTemplate, skill),
    REC_FIELD("cif", REC_UINT, WeaponTemplate, cif)
};
const RecSchema weapon_schema = {
    "WEAP", "weapon", g_weapon_fields, 5, (df_u16)sizeof(WeaponTemplate), 0
};

#define A(name, member) REC_FIELD(name, REC_UINT, ArmorTemplate, member)
static const RecField g_armor_fields[] = {
    A("id", id),
    A("part", part),
    A("shield", shield),
    A("shield_parts", shield_parts),
    A("plate_low", plate_low),
    A("plate_high", plate_high),
    A("leather_low", leather_low),
    A("leather_high", leather_high),
    A("chain_low", chain_low),
    A("chain_high", chain_high)
};
#undef A
const RecSchema armor_schema = {
    "ARMR", "armor", g_armor_fields, (int)(sizeof(g_armor_fields) / sizeof(g_armor_fields[0])),
    (df_u16)sizeof(ArmorTemplate), 0
};

static const RecField g_material_fields[] = {
    REC_FIELD("id", REC_UINT, Material, id),
    REC_FIELD("name", REC_TEXT, Material, name),
    REC_FIELD("modifier", REC_INT, Material, modifier),
    REC_FIELD("weight_q", REC_UINT, Material, weight_q),
    REC_FIELD("value_q", REC_UINT, Material, value_q),
    REC_FIELD("condition_q", REC_UINT, Material, condition_q),
    REC_FIELD("value_mul", REC_UINT, Material, value_mul),
    REC_FIELD("armor", REC_UINT, Material, armor)
};
const RecSchema material_schema = {
    "MATL", "material", g_material_fields, 8, (df_u16)sizeof(Material), 0
};

#define L(name, member) REC_FIELD(name, REC_UINT, LootChance, member)
static const RecField g_loot_fields[] = {
    L("id", id),
    L("gold_min", gold_min),
    L("gold_max", gold_max),
    L("armor", armor),
    L("weapon", weapon),
    L("magic", magic),
    L("clothing", clothing),
    L("book", book),
    L("plant1", plant1),
    L("plant2", plant2),
    L("creature1", creature1),
    L("creature2", creature2),
    L("creature3", creature3),
    L("misc1", misc1),
    L("misc2", misc2),
    L("religious", religious)
};
#undef L
const RecSchema loot_schema = {
    "LOOT", "loot", g_loot_fields, (int)(sizeof(g_loot_fields) / sizeof(g_loot_fields[0])),
    (df_u16)sizeof(LootChance), 0
};

static RecTable g_items;
static RecTable g_weapons;
static const RecField g_dungeon_loot_fields[] = {
    REC_FIELD("id", REC_UINT, DungeonLoot, id),
    REC_FIELD("letter", REC_UINT, DungeonLoot, letter)
};
const RecSchema dungeon_loot_schema = {
    "LDUN", "dungeon_loot", g_dungeon_loot_fields, 2, (df_u16)sizeof(DungeonLoot), 0
};

static RecTable g_dungeon_loots;
static RecTable g_armors;
static RecTable g_materials;
static RecTable g_loots;

RecTable *item_table(void)
{
    if (!g_items.schema) {
        rec_table_init(&g_items, &item_schema, 40);
    }
    return &g_items;
}

RecTable *weapon_table(void)
{
    if (!g_weapons.schema) {
        rec_table_init(&g_weapons, &weapon_schema, 32);
    }
    return &g_weapons;
}

RecTable *armor_table(void)
{
    if (!g_armors.schema) {
        rec_table_init(&g_armors, &armor_schema, 11);
    }
    return &g_armors;
}

RecTable *loot_table(void)
{
    if (!g_loots.schema) {
        rec_table_init(&g_loots, &loot_schema, 21);
    }
    return &g_loots;
}

RecTable *dungeon_loot_table(void)
{
    if (!g_dungeon_loots.schema) {
        rec_table_init(&g_dungeon_loots, &dungeon_loot_schema, 19);
    }
    return &g_dungeon_loots;
}

RecTable *material_table(void)
{
    if (!g_materials.schema) {
        rec_table_init(&g_materials, &material_schema, MATERIAL_COUNT);
    }
    return &g_materials;
}

const ItemTemplate *item_template(int id)
{
    return id < 0 ? 0 : (const ItemTemplate *)rec_row(item_table(), id);
}

const WeaponTemplate *weapon_template(int id)
{
    return id < 0 ? 0 : (const WeaponTemplate *)rec_row(weapon_table(), id);
}

const ArmorTemplate *armor_template(int id)
{
    return id < 0 ? 0 : (const ArmorTemplate *)rec_row(armor_table(), id);
}

const LootChance *loot_chance(int letter)
{
    return letter <= 0 ? 0 : (const LootChance *)rec_row(loot_table(), letter);
}

const LootChance *dungeon_loot(int dungeon_type)
{
    const DungeonLoot *row;

    row = dungeon_type < 0 ? 0 : (const DungeonLoot *)rec_row(dungeon_loot_table(), dungeon_type);
    return row ? loot_chance(row->letter) : 0;
}

const Material *material(int id)
{
    return id < 0 ? 0 : (const Material *)rec_row(material_table(), id);
}
