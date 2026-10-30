/*
 * Daggerfall content tests. Privateer's Hold: the placed blocks, the
 * start marker, a torch sprite, and the fly frame. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/formats/arch3d.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/skyfile.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/tables/forecast.h"
#include "game/dagger/tables/maindat.h"
#include "game/dagger/tables/calendar.h"
#include "engine/scene/weather.h"
#include "engine/scene/clock.h"
#include "engine/scene/collide.h"
#include "engine/scene/light.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/places/people.h"
#include "engine/scene/entity.h"
#include "engine/scene/walker.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/places/terrain.h"
#include "game/dagger/places/wres.h"
#include "engine/scene/stream.h"
#include "engine/scene/keep.h"
#include "game/dagger/places/dunstream.h"
#include "engine/scene/interact.h"
#include "game/dagger/formats/hmi.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/tables/sounds.h"
#include "engine/assets/loader.h"
#include "engine/sound/ambience.h"
#include "engine/sound/mixer.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"

static int write_ppm(const Image *img, const Palette *pal, const char *path)
{
    FILE *f;
    int y;
    int x;

    f = fopen(path, "wb");
    if (!f) {
        return 0;
    }
    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);
    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
            df_u8 index;
            df_u8 rgb[3];

            index = img->index[y * img->width + x];
            rgb[0] = pal->r[index];
            rgb[1] = pal->g[index];
            rgb[2] = pal->b[index];
            if (fwrite(rgb, 1, 3, f) != 3) {
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return 1;
}

static int unique_indices(const Image *img)
{
    df_u8 seen[256];
    int n;
    int i;
    int count;

    for (i = 0; i < 256; i++) {
        seen[i] = 0;
    }
    n = img->width * img->height;
    for (i = 0; i < n; i++) {
        seen[img->index[i]] = 1;
    }
    count = 0;
    for (i = 0; i < 256; i++) {
        count += seen[i];
    }
    return count;
}

/* The camera's axes, which the player's walking (check_play.c) and the compass turn by. */
static void check_fly(void)
{
    Camera cam;
    Basis basis;

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    check_expect(check_near(basis.fx, 0.0f, 0.02f) && check_near(basis.fz, 1.0f, 0.02f),
           "yaw 0 forward is not +Z");
    check_expect(check_near(basis.rx, 1.0f, 0.02f) && check_near(basis.rz, 0.0f, 0.02f),
           "yaw 0 right is not +X");
}

/* A growing buffer for a .DAT made in memory. */
typedef struct MemDat {
    df_u8 *bytes;
    df_u32 n;
    df_u32 cap;
} MemDat;

static int put_mem(void *ctx, const void *src, df_u32 n)
{
    MemDat *m;

    m = (MemDat *)ctx;
    if (m->n + n > m->cap) {
        df_u32 cap;
        df_u8 *b;

        cap = (m->cap ? m->cap * 2 : 4096) + n;
        b = (df_u8 *)realloc(m->bytes, cap);
        if (!b) {
            return 0;
        }
        m->bytes = b;
        m->cap = cap;
    }
    memcpy(m->bytes + m->n, src, n);
    m->n += n;
    return 1;
}

/* A mod's HEAD: its name, and what it needs before it (0 for nothing). */
static void mod_head(MemDat *m, const char *self, const char *needs)
{
    RecHead h[2];

    memset(h, 0, sizeof(h));
    h[0].role = REC_HEAD_SELF;
    df_copy(h[0].name, self, REC_NAME);
    h[1].id = 1;
    h[1].role = REC_HEAD_NEEDS;
    df_copy(h[1].name, needs ? needs : "", REC_NAME);
    rec_put_begin(put_mem, m);
    rec_put_rows(put_mem, m, &rec_head_schema, h, needs ? 2 : 1, 0, 0);
}

