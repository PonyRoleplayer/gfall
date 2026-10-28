/*
 * dfrec: reads and makes the game's .DAT files (docs/data.md). A native
 * command-line program, built beside hostcheck from the same code.
 *
 *   dfrec import ARENA2 OUT.DAT    cook MAIN.DAT from classic's tables
 *   dfrec dump FILE.DAT [TAG]      every record as text
 *   dfrec build IN.TXT OUT.DAT     text to a mod .DAT, only the fields it sets
 *   dfrec same A.DAT B.DAT         1 when both merge to the same records
 *
 * The text a dump prints is the text build reads:
 *
 *   needs = MAIN.DAT
 *   [enemy 200]
 *   name = Goblin
 *   damage_max = 6, 0, 0
 *
 * A [table id] section names a table by its name or its chunk tag; a
 * field is the schema's name; a list fills an array from its start;
 * text runs to the end of the line. # starts a comment line. An error
 * names the file and line, and nothing is written.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/maindat.h"

#define LINE 512
/* Records one built file holds at most, and the needs it names. */
#define ROWS_MAX 4096
#define NEEDS_MAX 8

static int put_file(void *ctx, const void *src, df_u32 n)
{
    return fwrite(src, 1, n, (FILE *)ctx) == n;
}

static df_u8 *read_file(const char *path, df_u32 *n)
{
    FILE *f;
    long size;
    df_u8 *b;

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "dfrec: cannot open %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (df_u8 *)malloc(size > 0 ? (size_t)size : 1u);
    if (!b || (size > 0 && fread(b, 1, (size_t)size, f) != (size_t)size)) {
        fprintf(stderr, "dfrec: cannot read %s\n", path);
        fclose(f);
        free(b);
        return 0;
    }
    fclose(f);
    *n = (df_u32)size;
    return b;
}

/* The file name alone, for a HEAD's SELF row. */
static const char *base_name(const char *path)
{
    const char *s;

    s = path;
    for (; *path; path++) {
        if (*path == '/' || *path == '\\') {
            s = path + 1;
        }
    }
    return s;
}

static int cmd_import(const char *arena2, const char *out)
{
    FILE *f;
    int ok;

    vfs_mount(arena2);
    f = fopen(out, "wb");
    if (!f) {
        fprintf(stderr, "dfrec: cannot write %s\n", out);
        return 1;
    }
    ok = maindat_import(put_file, f);
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        fprintf(stderr, "dfrec: %s was not written whole\n", out);
        remove(out);
        return 1;
    }
    return 0;
}

static int cmd_dump(const char *path, const char *only)
{
    RecChunk c;
    df_u8 *b;
    df_u32 n;
    df_u32 pos;

    b = read_file(path, &n);
    if (!b) {
        return 1;
    }
    if (!rec_is_file(b, n)) {
        fprintf(stderr, "dfrec: %s is not a record file\n", path);
        return 1;
    }
    printf("# %s\n", base_name(path));
    pos = 0;
    while (rec_next(b, n, &pos, &c)) {
        RecTable *t;
        RecView v;
        const char *table;
        int head;
        int r;

        if (only && strcmp(only, c.tag) != 0) {
            continue;
        }
        if (!rec_view(&c, &v)) {
            printf("# %s: not a record chunk, %u bytes\n", c.tag, c.size);
            continue;
        }
        t = maindat_find(c.tag);
        head = strcmp(c.tag, rec_head_schema.tag) == 0;
        table = t ? t->schema->table : c.tag;
        for (r = 0; r < v.nrow; r++) {
            df_i32 iv;
            float fv;
            char text[256];
            int i;

            if (head) {
                df_i32 role;
                df_i32 size;
                char name[REC_NAME];
                int type;
                int fsize;
                int count;

                role = -1;
                size = 0;
                name[0] = 0;
                for (i = 0; i < v.nfield; i++) {
                    char fname[REC_NAME];

                    rec_view_field(&v, i, fname, &type, &fsize, &count);
                    rec_view_value(&v, r, i, 0, &iv, &fv, text, (int)sizeof(text));
                    if (strcmp(fname, "role") == 0) {
                        role = iv;
                    } else if (strcmp(fname, "size") == 0) {
                        size = iv;
                    } else if (strcmp(fname, "name") == 0) {
                        strncpy(name, text, REC_NAME - 1);
                        name[REC_NAME - 1] = 0;
                    }
                }
                if (role == REC_HEAD_NEEDS) {
                    printf("needs = %s\n", name);
                } else if (role == REC_HEAD_SELF) {
                    printf("# made by version %d\n", (int)size);
                } else {
                    printf("# from %s, %d bytes\n", name, (int)size);
                }
                continue;
            }
            rec_view_value(&v, r, 0, 0, &iv, &fv, text, (int)sizeof(text));
            printf("[%s %d]\n", table, (int)iv);
            for (i = 1; i < v.nfield; i++) {
                char fname[REC_NAME];
                int type;
                int fsize;
                int count;
                int e;

                rec_view_field(&v, i, fname, &type, &fsize, &count);
                printf("%s = ", fname);
                for (e = 0; e < (type == REC_TEXT ? 1 : count); e++) {
                    type = rec_view_value(&v, r, i, e, &iv, &fv, text, (int)sizeof(text));
                    if (e > 0) {
                        printf(", ");
                    }
                    if (type == REC_TEXT) {
                        printf("%s", text);
                    } else if (type == REC_FLOAT) {
                        printf("%.9g", (double)fv);
                    } else if (type == REC_UINT && fsize == 4) {
                        printf("%u", (unsigned)iv);
                    } else {
                        printf("%d", (int)iv);
                    }
                }
                printf("\n");
            }
        }
    }
    return 0;
}

