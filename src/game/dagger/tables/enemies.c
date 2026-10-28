#include "game/dagger/tables/enemies.h"
#include "game/dagger/formats/dungeon.h"

/* See enemies.h and docs/entities.md. */

/* Mobile id 99 in a fixed marker: no creature (DFU skips it). */
#define INVALID_KIND 99
/* A marker's action byte for one that attacks only when attacked (DFU's EnemyReactionTypes). */
#define PASSIVE 99

#define E(name, type, member) REC_FIELD(name, type, EnemyKind, member)
#define EA(name, type, member) REC_ARRAY(name, type, EnemyKind, member)
static const RecField g_enemy_fields[] = {
    E("id", REC_UINT, id),
    E("name", REC_TEXT, name),
    E("behaviour", REC_UINT, behaviour),
    E("affinity", REC_UINT, affinity),
    E("team", REC_UINT, team),
    E("has_idle", REC_UINT, has_idle),
    E("ranged1", REC_UINT, ranged1),
    E("ranged2", REC_UINT, ranged2),
    E("opens_doors", REC_UINT, opens_doors),
    E("prefers_ranged", REC_UINT, prefers_ranged),
    E("parries", REC_UINT, parries),
    E("casts_magic", REC_UINT, casts_magic),
    E("sees_invisible", REC_UINT, sees_invisible),
    E("male", REC_UINT, male),
    E("female", REC_UINT, female),
    E("corpse_archive", REC_UINT, corpse_archive),
    E("corpse_record", REC_UINT, corpse_record),
    E("blood", REC_UINT, blood),
    E("move_sound", REC_INT, move_sound),
    E("bark_sound", REC_INT, bark_sound),
    E("attack_sound", REC_INT, attack_sound),
    E("min_metal", REC_INT, min_metal),
    EA("damage_min", REC_UINT, damage_min),
    EA("damage_max", REC_UINT, damage_max),
    E("health_min", REC_UINT, health_min),
    E("health_max", REC_UINT, health_max),
    E("level", REC_UINT, level),
    E("armor", REC_INT, armor),
    E("map_chance", REC_UINT, map_chance),
    E("weight", REC_UINT, weight),
    E("soul_points", REC_UINT, soul_points),
    E("loot", REC_TEXT, loot)
};
#undef E
#undef EA

/* A kind a mod adds and leaves half-said: no sounds, hurt by any weapon. */
static const EnemyKind g_enemy_default = {
    0, "", ENEMY_GENERAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1, -1, -1, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 1, 0, 0, 0, 0u, ""
};

const RecSchema enemy_schema = {
    "ENEM", "enemy", g_enemy_fields, (int)(sizeof(g_enemy_fields) / sizeof(g_enemy_fields[0])),
    (df_u16)sizeof(EnemyKind), &g_enemy_default
};

static const RecField g_encounter_fields[] = {
    REC_FIELD("id", REC_UINT, Encounter, id),
    REC_FIELD("count", REC_UINT, Encounter, count),
    REC_ARRAY("kind", REC_UINT, Encounter, kind)
};

const RecSchema encounter_schema = {
    "ENCT", "encounter", g_encounter_fields, 3, (df_u16)sizeof(Encounter), 0
};

static RecTable g_kinds;
static RecTable g_tables;

RecTable *enemy_table(void)
{
    if (!g_kinds.schema) {
        rec_table_init(&g_kinds, &enemy_schema, 64);
    }
    return &g_kinds;
}

RecTable *encounter_table(void)
{
    if (!g_tables.schema) {
        rec_table_init(&g_tables, &encounter_schema, 24);
    }
    return &g_tables;
}

const EnemyKind *enemy_kind(int kind)
{
    return kind < 0 ? 0 : (const EnemyKind *)rec_row(enemy_table(), kind);
}

/* A kind that can stand in a place: it has pictures (39, the horse, has none: 294 is missing). */
static const EnemyKind *kind_of(int kind)
{
    const EnemyKind *k;

    k = enemy_kind(kind);
    return k && k->male ? k : 0;
}

int enemy_archive(int kind, int female)
{
    const EnemyKind *k;

    k = kind_of(kind);
    if (!k) {
        return -1;
    }
    return (int)(female ? k->female : k->male);
}

int enemy_has_idle(int kind)
{
    const EnemyKind *k;

    k = kind_of(kind);
    return k && k->has_idle;
}

int enemy_flies(int kind)
{
    const EnemyKind *k;

    k = kind_of(kind);
    return k && k->behaviour == ENEMY_FLYING;
}

int enemy_spectral(int kind)
{
    const EnemyKind *k;

    k = kind_of(kind);
    return k && k->behaviour == ENEMY_SPECTRAL;
}

int enemy_aquatic(int kind)
{
    const EnemyKind *k;

    k = kind_of(kind);
    return k && k->behaviour == ENEMY_AQUATIC;
}

