#include "game/dagger/tables/diseases.h"

/* See diseases.h and docs/data.md. */

static const RecField g_disease_fields[] = {
    REC_FIELD("id", REC_UINT, Disease, id),
    REC_FIELD("name", REC_TEXT, Disease, name),
    REC_ARRAY("attribute", REC_UINT, Disease, attribute),
    REC_FIELD("health", REC_UINT, Disease, health),
    REC_FIELD("fatigue", REC_UINT, Disease, fatigue),
    REC_FIELD("magicka", REC_UINT, Disease, magicka),
    REC_FIELD("damage_min", REC_UINT, Disease, damage_min),
    REC_FIELD("damage_max", REC_UINT, Disease, damage_max),
    REC_FIELD("days_min", REC_UINT, Disease, days_min),
    REC_FIELD("days_max", REC_UINT, Disease, days_max)
};
const RecSchema disease_schema = {
    "DISE", "disease", g_disease_fields, (int)(sizeof(g_disease_fields) / sizeof(g_disease_fields[0])),
    (df_u16)sizeof(Disease), 0
};

static RecTable g_diseases;

RecTable *disease_table(void)
{
    if (!g_diseases.schema) {
        rec_table_init(&g_diseases, &disease_schema, 32);
    }
    return &g_diseases;
}

const Disease *disease_kind(int id)
{
    return (const Disease *)rec_row(disease_table(), id);
}