/* One record of a built file: its table, its row, and which fields the text set. */
typedef struct Built {
    RecTable *table;
    df_i32 id;
    df_u8 row[512];
    df_u8 set[REC_FIELDS];
} Built;

static Built g_built[ROWS_MAX];
static int g_nbuilt;
static char g_needs[NEEDS_MAX][REC_NAME];
static int g_nneeds;

static char *trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
        *--e = 0;
    }
    return s;
}

/* A number into a field's element: 0 when it is not a number or out of the type's range. */
static int set_number(const RecField *f, df_u8 *row, int e, const char *s)
{
    char *end;
    df_u8 *p;

    p = row + f->offset + e * f->size;
    if (f->type == REC_FLOAT) {
        float v;

        v = strtof(s, &end);
        if (end == s || *trim(end)) {
            return 0;
        }
        memcpy(p, &v, 4);
        return 1;
    } else {
        long long v;
        long long lo;
        long long hi;

        v = strtoll(s, &end, 0);
        if (end == s || *trim(end)) {
            return 0;
        }
        if (f->type == REC_INT) {
            lo = -(1LL << (f->size * 8 - 1));
            hi = (1LL << (f->size * 8 - 1)) - 1;
        } else {
            lo = 0;
            hi = (1LL << (f->size * 8)) - 1;
        }
        if (v < lo || v > hi) {
            return 0;
        }
        memcpy(p, &v, f->size);
        return 1;
    }
}

static Built *built_for(RecTable *t, df_i32 id)
{
    Built *b;
    char num[16];
    int i;

    for (i = 0; i < g_nbuilt; i++) {
        if (g_built[i].table == t && g_built[i].id == id) {
            return &g_built[i];
        }
    }
    if (g_nbuilt == ROWS_MAX) {
        return 0;
    }
    b = &g_built[g_nbuilt++];
    memset(b, 0, sizeof(*b));
    b->table = t;
    b->id = id;
    /* The id is field 0 of every schema, and always carried. */
    sprintf(num, "%d", (int)id);
    if (!set_number(&t->schema->field[0], b->row, 0, num)) {
        g_nbuilt--;
        return 0;
    }
    b->set[0] = 1;
    return b;
}

static int fail(const char *path, int line, const char *what, const char *s)
{
    fprintf(stderr, "%s:%d: %s%s%s\n", path, line, what, s ? ": " : "", s ? s : "");
    return 0;
}

