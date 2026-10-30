#include "game/dagger/formats/factionfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See factionfile.h. */

#define FILE_NAME "FACTION.TXT"
/* The file holds 366; a mod's may hold more, and past this it is cut with a warning. */
#define MOST 512
/* How deep the tree goes, with room to spare. */
#define DEPTH_MOST 16

static Faction *g_rows;
static int g_count;
static int g_tried;

/* 1 when the line from s starts with key, any case, then a colon; *value past it and its blanks. */
static int key_is(const char *s, const char *end, const char *key, const char **value)
{
    while (*key) {
        char c;

        c = s < end ? *s : 0;
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != *key) {
            return 0;
        }
        s++;
        key++;
    }
    if (s >= end || *s != ':') {
        return 0;
    }
    s++;
    while (s < end && (*s == ' ' || *s == '\t')) {
        s++;
    }
    *value = s;
    return 1;
}

/* A signed number at *s, moved past it and what follows. 0 for no digits. */
static int read_int(const char **s, const char *end)
{
    const char *p;
    int sign;
    int v;

    p = *s;
    sign = 1;
    if (p < end && *p == '-') {
        sign = -1;
        p++;
    }
    v = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    *s = p;
    return v * sign;
}

/* Starts a row with what a faction has when the file does not say. */
static void row_begin(Faction *f, int id, int parent)
{
    int i;

    mem_zero(f, sizeof(*f));
    f->id = (short)id;
    f->parent = (short)parent;
    f->region = -1;
    f->face = -1;
    for (i = 0; i < 2; i++) {
        f->flat_archive[i] = 0;
        f->flat_record[i] = 0;
    }
}

/* One "key: value" line into f. */
static void read_line(Faction *f, const char *s, const char *end, int *flats)
{
    const char *v;
    int i;

    if (key_is(s, end, "type", &v)) {
        f->type = (short)read_int(&v, end);
    } else if (key_is(s, end, "name", &v)) {
        for (i = 0; i < FACTION_NAME - 1 && v < end && *v != '\r'; i++) {
            f->name[i] = *v++;
        }
        f->name[i] = 0;
    } else if (key_is(s, end, "rep", &v)) {
        f->rep = (short)read_int(&v, end);
    } else if (key_is(s, end, "region", &v)) {
        i = read_int(&v, end);
        f->region = (short)(i > 0 ? i - 1 : -1);
    } else if (key_is(s, end, "power", &v)) {
        f->power = (short)read_int(&v, end);
    } else if (key_is(s, end, "flags", &v)) {
        f->flags = (short)read_int(&v, end);
    } else if (key_is(s, end, "face", &v)) {
        f->face = (short)read_int(&v, end);
    } else if (key_is(s, end, "sgroup", &v)) {
        f->sgroup = (short)read_int(&v, end);
    } else if (key_is(s, end, "ggroup", &v)) {
        f->ggroup = (short)read_int(&v, end);
    } else if (key_is(s, end, "flat", &v)) {
        /* The first a man's; a second a woman's; one alone both's. */
        if (*flats < 2) {
            f->flat_archive[*flats] = (short)read_int(&v, end);
            f->flat_record[*flats] = (short)read_int(&v, end);
            if (*flats == 0) {
                f->flat_archive[1] = f->flat_archive[0];
                f->flat_record[1] = f->flat_record[0];
            }
            (*flats)++;
        }
    } else if (key_is(s, end, "ally", &v)) {
        for (i = 0; i < FACTION_LINKS; i++) {
            if (!f->ally[i]) {
                f->ally[i] = (short)read_int(&v, end);
                break;
            }
        }
    } else if (key_is(s, end, "enemy", &v)) {
        for (i = 0; i < FACTION_LINKS; i++) {
            if (!f->enemy[i]) {
                f->enemy[i] = (short)read_int(&v, end);
                break;
            }
        }
    }
}

int factionfile_open(void)
{
    const char *s;
    const char *end;
    df_u8 *bytes;
    df_u32 size;
    int stack[DEPTH_MOST];
    Faction *f;
    int flats;

    if (g_tried) {
        return g_rows != 0;
    }
    g_tried = 1;
    bytes = vfs_read(FILE_NAME, &size);
    if (!bytes) {
        log_text(LOG_WARN, "factions: " FILE_NAME " did not read");
        return 0;
    }
    g_rows = (Faction *)mem_keep((df_u32)MOST * (df_u32)sizeof(Faction));
    if (!g_rows) {
        return 0;
    }
    mem_zero(stack, sizeof(stack));
    f = 0;
    flats = 0;
    s = (const char *)bytes;
    end = s + size;
    while (s < end) {
        const char *line;
        const char *stop;
        int tabs;

        line = s;
        while (s < end && *s != '\n') {
            s++;
        }
        stop = s;
        s++;
        tabs = 0;
        while (line < stop && (*line == '\t' || *line == ' ')) {
            tabs += *line == '\t';
            line++;
        }
        if (line >= stop || *line == ';') {
            continue;
        }
        if (*line == '#') {
            int id;

            if (g_count >= MOST) {
                log_text(LOG_WARN, "factions: more than the table holds");
                break;
            }
            line++;
            id = read_int(&line, stop);
            if (tabs >= DEPTH_MOST) {
                tabs = DEPTH_MOST - 1;
            }
            f = &g_rows[g_count++];
            row_begin(f, id, tabs > 0 ? stack[tabs - 1] : 0);
            stack[tabs] = id;
            flats = 0;
        } else if (f) {
            read_line(f, line, stop, &flats);
        }
    }
    return 1;
}

int faction_count(void)
{
    return g_count;
}

const Faction *faction_at(int i)
{
    return i >= 0 && i < g_count ? &g_rows[i] : 0;
}

const Faction *faction_find(int id)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_rows[i].id == id) {
            return &g_rows[i];
        }
    }
    return 0;
}

/* The one of this type in the region. */
static const Faction *of_region(int type, int region)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_rows[i].type == type && g_rows[i].region == region) {
            return &g_rows[i];
        }
    }
    return 0;
}

const Faction *faction_people_of(int region)
{
    return of_region(FACTION_PEOPLE, region);
}

const Faction *faction_court_of(int region)
{
    return of_region(FACTION_COURTS, region);
}

const Faction *faction_province_of(int region)
{
    return of_region(FACTION_PROVINCE, region);
}
