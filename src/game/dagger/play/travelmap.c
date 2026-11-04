#include "game/dagger/play/travelmap.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/atlas.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/words.h"
#include "game/dagger/play/ride.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/palette.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/formats/worldmap.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See travelmap.h. Rectangles and files are DFU's DaggerfallTravelMapWindow's and DaggerfallTravelPopUp's. */

#define VK_BACK_KEY 0x08
#define VK_RETURN_KEY 0x0D
#define VK_ESCAPE_KEY 0x1B
#define VK_SPACE_KEY 0x20

/* The province map's panel, and the line over it. */
#define PANEL_X 0
#define PANEL_Y 12
#define PANEL_W 320
#define PANEL_H 160
#define BAR_Y 172
#define FRAME_Y 10
#define FRAME_H 164
static const Rect g_line_rect = { 0, 2, 320, 9 };
static const Rect g_find_rect = { 3, 175, 45, 11 };
static const Rect g_at_rect = { 3, 186, 45, 11 };
static const Rect g_exit_rect = { 278, 175, 39, 22 };
static const Rect g_side_rect = { 231, 176, 22, 20 };
static const Rect g_updown_rect = { 254, 176, 22, 20 };
/* The four filters, as TRAV01I0.IMG (lit) and TRAV01I1.IMG (shown off) cut them: their place and the part of the sheet. */
#define FILTERS 4
static const Rect g_filter_at[FILTERS] = { { 50, 175, 99, 11 }, { 50, 186, 99, 11 }, { 149, 175, 80, 11 }, { 149, 186, 80, 11 } };
static const Rect g_filter_part[FILTERS] = { { 0, 0, 99, 11 }, { 0, 11, 99, 11 }, { 99, 0, 80, 11 }, { 99, 11, 80, 11 } };
#define FILTER_SHEET_W 179
#define BUTTON_SHEET_W 45
#define ARROW_W 22
#define ARROW_H 20
enum {
    FILTER_DUNGEONS,
    FILTER_TEMPLES,
    FILTER_HOMES,
    FILTER_TOWNS
};

/* The popup: its picture at the place its header gives, and its buttons. */
#define POPUP_X 49
#define POPUP_Y 29
#define POPUP_W 223
#define POPUP_H 96
static const Rect g_cautious_rect = { 50, 51, 108, 9 };
static const Rect g_reckless_rect = { 50, 61, 108, 9 };
static const Rect g_foot_rect = { 163, 51, 108, 9 };
static const Rect g_ship_rect = { 163, 61, 108, 9 };
static const Rect g_inn_rect = { 50, 83, 108, 9 };
static const Rect g_camp_rect = { 163, 83, 108, 9 };
static const Rect g_begin_rect = { 222, 98, 48, 10 };
static const Rect g_popup_exit_rect = { 222, 112, 48, 10 };
#define MARK_W 5
/* The popup's green tick, DFU's RGB 85, 117, 48, as the art palette has it nearest. */
#define MARK_RGB_R 85
#define MARK_RGB_G 117
#define MARK_RGB_B 48
/* A day of the countdown, in real seconds. */
#define COUNT_DAY 0.05f

/* The confirmation's scroll and its buttons. */
static const Rect g_ask_rect = { 70, 60, 180, 70 };
static const Rect g_yes_rect = { 104, 112, CANVAS_BUTTON_W, CANVAS_BUTTON_H };
static const Rect g_no_rect = { 184, 112, CANVAS_BUTTON_W, CANVAS_BUTTON_H };

/* TEXT.RSC. */
#define RECORD_NO_PLACE 13
#define RECORD_TRAVEL_TO 31
#define RECORD_NO_GOLD 454

/* The world's palette index of the cross over the player's place, and of the dots by the place's type (DFU's). */
#define CROSS_INDEX 244
static const df_u8 g_dot_of_type[15] = { 33, 35, 37, 53, 237, 96, 39, 240, 51, 101, 243, 55, 246, 0, 0 };
/* Which filter hides each type. */
static const df_u8 g_filter_of_type[15] = {
    FILTER_TOWNS, FILTER_TOWNS, FILTER_TOWNS, FILTER_HOMES, FILTER_DUNGEONS, FILTER_TEMPLES, FILTER_TOWNS,
    FILTER_DUNGEONS, FILTER_HOMES, FILTER_TEMPLES, FILTER_DUNGEONS, FILTER_HOMES, FILTER_DUNGEONS, FILTER_DUNGEONS,
    FILTER_HOMES
};

