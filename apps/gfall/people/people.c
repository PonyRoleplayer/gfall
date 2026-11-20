#include "game/dagger/play/state.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/ui/hud.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"
#include "engine/scene/interact.h"

#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/people.h"
#include "game/dagger/play/people/shelf.h"
#include "game/dagger/play/ui/words.h"
#include "game/dagger/tables/datedefs.h"
#include "game/dagger/places/town.h"
#include "game/dagger/tables/holidaydefs.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/people/talk.h"
#include "game/dagger/play/people/steal.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/people/politics.h"
#include "game/dagger/play/people/hours.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/formats/interior.h"
#include "game/dagger/play/people/talkwin.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/play/travel/travelmap.h"

#include "game/dagger/play/play.h"

#include "people.h"
#include "callbacks.h"
#include "interaction.h"
#include "../buildings/buildings.h"
#include "../quests/hooks.h"
/*
 * The faction table, from FACTION.TXT as the dagger reader parses it, into
 * standing.c's plain records (people and law, slice 1). The tree and the allies
 * and enemies keep the file's ids; the file counts regions from 1 and the
 * reader from 0, as the decomp's regions run.
 */
static FactionDef g_faction_def[FACTIONS_MAX];

int gfall_people_load_factions(void)
{
    int i, k, n;

    if (!factionfile_open()) {
        return 0;
    }
    n = faction_count();
    if (n > FACTIONS_MAX) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        const Faction *f;
        FactionDef *d;

        f = faction_at(i);
        d = &g_faction_def[i];
        mem_zero(d, sizeof(*d));
        d->id = f->id;
        d->parent = f->parent;
        d->type = f->type;
        d->region = f->region;
        d->rep = f->rep;
        d->power = f->power;
        d->flags = f->flags;
        d->social_group = f->sgroup;
        d->guild_group = f->ggroup;
        for (k = 0; k < FACTION_LINKS && k < FACTION_LINKS; k++) {
            d->ally[k] = f->ally[k];
            d->enemy[k] = f->enemy[k];
        }
        for (k = 0; k < FACTION_NAME - 1 && f->name[k]; k++) {
            d->name[k] = f->name[k];
        }
    }
    return factions_define(g_faction_def, n);
}

/*
 * A shop's shelves (people and law, slice 2): a shelf used opens the inventory as a shop over that
 * shelf's own stock. The stock is made the first time a shelf is used in a visit and gone with the
 * visit (building_load_interior clears and restocks at each entry); a visit is one level and one
 * building. The building's type and quality come from the town's blocks (travel_building), the
 * prices from the region's standing, the holiday from the clock.
 */
#define SHELVES_MOST 64
static df_u32 g_shelf_box[SHELVES_MOST];
static const Level *g_shelf_level;
static unsigned g_shelf_place;

static void forget_shelves(void)
{
    int i;

    for (i = 0; i < SHELVES_MOST; i++) {
        if (g_shelf_box[i]) {
            shelf_discard(g_shelf_box[i]);
            g_shelf_box[i] = 0;
        }
    }
}

int gfall_offer_say(int text, int price, char *out, int cap)
{
    Words w;

    mem_zero(&w, sizeof(w));
    w.amount = price;
    w.city = town_name();
    w.place = "This shop";
    words_use(&w);
    return words_say(text, out, cap);
}

int gfall_daedra_say(int text, int price, int prince, char *out, int cap)
{
    Words w;
    const Faction *daedra;
    char date[48];

    mem_zero(&w, sizeof(w));
    w.amount = price;
    w.city = town_name();
    w.place = "This shop";
    daedra = faction_find(prince);
    w.daedra = daedra ? daedra->name : "";
    date_text(game_state()->minutes, date, sizeof(date));
    w.date = date;
    words_use(&w);
    return words_say(text, out, cap);
}

/*
 * The talk window's world (people and law, slice 6): the town's buildings as the player stands in it, their signs,
 * which way they lie, and the words with their macros said. The window itself is play/people/talkwin.c; this is only
 * what it asks the game for. A building is named for the sign over it (names.c) and found again by an id made of
 * its block and record.
 */
static Camera g_talk_cam;
static int g_talk_region, g_talk_index;
static float g_talk_x, g_talk_z;
static int g_talk_in_town;
static char g_talk_region_name[32];
static Words g_talk_words;