static int parse(const char *path)
{
    FILE *f;
    char buf[LINE];
    Built *cur;
    int line;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "dfrec: cannot open %s\n", path);
        return 0;
    }
    cur = 0;
    line = 0;
    while (fgets(buf, sizeof(buf), f)) {
        char *s;
        char *eq;
        char *key;
        char *val;
        const RecSchema *sc;
        int i;

        line++;
        s = trim(buf);
        if (!*s || *s == '#') {
            continue;
        }
        if (*s == '[') {
            char name[64];
            long id;
            RecTable *t;

            if (sscanf(s, "[%63s %ld]", name, &id) != 2 || s[strlen(s) - 1] != ']') {
                fclose(f);
                return fail(path, line, "a section is [table id]", s);
            }
            t = maindat_find(name);
            if (!t) {
                fclose(f);
                return fail(path, line, "no such table", name);
            }
            cur = built_for(t, (df_i32)id);
            if (!cur) {
                fclose(f);
                return fail(path, line, "too many records, or an id out of range", 0);
            }
            continue;
        }
        eq = strchr(s, '=');
        if (!eq) {
            fclose(f);
            return fail(path, line, "a line is name = value", s);
        }
        *eq = 0;
        key = trim(s);
        val = trim(eq + 1);
        if (!cur) {
            if (strcmp(key, "needs") == 0 && g_nneeds < NEEDS_MAX && strlen(val) < REC_NAME) {
                strcpy(g_needs[g_nneeds++], val);
                continue;
            }
            fclose(f);
            return fail(path, line, "before any [table id], only needs = FILE.DAT", key);
        }
        sc = cur->table->schema;
        for (i = 1; i < sc->nfield; i++) {
            if (strcmp(sc->field[i].name, key) == 0) {
                break;
            }
        }
        if (i == sc->nfield) {
            fclose(f);
            return fail(path, line, "no such field", key);
        }
        if (sc->field[i].type == REC_TEXT) {
            if ((int)strlen(val) >= sc->field[i].size) {
                fclose(f);
                return fail(path, line, "text too long", key);
            }
            memset(cur->row + sc->field[i].offset, 0, sc->field[i].size);
            memcpy(cur->row + sc->field[i].offset, val, strlen(val));
        } else {
            int e;
            char *part;

            e = 0;
            part = strtok(val, ",");
            while (part) {
                if (e >= sc->field[i].count || !set_number(&sc->field[i], cur->row, e, trim(part))) {
                    fclose(f);
                    return fail(path, line, e >= sc->field[i].count ? "too many values" : "not a number in range", key);
                }
                e++;
                part = strtok(0, ",");
            }
            if (e == 0) {
                fclose(f);
                return fail(path, line, "no value", key);
            }
        }
        cur->set[i] = 1;
    }
    fclose(f);
    return 1;
}

/*
 * The built records as chunks: one a table and set of fields, so each
 * record carries only the fields its text named.
 */
static int write_built(FILE *f, const char *out)
{
    RecHead head[1 + NEEDS_MAX];
    df_u8 done[ROWS_MAX];
    static df_u8 rows[ROWS_MAX * 512];
    int i;

    memset(head, 0, sizeof(head));
    head[0].role = REC_HEAD_SELF;
    strncpy(head[0].name, base_name(out), REC_NAME - 1);
    for (i = 0; i < g_nneeds; i++) {
        head[1 + i].id = (df_u16)(1 + i);
        head[1 + i].role = REC_HEAD_NEEDS;
        strcpy(head[1 + i].name, g_needs[i]);
    }
    if (!rec_put_begin(put_file, f) || !rec_put_rows(put_file, f, &rec_head_schema, head, 1 + g_nneeds, 0, 0)) {
        return 0;
    }
    memset(done, 0, sizeof(done));
    for (i = 0; i < g_nbuilt; i++) {
        const RecSchema *sc;
        int fields[REC_FIELDS];
        int nf;
        int n;
        int k;

        if (done[i]) {
            continue;
        }
        sc = g_built[i].table->schema;
        nf = 0;
        for (k = 0; k < sc->nfield; k++) {
            if (g_built[i].set[k]) {
                fields[nf++] = k;
            }
        }
        n = 0;
        for (k = i; k < g_nbuilt; k++) {
            if (!done[k] && g_built[k].table == g_built[i].table
                && memcmp(g_built[k].set, g_built[i].set, REC_FIELDS) == 0) {
                memcpy(rows + (size_t)n * sc->row_size, g_built[k].row, sc->row_size);
                done[k] = 1;
                n++;
            }
        }
        if (!rec_put_rows(put_file, f, sc, rows, n, fields, nf)) {
            return 0;
        }
    }
    return 1;
}