/* Each province's map: its file, the part of a map that is more than one screen, and its world pixel at the panel's top left. */
typedef struct RegionMap {
    df_u8 region;
    df_u8 part;
    df_u16 x;
    df_u16 y;
    char file[13];
} RegionMap;
static const RegionMap g_maps[] = {
    { 0, 0, 212, 340, "FMAPAI00.IMG" }, { 0, 1, 322, 340, "FMAPBI00.IMG" },
    { 1, 0, 583, 279, "FMAPAI01.IMG" }, { 1, 1, 680, 279, "FMAPBI01.IMG" },
    { 1, 2, 583, 340, "FMAPCI01.IMG" }, { 1, 3, 680, 340, "FMAPDI01.IMG" },
    { 5, 0, 381, 4, "FMAP0I05.IMG" }, { 9, 0, 525, 114, "FMAP0I09.IMG" }, { 11, 0, 437, 340, "FMAP0I11.IMG" },
    { 16, 0, 578, 0, "FMAPAI16.IMG" }, { 16, 1, 680, 0, "FMAPBI16.IMG" },
    { 16, 2, 578, 52, "FMAPCI16.IMG" }, { 16, 3, 680, 52, "FMAPDI16.IMG" },
    { 17, 0, 39, 106, "FMAP0I17.IMG" }, { 18, 0, 20, 29, "FMAP0I18.IMG" }, { 19, 0, 82, 244, "FMAP0I19.IMG" },
    { 20, 0, 217, 293, "FMAP0I20.IMG" }, { 21, 0, 263, 79, "FMAP0I21.IMG" }, { 22, 0, 548, 219, "FMAP0I22.IMG" },
    { 23, 0, 680, 146, "FMAP0I23.IMG" }, { 26, 0, 680, 80, "FMAP0I26.IMG" }, { 32, 0, 41, 0, "FMAP0I32.IMG" },
    { 33, 0, 660, 101, "FMAP0I33.IMG" }, { 34, 0, 578, 40, "FMAP0I34.IMG" }, { 35, 0, 525, 3, "FMAP0I35.IMG" },
    { 36, 0, 440, 40, "FMAP0I36.IMG" }, { 37, 0, 448, 0, "FMAP0I37.IMG" }, { 38, 0, 366, 0, "FMAP0I38.IMG" },
    { 39, 0, 300, 8, "FMAP0I39.IMG" }, { 40, 0, 202, 0, "FMAP0I40.IMG" }, { 41, 0, 223, 6, "FMAP0I41.IMG" },
    { 42, 0, 148, 76, "FMAP0I42.IMG" }, { 43, 0, 15, 340, "FMAP0I43.IMG" }, { 44, 0, 61, 340, "FMAP0I44.IMG" },
    { 45, 0, 86, 338, "FMAP0I45.IMG" }, { 46, 0, 132, 340, "FMAP0I46.IMG" }, { 47, 0, 344, 309, "FMAP0I47.IMG" },
    { 48, 0, 381, 251, "FMAP0I48.IMG" }, { 49, 0, 553, 255, "FMAP0I49.IMG" }, { 50, 0, 661, 217, "FMAP0I50.IMG" },
    { 51, 0, 672, 275, "FMAP0I51.IMG" }, { 52, 0, 680, 256, "FMAP0I52.IMG" }, { 53, 0, 680, 340, "FMAP0I53.IMG" },
    { 54, 0, 491, 340, "FMAP0I54.IMG" }, { 55, 0, 293, 340, "FMAP0I55.IMG" }, { 56, 0, 263, 340, "FMAP0I56.IMG" },
    { 57, 0, 680, 157, "FMAP0I57.IMG" }, { 58, 0, 17, 53, "FMAP0I58.IMG" }, { 59, 0, 0, 0, "FMAP0I59.IMG" },
    { 60, 0, 107, 11, "FMAP0I60.IMG" }, { 61, 0, 255, 275, "FMAP0I61.IMG" }
};
#define MAPS ((int)(sizeof(g_maps) / sizeof(g_maps[0])))
/*
 * Betony's map is drawn four times larger than the world: DFU's own exception.
 * Its corner is fitted to the island: DFU's (80, 123) comes with hover offsets
 * in another form, and its 25 places then land on the island's land, every one.
 */
#define BETONY 19
#define BETONY_SCALE 4

enum {
    STEP_WORLD,
    STEP_REGION,
    STEP_ASK,
    STEP_POPUP,
    STEP_COUNTING,
    STEP_FIND
};

#define NAMES_MOST 4096
#define LINE_TEXT 96