static const TownBuilding *talk_building(unsigned id)
{
    int count, i;
    const TownBuilding *rows;

    if (!g_talk_in_town) {
        return 0;
    }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    for (i = 0; i < count; i++) {
        if (gfall_building_id(&rows[i]) == id) {
            return &rows[i];
        }
    }
    return 0;
}

/* xn_math_approx_dist2d is not in the tree: the longer side and half the shorter. */
static int talk_distance_to(const TownBuilding *t)
{
    float dx, dz;
    int a, b;

    dx = t->x - g_talk_x;
    dz = t->z - g_talk_z;
    a = (int)(dx < 0 ? -dx : dx);
    b = (int)(dz < 0 ? -dz : dz);
    return a > b ? a + b / 2 : b + a / 2;
}

#if TALK_TOPICS <= TOWN_BUILDINGS
#error Talk topics must have room for a full town and Regional
#endif

static int talk_buildings(TalkBuilding *out, int cap)
{
    int count, i, n;
    const TownBuilding *rows;

    if (!g_talk_in_town) {
        return 0;
    }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    for (i = n = 0; i < count && n < cap; i++) {
        int flags;

        /* A residence is a topic only when a quest marks it as a destination. */
        flags = 0;
        if (talk_category(rows[i].type, 0) < 0) {
            if (!quest_building_site(g_talk_region, g_talk_index, rows[i].block, rows[i].record, 0, 0)) {
                continue;
            }
            flags = 8;
        }
        out[n].id = gfall_building_id(&rows[i]);
        out[n].type = rows[i].type;
        out[n].flags = flags;
        out[n++].distance = talk_distance_to(&rows[i]);
    }
    return n;
}

static void talk_building_name(unsigned id, char *out, int cap)
{
    const TownBuilding *t;

    out[0] = 0;
    t = talk_building(id);
    if (t) {
        gfall_building_name(t, g_talk_region, g_talk_index, out, cap);
    }
}

static int talk_town_has(int key, int by_type)
{
    int count, i;
    const TownBuilding *rows;

    if (!g_talk_in_town) {
        return 0;
    }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    for (i = 0; i < count; i++) {
        if (by_type ? rows[i].type == key : rows[i].faction == key) {
            return 1;
        }
    }
    return 0;
}

static int talk_distance(unsigned id)
{
    const TownBuilding *t;

    t = talk_building(id);
    return t ? talk_distance_to(t) : 99999;
}

/* Which of the eight ways (east first, counter-clockwise) a building lies, by the slope's three bands (tan 22.5 degrees is about 0.414). */
static int compass_index(float dx, float dz)
{
    int east, north;

    east = dx > 0.0f ? 1 : -1;
    north = dz > 0.0f ? 1 : -1;
    dx = dx < 0.0f ? -dx : dx;
    dz = dz < 0.0f ? -dz : dz;
    if (dz < dx * 0.414f) { return east > 0 ? 0 : 4; }
    if (dx < dz * 0.414f) { return north > 0 ? 2 : 6; }
    if (north > 0) { return east > 0 ? 1 : 3; }
    return east > 0 ? 7 : 5;
}

static char g_pq_name[40], g_pq_place[64];

/* A record said with the window's macros: %key what is asked, %loc the place, %di its way, %hnt the hint already said. */
static int talk_say(int text, const TalkWinSay *ctx, char *out, int cap)
{
    static const char *const compass[8] = { "east", "northeast", "north", "northwest", "west", "southwest", "south", "southeast" };
    const TownBuilding *t;

    mem_zero(&g_talk_words, sizeof(g_talk_words));
    g_talk_words.key = ctx->key;
    g_talk_words.loc = ctx->loc ? ctx->loc : ctx->key;
    g_talk_words.hint = ctx->hint;
    g_talk_words.city = g_talk_in_town ? town_name() : "";
    g_talk_words.region = g_talk_region_name;
    g_talk_words.place = "This place";
    g_talk_words.questor = g_pq_name; g_talk_words.questor_place = g_pq_place;
    g_talk_words.tone = ctx->tone;
    t = ctx->building ? talk_building(ctx->building) : 0;
    if (t) {
        g_talk_words.direction = compass[compass_index(t->x - g_talk_x, t->z - g_talk_z)];
    }
    words_use(&g_talk_words);
    return words_say(text, out, cap);
}

