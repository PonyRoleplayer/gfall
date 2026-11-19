#include "game/dagger/play/state.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/input/activate.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/items/ground.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/places/entbuild.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/ui/movie.h"
#include "game/dagger/play/people/guild.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"
#include "engine/scene/stream.h"

#include "game/dagger/formats/factionfile.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/play/ui/words.h"
#include "game/dagger/places/town.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/play/travel/travelmap.h"

#include "game/dagger/play/character/curse.h"

#include "hooks.h"
static int quest_text(const char *quest, int message, int in_text_rsc, char *out, int cap)
{
    char file[16];

    if (in_text_rsc) { return textrsc_read(message, out, cap); }
    log_format(file, sizeof(file), "%s.QRC", quest);
    return textrsc_read_in(file, message, out, cap);
}

static void quest_say(const char *text) { possessions_message(text, 0); }

/* A prince's scene: his animation and the words, for the offer of his quest (quest_offer_prompt). */
static int quest_scene(char code, const char *words, int ask, void (*answer)(int answer))
{
    const char *file = prince_flc(code);

    return file ? movie_scene(file, words, ask, answer) : 0;
}

/* The TEXT.RSC macros of a quest's text: the player's name and race, the town and the region. */
static int quest_macros(const char *in, char *out, int cap)
{
    static WordsPlayer pc;
    Words w;
    int i;
    char province[32];
    const Faction *faction;

    for (i = 0; i < (int)sizeof(pc.name) - 1 && character_state()->name[i]; i++) { pc.name[i] = character_state()->name[i]; }
    pc.name[i] = 0;
    pc.race = character_state()->race; pc.female = character_state()->female;
    faction = faction_province_of(travel_region());
    df_copy(province, faction ? faction->name : "", sizeof(province));
    mem_zero(&w, sizeof(w));
    w.pc = &pc; w.city = town_name(); w.region = province;
    words_use(&w);
    words_expand(in, out, cap);
    for (i = 0; out[i]; i++) { }
    return i;
}

/* What the quests ask of the map: the travel list's locations, where the player stands, a region's name, the journey the quests estimate (ships' sea pixels cost 51) and the map's discovery. */
/* The index of the outside the player is in or under, in its region's list (the travel list's index): a dungeon's is the outside it lies under. -1 when in no location. */
int gfall_quest_outside(void)
{
    const PlaceWhere *where;
    const Dungeon *data;
    int index;

    where = &world_where()->where;
    if (where->kind == WHERE_DUNGEON) {
        data = travel_place_data();
        index = data ? dungeon_outside(where->region, data->exterior_id) : -1;
        return index;
    }
    return where->located ? where->index : -1;
}

/* Every dungeon record of the map, read once: a number's dungeon and a vampire's crypts search it. */
#define DUNGEON_REFS 4400
static DunRef g_dungeon_refs[DUNGEON_REFS];

/* The curses (docs/curses.md 4.5, curses-cures-and-dreams.md 6): a new vampire wakes in a crypt of the region. The original counts the region's crypts (dungeon type 0), draws n below that count and
 * loads the region's n-th dungeon of ANY type (location_load_nth_dungeon_of_type's bug): kept. The player stands on the dungeon's start marker; a region with no crypt leaves them where they are. */
static int curse_haunt(int region)
{
    static Dungeon dungeon;
    int n, i, count, pick;

    n = dungeon_list(g_dungeon_refs, DUNGEON_REFS);
    if (n > DUNGEON_REFS) { n = DUNGEON_REFS; }
    count = 0;
    for (i = 0; i < n; i++) {
        if (g_dungeon_refs[i].region == region && dungeon_load_at(region, g_dungeon_refs[i].index, &dungeon) && dungeon.dungeon_type == DUN_TYPE_CRYPT) { count++; }
    }
    if (!count) { return 0; }
    pick = random_range(0, count - 1);
    for (i = 0; i < n; i++) {
        if (g_dungeon_refs[i].region != region) { continue; }
        if (pick-- == 0) { return travel_go_into_dungeon(region, g_dungeon_refs[i].index); }
    }
    return 0;
}
static int curse_clan(int region)
{
    const Faction *province;

    province = faction_province_of(region);
    return province ? province->vampire : 0;
}
static int curse_region(void) { return travel_region(); }

static void quest_here(int *region, int *index, int *px, int *py)
{
    travel_capture();
    *region = travel_region();
    *index = gfall_quest_outside();
    *px = world_where()->px; *py = world_where()->py;
}

