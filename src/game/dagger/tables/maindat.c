#include "game/dagger/tables/maindat.h"
#include "game/dagger/tables/classic.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/forecast.h"
#include "game/dagger/tables/calendar.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/classcfg.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"

/* See maindat.h and docs/data.md. */

/* The files merged, for a later file's needs, and the log. */
#define MERGED_MAX 32

/*
 * ARENA2 files MAIN.DAT is made from: a different size makes it stale.
 * MONSTER.BSA holds the kinds ENEM describes (their attributes, from
 * each monster's ENEMY###.CFG); the class files are CLAS, and a class
 * enemy's attributes.
 */
#define SOURCES (1 + CLASSCFG_COUNT)

static void source_name(int i, char *dst)
{
    if (i == 0) {
        df_copy(dst, "MONSTER.BSA", REC_NAME);
    } else {
        classcfg_name(i - 1, dst);
    }
}

/* The most kinds the import fills attributes in for: classic has 62. */
#define KINDS_MAX 96

/*
 * Each kind's attributes from its career file, CLASS##.CFG's layout:
 * a monster (id below 128) its ENEMY###.CFG in MONSTER.BSA, a class
 * enemy (128 + class) its CLASS##.CFG. A kind whose file is missing
 * keeps 50 in each, with a warning.
 */
static void kind_attributes(EnemyKind *kinds, int n)
{
    Bsa *monsters;
    int i;

    monsters = bsa_open("MONSTER.BSA");
    for (i = 0; i < n; i++) {
        char name[REC_NAME];
        const df_u8 *b;
        df_u32 size;
        ClassCfg cfg;
        int a;

        b = 0;
        size = 0;
        if (kinds[i].id < 128) {
            const BsaRec *rec;

            log_format(name, (int)sizeof(name), "ENEMY%03d.CFG", (int)kinds[i].id);
            rec = monsters ? bsa_find_name(monsters, name) : 0;
            if (rec) {
                b = bsa_read(monsters, rec);
                size = rec->size;
            }
        } else {
            classcfg_name(kinds[i].id - 128, name);
            b = vfs_read(name, &size);
        }
        if (!b || !classcfg_read(b, size, &cfg)) {
            log_print(LOG_WARN, "data: %s did not read; kind %d's attributes are 50", name, (int)kinds[i].id);
            for (a = 0; a < 8; a++) {
                kinds[i].attribute[a] = 50;
            }
            continue;
        }
        for (a = 0; a < 8; a++) {
            kinds[i].attribute[a] = (df_u8)(cfg.attribute[a] > 255 ? 255 : cfg.attribute[a]);
        }
    }
    if (monsters) {
        bsa_close(monsters);
    }
}

static char g_merged[MERGED_MAX][REC_NAME];
static int g_nmerged;
static RecTable g_head;
/* Chunks refused as damaged since the last reset; the message maindat_start gives a person. */
static int g_damaged;
static const char *g_error = "";

static RecTable *table_at(int i)
{
    switch (i) {
    case 0:
        return enemy_table();
    case 1:
        return encounter_table();
    case 2:
        return weather_odds_table();
    case 3:
        return weather_look_table();
    case 4:
        return cal_month_table();
    case 5:
        return cal_weekday_table();
    case 6:
        return cal_info_table();
    case 7:
        return class_table();
    case 8:
        return race_table();
    case 9:
        return skill_table();
    case 10:
        return item_table();
    case 11:
        return weapon_table();
    case 12:
        return material_table();
    case 13:
        return armor_table();
    case 14:
        return loot_table();
    case 15:
        return dungeon_loot_table();
    case 16:
        return building_table();
    case 17:
        return crime_table();
    default:
        return 0;
    }
}

int maindat_table_count(void)
{
    return 18;
}

RecTable *maindat_table(int i)
{
    return table_at(i);
}

