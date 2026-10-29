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
    I("blunt", REC_UINT, blunt)
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

static const RecField g_material_fields[] = {
    REC_FIELD("id", REC_UINT, Material, id),
    REC_FIELD("name", REC_TEXT, Material, name),
    REC_FIELD("modifier", REC_INT, Material, modifier),
    REC_FIELD("weight_q", REC_UINT, Material, weight_q),
    REC_FIELD("value_q", REC_UINT, Material, value_q),
    REC_FIELD("condition_q", REC_UINT, Material, condition_q)
};
const RecSchema material_schema = {
    "MATL", "material", g_material_fields, 6, (df_u16)sizeof(Material), 0
};

static RecTable g_items;
static RecTable g_weapons;
static RecTable g_materials;

RecTable *item_table(void)
{
    if (!g_items.schema) {
        rec_table_init(&g_items, &item_schema, 32);
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

const Material *material(int id)
{
    return id < 0 ? 0 : (const Material *)rec_row(material_table(), id);
}