static struct {
    int open;
    Player *player;
    Composer composer;
    df_u8 rgb[768];
    /* The art palette's colours as the map's nearest: a picture under ART_PAL.COL drawn on the map's. */
    df_u8 art_to_map[256];
    df_u8 ink;
    df_u8 shadow;
    const df_u8 *world;
    const df_u8 *mask;
    const df_u8 *frame;
    const df_u8 *buttons;
    const df_u8 *filters_on;
    const df_u8 *filters_off;
    const df_u8 *arrows[4];
    const df_u8 *popup;
    const df_u8 *region_pic;
    int step;
    int region;
    int part;
    int filter_off[FILTERS];
    DunRef *names;
    int nnames;
    int here_px;
    int here_py;
    int here_region;
    int place;
    int place_px;
    int place_py;
    int place_type;
    char place_name[33];
    int reckless;
    int ship;
    int camp;
    Journey journey;
    int days_left;
    float count_time;
    char line[LINE_TEXT];
    char ask[LINE_TEXT * 2];
    char typed[33];
    const char *note;
    Words words;
    /* The screen as the map left it, before the popup or the scroll: what they did not draw is not mapped again. */
    df_u8 *under;
    int changed;
} g;

/* ---- Pictures and palettes. ---- */

/* A picture's pixels under its header, or a whole raw one: what composer_read gives, or the file's bytes. */
static const df_u8 *raw(const char *name)
{
    df_u32 size;

    return composer_table(&g.composer, name, &size);
}

/* The nearest of the map's colours to each of the art palette's. Once at opening: 65536 sums. */
static int map_palettes(void)
{
    const df_u8 *bytes;
    df_u32 size;
    Palette map;
    Palette art;
    int i;
    int k;

    bytes = composer_table(&g.composer, "FMAP_PAL.COL", &size);
    if (!bytes || !palette_load(bytes, size, &map)) {
        return 0;
    }
    bytes = composer_table(&g.composer, "ART_PAL.COL", &size);
    if (!bytes || !palette_load(bytes, size, &art)) {
        return 0;
    }
    for (i = 0; i < 256; i++) {
        g.rgb[i * 3] = map.r[i];
        g.rgb[i * 3 + 1] = map.g[i];
        g.rgb[i * 3 + 2] = map.b[i];
    }
    for (i = 0; i < 256; i++) {
        int best;
        long best_d;

        best = 0;
        best_d = 0x7fffffffL;
        /* Index 0 is clear in a cutout: an opaque colour never maps to it. */
        for (k = 1; k < 256; k++) {
            long dr;
            long dg;
            long db;
            long d;

            dr = (long)art.r[i] - map.r[k];
            dg = (long)art.g[i] - map.g[k];
            db = (long)art.b[i] - map.b[k];
            d = dr * dr + dg * dg + db * db;
            if (d < best_d) {
                best_d = d;
                best = k;
            }
        }
        g.art_to_map[i] = (df_u8)(i == 0 ? 0 : best);
    }
    g.ink = g.art_to_map[CANVAS_INK];
    g.shadow = g.art_to_map[CANVAS_INK_SHADOW];
    return 1;
}

/* What was drawn in the art palette within r, into the map's. */
static void art_rect_to_map(const Rect *r)
{
    int y;
    int x;

    for (y = r->y; y < r->y + r->h && y < CANVAS_H; y++) {
        for (x = r->x; x < r->x + r->w && x < CANVAS_W; x++) {
            g.composer.screen[y * CANVAS_W + x] = g.art_to_map[g.composer.screen[y * CANVAS_W + x]];
        }
    }
}

/* The nearest art palette index to an RGB colour: for the popup's tick. */
static int art_index_of(int r, int gr, int b)
{
    const df_u8 *bytes;
    df_u32 size;
    Palette art;
    int best;
    long best_d;
    int k;

    bytes = composer_table(&g.composer, "ART_PAL.COL", &size);
    if (!bytes || !palette_load(bytes, size, &art)) {
        return CANVAS_INK;
    }
    best = 1;
    best_d = 0x7fffffffL;
    for (k = 1; k < 256; k++) {
        long d;

        d = ((long)art.r[k] - r) * ((long)art.r[k] - r) + ((long)art.g[k] - gr) * ((long)art.g[k] - gr)
            + ((long)art.b[k] - b) * ((long)art.b[k] - b);
        if (d < best_d) {
            best_d = d;
            best = k;
        }
    }
    return best;
}

/* ---- Provinces. ---- */

static const RegionMap *map_of(int region, int part)
{
    int i;

    for (i = 0; i < MAPS; i++) {
        if (g_maps[i].region == region && g_maps[i].part == part) {
            return &g_maps[i];
        }
    }
    return 0;
}

int travelmap_region_origin(int region, int part, int *x, int *y, int *scale)
{
    const RegionMap *m;

    m = map_of(region, part);
    if (!m) {
        return 0;
    }
    *x = m->x;
    *y = m->y;
    *scale = region == BETONY ? BETONY_SCALE : 1;
    return 1;
}

