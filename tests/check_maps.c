/*
 * What the GFALL tests of the retired player checked of Daggerfall's own pieces that remain: the TEXT.RSC macros, the arrow of the
 * maps, the indoor map over the hold and a building's people as entities with the load hooks. Run by hostcheck after the
 * dagger checks (ARENA2 mounted, MAIN.DAT merged).
 */

#include "check_util.h"
#include "check_maps.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/scene/keep.h"
#include "engine/scene/ent.h"
#include "engine/scene/automap.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/places/entbuild.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/places/town.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/dunmap.h"
#include "game/dagger/play/marker.h"
#include "game/dagger/play/words.h"

#include "engine/scene/stream.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/formats/img.h"
#include "engine/scene/walker.h"
#include <stdio.h>
#include <string.h>

/* One passage's move as the program makes it: the place left, the next opened, stood in. */
static int travel_go(Level *level, Camera *cam, TravelPose *at)
{
    travel_leave();
    if (!travel_open(level, 2)) {
        check_expect(0, travel_error());
        return 0;
    }
    travel_land(level, cam, WALK_EYE, at);
    return 1;
}

static void write_screen(const char *path, const df_u8 *pixels)
{
    static df_u8 rgb[768];
    FILE *f;
    int i;

    if (!img_art_palette(rgb)) {
        return;
    }
    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n320 200\n255\n");
    for (i = 0; i < 320 * 200; i++) {
        fwrite(&rgb[pixels[i] * 3], 1, 3, f);
    }
    fclose(f);
}

/* As write_screen, under the travel map's palette when map is 1. */
static int building_of_type(int region, int index, int type, int *block, int *record)
{
    const TownBuilding *rows;
    int count;
    int i;

    rows = town_buildings(region, index, &count);
    for (i = 0; rows && i < count; i++) {
        if (rows[i].type == type) {
            *block = rows[i].block;
            *record = rows[i].record;
            return i;
        }
    }
    return -1;
}

/* Daggerfall's index in its region's list. */
static int daggerfall_index(void)
{
    static DunRef towns[16000];
    int i;

    i = exterior_list(towns, 16000);
    while (--i >= 0) {
        if (towns[i].region == 17 && df_eq(towns[i].name, "Daggerfall")) {
            return towns[i].index;
        }
    }
    return -1;
}

static df_u8 g_poly_pic[640 * 480];

static void draw_polygon(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb)
{
    int i;

    (void)ctx;
    for (i = 0; i < n; i++) {
        const AutomapCorner *a;
        const AutomapCorner *b;
        int k;

        a = &corner[i];
        b = &corner[(i + 1) % n];
        for (k = 0; k <= 32; k++) {
            int x;
            int y;

            x = (int)(a->x + (b->x - a->x) * (float)k / 32.0f);
            y = 479 - (int)(a->y + (b->y - a->y) * (float)k / 32.0f);
            if (x >= 0 && x < 640 && y >= 0 && y < 480) {
                g_poly_pic[y * 640 + x] = (df_u8)(1 + ((rgb >> 8) & 0xff) / 2);
            }
        }
    }
}

static void write_polygons(const char *path, const Scene *scene, const AutomapView *view)
{
    FILE *f;
    int i;

    mem_zero(g_poly_pic, sizeof(g_poly_pic));
    automap_polygons(scene, view, draw_polygon, 0);
    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P5\n640 480\n255\n");
    for (i = 0; i < 640 * 480; i++) {
        fputc(g_poly_pic[i] ? 128 + g_poly_pic[i] / 2 : 0, f);
    }
    fclose(f);
}
static void count_polygon(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb)
{
    (void)corner;
    (void)n;
    (void)rgb;
    (*(int *)ctx)++;
}

static void check_words(void)
{
    static const int records[] = { 7206, 7207, 7208, 7209, 7215, 7218, 7225, 7231, 7333, 7250, 7251, 7266, 7281, 7304, 7261, 7294, 8050, 8055, 8062, 8063 };
    WordsPlayer sheet;
    Words w;
    char out[512];
    int r;
    int i;

    mem_zero(&sheet, sizeof(sheet));
    strcpy(sheet.name, "Ada Lovel");
    sheet.race = RACE_BRETON;
    mem_zero(&w, sizeof(w));
    w.pc = &sheet;
    w.key = "the bank";
    w.loc = "the bank";
    w.direction = "north";
    w.hint = "it is north";
    w.city = "Daggerfall";
    w.region = "Daggerfall";
    w.crime = "theft";
    w.penalty = "a fine";
    w.tone = 1;
    w.reaction = 5;
    words_use(&w);
    for (r = 0; r < (int)(sizeof(records) / sizeof(records[0])); r++) {
        check_expect(textrsc_variants(records[r]) >= 1, "words: a record of talk does not read");
        for (i = 0; i < 20; i++) {
            check_expect(words_say(records[r], out, (int)sizeof(out)) > 0, "words: a record says nothing");
            check_expect(strchr(out, '%') == 0, "words: a macro is left in what is said");
        }
    }
    /* Every variant of a two-variant record comes up by its number, and a number past wraps round. */
    {
        char a[64];
        char b[64];

        check_expect(textrsc_variants(7221) == 2, "words: 7221 has two variants");
        textrsc_variant(7221, 0, a, (int)sizeof(a));
        textrsc_variant(7221, 2, b, (int)sizeof(b));
        check_expect(strcmp(a, b) == 0, "words: a variant's number does not wrap");
        textrsc_variant(7221, 1, b, (int)sizeof(b));
        check_expect(strcmp(a, b) != 0, "words: the variants are one");
    }
    words_expand("%pcf the %ra", out, (int)sizeof(out));
    check_expect(strcmp(out, "Ada the Breton") == 0, "words: the first name and the race");
    words_use(0);
}