/*
 * People met (people and law, slice 8), as click_pedestrian, click_npc and npc_click_service have them (src/lifted/click.c).
 * A person in the street: Steal tries their pocket, every other mode talks (info too). A person standing in a
 * building (a usable): info names them; the others, within 256 units, offer a shopkeeper's panel or talk (npc_click_service).
 * Whoever is picked is remembered here until the window or a panel acts on them.
 */
#define PERSON_TRIED_MOST 32
static unsigned g_person_tried[PERSON_TRIED_MOST];
static int g_person_tried_count;

typedef struct Met {
    int faction, archive, record, female;
    unsigned who;
    char name[NAME_MAX];
    Camera cam;
} Met;
static Met g_met;

static int person_tried(unsigned who)
{
    int i;

    for (i = 0; i < g_person_tried_count && i < PERSON_TRIED_MOST; i++) {
        if (g_person_tried[i] == who) {
            return 1;
        }
    }
    return 0;
}

static void talk_say_text(int text, char *out, int cap)
{
    TalkWinSay ctx;

    mem_zero(&ctx, sizeof(ctx));
    ctx.tone = 1;
    if (talk_say(text, &ctx, out, cap) < 0) {
        out[0] = 0;
    }
}

void gfall_person_say_text(int text, char *out, int cap)
{
    Words w;

    mem_zero(&w, sizeof(w));
    w.city = town_name();
    w.tone = 1;
    w.place = "This place";
    words_use(&w);
    if (words_say(text, out, cap) < 0) {
        out[0] = 0;
    }
}

/* talk_start for whoever g_met is: the window opens, or the record that says they will not (shown as a message). */
/* rumor_pick_news over the one file: a quest's rumour or a faction's, one of the two at random when both have news. */
static int seam_quest_news(int faction, char *out, int cap)
{
    char quest[400];
    int have_quest = quest_news(faction, quest, (int)sizeof(quest)) > 0;

    if (politics_news(out, cap) && (!have_quest || random_range(0, 1))) { return 1; }
    if (!have_quest) { return 0; }
    df_copy(out, quest, cap);
    return 1;
}
static int seam_quest_rumour_for(unsigned npc, char *out, int cap) { return quest_rumour_for((int)npc, out, cap); }

static int g_talk_events_done;
static int potential_questor(char *name, int ncap, char *place, int pcap);
static char g_pq_name[40], g_pq_place[64];

void gfall_start_talk(void)
{
    TalkWho target;
    TalkPlayer player;
    TalkWinWorld world;
    static int members[MEMBERSHIPS];
    static char line[400];
    TravelWhere here;
    const Faction *province;
    int text, origin[2], block, record, index, region;
    float x, z;

    /* A quest's person met: its event 28 runs on the talk path, and a quest that takes the click answers in its own words. */
    if (!g_talk_events_done && quest_talk((int)g_met.who, g_met.faction)) { return; }
    g_talk_events_done = 0;
    mem_zero(&target, sizeof(target));
    target.npc = g_met.who;
    target.faction = g_met.faction;
    target.person = 1;
    /* The factions the player belongs to, and the rank held in a local temple (guild_local_temple_rank). */
    player.member_count = 0;
    for (index = 0; index < MEMBERSHIPS; index++) {
        if (guilds_state()->m[index].used) {
            members[player.member_count++] = guilds_state()->m[index].faction;
        }
    }
    player.members = members;
    player.temple_rank = guild_local_temple_rank(travel_region());
    /* Where the talk is: the town the player stands in (for the places asked after) and the region's name. */
    g_talk_cam = g_met.cam;
    g_talk_in_town = travel_town_here(&g_talk_cam, &region, &index, &x, &z, &block, &record);
    if (g_talk_in_town) {
        g_talk_region = region;
        g_talk_index = index;
        g_talk_x = x;
        g_talk_z = z;
    }
    province = faction_province_of(travel_region());
    df_copy(g_talk_region_name, province ? province->name : "", (int)sizeof(g_talk_region_name));
    gfall_screen_talk_name(g_met.name);
    travel_here(&here, origin);
    mem_zero(&world, sizeof(world));
    world.buildings = talk_buildings;
    world.building_name = talk_building_name;
    world.town_has = talk_town_has;
    world.distance = talk_distance;
    world.say = talk_say;
    world.news = seam_quest_news;
    world.topic_count = quest_topic_count; world.topic_name = quest_topic_name; world.topic_answer = quest_topic_answer;
    world.rumour_for = seam_quest_rumour_for;
    world.width = gfall_talk_width;
    world.outdoors = here.kind != TRAVEL_INSIDE;
    world.region = travel_region();
    /* quest_find_potential_questor (4.2): no work in a dungeon or where no building is marked; else a marked building's person is named, and the building. */
    world.no_work = !potential_questor(g_pq_name, (int)sizeof(g_pq_name), g_pq_place, (int)sizeof(g_pq_place));
    talkwin_world(&world);
    gfall_screen_talk_face(g_met.archive, g_met.record);
    if (!possessions_talk(&target, &player, 0, &text) && text) {
        talk_say_text(text, line, (int)sizeof(line));
        possessions_message(line, 0);
    }
}