/* A schema's field index by name, for a mod that carries some fields. */
static int field_of(const RecSchema *s, const char *name)
{
    int i;

    for (i = 0; i < s->nfield; i++) {
        if (strcmp(s->field[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static MemDat g_main;

/*
 * MAIN.DAT (docs/data.md): cooked in memory from classic's tables, not
 * stale for this ARENA2, and its records the numbers the code held.
 * Then a load order of mods over it: a new kind, a field of the rat
 * alone, a later file winning, a file whose need is missing. The
 * tables are left as MAIN.DAT alone for the checks after it.
 */
static void check_data(void)
{
    const EnemyKind *rat;
    const EnemyKind *goblin;
    const Encounter *human;
    static const df_u16 human_kinds[ENCOUNTER_LEN] = {
        144, 136, 0, 4, 141, 133, 3, 129, 8, 145, 144, 136, 143, 24, 141, 23, 129, 21, 144, 28
    };
    WeatherLook before;
    WeatherLook after;
    MemDat mod;
    MemDat later;
    MemDat orphan;
    EnemyKind rows[2];
    WeatherKindLook look;
    int fields[6];
    int ok;
    int i;

    check_expect(maindat_import(put_mem, &g_main), "MAIN.DAT was not cooked");
    check_expect(!maindat_stale(g_main.bytes, g_main.n), "a MAIN.DAT just cooked is stale");
    maindat_reset();
    check_expect(maindat_merge(g_main.bytes, g_main.n, MAINDAT_NAME), "MAIN.DAT did not merge");
    weather_look(WEATHER_FOG, &before);
    maindat_apply();
    weather_look(WEATHER_FOG, &after);
    check_expect(enemy_table()->count == 62 && encounter_table()->count == 20
                     && weather_odds_table()->count == 40 && weather_look_table()->count == WEATHER_KINDS,
                 "MAIN.DAT does not hold 62 kinds, 20 encounter tables, 40 odds and 7 looks");
    check_expect(memcmp(&before, &after, sizeof(before)) == 0, "MAIN.DAT's fog is not the engine's own");
    /* The calendar: twelve months, seven weekdays, and the engine's own Tamriel byte for byte. */
    {
        Calendar cooked;
        Calendar tamriel;

        memset(&tamriel, 0, sizeof(tamriel));
        clock_calendar_default(&tamriel);
        check_expect(cal_month_table()->count == 12 && cal_weekday_table()->count == 7
                         && calendar_from_records(&cooked) && memcmp(&cooked, &tamriel, sizeof(cooked)) == 0,
                     "MAIN.DAT's calendar is not the engine's Tamriel");
    }
    rat = enemy_kind(ENEMY_RAT);
    check_expect(rat && strcmp(rat->name, "Rat") == 0 && rat->male == 255 && rat->has_idle
                     && rat->behaviour == ENEMY_GENERAL && rat->health_min == 9 && rat->health_max == 16
                     && rat->damage_max[0] == 4 && rat->armor == 6 && rat->move_sound == 115,
                 "the rat is not EnemyBasics' rat");
    human = (const Encounter *)rec_row(encounter_table(), DUN_TYPE_HUMAN_STRONGHOLD);
    ok = human && human->count == 20;
    for (i = 0; ok && i < ENCOUNTER_LEN; i++) {
        ok = human->kind[i] == human_kinds[i];
    }
    check_expect(ok, "the human stronghold's encounter table is not classic's");
    human = (const Encounter *)rec_row(encounter_table(), DUN_TYPE_CEMETERY);
    check_expect(human && human->count == 11, "the cemetery's table does not use eleven kinds");

    /* Stale: another importer's version, or another size of MONSTER.BSA. */
    {
        RecChunk c;
        df_u32 pos;
        df_u8 *copy;

        copy = (df_u8 *)malloc(g_main.n);
        memcpy(copy, g_main.bytes, g_main.n);
        pos = 0;
        rec_next(copy, g_main.n, &pos, &c);
        /* HEAD's rows: id 2, role 1, size 4, name 16; the first row's size is the version. */
        ((df_u8 *)c.data)[12 + 4 * 24 + 3] ^= 1u;
        check_expect(maindat_stale(copy, g_main.n), "a MAIN.DAT of another importer version is not stale");
        memcpy(copy, g_main.bytes, g_main.n);
        ((df_u8 *)c.data)[12 + 4 * 24 + 23 + 3] ^= 1u;
        check_expect(maindat_stale(copy, g_main.n), "a MAIN.DAT of another MONSTER.BSA is not stale");
        free(copy);
    }

    /*
     * Damaged though current: cut in half, or a chunk's field broken, it
     * does not merge whole (maindat_start cooks it again, then tells the
     * person to delete it).
     */
    check_expect(maindat_whole(), "MAIN.DAT just merged is not whole");
    /* Every field's name fits a chunk's 16 bytes with its terminator, or it is cut and never matches. */
    {
        int k;
        int f;
        int short_names;

        short_names = 1;
        for (k = 0; k < maindat_table_count(); k++) {
            const RecSchema *s;

            s = maindat_table(k)->schema;
            for (f = 0; f < s->nfield; f++) {
                if (strlen(s->field[f].name) >= REC_NAME) {
                    printf("data: %s's field %s is too long\n", s->tag, s->field[f].name);
                    short_names = 0;
                }
            }
        }
        check_expect(short_names, "a table's field name is too long for a chunk");
    }
    {
        RecChunk c;
        df_u32 pos;
        df_u8 *copy;

        maindat_reset();
        maindat_merge(g_main.bytes, g_main.n / 2, MAINDAT_NAME);
        check_expect(!maindat_whole(), "half a MAIN.DAT merges whole");
        copy = (df_u8 *)malloc(g_main.n);
        memcpy(copy, g_main.bytes, g_main.n);
        pos = 0;
        rec_next(copy, g_main.n, &pos, &c);
        rec_next(copy, g_main.n, &pos, &c);
        /* ENEM's first field's offset, past its row. */
        ((df_u8 *)c.data)[12 + 20] = 0xff;
        maindat_reset();
        maindat_merge(copy, g_main.n, MAINDAT_NAME);
        check_expect(!maindat_whole(), "a MAIN.DAT with a damaged chunk merges whole");
        free(copy);
        maindat_reset();
        maindat_merge(g_main.bytes, g_main.n, MAINDAT_NAME);
    }

    /* A mod: a goblin, kind 200, said in part; the rat's least health alone. */
    memset(&mod, 0, sizeof(mod));
    memset(rows, 0, sizeof(rows));
    rows[0].id = 200;
    df_copy(rows[0].name, "Goblin", 24);
    rows[0].male = 255;
    rows[0].female = 255;
    rows[0].health_min = 10;
    rows[0].health_max = 20;
    fields[0] = 0;
    fields[1] = field_of(&enemy_schema, "name");
    fields[2] = field_of(&enemy_schema, "male");
    fields[3] = field_of(&enemy_schema, "female");
    fields[4] = field_of(&enemy_schema, "health_min");
    fields[5] = field_of(&enemy_schema, "health_max");
    mod_head(&mod, "GOBLINS.DAT", MAINDAT_NAME);
    rec_put_rows(put_mem, &mod, &enemy_schema, rows, 1, fields, 6);
    rows[1].id = ENEMY_RAT;
    rows[1].health_min = 30;
    rec_put_rows(put_mem, &mod, &enemy_schema, &rows[1], 1, fields, 1);
    rec_put_rows(put_mem, &mod, &enemy_schema, &rows[1], 1, (fields[1] = fields[4], fields), 2);
    /* A calendar mod: Midyear renamed, nothing else of it. */
    {
        CalMonth month;

        memset(&month, 0, sizeof(month));
        month.id = 5;
        df_copy(month.name, "High Summer", CLOCK_NAME);
        fields[1] = field_of(&cal_month_schema, "name");
        rec_put_rows(put_mem, &mod, &cal_month_schema, &month, 1, fields, 2);
    }
    /* A look: the fog thicker, and nothing else of it. */
    memset(&look, 0, sizeof(look));
    look.id = WEATHER_FOG;
    look.fog = 300.0f;
    fields[1] = field_of(&weather_look_schema, "fog");
    rec_put_rows(put_mem, &mod, &weather_look_schema, &look, 1, fields, 2);
    check_expect(maindat_merge(mod.bytes, mod.n, "GOBLINS.DAT"), "a mod needing MAIN.DAT did not merge after it");
    maindat_apply();
    goblin = enemy_kind(200);
    rat = enemy_kind(ENEMY_RAT);
    weather_look(WEATHER_FOG, &after);
    check_expect(goblin && strcmp(goblin->name, "Goblin") == 0 && goblin->health_max == 20
                     && goblin->move_sound == -1 && goblin->level == 1 && enemy_archive(200, 0) == 255,
                 "a mod's new kind is not its fields over the defaults");
    check_expect(rat && rat->health_min == 30 && rat->health_max == 16 && strcmp(rat->name, "Rat") == 0,
                 "a mod's rat changed more than its least health");
    check_expect(after.fog == 300.0f && after.cloud == before.cloud && after.view == before.view,
                 "a mod's fog did not reach the engine alone");
    check_expect(strcmp(clock_month_name(5), "High Summer") == 0 && clock_month_days(5) == 30
                     && clock_months() == 12,
                 "a mod's month name did not reach the clock alone");

    /* A later file wins the field both set; one needing a file not merged is skipped. */
    memset(&later, 0, sizeof(later));
    mod_head(&later, "LATER.DAT", "GOBLINS.DAT");
    rows[1].health_min = 40;
    fields[1] = field_of(&enemy_schema, "health_min");
    rec_put_rows(put_mem, &later, &enemy_schema, &rows[1], 1, fields, 2);
    memset(&orphan, 0, sizeof(orphan));
    mod_head(&orphan, "ORPHAN.DAT", "NOPE.DAT");
    rows[1].health_min = 99;
    rec_put_rows(put_mem, &orphan, &enemy_schema, &rows[1], 1, fields, 2);
    check_expect(maindat_merge(later.bytes, later.n, "LATER.DAT") && enemy_kind(ENEMY_RAT)->health_min == 40,
                 "the later file does not win the rat's health");
    check_expect(!maindat_merge(orphan.bytes, orphan.n, "ORPHAN.DAT") && enemy_kind(ENEMY_RAT)->health_min == 40,
                 "a file whose need is missing was merged");
    printf("data: MAIN.DAT %u bytes, %d kinds, a mod's kind 200 and the rat's health over it\n",
           g_main.n, 62);
    free(mod.bytes);
    free(later.bytes);
    free(orphan.bytes);

    /* MAIN.DAT alone again, its looks back on the engine. */
    maindat_reset();
    maindat_merge(g_main.bytes, g_main.n, MAINDAT_NAME);
    maindat_apply();
}

/*
 * A creature marker's rules (enemies.c), on markers made up: a fixed
 * one's kind and gender, the invalid kind, a fish out of water and in
 * it, a random one above and below the surface, and the hold's draws.
 */
static void check_enemy_rules(void)
{
    Marker mk;
    int female;
    int passive;
    int kind;
    int i;
    int low;

    mem_zero(&mk, sizeof(mk));
    mk.record = ENEMY_FIXED_MARKER;
    mk.y = -500.0f;
    /* A mage, garbage in the high byte, the file saying a woman, passive. */
    mk.id = 0x5a00u | 128u;
    mk.flags = 1;
    mk.action = 99;
    kind = enemy_at(&mk, ENEMY_DRY, &female, &passive);
    check_expect(kind == 128 && female && passive && enemy_archive(kind, female) == 485,
                 "a fixed marker's mage, a woman, passive, is not read so");
    mk.id = 99;
    check_expect(enemy_at(&mk, ENEMY_DRY, &female, &passive) == -1, "a fixed marker of kind 99 places one");
    mk.id = 39;
    check_expect(enemy_at(&mk, ENEMY_DRY, &female, &passive) == -1, "a horse, which has no pictures, is placed");
    /* A slaughterfish: none in a dry block, nor 30 above the water; one at the surface. */
    mk.id = ENEMY_SLAUGHTERFISH;
    mk.flags = 0;
    mk.action = 0;
    kind = enemy_at(&mk, ENEMY_DRY, &female, &passive);
    check_expect(kind == -1, "a slaughterfish is placed in a dry block");
    check_expect(enemy_at(&mk, 530, &female, &passive) == -1, "a slaughterfish is placed 30 above the water");
    check_expect(enemy_at(&mk, 500, &female, &passive) == ENEMY_SLAUGHTERFISH,
                 "a slaughterfish at the water's surface is not placed");
    /*
     * Random markers in a crypt, a level-1 player: a dry one draws from
     * the crypt's first six, one under the water from the underwater
     * table's first six.
     */
    enemy_dungeon(1234, DUN_TYPE_CRYPT, 1);
    mk.record = ENEMY_RANDOM_MARKER;
    low = 1;
    for (i = 1; i < 256; i++) {
        int dry;
        int wet;

        mk.flags = (df_u16)i;
        dry = enemy_at(&mk, ENEMY_DRY, &female, &passive);
        wet = enemy_at(&mk, 400, &female, &passive);
        low = low && (dry == 0 || dry == 15 || dry == 3)
            && (wet == ENEMY_SLAUGHTERFISH || wet == 15 || wet == ENEMY_DREUGH);
    }
    check_expect(low, "a level-1 crypt's random markers draw past the tables' first six");
    check_expect(enemy_has_idle(ENEMY_RAT) && !enemy_has_idle(3) && enemy_flies(3)
                 && enemy_spectral(ENEMY_GHOST) && !enemy_flies(ENEMY_GHOST)
                 && enemy_archive(146, 1) == 399 && enemy_archive(40, 0) == 295,
                 "the kinds' table is not EnemyBasics'");
}

/*
 * The hold's creatures (a human stronghold): 42 where its markers are,
 * each with its five walking views, standing ones when its kind has
 * them, standing on a floor (a flyer where its marker is), and drawn.
 */
static void check_hold_creatures(const World *world, const Dungeon *hold)
{
    int count[ENEMY_KINDS];
    int i;
    int ok_views;
    int ok_floor;
    int floats;
    int drawn;

    check_expect(hold->dungeon_type == DUN_TYPE_HUMAN_STRONGHOLD,
                 "the hold is not a human stronghold in MAPTABLE");
    for (i = 0; i < ENEMY_KINDS; i++) {
        count[i] = 0;
    }
    ok_views = 1;
    ok_floor = 1;
    floats = 0;
    drawn = 0;
    for (i = 0; i < world_entity_count(world); i++) {
        const Entity *e;
        Flat fl;
        int kind;
        int passive;
        int k;

        e = world_entity(world, i);
        kind = world_entity_kind(world, i, &passive);
        if (kind < 0 || kind >= ENEMY_KINDS) {
            ok_views = 0;
            continue;
        }
        count[kind]++;
        for (k = 0; k < ENTITY_VIEWS; k++) {
            ok_views = ok_views && e->pose[ENTITY_WALK].slot[k] != 0xffff && world_image_cutout(world, e->pose[ENTITY_WALK].slot[k])
                && e->pose[ENTITY_WALK].w[k] > 0 && e->pose[ENTITY_WALK].h[k] > 0;
            ok_views = ok_views && (enemy_has_idle(kind) ? e->pose[ENTITY_IDLE].slot[k] != 0xffff : e->pose[ENTITY_IDLE].slot[0] == 0xffff);
        }
        ok_views = ok_views && e->live && !e->moving;
        if (enemy_flies(kind) || enemy_spectral(kind)) {
            floats++;
        } else {
            /* A floor just under its feet. */
            float o[3];
            float d[3];
            float t;
            int m;
            int under;

            o[0] = e->x;
            o[1] = e->y + 4.0f;
            o[2] = e->z;
            d[0] = 0.0f;
            d[1] = -1.0f;
            d[2] = 0.0f;
            under = 0;
            for (m = 0; m < world_model_count(world) && !under; m++) {
                under = instance_ray(world_instance(world, m), o, d, 8.0f, &t);
            }
            ok_floor = ok_floor && under;
        }
        drawn += entity_flat(e, e->x + 200.0f, e->z, &fl) && fl.y > e->y;
    }
    printf("hold creatures: %d (rats %d, imps %d, bats %d, bears %d, orcs %d, skeletons %d, "
           "rogues %d, thieves %d, archers %d), %d flying, %d drawn\n",
           world_entity_count(world), count[0], count[1], count[3], count[4], count[7], count[15],
           count[136], count[138], count[141], floats, drawn);
    check_expect(world_entity_count(world) == 42, "the hold does not have its 42 creatures");
    /* The draws are seeded by the location: the same every load. */
    check_expect(count[0] == 14 && count[3] == 6 && count[141] == 6 && floats == 9,
                 "the hold's creatures are not its draws (14 rats, 6 bats, 6 archers, 9 flying)");
    check_expect(ok_views, "a creature lacks a view, or its standing pictures are not its kind's");
    check_expect(ok_floor, "a creature that walks does not stand on a floor");
    check_expect(drawn == world_entity_count(world), "a creature is not drawn on its feet");
}

static void check_view(const World *world)
{
    int i;
    int found_floor;
    int found_wall;
    float floor_y;
    float floor_x;
    float floor_z;
    float wall_x0, wall_y0, wall_z0;
    float wall_x1, wall_y1, wall_z1;
    int wall_axis;
    Camera cam;
    Basis basis;
    float vx, vy, vz, sx0, sy0, sx1, sy1, oow, ooz;
    float minx, miny, minz, maxx, maxy, maxz;

    found_floor = 0;
    found_wall = 0;
    floor_y = 0.0f;
    floor_x = 0.0f;
    floor_z = 0.0f;
    wall_axis = 0;
    wall_x0 = wall_y0 = wall_z0 = 0.0f;
    wall_x1 = wall_y1 = wall_z1 = 0.0f;

    for (i = 0; i < world_model_count(world) && (!found_floor || !found_wall); i++) {
        const Instance *in;
        int t;

        in = world_instance(world, i);
        for (t = 0; t < in->mesh->ntris && (!found_floor || !found_wall); t++) {
            const Tri *tri;
            float x[3], y[3], z[3];
            int k;
            float yspan, xspan, zspan;

            tri = &in->mesh->tris[t];
            for (k = 0; k < 3; k++) {
                instance_point(in, tri->x[k], tri->y[k], tri->z[k], &x[k], &y[k], &z[k]);
            }
            yspan = df_absf(y[0] - y[1]) + df_absf(y[1] - y[2]);
            xspan = df_absf(x[0] - x[1]) + df_absf(x[1] - x[2]);
            zspan = df_absf(z[0] - z[1]) + df_absf(z[1] - z[2]);
            if (!found_floor && yspan < 0.05f && xspan > 2.0f && zspan > 2.0f) {
                found_floor = 1;
                floor_y = y[0];
                floor_x = (x[0] + x[1] + x[2]) / 3.0f;
                floor_z = (z[0] + z[1] + z[2]) / 3.0f;
            }
            if (!found_wall && yspan > 2.0f && (xspan < 0.05f || zspan < 0.05f)) {
                int hi;
                int lo;

                hi = 0;
                lo = 0;
                for (k = 1; k < 3; k++) {
                    if (y[k] > y[hi]) {
                        hi = k;
                    }
                    if (y[k] < y[lo]) {
                        lo = k;
                    }
                }
                if (y[hi] - y[lo] > 2.0f) {
                    found_wall = 1;
                    wall_axis = (xspan < 0.05f) ? 0 : 2;
                    wall_x0 = x[lo];
                    wall_y0 = y[lo];
                    wall_z0 = z[lo];
                    wall_x1 = x[hi];
                    wall_y1 = y[hi];
                    wall_z1 = z[hi];
                }
            }
        }
    }
    check_expect(found_floor, "no constant-Y floor");
    check_expect(found_wall, "no upright wall");
    printf("floor y %.2f at %.1f %.1f\n", floor_y, floor_x, floor_z);
    printf("wall axis %s  low y %.2f  high y %.2f\n",
           wall_axis == 0 ? "X" : "Z", wall_y0, wall_y1);

    world_bounds(world, &minx, &miny, &minz, &maxx, &maxy, &maxz);
    printf("bounds %.1f..%.1f  %.1f..%.1f  %.1f..%.1f\n",
           minx, maxx, miny, maxy, minz, maxz);
    check_expect(maxy > miny, "vertical extent is empty");

    camera_set(&cam, floor_x, floor_y, floor_z - 20.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    camera_view(&cam, &basis, floor_x, floor_y, floor_z + 30.0f, &vx, &vy, &vz);
    check_expect(camera_project(vx, vy, vz, &sx0, &sy0, &oow, &ooz), "level point is behind the camera");
    camera_view(&cam, &basis, floor_x, floor_y + 40.0f, floor_z + 30.0f, &vx, &vy, &vz);
    check_expect(camera_project(vx, vy, vz, &sx1, &sy1, &oow, &ooz), "raised point is behind the camera");
    check_expect(sy1 > sy0 + 5.0f, "higher world Y is not higher on screen");

    if (found_wall) {
        float cx, cy, cz;

        if (wall_axis == 0) {
            cx = wall_x0 - 30.0f;
            cy = (wall_y0 + wall_y1) * 0.5f;
            cz = (wall_z0 + wall_z1) * 0.5f;
            camera_set(&cam, cx, cy, cz, 1.5707963f, 0.0f);
        } else {
            cx = (wall_x0 + wall_x1) * 0.5f;
            cy = (wall_y0 + wall_y1) * 0.5f;
            cz = wall_z0 - 30.0f;
            camera_set(&cam, cx, cy, cz, 0.0f, 0.0f);
        }
        camera_basis(&cam, &basis);
        camera_view(&cam, &basis, wall_x0, wall_y0, wall_z0, &vx, &vy, &vz);
        check_expect(camera_project(vx, vy, vz, &sx0, &sy0, &oow, &ooz), "wall bottom is behind the camera");
        camera_view(&cam, &basis, wall_x1, wall_y1, wall_z1, &vx, &vy, &vz);
        check_expect(camera_project(vx, vy, vz, &sx1, &sy1, &oow, &ooz), "wall top is behind the camera");
        check_expect(sy1 > sy0 + 5.0f, "wall top is not above the wall bottom");
    }

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    camera_view(&cam, &basis, 10.0f, 0.0f, 20.0f, &vx, &vy, &vz);
    check_expect(check_near(vx, 10.0f, 0.02f) && check_near(vy, 0.0f, 0.02f) && check_near(vz, 20.0f, 0.02f),
           "yaw 0 did not put +X on the right and +Z in front");
}

static const Flat *find_flat(const World *world, int file, int image)
{
    int i;

    for (i = 0; i < world_flat_count(world); i++) {
        const Flat *fl;

        fl = world_flat(world, i);
        if (fl && (int)fl->file == file && (int)fl->image == image) {
            return fl;
        }
    }
    return 0;
}

/* This ARENA2 tree. MAPDITEM.002 and the other empty region files are four bytes. */
#define LEVEL_COUNT 4232

static DunRef g_region61;
static int g_have_region61;
/* Daggerfall's castle, location 50027: the interior culling case. */
static DunRef g_castle;
static int g_have_castle;
/* Anthor's Web: a dungeon with water in its lower part. */
static DunRef g_wet;
static int g_have_wet;

static void check_levels(void)
{
    DunRef *rows;
    Dungeon from_list;
    Dungeon hold;
    int n;
    int i;
    int found;
    int blocks;

    g_have_region61 = 0;
    g_have_castle = 0;
    rows = (DunRef *)mem_alloc((df_u32)8192 * (df_u32)sizeof(DunRef));
    check_expect(rows != 0, "dungeon list allocation");
    if (!rows) {
        return;
    }
    n = dungeon_list(rows, 8192);
    printf("dungeons %d\n", n);
    check_expect(n == LEVEL_COUNT, "dungeon count");
    found = -1;
    for (i = 0; i < n && i < 8192; i++) {
        if (rows[i].region == 61) {
            g_region61 = rows[i];
            g_have_region61 = 1;
        }
        if (rows[i].region == 17 && df_eq(rows[i].name, "Privateer's Hold")) {
            found = i;
        }
        if (rows[i].location_id == 50027) {
            g_castle = rows[i];
            g_have_castle = 1;
        }
        if (df_eq(rows[i].name, "Anthor's Web")) {
            g_wet = rows[i];
            g_have_wet = 1;
        }
    }
    check_expect(found >= 0 && rows[found].nblocks == 5 && rows[found].location_id == 50050
           && rows[found].outdoor == 0,
           "Privateer's Hold is not region 17 record with 5 blocks");
    if (found < 0) {
        return;
    }
    check_expect(dungeon_load_at(rows[found].region, rows[found].index, &from_list),
           dungeon_error());
    check_expect(dungeon_load(&hold), dungeon_error());
    check_expect(from_list.nblocks == hold.nblocks && from_list.location_id == hold.location_id
           && from_list.climate == hold.climate,
           "listed hold does not match dungeon_load");
    blocks = from_list.nblocks;
    if (hold.nblocks < blocks) {
        blocks = hold.nblocks;
    }
    for (i = 0; i < blocks; i++) {
        check_expect(df_eq(from_list.block[i].name, hold.block[i].name)
               && from_list.block[i].x == hold.block[i].x
               && from_list.block[i].z == hold.block[i].z
               && from_list.block[i].start == hold.block[i].start,
               "hold block list mismatch");
    }
}

/*
 * texture_smooth on a 3 x 2 wall stretched to 4 x 2: black, white,
 * black across. Texel centres land at -0.125, 0.625, 1.375 and 2.125
 * source texels, wrapping, so the ends stay black and the middle two
 * are 5/8 white: nearest the grey entry. Rows do not stretch and are
 * repeated to fill the 4 x 4 square.
 */
static void check_smooth(void)
{
    df_u8 pr[256];
    df_u8 pg[256];
    df_u8 pb[256];
    df_u8 src[6];
    df_u8 pot[16];
    Image img;
    int i;
    int ok;

    for (i = 0; i < 256; i++) {
        pr[i] = 255;
        pg[i] = 0;
        pb[i] = (df_u8)i;
    }
    pr[1] = 0;   pg[1] = 0;   pb[1] = 0;
    pr[2] = 255; pg[2] = 255; pb[2] = 255;
    pr[3] = 128; pg[3] = 128; pb[3] = 128;
    src[0] = 1; src[1] = 2; src[2] = 1;
    src[3] = 1; src[4] = 2; src[5] = 1;
    img.width = 3;
    img.height = 2;
    img.pot_w = 4;
    img.pot_h = 4;
    img.span_w = 4;
    img.span_h = 2;
    img.xscale = 0;
    img.yscale = 0;
    img.index = src;
    img.pot = pot;
    for (i = 0; i < 16; i++) {
        pot[i] = 0;
    }
    texture_smooth(&img, pr, pg, pb);
    ok = 1;
    for (i = 0; i < 16; i++) {
        static const df_u8 want[4] = { 1, 3, 3, 1 };

        ok = ok && pot[i] == want[i & 3];
    }
    check_expect(ok && img.span_w == 4 && img.span_h == 2,
                 "texture_smooth did not blend a stretched wall");
}

/*
 * Model 457 has door-jamb faces 2 units wide. The old loader dropped a
 * real corner from three of them, by angle, and lost 12 square units
 * of each: 3281346 in all. Every corner kept, the faces' triangles add
 * up to the file's polygons, 3281382.
 */
static void check_jambs(void)
{
    Bsa *bsa;
    const BsaRec *rec;
    Mesh *mesh;
    double area;
    int t;

    bsa = bsa_open("ARCH3D.BSA");
    rec = bsa ? bsa_find_id(bsa, 457) : 0;
    mesh = rec ? arch3d_load(bsa_read(bsa, rec), rec->size, 457) : 0;
    check_expect(mesh != 0, "model 457 did not load");
    if (!mesh) {
        return;
    }
    area = 0.0;
    for (t = 0; t < mesh->ntris; t++) {
        const Tri *q;
        double ax, ay, az, bx, by, bz, cx, cy, cz;

        q = &mesh->tris[t];
        ax = q->x[1] - q->x[0];
        ay = q->y[1] - q->y[0];
        az = q->z[1] - q->z[0];
        bx = q->x[2] - q->x[0];
        by = q->y[2] - q->y[0];
        bz = q->z[2] - q->z[0];
        cx = ay * bz - az * by;
        cy = az * bx - ax * bz;
        cz = ax * by - ay * bx;
        cx = cx * cx + cy * cy + cz * cz;
        /* x87, as arch3d does: hostcheck links no libm. */
        __asm__ __volatile__("fsqrt" : "=t"(cx) : "0"(cx));
        area += cx * 0.5;
    }
    printf("model 457 faces %d triangles %d area %.0f\n", mesh->nfaces, mesh->ntris, area);
    check_expect(mesh->nfaces == 498 && area > 3281380.0 && area < 3281384.0,
                 "model 457's door jambs are not whole");
}

/*
 * ARCH3D v2.5 counts a point reference in 4-byte steps. Read as an
 * index, model 41500's references ran past its 11 points into the rest
 * of the record. Its farthest point has to be its header radius, 246.
 */
static void check_v25(void)
{
    Bsa *bsa;
    const BsaRec *rec;
    const df_u8 *bytes;
    Mesh *mesh;
    double far2;
    double radius;
    int t;
    int k;

    bsa = bsa_open("ARCH3D.BSA");
    rec = bsa ? bsa_find_id(bsa, 41500) : 0;
    bytes = rec ? bsa_read(bsa, rec) : 0;
    mesh = bytes ? arch3d_load(bytes, rec->size, 41500) : 0;
    check_expect(mesh != 0, "model 41500 did not load");
    if (!mesh) {
        return;
    }
    far2 = 0.0;
    for (t = 0; t < mesh->ntris; t++) {
        for (k = 0; k < 3; k++) {
            double d;

            d = (double)mesh->tris[t].x[k] * mesh->tris[t].x[k]
                + (double)mesh->tris[t].y[k] * mesh->tris[t].y[k]
                + (double)mesh->tris[t].z[k] * mesh->tris[t].z[k];
            if (d > far2) {
                far2 = d;
            }
        }
    }
    radius = (double)df_ri32(bytes + 12) / 256.0;
    printf("model 41500 (v2.5) faces %d farthest^2 %.0f radius^2 %.0f\n",
           mesh->nfaces, far2, radius * radius);
    check_expect(mesh->nfaces == 8 && far2 <= radius * radius * 1.01 && far2 >= radius * radius * 0.98,
                 "v2.5 model 41500 does not reach its header radius");
    check_expect(arch3d_bad_refs() == 0, "an ARCH3D face referred past its point table");
}

/*
 * Flats that stand on the floor under them: their bottom (centre less
 * half the height) within a few units above it. Dungeon flats are placed
 * by their centre and town flats by their base; lifting a dungeon flat
 * as if by its base floated every one half its height up.
 */
static int flats_on_floor(const World *world, int *over_floor)
{
    Level lv;
    int i;
    int on;

    world_as_level(world, &lv);
    on = 0;
    *over_floor = 0;
    if (!collide_use(&lv.scene)) {
        return 0;
    }
    collide_set_shape(1.0f, 1.0f);
    for (i = 0; i < lv.scene.nsprites; i++) {
        const Flat *f;
        float bottom;
        float fy;

        f = &lv.scene.sprites[i];
        bottom = f->y - f->h * 0.5f;
        collide_gather(f->x - 1.0f, bottom - 400.0f, f->z - 1.0f, f->x + 1.0f, bottom + 2.0f, f->z + 1.0f);
        if (!collide_floor(f->x, f->z, bottom + 1.0f, bottom - 400.0f, &fy)) {
            continue;
        }
        (*over_floor)++;
        if (bottom - fy > -1.0f && bottom - fy < 8.0f) {
            on++;
        }
    }
    return on;
}

/*
 * SKY##.DAT and NITE: which file a climate draws, the frame by the
 * hour, and how the halves land on the circle (docs/daylight.md).
 */
static void put_u32(FILE *f, df_u32 v)
{
    fputc((int)(v & 255u), f);
    fputc((int)((v >> 8) & 255u), f);
    fputc((int)((v >> 16) & 255u), f);
    fputc((int)((v >> 24) & 255u), f);
}

/* 16-bit stereo at 11025 Hz, to listen to what a test mixed. */
static void write_wav(const char *path, const df_i16 *buf, int frames)
{
    FILE *f;
    int i;

    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fwrite("RIFF", 1, 4, f);
    put_u32(f, 36u + (df_u32)frames * 4u);
    fwrite("WAVEfmt ", 1, 8, f);
    put_u32(f, 16);
    put_u32(f, 1u | (2u << 16));
    put_u32(f, 11025);
    put_u32(f, 11025u * 4u);
    put_u32(f, 4u | (16u << 16));
    fwrite("data", 1, 4, f);
    put_u32(f, (df_u32)frames * 4u);
    for (i = 0; i < frames * 2; i++) {
        fputc(buf[i] & 255, f);
        fputc((buf[i] >> 8) & 255, f);
    }
    fclose(f);
}

/*
 * Run a place's sound for secs at 20 frames a second from cam, mixing
 * into out (frames of 11025 Hz), as draw_frame would. strokes: a
 * lightning stroke every this many frames, 0 for none.
 */
static int run_sound(const Level *level, const Camera *cam, float night, int strokes,
                     float secs, df_i16 *out, int cap)
{
    Weather w;
    int frames;
    int k;
    int n;

    frames = 0;
    n = df_iround(secs * 20.0f);
    for (k = 0; k < n; k++) {
        int per;

        weather_state(&w);
        ambience_frame(level, cam, &w, night, strokes > 0 && k % strokes == 1, 0.05f);
        per = 551 + (k % 20 == 0 ? 5 : 0);
        if (frames + per > cap) {
            break;
        }
        mix_clear(out + frames * 2, per);
        sound_mix(out + frames * 2, per);
        frames += per;
    }
    return frames;
}

static double rms_of(const df_i16 *buf, int frames)
{
    double e;
    int i;

    e = 0.0;
    for (i = 0; i < frames * 2; i++) {
        e += (double)buf[i] * buf[i];
    }
    if (frames <= 0) {
        return 0.0;
    }
    e /= frames * 2.0;
    {
        /* Newton's square root: the host build links no libm. */
        double r;
        int k;

        r = e > 1.0 ? e : 1.0;
        for (k = 0; k < 60; k++) {
            r = 0.5 * (r + e / r);
        }
        return r;
    }
}

/* DAGGER.SND and MIDI.BSA as the game reads them (docs/sound.md). */
static void check_sound_files(void)
{
    const SoundClip *rain;
    int i;
    int ok;
    int most_events;
    int most_notes;
    int stolen_any;
    static df_u32 words[(MUSIC_EVENTS + MUSIC_EVENTS / 8 + 64) * 3];

    printf("sound clips %d\n", sndfile_count());
    check_expect(sndfile_count() == 459, "DAGGER.SND does not have 459 records");
    rain = sndfile_clip(SND_RAIN);
    check_expect(rain && rain->frames == 31346 && rain->rate == 11025 && rain->data[0] == -3,
                 "the rain clip (389) is not 31346 signed samples at 11025 Hz");
    check_expect(sndfile_clip(SND_RAIN) == rain, "a clip loaded twice");
    check_expect(sndfile_by_id(385) == SND_RAIN && sndfile_clip(5) == 0,
                 "record id 385 is not position 389, or the empty record at 5 gave a clip");
    printf("songs %d\n", hmi_count());
    check_expect(hmi_count() == 131, "MIDI.BSA does not have 131 songs");
    ok = 1;
    most_events = 0;
    most_notes = 0;
    stolen_any = 0;
    for (i = 0; i < hmi_count(); i++) {
        Song song;
        int n;
        int stolen;
        int peak;

        mem_zero(&song, sizeof(song));
        if (!hmi_load(hmi_name(i), 0, &song) || song.division != 480 || song.length == 0) {
            printf("song %s did not read\n", hmi_name(i));
            ok = 0;
            continue;
        }
        n = music_render(&song, words, (int)(sizeof(words) / 4), MUSIC_VOICES_MAX, 100, &stolen, &peak);
        if (n == 0) {
            ok = 0;
        }
        /* No song waits long after its music before it loops (SUNNYDAY's stray tail). */
        {
            df_u32 note;
            int k;

            note = 0;
            for (k = 0; k < song.count; k++) {
                if ((song.events[k].msg & 0xf0u) == 0x80u && song.events[k].tick > note) {
                    note = song.events[k].tick;
                }
            }
            if (song.length > note + 960u) {
                printf("song %s ends %u ticks after its last note\n", hmi_name(i),
                       (unsigned int)(song.length - note));
                ok = 0;
            }
            if (df_eq(hmi_name(i), "SUNNYDAY.HMI")) {
                printf("song SUNNYDAY.HMI %.1f s\n", (double)song.length / 120.0);
                check_expect(song.length / 120u >= 55u && song.length / 120u <= 58u,
                             "SUNNYDAY.HMI is not about 57 seconds");
            }
        }
        most_events = song.count > most_events ? song.count : most_events;
        most_notes = peak > most_notes ? peak : most_notes;
        stolen_any += stolen;
        if (df_eq(hmi_name(i), "D1.HMI")) {
            printf("song D1.HMI %d events, %.1f s, %d notes at most\n", song.count,
                   (double)song.length / 120.0, peak);
            check_expect(song.length / 120u >= 222u && song.length / 120u <= 226u,
                         "D1.HMI is not about 224 seconds");
        }
    }
    printf("songs: most events %d, most notes at once %d, cut by 32 voices %d\n",
           most_events, most_notes, stolen_any);
    check_expect(ok, "a song of MIDI.BSA did not read or render");
    check_expect(most_notes <= 32 && stolen_any == 0 && most_events < MUSIC_EVENTS,
                 "a Daggerfall song needs more than 32 voices or MUSIC_EVENTS");
    {
        Song fm;

        mem_zero(&fm, sizeof(fm));
        check_expect(hmi_load("D1FM.HMI", 1, &fm) && fm.count > 0, "the FM tracks of D1FM.HMI did not read");
    }
}

/* The hold: torches loop their fire, the dungeon's noises come now and then. */
static void check_sound_hold(const World *world)
{
    static df_i16 buf[11025 * 6 * 2];
    const Soundscape *sc;
    const Emitter *torch;
    Level hl;
    Camera cam;
    int heard;
    int loops;
    int frames;
    int k;
    const char *name;

    world_as_level(world, &hl);
    sc = world_soundscape(world);
    printf("hold emitters %d\n", world_emitter_count(world));
    check_expect(world_emitter_count(world) > 10 && hl.scene.nemitters == world_emitter_count(world)
                 && hl.sound == sc, "the hold's torches make no sound");
    check_expect(sc && sc->kind == SCAPE_DUNGEON && sc->naround == 14 && !sc->rain,
                 "the hold's soundscape is not the dungeon's fourteen noises");
    name = sc ? sc->song(sc, -1, 0) : 0;
    {
        Song song;

        mem_zero(&song, sizeof(song));
        check_expect(name && loader_call("song", name, 0, 0, &song), "the hold's song does not load");
    }
    torch = world_emitter_at(world, 0);
    for (k = 0; k < world_emitter_count(world); k++) {
        if (world_emitter_at(world, k)->every == 0.0f) {
            torch = world_emitter_at(world, k);
            break;
        }
    }
    camera_set(&cam, torch->x + 60.0f, torch->y, torch->z, 0.0f, 0.0f);
    sound_set_mix_rate(11025);
    sound_set_on(1);
    music_stop();
    frames = run_sound(&hl, &cam, 0.0f, 0, 5.0f, buf, 11025 * 6);
    ambience_stats(&heard, &loops);
    printf("hold sound: song %s, %d heard, %d loops, %d voices, rms %.0f\n",
           music_name(), heard, loops, sound_busy(), rms_of(buf, frames));
    check_expect(loops >= 1 && rms_of(buf, frames) > 50.0, "beside a torch the fire is silent");
    check_expect(df_eq(music_name(), name), "the hold's song did not start");
    {
        const Emitter *list;
        int playing;

        playing = 0;
        for (k = 0; k < ambience_emitters(&list); k++) {
            playing += ambience_emitter_playing(k);
        }
        check_expect(ambience_emitters(&list) == world_emitter_count(world) && list == hl.scene.emitters
                     && playing == loops && torch->label && df_eq(torch->label, "fire"),
                     "sndlist and sndview do not see the hold's emitters, or the playing torch");
    }
    write_wav("build/snd0.wav", buf, frames);
    camera_set(&cam, torch->x + 5000.0f, torch->y, torch->z, 0.0f, 0.0f);
    run_sound(&hl, &cam, 0.0f, 0, 0.5f, buf, 11025 * 6);
    ambience_stats(&heard, &loops);
    check_expect(loops == 0, "a torch out of reach still holds a voice");
    sound_stop_all();
    music_stop();
}

/* A farm: songs for every weather, the rain and thunder of a storm. */
/* How many of world's usables are of kind and, when what >= 0, lead there. */
static int count_use(const World *world, int kind, int what)
{
    int n;
    int i;

    n = 0;
    for (i = 0; i < world_usable_count(world); i++) {
        const Usable *u;

        u = world_usable(world, i);
        if (u->kind == kind && (what < 0 || u->what == what)) {
            n++;
        }
    }
    return n;
}

/*
 * Stand d units in front of usable i, facing it, and use what the eye
 * meets. The index used, or -1.
 */
static int use_from_front(const Level *level, const World *world, int i, float d)
{
    const Usable *u;
    Camera cam;
    UseHit hit;
    float x, y, z;
    float nx, nz;

    u = world_usable(world, i);
    if (u->model >= 0) {
        Bound b;
        const Instance *in;

        in = world_instance(world, u->model);
        if (!instance_world_box(in, &b)) {
            return -1;
        }
        x = b.cx;
        y = b.cy;
        z = b.cz;
        /* A door model is thin along its own Z or X: face it across the thin side. */
        if (b.ex < b.ez) {
            nx = 1.0f;
            nz = 0.0f;
        } else {
            nx = 0.0f;
            nz = 1.0f;
        }
    } else {
        x = u->cx;
        y = u->cy;
        z = u->cz;
        nx = u->nx;
        nz = u->nz;
    }
    camera_set(&cam, x + nx * d, y, z + nz * d, df_atan2(-nx, -nz), 0.0f);
    if (!interact_aim(&level->scene, &cam, &hit)) {
        camera_set(&cam, x - nx * d, y, z - nz * d, df_atan2(nx, nz), 0.0f);
        if (!interact_aim(&level->scene, &cam, &hit)) {
            return -1;
        }
    }
    interact_use(&level->scene, hit.usable);
    return hit.usable;
}

/*
 * The hold's doors and its lever: what moves, what is locked, the way
 * out, and a door that swings a quarter turn when used.
 */
static void check_doors_hold(const World *world)
{
    Level level;
    int doors, actions, locked, exits;
    int door;
    int lever;
    int i;

    world_as_level(world, &level);
    doors = count_use(world, USE_MOVER, -1);
    actions = 0;
    locked = 0;
    for (i = 0; i < world_mover_count(world); i++) {
        const Mover *m;

        m = world_mover(world, i);
        actions += !m->door;
        locked += m->lock > 0;
    }
    exits = count_use(world, USE_PASSAGE, DOOR_EXIT);
    printf("hold doors: %d to use, %d movers (%d not doors, %d locked), %d ways out\n",
           doors, world_mover_count(world), actions, locked, exits);
    check_expect(world_place(world) == PLACE_DUNGEON, "the hold is not a dungeon");
    check_expect(exits >= 1, "the hold has no way out");
    /* A door that is not locked, used from in front: it swings a quarter turn. */
    door = -1;
    lever = -1;
    for (i = 0; i < world_usable_count(world); i++) {
        const Usable *u;
        const Mover *m;

        u = world_usable(world, i);
        if (u->kind != USE_MOVER) {
            continue;
        }
        m = world_mover(world, u->mover);
        if (m->door && !m->lock && door < 0) {
            door = i;
        }
        if (!m->door && m->next >= 0 && lever < 0) {
            lever = i;
        }
    }
    check_expect(door >= 0, "the hold has no unlocked door to use");
    if (door >= 0) {
        const Usable *u;
        const Mover *m;
        float m0[9];
        int used;
        int k;

        u = world_usable(world, door);
        m = world_mover(world, u->mover);
        for (k = 0; k < 9; k++) {
            m0[k] = m->inst->m[k];
        }
        used = use_from_front(&level, world, door, 64.0f);
        check_expect(used == door, "a door in front of the eye was not the one used");
        mover_step(&level.scene, 0.75f);
        check_expect(m->state == MOVER_FORWARD && m->t > 0.4f && m->t < 0.6f,
                     "a used door is not half open after 0.75 s");
        mover_step(&level.scene, 1.0f);
        /* A quarter turn back about its up: its local X now points where its local Z did. */
        printf("hold door %d: state %d, x axis %.2f %.2f %.2f was %.2f %.2f %.2f\n", door,
               m->state, m->inst->m[0], m->inst->m[3], m->inst->m[6], m0[0], m0[3], m0[6]);
        check_expect(m->state == MOVER_END
                     && check_near(m->inst->m[0], m0[2], 0.01f)
                     && check_near(m->inst->m[6], m0[8], 0.01f),
                     "a used door did not swing a quarter turn");
        interact_use(&level.scene, door);
        mover_step(&level.scene, 2.0f);
        check_expect(m->state == MOVER_START && check_near(m->inst->m[0], m0[0], 0.001f),
                     "a door used again did not shut");
    }
    /* The lever: it starts the chain after it, which moves together. */
    check_expect(lever >= 0, "the hold has no chained action");
    if (lever >= 0) {
        const Usable *u;
        const Mover *m;
        const Mover *n;

        u = world_usable(world, lever);
        m = world_mover(world, u->mover);
        n = world_mover(world, m->next);
        interact_use(&level.scene, lever);
        printf("hold action %d: next %d moving %d, %.2f s, turn %.2f %.2f %.2f\n", lever,
               m->next, n->state == MOVER_FORWARD, n->secs, n->rx, n->ry, n->rz);
        check_expect(n->state == MOVER_FORWARD, "a used action did not start the next in its chain");
        check_expect(!interact_use(&level.scene, lever), "a chain started again while it moved");
        mover_step(&level.scene, 10.0f);
        check_expect(n->state == MOVER_END, "a chained action did not finish");
        /* And back, so the tests after see the hold as it was built. */
        interact_use(&level.scene, lever);
        mover_step(&level.scene, 10.0f);
        check_expect(n->state == MOVER_START, "a chained action used again did not go back");
    }
    /* The way out: a face of the exit model, a door's size, upright, and used from in front. */
    i = world_passage(world, DOOR_EXIT, 0.0f, 0.0f, 0.0f);
    if (i >= 0) {
        const Usable *u;

        u = world_usable(world, i);
        printf("hold exit: at %.0f %.0f %.0f facing %.2f %.2f, %.0f by %.0f\n",
               u->cx, u->cy, u->cz, u->nx, u->nz, u->hw * 2.0f, u->hh * 2.0f);
        check_expect(u->hw * 2.0f > 40.0f && u->hw * 2.0f < 200.0f
                     && u->hh * 2.0f > 60.0f && u->hh * 2.0f < 200.0f,
                     "the hold's way out is not a door's size");
        check_expect(use_from_front(&level, world, i, 48.0f) == i,
                     "the hold's way out was not used from in front of it");
    }
}

/*
 * Stand where a passage lets one out (doors_landing, d out, at eye
 * height), turn round and use what the eye meets. Its index, or -1.
 */
static int turn_and_use(const World *world, const Usable *u, float d)
{
    Level level;
    Camera cam;
    UseHit hit;
    float x, y, z, yaw;

    world_as_level(world, &level);
    doors_landing(u, d, 68.4f, &x, &y, &z, &yaw);
    camera_set(&cam, x, y, z, yaw + DF_TAU * 0.5f, 0.0f);
    if (!interact_aim(&level.scene, &cam, &hit)) {
        return -1;
    }
    return hit.usable;
}

/*
 * A town's doors: its buildings' doors lead in, and the inside of the
 * first, loaded where the building stands, has a way out by that door.
 */
static void check_doors_town(const World *town, const Dungeon *ext)
{
    const Usable *in_door;
    World *inside;
    int enter;
    int people;
    int leave;
    int k;
    float ix, iy, iz;

    enter = count_use(town, USE_PASSAGE, DOOR_ENTER);
    people = count_use(town, USE_TALK, -1);
    printf("farm doors: %d into buildings, %d people, %d ways down\n",
           enter, people, count_use(town, USE_PASSAGE, DOOR_DUNGEON));
    check_expect(world_place(town) == PLACE_TOWN && enter >= 1, "a farm has no door into a building");
    k = world_passage(town, DOOR_ENTER, 0.0f, 0.0f, 0.0f);
    if (k < 0) {
        return;
    }
    in_door = world_usable(town, k);
    ix = in_door->cx;
    iy = in_door->cy;
    iz = in_door->cz;
    printf("farm door %d: building %d of block %d, at %.0f %.0f %.0f facing %.2f %.2f, %.0f by %.0f\n",
           k, in_door->tag & 0xff, in_door->tag >> 8, ix, iy, iz, in_door->nx, in_door->nz,
           in_door->hw * 2.0f, in_door->hh * 2.0f);
    check_expect(in_door->hw * 2.0f > 20.0f && in_door->hw * 2.0f < 200.0f
                 && in_door->hh * 2.0f > 40.0f && in_door->hh * 2.0f < 200.0f,
                 "a farm's door is not a door's size");
    inside = world_load_inside(ext, in_door->tag >> 8, in_door->tag & 0xff);
    if (!inside) {
        fprintf(stderr, "fail: %s\n", world_error());
    }
    check_expect(inside != 0, "a farm building's inside did not load");
    if (!inside) {
        return;
    }
    leave = world_passage(inside, DOOR_LEAVE, ix, iy, iz);
    {
        int fac;
        int bld;
        float sx, sy, sz, syaw, spitch;

        bld = world_building(inside, &fac);
        world_start(inside, &sx, &sy, &sz, &syaw, &spitch);
        printf("farm inside: building type %d faction %d, %d models, %d to use (%d doors, %d out), "
               "start %.0f %.0f %.0f, song %s\n",
               bld, fac, world_model_count(inside), world_usable_count(inside),
               count_use(inside, USE_MOVER, -1), count_use(inside, USE_PASSAGE, DOOR_LEAVE),
               sx, sy, sz, world_soundscape(inside)->song(world_soundscape(inside), 0, 0));
    }
    check_expect(world_place(inside) == PLACE_INSIDE && !world_outdoor(inside) && !world_sky(inside),
                 "a building's inside is under the sky");
    {
        Level il;
        int lit;
        int through;

        /* A house's lamps light their own storey, not the ones above and below through the floors. */
        world_as_level(inside, &il);
        light_set_shadow(0);
        through = light_build(&il.scene);
        light_set_shadow(1);
        lit = light_build(&il.scene);
        printf("farm inside: %d lights, %d decals, %d through floors and walls\n",
               il.scene.nlights, lit, through);
        check_expect(lit < through, "a house's lamps still light through its floors");
    }
    check_expect(leave >= 0, "a building's inside has no way out");
    if (leave >= 0) {
        const Usable *out;
        float dx, dz;

        out = world_usable(inside, leave);
        dx = out->cx - ix;
        dz = out->cz - iz;
        printf("farm way out: at %.0f %.0f %.0f facing %.2f %.2f, %.0f from the street's door\n",
               out->cx, out->cy, out->cz, out->nx, out->nz, df_sqrt(dx * dx + dz * dz));
        /* The same door seen from the other side: close by, facing the other way. */
        check_expect(dx * dx + dz * dz < 64.0f * 64.0f, "a building's way out is not by its door");
        check_expect(out->nx * in_door->nx + out->nz * in_door->nz < -0.7f,
                     "a building's way out does not face in");
        /* Landed in by it as LEVELFALL lands, turned round, the eye meets it. */
        check_expect(turn_and_use(inside, out, 32.0f) == leave,
                     "turned round on coming in, the way out is not what is used");
    }
    check_expect(turn_and_use(town, in_door, 48.0f) == k,
                 "turned round on coming out, the street's door is not what is used");
}

/* 1 when world loaded pictures of TEXTURE.file. */
static int uses_file(const World *world, int file)
{
    int k;

    for (k = 0; k < world_texture_file_count(world); k++) {
        if (world_texture_file(world, k) == file) {
            return 1;
        }
    }
    return 0;
}

/*
 * The seasons: the farm (woodland family) in summer is drawn on ground
 * 302 and the manor set 338; in winter on 303 and 339, their snowy sets,
 * and its window still marked. Back to summer after.
 */
static void check_seasons(const Dungeon *ext)
{
    World *w;
    int s;

    for (s = 0; s < 2; s++) {
        clock_set_season(s ? CLOCK_WINTER : CLOCK_SUMMER);
        w = world_load_dungeon(ext);
        check_expect(w != 0, "the farm did not load in a season");
        if (!w) {
            continue;
        }
        printf("farm in %s: ground %d, manor set %d, %d windows\n", clock_season_name(world_season(w)),
               uses_file(w, 303) ? 303 : (uses_file(w, 302) ? 302 : -1),
               uses_file(w, 339) ? 339 : (uses_file(w, 338) ? 338 : -1), world_glass_count(w));
        check_expect(world_season(w) == (s ? CLOCK_WINTER : CLOCK_SUMMER), "the farm was not built in the clock's season");
        check_expect(s ? (uses_file(w, 303) && uses_file(w, 339) && !uses_file(w, 302) && !uses_file(w, 338))
                       : (uses_file(w, 302) && uses_file(w, 338) && !uses_file(w, 303) && !uses_file(w, 339)),
                     "a farm's ground and walls are not its season's");
        check_expect(world_glass_count(w) >= 1, "a farm's window is not marked to light at night");
    }
    clock_set_season(CLOCK_SUMMER);
}

/* 1 when a model of the scene stands under (x, z): its box holds the point across the ground. */
static int ground_under(const Scene *sc, float x, float z)
{
    int i;

    for (i = 0; i < sc->nmodels; i++) {
        const Bound *b;

        if (!sc->models[i].mesh) {
            continue;
        }
        b = &sc->bounds[i];
        if (b->ex >= 0.0f && x >= b->cx - b->ex && x <= b->cx + b->ex
            && z >= b->cz - b->ez && z <= b->cz + b->ez) {
            return 1;
        }
    }
    return 0;
}

/*
 * Every lit light's decals lie within its reach of it: so a rebase moved
 * them with the lights. Returns how many lights have decals; *far is set
 * when one does not.
 */
static int stream_pools(int *far)
{
    int lit;
    int i;

    lit = 0;
    *far = 0;
    for (i = 0; i < light_count(); i++) {
        const Decal *d;
        const Light *li;
        float reach;
        int n;
        int k;

        n = light_decals(i, &d);
        if (n < 1) {
            continue;
        }
        lit++;
        li = light_at(i);
        reach = light_reach(i) * 1.42f + 2.0f;
        for (k = 0; k < n; k++) {
            const float *c;

            c = &light_corners_of(i)[d[k].first * LIGHT_CORNER];
            if (df_absf(c[0] - li->x) > reach || df_absf(c[2] - li->z) > reach
                || df_absf(d[k].cx - li->x) > reach || df_absf(d[k].cz - li->z) > reach
                || df_absf(d[k].nx * c[0] + d[k].ny * c[1] + d[k].nz * c[2] + d[k].d) > 1.0f) {
                *far = 1;
            }
        }
    }
    return lit;
}

/* Run stream frames until nothing within the radius is waiting, nor a light to cut (at most a few hundred). */
static void settle(Camera *cam)
{
    int waiting;
    int filling;
    int k;

    for (k = 0; k < 400; k++) {
        wild_update(cam, 50.0f);
        /* As LEVELFALL's frame: the people come and go, and let their clothes go out of town. */
        people_update(cam, 0.05f);
        stream_stats(0, &waiting, &filling, 0);
        if (!waiting && !filling && !light_stream_pending()) {
            return;
        }
    }
}

/*
 * The land (terrain.c): two chunks side by side agree on their shared
 * edge to the bit; a town's level is the land's at its edge and stays
 * so a blend away; the sea is water at y 0; the tiles are the archive's.
 */
static void check_land(void)
{
    static TerrainChunk a;
    static TerrainChunk b;
    TerrainFlat town;
    int seam;
    int j;
    int k;
    int water;
    int tiles_ok;
    float edge;
    float off;

    check_expect(worldmap_open(), "the world map did not read");
    /* West of Daggerfall, and the chunk east of it. */
    terrain_chunk(&a, 1640, 2280, 0, 0);
    terrain_chunk(&b, 1641, 2280, 0, 0);
    seam = 1;
    for (j = 0; j <= TERRAIN_CELLS; j++) {
        seam = seam && a.y[j][TERRAIN_CELLS] == b.y[j][0];
    }
    terrain_chunk(&b, 1640, 2281, 0, 0);
    for (j = 0; j <= TERRAIN_CELLS; j++) {
        seam = seam && a.y[TERRAIN_CELLS][j] == b.y[0][j];
    }
    check_expect(seam, "two chunks' land does not meet on their edge");
    /* A town of one block at the middle of the chunk area: flat on it, the land's a blend away. */
    town.x0 = 1640.0 * 4096 + 1024;
    town.z0 = 2280.0 * 4096 + 1024;
    town.x1 = town.x0 + 2048;
    town.z1 = town.z0 + 2048;
    town.s = terrain_level(town.x0, town.z0, town.x1, town.z1);
    edge = terrain_y(terrain_scaled(town.x0, town.z0 + 1000.0, &town, 1));
    off = terrain_y(terrain_scaled(town.x0 - TERRAIN_BLEND - 1.0, town.z0, &town, 1))
        - terrain_y(terrain_natural(town.x0 - TERRAIN_BLEND - 1.0, town.z0));
    check_expect(edge == terrain_y(town.s) && off == 0.0f,
                 "a town is not flat at its level, or its blend reaches past TERRAIN_BLEND");
    terrain_chunk(&a, 1640, 2280, &town, 1);
    check_expect(a.covered == (1u << 5 | 1u << 6 | 1u << 9 | 1u << 10)
                 && a.y[1][1] == terrain_y(town.s) && a.y[3][3] == terrain_y(town.s),
                 "a chunk under a town does not leave its cells to it, flat at its level");
    /* The sea at the map's corner: water at y 0, all of it. */
    terrain_chunk(&a, 2, 2, 0, 0);
    water = 1;
    for (k = 0; k < 256; k++) {
        water = water && a.tiles[k] == TERRAIN_WATER;
    }
    check_expect(water && a.y[2][2] == 0.0f, "the sea is not water at y 0");
    /* Every tile is a record the ground archives have (0..55), turned and flipped as RMB tiles are. */
    tiles_ok = 1;
    for (j = 0; j < 64; j++) {
        terrain_chunk(&a, 1600 + j * 3, 2250 + j, 0, 0);
        for (k = 0; k < 256; k++) {
            tiles_ok = tiles_ok && (a.tiles[k] & 0x3f) < 56;
        }
    }
    check_expect(tiles_ok, "a land tile is past the ground archive's 56 records");
}

/*
 * The stream's pictures (wres): held ones stay, released ones go the
 * least recently released first when trimmed, come back when held
 * again, and a game-made picture's slot is reused once forgotten.
 */
static void check_wres(const Palette *pal)
{
    Level wl;
    Image *made;
    int a;
    int b;
    int c;
    int d;
    int e;
    int in_ram;
    int held;
    df_u32 ram;
    df_u32 before;

    mem_zero(&wl, sizeof(wl));
    check_expect(pal && wres_begin(pal, &wl), "wres did not begin");
    a = wres_slot(302, 2, WRES_WALL);
    b = wres_slot(302, 3, WRES_WALL);
    c = wres_slot(210, 0, WRES_SPRITE);
    check_expect(a >= 0 && b >= 0 && c >= 0 && a != b && wres_slot(302, 2, WRES_WALL) == a
                 && wres_slot(302, 2, WRES_SPRITE) != a && wl.nimages >= 4,
                 "a picture's slot is not its archive, record and use");
    check_expect(!wres_image(a) && wres_acquire(a) && wres_acquire(b) && wres_acquire(c)
                 && wres_image(a) && wres_image(c) && wl.images[a] == wres_image(a)
                 && wres_image(c)->pot_w == wres_image(c)->pot_h,
                 "held pictures did not load");
    wres_stats(0, &in_ram, &ram, &held, 0, 0);
    check_expect(in_ram == 3 && held == 3 && ram > 0, "three held pictures are not counted in RAM");
    /* b released first, then a: a trim to nothing frees both, c stays held. */
    wres_release(b);
    wres_release(a);
    before = ram;
    wres_trim(ram - 1);
    wres_stats(0, &in_ram, &ram, &held, 0, 0);
    check_expect(!wres_image(b) && wres_image(a) && ram < before,
                 "the least recently released picture was not the one trimmed");
    wres_trim(0);
    wres_stats(0, &in_ram, &ram, &held, 0, 0);
    check_expect(!wres_image(a) && wres_image(c) && in_ram == 1 && held == 1,
                 "a trim freed a held picture, or kept a released one");
    check_expect(wres_acquire(a) && wres_image(a) && wres_image(a)->width > 0,
                 "a trimmed picture did not come back when held again");
    /* A game-made picture: a slot of its own, the same slot again once forgotten. */
    made = (Image *)mem_alloc(sizeof(Image));
    mem_zero(made, sizeof(Image));
    made->width = made->height = made->pot_w = made->pot_h = made->span_w = made->span_h = 8;
    made->pot = made->index = (df_u8 *)mem_alloc(64);
    d = wres_dynamic(made, 1.0f);
    check_expect(d >= 0 && wl.images[d] == made && d != a && d != b && d != c, "a made picture got no slot");
    wres_forget(d);
    e = wres_dynamic(made, 1.0f);
    check_expect(!wl.images[d] || e == d, "a forgotten slot was not given back");
    check_expect(e == d, "the next made picture did not reuse the forgotten slot");
    wres_forget(e);
    /*
     * In a stream (a cap on the files): a picture is read in parts, its
     * file a piece a call (an 80 KB TEXTURE.386: three), then its pixels,
     * then its chain; it is not drawn until the last.
     */
    texture_cache_clear();
    texture_cache_limit(4u * 1024u * 1024u);
    a = wres_slot(386, 1, WRES_SPRITE);
    wres_hold(a);
    for (d = 0; d < 32 && wres_load(a); d++) {
        if (d == 0) {
            check_expect(!wres_image(a) && !wl.images[a], "a picture is drawn before it is read whole");
        }
    }
    check_expect(d >= 5 && wres_image(a) && wl.images[a] == wres_image(a) && texture_cache_bytes() > 65536u,
                 "a picture was not read in parts: its file in pieces, its pixels, its chain");
    texture_cache_limit(0);
    texture_cache_clear();
    wres_end();
}

/*
 * The world map and the stream (docs/streaming.md): Daggerfall's pixel
 * holds Daggerfall; a stream opened there fills its ring, and a walk of
 * 40 chunks east keeps the ground under the camera, moves the origin
 * with it, and keeps the camera's world position exact.
 */
static void check_stream(void)
{
    Level level;
    Camera cam;
    float x, y, z;
    double wx0, wz0, wx, wz;
    int region, index, px, py;
    int loaded, waiting, filling, shifts;
    int step;
    int under;

    check_expect(worldmap_open(), "the world map did not read");
    check_expect(worldmap_locations() == 15251, "the world map does not have 15251 locations");
    /* Location types from MAPTABLE: Daggerfall a city; the hold's outside (109, 158) no settlement. */
    printf("worldmap: Daggerfall type %d, Privateer's Hold's outside type %d\n",
           worldmap_location_type(207, 213), worldmap_location_type(109, 158));
    check_expect(worldmap_location_type(207, 213) == WORLDMAP_TOWN_CITY
                 && worldmap_location_type(109, 158) > WORLDMAP_TOWN_VILLAGE
                 && worldmap_location_type(0, 0) == -1,
                 "Daggerfall is not a city, or the hold's outside is a settlement, or the sea has a location");
    check_expect(worldmap_location(207, 213, &region, &index) && region == 17
                 && worldmap_climate(207, 213) == CLIMATE_WOODLANDS && worldmap_height(207, 213) == 20,
                 "pixel 207 213 is not Daggerfall's, in the woodlands, at height 20");
    check_expect(worldmap_pixel_of(region, index, &px, &py) && px == 207 && py == 213,
                 "Daggerfall's location does not lead back to its pixel");
    check_expect(worldmap_climate(0, 0) == CLIMATE_OCEAN && worldmap_height(0, 0) <= 1,
                 "the map's corner is not sea");
    /* A new place, as LEVELFALL starts one: the files the places before read are let go. */
    texture_cache_clear();
    mem_temp_begin();
    check_expect(wild_open(207, 213, 2, &level, &x, &y, &z), "the wilderness stream did not open");
    camera_set(&cam, x, y, z, 0.0f, 0.0f);
    stream_world(cam.x, cam.z, &wx0, &wz0);
    check_expect(wx0 == 207.0 * 32768.0 + 16384.0 && wz0 == (499.0 - 213.0) * 32768.0 + 16384.0,
                 "the stream did not start in the middle of Daggerfall's pixel");
    /* Frames until the ring is full, as the view's loop would run them. */
    settle(&cam);
    stream_stats(&loaded, &waiting, &filling, &shifts);
    check_expect(loaded == 25 && waiting == 0 && ground_under(&level.scene, cam.x, cam.z),
                 "a radius 2 stream did not fill its 5 x 5 chunks, or there is no ground under the camera");
    /* Collision walks the slots' spans (no grid to build again as chunks come): the floor is found. */
    {
        double gx;
        double gz;
        float g;
        float fy;

        /* Daggerfall stands on its level, the land's mean over the town: the camera's eye over it. */
        stream_world(cam.x, cam.z, &gx, &gz);
        g = wild_ground(gx, gz);
        printf("stream: Daggerfall's level y %.0f, the camera at %.0f\n", g, cam.y);
        check_expect(g > 1000.0f && cam.y > g + 60.0f && cam.y < g + 80.0f,
                     "the stream did not start the camera at eye height over Daggerfall's level");
        fy = -1000.0f;
        check_expect(collide_use(&level.scene)
                     && collide_gather(cam.x - 32.0f, g - 200.0f, cam.z - 32.0f, cam.x + 32.0f, g + 200.0f,
                                       cam.z + 32.0f) > 0
                     && collide_floor(cam.x, cam.z, g + 200.0f, g - 200.0f, &fy)
                     && fy > g - 1.0f && fy < g + 1.0f,
                     "collision found no floor under the camera in the stream");
    }
    /*
     * Daggerfall's streets at noon (docs/entities.md): its people come
     * round the camera, on walking cells, at the town's level, in their
     * pictures, and walk.
     */
    {
        const Scene *sc;
        float before[PEOPLE_MAX][2];
        float was_minute;
        int on_cells;
        int moved;
        int drawn;
        int there;
        int k;

        sc = &level.scene;
        was_minute = clock_minute();
        clock_set_minute(12.0f * 60.0f);
        /*
         * Landed (wild_fill ends with it): the town's whole population out
         * at once, in clothes its block chunks already read, before any
         * frame; not one a tick, their pictures one a frame, after it shows.
         */
        people_fill(&cam);
        {
            int ahead;

            /* Out where the camera looks too, as if walking there all along: nothing was seen yet. */
            ahead = 0;
            for (k = 0; k < sc->nentities; k++) {
                const Entity *e;
                float dx;
                float dz;

                e = &sc->entities[k];
                dx = e->x - cam.x;
                dz = e->z - cam.z;
                ahead += e->live && dx * dx + dz * dz < PEOPLE_POP * PEOPLE_POP
                    && dx * df_sin(cam.yaw) + dz * df_cos(cam.yaw) > 0.0f;
            }
            printf("stream: landed at noon: %d people, %d of them ahead within sight\n", people_count(), ahead);
            check_expect(people_count() == PEOPLE_MAX && people_pictures() == PEOPLE_SLOTS && ahead > 10,
                         "landing at noon, Daggerfall's 96 people are not out, ahead too, before the first frame");
        }
        for (k = 0; k < 600; k++) {
            people_update(&cam, 0.05f);
        }
        on_cells = 0;
        drawn = 0;
        there = people_count();
        for (k = 0; k < sc->nentities; k++) {
            const Entity *e;
            double ex;
            double ez;
            Flat fl;

            e = &sc->entities[k];
            before[k][0] = e->x;
            before[k][1] = e->z;
            if (!e->live) {
                continue;
            }
            stream_world(e->x, e->z, &ex, &ez);
            on_cells += wild_nav(ex, ez) > 0 && e->y == wild_ground(ex, ez);
            drawn += entity_flat(e, cam.x + 300.0f, cam.z, &fl) && fl.w > 20.0f && fl.h > 60.0f;
        }
        for (k = 0; k < 100; k++) {
            people_update(&cam, 0.05f);
        }
        moved = 0;
        for (k = 0; k < sc->nentities; k++) {
            moved += sc->entities[k].live && (sc->entities[k].x != before[k][0] || sc->entities[k].z != before[k][1]);
        }
        printf("stream: Daggerfall at noon: %d people (%d pictures read), %d on walking cells at the town's level, "
               "%d drawn, %d walked in 5 s\n",
               there, people_pictures(), on_cells, drawn, moved);
        check_expect(sc->entities && there > 40 && on_cells == there && drawn == there && moved > there / 2,
                     "Daggerfall's streets have no people walking them at noon");
        /* The engine says no: they go (out of sight), and none come, at noon. */
        entity_set_population(0);
        for (k = 0; k < 4; k++) {
            Camera look;

            camera_set(&look, cam.x, cam.y, cam.z, (float)k * (DF_TAU / 4.0f), 0.0f);
            people_update(&look, 0.05f);
        }
        for (k = 0; k < 40; k++) {
            people_update(&cam, 0.05f);
        }
        check_expect(people_count() == 0, "with population off the people stayed, or came again");
        entity_set_population(1);
        for (k = 0; k < 400; k++) {
            people_update(&cam, 0.05f);
        }
        check_expect(people_count() > 40, "with population on again the people did not come back");
        /* At night they go (out of sight), and none come. */
        clock_set_minute(23.0f * 60.0f);
        for (k = 0; k < 4; k++) {
            Camera look;

            /* Turned round, so everyone is behind the camera at least once. */
            camera_set(&look, cam.x, cam.y, cam.z, (float)k * (DF_TAU / 4.0f), 0.0f);
            people_update(&look, 0.05f);
        }
        check_expect(people_count() == 0, "the people stayed in the streets at night");
        clock_set_minute(was_minute);
    }
    /* Beside one of the stream's sounds, it is heard, from its chunk's span among ten thousand slots. */
    {
        static df_i16 sbuf[11025 * 2 * 2];
        Camera ear;
        int heard;
        int loops;
        int frames;
        int k;

        /* A loop (a fire) if there is one: a cat may not mew in two seconds. */
        for (k = 0; k < level.scene.nemitters
                    && (!level.scene.emitters[k].clip || level.scene.emitters[k].every > 0.0f); k++) {
        }
        if (k == level.scene.nemitters) {
            for (k = 0; k < level.scene.nemitters && !level.scene.emitters[k].clip; k++) {
            }
        }
        check_expect(k < level.scene.nemitters && level.scene.nemitters > 2048,
                     "the stream has no sounds, or fewer slots than the ambience ever followed");
        if (k < level.scene.nemitters) {
            const Emitter *e;

            e = &level.scene.emitters[k];
            camera_set(&ear, e->x + 32.0f, e->y, e->z, 0.0f, 0.0f);
            frames = run_sound(&level, &ear, 0.0f, 0, 2.0f, sbuf, 11025 * 2);
            ambience_stats(&heard, &loops);
            printf("stream: beside a %s, %d heard, %d loops, rms %.0f\n", e->label ? e->label : "sound",
                   heard, loops, rms_of(sbuf, frames));
            check_expect(heard > 0 && (e->every > 0.0f || (loops > 0 && rms_of(sbuf, frames) > 50.0)),
                         "beside a sound of the stream nothing is heard");
        }
        sound_stop_all();
    }
    /*
     * The stream's doors are the town's: a door into a house, put in the
     * town's own frame by wild_location_at, is where the whole town loaded
     * on its own has the same door (LEVELFALL goes in by it, and comes back).
     */
    {
        Dungeon tdun;
        World *town;
        int tried;
        int matched;
        int k;

        tried = 0;
        matched = 0;
        town = 0;
        for (k = 0; k < level.scene.nusables && tried < 20; k++) {
            const Usable *u;
            double dwx;
            double dwz;
            double ox;
            double oz;
            int rg;
            int ix;
            int t;

            u = &level.scene.usables[k];
            if (u->reach <= 0.0f || u->kind != USE_PASSAGE || u->what != DOOR_ENTER) {
                continue;
            }
            stream_world(u->cx, u->cz, &dwx, &dwz);
            if (!wild_location_at(dwx, dwz, &rg, &ix, &ox, &oz)) {
                continue;
            }
            if (!town) {
                check_expect(exterior_load_at(rg, ix, &tdun), "Daggerfall's record did not read");
                town = world_load_dungeon(&tdun);
                check_expect(town != 0, "Daggerfall did not load whole");
                if (!town) {
                    break;
                }
            }
            tried++;
            for (t = 0; t < world_usable_count(town); t++) {
                const Usable *w;

                w = world_usable(town, t);
                if (w->what == DOOR_ENTER && w->tag == u->tag
                    && df_absf(w->cx - (float)(dwx - ox)) < 1.0f && df_absf(w->cz - (float)(dwz - oz)) < 1.0f
                    && df_absf(w->cy + wild_ground(dwx, dwz) - u->cy) < 1.0f) {
                    matched++;
                    break;
                }
            }
        }
        printf("stream: %d of %d doors into houses are the town's own\n", matched, tried);
        check_expect(tried == 20 && matched == tried,
                     "the stream's doors are not where the town loaded whole has them");
    }
    /* The middle of Daggerfall: its houses, its flats, its doors, its lamps, its sounds. */
    {
        const Scene *sc;
        int houses;
        int flats;
        int doors;
        int lamps;
        int sounds;
        int k;

        sc = &level.scene;
        houses = flats = doors = lamps = sounds = 0;
        for (k = 0; k < STREAM_SLOTS; k++) {
            int f;
            int n;
            int fa;
            int r;

            stream_slot_models(k, &f, &n, &fa, &r);
            houses += r;
        }
        for (k = 0; k < sc->nsprites; k++) {
            flats += sc->sprites[k].w > 0.0f;
        }
        for (k = 0; k < sc->nusables; k++) {
            doors += sc->usables[k].reach > 0.0f;
        }
        for (k = 0; k < sc->nlights; k++) {
            lamps += sc->lights[k].radius > 0.0f;
        }
        for (k = 0; k < sc->nemitters; k++) {
            sounds += sc->emitters[k].clip != 0;
        }
        {
            int far;
            int lit;

            lit = stream_pools(&far);
            printf("stream: Daggerfall's middle 5 x 5: %d models past the ground, %d flats, %d doors, %d lamps (%d with pools, %d decals), %d sounds\n",
                   houses, flats, doors, lamps, lit, light_decal_total(), sounds);
            check_expect(houses > 200 && flats > 100 && doors > 50 && lamps > 10,
                         "the stream's chunks in Daggerfall lack its houses, flats, doors or lamps");
            check_expect(lit > lamps / 2 && !far,
                         "the stream's lamps did not lay their pools, or laid them away from the lamp");
            /* Each chunk's far ground covers what its near ground does, in fewer quads. */
            {
                double near_area;
                double far_area;
                int near_n;
                int far_n;
                int towns;
                int sl;

                near_area = far_area = 0.0;
                near_n = far_n = towns = 0;
                for (sl = 0; sl < STREAM_SLOTS; sl++) {
                    int f;
                    int n;
                    int fa;
                    int r;
                    int m;

                    stream_slot_models(sl, &f, &n, &fa, &r);
                    for (m = f; m < f + n + fa; m++) {
                        const Bound *bd;
                        double a;

                        bd = &sc->bounds[m];
                        a = 4.0 * (double)bd->ex * (double)bd->ez;
                        if (m < f + n) {
                            near_area += a;
                            near_n++;
                        } else {
                            far_area += a;
                            far_n++;
                            if (sc->models[m].mesh->tris[0].slot < level.nimages
                                && level.images[sc->models[m].mesh->tris[0].slot]
                                && level.images[sc->models[m].mesh->tris[0].slot]->width == 128) {
                                if (!towns) {
                                    write_ppm(level.images[sc->models[m].mesh->tris[0].slot],
                                              level.palette, "build/far0.ppm");
                                }
                                towns++;
                            }
                        }
                    }
                }
                printf("stream: near ground %d quads %.0f units², far %d quads %.0f units², %d baked\n",
                       near_n, near_area, far_n, far_area, towns);
                check_expect(near_area > 0.0 && far_area == near_area && far_n * 8 < near_n && towns > 10,
                             "the far ground does not cover the near ground's square, or is not fewer quads");
            }
            /* Cut chunk by chunk as they came, the pools are what cutting them all at once gives. */
            {
                int before;

                before = light_decal_total();
                light_build(&level.scene);
                settle(&cam);
                check_expect(light_decal_total() == before,
                             "the lamps cut as their chunks came differ from the lamps cut all at once");
            }
        }
    }
    /* East 40 chunks, out of Daggerfall and on, 512 units a frame. */
    under = 1;
    for (step = 0; step < 40 * 8; step++) {
        cam.x += 512.0f;
        settle(&cam);
        if (under && !ground_under(&level.scene, cam.x, cam.z)) {
            printf("stream: no ground at step %d, local %.0f %.0f\n", step, cam.x, cam.z);
            under = 0;
        }
    }
    {
        int far;

        stream_pools(&far);
        check_expect(!far, "a lamp's pool did not move with the origin");
    }
    stream_stats(&loaded, &waiting, &filling, &shifts);
    stream_world(cam.x, cam.z, &wx, &wz);
    wild_here(&cam, &px, &py);
    printf("stream: 40 chunks east, %d loaded, %d shifts, local x %.0f, world %.0f, pixel %d %d, ground under all the way %d\n",
           loaded, shifts, cam.x, wx, px, py, under);
    {
        int slots;
        int in_ram;
        int held;
        int meshes;
        df_u32 ram;
        df_u32 mesh_ram;

        wres_stats(&slots, &in_ram, &ram, &held, &meshes, &mesh_ram);
        printf("stream: %d picture slots, %d in RAM (%u KB, %d held), %d meshes (%u KB), files %u KB, decals %d\n",
               slots, in_ram, (unsigned int)(ram / 1024u), held, meshes, (unsigned int)(mesh_ram / 1024u),
               (unsigned int)(texture_cache_bytes() / 1024u), light_decal_total());
        check_expect(ram <= WRES_PICTURE_RAM + 2u * 1024u * 1024u && texture_cache_bytes() <= 5u * 1024u * 1024u,
                     "the stream's pictures or files passed their RAM caps");
    }
    check_expect(under, "a walk east lost the ground under the camera");
    /* The 5 x 5 round the camera, and the column behind it kept one ring longer. */
    check_expect(loaded == 30 && waiting == 0 && shifts >= 2
                 && cam.x < (float)(STREAM_CHUNK * (STREAM_REBASE + 1)),
                 "the stream did not keep its ring and the ring behind, or the origin did not follow the camera");
    check_expect(wx == wx0 + 40.0 * STREAM_CHUNK && wz == wz0 && px == 212 && py == 213,
                 "the camera's world position drifted as the origin moved");
    /* The land east of Daggerfall: hills, its tiles, its flora, few triangles. */
    {
        const Scene *sc;
        int near_tris;
        int far_tris;
        int near_max;
        int far_max;
        int chunks;
        int flora;
        int sl;
        float lo;
        float hi;

        sc = &level.scene;
        near_tris = far_tris = near_max = far_max = chunks = flora = 0;
        lo = 1.0e9f;
        hi = -1.0e9f;
        for (sl = 0; sl < STREAM_SLOTS; sl++) {
            int f;
            int n;
            int fa;
            int r;
            int m;
            int nt;
            int ft;

            stream_slot_models(sl, &f, &n, &fa, &r);
            if (n + fa + r < 1) {
                continue;
            }
            chunks++;
            nt = ft = 0;
            for (m = f; m < f + n; m++) {
                nt += sc->models[m].mesh ? sc->models[m].mesh->ntris : 0;
                if (sc->models[m].mesh) {
                    lo = sc->bounds[m].cy - sc->bounds[m].ey < lo ? sc->bounds[m].cy - sc->bounds[m].ey : lo;
                    hi = sc->bounds[m].cy + sc->bounds[m].ey > hi ? sc->bounds[m].cy + sc->bounds[m].ey : hi;
                }
            }
            for (m = f + n; m < f + n + fa; m++) {
                ft += sc->models[m].mesh ? sc->models[m].mesh->ntris : 0;
            }
            near_tris += nt;
            far_tris += ft;
            near_max = nt > near_max ? nt : near_max;
            far_max = ft > far_max ? ft : far_max;
        }
        for (sl = 0; sl < sc->nsprites; sl++) {
            flora += sc->sprites[sl].w > 0.0f && sc->sprites[sl].sight > 0;
        }
        printf("stream: the land east: %d chunks, near %d triangles (at most %d a chunk), far %d (at most %d), "
               "%d flora, ground y %.0f..%.0f\n",
               chunks, near_tris, near_max, far_tris, far_max, flora, lo, hi);
        /* The console's flora switches: off, a share by rank, a nearer sight; no reload. */
        {
            static int vis[4096];
            int counts[5];
            int q;

            for (q = 0; q < 5; q++) {
                int v;
                int k;

                scene_set_flora(q != 3);
                scene_set_flora_density(q == 0 ? SCENE_FLORA_DENSITY : (q == 1 ? 100 : (q == 2 ? 0 : 100)));
                scene_set_flora_sight(q == 4 ? 512 : 0);
                counts[q] = 0;
                /* Round the camera, to the far clip the stream was opened for; no cells from another test. */
                scene_set_limits(8192.0f, 0);
                scene_set_occlude(0);
                for (k = 0; k < 4; k++) {
                    Camera look;
                    int j;

                    camera_set(&look, cam.x, cam.y, cam.z, (float)k * (DF_TAU / 4.0f), -0.3f);
                    v = scene_visible_sprites(sc, &look, vis, 4096);
                    for (j = 0; j < v; j++) {
                        counts[q] += sc->sprites[vis[j]].sight > 0;
                    }
                }
                scene_set_limits(DF_FAR, 1);
                scene_set_occlude(1);
            }
            scene_set_flora(1);
            scene_set_flora_density(SCENE_FLORA_DENSITY);
            scene_set_flora_sight(0);
            printf("stream: flora in view: %d at the default, %d at 100%%, %d at 0%%, %d off, %d within 512\n",
                   counts[0], counts[1], counts[2], counts[3], counts[4]);
            check_expect(counts[1] > counts[0] && counts[0] > 0 && counts[2] == 0 && counts[3] == 0
                         && counts[4] < counts[1],
                         "the flora switches did not hide and show the flora");
        }
        check_expect(chunks == 30 && near_max > 0 && near_max <= 256 && far_max > 0 && far_max <= 24
                     && near_tris < chunks * 160 && flora > chunks * 4 && hi - lo > 100.0f,
                     "the land east of Daggerfall is not hills of few triangles with flora on them");
    }
    /* Into another climate, a jump east: its sky and weather odds come with the camera. */
    {
        int serial;
        int to;

        serial = level.sky_serial;
        for (to = 213; to < WORLDMAP_W && (worldmap_climate(to, 213) == CLIMATE_WOODLANDS
                                           || climate_family(worldmap_climate(to, 213)) < 0); to++) {
        }
        check_expect(to < WORLDMAP_W, "no other climate east of Daggerfall");
        cam.x += (float)((to - 212) * WORLDMAP_BLOCKS * STREAM_CHUNK);
        settle(&cam);
        wild_here(&cam, &px, &py);
        printf("stream: east to pixel %d %d, climate %d, sky %d, sky changes %d\n", px, py,
               worldmap_climate(px, py), level.sky ? level.sky->index : -1, level.sky_serial - serial);
        check_expect(px == to && level.sky_serial > serial && level.sky
                     && level.sky->index == skyfile_index(worldmap_climate(px, py), clock_season()),
                     "the sky did not follow the camera into another climate");
    }
    /* Past the meshes' cap, the stream starts again round the camera: every mesh read again. */
    {
        int meshes0;
        int meshes1;
        df_u32 ram0;
        df_u32 ram1;

        wres_stats(0, 0, 0, 0, &meshes0, &ram0);
        check_expect(!wild_crowded(), "a short walk passed the meshes' cap");
        wild_restart(&cam);
        wres_stats(0, 0, 0, 0, &meshes1, &ram1);
        printf("stream: restarted: meshes %d (%u KB) before, %d (%u KB) after\n",
               meshes0, (unsigned int)(ram0 / 1024u), meshes1, (unsigned int)(ram1 / 1024u));
        check_expect(stream_missing(STREAM_RADIUS_MAX) == 0 && ground_under(&level.scene, cam.x, cam.z)
                     && meshes1 <= meshes0 && ram1 <= ram0,
                     "a restart did not load the radius again with the meshes it needs");
    }
    /* The season turns: the chunks load again in its pictures, the camera's ring at once. */
    {
        int was;
        int winter_ground;
        int k;

        was = clock_season();
        clock_set_season(was == CLOCK_WINTER ? CLOCK_SUMMER : CLOCK_WINTER);
        check_expect(wild_season_turned(), "the stream did not see the season turn");
        wild_reseason(&cam);
        winter_ground = 0;
        for (k = 0; k < level.scene.nmodels; k++) {
            const Instance *in;

            in = &level.scene.models[k];
            if (in->mesh && in->mesh->ntris > 0) {
                int file;
                int record;

                /* The land's tiles are the ground archive's, in winter its snowy one (x03). */
                if (wres_picture((int)in->mesh->tris[0].slot, &file, &record) && file % 100 == 3) {
                    winter_ground++;
                }
            }
        }
        check_expect(!wild_season_turned() && stream_missing(STREAM_RADIUS_MAX) == 0
                     && ground_under(&level.scene, cam.x, cam.z)
                     && (clock_season() == CLOCK_WINTER) == (winter_ground > 0),
                     "the season's turn did not load the whole radius again in its pictures");
        clock_set_season(was);
    }
    /*
     * A run far past the budget (1000 units a frame, a step of filling a
     * frame): the chunk under the camera, and the next within 256 of an
     * edge, is loaded at once whenever the filling has not got there.
     */
    {
        double rx;
        double rz;
        int outran;

        /* From here, and back after: the tests after it start where the walk ended. */
        stream_world(cam.x, cam.z, &rx, &rz);
        outran = stream_outran();
        under = 1;
        for (step = 0; step < 60; step++) {
            cam.x += 1000.0f;
            wild_update(&cam, 0.0f);
            under = under && ground_under(&level.scene, cam.x, cam.z)
                && ground_under(&level.scene, cam.x + 255.0f, cam.z);
        }
        printf("stream: a run of 60000 units, the camera's chunk loaded at once %d times, ground under all the way %d\n",
               stream_outran() - outran, under);
        check_expect(under && stream_outran() > outran, "a run past the stream's filling fell off its edge");
        stream_local(rx, rz, &cam.x, &cam.z);
        settle(&cam);
    }
    wild_close();
    check_wres(level.palette);
    mem_temp_end();
}

static void check_sound_town(const World *town)
{
    static df_i16 buf[11025 * 8 * 2];
    const Soundscape *sc;
    Level tl;
    Camera cam;
    double quiet;
    double storm;
    int frames;
    int kind;
    int ok;

    world_as_level(town, &tl);
    sc = world_soundscape(town);
    {
        int animals;
        int k;

        animals = 0;
        for (k = 0; k < world_emitter_count(town); k++) {
            animals += world_emitter_at(town, k)->every > 0.0f;
        }
        printf("farm emitters %d, animals %d\n", world_emitter_count(town), animals);
    }
    check_expect(sc && sc->kind == SCAPE_TOWN && sc->rain && sc->night && sc->wind
                 && sc->nthunder == 3 && sc->nday == 2,
                 "the farm's soundscape lacks rain, crickets, wind, thunder or birds");
    ok = 1;
    for (kind = 0; sc && kind < WEATHER_KINDS; kind++) {
        int night;
        int index;

        for (night = 0; night < 2; night++) {
            for (index = 0; index < 40; index++) {
                Soundscape probe;
                Song song;
                const char *name;

                probe = *sc;
                probe.index = index;
                name = probe.song(&probe, kind, night);
                mem_zero(&song, sizeof(song));
                if (!name || !hmi_load(name, 0, &song)) {
                    printf("no song for weather %d night %d: %s\n", kind, night, name ? name : "-");
                    ok = 0;
                }
            }
        }
    }
    check_expect(ok, "a playlist names a song MIDI.BSA does not have");
    world_start(town, &cam.x, &cam.y, &cam.z, &cam.yaw, &cam.pitch);
    sound_stop_all();
    weather_set(WEATHER_SUNNY);
    frames = run_sound(&tl, &cam, 0.0f, 0, 1.0f, buf, 11025 * 8);
    quiet = rms_of(buf, frames);
    weather_set(WEATHER_THUNDER);
    frames = run_sound(&tl, &cam, 0.0f, 30, 7.0f, buf, 11025 * 8);
    storm = rms_of(buf, frames);
    printf("farm sound: sunny rms %.0f, storm rms %.0f, song %s\n", quiet, storm, music_name());
    check_expect(storm > quiet + 100.0, "a thunderstorm is not louder than a sunny day");
    write_wav("build/snd1.wav", buf, frames);
    weather_set(WEATHER_SUNNY);
    sound_stop_all();
    music_stop();
}

static void check_skies(void)
{
    static df_u8 morning[SKY_W * SKY_H];
    static df_u8 evening[SKY_W * SKY_H];
    static df_u8 half[512];
    df_u8 rgb[768];
    df_u8 rgb2[768];
    Sky sky;
    VfsFile *f;
    int same;
    int k;

    check_expect(skyfile_index(CLIMATE_WOODLANDS, SEASON_SUMMER) == 18
                 && skyfile_index(CLIMATE_DESERT, SEASON_FALL) == 8
                 && skyfile_index(CLIMATE_MOUNTAIN, SEASON_WINTER) == 3
                 && skyfile_index(CLIMATE_SWAMP, SEASON_SPRING) == 25,
                 "a climate draws the wrong sky file");
    {
        df_u8 odds[WEATHER_KINDS];
        int total;
        int k;

        /* The Chronicles' table: woodlands in winter snow, deserts in summer never rain. */
        forecast_odds(CLIMATE_WOODLANDS, SEASON_WINTER, odds);
        total = 0;
        for (k = 0; k < WEATHER_KINDS; k++) {
            total += odds[k];
        }
        check_expect(total == 100 && odds[WEATHER_SNOW] == 25, "woodland winter odds are not the table's");
        forecast_odds(CLIMATE_DESERT, SEASON_SUMMER, odds);
        check_expect(odds[WEATHER_SUNNY] == 85 && odds[WEATHER_RAIN] == 0 && odds[WEATHER_SNOW] == 0,
                     "the summer desert can rain");
    }
    check_expect(skyfile_frame(360.0f) == 0 && skyfile_frame(720.0f) == 32
                 && skyfile_frame(1079.0f) == 63 && skyfile_frame(480.0f) == 20,
                 "the sky frames do not follow the hour");
    skyfile_open(CLIMATE_WOODLANDS, SEASON_SUMMER, &sky);
    /* Keys are the file times 128 plus the frame; 64 is the night picture. */
    check_expect(sky.pick(&sky, 120.0f, WEATHER_SKY_CLEAR) == 18 * 128 + 64
                 && sky.pick(&sky, 1080.0f, WEATHER_SKY_CLEAR) == 18 * 128 + 64
                 && sky.pick(&sky, 720.0f, WEATHER_SKY_CLEAR) == 18 * 128 + 32 && sky.night == 2,
                 "night is not its own picture");
    check_expect(sky.pick(&sky, 720.0f, WEATHER_SKY_RAIN) == 20 * 128 + 32
                 && sky.pick(&sky, 720.0f, WEATHER_SKY_OVERCAST) == 21 * 128 + 32
                 && sky.pick(&sky, 720.0f, WEATHER_SKY_SNOW) == 22 * 128 + 32
                 && sky.pick(&sky, 120.0f, WEATHER_SKY_RAIN) == 20 * 128,
                 "bad weather does not draw the climate's rain or snow sky, frame 0 at night");
    check_expect(sky.read(&sky, 21 * 128 + 31, morning, rgb), "SKY21.DAT did not read");
    check_expect(sky.read(&sky, 18 * 128 + 20, morning, rgb) && sky.read(&sky, 18 * 128 + 43, evening, rgb2),
                 "SKY18.DAT frame 20 did not read");
    /* Record 1, frame 20, row 100: the eastern half, columns 0..511 in the morning. */
    f = vfs_open("SKY18.DAT");
    check_expect(f && vfs_read_at(f, 32u * 776u + 32u * 16384u + 112640u * 52u + 100u * 512u, half, 512)
                 && vfs_read_at(f, 20u * 776u + 8u, rgb2, 768),
                 "SKY18.DAT is not there");
    if (f) {
        vfs_close(f);
    }
    same = 1;
    for (k = 0; k < 768; k++) {
        same = same && rgb[k] == rgb2[k];
    }
    check_expect(same, "the sky palette is not frame 20's, after its 8-byte header");
    same = 1;
    for (k = 0; k < 512; k++) {
        same = same && morning[100 * SKY_W + k] == half[k]
            && evening[100 * SKY_W + 512 + k] == half[k];
    }
    check_expect(same, "the eastern half is not first in the morning and second in the evening");
    check_expect(sky.read(&sky, 18 * 128 + 64, morning, rgb) && morning[219 * SKY_W] == morning[218 * SKY_W]
                 && morning[10 * SKY_W + 5] == morning[10 * SKY_W + 517],
                 "the night picture does not fill both halves and the last row");
    /*
     * A piece a call (read_part, a frame each as the hour turns): the
     * same picture and palette as read at once, a morning's and an
     * evening's (its halves swapped); a night's in both halves and its
     * last row, under its palette (its stars fall anew each read).
     */
    {
        static const int keys[3] = { 18 * 128 + 20, 18 * 128 + 43, 18 * 128 + 64 };
        int n;
        int r;
        int part;

        for (n = 0; n < 3; n++) {
            same = sky.read(&sky, keys[n], morning, rgb);
            part = 0;
            do {
                r = sky.read_part(&sky, keys[n], part++, evening, rgb2);
            } while (r > 0 && part < 64);
            same = same && r == 0 && part <= 10;
            for (k = 0; same && k < 768; k++) {
                same = rgb[k] == rgb2[k];
            }
            if (n < 2) {
                for (k = 0; same && k < SKY_W * SKY_H; k++) {
                    same = morning[k] == evening[k];
                }
            } else {
                same = same && evening[219 * SKY_W] == evening[218 * SKY_W];
            }
            check_expect(same, "the sky read a piece a call is not the picture read at once");
        }
    }
}

/* Pictures the hold's creatures wear, by slot. */
static df_u8 g_worn[1024];

/*
 * How many lights report decals they do not have. The draw pass trusts
 * light_box and light_decals and walks what they report, so a light
 * halfway through its cut must report nothing yet (docs/lighting.md).
 * hostcheck never draws, so this asks what the draw pass would ask.
 */
static int lights_misreporting(void)
{
    int wrong;
    int i;

    wrong = 0;
    for (i = 0; i < light_count(); i++) {
        const Decal *decals;
        float cx, cy, cz, ex, ey, ez;
        int count;
        int has_box;

        count = light_decals(i, &decals);
        has_box = light_box(i, &cx, &cy, &cz, &ex, &ey, &ez);
        if (count > 0 && (!decals || !light_corners_of(i))) {
            wrong++;
        } else if (has_box && count < 1) {
            wrong++;
        }
    }
    return wrong;
}

/*
 * A streamed dungeon's lights keep their shadows (docs/streaming.md,
 * Dungeons): the hold by block cuts the very decals the hold loaded
 * whole does, rays and all; and with no budget at all, a face a call,
 * the cut goes on across calls and comes out the same. Between those
 * calls the draw pass may run, so every light must be safe to draw then.
 */
static void check_dungeon_lights(void)
{
    Dungeon hold;
    World *whole;
    Level wl;
    Level sl;
    Camera cam;
    float x, y, z, yaw, pitch;
    int want;
    int got;
    int calls;
    int lights;
    int misreported;
    int i;

    keep_clear();
    whole = world_load();
    if (!whole || !dungeon_load(&hold)) {
        check_expect(0, "dungeon lights: the hold did not load");
        return;
    }
    world_as_level(whole, &wl);
    want = light_build(&wl.scene);
    if (!dunstream_open(&hold, &sl, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "dungeon lights: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    got = light_decal_total();
    /* Again, every light, with no time at all: a face a call. */
    light_build(&sl.scene);
    lights = light_stream_pending();
    calls = 0;
    misreported = 0;
    for (i = 0; i < 2000000 && light_stream_pending() > 0; i++) {
        light_stream_service(0.0f);
        calls++;
        misreported += lights_misreporting();
    }
    printf("dungeon lights: the hold whole %d decals, by block %d, again a face a call %d (%d calls for %d queued)\n",
           want, got, light_decal_total(), calls, lights);
    printf("dungeon lights: %d lights misreported their decals between those calls\n", misreported);
    check_expect(misreported == 0, "dungeon lights: a light halfway through its cut reported decals it does not have");
    check_expect(want > 0 && got == want, "dungeon lights: the hold by block does not cut the whole hold's decals");
    check_expect(light_decal_total() == want && calls > lights,
                 "dungeon lights: cut a face a call, the hold's decals came out otherwise, or not across calls");
    dunstream_close();
    keep_clear();
}

int check_dagger(const char *root_in)
{
    World *world;
    int models;
    int tris;
    int images;
    int pix;
    const Image *shot;
    int i;
    int padded;
    int worn;
    df_u32 bytes;

    const char *root;

    root = root_in ? root_in : "daggerfall/DAGGER/ARENA2";
    mem_init();
    vfs_mount(root);
    check_data();
    check_smooth();
    check_jambs();
    check_v25();
    check_fly();
    check_levels();
    check_sound_files();

    world = world_load();
    if (!world) {
        fprintf(stderr, "fail: %s\n", world_error());
        return 1;
    }
    /*
     * Its creatures are read as they come near (world_creatures_step):
     * none at load, all of them with no camera and no budget.
     */
    {
        int k;
        int live;

        live = 0;
        for (k = 0; k < world_entity_count(world); k++) {
            live += world_entity(world, k)->live;
        }
        check_expect(world_entity_count(world) == 42 && live == 0,
                     "the hold's creatures were read at load, not as they come near");
        world_creatures_step(world, 0, 0.0f, 0.0f);
    }
    models = world_model_count(world);
    tris = world_triangle_count(world);
    images = world_image_count(world);
    printf("block %s\n", world_block_name());
    printf("models %d\n", models);
    printf("triangles %d\n", tris);
    printf("flats %d\n", world_flat_count(world));
    printf("editor skipped %d\n", world_editor_count(world));
    printf("textures %d\n", images);
    printf("uv failures %d\n", arch3d_uv_failures());
    check_expect(models == 365, "model count");
    /*
     * 9075 before arch3d kept every real corner and welded T-junctions:
     * the extra triangles are the corners of thin jambs it used to drop,
     * and the corners it inserts where faces meet partway along an edge.
     */
    check_expect(tris == 9315, "triangle count");
    /* RDB object type 2: 71 lights in the hold's five blocks. See docs/lighting.md. */
    printf("lights %d\n", world_light_count(world));
    check_expect(world_light_count(world) == 71, "light count");
    check_expect(!world_outdoor(world) && !world_sky(world), "the hold has a sky");
    {
        Level level;
        Camera cam;
        Basis basis;
        int order[512];
        int vis;
        int front;
        int faces;
        int mi;
        float x, y, z, yaw, pitch;

        world_as_level(world, &level);
        world_start(world, &x, &y, &z, &yaw, &pitch);
        camera_set(&cam, x, y, z, yaw, pitch);
        vis = scene_visible_models(&level.scene, &cam, order, 512);
        check_expect(vis > 0 && vis < models, "hold cull kept every model");
        /* Occlusion only ever removes from the frustum's list. */
        {
            int frustum;
            int occ_faces;
            int occ_hidden;

            scene_occ_stats(&occ_faces, &occ_hidden);
            scene_set_occlude(0);
            frustum = scene_visible_models(&level.scene, &cam, order, 512);
            scene_set_occlude(1);
            printf("hold visible %d frustum %d occluders %d\n", vis, frustum, occ_faces);
            check_expect(vis <= frustum && frustum - vis == occ_hidden,
                         "hold occlusion added models");
            vis = scene_visible_models(&level.scene, &cam, order, 512);
        }
        camera_basis(&cam, &basis);
        front = 0;
        faces = 0;
        for (mi = 0; mi < models; mi++) {
            const Instance *in;
            int f;

            in = world_instance(world, mi);
            if (!in || !in->mesh || !in->mesh->faces) {
                continue;
            }
            for (f = 0; f < in->mesh->nfaces; f++) {
                const Face *face;
                const Tri *tri;

                face = &in->mesh->faces[f];
                if (face->ntris < 2 || face->first < 0) {
                    continue;
                }
                tri = &in->mesh->tris[face->first];
                faces++;
                if (instance_faces(in, &cam, &basis,
                                   face->nx, face->ny, face->nz,
                                   tri->x[0], tri->y[0], tri->z[0])) {
                    front++;
                }
            }
        }
        check_expect(faces > 0 && front > 0 && front < faces,
                     "hold faces did not split into front and back");
    }
    check_expect(world_flat_count(world) == 130, "flat count");
    check_expect(world_editor_count(world) == 56, "editor marker count");
    check_expect(images > 0, "no textures");
    {
        int nfiles;
        int f;
        int saw_crypt;
        int saw_cave;

        /*
         * Location 50050 is woodlands. Key 122 resolves to TEXTURE.319,
         * and cave set 147 records that exist in 347 move there.
         */
        saw_crypt = 0;
        saw_cave = 0;
        nfiles = world_texture_file_count(world);
        for (f = 0; f < nfiles; f++) {
            int file;

            file = world_texture_file(world, f);
            if (file == 319) {
                saw_crypt = 1;
            }
            if (file == 347) {
                saw_cave = 1;
            }
        }
        check_expect(saw_crypt, "woodland crypt set was not selected");
        check_expect(saw_cave, "woodland cave set was not selected");
    }
    {
        const Flat *torch;
        float sx, sy, sz, syaw, spitch;
        float minx, miny, minz, maxx, maxy, maxz;
        int outside;

        {
            int over;
            int on;

            on = flats_on_floor(world, &over);
            printf("hold flats standing on the floor: %d of %d over one\n", on, over);
            check_expect(over > 100 && on * 3 > over, "the hold's flats do not stand on its floors");
        }
        torch = find_flat(world, 210, 6);
        check_expect(torch && check_near(torch->w, 16.0f, 0.01f) && check_near(torch->h, 94.0f, 0.01f),
               "torch 210/6 is not 16 by 94");
        /* Its four frames in one sheet, the square it had anyway, and they differ. */
        if (torch) {
            const Image *ti;

            ti = world_image(world, torch->slot);
            check_expect(ti && ti->frames == 4 && ti->pot_w == 128 && ti->frame_cols >= 4
                         && ti->frame_dx == 18 && torch->rate == 12
                         && memcmp(ti->pot, ti->pot + ti->frame_dx, 16) != 0,
                         "the torch's frames are not a sheet of four at 12 a second");
        }
        {
            Level hl;
            int k;
            int flick;

            world_as_level(world, &hl);
            flick = 0;
            for (k = 0; k < hl.scene.nlights; k++) {
                flick += hl.scene.lights[k].flicker;
            }
            printf("hold lights flickering: %d of %d\n", flick, hl.scene.nlights);
            {
                int lit;
                int through;

                /* Through its walls the hold's lights cut 2282 decals; seeing only what they reach, fewer. */
                light_set_shadow(0);
                through = light_build(&hl.scene);
                light_set_shadow(1);
                lit = light_build(&hl.scene);
                printf("hold decals: %d, %d through walls\n", lit, through);
                check_expect(through == 2282 && lit > 1000 && lit < through * 4 / 5,
                             "the hold's lights still reach through its walls");
            }
            check_expect(flick > 20 && flick < hl.scene.nlights, "the hold's flames do not flicker, or all do");
        }
        check_sound_hold(world);
        check_doors_hold(world);
        world_start(world, &sx, &sy, &sz, &syaw, &spitch);
        printf("start %.2f %.2f %.2f yaw %.3f pitch %.3f\n", sx, sy, sz, syaw, spitch);
        /* The enter marker, the cave by the campfire at the bottom of the hold; not marker 10, beside the way out. */
        check_expect(check_near(sx, 510.0f, 0.1f) && check_near(sy, 25.0f, 0.1f) &&
               check_near(sz, 1672.0f, 0.1f),
               "start is not the enter marker in S0000999");
        check_expect(check_near(syaw, 0.0f, 0.001f) && check_near(spitch, 0.0f, 0.001f),
               "start look is not level along +Z");
        world_bounds(world, &minx, &miny, &minz, &maxx, &maxy, &maxz);
        check_expect(minx < -2000.0f && maxx > 4000.0f && minz < -2000.0f && maxz > 4000.0f,
               "border blocks are not around the hold");
        outside = 0;
        for (i = 0; i < models; i++) {
            const Instance *in;

            in = world_instance(world, i);
            if (in && (in->x < -1500.0f || in->x > 2500.0f || in->z < -1500.0f || in->z > 2500.0f)) {
                outside++;
            }
        }
        check_expect(outside > 40, "border models were not shifted onto their tiles");
    }

    /*
     * The pictures the hold's creatures wear go to the card by distance
     * (texcache_resident), not in the load's budget: counted apart.
     */
    {
        int e;

        for (i = 0; i < images && i < 1024; i++) {
            g_worn[i] = 0;
        }
        for (e = 0; e < world_entity_count(world); e++) {
            const Entity *ent;
            int k;

            ent = world_entity(world, e);
            for (k = 0; k < ENTITY_VIEWS; k++) {
                if (ent->pose[ENTITY_WALK].slot[k] < 1024) {
                    g_worn[ent->pose[ENTITY_WALK].slot[k]] = 1;
                }
                if (ent->pose[ENTITY_IDLE].slot[k] < 1024) {
                    g_worn[ent->pose[ENTITY_IDLE].slot[k]] = 1;
                }
            }
        }
    }
    shot = 0;
    bytes = 0;
    padded = 0;
    worn = 0;
    for (i = 0; i < images; i++) {
        const Image *img;
        df_u32 n;

        img = world_image(world, i);
        if (g_worn[i]) {
            /* A creature's record: a sprite, padded at its own size, never a wall's. */
            worn += world_image_cutout(world, i) && img->span_w == img->width
                && img->span_h == img->height && img->pot_w == img->pot_h;
            continue;
        }
        check_expect(img->pot_w == img->pot_h && img->pot_w >= 1, "upload is not square");
        check_expect(img->span_w >= 1 && img->span_h >= 1 &&
               img->span_w <= img->pot_w && img->span_h <= img->pot_h,
               "texture span is outside the square");
        /*
         * A wall repeats, so its copies must tile the square. A sprite
         * is its own size in the corner, padded clear: nothing tiles.
         */
        if (world_image_cutout(world, i) && img->span_w == img->width
            && img->span_h == img->height) {
            padded++;
        } else {
            check_expect((img->pot_w % img->span_w) == 0 && (img->pot_h % img->span_h) == 0,
                   "wall texture span does not tile the square");
        }
        if (img->width == 64 && img->height == 32) {
            check_expect(img->pot_w == 64 && img->span_w == 64 && img->span_h == 32,
                   "wall 64x32 was not tiled into 64x64");
        }
        if (img->width == 16 && img->height == 94) {
            check_expect(img->pot_w == 128 && img->span_w == 16 && img->span_h == 94
                         && img->pot[0] == img->index[0] && img->pot[127] == 0
                         && img->pot[127 * 128] == 0,
                   "torch 16x94 was not padded at its own size into 128x128");
        }
        n = (df_u32)img->pot_w * (df_u32)img->pot_h;
        if (world_image_cutout(world, i)) {
            n *= 2;
        }
        bytes += n;
        if (!shot && unique_indices(img) > 1) {
            shot = img;
        }
    }
    printf("tmu bytes %u, sprites padded %d, creatures' pictures %d\n", bytes, padded, worn);
    check_expect(padded == 33, "every sprite in the hold should be padded, none shared with a wall");
    check_expect(worn == 100, "the hold's creatures do not wear 100 padded sprite pictures");
    check_expect(bytes > 0 && bytes < 1500000, "texture bytes do not fit a 2MB TMU");
    check_expect(shot != 0, "no textured image");
    if (shot) {
        pix = unique_indices(shot);
        printf("tex0 %dx%d unique %d pot %dx%d span %dx%d\n",
               shot->width, shot->height, pix, shot->pot_w, shot->pot_h,
               shot->span_w, shot->span_h);
        check_expect(write_ppm(shot, world_palette(world), "build/tex0.ppm"), "ppm write failed");
    }
    check_view(world);
    check_enemy_rules();
    /*
     * Water (docs/water.md): the hold is dry. Anthor's Web is flooded
     * low: its level within the place's height, each rectangle over a
     * model that reaches below it, and the scene says a body under it.
     */
    check_expect(world_water_count(world) == 0, "Privateer's Hold has water");
    {
        Dungeon wd;
        World *wet;

        check_expect(g_have_wet, "Anthor's Web is not in the dungeon list");
        if (g_have_wet && dungeon_load_at(g_wet.region, g_wet.index, &wd)
            && (wet = world_load_dungeon(&wd)) != 0) {
            Level wl;
            float minx, miny, minz, maxx, maxy, maxz;
            int over;
            int k;

            world_bounds(wet, &minx, &miny, &minz, &maxx, &maxy, &maxz);
            world_as_level(wet, &wl);
            over = 0;
            for (k = 0; k < world_water_count(wet); k++) {
                const Water *w;
                int m;

                w = world_water(wet, k);
                for (m = 0; m < wl.scene.nmodels; m++) {
                    const Bound *b;

                    b = &wl.scene.bounds[m];
                    if (b->cy - b->ey < w->y && b->cx + b->ex > w->x0 && b->cx - b->ex < w->x1
                        && b->cz + b->ez > w->z0 && b->cz - b->ez < w->z1) {
                        over++;
                        break;
                    }
                }
            }
            if (world_water_count(wet) > 0) {
                const Water *w0;

                w0 = world_water(wet, 0);
                printf("Anthor's Web water: %d rectangles, level %.0f, the place %.0f..%.0f\n",
                       world_water_count(wet), w0->y, miny, maxy);
                check_expect(w0->y > miny && w0->y < maxy && over == world_water_count(wet)
                             && wl.scene.nwater == world_water_count(wet)
                             && scene_water_body(&wl.scene, (w0->x0 + w0->x1) * 0.5f, w0->y - 10.0f,
                                                 (w0->z0 + w0->z1) * 0.5f) == WATER_HEAD,
                             "Anthor's Web's water is not over its flooded rooms at a level within it");
            } else {
                check_expect(0, "Anthor's Web has no water");
            }
        }
    }
    {
        Dungeon hold;

        check_expect(dungeon_load(&hold), "the hold's record did not read again");
        check_hold_creatures(world, &hold);
    }

    {
        Dungeon one;
        World *other;
        float sx, sy, sz, syaw, spitch;
        float minx, miny, minz, maxx, maxy, maxz;

        check_expect(g_have_region61, "region 61 has no dungeon");
        if (g_have_region61 && dungeon_load_at(g_region61.region, g_region61.index, &one)) {
            printf("other %s blocks %d\n", one.title, one.nblocks);
            other = world_load_dungeon(&one);
            if (!other) {
                fprintf(stderr, "fail: %s\n", world_error());
            }
            check_expect(other && world_model_count(other) > 0, "region 61 did not load");
            if (other) {
                world_start(other, &sx, &sy, &sz, &syaw, &spitch);
                world_bounds(other, &minx, &miny, &minz, &maxx, &maxy, &maxz);
                check_expect(sx >= minx && sx <= maxx && sy >= miny && sy <= maxy
                       && sz >= minz && sz <= maxz,
                       "region 61 start is outside the dungeon");
            }
        } else if (g_have_region61) {
            check_expect(0, dungeon_error());
        }
    }

    /*
     * Inside Daggerfall's castle, facing a closed door down a hall. The
     * frustum keeps 38 models at the default far clip; 4 have a pixel
     * on the glass. Occlusion has to get near that, with walls, floor
     * and the door leaves covering each other's seams.
     */
    {
        Dungeon castle;
        World *inside;

        check_expect(g_have_castle, "Daggerfall castle is not in the dungeon list");
        if (g_have_castle && dungeon_load_at(g_castle.region, g_castle.index, &castle)) {
            inside = world_load_dungeon(&castle);
            check_expect(inside != 0, "Daggerfall castle did not load");
            if (inside) {
                Level level;
                Camera cam;
                int order[2048];
                int vis;
                int frustum;

                world_as_level(inside, &level);
                /*
                 * Its creatures, read as they come near (docs/entities.md):
                 * from its start, those within a block now, the rest later;
                 * a step with a budget reads at least a part of one more.
                 */
                {
                    Camera at;
                    float sx, sy, sz, syaw, spitch;
                    int near_left;
                    int live;
                    int far_live;
                    int k;

                    world_start(inside, &sx, &sy, &sz, &syaw, &spitch);
                    camera_set(&at, sx, sy, sz, syaw, spitch);
                    near_left = world_creatures_step(inside, &at, 2048.0f, 0.0f);
                    live = 0;
                    far_live = 0;
                    for (k = 0; k < world_entity_count(inside); k++) {
                        const Entity *e;
                        float dx;
                        float dz;

                        e = world_entity(inside, k);
                        dx = e->x - sx;
                        dz = e->z - sz;
                        live += e->live;
                        far_live += e->live && dx * dx + dz * dz > 2048.0f * 2048.0f;
                    }
                    printf("castle creatures: %d, %d read near the start, %d left\n",
                           world_entity_count(inside), live,
                           world_creatures_step(inside, &at, 0.0f, 0.001f));
                    check_expect(near_left == 0 && live > 0 && live < world_entity_count(inside) && far_live == 0,
                                 "the castle's creatures near its start were not read alone");
                    world_creatures_step(inside, 0, 0.0f, 0.0f);
                }
                /*
                 * Its teleports (docs/interact.md): a portal door and one
                 * other, each sending to a marker in the castle, a body's
                 * eye above the floor there.
                 */
                {
                    float minx, miny, minz, maxx, maxy, maxz;
                    int tele;
                    int doors;
                    int inside_box;
                    int k;

                    world_bounds(inside, &minx, &miny, &minz, &maxx, &maxy, &maxz);
                    tele = 0;
                    doors = 0;
                    inside_box = 0;
                    for (k = 0; k < world_mover_count(inside); k++) {
                        const Mover *m;

                        m = world_mover(inside, k);
                        if (!m->teleport) {
                            continue;
                        }
                        tele++;
                        doors += m->door;
                        inside_box += m->to_x > minx && m->to_x < maxx && m->to_z > minz && m->to_z < maxz
                            && m->to_y > miny + WALK_EYE * 0.5f && m->to_y < maxy;
                    }
                    printf("castle teleports: %d, %d of them doors, %d to a place in the castle\n",
                           tele, doors, inside_box);
                    check_expect(tele == 2 && doors == 1 && inside_box == 2,
                                 "the castle's portal door and teleport do not send into the castle");
                }
                scene_set_limits(DF_FAR, 1);
                camera_set(&cam, 2002.0f, 1081.0f, 1022.0f,
                           268.0f * (DF_TAU / 360.0f), 2.0f * (DF_TAU / 360.0f));
                scene_set_occlude(0);
                frustum = scene_visible_models(&level.scene, &cam, order, 2048);
                scene_set_occlude(1);
                vis = scene_visible_models(&level.scene, &cam, order, 2048);
                printf("castle door visible %d frustum %d\n", vis, frustum);
                check_expect(frustum > 30 && vis <= 8, "castle door view kept the rooms behind it");
            }
        } else if (g_have_castle) {
            check_expect(0, dungeon_error());
        }
    }

    {
        DunRef *places;
        Dungeon ext;
        World *town;
        int n;
        int i;
        int dagger;
        int farm;
        int hold_out;
        float sx, sy, sz, syaw, spitch;
        float minx, miny, minz, maxx, maxy, maxz;

        places = (DunRef *)mem_alloc((df_u32)16384 * (df_u32)sizeof(DunRef));
        check_expect(places != 0, "outdoor list allocation");
        if (places) {
            n = exterior_list(places, 16384);
            printf("outdoors %d\n", n);
            check_expect(n == 15251, "outdoor count");
            dagger = -1;
            farm = -1;
            hold_out = -1;
            for (i = 0; i < n && i < 16384; i++) {
                if (places[i].region != 17) {
                    continue;
                }
                if (df_eq(places[i].name, "Daggerfall")) {
                    dagger = i;
                }
                if (df_eq(places[i].name, "The Ashwing Farmstead")) {
                    farm = i;
                }
                if (df_eq(places[i].name, "Privateer's Hold")) {
                    hold_out = i;
                }
            }
            check_expect(dagger >= 0 && places[dagger].nblocks == 64 && places[dagger].outdoor,
                   "Daggerfall is not an 8 by 8 outdoor");
            if (dagger >= 0 && exterior_load_at(places[dagger].region, places[dagger].index, &ext)) {
                check_expect(ext.nblocks == 64 && ext.outdoor
                       && df_eq(ext.block[0].name, "WALLAA02.RMB")
                       && ext.block[0].x == 0 && ext.block[0].z == 0,
                       "Daggerfall block 0 is not WALLAA02.RMB");
            } else if (dagger >= 0) {
                check_expect(0, dungeon_error());
            }
            check_expect(farm >= 0 && places[farm].nblocks == 1, "Ashwing Farmstead missing");
            /* The hold's outside: its way down, by the location the dungeon shares. */
            check_expect(hold_out >= 0, "Privateer's Hold has no outside");
            if (hold_out >= 0 && exterior_load_at(places[hold_out].region, places[hold_out].index, &ext)) {
                World *top;

                top = world_load_dungeon(&ext);
                check_expect(top != 0, "Privateer's Hold's outside did not load");
                if (top) {
                    int down;

                    down = world_passage(top, DOOR_DUNGEON, 0.0f, 0.0f, 0.0f);
                    printf("hold outside: location %d, %d blocks, %d ways down",
                           ext.location_id, ext.nblocks, count_use(top, USE_PASSAGE, DOOR_DUNGEON));
                    if (down >= 0) {
                        const Usable *u;

                        u = world_usable(top, down);
                        printf(", at %.0f %.0f %.0f facing %.2f %.2f, %.0f by %.0f",
                               u->cx, u->cy, u->cz, u->nx, u->nz, u->hw * 2.0f, u->hh * 2.0f);
                    }
                    printf("\n");
                    check_expect(down >= 0, "Privateer's Hold's outside has no way down");
                }
                {
                    Dungeon below;

                    /* The way down leads to the dungeon of the same location: the viewer pairs them so. */
                    check_expect(dungeon_load(&below), dungeon_error());
                    printf("hold below: location %d, under %d\n", below.location_id, below.exterior_id);
                    check_expect(below.exterior_id == ext.exterior_id,
                                 "Privateer's Hold's dungeon is not under its outside");
                }
            }
            if (farm >= 0 && exterior_load_at(places[farm].region, places[farm].index, &ext)) {
                printf("outdoor %s blocks %d\n", ext.title, ext.nblocks);
                town = world_load_dungeon(&ext);
                if (!town) {
                    fprintf(stderr, "fail: %s\n", world_error());
                }
                check_expect(town && world_model_count(town) > 0, "outdoor did not load");
                if (town) {
                    Level tl;
                    int glow;
                    int k;

                    world_as_level(town, &tl);
                    glow = 0;
                    for (k = 0; k < world_flat_count(town); k++) {
                        glow += world_flat(town, k)->glow ? 1 : 0;
                    }
                    {
                        int over;
                        int on;

                        on = flats_on_floor(town, &over);
                        check_expect(over > 10 && on * 10 > over * 8, "a farm's flats do not stand on the ground");
                    }
                    check_expect(world_outdoor(town) && tl.sky && tl.scene.outdoor
                                 && world_light_count(town) == glow,
                                 "an outdoor place has no sky, or a lamp flat has no light");
                }
                if (town) {
                    check_sound_town(town);
                    check_doors_town(town, &ext);
                    check_seasons(&ext);
                }
                if (town) {
                    world_start(town, &sx, &sy, &sz, &syaw, &spitch);
                    world_bounds(town, &minx, &miny, &minz, &maxx, &maxy, &maxz);
                    check_expect(sx >= minx && sx <= maxx && sz >= minz && sz <= maxz
                           && sy >= maxy,
                           "outdoor start is not above the place");
                }
            } else if (farm >= 0) {
                check_expect(0, dungeon_error());
            }
        }
    }

    check_skies();
    check_land();
    check_stream();
    check_dungeon_lights();
    return check_failed() ? 1 : 0;
}