static void check_marker(void)
{
    static unsigned char up[MARKER_SIDE * MARKER_SIDE];
    static unsigned char right[MARKER_SIDE * MARKER_SIDE];
    int fill;
    int edge;
    int top_row;
    int right_col;
    int i;

    marker_compose(up, 0);
    marker_compose(right, MARKER_STEPS / 4);
    fill = 0;
    edge = 0;
    top_row = MARKER_SIDE;
    right_col = -1;
    for (i = 0; i < MARKER_SIDE * MARKER_SIDE; i++) {
        fill += up[i] == MARKER_FILL;
        edge += up[i] == MARKER_EDGE;
        if (up[i] && i / MARKER_SIDE < top_row) {
            top_row = i / MARKER_SIDE;
        }
        if (right[i] && i % MARKER_SIDE > right_col) {
            right_col = i % MARKER_SIDE;
        }
    }
    check_expect(fill > 60 && edge > 60, "marker: the arrow has no fill or no outline");
    check_expect(top_row <= 2 && up[top_row * MARKER_SIDE + MARKER_SIDE / 2 - 1], "marker: step 0's tip is not at the top middle");
    check_expect(right_col >= MARKER_SIDE - 3 && right[(MARKER_SIDE / 2) * MARKER_SIDE + right_col - 1], "marker: a quarter turn does not point right");
    check_expect(marker_step(0.0f) == 0 && marker_step(DF_TAU / 4.0f) == MARKER_STEPS / 4 && marker_step(-0.01f) == 0,
                 "marker: the steps of a turn");
}