int gfall_person_used(const Camera *aim, int mode)
{
    const Faction *people;
    NpcName npc;
    int female;
    unsigned who;
    float along;

    if (!people_in_view_at(aim, TALK_REACH, &female, &who, &along)) {
        return 0;
    }
    people = faction_people_of(travel_region());
    if (!people) {
        return 0;
    }
    mem_zero(&npc, sizeof(npc));
    npc.type = 8;
    npc.region = travel_region();
    npc.id = who;
    npc.female = female;
    mem_zero(&g_met, sizeof(g_met));
    npc_name(&npc, g_met.name, (int)sizeof(g_met.name));
    g_met.faction = people->id;
    g_met.archive = people->flat_archive[female];
    g_met.record = people->flat_record[female];
    g_met.female = female;
    g_met.who = who;
    g_met.cam = *aim;
    if (mode == MODE_STEAL) {
        StealState taken;
        static char line[96];

        /* click_pedestrian: nearer than 128, and the person is marked before the try, so each is tried once, win or lose. */
        if (along > 128.0f || person_tried(who)) {
            return 1;
        }
        if (g_person_tried_count < PERSON_TRIED_MOST) {
            g_person_tried[g_person_tried_count++] = who;
        }
        steal_pocket(0, 0, &taken);
        if (taken.text) {
            gfall_person_say_text(taken.text, line, (int)sizeof(line));
            interact_say(line);
        } else if (taken.line) {
            interact_say(taken.line);
        }
        return 1;
    }
    gfall_start_talk();
    return 1;
}

/*
 * Whether the person met in this building keeps its shop: the block's own people (interior_people) carry the flag the
 * dagger layer drops. The person is found by their picture and faction among those of the building the player stands in.
 */
static int seam_interior_shopkeeper(const Camera *cam, int faction, int image)
{
    InteriorPerson people[INTERIOR_PEOPLE];
    const unsigned char *bytes;
    unsigned size;
    int region, index, block, record, n, i;
    float x, z;

    if (!travel_town_here(cam, &region, &index, &x, &z, &block, &record) || block < 0) {
        return 0;
    }
    bytes = town_block_bytes(region, index, block, &size);
    n = interior_people(bytes, size, record, people, INTERIOR_PEOPLE);
    for (i = 0; i < n; i++) {
        if (people[i].image == image && people[i].faction == faction) {
            return interior_shopkeeper(&people[i]);
        }
    }
    return 0;
}

/*
 * A person standing in a building (a usable of USE_TALK: their faction and flat). npc_click_service: within a shop or a
 * bank, a shopkeeper offers a panel; anyone else talks. The shopkeeper's flag (person flags & 8) is read from the block
 * by GFALL's own reader (interior_shopkeeper). The armorer and the general store test the faction number's bit 8 where
 * the others test the flag, as the decomp has it.
 */

/* quest_mark_givers (4.1 of quest-hooks.md): a person standing in a building of a type other than 1, 11, 14 and 16 up, of a faction of type 2, 12 or 4 not under the Mages', Fighters', Thieves' or
 * Brotherhood's trees, is a giver for the day by a roll (type 4 at 51%, the others 26%). The original rolls each person from `rand` reseeded with the day; we roll each building from the day and its id,
 * so all its people are marked or none (the Work answer can then name the building). */
static int building_marked(unsigned place, int building_type, const FactionDef *f)
{
    int root, guard;
    const FactionDef *top;
    unsigned roll;

    if (!f || building_type == BUILDING_HOUSE_FOR_SALE || building_type == BUILDING_GUILD_HALL || building_type == BUILDING_TEMPLE || building_type >= BUILDING_PALACE || building_type < 0) { return 0; }
    if (!(f->type == 4 || f->type == 2 || f->type == 12)) { return 0; }
    root = f->id; top = f;
    for (guard = 0; top && top->parent && guard < 8; guard++) { top = faction_def(faction_index(top->parent)); if (top) { root = top->id; } }
    if (root == 40 || root == 41 || root == 42 || root == 108) { return 0; }
    roll = (unsigned)(game_state()->minutes / 1440u) * 2654435761u ^ place * 40503u;
    roll = (roll >> 8) % 100u;
    return (int)roll <= (f->type == 4 ? 50 : 25);
}