/*
 * Classic's rand (Watcom's, DFU's DFRandom): the high 15 bits of an LCG.
 * The same as world.c's for the face archives; one dungeon seeds it
 * with its location id.
 */
static df_u32 g_rng;

static int classic_rand(void)
{
    g_rng = g_rng * 0x41C64E6Du + 0x3039u;
    return (int)((g_rng >> 16) & 0x7fffu);
}

static int range_inclusive(int lo, int hi)
{
    return classic_rand() % (hi - lo + 1) + lo;
}

/* The draws, by a random marker's slot (its flags, 1..255; 0 picks one of 1..6). */
static df_u16 g_dry[256];
static df_u16 g_wet[256];
static int g_level = 1;

/*
 * ChooseRandomEnemyType: four times in five a kind near the player's
 * level (three either side); else from the table's start up to a
 * step past it; one time in twenty, a low-level player, two past. At
 * level 1, always one of the first six.
 */
static int draw_kind(const Encounter *t)
{
    int lo;
    int hi;
    int roll;
    int len;

    roll = range_inclusive(1, 100);
    lo = 0;
    if (roll > 95 && g_level <= 5) {
        hi = g_level + 2;
    } else if (roll > 80) {
        hi = g_level + 1;
    } else {
        lo = g_level - 3;
        hi = g_level + 3;
    }
    if (lo < 0) {
        lo = 0;
        hi = 5;
    } else if (hi > 19) {
        lo = 14;
        hi = 19;
    }
    /* Not classic (it read past the cemetery's eleven): kept within the table. */
    len = t->count > ENCOUNTER_LEN ? ENCOUNTER_LEN : t->count;
    if (len < 1) {
        return INVALID_KIND;
    }
    if (hi > len - 1) {
        hi = len - 1;
        lo = lo > hi ? hi : lo;
    }
    return (int)t->kind[range_inclusive(lo, hi)];
}

void enemy_dungeon(int location_id, int dungeon_type, int level)
{
    const Encounter *t;
    const Encounter *wet;
    int i;

    g_level = level < 1 ? 1 : level;
    g_rng = (df_u32)location_id;
    /* The dungeon's table by its type; DUN_TYPE_NONE and a type no table names have none. */
    t = dungeon_type == ENCOUNTER_UNDERWATER ? 0 : (const Encounter *)rec_row(encounter_table(), dungeon_type);
    wet = (const Encounter *)rec_row(encounter_table(), ENCOUNTER_UNDERWATER);
    for (i = 0; i < 256; i++) {
        /* A dungeon of no known kind: none at its random markers. */
        g_dry[i] = (df_u16)(t ? draw_kind(t) : INVALID_KIND);
    }
    for (i = 0; i < 256; i++) {
        g_wet[i] = (df_u16)(wet ? draw_kind(wet) : INVALID_KIND);
    }
}

int enemy_water(const Marker *marks, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (marks[i].record == 10) {
            int sound;

            /* The start marker's sound byte: 8 units a step down, 0 for none. */
            sound = (int)(marks[i].id >> 8);
            return sound ? -8 * sound : ENEMY_DRY;
        }
    }
    return ENEMY_DRY;
}

int enemy_at(const Marker *mk, int water, int *female, int *passive)
{
    int kind;
    int file_y;
    int gendered;

    *female = 0;
    *passive = 0;
    /* Classic's height: down is positive. */
    file_y = -(int)mk->y;
    gendered = 0;
    if (mk->record == ENEMY_FIXED_MARKER) {
        /* The low byte; the high one is garbage in Daggerfall's files. */
        kind = (int)(mk->id & 0xffu);
        if (kind == INVALID_KIND) {
            return -1;
        }
        gendered = 1;
    } else if (mk->record == ENEMY_RANDOM_MARKER) {
        int slot;

        slot = (int)(mk->flags & 0xffu);
        if (slot == 0) {
            slot = range_inclusive(1, 6);
        }
        /* Below the water's surface (a greater y is lower): the underwater draw. */
        kind = water < file_y ? (int)g_wet[slot] : (int)g_dry[slot];
    } else {
        return -1;
    }
    if (enemy_archive(kind, 0) < 0) {
        return -1;
    }
    /* A water dweller needs water, its marker no more than 20 above the surface. */
    if (enemy_aquatic(kind) && (water == ENEMY_DRY || water - 20 > file_y)) {
        return -1;
    }
    /*
     * A class's gender: a fixed marker's flags (1 a woman, 2 a man); a
     * random one's, or a fixed one that names neither, a coin.
     */
    if (kind >= ENEMY_FIRST_CLASS) {
        if (gendered && (mk->flags & 2u)) {
            *female = 0;
        } else if (gendered && (mk->flags & 1u)) {
            *female = 1;
        } else {
            *female = classic_rand() & 1;
        }
    }
    *passive = mk->action == PASSIVE;
    return kind;
}