static void check_dunmap(void)
{
    Level level;
    Camera cam;
    TravelPose at;
    float feet[3];
    float sx;
    float sy;
    int top;
    int behind;
    int holes;
    int arrow;
    int i;
    df_u32 place;

    keep_clear();
    automap_clear();
    travel_start_game();
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    place = automap_place();
    check_expect(place != 0 && (place >> 30) == 1u, "dunmap: the hold's automap place is not named, a dungeon's");
    feet[0] = at.x; feet[1] = at.y; feet[2] = at.z;
    automap_reveal(&level.scene);
    check_expect(automap_count() > 300, "dunmap: the hold revealed has too few rows");
    check_expect(dunmap_open(feet, at.yaw), "dunmap: it did not open");
    /* Classic's (1, 1)..(318, 169) of 320 x 200 on 640 x 480 from the bottom: the engine's own test's numbers. */
    /* DFU's (1, 1, 318, 169) is a corner and a size: x 2..638, y from the bottom 480 - 2.4 x 170 .. 480 - 2.4. */
    check_expect(dunmap_view()->x0 == 2.0f && dunmap_view()->x1 == 638.0f && dunmap_view()->y1 > 477.5f && dunmap_view()->y1 < 477.7f
                 && dunmap_view()->y0 > 71.9f && dunmap_view()->y0 < 72.1f, "dunmap: the map's rectangle");
    check_expect(automap_to_screen(dunmap_view(), feet[0], feet[1], feet[2], &sx, &sy),
                 "dunmap: the player is not on the map from above");
    top = 0;
    automap_polygons(&level.scene, dunmap_view(), count_polygon, &top);
    check_expect(top > 20, "dunmap: the hold from above draws nothing");
    write_polygons("build/dunmap-top.pgm", &level.scene, dunmap_view());
    dunmap_compose();
    holes = 0;
    arrow = 0;
    for (i = 0; i < 320 * 200; i++) {
        int x;
        int y;

        x = i % 320;
        y = i / 320;
        if (!dunmap_pixels()[i]) {
            holes += x >= 1 && x < 319 && y >= 1 && y < 170;
            check_expect(x >= 1 && x < 319 && y >= 1 && y < 170, "dunmap: a clear pixel outside the map's hole");
            if (!(x >= 1 && x < 319 && y >= 1 && y < 170)) {
                break;
            }
        } else if (x >= 1 && x < 319 && y >= 1 && y < 170) {
            arrow++;
        }
    }
    /* The hole whole: the arrow is a picture of its own now, over it. */
    check_expect(holes > 52000 && arrow == 0, "dunmap: the hole is not whole");
    {
        float mx;
        float my;
        float angle;

        check_expect(dunmap_marker(&mx, &my, &angle) && mx > 2.0f && mx < 638.0f && my > 72.0f && my < 478.0f,
                     "dunmap: the player's arrow is not on the map");
        /* From above the facing is up the map. */
        check_expect(df_absf(df_wrap_tau(angle + 0.05f) - 0.05f) < 0.05f, "dunmap: from above, the arrow does not point up");
    }
    /* Dragging: the map follows the pointer, the view's centre the other way; a right drag turns it. */
    {
        float cx;
        float cz;
        float yaw;

        cx = dunmap_view()->cx;
        cz = dunmap_view()->cz;
        yaw = dunmap_view()->yaw;
        dunmap_drag(10, 0);
        check_expect(df_absf(dunmap_view()->cx - cx) + df_absf(dunmap_view()->cz - cz) > 1.0f, "dunmap: a drag did not move the view");
        dunmap_drag(-10, 0);
        check_expect(df_absf(dunmap_view()->cx - cx) < 0.5f && df_absf(dunmap_view()->cz - cz) < 0.5f, "dunmap: a drag back did not come back");
        dunmap_turn_by(20);
        check_expect(df_absf(dunmap_view()->yaw - yaw) > 0.1f, "dunmap: a right drag did not turn");
        dunmap_turn_by(-20);
        check_expect(dunmap_repeats(110, 175) && !dunmap_repeats(285, 175) && dunmap_on_map(160, 80) && !dunmap_on_map(160, 180),
                     "dunmap: which buttons repeat, where the map is");
    }
    write_screen("build/dunmap0.ppm", dunmap_pixels());
    /* From behind and above: the player still in view, every face toward the camera. */
    dunmap_click(80, 175);
    check_expect(dunmap_view()->mode == AUTOMAP_3D, "dunmap: the grid did not turn to the 3D view");
    check_expect(automap_to_screen(dunmap_view(), feet[0], feet[1], feet[2], &sx, &sy),
                 "dunmap: the player is not in the 3D view");
    behind = 0;
    automap_polygons(&level.scene, dunmap_view(), count_polygon, &behind);
    write_polygons("build/dunmap-3d.pgm", &level.scene, dunmap_view());
    check_expect(behind > 0, "dunmap: the 3D view draws nothing");
    /* The stairs move the slice a storey; the compass brings the view back; the wheel zooms; Exit leaves. */
    dunmap_click(80, 175);
    dunmap_click(240, 175);
    check_expect(dunmap_storey() == 1 && dunmap_view()->bottom > feet[1], "dunmap: upstairs");
    {
        float scale;

        scale = dunmap_view()->scale;
        dunmap_wheel(1);
        check_expect(dunmap_view()->scale > scale, "dunmap: the wheel did not zoom in");
    }
    dunmap_click(110, 175);
    dunmap_click(5, 175);
    check_expect(dunmap_storey() == 0 && dunmap_view()->cx == feet[0], "dunmap: the compass did not come back");
    check_expect(dunmap_key('M') == DUNMAP_LEAVE && dunmap_click(285, 175) == DUNMAP_LEAVE, "dunmap: M or Exit");
    dunmap_close();
    travel_leave();
    check_expect(automap_place() == 0, "dunmap: a place is named after leaving it");
    automap_clear();
    keep_clear();
    printf("dunmap: the hold's %d polygons from above, %d from behind; the chrome's hole and arrow; build/dunmap0.ppm\n",
           top, behind);
}

/* An armorer in Daggerfall, entered through its own door: its shelf, its person, what the building says; a save holds it all. */
/* The load hooks of a building's people: one changed (its faction made 9999), one added (owner 0x7123). */
static int g_hook_changed, g_hook_added;

static int test_change_hook(const EntBuild *b, Entity *e)
{
    (void)b;
    g_hook_changed++;
    if (b->nth == 0) {
        e->use.what = 9999;
    }
    return 1;
}

static int test_add_hook(const EntBuild *b, Entity *out, int cap)
{
    if (cap < 1 || !b->has_first) {
        return 0;
    }
    g_hook_added++;
    if (!entbuild_person(&out[0], 182, 0, 0x7123, b->first_x + 40.0f, b->first_y, b->first_z)) {
        return 0;
    }
    return 1;
}