/* The marks a click cleared today ("the person's mark is cleared at once, whether or not a file was found"), by day and building. */
static struct { unsigned day, place; } g_cleared[16];
static int g_cleared_next;

static int mark_cleared(unsigned place)
{
    int i;

    for (i = 0; i < 16; i++) { if (g_cleared[i].place == place && g_cleared[i].day == (unsigned)(game_state()->minutes / 1440u) + 1u) { return 1; } }
    return 0;
}

/* quest_mark_givers in a dungeon (4.1 of quest-hooks.md): only Daggerfall's, Wayrest's and Sentinel's castles (locations 50027, 50029, 50033) mark people, any resident of a type 2, 12 or 4 faction (not under the
 * guilds' trees) by a roll of the day and the person; and quest_pick_for_npc offers nothing in a dungeon but an R quest to a person of a type 4 faction (not 407). 1 when one was offered. */
static int castle_quest(const FactionDef *f)
{
    const Dungeon *data;
    char name[9];
    unsigned roll;
    int root, guard;
    const FactionDef *top;

    if (world_where()->where.kind != WHERE_DUNGEON || !f || f->type != 4 || f->id == 407) { return 0; }
    data = travel_place_data();
    if (!data || (data->location_id != 50027 && data->location_id != 50029 && data->location_id != 50033)) { return 0; }
    root = f->id; top = f;
    for (guard = 0; top && top->parent && guard < 8; guard++) { top = faction_def(faction_index(top->parent)); if (top) { root = top->id; } }
    if (root == 40 || root == 41 || root == 42 || root == 108) { return 0; }
    roll = (unsigned)(game_state()->minutes / 1440u) * 2654435761u ^ g_met.who * 40503u;
    roll = (roll >> 8) % 100u;
    if ((int)roll > 50 || mark_cleared(g_met.who) || quest_person_busy((int)g_met.who)) { return 0; }
    g_cleared[g_cleared_next].day = (unsigned)(game_state()->minutes / 1440u) + 1u; g_cleared[g_cleared_next].place = g_met.who; g_cleared_next = (g_cleared_next + 1) & 15;
    if (!quest_pick_seeded('R', 0, 'C', character_state()->level, g_met.who, name)) { return 0; }
    quest_set_questor((int)g_met.who, g_met.faction, g_met.name, town_name());
    return quest_begin(name, g_met.faction) >= 0;
}

/* The giver the Work answer points to (%pqn, %pqp): one of the town's marked buildings picked by the day, a name made by the town's naming, and the building's own. 0 away from a town or when none is
 * marked. */
static int potential_questor(char *name, int ncap, char *place, int pcap)
{
    const TownBuilding *rows;
    int count, i, n, pick;
    NpcName npc;

    if (!g_talk_in_town || world_where()->where.kind == WHERE_DUNGEON) { return 0; }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    n = 0;
    for (i = 0; rows && i < count; i++) {
        if (building_marked(gfall_building_id(&rows[i]), rows[i].type, faction_def(faction_index(rows[i].faction))) && !mark_cleared(gfall_building_id(&rows[i]))) { n++; }
    }
    if (!n) { return 0; }
    pick = (int)(((unsigned)(game_state()->minutes / 1440u) * 2246822519u >> 7) % (unsigned)n);
    for (i = 0; i < count; i++) {
        if (!(building_marked(gfall_building_id(&rows[i]), rows[i].type, faction_def(faction_index(rows[i].faction))) && !mark_cleared(gfall_building_id(&rows[i]))) || pick-- != 0) { continue; }
        mem_zero(&npc, sizeof(npc));
        npc.type = 8; npc.region = travel_region(); npc.id = gfall_building_id(&rows[i]) * 31u + (unsigned)(game_state()->minutes / 1440u); npc.faction = rows[i].faction;
        npc_name(&npc, name, ncap);
        talk_building_name(gfall_building_id(&rows[i]), place, pcap);
        return 1;
    }
    return 0;
}

