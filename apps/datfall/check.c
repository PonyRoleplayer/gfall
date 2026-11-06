/*
 * Host check for DATFALL's reader (docs/data.md). Cooks a real MAIN.DAT
 * from ARENA2 and checks the kinds, the rat, a row filter and a cut-off
 * chunk. No window.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "beast.h"
#include "books.h"
#include "look.h"
#include "made.h"
#include "means.h"
#include "paper.h"
#include "pics.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/worldmap.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/maindat.h"
#include "engine/assets/texture.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/palette.h"
#include "game/dagger/formats/texture.h"
#include "engine/scene/clock.h"
#include "game/dagger/tables/careers.h"
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

    head = kind_rows(f, "HEAD", 1 + 1 + 1 + 18);
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
    kind_rows(f, "EFCT", 90);
    kind_rows(f, "SPEL", 89);

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
            {
                int rk;
                int rows;
                int row;
                int found;
                char id[32];
                char label[64];
                char file[PAPER_NAME];

                rk = dat_find_tag(merged, "RACE");
                rows = rk >= 0 ? dat_rows(merged, rk) : 0;
                found = 0;
                for (row = 0; row < rows; row++) {
                    dat_row_label(merged, rk, row, id, (int)sizeof(id), label, (int)sizeof(label));
                    if (!df_eq(label, "Breton")) {
                        continue;
                    }
                    found = 1;
                    file[0] = 0;
                    expect(paper_file(merged, row, 5, file, (int)sizeof(file))
                               && df_eq(file, "FACE00I0.CIF"),
                           "Breton's heads_male is not FACE00I0.CIF");
                    expect(paper_heads(file) == 10, "Breton does not have 10 heads");
                }
                expect(found, "merged RACE has no Breton");
            }
            {
                int ek;
                int rows;
                int row;
                int found;
                int pi;
                char id[32];
                char label[64];
                BeastPose poses[BEAST_POSES];
                int np;
                int saw_walk;
                int saw_stand;
                int saw_corpse;

                ek = dat_find_tag(merged, "ENEM");
                rows = ek >= 0 ? dat_rows(merged, ek) : 0;
                found = 0;
                saw_walk = 0;
                saw_stand = 0;
                saw_corpse = 0;
                for (row = 0; row < rows; row++) {
                    dat_row_label(merged, ek, row, id, (int)sizeof(id), label, (int)sizeof(label));
                    if (!df_eq(label, "Rat")) {
                        continue;
                    }
                    found = 1;
                    np = beast_poses(merged, row, poses, BEAST_POSES);
                    for (pi = 0; pi < np; pi++) {
                        if (df_eq(poses[pi].label, "Walk")) {
                            saw_walk = poses[pi].archive == 255 && poses[pi].record == 0
                                       && poses[pi].views == 5;
                        }
                        if (df_eq(poses[pi].label, "Attack")) {
                            expect(poses[pi].record == 5 && poses[pi].views == 5,
                                   "the rat's attack is not records 5..9");
                        }
                        if (df_eq(poses[pi].label, "Hurt")) {
                            expect(poses[pi].record == 10 && poses[pi].views == 5,
                                   "the rat's hurt is not records 10..14");
                        }
                        if (df_eq(poses[pi].label, "Stand")) {
                            saw_stand = poses[pi].record == 15 && poses[pi].views == 5
                                        && poses[pi].archive == 255;
                        }
                        if (df_eq(poses[pi].label, "Corpse")) {
                            saw_corpse = poses[pi].archive == 401 && poses[pi].record == 1
                                         && poses[pi].views == 1;
                        }
                    }
                }
                expect(found, "merged ENEM has no Rat");
                expect(saw_walk, "the rat's walk is not TEXTURE.255 records 0..4");
                expect(saw_stand, "the rat's stand is not records 15..19");
                expect(saw_corpse, "the rat's corpse is not TEXTURE.401 record 1");
            }
            {
                MadeItem made;

                expect(made_of(merged, 120, 1, &made) && made.quarters == 22
                           && made.value == 90 && made.condition == 1200,
                       "a steel longsword is not 5.50 kg, value 90, condition 1200");
                expect(made_cif(merged, 120) == 4, "the longsword's CIF is not WEAPON04.CIF");
            }
            {
                MadeItem made;
                int arch;
                int rec;
                int low;
                int high;

                arch = 0;
                rec = 0;
                low = 0;
                high = 0;
                expect(made_armour(merged, 102, 0, &made) && made.quarters == 50 && made.value == 300
                           && made.condition == 4096,
                       "an iron cuirass is not 12.50 kg, value 300, condition 4096");
                expect(made_variant(merged, 102, 0, &low, &high)
                           && made_picture(merged, 102, 0, low, 1, 2, &arch, &rec)
                           && arch == 251 && rec == 3 + low,
                       "a Breton man's iron cuirass is not archive 251 at player_record plus plate_low");
                expect(cif_metal(125, 10) == 125, "leather recoloured a metal index");
            }
            {
                int nd;
                int nt;
                int n;
                int i;
                int more;
                int place;
                int px;
                int py;
                LookHit hits[64];
                DunRef *duns;
                DunRef *towns;
                char tag[8];
                char table[24];
                char name[64];

                nd = dungeon_list(0, 0);
                nt = exterior_list(0, 0);
                duns = (DunRef *)mem_alloc((df_u32)nd * (df_u32)sizeof(DunRef));
                towns = (DunRef *)mem_alloc((df_u32)nt * (df_u32)sizeof(DunRef));
                expect(duns && towns && dungeon_list(duns, nd) == nd && exterior_list(towns, nt) == nt,
                       "lookup had no place index");
                if (duns && towns) {
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "breton", hits, 64, &more);
                    expect(n > 0 && hits[0].where == LOOK_RACE && hits[0].id == 1,
                           "breton did not return the race");
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "rat", hits, 64, &more);
                    look_hit_name(merged, duns, towns, n > 0 ? &hits[0] : 0, name, (int)sizeof(name));
                    expect(n > 0 && hits[0].where == LOOK_CREATURE && df_eq(name, "Rat"),
                           "rat did not return the creature");
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "longsword", hits, 64, &more);
                    expect(n > 0 && hits[0].where == LOOK_WEAPON && hits[0].id == 120,
                           "longsword did not return the weapon template");
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "daggerfall", hits, 64, &more);
                    px = -1;
                    py = -1;
                    place = 0;
                    if (worldmap_open()) {
                        for (i = 0; i < n; i++) {
                            if (hits[i].where != LOOK_PLACE || !hits[i].id || hits[i].extra != 17) {
                                continue;
                            }
                            if (worldmap_pixel_of(towns[hits[i].row].region, towns[hits[i].row].index, &px, &py)
                                && px == 207 && py == 213) {
                                place = 1;
                            }
                        }
                    }
                    expect(place, "daggerfall did not return the place at 207,213");
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "midyear", hits, 64, &more);
                    tag[0] = 0;
                    if (n > 0 && hits[0].where == LOOK_TABLE) {
                        dat_kind(merged, hits[0].id, tag, (int)sizeof(tag), table, (int)sizeof(table));
                    }
                    expect(n > 0 && hits[0].where == LOOK_TABLE && df_eq(tag, "CMON"),
                           "midyear was not found as a CMON row");
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "warrior", hits, 64, &more);
                    /* Skeletal Warrior contains the text, so it follows the two exact rows. */
                    expect(n >= 2 && hits[0].where == LOOK_CREATURE && hits[0].id == 144
                               && hits[1].where == LOOK_CLASS && hits[1].id == 16,
                           "warrior is not the creature then the class");
                    n = look_search(merged, duns, nd, towns, nt, 0, 0, "d", hits, 64, &more);
                    place = 0;
                    for (i = 0; i < n; i++) {
                        if (hits[i].where == LOOK_PLACE) {
                            place = 1;
                        }
                    }
                    expect(!place, "a one-character query returned a place");
                    {
                        int hi;
                        int out;
                        int other;
                        Dungeon hold;
                        char line[80];
                        char want[80];

                        hi = -1;
                        for (i = 0; i < nd; i++) {
                            if (duns[i].region == 17 && df_eq(duns[i].name, "Privateer's Hold")) {
                                hi = i;
                            }
                        }
                        expect(hi >= 0 && dungeon_load_at(17, duns[hi].index, &hold),
                               "Privateer's Hold did not load");
                        other = -1;
                        line[0] = 0;
                        if (hi >= 0) {
                            out = dungeon_outside(17, hold.exterior_id);
                            expect(atlas_other_line(duns, nd, towns, nt, 17, 0, hold.location_id,
                                                    hold.exterior_id, line, (int)sizeof(line), &other)
                                       && other == out,
                                   "the hold's outside line does not match dungeon_outside");
                            want[0] = 0;
                            for (i = 0; i < nt; i++) {
                                if (towns[i].region == 17 && towns[i].index == out) {
                                    snprintf(want, sizeof(want), "Above: %s", towns[i].name);
                                }
                            }
                            expect(want[0] && df_eq(line, want), "the hold's Above line is not that town");
                        }
                        expect(df_eq(means_dungeon_type(2), "Human stronghold"),
                               "dungeon type 2 is not Human stronghold");
                    }
                }
            }
            {
                int count;
                int first;
                int attr;
                int kind;
                int row;
                int sf;
                char name[64];
                char season[16];
                char word[16];

                count = 0;
                first = -1;
                expect(means_encounter(merged, 2, &count, &first) && count == 20 && first == 144,
                       "encounter 2 is not 20 kinds starting at 144");
                name[0] = 0;
                expect(means_creature_name(merged, 144, name, (int)sizeof(name)) && df_eq(name, "Warrior"),
                       "kind 144 is not Warrior");
                name[0] = 0;
                expect(means_class_name(merged, 16, name, (int)sizeof(name)) && df_eq(name, "Warrior"),
                       "class 16 is not Warrior");
                attr = -1;
                name[0] = 0;
                expect(means_skill(merged, 21, name, (int)sizeof(name), &attr)
                           && df_eq(name, "Running") && attr == ATTR_SPEED,
                       "skill 21 is not Running, governed by Speed");
                kind = dat_find_tag(merged, "CMON");
                row = -1;
                sf = -1;
                if (kind >= 0) {
                    int i;
                    int n;
                    char id[16];
                    char label[32];

                    n = dat_rows(merged, kind);
                    for (i = 0; i < n; i++) {
                        label[0] = 0;
                        dat_row_label(merged, kind, i, id, (int)sizeof(id), label, (int)sizeof(label));
                        if (df_eq(label, "Midyear")) {
                            row = i;
                        }
                    }
                    sf = field_of(merged, kind, "season");
                }
                season[0] = 0;
                if (sf >= 0 && row >= 0) {
                    dat_value(merged, kind, row, sf, 0, season, (int)sizeof(season));
                }
                word[0] = 0;
                {
                    const char *got;

                    got = clock_season_name(season[0] ? atoi(season) : -1);
                    if (got && got[0]) {
                        word[0] = got[0] >= 'a' && got[0] <= 'z' ? (char)(got[0] - 'a' + 'A') : got[0];
                        word[1] = 0;
                        if (got[1]) {
                            strncat(word, got + 1, (size_t)sizeof(word) - 2);
                        }
                    }
                }
                expect(df_eq(clock_season_name(CLOCK_SUMMER), "summer") && df_eq(word, "Summer"),
                       "a month's season did not come from clock_season_name");
            }
            {
                int ck;
                int sk;
                int warrior;
                int i;
                int n;
                int fi;
                int grp;
                int type;
                int size;
                int count;
                int elem;
                char id[16];
                char label[64];
                char fname[32];
                char text[16];
                char skill[64];
                static const char *groups[3] = { "primary", "major", "minor" };

                ck = dat_find_tag(merged, "CLAS");
                sk = dat_find_tag(merged, "SKIL");
                expect(ck >= 0 && dat_rows(merged, ck) == 18, "CLAS is not 18 rows");
                expect(sk >= 0 && dat_rows(merged, sk) == 35, "SKIL is not 35 rows");
                warrior = -1;
                if (ck >= 0) {
                    n = dat_rows(merged, ck);
                    for (i = 0; i < n; i++) {
                        id[0] = 0;
                        dat_row_label(merged, ck, i, id, (int)sizeof(id), label, (int)sizeof(label));
                        if (df_eq(id, "16")) {
                            warrior = i;
                        }
                    }
                }
                expect(warrior >= 0, "class 16 has no row");
                if (warrior >= 0) {
                    for (grp = 0; grp < 3; grp++) {
                        fi = field_of(merged, ck, groups[grp]);
                        expect(fi >= 0, "a Warrior skill group is missing");
                        if (fi < 0) {
                            continue;
                        }
                        dat_field(merged, ck, fi, fname, (int)sizeof(fname), &type, &size, &count);
                        expect(count > 0, "a Warrior skill group is empty");
                        for (elem = 0; elem < count; elem++) {
                            text[0] = 0;
                            skill[0] = 0;
                            dat_value(merged, ck, warrior, fi, elem, text, (int)sizeof(text));
                            expect(means_skill(merged, atoi(text), skill, (int)sizeof(skill), 0) && skill[0],
                                   "a Warrior skill id did not resolve to a name");
                        }
                    }
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
        {
            TexArc arcs[TEX_SCAN];
            int na;
            int ai;
            int found;

            na = tex_list(arcs, TEX_SCAN);
            found = 0;
            for (ai = 0; ai < na && ai < TEX_SCAN; ai++) {
                if (arcs[ai].file == 255) {
                    found = arcs[ai].records;
                }
            }
            expect(na == 472, "ARENA2 does not have 472 TEXTURE archives");
            expect(found == 20 && texture_record_count(255) == 20,
                   "TEXTURE.255 does not have 20 records");
        }
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

    expect(WORLDMAP_W == 1000 && WORLDMAP_H == 500, "the world map is not 1000 x 500");
    expect(worldmap_open() && worldmap_height(207, 213) == 20
               && worldmap_climate(207, 213) == CLIMATE_WOODLANDS
               && worldmap_location_type(207, 213) == WORLDMAP_TOWN_CITY
               && worldmap_locations() == 15251,
           "pixel 207,213 is not Daggerfall, woodlands, height 20, a city");
    expect(df_eq(atlas_climate_name(CLIMATE_WOODLANDS), "Woodlands"),
           "woodlands has no climate name");
    expect(dungeon_list(0, 0) == 4232, "the dungeon index is not 4232");
    expect(exterior_list(0, 0) == 15251, "the outdoor index is not 15251");
    {
        int nd;
        int nt;
        int n;
        int i;
        int found;
        DunRef *duns;
        DunRef *towns;
        int *hi;
        unsigned char *ho;

        nd = 4232;
        nt = 15251;
        duns = (DunRef *)mem_alloc((df_u32)nd * (df_u32)sizeof(DunRef));
        towns = (DunRef *)mem_alloc((df_u32)nt * (df_u32)sizeof(DunRef));
        hi = (int *)mem_alloc((df_u32)(nd + nt) * (df_u32)sizeof(int));
        ho = (unsigned char *)mem_alloc((df_u32)(nd + nt));
        expect(duns && towns && hi && ho, "the place index was not allocated");
        if (duns && towns && hi && ho) {
            expect(dungeon_list(duns, nd) == nd && exterior_list(towns, nt) == nt,
                   "the place lists did not fill");
            n = atlas_place_hits(duns, nd, towns, nt, "Daggerfall", hi, ho, nd + nt);
            found = 0;
            for (i = 0; i < n; i++) {
                if (ho[i] && towns[hi[i]].region == 17 && df_eq(towns[hi[i]].name, "Daggerfall")) {
                    found = 1;
                }
            }
            expect(found, "Daggerfall was not in the place hits");
        }
    }

    if (g_fail) {
        return 1;
    }
    printf("datfall: MAIN.DAT ok\n");
    return 0;
}