static int cmd_build(const char *in, const char *out)
{
    FILE *f;
    int ok;

    if (strlen(base_name(out)) >= REC_NAME) {
        fprintf(stderr, "dfrec: %s: a .DAT's name is under %d characters\n", out, REC_NAME);
        return 1;
    }
    if (!parse(in)) {
        return 1;
    }
    f = fopen(out, "wb");
    if (!f) {
        fprintf(stderr, "dfrec: cannot write %s\n", out);
        return 1;
    }
    ok = write_built(f, out);
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        fprintf(stderr, "dfrec: %s was not written whole\n", out);
        remove(out);
        return 1;
    }
    return 0;
}

/* Every table's rows, copied, after merging one file. */
typedef struct Snap {
    df_u8 *rows[16];
    int count[16];
} Snap;

static int snap_file(const char *path, Snap *s)
{
    df_u8 *b;
    df_u32 n;
    int i;

    b = read_file(path, &n);
    if (!b) {
        return 0;
    }
    maindat_reset();
    if (!maindat_merge(b, n, base_name(path))) {
        fprintf(stderr, "dfrec: %s did not merge\n", path);
        return 0;
    }
    for (i = 0; i < maindat_table_count() && i < 16; i++) {
        RecTable *t;
        size_t bytes;

        t = maindat_table(i);
        bytes = (size_t)t->count * t->schema->row_size;
        s->count[i] = t->count;
        s->rows[i] = (df_u8 *)malloc(bytes ? bytes : 1u);
        memcpy(s->rows[i], t->rows, bytes);
    }
    free(b);
    return 1;
}

static int cmd_same(const char *a, const char *b)
{
    Snap sa;
    Snap sb;
    int i;

    if (!snap_file(a, &sa) || !snap_file(b, &sb)) {
        return 1;
    }
    /* sb was merged last: its rows are the tables' now. Find each of sa's by id. */
    for (i = 0; i < maindat_table_count() && i < 16; i++) {
        RecTable *t;
        int r;

        t = maindat_table(i);
        if (sa.count[i] != sb.count[i]) {
            printf("%s: %d records against %d\n", t->schema->table, sa.count[i], sb.count[i]);
            return 1;
        }
        for (r = 0; r < sa.count[i]; r++) {
            const df_u8 *ra;
            const df_u8 *rb;
            df_i32 id;
            const RecField *f;

            ra = sa.rows[i] + (size_t)r * t->schema->row_size;
            f = &t->schema->field[0];
            id = 0;
            memcpy(&id, ra + f->offset, f->size);
            rb = (const df_u8 *)rec_row(t, id);
            if (!rb || memcmp(ra, rb, t->schema->row_size) != 0) {
                printf("%s %d differs\n", t->schema->table, (int)id);
                return 1;
            }
        }
    }
    printf("same\n");
    return 0;
}

int main(int argc, char **argv)
{
    mem_init();
    if (argc == 4 && strcmp(argv[1], "import") == 0) {
        return cmd_import(argv[2], argv[3]);
    }
    if ((argc == 3 || argc == 4) && strcmp(argv[1], "dump") == 0) {
        return cmd_dump(argv[2], argc == 4 ? argv[3] : 0);
    }
    if (argc == 4 && strcmp(argv[1], "build") == 0) {
        return cmd_build(argv[2], argv[3]);
    }
    if (argc == 4 && strcmp(argv[1], "same") == 0) {
        return cmd_same(argv[2], argv[3]);
    }
    fprintf(stderr,
            "usage: dfrec import ARENA2 OUT.DAT\n"
            "       dfrec dump FILE.DAT [TAG]\n"
            "       dfrec build IN.TXT OUT.DAT\n"
            "       dfrec same A.DAT B.DAT\n");
    return 2;
}