/* Clicking a marked person (after the quest events of the click) offers a quest: R for a type 4 faction other than 407, else A or K, for a non-member at the player's level, the one their id picks.
 * 1 when one was offered. */
static int street_quest(const FactionDef *f, int building_type, unsigned place)
{
    char name[9];

    if (!building_marked(place, building_type, f) || mark_cleared(place) || quest_person_busy((int)g_met.who)) { return 0; }
    g_cleared[g_cleared_next].day = (unsigned)(game_state()->minutes / 1440u) + 1u; g_cleared[g_cleared_next].place = place; g_cleared_next = (g_cleared_next + 1) & 15;
    if (!quest_pick_seeded(f->type == 4 && f->id != 407 ? 'R' : 'A', f->type == 4 && f->id != 407 ? 0 : 'K', 'C', character_state()->level, g_met.who, name)) { return 0; }
    quest_set_questor((int)g_met.who, g_met.faction, g_met.name, town_name());
    return quest_begin(name, g_met.faction) >= 0;
}

int gfall_talk_used(const Usable *u, int mode, float dist, const Camera *cam)
{
    const FactionDef *f;
    NpcName npc;
    int type, faction, quality, flat_archive, flat_record, female, merchant, kind;
    unsigned place;
    int what, qwho, qfaction, qfemale, quest;
    char qname[40];

    /* A quest's person (their flat is marked with the quest and the person): their own faction, id and name. */
    quest = u->what >= QUEST_PERSON_MARK && quest_person_of_tag(u->what, &qwho, &qfaction, &qfemale, qname, (int)sizeof(qname));
    what = quest ? (qfaction ? qfaction : 510) : u->what;
    flat_archive = u->tag >> 7;
    flat_record = u->tag & 127;
    female = faces_female(flat_archive, flat_record);
    mem_zero(&g_met, sizeof(g_met));
    g_met.faction = what;
    g_met.archive = flat_archive;
    g_met.record = flat_record;
    g_met.female = female < 0 ? 0 : female;
    g_met.who = quest ? (unsigned)qwho : (unsigned)u->tag * 31u + (unsigned)what;
    g_met.cam = *cam;
    mem_zero(&npc, sizeof(npc));
    npc.type = 8;
    f = faction_def(faction_index(what));
    npc.faction = what;
    npc.faction_is_individual = f && f->type == 4;
    npc.faction_name = f ? f->name : 0;
    npc.region = travel_region();
    npc.id = g_met.who;
    npc.female = g_met.female;
    npc_name(&npc, g_met.name, (int)sizeof(g_met.name));
    if (quest && qname[0]) { df_copy(g_met.name, qname, (int)sizeof(g_met.name)); }
    if (mode == MODE_INFO) {
        interact_say(g_met.name);
        return 1;
    }
    if (dist > 256.0f) {
        return 1;
    }
    /* npc_click_service: a person of a coven (a faction of type 8) opens the coven's menu, after the quest events of the click. */
    if (f && f->type == FACTION_TYPE_COVEN) {
        if (quest_talk((int)g_met.who, g_met.faction)) { return 1; }
        gfall_coven_enter(what);
        return 1;
    }
    kind = 0;
    merchant = seam_interior_shopkeeper(cam, what, u->tag);
    if (travel_building(&type, &faction, &quality, &place)) {
        /* npc_click_service: the Thieves' and the Brotherhood's people serve their members, a stranger of the Brotherhood only talks, halls and temples have their menu. */
        int parent;

        parent = f ? f->parent : 0;
        if ((what == 42 || parent == 42) && guild_find_kind(3)) { gfall_hall_enter(what, faction); return 1; }
        if ((what == 108 || parent == 108) && guild_find_kind(0)) { gfall_hall_enter(what, faction); return 1; }
        if (what == 108 || parent == 108) { gfall_start_talk(); return 1; }
        if (type == BUILDING_GUILD_HALL || type == BUILDING_TEMPLE) { gfall_hall_enter(what, faction); return 1; }
        if (type == BUILDING_TAVERN && merchant) { kind = SERVICE_TAVERN; }
        else if (type == 3 && merchant) { kind = SERVICE_BANK; }
        else if ((type == 2 || type == 9) && (what & 8)) { kind = SERVICE_REPAIR; }
        else if (type == 13 && merchant) { kind = SERVICE_REPAIR; }
        else if ((type == 0 || type == 5 || type == 6 || type == 8 || type == 12) && merchant) { kind = SERVICE_SELL; }
    }
    if (kind && possessions_service(kind)) {
        return 1;
    }
    /* The quest events of the click come first (28, 71, 1, 78), then a marked person's offer, then talk. */
    if (quest_talk((int)g_met.who, g_met.faction)) { return 1; }
    if (!kind && travel_building(&type, &faction, &quality, &place) && street_quest(f, type, place)) { return 1; }
    if (!kind && world_where()->where.kind == WHERE_DUNGEON && castle_quest(f)) { return 1; }
    g_talk_events_done = 1;
    gfall_start_talk();
    return 1;
}

