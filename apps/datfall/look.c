#include "look.h"

#include "made.h"
#include "paper.h"

#include "engine/core/df.h"

/*
 * Ranks: exact name, then a name that contains the text. Within a rank
 * the order is race, creature, weapon, class, skill, place, table,
 * picture (docs/cd-gfall.md).
 */

static int fold_c(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int same_name(const char *a, const char *b)
{
    int i;

    if (!a || !b) {
        return 0;
    }
    for (i = 0; a[i] || b[i]; i++) {
        if (fold_c(a[i]) != fold_c(b[i])) {
            return 0;
        }
    }
    return a[0] != 0;
}

static int has_name(const char *hay, const char *q)
{
    int i;
    int j;

    if (!q || !q[0]) {
        return 0;
    }
    if (!hay) {
        return 0;
    }
    for (i = 0; hay[i]; i++) {
        for (j = 0; q[j]; j++) {
            if (!hay[i + j] || fold_c(hay[i + j]) != fold_c(q[j])) {
                break;
            }
        }
        if (!q[j]) {
            return 1;
        }
    }
    return 0;
}

static int q_len(const char *q)
{
    int n;

    n = 0;
    if (!q) {
        return 0;
    }
    while (q[n]) {
        n++;
    }
    return n;
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

static void tex_name(char *dst, int cap, int file)
{
    char num[8];
    int n;
    int i;
    unsigned u;

    if (file < 0) {
        file = 0;
    }
    u = (unsigned)file;
    n = 0;
    if (u == 0) {
        num[n++] = '0';
    }
    while (u && n < 7) {
        num[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    }
    copy_s(dst, cap, "TEXTURE.");
    i = 0;
    while (dst[i]) {
        i++;
    }
    while (n < 3) {
        if (i < cap - 1) {
            dst[i++] = '0';
        }
        n++;
    }
    while (n > 0 && i < cap - 1) {
        dst[i++] = num[--n];
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
    return n;
}

struct Walk {
    DatFile *book;
    const DunRef *duns;
    int ndun;
    const DunRef *towns;
    int ntown;
    const TexArc *arcs;
    int narc;
    const char *q;
    LookHit *hits;
    int cap;
    int n;
    int more;
    int exact;
    int sub;
    int places;
};

static int push(struct Walk *w, int where, int row, int id, int extra)
{
    if (w->n >= w->cap) {
        w->more = 1;
        return 0;
    }
    if (!w->exact) {
        if (w->sub >= LOOK_SUB_MAX) {
            w->more = 1;
            return 0;
        }
        w->sub++;
    }
    w->hits[w->n].where = where;
    w->hits[w->n].exact = w->exact;
    w->hits[w->n].row = row;
    w->hits[w->n].id = id;
    w->hits[w->n].extra = extra;
    w->n++;
    return 1;
}

static int named(struct Walk *w, const char *name)
{
    if (!name || !name[0]) {
        return 0;
    }
    if (w->exact) {
        return same_name(name, w->q);
    }
    return has_name(name, w->q) && !same_name(name, w->q);
}

static void add_label_rows(struct Walk *w, const char *tag, int where)
{
    int kind;
    int n;
    int i;
    char id[32];
    char label[64];

    if (!w->book) {
        return;
    }
    kind = dat_find_tag(w->book, tag);
    if (kind < 0) {
        return;
    }
    n = dat_rows(w->book, kind);
    for (i = 0; i < n; i++) {
        id[0] = 0;
        label[0] = 0;
        dat_row_label(w->book, kind, i, id, (int)sizeof(id), label, (int)sizeof(label));
        if (!named(w, label)) {
            continue;
        }
        if (!push(w, where, i, parse_i(id), kind)) {
            return;
        }
    }
}

static void add_armour(struct Walk *w)
{
    int i;
    int id;
    char name[64];

    for (i = 0; i < 512; i++) {
        id = 0;
        name[0] = 0;
        if (!made_armour_at(w->book, i, &id, name, (int)sizeof(name))) {
            break;
        }
        if (!named(w, name)) {
            continue;
        }
        if (!push(w, LOOK_ARMOUR, i, id, 0)) {
            return;
        }
    }
}

static void add_weapons(struct Walk *w)
{
    int i;
    int id;
    char name[64];

    for (i = 0; i < 512; i++) {
        id = 0;
        name[0] = 0;
        if (!made_weapon_at(w->book, i, &id, name, (int)sizeof(name))) {
            break;
        }
        if (!named(w, name)) {
            continue;
        }
        if (!push(w, LOOK_WEAPON, i, id, 0)) {
            return;
        }
    }
}

static void add_places(struct Walk *w)
{
    int i;

    if (!w->places) {
        return;
    }
    for (i = 0; w->duns && i < w->ndun; i++) {
        if (!named(w, w->duns[i].name)) {
            continue;
        }
        if (!push(w, LOOK_PLACE, i, 0, w->duns[i].region)) {
            return;
        }
    }
    for (i = 0; w->towns && i < w->ntown; i++) {
        if (!named(w, w->towns[i].name)) {
            continue;
        }
        if (!push(w, LOOK_PLACE, i, 1, w->towns[i].region)) {
            return;
        }
    }
}

static int dedicated(const char *tag)
{
    return df_eq(tag, "RACE") || df_eq(tag, "ENEM") || df_eq(tag, "CLAS") || df_eq(tag, "SKIL")
        || df_eq(tag, "WEAP") || df_eq(tag, "ITEM") || df_eq(tag, "ARMR") || df_eq(tag, "MATL");
}

static void add_tables(struct Walk *w)
{
    int kinds;
    int k;
    int rows;
    int r;
    char tag[8];
    char table[24];
    char id[32];
    char label[64];

    if (!w->book) {
        return;
    }
    kinds = dat_kinds(w->book);
    for (k = 0; k < kinds; k++) {
        tag[0] = 0;
        table[0] = 0;
        dat_kind(w->book, k, tag, (int)sizeof(tag), table, (int)sizeof(table));
        if (named(w, tag) || named(w, table)) {
            if (!push(w, LOOK_TABLE, -1, k, 0)) {
                return;
            }
        }
        if (dedicated(tag)) {
            continue;
        }
        rows = dat_rows(w->book, k);
        for (r = 0; r < rows; r++) {
            id[0] = 0;
            label[0] = 0;
            dat_row_label(w->book, k, r, id, (int)sizeof(id), label, (int)sizeof(label));
            if (!named(w, label)) {
                continue;
            }
            if (!push(w, LOOK_TABLE, r, k, 0)) {
                return;
            }
        }
    }
}

static void add_pictures(struct Walk *w)
{
    int i;
    int kind;
    int rows;
    int row;
    int part;
    char name[32];

    for (i = 0; w->arcs && i < w->narc; i++) {
        tex_name(name, (int)sizeof(name), w->arcs[i].file);
        if (!named(w, name)) {
            continue;
        }
        if (!push(w, LOOK_PICTURE, w->arcs[i].file, -1, 0)) {
            return;
        }
    }
    if (!w->book) {
        return;
    }
    kind = dat_find_tag(w->book, "RACE");
    if (kind < 0) {
        return;
    }
    rows = dat_rows(w->book, kind);
    for (row = 0; row < rows; row++) {
        for (part = 0; part < PAPER_PARTS; part++) {
            name[0] = 0;
            if (!paper_file(w->book, row, part, name, (int)sizeof(name))) {
                continue;
            }
            if (!named(w, name)) {
                continue;
            }
            if (!push(w, LOOK_PICTURE, row, part, kind)) {
                return;
            }
        }
    }
}

static void one_pass(struct Walk *w)
{
    add_label_rows(w, "RACE", LOOK_RACE);
    add_label_rows(w, "ENEM", LOOK_CREATURE);
    add_weapons(w);
    add_armour(w);
    add_label_rows(w, "CLAS", LOOK_CLASS);
    add_label_rows(w, "SKIL", LOOK_SKILL);
    add_places(w);
    add_tables(w);
    add_pictures(w);
}

int look_search(DatFile *book, const DunRef *duns, int ndun, const DunRef *towns, int ntown,
                const TexArc *arcs, int narc, const char *q, LookHit *hits, int cap, int *more)
{
    struct Walk w;

    if (more) {
        *more = 0;
    }
    if (!q || !q[0] || !hits || cap < 1) {
        return 0;
    }
    w.book = book;
    w.duns = duns;
    w.ndun = ndun;
    w.towns = towns;
    w.ntown = ntown;
    w.arcs = arcs;
    w.narc = narc;
    w.q = q;
    w.hits = hits;
    w.cap = cap;
    w.n = 0;
    w.more = 0;
    w.sub = 0;
    w.places = q_len(q) >= 2;
    w.exact = 1;
    one_pass(&w);
    w.exact = 0;
    one_pass(&w);
    if (more) {
        *more = w.more;
    }
    return w.n;
}

const char *look_where_name(int where)
{
    if (where == LOOK_RACE) {
        return "Race";
    }
    if (where == LOOK_CREATURE) {
        return "Creature";
    }
    if (where == LOOK_WEAPON) {
        return "Weapon";
    }
    if (where == LOOK_ARMOUR) {
        return "Armour";
    }
    if (where == LOOK_CLASS) {
        return "Class";
    }
    if (where == LOOK_SKILL) {
        return "Skill";
    }
    if (where == LOOK_PLACE) {
        return "Place";
    }
    if (where == LOOK_TABLE) {
        return "Table";
    }
    if (where == LOOK_PICTURE) {
        return "Picture";
    }
    return "";
}

void look_hit_name(DatFile *book, const DunRef *duns, const DunRef *towns, const LookHit *hit,
                   char *dst, int cap)
{
    char id[32];
    char label[64];

    if (!dst || cap < 1) {
        return;
    }
    dst[0] = 0;
    if (!hit) {
        return;
    }
    if (hit->where == LOOK_PLACE) {
        if (hit->id && towns) {
            copy_s(dst, cap, towns[hit->row].name);
        } else if (duns) {
            copy_s(dst, cap, duns[hit->row].name);
        }
        return;
    }
    if (hit->where == LOOK_WEAPON) {
        int idn;

        idn = 0;
        made_weapon_at(book, hit->row, &idn, label, (int)sizeof(label));
        copy_s(dst, cap, label);
        return;
    }
    if (hit->where == LOOK_ARMOUR) {
        int idn;

        idn = 0;
        made_armour_at(book, hit->row, &idn, label, (int)sizeof(label));
        copy_s(dst, cap, label);
        return;
    }
    if (hit->where == LOOK_PICTURE && hit->id < 0) {
        tex_name(dst, cap, hit->row);
        return;
    }
    if (hit->where == LOOK_PICTURE) {
        paper_file(book, hit->row, hit->id, dst, cap);
        return;
    }
    if (hit->where == LOOK_TABLE && hit->row < 0) {
        dat_kind(book, hit->id, label, (int)sizeof(label), id, (int)sizeof(id));
        copy_s(dst, cap, label);
        return;
    }
    if (!book) {
        return;
    }
    id[0] = 0;
    label[0] = 0;
    if (hit->where == LOOK_TABLE) {
        dat_row_label(book, hit->id, hit->row, id, (int)sizeof(id), label, (int)sizeof(label));
    } else {
        dat_row_label(book, hit->extra, hit->row, id, (int)sizeof(id), label, (int)sizeof(label));
    }
    copy_s(dst, cap, label[0] ? label : id);
}