/* A location's number (MAPS' LOC_ID, 1 for an outside and its dungeon the next): the region and the outside's index in it, found by reading each region's two lists once and kept. */
static int quest_find_number(int number, int *region, int *index)
{
    static struct { int number, region, index; } cache[16];
    static int cached;
    int n, i, k;

    for (k = 0; k < cached; k++) { if (cache[k].number == number) { *region = cache[k].region; *index = cache[k].index; return cache[k].index >= 0; } }
    exterior_find_number(number, region, index);
    if (*index < 0) {
        /* A dungeon's number: its record in the dungeon list names the outside it lies under. */
        n = dungeon_list(g_dungeon_refs, DUNGEON_REFS);
        if (n > DUNGEON_REFS) { n = DUNGEON_REFS; }
        for (i = 0; i < n && *index < 0; i++) {
            if (g_dungeon_refs[i].location_id == number) {
                k = dungeon_outside(g_dungeon_refs[i].region, g_dungeon_refs[i].exterior_id);
                if (k >= 0) { *region = g_dungeon_refs[i].region; *index = k; }
            }
        }
    }
    if (cached < 16) { cache[cached].number = number; cache[cached].region = *region; cache[cached].index = *index; cached++; }
    return *index >= 0;
}

static int quest_region_name(int region, char *out, int cap)
{
    const Faction *faction;

    faction = faction_province_of(region);
    df_copy(out, faction ? faction->name : "", cap);
    return faction ? 1 : 0;
}

/* quest_place_object: a quest's creature among the dungeon's actors, unseen and near the player. */
static int quest_spawn_foe(int kind, df_u32 thing)
{
    Camera cam;

    move_camera(&cam);
    return encounter_place(&cam, kind, thing);
}

/* A pile at the player's feet for a quest's item: the container the drop screen would make. */
static df_u32 quest_place_item(void)
{
    return ground_container(game_state()->x,
        world_where()->where.kind == WHERE_WORLD ? move_body()->feet[1] : game_state()->eye_y - 32,
        game_state()->z);
}

static int quest_journey(int sx, int sy, int ex, int ey)
{
    JourneyPlan journey;

    journey_plan(sx, sy, ex, ey, 0, 0, 1, journey_transport(), &journey);
    return journey.minutes;
}

/* The place a logbook entry is written in: the town, else the region. */
static const char *quest_place(void)
{
    static char place[40];
    const char *town;
    const Faction *faction;

    town = town_name();
    if (town && town[0]) { df_copy(place, town, sizeof(place)); return place; }
    faction = faction_province_of(travel_region());
    df_copy(place, faction ? faction->name : "", sizeof(place));
    return place;
}

/* A quest marker of the dungeon the player is in (dunstream's), a quest's creature on the floor beside it, and a pile there for its item. */
static int quest_marker_at(int marker, int record, unsigned seed, float *x, float *y, float *z)
{
    /* GFALL loads its dungeons whole (a streamed one is the viewers'): the world's markers; a streamed dungeon's are dunstream's. */
    if (dunstream_active()) { return dunstream_quest_marker(marker, record, seed, x, y, z); }
    return travel_whole_world() && world_quest_marker(travel_whole_world(), marker, record, seed, x, y, z);
}

static int quest_spawn_foe_at(int kind, df_u32 thing, float x, float y, float z)
{
    Camera cam;

    move_camera(&cam);
    return encounter_place_at(&cam, kind, thing, x, y, z);
}

static df_u32 quest_place_item_at(float x, float y, float z)
{
    return ground_container(x, y, z);
}

/* A building of a town for a quest's place: of the type asked (17..20 any house, below 0 any) picked by the seed. */
static int quest_pick_building(int region, int index, int type, unsigned seed, int *block, int *record)
{
    const TownBuilding *list;
    int count, i, n, pick;

    list = town_buildings(region, index, &count);
    if (!list) { return 0; }
    n = 0;
    for (i = 0; i < count; i++) {
        if (type < 0 ? (list[i].type < 17 && list[i].type != 1) : (list[i].type == type || (type >= 17 && type <= 20 && list[i].type >= 17 && list[i].type <= 20))) { n++; }
    }
    if (!n) { return 0; }
    pick = (int)(seed % (unsigned)n);
    for (i = 0; i < count; i++) {
        if (type < 0 ? (list[i].type < 17 && list[i].type != 1) : (list[i].type == type || (type >= 17 && type <= 20 && list[i].type >= 17 && list[i].type <= 20))) {
            if (pick-- == 0) { *block = list[i].block; *record = list[i].record; return 1; }
        }
    }
    return 0;
}