/* How many screens a province's map is. */
static int parts_of(int region)
{
    int n;

    for (n = 0; map_of(region, n); n++) {
    }
    return n;
}

/* The province's name, by its ruling house. */
static const char *region_name(int region)
{
    const Faction *f;

    f = factionfile_open() ? faction_province_of(region) : 0;
    return f ? f->name : "";
}

/* A province's map and its places' names, read into the composer's heap; the last province's are let go with it. */
static int show_region(int region, int part)
{
    const RegionMap *m;
    MemHeap *was;

    m = map_of(region, part);
    if (!m) {
        return 0;
    }
    g.region_pic = composer_read(&g.composer, m->file);
    if (!g.region_pic) {
        log_print(LOG_WARN, "travel map: %s did not read", m->file);
        return 0;
    }
    if (region != g.region || !g.names) {
        was = mem_heap_use(g.composer.heap);
        if (!g.names) {
            g.names = (DunRef *)mem_alloc((df_u32)NAMES_MOST * (df_u32)sizeof(DunRef));
        }
        mem_heap_use(was);
        g.nnames = g.names ? exterior_list_region(region, g.names, NAMES_MOST) : 0;
        /* It counts what it could not hold too. */
        g.nnames = g.nnames > NAMES_MOST ? NAMES_MOST : g.nnames;
    }
    g.region = region;
    g.part = part;
    g.step = STEP_REGION;
    df_copy(g.line, region_name(region), LINE_TEXT);
    return 1;
}

/* A world pixel's place on the panel, and back: by the map's corner and scale. */
static int to_panel(int px, int py, int *x, int *y)
{
    const RegionMap *m;
    int scale;

    m = map_of(g.region, g.part);
    if (!m) {
        return 0;
    }
    scale = g.region == BETONY ? BETONY_SCALE : 1;
    *x = (px - m->x) * scale;
    *y = (py - m->y) * scale;
    return *x >= 0 && *x < PANEL_W && *y >= 0 && *y < PANEL_H;
}

/* The place a type is shown as, or not at all (one of the player's ships; one a filter hides). */
static int shows_type(int type)
{
    if (type < 0 || type >= 14) {
        return 0;
    }
    return !g.filter_off[g_filter_of_type[type]];
}

/* The known place nearest panel point (x, y), within two of the panel's pixels; -1 for none. */
static int place_near(int x, int y)
{
    int n;
    int k;
    int best;
    int best_d;

    n = worldmap_locations();
    best = -1;
    best_d = 3 * 3 * 2;
    for (k = 0; k < n; k++) {
        int px;
        int py;
        int region;
        int index;
        int type;
        int known;
        int sx;
        int sy;
        int d;

        if (!worldmap_location_info(k, &px, &py, &region, &index, &type, &known) || region != g.region
            || !atlas_known(k) || !shows_type(type) || !to_panel(px, py, &sx, &sy)) {
            continue;
        }
        d = (sx - x) * (sx - x) + (sy - y) * (sy - y);
        if (d < best_d) {
            best = k;
            best_d = d;
        }
    }
    return best;
}

/* A location's name, from its province's list. */
static const char *name_of(int k)
{
    int px;
    int py;
    int region;
    int index;
    int type;
    int known;

    if (!worldmap_location_info(k, &px, &py, &region, &index, &type, &known) || region != g.region
        || index < 0 || index >= g.nnames) {
        return "";
    }
    return g.names[index].name;
}

/* ---- Open and close. ---- */

int travelmap_open(Player *p, int px, int py, int region)
{
    int i;
    int k;

    travelmap_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.here_px = px;
    g.here_py = py;
    g.here_region = region;
    g.region = -1;
    g.place = -1;
    g.world = raw("TRAV0I00.IMG");
    g.mask = raw("TRAV0I01.IMG");
    g.frame = composer_read(&g.composer, "MBRD00I0.IMG");
    g.buttons = raw("TRAV0I03.IMG");
    g.filters_on = raw("TRAV01I0.IMG");
    g.filters_off = raw("TRAV01I1.IMG");
    g.arrows[0] = composer_read(&g.composer, "TRAVAI05.IMG");
    g.arrows[1] = composer_read(&g.composer, "TRAVBI05.IMG");
    g.arrows[2] = composer_read(&g.composer, "TRAVCI05.IMG");
    g.arrows[3] = composer_read(&g.composer, "TRAVDI05.IMG");
    g.popup = composer_read(&g.composer, "TRAV0I04.IMG");
    for (i = 0, k = 1; i < 4; i++) {
        k = k && g.arrows[i];
    }
    if (!g.world || !g.mask || !g.frame || !g.buttons || !g.filters_on || !g.filters_off || !k || !g.popup
        || !map_palettes() || !worldmap_open()) {
        log_text(LOG_WARN, "travel map: its pictures did not read");
        travelmap_close();
        return 0;
    }
    {
        MemHeap *was;

        was = mem_heap_use(g.composer.heap);
        g.under = (df_u8 *)mem_alloc(CANVAS_BYTES);
        mem_heap_use(was);
    }
    if (!g.under) {
        travelmap_close();
        return 0;
    }
    g.step = STEP_WORLD;
    g.ship = 1;
    g.words.pc = &p->sheet;
    g.changed = 1;
    return 1;
}

