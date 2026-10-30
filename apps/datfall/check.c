/*
 * Host check for DATFALL's reader (docs/data.md). Cooks a real MAIN.DAT
 * from ARENA2 and checks the kinds, the rat, a row filter and a cut-off
 * chunk. No window.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "books.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/maindat.h"
#include "engine/assets/texture.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/palette.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/tables/classic.h"
#include "view.h"

static int g_fail;

static void expect(int ok, const char *msg)
{
    if (ok) {
        return;
    }
    fprintf(stderr, "datfall: %s\n", msg);
    g_fail = 1;
}

typedef struct Buf {
    df_u8 *p;
    df_u32 n;
    df_u32 cap;
} Buf;

static int put_buf(void *ctx, const void *src, df_u32 n)
{
    Buf *b;
    df_u32 cap;
    df_u8 *p;

    b = (Buf *)ctx;
    if (b->n + n > b->cap) {
        cap = b->cap ? b->cap : 4096u;
        while (cap < b->n + n) {
            if (cap > 0x40000000u) {
                return 0;
            }
            cap *= 2u;
        }
        p = (df_u8 *)realloc(b->p, cap);
        if (!p) {
            return 0;
        }
        b->p = p;
        b->cap = cap;
    }
    if (n) {
        memcpy(b->p + b->n, src, n);
    }
    b->n += n;
    return 1;
}

static int field_of(const DatFile *f, int kind, const char *name)
{
    int i;
    int n;
    int type;
    int size;
    int count;
    char got[16];

    n = dat_fields(f, kind);
    for (i = 0; i < n; i++) {
        if (!dat_field(f, kind, i, got, (int)sizeof(got), &type, &size, &count)) {
            return -1;
        }
        if (df_eq(got, name)) {
            return i;
        }
    }
    return -1;
}

static int kind_rows(const DatFile *f, const char *tag, int want)
{
    int k;

    k = dat_find_tag(f, tag);
    if (k < 0) {
        fprintf(stderr, "datfall: missing chunk %s\n", tag);
        g_fail = 1;
        return -1;
    }
    if (dat_rows(f, k) != want) {
        fprintf(stderr, "datfall: %s has %d rows, want %d\n", tag, dat_rows(f, k), want);
        g_fail = 1;
    }
    return k;
}

int main(void)
{
    Buf buf;
    DatFile *f;
    DatFile *cut;
    int enem;
    int head;
    int name;
    int health;
    int dmg;
    char text[64];
    char id[16];
    char label[32];
    char table[24];
    static const df_u8 past[] = {
        'G', 'D', 'A', 'T', 1, 0, 0, 0,
        'X', 'X', 'X', 'X', 100, 0, 0, 0
    };

    mem_init();
    vfs_mount("daggerfall/DAGGER/ARENA2");
    buf.p = 0;
    buf.n = 0;
    buf.cap = 0;
    if (!maindat_import(put_buf, &buf)) {
        fprintf(stderr, "datfall: MAIN.DAT was not cooked\n");
        free(buf.p);
        return 1;
    }
    f = dat_open(buf.p, buf.n);
    if (!f) {
        fprintf(stderr, "datfall: no memory\n");
        free(buf.p);
        return 1;
    }
    expect(dat_ok(f) && dat_version(f) == 1 && dat_size(f) == buf.n && !dat_warning(f)[0],
           "cooked MAIN.DAT did not read as a whole record file");
    expect(dat_kinds(f) >= 11, "cooked MAIN.DAT is missing chunks");

    head = kind_rows(f, "HEAD", 1 + 1 + 18);
    if (head >= 0) {
        dat_kind(f, head, text, (int)sizeof(text), table, (int)sizeof(table));
        expect(df_eq(table, "head"), "HEAD's table name is not head");
    }
    enem = kind_rows(f, "ENEM", 62);
    kind_rows(f, "ENCT", 20);
    kind_rows(f, "WTHR", 40);
    kind_rows(f, "WLOK", 7);
    kind_rows(f, "CMON", 12);
    kind_rows(f, "CWEK", 7);
    kind_rows(f, "CALE", 1);
    kind_rows(f, "CLAS", 18);
    kind_rows(f, "RACE", 8);
    kind_rows(f, "SKIL", 35);

    if (enem >= 0) {
        dat_kind(f, enem, text, (int)sizeof(text), table, (int)sizeof(table));
        expect(df_eq(table, "enemy"), "ENEM's table name is not enemy");
        dat_row_label(f, enem, 0, id, (int)sizeof(id), label, (int)sizeof(label));
        expect(df_eq(id, "0") && df_eq(label, "Rat"), "row 0 is not the rat");
        name = field_of(f, enem, "name");
        health = field_of(f, enem, "health_min");
        dmg = field_of(f, enem, "damage_max");
        expect(name >= 0 && health >= 0 && dmg >= 0, "the rat's fields are missing");
        text[0] = 0;
        if (health >= 0) {
            dat_value(f, enem, 0, health, 0, text, (int)sizeof(text));
        }
        expect(df_eq(text, "9"), "the rat's least health is not 9");
        text[0] = 0;
        if (dmg >= 0) {
            dat_value(f, enem, 0, dmg, 0, text, (int)sizeof(text));
        }
        expect(df_eq(text, "4"), "the rat's first damage_max is not 4");
        expect(dat_filter(f, enem, "rat") >= 1, "filter rat matched nothing");
        dat_row_label(f, enem, dat_filter_row(f, 0), id, (int)sizeof(id), label, (int)sizeof(label));
        expect(df_eq(label, "Rat"), "the first rat match is not Rat");
        expect(dat_filter(f, enem, "zzz-no-such") == 0, "a miss still matched a row");
        expect(dat_kind_pick(f, "skill") == dat_find_tag(f, "SKIL"), "skill did not pick SKIL");
        expect(dat_kind_pick(f, "en") == enem, "en did not pick ENEM");
    }

    cut = dat_open(past, (df_u32)sizeof(past));
    expect(cut && dat_ok(cut) && dat_warning(cut)[0] && dat_kinds(cut) == 0,
           "a chunk past the end was not reported");
    dat_close(cut);
    dat_close(f);

    /* Every chunk's fields, and a mod's new field and new table over the rat. */
    {
        typedef struct EnMod {
            df_u16 id;
            df_u16 health_min;
            char note[8];
        } EnMod;
        typedef struct NoteRow {
            df_u16 id;
            char note[8];
        } NoteRow;
        static const RecField en_fields[] = {
            REC_FIELD("id", REC_UINT, EnMod, id),
            REC_FIELD("health_min", REC_UINT, EnMod, health_min),
            REC_FIELD("note", REC_TEXT, EnMod, note)
        };
        static const RecField note_fields[] = {
            REC_FIELD("id", REC_UINT, NoteRow, id),
            REC_FIELD("note", REC_TEXT, NoteRow, note)
        };
        static const RecSchema en_schema = {
            "ENEM", "enemy", en_fields, 3, (df_u16)sizeof(EnMod), 0
        };
        static const RecSchema note_schema = {
            "NOTE", "note", note_fields, 2, (df_u16)sizeof(NoteRow), 0
        };
        EnMod enem;
        NoteRow note;
        Buf mod;
        Books *books;
        DatFile *merged;
        DatFile *alone;
        const char *names[3];
        const df_u8 *bytes[3];
        df_u32 sizes[3];
        char order_names[4][BOOKS_NAME];
        const char *order_text;
        int k;
        int nf;
        int fi;
        int note_f;
        int ek;
        int weight_a;
        int weight_m;
        char one[64];
        char two[64];
        static const char order_src[] = "# comment\n\nMAIN.DAT\nGOBLINS.DAT\n  extra.dat\n";

        order_text = order_src;
        expect(books_order((const df_u8 *)order_text, (df_u32)strlen(order_text), order_names, 4) == 2
                   && df_eq(order_names[0], "GOBLINS.DAT") && df_eq(order_names[1], "extra.dat"),
               "LOADORDR did not keep the two mod names");

        memset(&enem, 0, sizeof(enem));
        enem.id = 0;
        enem.health_min = 30;
        enem.note[0] = 'h';
        enem.note[1] = 'i';
        memset(&note, 0, sizeof(note));
        note.id = 1;
        note.note[0] = 'm';
        note.note[1] = 'o';
        note.note[2] = 'd';
        mod.p = 0;
        mod.n = 0;
        mod.cap = 0;
        expect(rec_put_begin(put_buf, &mod) && rec_put_rows(put_buf, &mod, &en_schema, &enem, 1, 0, 0)
                   && rec_put_rows(put_buf, &mod, &note_schema, &note, 1, 0, 0),
               "the mod was not written");
        names[0] = "MAIN.DAT";
        names[1] = "extra.dat";
        names[2] = "GONE.DAT";
        bytes[0] = buf.p;
        bytes[1] = mod.p;
        bytes[2] = 0;
        sizes[0] = buf.n;
        sizes[1] = mod.n;
        sizes[2] = 0;
        books = books_open(names, bytes, sizes, 3);
        if (!books) {
            fprintf(stderr, "datfall: books did not open\n");
            g_fail = 1;
        } else {
            merged = books_merged(books);
            alone = books_file(books, 0);
            expect(merged && dat_ok(merged) && alone && dat_ok(alone), "merged MAIN.DAT did not read");
            expect(!books_ok(books, 2), "a missing file was merged");
            expect(strstr(books_status(books), "MAIN.DAT") && strstr(books_status(books), "extra.dat")
                       && !strstr(books_status(books), "GONE.DAT"),
                   "the status bar names are not the files that merged");
            if (merged && alone) {
                expect(dat_kinds(merged) == dat_kinds(alone) + 1, "the new table was not merged");
                for (k = 0; k < dat_kinds(merged); k++) {
                    dat_kind(merged, k, text, (int)sizeof(text), table, (int)sizeof(table));
                    nf = dat_fields(merged, k);
                    printf("datfall: %s %s", text, table);
                    expect(nf > 0 && !dat_damaged(merged, k), "a merged chunk has no fields");
                    for (fi = 0; fi < nf; fi++) {
                        char fname[16];
                        int type;
                        int size;
                        int count;

                        expect(dat_field(merged, k, fi, fname, (int)sizeof(fname), &type, &size, &count)
                                   && fname[0],
                               "a field has no name");
                        printf(" %s", fname);
                    }
                    printf("\n");
                }
                ek = dat_find_tag(merged, "ENEM");
                expect(ek >= 0 && dat_rows(merged, ek) == 62, "merged ENEM is not 62 rows");
                if (ek >= 0) {
                    note_f = field_of(merged, ek, "note");
                    health = field_of(merged, ek, "health_min");
                    dat_row_label(merged, ek, 0, id, (int)sizeof(id), label, (int)sizeof(label));
                    expect(df_eq(label, "Rat"), "the merged rat lost its name");
                    text[0] = 0;
                    if (health >= 0) {
                        dat_value(merged, ek, 0, health, 0, text, (int)sizeof(text));
                    }
                    expect(df_eq(text, "30"), "the mod did not set the rat's health to 30");
                    text[0] = 0;
                    if (note_f >= 0) {
                        dat_value(merged, ek, 0, note_f, 0, text, (int)sizeof(text));
                    }
                    expect(note_f >= 0 && df_eq(text, "hi"), "the mod's new field did not show up");
                }
                k = dat_find_tag(merged, "NOTE");
                expect(k >= 0 && dat_rows(merged, k) == 1, "the mod's new table is missing");
                k = dat_find_tag(alone, "ITEM");
                ek = dat_find_tag(merged, "ITEM");
                if (k >= 0 && ek >= 0) {
                    weight_a = field_of(alone, k, "weight");
                    weight_m = field_of(merged, ek, "weight");
                    one[0] = 0;
                    two[0] = 0;
                    if (weight_a >= 0) {
                        dat_value(alone, k, 0, weight_a, 0, one, (int)sizeof(one));
                    }
                    if (weight_m >= 0) {
                        dat_value(merged, ek, 0, weight_m, 0, two, (int)sizeof(two));
                    }
                    expect(weight_a >= 0 && df_eq(one, two), "a float field did not survive the merge");
                }
            }
            books_close(books);
        }
        free(mod.p);
    }

    free(buf.p);

    /* Breton's body, the rat's front frame, and a weapon metal pixel.
     * No window. ARENA2 is already mounted above. */
    {
        const CareerRace *races;
        const EnemyKind *enemies;
        ImgPicture body;
        CifSet cif;
        Image *rat;
        Palette pal;
        const df_u8 *pb;
        df_u32 pn;
        int count;
        int metal;
        const CifFrame *swing;

        races = classic_races(&count);
        expect(count >= 1 && races && df_eq(races[0].name, "Breton")
                   && df_eq(races[0].body_male, "BODY00I0.IMG"),
               "Breton's body file is not BODY00I0.IMG");
        expect(img_load("BODY00I0.IMG", &body) && body.width == 69 && body.height == 144
                   && body.pixels && body.pixels[27] == 44,
               "Breton body pixel (27, 0) is not index 44");

        enemies = classic_enemies(&count);
        expect(count > 0 && enemies && enemies[0].id == 0 && enemies[0].male == 255,
               "the rat's pictures are not TEXTURE.255");
        texture_cache_limit(2u * 1024u * 1024u);
        rat = texture_load_frame(255, 0, 0);
        if (!rat || !rat->index) {
            fprintf(stderr, "datfall: the rat's front frame did not load\n");
            g_fail = 1;
        } else {
            expect(rat->width == 65 && rat->height == 67, "the rat's front frame is not 65x67");
            expect(texture_frame_count(255, 0) == 8, "the rat's front record is not 8 frames");
            expect(rat->index[26] == 189, "the rat's pixel (26, 0) is not index 189");
        }

        expect(cif_open("WEAPON00.CIF", &cif), "WEAPON00.CIF did not open");
        expect(cif.n > 1 && cif.frame[0].w == 60 && cif.frame[0].h == 86
                   && cif.frame[0].pixels && cif.frame[0].pixels[0] == 125,
               "WEAPON00's armed picture is not 60x86 index 125");
        metal = cif_metal(125, 4);
        expect(metal == 157, "dwarven did not move index 125 to 157");
        pb = vfs_read("ART_PAL.COL", &pn);
        expect(palette_load(pb, pn, &pal) && pal.r[157] > pal.b[157] && pal.g[157] > pal.b[157],
               "dwarven index 157 is not the yellow ramp");
        swing = cif_frame(&cif, 1);
        expect(swing && swing->w == 117 && swing->h == 200
                   && swing->pixels && swing->pixels[10 * 117 + 10] == 0,
               "WEAPON00's first swing is not 117x200");
        cif_close(&cif);
        texture_cache_clear();
    }

    if (g_fail) {
        return 1;
    }
    printf("datfall: MAIN.DAT ok\n");
    return 0;
}