static int quest_outdoors(void) { return world_where()->where.kind == WHERE_WORLD; }

/* A building of a town for a person's site: of a type, any shop-like one, of a faction, or of a faction of a type. */
static int site_matches(const TownBuilding *b, int mode, int value)
{
    const FactionDef *f;

    if (mode == 0) { return value == 11 ? (b->type == 11 && b->faction == 40) : (b->type == value || (value >= 17 && value <= 20 && b->type >= 17 && b->type <= 20)); }
    if (mode == 1) { return b->type < 17; }
    if (mode == 2) { return b->faction == value; }
    f = b->faction ? faction_def(faction_index(b->faction)) : 0;
    return f && f->type == value;
}

static int quest_pick_site(int region, int index, int mode, int value, unsigned seed, int *block, int *record, int *faction)
{
    const TownBuilding *list;
    int count, i, n, pick;

    list = town_buildings(region, index, &count);
    if (!list) { return 0; }
    n = 0;
    for (i = 0; i < count; i++) { if (site_matches(&list[i], mode, value)) { n++; } }
    if (!n) { return 0; }
    pick = (int)(seed % (unsigned)n);
    for (i = 0; i < count; i++) {
        if (site_matches(&list[i], mode, value) && pick-- == 0) { *block = list[i].block; *record = list[i].record; *faction = list[i].faction; return 1; }
    }
    return 0;
}

/* A quest person's name, as any person of the town is named: by their id, sex and faction. */
static void quest_person_name(int who, int faction, int female, char *out, int cap)
{
    NpcName npc;
    const FactionDef *f;

    mem_zero(&npc, sizeof(npc));
    f = faction ? faction_def(faction_index(faction)) : 0;
    npc.type = 8; npc.region = travel_region(); npc.id = (unsigned)who; npc.female = female;
    npc.faction = faction; npc.faction_is_individual = f && f->type == 4; npc.faction_name = f ? f->name : 0;
    npc_name(&npc, out, cap);
}

/* The engine's hooks for who stands in a building (entbuild_set_hooks): the quest persons put in it (quest_people_in), found for the building once, are told apart from the building's own people.
 * A person of a building type takes over one of its people (the original's NPC site: the engine's change hook is asked of each in turn, and the person picked by the quest's seed becomes the quest's: their
 * picture and tag, standing where they stood); the others, and any takeover with no one to take, are added, standing on a quest marker (199.11) of the building or beside its first person. */
static QuestHere g_here[8];
static int g_here_count, g_here_done[8];
static int g_here_key[4] = { -1, -1, -1, -1 };

static void quest_people_for(const EntBuild *b)
{
    /* A new building: its quest people, none taken over yet. */
    g_here_key[0] = b->region; g_here_key[1] = b->index; g_here_key[2] = b->block; g_here_key[3] = b->record;
    g_here_count = quest_people_in(b->region, b->index, b->block, b->record, g_here, 8);
    mem_zero(g_here_done, sizeof(g_here_done));
}

static int quest_people_change(const EntBuild *b, Entity *e)
{
    int i;

    if (b->nth == 0) { quest_people_for(b); }
    for (i = 0; i < g_here_count; i++) {
        const Faction *f;
        int slot, w, h, file, image;

        if (!g_here[i].takeover || g_here_done[i] || b->count < 1 || (int)(g_here[i].seed % (unsigned)b->count) != b->nth) { continue; }
        f = factionfile_open() && g_here[i].faction ? faction_find(g_here[i].faction) : 0;
        if (!f || !f->flat_archive[g_here[i].female]) { f = faction_people_of(b->region); }
        file = f ? f->flat_archive[g_here[i].female] : 182;
        image = f ? f->flat_record[g_here[i].female] : 0;
        if (!entbuild_slot(file, image, &slot, &w, &h)) { continue; }
        /* The NPC site (8.5 of quest-places-and-start.md): the picture is the quest person's, the faction kept, the tag theirs, the place the NPC's own. */
        e->pose[ENTITY_IDLE].slot[0] = (df_u16)slot; e->pose[ENTITY_IDLE].w[0] = (df_u16)w; e->pose[ENTITY_IDLE].h[0] = (df_u16)h;
        e->owner = (df_u32)QUEST_PERSON_TAG(g_here[i].slot, g_here[i].person);
        e->use.what = (int)e->owner & 0x7fff;
        e->use.tag = (file << 7) + image;
        g_here_done[i] = 1;
        break;
    }
    return 1;
}