void travelmap_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

void travelmap_palette(unsigned char *rgb)
{
    int i;

    for (i = 0; i < 768; i++) {
        rgb[i] = g.rgb[i];
    }
}

/* ---- The journey. ---- */

static void plan(void)
{
    /* "Foot/horse": on the horse when one is owned, else the cart, else on foot (ride.h). */
    journey_plan(g.here_px, g.here_py, g.place_px, g.place_py, g.reckless, !g.camp, g.ship, ride_journey(g.player), 0, &g.journey);
    if (g.ship && bank_ship() != BANK_SHIP_NONE) {
        journey_own_ship(&g.journey);
    }
}

/* A place chosen: ask whether to go. */
static void choose(int k)
{
    int region;
    int index;
    int known;

    if (!worldmap_location_info(k, &g.place_px, &g.place_py, &region, &index, &g.place_type, &known)) {
        return;
    }
    g.place = k;
    df_copy(g.place_name, name_of(k), (int)sizeof(g.place_name));
    g.words.place = g.place_name;
    words_use(&g.words);
    words_say(RECORD_TRAVEL_TO, g.ask, (int)sizeof(g.ask));
    g.step = STEP_ASK;
}

/* The countdown's days, then the journey. */
static void begin(void)
{
    plan();
    if (g.journey.cost > (int)g.player->pack.gold) {
        textrsc_read(RECORD_NO_GOLD, g.line, LINE_TEXT);
        g.note = g.line;
        return;
    }
    g.days_left = g.journey.days;
    g.count_time = 0.0f;
    g.step = STEP_COUNTING;
}

int travelmap_step(float dt)
{
    if (g.step != STEP_COUNTING) {
        return TRAVELMAP_STAY;
    }
    g.count_time += dt;
    while (g.count_time >= COUNT_DAY && g.days_left > 0) {
        g.count_time -= COUNT_DAY;
        g.days_left--;
        g.changed = 1;
    }
    return g.days_left > 0 ? TRAVELMAP_STAY : TRAVELMAP_GO;
}

void travelmap_chosen(int *region, int *index, Journey *journey, int *cautious)
{
    int px;
    int py;
    int type;
    int known;

    *region = -1;
    *index = -1;
    if (g.place >= 0) {
        worldmap_location_info(g.place, &px, &py, region, index, &type, &known);
    }
    plan();
    *journey = g.journey;
    *cautious = !g.reckless;
}

/* ---- Composing. ---- */

/* The bottom bar of the province view, from the world's picture, with its buttons and filters. */
static void compose_bar(void)
{
    int i;

    canvas_copy_part(g.world, CANVAS_W, 0, BAR_Y, CANVAS_W, CANVAS_H - BAR_Y, 0, BAR_Y);
    canvas_copy_part(g.buttons, BUTTON_SHEET_W, 0, 0, BUTTON_SHEET_W, 11, g_find_rect.x, g_find_rect.y);
    canvas_copy_part(g.buttons, BUTTON_SHEET_W, 0, 11, BUTTON_SHEET_W, 11, g_at_rect.x, g_at_rect.y);
    for (i = 0; i < FILTERS; i++) {
        canvas_copy_part(g.filter_off[i] ? g.filters_off : g.filters_on, FILTER_SHEET_W, g_filter_part[i].x,
                         g_filter_part[i].y, g_filter_part[i].w, g_filter_part[i].h, g_filter_at[i].x, g_filter_at[i].y);
    }
    /* A map of several screens has its arrows: right on the left half, left on the right; down on the top row, up on the bottom. */
    if (parts_of(g.region) > 1) {
        canvas_copy_part(g.arrows[g.part & 1 ? 3 : 2], ARROW_W, 0, 0, ARROW_W, ARROW_H, g_side_rect.x, g_side_rect.y);
    }
    if (parts_of(g.region) > 2) {
        canvas_copy_part(g.arrows[g.part > 1 ? 1 : 0], ARROW_W, 0, 0, ARROW_W, ARROW_H, g_updown_rect.x, g_updown_rect.y);
    }
}