/* click.c 960..985, a house's chest or cupboard (a 418 prop, or a 410 furniture variant that furniture_is_container says holds things): stocked the first time by shop_generate_stock, empty ones do nothing; the player's own house opens freely; anyone else's asks first (TEXT.RSC 37), and a grab may be seen: a pickpocket roll against 108, then theft (crime 13) and the guards. */
static df_u32 g_container_pending;

static void container_answer(int yes)
{
    if (!yes || !g_container_pending) { return; }
    if (random_range(0, 255) >= 108 - character_state()->skill[15]) {
        law_note(CRIME_THEFT);
        guards_summon(1);
    }
    if (!possessions_open(VIEW_INVENTORY, g_container_pending)) { interact_say("You cannot take anything now."); }
}

int gfall_container_used(int index)
{
    const Usable *u;
    ObjectInfo *box;
    df_u32 id;
    int type, faction, quality;
    unsigned place;

    u = &seam_level()->scene.usables[index];
    if (!travel_building(&type, &faction, &quality, &place)) {
        interact_say("There is nothing here you want.");
        return 1;
    }
    id = loot_container(object_at(WORLD_ROOT)->place_key, (df_u32)u->tag, u->cx, u->cy, u->cz, -1);
    box = id ? object_edit(id) : 0;
    if (!box) { interact_say("There is no room for another container."); return 1; }
    if (!(box->flags & 2)) {
        box->flags |= 2;
        shelf_stock(id, (int)((df_u32)u->hd % 100u), quality, type, u->model);
    }
    if (!inventory_rows(id, -1, 0, 0)) {
        interact_say("There is nothing here you want.");
        return 1;
    }
    if (bank_owns_place(place)) {
        if (!possessions_open(VIEW_INVENTORY, id)) { interact_say("You cannot take anything now."); }
        return 1;
    }
    g_container_pending = id;
    if (!possessions_ask(words_state(37), container_answer)) { interact_say("You cannot take anything now."); }
    return 1;
}

void gfall_notice_used(void)
{
    static char board[700], page[760];
    char town[40];

    df_copy(town, town_name(), sizeof(town));
    if (!politics_board(travel_region(), board, (int)sizeof(board))) { interact_say("There is nothing on the notice board."); return; }
    log_format(page, sizeof(page), "%s\n%s", town, board);
    possessions_message(page, 0);
}

int gfall_shelf_used(int index, int variant)
{
    ShopState shop;
    int type, faction, quality, region;
    unsigned place;

    if (!travel_building(&type, &faction, &quality, &place)) {
        interact_say("There is nothing here you want.");
        return 1;
    }
    if (g_shelf_level != seam_level() || g_shelf_place != place) {
        forget_shelves();
        g_shelf_level = seam_level();
        g_shelf_place = place;
    }
    if (index < 0 || index >= SHELVES_MOST) {
        return 1;
    }
    if (!g_shelf_box[index]) {
        g_shelf_box[index] = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
        if (!g_shelf_box[index]) {
            interact_say("There is no room for another shelf.");
            return 1;
        }
        shelf_stock(g_shelf_box[index], variant, quality, type, index);
        /* shelf_open_stock: an alchemist's shelf, a quarter of the time, also holds a recipe for a potion; each shelf of a general store, when first opened, a horse and a cart (collected_count == 0). */
        if (type == 0 && random_range(1, 100) < 25) { item_create(g_shelf_box[index], 27, 4, 0); }
        if (type == 9) {
            item_create(g_shelf_box[index], 23, 1, -1);
            item_create(g_shelf_box[index], 23, 0, -1);
        }
    }
    region = travel_region();
    shop.region_adjust = region_price(region);
    shop.quality = quality;
    shop.type = type;
    shop.holiday = holiday_today(game_state()->minutes, region);
    shop.half = 0;
    shop.scale = 256;
    {
        /* shelf_open_stock: a shop that is open sells from its shelves; one that is shut lets them be opened as a plain container, the goods free to take. */
        HoursPlayer hours;
        Membership *m;

        m = guild_find_kind(0); hours.brotherhood = m ? m->rank : -1;
        m = guild_find_kind(3); hours.thieves = m ? m->rank : -1;
        m = guild_find_kind(2); hours.fighters = m ? m->rank : -1;
        m = guild_find_kind(1); hours.mages = m ? m->rank : -1;
        if (!building_open(type, faction, place, game_state()->minutes, 0, &hours)) {
            if (!possessions_open(VIEW_INVENTORY, g_shelf_box[index])) {
                interact_say("You cannot take anything now.");
            }
            return 1;
        }
    }
    if (!possessions_trade(MARKET_BUY, &shop, g_shelf_box[index])) {
        interact_say("You cannot trade now.");
    }
    return 1;
}

