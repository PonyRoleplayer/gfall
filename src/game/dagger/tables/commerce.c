#include "game/dagger/tables/commerce.h"

/* See commerce.h and docs/data.md. */

static const RecField g_building_fields[] = {
    REC_FIELD("id", REC_UINT, BuildingKind, id),
    REC_FIELD("name", REC_TEXT, BuildingKind, name),
    REC_FIELD("open", REC_UINT, BuildingKind, open),
    REC_FIELD("close", REC_UINT, BuildingKind, close),
    REC_FIELD("stock_weapons", REC_UINT, BuildingKind, stock_weapons),
    REC_FIELD("stock_armor", REC_UINT, BuildingKind, stock_armor)
};
const RecSchema building_schema = {
    "BLDG", "building", g_building_fields, (int)(sizeof(g_building_fields) / sizeof(g_building_fields[0])),
    (df_u16)sizeof(BuildingKind), 0
};

static const RecField g_crime_fields[] = {
    REC_FIELD("id", REC_UINT, CrimeKind, id),
    REC_FIELD("name", REC_TEXT, CrimeKind, name),
    REC_FIELD("loss", REC_UINT, CrimeKind, loss),
    REC_FIELD("per_point", REC_UINT, CrimeKind, per_point),
    REC_FIELD("base", REC_UINT, CrimeKind, base),
    REC_FIELD("low", REC_UINT, CrimeKind, low),
    REC_FIELD("high", REC_UINT, CrimeKind, high)
};
const RecSchema crime_schema = {
    "CRIM", "crime", g_crime_fields, (int)(sizeof(g_crime_fields) / sizeof(g_crime_fields[0])),
    (df_u16)sizeof(CrimeKind), 0
};

static RecTable g_buildings;
static RecTable g_crimes;

RecTable *building_table(void)
{
    if (!g_buildings.schema) {
        rec_table_init(&g_buildings, &building_schema, BUILDING_TYPES);
    }
    return &g_buildings;
}

RecTable *crime_table(void)
{
    if (!g_crimes.schema) {
        rec_table_init(&g_crimes, &crime_schema, CRIME_COUNT);
    }
    return &g_crimes;
}

const BuildingKind *building_kind(int type)
{
    return type < 0 ? 0 : (const BuildingKind *)rec_row(building_table(), type);
}

const CrimeKind *crime_kind(int crime)
{
    return crime < 0 ? 0 : (const CrimeKind *)rec_row(crime_table(), crime);
}

int building_is_shop(int type)
{
    const BuildingKind *b;

    b = building_kind(type);
    return b && (b->stock_weapons || b->stock_armor);
}

int building_open_at(int type, int hour)
{
    const BuildingKind *b;

    b = building_kind(type);
    return b && hour >= b->open && hour < b->close;
}