/* The known places of the province as dots, by type, and the cross where the player is. */
static void compose_places(void)
{
    int n;
    int k;
    int x;
    int y;

    n = worldmap_locations();
    for (k = 0; k < n; k++) {
        int px;
        int py;
        int region;
        int index;
        int type;
        int known;

        if (!worldmap_location_info(k, &px, &py, &region, &index, &type, &known) || region != g.region
            || !atlas_known(k) || !shows_type(type) || type == WORLDMAP_HOME_YOUR_SHIPS || !to_panel(px, py, &x, &y)) {
            continue;
        }
        g.composer.screen[(PANEL_Y + y) * CANVAS_W + PANEL_X + x] = g_dot_of_type[type];
    }
    if (to_panel(g.here_px, g.here_py, &x, &y)) {
        int i;

        for (i = 0; i < PANEL_W; i++) {
            if (i < x - 2 || i > x + 2) {
                g.composer.screen[(PANEL_Y + y) * CANVAS_W + PANEL_X + i] = CROSS_INDEX;
            }
        }
        for (i = 0; i < PANEL_H; i++) {
            if (i < y - 2 || i > y + 2) {
                g.composer.screen[(PANEL_Y + i) * CANVAS_W + PANEL_X + x] = CROSS_INDEX;
            }
        }
    }
}

/* A tick on a chosen button of the popup, in the art palette before the popup is mapped. */
static void tick(const Rect *r, int index)
{
    Rect mark;

    mark.x = (df_u16)(r->x + 2);
    mark.y = (df_u16)(r->y + 2);
    mark.w = MARK_W;
    mark.h = MARK_W;
    canvas_box(&mark, index);
}

static void compose_popup(void)
{
    static const Rect whole = { POPUP_X, POPUP_Y, POPUP_W, POPUP_H };
    char text[24];
    int mark;

    plan();
    canvas_copy_part(g.popup, POPUP_W, 0, 0, POPUP_W, POPUP_H, POPUP_X, POPUP_Y);
    mark = art_index_of(MARK_RGB_R, MARK_RGB_G, MARK_RGB_B);
    tick(g.reckless ? &g_reckless_rect : &g_cautious_rect, mark);
    tick(g.ship ? &g_ship_rect : &g_foot_rect, mark);
    tick(g.camp ? &g_camp_rect : &g_inn_rect, mark);
    canvas_set_ink(CANVAS_INK, CANVAS_INK_SHADOW);
    log_format(text, (int)sizeof(text), "%u", (unsigned)g.player->pack.gold);
    canvas_label(148, 97, text);
    log_format(text, (int)sizeof(text), "%d", g.journey.cost);
    canvas_label(117, 107, text);
    log_format(text, (int)sizeof(text), "%d", g.step == STEP_COUNTING ? g.days_left : g.journey.days);
    canvas_label(129, 117, text);
    art_rect_to_map(&whole);
}

static void compose_ask(void)
{
    Rect inside;

    canvas_scroll(&g_ask_rect);
    inside.x = (df_u16)(g_ask_rect.x + 10);
    inside.y = (df_u16)(g_ask_rect.y + 10);
    inside.w = (df_u16)(g_ask_rect.w - 20);
    inside.h = 40;
    canvas_set_ink(CANVAS_INK, CANVAS_INK_SHADOW);
    canvas_text(&inside, g.ask);
    canvas_button(CANVAS_BUTTON_YES, g_yes_rect.x, g_yes_rect.y);
    canvas_button(CANVAS_BUTTON_NO, g_no_rect.x, g_no_rect.y);
    /* The scroll's burnt edges let the map through: what shows was the map's already, and stays. */
    {
        int y;
        int x;

        for (y = g_ask_rect.y; y < g_ask_rect.y + g_ask_rect.h + CANVAS_BUTTON_H; y++) {
            for (x = g_ask_rect.x; x < g_ask_rect.x + g_ask_rect.w + 22; x++) {
                df_u8 *at;

                if (y >= CANVAS_H || x >= CANVAS_W) {
                    continue;
                }
                at = &g.composer.screen[y * CANVAS_W + x];
                *at = g.under[y * CANVAS_W + x] == *at ? *at : g.art_to_map[*at];
            }
        }
    }
}