/* Case folded, as DOS names are. */
static int same_name(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] && b[i]; i++) {
        int x;
        int y;

        x = a[i] >= 'a' && a[i] <= 'z' ? a[i] - 32 : a[i];
        y = b[i] >= 'a' && b[i] <= 'z' ? b[i] - 32 : b[i];
        if (x != y) {
            return 0;
        }
    }
    return a[i] == b[i];
}

RecTable *maindat_find(const char *tag_or_name)
{
    int i;

    for (i = 0; i < maindat_table_count(); i++) {
        RecTable *t;

        t = table_at(i);
        if (df_eq(tag_or_name, t->schema->tag) || same_name(tag_or_name, t->schema->table)) {
            return t;
        }
    }
    return 0;
}

static df_u32 source_size(const char *name)
{
    VfsFile *f;
    df_u32 n;

    f = vfs_open(name);
    if (!f) {
        return 0;
    }
    n = vfs_size(f);
    vfs_close(f);
    return n;
}

int maindat_import(RecPut put, void *ctx)
{
    RecHead head[1 + SOURCES];
    WeatherOdds odds[64];
    WeatherKindLook looks[WEATHER_KINDS];
    const EnemyKind *kinds;
    const Encounter *tables;
    int n;
    int i;

    mem_zero(head, sizeof(head));
    head[0].id = 0;
    head[0].role = REC_HEAD_SELF;
    head[0].size = MAINDAT_VERSION;
    df_copy(head[0].name, MAINDAT_NAME, REC_NAME);
    for (i = 0; i < SOURCES; i++) {
        head[1 + i].id = (df_u16)(1 + i);
        head[1 + i].role = REC_HEAD_SOURCE;
        source_name(i, head[1 + i].name);
        head[1 + i].size = source_size(head[1 + i].name);
    }
    if (!rec_put_begin(put, ctx) || !rec_put_rows(put, ctx, &rec_head_schema, head, 1 + SOURCES, 0, 0)) {
        return 0;
    }
    kinds = classic_enemies(&n);
    {
        static EnemyKind filled[KINDS_MAX];

        if (n > KINDS_MAX) {
            n = KINDS_MAX;
        }
        mem_copy(filled, kinds, (df_u32)n * (df_u32)sizeof(EnemyKind));
        kind_attributes(filled, n);
        if (!rec_put_rows(put, ctx, &enemy_schema, filled, n, 0, 0)) {
            return 0;
        }
    }
    tables = classic_encounters(&n);
    if (!rec_put_rows(put, ctx, &encounter_schema, tables, n, 0, 0)) {
        return 0;
    }
    n = classic_weather_odds(odds, 64);
    if (!rec_put_rows(put, ctx, &weather_odds_schema, odds, n, 0, 0)) {
        return 0;
    }
    /* The looks are the engine's own defaults until a mod changes them. */
    for (i = 0; i < WEATHER_KINDS; i++) {
        WeatherLook l;

        weather_look(i, &l);
        looks[i].id = (df_u16)i;
        looks[i].cloud = l.cloud;
        looks[i].fog = l.fog;
        looks[i].rain = l.rain;
        looks[i].snow = l.snow;
        looks[i].storm = l.storm;
        looks[i].sky = l.sky;
        looks[i].view = l.view;
        looks[i].haze = l.haze;
    }
    if (!rec_put_rows(put, ctx, &weather_look_schema, looks, WEATHER_KINDS, 0, 0)) {
        return 0;
    }
    {
        const CalMonth *months;
        const CalWeekday *days;

        months = classic_months(&n);
        if (!rec_put_rows(put, ctx, &cal_month_schema, months, n, 0, 0)) {
            return 0;
        }
        days = classic_weekdays(&n);
        if (!rec_put_rows(put, ctx, &cal_weekday_schema, days, n, 0, 0)) {
            return 0;
        }
    }
    if (!rec_put_rows(put, ctx, &cal_info_schema, classic_calendar(), 1, 0, 0)) {
        return 0;
    }
    /* The classes from ARENA2's own files; a missing one is left out (a mod may give it). */
    {
        CareerClass classes[CLASSCFG_COUNT];
        const CareerRace *races;
        const CareerSkill *skills;
        char name[REC_NAME];
        int k;

        n = 0;
        for (k = 0; k < CLASSCFG_COUNT; k++) {
            const df_u8 *b;
            df_u32 size;

            classcfg_name(k, name);
            b = vfs_read(name, &size);
            if (b && career_from_cfg(k, b, size, &classes[n])) {
                n++;
            } else {
                log_print(LOG_WARN, "data: %s did not read; no class %d", name, k);
            }
        }
        if (!rec_put_rows(put, ctx, &class_schema, classes, n, 0, 0)) {
            return 0;
        }
        races = classic_races(&n);
        if (!rec_put_rows(put, ctx, &race_schema, races, n, 0, 0)) {
            return 0;
        }
        skills = classic_skills(&n);
        if (!rec_put_rows(put, ctx, &skill_schema, skills, n, 0, 0)) {
            return 0;
        }
    }
    {
        const ItemTemplate *items;
        const WeaponTemplate *weapons;
        const ArmorTemplate *armors;
        const LootChance *loots;
        const DungeonLoot *dungeon_loots;
        const Material *materials;

        items = classic_items(&n);
        if (!rec_put_rows(put, ctx, &item_schema, items, n, 0, 0)) {
            return 0;
        }
        weapons = classic_weapons(&n);
        if (!rec_put_rows(put, ctx, &weapon_schema, weapons, n, 0, 0)) {
            return 0;
        }
        armors = classic_armors(&n);
        if (!rec_put_rows(put, ctx, &armor_schema, armors, n, 0, 0)) {
            return 0;
        }
        loots = classic_loots(&n);
        if (!rec_put_rows(put, ctx, &loot_schema, loots, n, 0, 0)) {
            return 0;
        }
        dungeon_loots = classic_dungeon_loots(&n);
        if (!rec_put_rows(put, ctx, &dungeon_loot_schema, dungeon_loots, n, 0, 0)) {
            return 0;
        }
        materials = classic_materials(&n);
        if (!rec_put_rows(put, ctx, &material_schema, materials, n, 0, 0)) {
            return 0;
        }
        {
            const BuildingKind *buildings;
            const CrimeKind *crimes;

            buildings = classic_buildings(&n);
            if (!rec_put_rows(put, ctx, &building_schema, buildings, n, 0, 0)) {
                return 0;
            }
            crimes = classic_crimes(&n);
            return rec_put_rows(put, ctx, &crime_schema, crimes, n, 0, 0);
        }
    }
}