/*
 * Where the player may sleep (rest_allowed): the dungeon, the wilds, a town's streets (where the watch
 * minds a sleeper) or inside a building. The outdoor location's kind (a keep, a ruin, a graveyard are no
 * town and let a sleeper be) is not carried by the dagger's location records, so every town is a town.
 */
static const Camera *g_site_camera;

void gfall_people_set_camera(const Camera *camera)
{
    g_site_camera = camera;
}

int gfall_rest_site(RestSite *site)
{
    TravelWhere here;
    int origin[2], region, index, block, record, type, faction, quality;
    unsigned place;
    float x, z;

    mem_zero(site, sizeof(*site));
    travel_here(&here, origin);
    /* A ship owned and the player on its location (the streets of "Your Ship" or its cabin): a berth, always welcome (rest_allowed, rest.c 199..201). */
    if (bank_state()->ship && here.kind != TRAVEL_DUNGEON && here.located && here.region == SHIP_REGION) { site->ship_berth = 1; }
    if (here.kind == TRAVEL_DUNGEON) {
        site->kind = SITE_DUNGEON;
    } else if (here.kind == TRAVEL_INSIDE && travel_building(&type, &faction, &quality, &place)) {
        site->kind = SITE_INSIDE;
        site->building_type = type;
        site->building_faction = faction;
        site->building = place;
        site->own_house = bank_owns_place(place) ? place : 0;
        site->fighters_member = guild_find_faction(FACTION_FIGHTERS_GUILD) != 0; /* a bed is for the Fighters Guild's members, not the knights (run_0006DC35.c) */
    } else if (g_site_camera && travel_town_here(g_site_camera, &region, &index, &x, &z, &block, &record)) {
        site->kind = SITE_TOWN;
    } else {
        site->kind = SITE_WILD;
    }
    return 1;
}

int gfall_court_say(int text, const Court *court, char *out, int cap)
{
    static WordsPlayer pc;
    static char penalty[96], prison[48];
    Words w;
    int i;

    for (i = 0; i < (int)sizeof(pc.name) - 1 && character_state()->name[i]; i++) { pc.name[i] = character_state()->name[i]; }
    pc.name[i] = 0;
    mem_zero(&w, sizeof(w));
    w.pc = &pc;
    w.city = town_name();
    w.crime = crime_name(court->crime + 1);
    if (court->severity == 0) { df_copy(penalty, "Banishment", sizeof(penalty)); }
    else if (court->severity == 1) { df_copy(penalty, "Execution", sizeof(penalty)); }
    else { log_format(penalty, sizeof(penalty), "%d gold pieces in fines and %d days in prison", court->fine, court->days); }
    w.penalty = penalty;
    w.days = court->days;
    w.more_days = court->days;
    /* The prison a death sentence names (record 8100's variants: "The Cage of Slarpen", "The Hall of Yegon"). */
    df_copy(prison, "the prison", sizeof(prison));
    words_use(&w);
    words_say(8100, prison, sizeof(prison));
    w.prison = prison;
    words_use(&w);
    return words_say(text, out, cap);
}

void gfall_court_release(int where)
{
    /* The street (8) and the walls' edge (4) and an acquittal's (12) are all out of the building the case was heard in. */
    (void)where;
    travel_out_to_street();
}
unsigned gfall_people_id(void) { return g_met.who; }
const char *gfall_people_name(void) { return g_met.name; }