void travelmap_compose(void)
{
    canvas_use(&g.composer);
    if (g.step == STEP_WORLD) {
        canvas_copy_all(g.world);
    } else {
        canvas_box(&g_line_rect, 0);
        {
            static const Rect top = { 0, 0, 320, 12 };

            canvas_box(&top, 0);
        }
        canvas_copy_part(g.region_pic, PANEL_W, 0, 0, PANEL_W, PANEL_H, PANEL_X, PANEL_Y);
        compose_places();
        canvas_cutout(g.frame, CANVAS_W, FRAME_H, 0, FRAME_Y);
        compose_bar();
    }
    canvas_set_ink(g.ink, g.shadow);
    if (g.step == STEP_FIND) {
        char text[LINE_TEXT];

        log_format(text, (int)sizeof(text), "Find: %s_", g.typed);
        canvas_label_centred(&g_line_rect, 0, text);
    } else {
        canvas_label_centred(&g_line_rect, 0, g.note ? g.note : g.line);
    }
    /* What follows draws in the art palette: kept as it is, to tell what it changed. */
    mem_copy(g.under, g.composer.screen, CANVAS_BYTES);
    if (g.step == STEP_ASK) {
        compose_ask();
    } else if (g.step == STEP_POPUP || g.step == STEP_COUNTING) {
        compose_popup();
    }
    canvas_set_ink(CANVAS_INK, CANVAS_INK_SHADOW);
    g.changed = 0;
}

const unsigned char *travelmap_pixels(void)
{
    return g.composer.screen;
}

int travelmap_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/* ---- The pointer, clicks and keys. ---- */

/* The province under a point of the world's picture: its mask's value less 128; -1 for none, or one with no map. */
static int region_at(int x, int y)
{
    int v;

    if (x < 0 || y < 0 || x >= CANVAS_W || y >= CANVAS_H) {
        return -1;
    }
    v = (int)g.mask[y * CANVAS_W + x] - 128;
    return v >= 0 && map_of(v, 0) ? v : -1;
}

void travelmap_point(int x, int y)
{
    char was[LINE_TEXT];

    df_copy(was, g.line, LINE_TEXT);
    if (g.step == STEP_WORLD) {
        int r;

        r = region_at(x, y);
        df_copy(g.line, r >= 0 ? region_name(r) : "", LINE_TEXT);
    } else if (g.step == STEP_REGION) {
        int k;

        k = y >= PANEL_Y && y < PANEL_Y + PANEL_H ? place_near(x - PANEL_X, y - PANEL_Y) : -1;
        df_copy(g.line, k >= 0 ? name_of(k) : region_name(g.region), LINE_TEXT);
    }
    if (!df_eq(was, g.line)) {
        g.changed = 1;
    }
}

