#include "means.h"

#include "engine/core/df.h"
#include "engine/scene/clock.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/tables/careers.h"

/*
 * Names are read from the merged book, clock_season_name, and
 * texture_filename. The value column stays the record.
 */

static const char *g_attr[ATTR_COUNT] = {
    "Strength", "Intelligence", "Willpower", "Agility",
    "Endurance", "Personality", "Speed", "Luck"
};

static const char *g_dun_type[] = {
    "Crypt", "Orc stronghold", "Human stronghold", "Prison",
    "Desecrated temple", "Mine", "Natural cave", "Coven",
    "Vampire haunt", "Laboratory", "Harpy nest", "Ruined castle",
    "Spider nest", "Giant stronghold", "Dragon's den", "Barbarian stronghold",
    "Volcanic caves", "Scorpion nest", "Cemetery"
};

const char *means_attr_name(int attr)
{
    if (attr < 0 || attr >= ATTR_COUNT) {
        return "";
    }
    return g_attr[attr];
}

const char *means_dungeon_type(int type)
{
    if (type < 0 || type >= (int)(sizeof(g_dun_type) / sizeof(g_dun_type[0]))) {
        return "";
    }
    return g_dun_type[type];
}

static void copy_s(char *dst, int cap, const char *src)
{
    int i;

    if (!dst || cap < 1) {
        return;
    }
    if (!src) {
        src = "";
    }
    for (i = 0; i < cap - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static void put_u(char *dst, int n)
{
    char tmp[16];
    int i;
    int k;
    unsigned u;

    if (n < 0) {
        n = 0;
    }
    u = (unsigned)n;
    if (u == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return;
    }
    i = 0;
    while (u && i < 15) {
        tmp[i++] = (char)('0' + (u % 10u));
        u /= 10u;
    }
    for (k = 0; k < i; k++) {
        dst[k] = tmp[i - 1 - k];
    }
    dst[i] = 0;
}

static int parse_i(const char *s)
{
    int n;
    int i;

    if (!s || s[0] < '0' || s[0] > '9') {
        return -1;
    }
    n = 0;
    for (i = 0; s[i] >= '0' && s[i] <= '9'; i++) {
        n = n * 10 + (s[i] - '0');
    }
    return i ? n : -1;
}

static int field_named(DatFile *f, int kind, const char *name)
{
    int n;
    int i;
    char got[32];
    int type;
    int size;
    int count;

    n = dat_fields(f, kind);
    for (i = 0; i < n; i++) {
        got[0] = 0;
        if (!dat_field(f, kind, i, got, (int)sizeof(got), &type, &size, &count)) {
            break;
        }
        if (df_eq(got, name)) {
            return i;
        }
    }
    return -1;
}

static int row_of_id(DatFile *f, const char *tag, int id)
{
    int kind;
    int n;
    int i;
    int idf;
    char want[16];
    char got[32];

    if (!f || id < 0) {
        return -1;
    }
    kind = dat_find_tag(f, tag);
    if (kind < 0) {
        return -1;
    }
    idf = field_named(f, kind, "id");
    if (idf < 0) {
        idf = 0;
    }
    put_u(want, id);
    n = dat_rows(f, kind);
    for (i = 0; i < n; i++) {
        got[0] = 0;
        dat_value(f, kind, i, idf, 0, got, (int)sizeof(got));
        if (df_eq(got, want)) {
            return i;
        }
    }
    return -1;
}

static void row_label_at(DatFile *f, const char *tag, int row, char *dst, int cap)
{
    int kind;
    char id[32];
    char label[64];

    dst[0] = 0;
    kind = f ? dat_find_tag(f, tag) : -1;
    if (kind < 0 || row < 0) {
        return;
    }
    dat_row_label(f, kind, row, id, (int)sizeof(id), label, (int)sizeof(label));
    copy_s(dst, cap, label[0] ? label : id);
}

int means_creature_row(DatFile *f, int id)
{
    return row_of_id(f, "ENEM", id);
}

int means_skill_row(DatFile *f, int id)
{
    return row_of_id(f, "SKIL", id);
}

int means_creature_name(DatFile *f, int id, char *name, int namecap)
{
    int row;

    if (!name || namecap < 1) {
        return 0;
    }
    name[0] = 0;
    row = means_creature_row(f, id);
    if (row < 0) {
        return 0;
    }
    row_label_at(f, "ENEM", row, name, namecap);
    return name[0] != 0;
}

int means_class_name(DatFile *f, int id, char *name, int namecap)
{
    int row;

    if (!name || namecap < 1) {
        return 0;
    }
    name[0] = 0;
    row = row_of_id(f, "CLAS", id);
    if (row < 0) {
        return 0;
    }
    row_label_at(f, "CLAS", row, name, namecap);
    return name[0] != 0;
}

int means_skill(DatFile *f, int id, char *name, int namecap, int *attr)
{
    int kind;
    int row;
    int af;
    char text[16];

    if (attr) {
        *attr = -1;
    }
    if (!name || namecap < 1) {
        return 0;
    }
    name[0] = 0;
    row = means_skill_row(f, id);
    if (row < 0) {
        return 0;
    }
    row_label_at(f, "SKIL", row, name, namecap);
    kind = dat_find_tag(f, "SKIL");
    af = kind >= 0 ? field_named(f, kind, "attribute") : -1;
    if (af >= 0) {
        text[0] = 0;
        dat_value(f, kind, row, af, 0, text, (int)sizeof(text));
        if (attr) {
            *attr = parse_i(text);
        }
    }
    return name[0] != 0;
}

int means_encounter(DatFile *f, int id, int *count, int *first)
{
    int kind;
    int row;
    int cf;
    int kf;
    char text[16];

    if (count) {
        *count = 0;
    }
    if (first) {
        *first = -1;
    }
    kind = f ? dat_find_tag(f, "ENCT") : -1;
    row = row_of_id(f, "ENCT", id);
    if (kind < 0 || row < 0) {
        return 0;
    }
    cf = field_named(f, kind, "count");
    kf = field_named(f, kind, "kind");
    if (cf >= 0 && count) {
        text[0] = 0;
        dat_value(f, kind, row, cf, 0, text, (int)sizeof(text));
        *count = parse_i(text);
    }
    if (kf >= 0 && first) {
        text[0] = 0;
        dat_value(f, kind, row, kf, 0, text, (int)sizeof(text));
        *first = parse_i(text);
    }
    return 1;
}

int means_kind_at(DatFile *f, int id, int elem, int *kind_id, char *name, int namecap)
{
    int kind;
    int row;
    int kf;
    int count;
    char text[16];

    if (kind_id) {
        *kind_id = -1;
    }
    if (name && namecap > 0) {
        name[0] = 0;
    }
    count = 0;
    if (!means_encounter(f, id, &count, 0) || elem < 0 || elem >= count) {
        return 0;
    }
    kind = dat_find_tag(f, "ENCT");
    row = row_of_id(f, "ENCT", id);
    kf = kind >= 0 ? field_named(f, kind, "kind") : -1;
    if (row < 0 || kf < 0) {
        return 0;
    }
    text[0] = 0;
    dat_value(f, kind, row, kf, elem, text, (int)sizeof(text));
    if (kind_id) {
        *kind_id = parse_i(text);
    }
    if (name && namecap > 0) {
        means_creature_name(f, parse_i(text), name, namecap);
    }
    return 1;
}

static void add_piece(char *dst, int cap, const char *piece)
{
    int n;
    int i;

    if (!dst || cap < 1) {
        return;
    }
    n = 0;
    while (dst[n]) {
        n++;
    }
    if (n > 0 && n < cap - 1) {
        dst[n++] = ',';
        if (n < cap - 1) {
            dst[n++] = ' ';
        }
    }
    if (!piece) {
        piece = "";
    }
    for (i = 0; piece[i] && n < cap - 1; i++) {
        dst[n++] = piece[i];
    }
    dst[n] = 0;
}

static void name_each(DatFile *f, int kind, int row, int field, int count, int which, char *dst, int cap)
{
    int e;
    char text[32];
    char name[64];

    dst[0] = 0;
    for (e = 0; e < count; e++) {
        text[0] = 0;
        name[0] = 0;
        dat_value(f, kind, row, field, e, text, (int)sizeof(text));
        if (which == 0) {
            means_creature_name(f, parse_i(text), name, (int)sizeof(name));
        } else if (which == 1) {
            means_skill(f, parse_i(text), name, (int)sizeof(name), 0);
        } else {
            {
                int attr;

                attr = parse_i(text);
                if (attr >= 0 && attr < ATTR_COUNT) {
                    copy_s(name, (int)sizeof(name), g_attr[attr]);
                }
            }
        }
        add_piece(dst, cap, name);
    }
}

static void cap_copy(char *dst, int cap, const char *src)
{
    copy_s(dst, cap, src);
    if (dst[0] >= 'a' && dst[0] <= 'z') {
        dst[0] = (char)(dst[0] - 'a' + 'A');
    }
}

static void weapon_cif(char *dst, int cap, int n)
{
    char num[8];
    int i;

    if (n < 0) {
        n = 0;
    }
    put_u(num, n);
    copy_s(dst, cap, "WEAPON");
    i = 0;
    while (dst[i]) {
        i++;
    }
    if (num[1] == 0 && i < cap - 1) {
        dst[i++] = '0';
    }
    copy_s(dst + i, cap - i, num);
    i = 0;
    while (dst[i]) {
        i++;
    }
    copy_s(dst + i, cap - i, ".CIF");
}

void means_of(DatFile *f, int kind, int row, int field, char *dst, int cap)
{
    char tag[8];
    char table[24];
    char fname[32];
    char text[32];
    int type;
    int size;
    int count;

    if (!dst || cap < 1) {
        return;
    }
    dst[0] = 0;
    if (!f || kind < 0 || row < 0 || field < 0) {
        return;
    }
    tag[0] = 0;
    fname[0] = 0;
    dat_kind(f, kind, tag, (int)sizeof(tag), table, (int)sizeof(table));
    if (!dat_field(f, kind, field, fname, (int)sizeof(fname), &type, &size, &count)) {
        return;
    }
    if (count < 1) {
        count = 1;
    }
    if (df_eq(tag, "ENCT") && df_eq(fname, "kind")) {
        int cf;
        int used;

        cf = field_named(f, kind, "count");
        used = count;
        if (cf >= 0) {
            text[0] = 0;
            dat_value(f, kind, row, cf, 0, text, (int)sizeof(text));
            if (parse_i(text) >= 0 && parse_i(text) < used) {
                used = parse_i(text);
            }
        }
        name_each(f, kind, row, field, used, 0, dst, cap);
        return;
    }
    if (df_eq(tag, "CLAS") && (df_eq(fname, "primary") || df_eq(fname, "major") || df_eq(fname, "minor"))) {
        name_each(f, kind, row, field, count, 1, dst, cap);
        return;
    }
    if ((df_eq(tag, "CLAS") || df_eq(tag, "SKIL") || df_eq(tag, "ENEM"))
        && (df_eq(fname, "attribute") || df_eq(fname, "attributes"))) {
        name_each(f, kind, row, field, count, 2, dst, cap);
        return;
    }
    if (df_eq(tag, "WEAP") && df_eq(fname, "skill")) {
        text[0] = 0;
        dat_value(f, kind, row, field, 0, text, (int)sizeof(text));
        means_skill(f, parse_i(text), dst, cap, 0);
        return;
    }
    if (df_eq(tag, "WEAP") && df_eq(fname, "cif")) {
        text[0] = 0;
        dat_value(f, kind, row, field, 0, text, (int)sizeof(text));
        weapon_cif(dst, cap, parse_i(text));
        return;
    }
    if (df_eq(tag, "CMON") && df_eq(fname, "season")) {
        const char *word;

        text[0] = 0;
        dat_value(f, kind, row, field, 0, text, (int)sizeof(text));
        word = clock_season_name(parse_i(text));
        cap_copy(dst, cap, word ? word : "");
        return;
    }
    if (df_eq(tag, "ENEM")
        && (df_eq(fname, "male") || df_eq(fname, "female") || df_eq(fname, "corpse_archive"))) {
        text[0] = 0;
        dat_value(f, kind, row, field, 0, text, (int)sizeof(text));
        if (parse_i(text) > 0) {
            texture_filename(parse_i(text), dst, cap);
        }
        return;
    }
}