/* A file's HEAD rows, into g_head. 0 when it has none. */
static int read_head(const df_u8 *bytes, df_u32 n)
{
    RecChunk c;
    df_u32 pos;

    if (!g_head.schema) {
        rec_table_init(&g_head, &rec_head_schema, 8);
    }
    rec_table_clear(&g_head);
    pos = 0;
    while (rec_next(bytes, n, &pos, &c)) {
        if (df_eq(c.tag, rec_head_schema.tag)) {
            return rec_merge(&g_head, &c, 0);
        }
    }
    return 0;
}

static const RecHead *head_row(int i)
{
    return (const RecHead *)(g_head.rows + (df_u32)i * sizeof(RecHead));
}

int maindat_stale(const df_u8 *bytes, df_u32 n)
{
    int self;
    int i;

    if (!rec_is_file(bytes, n) || !read_head(bytes, n)) {
        return 1;
    }
    self = 0;
    for (i = 0; i < g_head.count; i++) {
        const RecHead *h;

        h = head_row(i);
        if (h->role == REC_HEAD_SELF) {
            self = h->size == MAINDAT_VERSION && same_name(h->name, MAINDAT_NAME);
        } else if (h->role == REC_HEAD_SOURCE && h->size != source_size(h->name)) {
            return 1;
        }
    }
    return !self;
}

