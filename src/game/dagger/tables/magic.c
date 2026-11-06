#include "game/dagger/tables/magic.h"

/* See magic.h and docs/data.md. */

static const RecField g_effect_fields[] = {
    REC_FIELD("id", REC_UINT, MagicEffect, id),
    REC_FIELD("group", REC_TEXT, MagicEffect, group),
    REC_FIELD("sub", REC_TEXT, MagicEffect, sub),
    REC_FIELD("school", REC_UINT, MagicEffect, school),
    REC_FIELD("supports", REC_UINT, MagicEffect, supports),
    REC_FIELD("targets", REC_UINT, MagicEffect, targets),
    REC_FIELD("elements", REC_UINT, MagicEffect, elements),
    REC_ARRAY("duration_cost", REC_UINT, MagicEffect, duration_cost),
    REC_ARRAY("chance_cost", REC_UINT, MagicEffect, chance_cost),
    REC_ARRAY("magnitude_cost", REC_UINT, MagicEffect, magnitude_cost)
};
const RecSchema effect_schema = {
    "EFCT", "effect", g_effect_fields, (int)(sizeof(g_effect_fields) / sizeof(g_effect_fields[0])),
    (df_u16)sizeof(MagicEffect), 0
};

static const RecField g_spell_fields[] = {
    REC_FIELD("id", REC_UINT, MagicSpell, id),
    REC_FIELD("name", REC_TEXT, MagicSpell, name),
    REC_FIELD("icon", REC_UINT, MagicSpell, icon),
    REC_FIELD("number", REC_UINT, MagicSpell, number),
    REC_FIELD("element", REC_UINT, MagicSpell, element),
    REC_FIELD("target", REC_UINT, MagicSpell, target),
    REC_FIELD("price", REC_UINT, MagicSpell, price),
    REC_ARRAY("effect", REC_UINT, MagicSpell, effect),
    REC_ARRAY("duration", REC_UINT, MagicSpell, duration),
    REC_ARRAY("chance", REC_UINT, MagicSpell, chance),
    REC_ARRAY("magnitude", REC_UINT, MagicSpell, magnitude)
};
const RecSchema spell_schema = {
    "SPEL", "spell", g_spell_fields, (int)(sizeof(g_spell_fields) / sizeof(g_spell_fields[0])),
    (df_u16)sizeof(MagicSpell), 0
};

static RecTable g_effects;
static RecTable g_spells;

RecTable *effect_table(void)
{
    if (!g_effects.schema) {
        rec_table_init(&g_effects, &effect_schema, 128);
    }
    return &g_effects;
}

RecTable *spell_table(void)
{
    if (!g_spells.schema) {
        rec_table_init(&g_spells, &spell_schema, 128);
    }
    return &g_spells;
}

const MagicEffect *magic_effect(int id)
{
    return (const MagicEffect *)rec_row(effect_table(), id);
}

const MagicSpell *magic_standard(int number)
{
    RecTable *t;
    int i;

    t = spell_table();
    for (i = 0; i < t->count; i++) {
        const MagicSpell *s;

        s = (const MagicSpell *)(t->rows + (df_u32)i * sizeof(MagicSpell));
        if (s->number == number) {
            return s;
        }
    }
    return 0;
}

void magic_settings(const MagicSpell *spell, int k, EffectSettings *out)
{
    out->effect = spell->effect[k];
    out->duration_base = spell->duration[3 * k + 0];
    out->duration_plus = spell->duration[3 * k + 1];
    out->duration_per = spell->duration[3 * k + 2];
    out->chance_base = spell->chance[3 * k + 0];
    out->chance_plus = spell->chance[3 * k + 1];
    out->chance_per = spell->chance[3 * k + 2];
    out->magnitude_base_low = spell->magnitude[5 * k + 0];
    out->magnitude_base_high = spell->magnitude[5 * k + 1];
    out->magnitude_plus_low = spell->magnitude[5 * k + 2];
    out->magnitude_plus_high = spell->magnitude[5 * k + 3];
    out->magnitude_per = spell->magnitude[5 * k + 4];
}