/* A person put in the dungeon the player is in stands on their quest marker when the marker is in this block (a dungeon's block is stream chunk (b->cx, b->cz); the point is in the stream's frame, which
 * the chunk's origin is taken off): a body made by the engine's dungeon hook, a person to talk to. */
static int quest_dungeon_people(const EntBuild *b, Entity *out, int cap)
{
    QuestHere here[8];
    int i, n, made, ox, oz;

    n = quest_people_in(b->region, gfall_quest_outside(), -2, -1, here, 8);
    ox = oz = 0;
    if (b->block >= 0) { stream_origin(&ox, &oz); }
    made = 0;
    for (i = 0; i < n && made < cap; i++) {
        const Faction *f;
        float lx, lz;
        int file, image;

        if (!here[i].has_point) { continue; }
        /* A whole dungeon (GFALL's): the point is in the world's frame; a streamed block: in the chunk's, its origin taken off, and only the block that holds it makes them. */
        lx = here[i].x; lz = here[i].z;
        if (b->block >= 0) {
            lx -= (float)(b->cx - ox) * DUNSTREAM_BLOCK; lz -= (float)(b->cz - oz) * DUNSTREAM_BLOCK;
            if (lx < 0.0f || lx >= DUNSTREAM_BLOCK || lz < 0.0f || lz >= DUNSTREAM_BLOCK) { continue; }
        }
        f = factionfile_open() && here[i].faction ? faction_find(here[i].faction) : 0;
        if (!f || !f->flat_archive[here[i].female]) { f = faction_people_of(b->region); }
        file = f ? f->flat_archive[here[i].female] : 182;
        image = f ? f->flat_record[here[i].female] : 0;
        if (entbuild_person(&out[made], file, image, QUEST_PERSON_TAG(here[i].slot, here[i].person), lx, here[i].y, lz)) { made++; }
    }
    return made;
}

static int quest_people_add(const EntBuild *b, Entity *out, int cap)
{
    int i, made;
    float mx, my, mz;

    if (b->place == PLACE_DUNGEON) { return quest_dungeon_people(b, out, cap); }

    if (g_here_key[0] != b->region || g_here_key[1] != b->index || g_here_key[2] != b->block || g_here_key[3] != b->record || b->count == 0) { quest_people_for(b); }
    made = 0;
    for (i = 0; i < g_here_count && made < cap; i++) {
        const Faction *f;
        int file, image;
        float x, y, z;

        if (g_here_done[i]) { continue; }
        f = factionfile_open() && g_here[i].faction ? faction_find(g_here[i].faction) : 0;
        if (!f || !f->flat_archive[g_here[i].female]) { f = faction_people_of(b->region); }
        file = f ? f->flat_archive[g_here[i].female] : 182;
        image = f ? f->flat_record[g_here[i].female] : 0;
        if (entbuild_marker_nth(11, made, &mx, &my, &mz)) { x = mx; y = my; z = mz; }
        else if (b->has_first) { x = b->first_x + 48.0f * (float)(made + 1); y = b->first_y; z = b->first_z; }
        else { continue; }
        if (entbuild_person(&out[made], file, image, QUEST_PERSON_TAG(g_here[i].slot, g_here[i].person), x, y, z)) { made++; }
    }
    return made;
}

void gfall_quests_install_text(void)
{
    static const QuestWorld world = {
        travel_count, travel_location, quest_here, quest_region_name, quest_journey, travel_discover,
        quest_spawn_foe, teleport, quest_place_item, quest_find_number, quest_marker_at,
        quest_spawn_foe_at, quest_place_item_at, quest_pick_building, quest_pick_site, quest_person_name, quest_outdoors
    };
    static const CurseWorld curse = { curse_haunt, curse_clan, curse_region };

    quest_set_text(quest_text);
    quest_set_say(quest_say);
    quest_set_ask(possessions_ask);
    quest_set_scene(quest_scene);
    quest_set_macros(quest_macros);
    quest_set_place(quest_place);
    quest_set_world(&world);
    curse_set_world(&curse);
}

void gfall_quests_install_entities(void)
{
    activate_set_global(OTHER_GLOBAL, quest_global_set);
    possessions_set_letter(quest_item_text);
    entbuild_set_hooks(quest_people_change, quest_people_add);
}