void maindat_reset(void)
{
    int i;

    for (i = 0; i < maindat_table_count(); i++) {
        rec_table_clear(table_at(i));
    }
    g_nmerged = 0;
    g_damaged = 0;
}

int maindat_whole(void)
{
    return g_damaged == 0 && enemy_table()->count > 0 && encounter_table()->count > 0
        && weather_odds_table()->count > 0 && weather_look_table()->count > 0
        && cal_month_table()->count > 0 && cal_weekday_table()->count > 0
        && rec_row(cal_info_table(), 0) != 0 && class_table()->count > 0 && race_table()->count > 0
        && skill_table()->count == SKILL_COUNT && item_table()->count > 0 && weapon_table()->count > 0
        && armor_table()->count > 0 && loot_table()->count > 0
        && building_table()->count > 0 && crime_table()->count > 0
        && material_table()->count == MATERIAL_COUNT;
}

const char *maindat_error(void)
{
    return g_error;
}

static int was_merged(const char *name)
{
    int i;

    for (i = 0; i < g_nmerged; i++) {
        if (same_name(g_merged[i], name)) {
            return 1;
        }
    }
    return 0;
}

int maindat_merge(const df_u8 *bytes, df_u32 n, const char *name)
{
    RecChunk c;
    RecMerge st;
    df_u32 pos;
    int unknown;
    int i;

    if (!rec_is_file(bytes, n)) {
        log_print(LOG_WARN, "data: %s is not a record file, skipped", name);
        return 0;
    }
    if (read_head(bytes, n)) {
        for (i = 0; i < g_head.count; i++) {
            const RecHead *h;

            h = head_row(i);
            if (h->role == REC_HEAD_NEEDS && !was_merged(h->name)) {
                log_print(LOG_WARN, "data: %s needs %s before it, skipped", name, h->name);
                return 0;
            }
        }
    }
    mem_zero(&st, sizeof(st));
    unknown = 0;
    pos = 0;
    while (rec_next(bytes, n, &pos, &c)) {
        RecTable *t;

        if (df_eq(c.tag, rec_head_schema.tag)) {
            continue;
        }
        t = maindat_find(c.tag);
        if (!t) {
            unknown++;
            continue;
        }
        if (!rec_merge(t, &c, &st)) {
            log_print(LOG_WARN, "data: %s's %s chunk is damaged, skipped", name, c.tag);
            g_damaged++;
        }
    }
    if (g_nmerged < MERGED_MAX) {
        df_copy(g_merged[g_nmerged++], name, REC_NAME);
    }
    log_print(LOG_INFO, "data: %s merged, %d added, %d changed, %d fields and %d chunks not known",
              name, st.added, st.changed, st.skipped, unknown);
    return 1;
}

int maindat_merged(void)
{
    return g_nmerged;
}

const char *maindat_merged_name(int i)
{
    return i >= 0 && i < g_nmerged ? g_merged[i] : "";
}

void maindat_apply(void)
{
    forecast_looks();
    calendar_apply();
}

/* DATA\name beside the exe: MAIN.DAT, LOADORDR.TXT, a mod. */
static void in_data(char *dst, const char *name)
{
    int k;

    df_copy(dst, MAINDAT_DIR "\\", REC_NAME);
    for (k = 0; dst[k]; k++) {
    }
    df_copy(dst + k, name, REC_NAME);
}

static df_u8 *read_beside(const char *name, df_u32 *n)
{
    char path[2 * REC_NAME];

    in_data(path, name);
    return vfs_read_beside(path, n);
}

static int put_file(void *ctx, const void *src, df_u32 n)
{
    (void)ctx;
    return fs_append(src, n);
}