/* The typed name found among the known places, the first whose name begins with it; on to its province. */
static void find_typed(void)
{
    MemHeap *scratch;
    MemHeap *was;
    DunRef *rows;
    int n;
    int i;
    int found;

    found = -1;
    scratch = mem_heap_new();
    if (!scratch) {
        return;
    }
    was = mem_heap_use(scratch);
    rows = (DunRef *)mem_alloc(16000u * (df_u32)sizeof(DunRef));
    n = rows ? exterior_list(rows, 16000) : 0;
    for (i = 0; i < n && found < 0 && g.typed[0]; i++) {
        int k;
        int px;
        int py;

        k = 0;
        while (g.typed[k] && rows[i].name[k] && ((rows[i].name[k] | 32) == (g.typed[k] | 32))) {
            k++;
        }
        if (g.typed[k] || !worldmap_pixel_of(rows[i].region, rows[i].index, &px, &py)) {
            continue;
        }
        k = worldmap_location_id(px, py);
        if (k >= 0 && atlas_known(k)) {
            found = k;
        }
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
    if (found < 0) {
        textrsc_read(RECORD_NO_PLACE, g.line, LINE_TEXT);
        g.note = g.line;
        g.step = g.region >= 0 ? STEP_REGION : STEP_WORLD;
        return;
    }
    {
        int px;
        int py;
        int region;
        int index;
        int type;
        int known;
        int part;

        worldmap_location_info(found, &px, &py, &region, &index, &type, &known);
        /* The screen of the province's map that has it. */
        for (part = 0; map_of(region, part); part++) {
            int x;
            int y;

            g.region = region;
            g.part = part;
            if (to_panel(px, py, &x, &y)) {
                break;
            }
        }
        if (!show_region(region, map_of(region, part) ? part : 0)) {
            g.step = STEP_WORLD;
            return;
        }
        choose(found);
    }
}

static int click_region(int x, int y)
{
    int i;

    if (rect_has(&g_exit_rect, x, y)) {
        g.step = STEP_WORLD;
        g.note = 0;
        return TRAVELMAP_STAY;
    }
    if (rect_has(&g_find_rect, x, y)) {
        g.typed[0] = 0;
        g.step = STEP_FIND;
        return TRAVELMAP_STAY;
    }
    if (rect_has(&g_at_rect, x, y)) {
        /* Back to the screen of the player's province, if they stand in one with a map: the cross marks them. */
        int r;
        int part;

        r = g.here_region;
        for (part = 0; r >= 0 && map_of(r, part); part++) {
            int px;
            int py;

            g.region = r;
            g.part = part;
            if (to_panel(g.here_px, g.here_py, &px, &py)) {
                show_region(r, part);
                return TRAVELMAP_STAY;
            }
        }
        return TRAVELMAP_STAY;
    }
    for (i = 0; i < FILTERS; i++) {
        if (rect_has(&g_filter_at[i], x, y)) {
            g.filter_off[i] = !g.filter_off[i];
            return TRAVELMAP_STAY;
        }
    }
    if (parts_of(g.region) > 1 && rect_has(&g_side_rect, x, y)) {
        show_region(g.region, g.part ^ 1);
        return TRAVELMAP_STAY;
    }
    if (parts_of(g.region) > 2 && rect_has(&g_updown_rect, x, y)) {
        show_region(g.region, g.part > 1 ? g.part - 2 : g.part + 2);
        return TRAVELMAP_STAY;
    }
    if (y >= PANEL_Y && y < PANEL_Y + PANEL_H) {
        int k;

        k = place_near(x - PANEL_X, y - PANEL_Y);
        if (k >= 0) {
            choose(k);
        }
    }
    return TRAVELMAP_STAY;
}

int travelmap_click(int x, int y)
{
    g.changed = 1;
    g.note = 0;
    switch (g.step) {
    case STEP_WORLD:
        if (rect_has(&g_exit_rect, x, y)) {
            return TRAVELMAP_LEAVE;
        }
        {
            int r;

            r = region_at(x, y);
            if (r >= 0) {
                show_region(r, 0);
            }
        }
        return TRAVELMAP_STAY;
    case STEP_REGION:
        return click_region(x, y);
    case STEP_ASK:
        if (rect_has(&g_yes_rect, x, y)) {
            g.step = STEP_POPUP;
        } else if (rect_has(&g_no_rect, x, y)) {
            g.step = STEP_REGION;
            g.place = -1;
        }
        return TRAVELMAP_STAY;
    case STEP_POPUP:
        if (rect_has(&g_cautious_rect, x, y)) {
            g.reckless = 0;
        } else if (rect_has(&g_reckless_rect, x, y)) {
            g.reckless = 1;
        } else if (rect_has(&g_foot_rect, x, y)) {
            g.ship = 0;
        } else if (rect_has(&g_ship_rect, x, y)) {
            g.ship = 1;
        } else if (rect_has(&g_inn_rect, x, y)) {
            g.camp = 0;
        } else if (rect_has(&g_camp_rect, x, y)) {
            g.camp = 1;
        } else if (rect_has(&g_begin_rect, x, y)) {
            begin();
        } else if (rect_has(&g_popup_exit_rect, x, y)) {
            g.step = STEP_REGION;
            g.place = -1;
        }
        return TRAVELMAP_STAY;
    default:
        return TRAVELMAP_STAY;
    }
}

int travelmap_key(int vk)
{
    int n;

    g.changed = 1;
    if (g.step == STEP_FIND) {
        for (n = 0; g.typed[n]; n++) {
        }
        if (vk == VK_RETURN_KEY) {
            find_typed();
        } else if (vk == VK_ESCAPE_KEY) {
            g.step = g.region >= 0 ? STEP_REGION : STEP_WORLD;
        } else if (vk == VK_BACK_KEY) {
            if (n > 0) {
                g.typed[n - 1] = 0;
            }
        } else if (((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') || vk == VK_SPACE_KEY) && n < 32) {
            g.typed[n] = (char)vk;
            g.typed[n + 1] = 0;
        }
        return TRAVELMAP_STAY;
    }
    if (vk != VK_ESCAPE_KEY) {
        return TRAVELMAP_STAY;
    }
    /* Escape steps back: the popup or the question to the province, the province to the world, the world away. */
    if (g.step == STEP_COUNTING) {
        return TRAVELMAP_STAY;
    }
    if (g.step == STEP_POPUP || g.step == STEP_ASK) {
        g.step = STEP_REGION;
        g.place = -1;
        return TRAVELMAP_STAY;
    }
    if (g.step == STEP_REGION) {
        g.step = STEP_WORLD;
        return TRAVELMAP_STAY;
    }
    return TRAVELMAP_LEAVE;
}

int travelmap_region(void)
{
    return g.step == STEP_WORLD ? -1 : g.region;
}

int travelmap_place(void)
{
    return g.place;
}

const char *travelmap_line(void)
{
    return g.note ? g.note : g.line;
}
