#include "game/dagger/tables/maindat.h"
#include "game/dagger/tables/classic.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/forecast.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"

/* See maindat.h and docs/data.md. */

/* The files merged, for a later file's needs, and the log. */
#define MERGED_MAX 32

/*
 * ARENA2 files MAIN.DAT is made from: a different size makes it stale.
 * MONSTER.BSA holds the kinds ENEM describes (read for their names once
 * its layout is confirmed against UESP).
 */
static const char *const g_sources[] = { "MONSTER.BSA" };
#define SOURCES ((int)(sizeof(g_sources) / sizeof(g_sources[0])))

static char g_merged[MERGED_MAX][REC_NAME];
static int g_nmerged;
static RecTable g_head;

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
    default:
        return 0;
    }
}

int maindat_table_count(void)
{
    return 4;
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
        head[1 + i].size = source_size(g_sources[i]);
        df_copy(head[1 + i].name, g_sources[i], REC_NAME);
    }
    if (!rec_put_begin(put, ctx) || !rec_put_rows(put, ctx, &rec_head_schema, head, 1 + SOURCES, 0, 0)) {
        return 0;
    }
    kinds = classic_enemies(&n);
    if (!rec_put_rows(put, ctx, &enemy_schema, kinds, n, 0, 0)) {
        return 0;
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
    return rec_put_rows(put, ctx, &weather_look_schema, looks, WEATHER_KINDS, 0, 0);
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
}

/* MAIN.DAT and the mods sit beside the exe, which is ARENA2's parent: the VFS reads them as "../NAME". */
static df_u8 *read_beside(const char *name, df_u32 *n)
{
    char rel[REC_NAME + 4];

    rel[0] = '.';
    rel[1] = '.';
    rel[2] = '/';
    df_copy(rel + 3, name, REC_NAME);
    return vfs_read(rel, n);
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

int maindat_start(void)
{
    df_u8 *bytes;
    df_u8 *order;
    df_u32 n;
    df_u32 pos;
    char name[REC_NAME];

    bytes = read_beside(MAINDAT_NAME, &n);
    if (!bytes || maindat_stale(bytes, n)) {
        log_text(LOG_INFO, bytes ? "data: MAIN.DAT is stale, cooking it again" : "data: cooking MAIN.DAT");
        if (!fs_create(MAINDAT_NAME) || !maindat_import(put_file, 0)) {
            fs_abort();
            log_text(LOG_ERROR, "data: MAIN.DAT could not be written beside the program");
            return 0;
        }
        fs_finish();
        bytes = read_beside(MAINDAT_NAME, &n);
        if (!bytes) {
            log_text(LOG_ERROR, "data: MAIN.DAT could not be read back");
            return 0;
        }
    }
    maindat_reset();
    if (!maindat_merge(bytes, n, MAINDAT_NAME)) {
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
            log_print(LOG_WARN, "data: %s is in " MAINDAT_ORDER " but not beside the program", name);
            continue;
        }
        maindat_merge(mod, mn, name);
    }
    maindat_apply();
    return 1;
}