/* The next name in LOADORDR.TXT's text at *pos: one a line, blanks and # lines skipped. */
static int next_name(const df_u8 *text, df_u32 n, df_u32 *pos, char *name)
{
    while (*pos < n) {
        int k;
        df_u32 end;

        end = *pos;
        while (end < n && text[end] != '\n') {
            end++;
        }
        while (*pos < end && (text[*pos] == ' ' || text[*pos] == '\t')) {
            (*pos)++;
        }
        k = 0;
        if (*pos < end && text[*pos] != '#') {
            while (*pos < end && k < REC_NAME - 1 && text[*pos] != '\r' && text[*pos] != ' '
                   && text[*pos] != '\t') {
                name[k++] = (char)text[(*pos)++];
            }
        }
        name[k] = 0;
        *pos = end + 1;
        if (k > 0) {
            return 1;
        }
    }
    return 0;
}

/* Cooks DATA\MAIN.DAT again and reads it back; 0, with g_error said, when it cannot. */
static df_u8 *cook(df_u32 *n)
{
    char path[2 * REC_NAME];
    df_u8 *bytes;

    in_data(path, MAINDAT_NAME);
    if (!fs_make_dir(MAINDAT_DIR) || !fs_create(path) || !maindat_import(put_file, 0)) {
        fs_abort();
        log_text(LOG_ERROR, "data: DATA\\MAIN.DAT could not be written beside the program");
        g_error = "The game could not write DATA\\MAIN.DAT beside itself. "
                  "Run it from a folder it may write to (not a CD), with room on the disk.";
        return 0;
    }
    fs_finish();
    bytes = read_beside(MAINDAT_NAME, n);
    if (!bytes) {
        log_text(LOG_ERROR, "data: MAIN.DAT could not be read back");
        g_error = "The game wrote DATA\\MAIN.DAT but could not read it back. "
                  "Delete DATA\\MAIN.DAT and start the game again.";
    }
    return bytes;
}

/* MAIN.DAT alone into empty tables: 1 when it merged whole. */
static int merge_main(const df_u8 *bytes, df_u32 n)
{
    maindat_reset();
    return maindat_merge(bytes, n, MAINDAT_NAME) && maindat_whole();
}

int maindat_start(void)
{
    df_u8 *bytes;
    df_u8 *order;
    df_u32 n;
    df_u32 pos;
    char name[REC_NAME];
    int cooked;

    g_error = "";
    cooked = 0;
    bytes = read_beside(MAINDAT_NAME, &n);
    if (!bytes || maindat_stale(bytes, n)) {
        log_text(LOG_INFO, bytes ? "data: MAIN.DAT is stale, cooking it again" : "data: cooking MAIN.DAT");
        bytes = cook(&n);
        if (!bytes) {
            return 0;
        }
        cooked = 1;
    }
    /* Damaged inside though current (a bad disk, a cut copy): cooked again once before giving up. */
    if (!merge_main(bytes, n) && !cooked) {
        log_text(LOG_WARN, "data: MAIN.DAT is damaged, cooking it again");
        bytes = cook(&n);
        if (!bytes) {
            return 0;
        }
        cooked = 1;
        merge_main(bytes, n);
    }
    if (!maindat_whole()) {
        log_text(LOG_ERROR, "data: MAIN.DAT is damaged even as cooked");
        g_error = "DATA\\MAIN.DAT is damaged. Delete it and start the game again: "
                  "it is made anew from ARENA2. If this comes back, check ARENA2 and the disk.";
        return 0;
    }
    order = read_beside(MAINDAT_ORDER, &n);
    pos = 0;
    while (order && next_name(order, n, &pos, name)) {
        df_u8 *mod;
        df_u32 mn;

        if (same_name(name, MAINDAT_NAME)) {
            continue;
        }
        mod = read_beside(name, &mn);
        if (!mod) {
            log_print(LOG_WARN, "data: %s is in " MAINDAT_ORDER " but not in " MAINDAT_DIR, name);
            continue;
        }
        maindat_merge(mod, mn, name);
    }
    maindat_apply();
    return 1;
}
