#include "game/dagger/tables/careers.h"
#include "game/dagger/formats/classcfg.h"
#include "engine/core/mem.h"

/* See careers.h and docs/data.md. */

#define C(name, type, member) REC_FIELD(name, type, CareerClass, member)
#define CA(name, type, member) REC_ARRAY(name, type, CareerClass, member)
static const RecField g_class_fields[] = {
    C("id", REC_UINT, id),
    C("name", REC_TEXT, name),
    C("hp_per_level", REC_UINT, hp_per_level),
    CA("attribute", REC_UINT, attribute),
    CA("primary", REC_UINT, primary),
    CA("major", REC_UINT, major),
    CA("minor", REC_UINT, minor),
    C("magicka", REC_FLOAT, magicka),
    C("resist", REC_UINT, resist),
    C("immune", REC_UINT, immune),
    C("low_tolerance", REC_UINT, low_tolerance),
    C("weakness", REC_UINT, weakness),
    C("abilities", REC_UINT, abilities),
    C("rapid_healing", REC_UINT, rapid_healing),
    C("regeneration", REC_UINT, regeneration),
    C("absorption", REC_UINT, absorption),
    C("attack_modifier", REC_UINT, attack_modifier),
    C("forbidden", REC_UINT, forbidden_materials),
    C("weapons", REC_UINT, weapons),
    C("advancement", REC_UINT, advancement)
};
#undef C
#undef CA
const RecSchema class_schema = {
    "CLAS", "class", g_class_fields, (int)(sizeof(g_class_fields) / sizeof(g_class_fields[0])),
    (df_u16)sizeof(CareerClass), 0
};

#define R(name, type, member) REC_FIELD(name, type, CareerRace, member)
static const RecField g_race_fields[] = {
    R("id", REC_UINT, id),
    R("name", REC_TEXT, name),
    R("description", REC_UINT, description),
    R("clip", REC_UINT, clip),
    R("background", REC_TEXT, background),
    R("body_male", REC_TEXT, body_male),
    R("clothed_male", REC_TEXT, body_male_clothed),
    R("body_female", REC_TEXT, body_female),
    R("clothed_female", REC_TEXT, body_female_clothed),
    R("heads_male", REC_TEXT, heads_male),
    R("heads_female", REC_TEXT, heads_female),
    R("resist", REC_UINT, resist),
    R("immune", REC_UINT, immune),
    R("low_tolerance", REC_UINT, low_tolerance),
    R("weakness", REC_UINT, weakness),
    R("abilities", REC_UINT, abilities)
};
#undef R
const RecSchema race_schema = {
    "RACE", "race", g_race_fields, (int)(sizeof(g_race_fields) / sizeof(g_race_fields[0])),
    (df_u16)sizeof(CareerRace), 0
};

static const RecField g_skill_fields[] = {
    REC_FIELD("id", REC_UINT, CareerSkill, id),
    REC_FIELD("name", REC_TEXT, CareerSkill, name),
    REC_FIELD("attribute", REC_UINT, CareerSkill, attribute),
    REC_FIELD("advance", REC_UINT, CareerSkill, advance)
};
const RecSchema skill_schema = {
    "SKIL", "skill", g_skill_fields, 4, (df_u16)sizeof(CareerSkill), 0
};

static RecTable g_classes;
static RecTable g_races;
static RecTable g_skills;
static CareerClass g_custom;
static int g_have_custom;

RecTable *class_table(void)
{
    if (!g_classes.schema) {
        rec_table_init(&g_classes, &class_schema, CLASSCFG_COUNT);
    }
    return &g_classes;
}

RecTable *race_table(void)
{
    if (!g_races.schema) {
        rec_table_init(&g_races, &race_schema, 8);
    }
    return &g_races;
}

RecTable *skill_table(void)
{
    if (!g_skills.schema) {
        rec_table_init(&g_skills, &skill_schema, SKILL_COUNT);
    }
    return &g_skills;
}

const CareerClass *career_class(int id)
{
    if (id == CLASS_CUSTOM) {
        return g_have_custom ? &g_custom : 0;
    }
    return id < 0 ? 0 : (const CareerClass *)rec_row(class_table(), id);
}

void career_set_custom(const CareerClass *c)
{
    g_custom = *c;
    g_custom.id = CLASS_CUSTOM;
    g_have_custom = 1;
}

const CareerRace *career_race(int id)
{
    return id < 0 ? 0 : (const CareerRace *)rec_row(race_table(), id);
}

const CareerSkill *career_skill(int id)
{
    return id < 0 ? 0 : (const CareerSkill *)rec_row(skill_table(), id);
}

int career_from_cfg(int id, const df_u8 *bytes, df_u32 n, CareerClass *out)
{
    ClassCfg cfg;
    int i;

    if (!classcfg_read(bytes, n, &cfg)) {
        return 0;
    }
    mem_zero(out, sizeof(*out));
    out->id = (df_u16)id;
    df_copy(out->name, cfg.name, 16);
    out->hp_per_level = cfg.hp_per_level;
    for (i = 0; i < ATTR_COUNT; i++) {
        out->attribute[i] = cfg.attribute[i];
    }
    for (i = 0; i < 3; i++) {
        out->primary[i] = cfg.primary[i];
        out->major[i] = cfg.major[i];
    }
    for (i = 0; i < 6; i++) {
        out->minor[i] = cfg.minor[i];
    }
    out->magicka = classcfg_magicka(cfg.abilities);
    out->resist = cfg.resist;
    out->immune = cfg.immune;
    out->low_tolerance = cfg.low_tolerance;
    out->weakness = cfg.weakness;
    out->abilities = cfg.abilities;
    out->rapid_healing = cfg.rapid_healing;
    out->regeneration = cfg.regeneration;
    out->absorption = cfg.absorption;
    out->attack_modifier = cfg.attack_modifier;
    out->forbidden_materials = cfg.forbidden_materials;
    out->weapons = cfg.weapons;
    out->advancement = cfg.advancement;
    return 1;
}