static void check_people_in_town(void)
{
    Level level;
    Camera cam;
    TravelPose at;
    TravelWhere where;
    float feet[3];
    int chunk[2];
    int index;
    int px;
    int py;
    int block;
    int record;
    int i;
    int type;
    int faction;
    int quality;
    unsigned place;
    int shelves;
    int folk;
    double wx;
    double wz;

    factionfile_open();
    index = daggerfall_index();
    if (index < 0) {
        return;
    }
    if (building_of_type(17, index, BUILDING_ARMORER, &block, &record) < 0) {
        check_expect(0, "people: Daggerfall has no armorer");
        return;
    }
    worldmap_open();
    worldmap_pixel_of(17, index, &px, &py);
    wx = ((double)px + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
    wz = ((double)(WORLDMAP_H - 1 - py) + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
    mem_zero(&where, sizeof(where));
    where.kind = TRAVEL_INSIDE;
    where.located = 1;
    where.region = 17;
    where.index = index;
    where.block = block;
    where.record = record;
    where.has_back = 1;
    where.back_x = wx;
    where.back_z = wz;
    feet[0] = 0.0f;
    feet[1] = 0.0f;
    feet[2] = 0.0f;
    chunk[0] = 0;
    chunk[1] = 0;
    travel_from_save(&where, feet, chunk, 0.0f, 0.0f);
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    check_expect(travel_building(&type, &faction, &quality, &place) && type == BUILDING_ARMORER && quality >= 0 && quality <= 20,
                 "people: the armorer's inside does not say it is one");
    check_expect(travel_region() == 17, "people: the armorer's region");
    shelves = 0;
    folk = 0;
    for (i = 0; i < level.scene.nusables; i++) {
        const Usable *u;

        u = &level.scene.usables[i];
        shelves += u->kind == USE_OTHER && u->what == OTHER_SHELF;
    }
    /* A building's people are entities now (a range beside the scene's): the ones the use ray can take to talk. */
    {
        EntList lists[ENT_LISTS];
        int nl, l, k, faced;
        const Entity *first_person;

        ent_place(&level.scene);
        nl = ent_lists(&level.scene, lists, ENT_LISTS);
        faced = 0;
        first_person = 0;
        for (l = 0; l < nl; l++) {
            for (k = 0; k < lists[l].n; k++) {
                const Entity *e;

                e = &lists[l].first[k];
                if ((e->flags & ENT_USABLE) && e->use.kind == USE_TALK) {
                    folk++;
                    if (!first_person) { first_person = e; }
                    faced += e->live != 0 && (e->flags & ENT_GHOST) != 0;
                }
            }
        }
        check_expect(shelves > 0, "people: an armorer has no shelves");
        check_expect(folk > 0, "people: an armorer has no one in it");
        check_expect(faced == folk, "people: a person in a building is not drawn, or is solid");
        if (first_person) {
            const Faction *f;

            f = faction_find(first_person->use.what);
            check_expect(f != 0, "people: someone in the shop has a faction the file does not");
            check_expect(faces_of_flat(first_person->use.tag >> 7, first_person->use.tag & 0x7f) >= 0, "people: a person's flat has no face");
        }
    }
    /* The load hooks (entbuild_set_hooks): the same building again with a hook that changes one person and one that adds a person of its own: one more, the changed one named, the added one theirs. */
    {
        int before, after, changed, added;

        before = folk;
        travel_leave();
        g_hook_changed = 0; g_hook_added = 0;
        entbuild_set_hooks(test_change_hook, test_add_hook);
        travel_from_save(&where, feet, chunk, 0.0f, 0.0f);
        if (!travel_go(&level, &cam, &at)) {
            entbuild_set_hooks(0, 0);
            return;
        }
        {
            EntList lists[ENT_LISTS];
            int nl, l, k;

            ent_place(&level.scene);
            nl = ent_lists(&level.scene, lists, ENT_LISTS);
            after = 0; changed = 0; added = 0;
            for (l = 0; l < nl; l++) {
                for (k = 0; k < lists[l].n; k++) {
                    const Entity *e;

                    e = &lists[l].first[k];
                    if ((e->flags & ENT_USABLE) && e->use.kind == USE_TALK) {
                        after++;
                        changed += e->use.what == 9999;
                        added += e->owner == 0x7123u;
                    }
                }
            }
        }
        entbuild_set_hooks(0, 0);
        check_expect(after == before + 1 && g_hook_changed == before && changed == 1 && added == 1 && g_hook_added == 1,
                     "people: the load hooks did not change one of the building's people and add one of their own");
    }
    travel_leave();
    printf("people in town: Daggerfall's armorer, quality %d, %d shelves, %d people\n", quality, shelves, folk);
}

void check_maps(void)
{
    check_words();
    check_marker();
    check_dunmap();
    check_people_in_town();
}
