#include "game/dagger/play/state.h"
#include "game/dagger/play/travel/doors.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/quests/quest.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/keep.h"
#include "engine/ui/pic.h"

#include "game/dagger/formats/factionfile.h"
#include "game/dagger/play/ui/words.h"
#include "game/dagger/places/town.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/travel/travelmap.h"
#include "game/dagger/play/travel/townmap.h"

#include "game/dagger/play/play.h"

#include "game/dagger/places/travel.h"
#include "game/dagger/places/world.h"
#include "game/dagger/play/input/move.h"
#include "game/dagger/play/input/activate.h"
#include "game/dagger/play/people/entry.h"
#include "game/dagger/play/people/lodging.h"
#include "game/dagger/play/watcom.h"
#include "game/dagger/tables/sounds.h"
#include "engine/sound/ambience.h"
#include "buildings.h"
unsigned gfall_building_id(const TownBuilding *t)
{
    const PlaceWhere *town = &world_where()->where;

    return town_building_key(town->region, town->index, t->block, t->record);
}

static Words g_building_words;

/* macro_ef_shop_owner_name draws from the executable's bank byte (0). */
static void building_owner(char *out, int cap)
{
    int n;

    name_generate(0, 0, out, cap);
    for (n = 0; out[n] && out[n] != ' '; n++) { }
    out[n] = 0;
}

static void building_expand(const char *in, char *out, int cap)
{
    words_use(&g_building_words);
    words_expand(in, out, cap);
}

/* Resolve the sign from the building's own town, including outside conversations. */
void gfall_building_name(const TownBuilding *t, int region, int index, char *out, int cap)
{
    BuildingName b;
    const Faction *province, *court;
    const FactionDef *f;
    char surname[24];

    province = faction_province_of(region);
    court = faction_court_of(region);
    if (court && !court->ruler) { court = faction_find(court->parent); }
    mem_zero(&g_building_words, sizeof(g_building_words));
    g_building_words.city = town_name();
    g_building_words.region = province ? province->name : "";
    g_building_words.shop_owner = building_owner;
    g_building_words.title = name_ruler_title(court ? court->ruler : 0);
    if (t->name_seed & BUILDING_TEXT_NAME) {
        building_expand(words_state(t->name_seed & ~BUILDING_TEXT_NAME), out, cap);
        if (out[0]) {
            return;
        }
    }
    mem_zero(&b, sizeof(b));
    b.type = t->type;
    b.faction = t->faction;
    b.name_seed = t->name_seed;
    b.region_name = g_building_words.region;
    f = faction_def(faction_index(t->faction));
    if (f) {
        b.faction_name = f->name;
        if (t->type == BUILDING_TEMPLE) {
            int i;

            /* faction_add_record links the first immediate child in file order. */
            for (i = 0; i < faction_count(); i++) {
                const Faction *child;

                child = faction_at(i);
                if (child->parent == f->id) {
                    b.child_name = child->name;
                    break;
                }
            }
        }
    }
    /* A residence that is a quest destination can be named for its resident. */
    if (t->type >= BUILDING_HOUSE1 && quest_building_site(region, index, t->block, t->record, surname, (int)sizeof(surname)) && surname[0]) {
        b.resident = surname;
    }
    building_name(&b, out, cap);
}

/* Place zero is reserved here for game-only notes, outside the keyed place
 * state that drops with dungeons. The save already carries these rows. */
static KeepKey building_note_key(int region, int index, int block, int record)
{
    KeepKey key;

    key.place = 0;
    key.thing = town_building_key(region, index, block, record);
    return key;
}

void gfall_building_note(const TownBuilding *building, int region, int index, int from_directions)
{
    Keep *note = keep_get(building_note_key(region, index, building->block, building->record), KEEP_MOVER);

    if (!note) { return; }
    note->game[0] = 1;
    if (from_directions) {
        const TownBuilding *rows;
        int count, i;

        /* Keep every discovered name, highlighting the latest directions in each town. */
        rows = town_buildings(region, index, &count);
        for (i = 0; i < count; i++) {
            Keep *other = keep_find(building_note_key(region, index, rows[i].block, rows[i].record));

            if (other && other != note) { other->game[1] = 0; }
        }
        note->game[1] = 1;
    }
}

void gfall_building_visit(const PlaceWhere *where)
{
    const TownBuilding *building = town_building(where->region, where->index, where->block, where->record);

    if (building) { gfall_building_note(building, where->region, where->index, 0); }
}

int gfall_building_map_name(const TownBuilding *building, int region, int index, char *out, int cap)
{
    const Keep *note;
    df_u32 seed;

    note = keep_find(building_note_key(region, index, building->block, building->record));
    if (!note || !note->game[0]) { return 0; }
    seed = game_state()->random;
    gfall_building_name(building, region, index, out, cap);
    random_seed(seed);
    return out[0] ? (note->game[1] ? TOWNMAP_MARKED : TOWNMAP_NAMED) : 0;
}

static const char *building_info(const Usable *door)
{
    static char name[65];
    const TownBuilding *building;
    int region, index;
    Usable local;

    if (door->what != DOOR_ENTER || world_where()->where.kind != WHERE_WORLD ||
        !travel_door_in_town(door, &local, &region, &index)) { return 0; }
    building = town_building(region, index, door->tag >> 8, door->tag & 255);
    if (!building) { return 0; }
    gfall_building_note(building, region, index, 0);
    return gfall_building_map_name(building, region, index, name, sizeof(name)) ? name : 0;
}

void gfall_building_announce(const PlaceWhere *where)
{
    const TownBuilding *t;
    const char *label;
    df_u32 seed;
    char name[65], line[MESSAGE_LEN];

    t = town_building(where->region, where->index, where->block, where->record);
    if (!t) { return; }
    seed = game_state()->random;
    gfall_building_name(t, where->region, where->index, name, (int)sizeof(name));
    random_seed(seed);

    label = name[0] ? name : "a building";
    if (df_eq(label, "Residence")) { label = "a residence"; }
    log_format(line, sizeof(line), "You've entered %s.", label);
    msg_set(line);
}

#define ROOM_LINE_HOURS_DIV 60
void gfall_building_room_lines(const PlaceWhere *where)
{
    const TownBuilding *rows;
    int count, i, k;
    char name[65], line[MESSAGE_LEN];

    rows = town_buildings(where->region, where->index, &count);
    for (k = 0; rows && k < ROOMS_MOST; k++) {
        const RoomInfo *room = &lodging_state()->room[k];

        if (!room->building || room->expires <= game_state()->minutes) { continue; }
        for (i = 0; i < count; i++) {
            if (gfall_building_id(&rows[i]) != room->building) { continue; }
            gfall_building_name(&rows[i], where->region, where->index, name, (int)sizeof(name));
            log_format(line, sizeof(line), "You have a room at %s for %d hours.", name, (int)((room->expires - game_state()->minutes) / ROOM_LINE_HOURS_DIV) + 1);
            msg_set(line);
            break;
        }
    }
}

static df_u32 g_in_building;
/* The draw taken at each entry that picks the visit's sneaking song (maplogic.c 395). */
static unsigned g_sneak_pick;
static int g_trespassing;

void gfall_building_entered(const PlaceWhere *where, int restoring)
{
    const TownBuilding *t;
    df_u32 seed;
    static int town_region = -1, town_index = -1;

    t = town_building(where->region, where->index, where->block, where->record);
    if (!t) { return; }
    /* The bashed doors and the picks tried last as long as the town stays loaded (building_try_enter's flags, support.c). */
    if (where->region != town_region || where->index != town_index) { entry_reset(); town_region = where->region; town_index = where->index; }
    g_in_building = town_building_key(where->region, where->index, where->block, where->record);
    seed = game_state()->random;
    g_sneak_pick = (unsigned)random_range(0, RAND_MAX_WATCOM);
    if (restoring) { random_seed(seed); }
    entry_grant(g_in_building, t->type, bank_owns_place(g_in_building) ? ACCESS_FULL : ACCESS_VISITOR, game_state()->minutes);
}

void gfall_building_left(void)
{
    if (!g_in_building) { return; }
    entry_leave(g_in_building);
    g_in_building = 0;
    g_trespassing = 0;
    ambience_set_song(0);
}

void gfall_building_frame(void)
{
    const PlaceWhere *where;
    df_u32 now;
    int access, quest_access, restriction, crime;

    if (!g_in_building) { return; }
    where = &world_where()->where;
    now = game_state()->minutes;
    access = entry_access(g_in_building, now);
    quest_access = quest_building_access(where->region, where->index, where->block, where->record, now);
    if (quest_access > access) { access = quest_access; }
    restriction = world_restriction_at(travel_whole_world(), move_body()->feet);
    crime = entry_trespass(restriction, access, g_trespassing);
    g_trespassing = crime != 0;
    /* While trespassing the building's music is the sneaking song; the place's own comes back when it ends. */
    ambience_set_song(crime ? sounds_sneak(g_sneak_pick) : 0);
    if (crime) { law_note(crime); guards_summon(0); }
}

/* The houses for sale of the town the player stands in (type 1 buildings, at most HOUSES_MOST) with their prices, and whether the place is a port. */
static int houses_for_sale(df_u32 *ids, int *prices, int cap)
{
    const PlaceWhere *w;
    const TownBuilding *rows;
    int count, i, n;

    w = &world_where()->where;
    if (!w->located) { return 0; }
    rows = town_buildings(w->region, w->index, &count);
    for (i = n = 0; rows && i < count && n < cap; i++) {
        if (rows[i].type != BUILDING_HOUSE_FOR_SALE) { continue; }
        ids[n] = gfall_building_id(&rows[i]);
        prices[n] = town_house_price(w->region, w->index, rows[i].block);
        n++;
    }
    return n;
}

/* saved_region_name and saved_location_name: the province the house is in and the town. */
static void house_names(char *region, char *town, int cap)
{
    const Faction *province = faction_province_of(travel_region());

    df_copy(region, province ? province->name : "", cap);
    df_copy(town, town_name(), cap);
}

/* The cabin of a ship: every building of its "Your Ship" location, by the key a visit to it makes. */
static int ship_places(int ship, df_u32 *keys, int cap)
{
    const TownBuilding *rows;
    int count, i, n = 0, index = ship == SHIP_LARGE ? 2 : 1;

    rows = town_buildings(SHIP_REGION, index, &count);
    for (i = 0; rows && i < count && n < cap; i++) { keys[n++] = town_building_key(SHIP_REGION, index, rows[i].block, rows[i].record); }
    return n;
}

static int in_port(void)
{
    const PlaceWhere *w;

    w = &world_where()->where;
    return w->located && exterior_is_port(w->region, w->index);
}

/* The castles of Daggerfall, Wayrest and Sentinel (locations 50027, 50029, 50033). */
#define CASTLE_DAGGERFALL 50027
#define CASTLE_WAYREST 50029
#define CASTLE_SENTINEL 50033
static int in_castle(void)
{
    const Dungeon *d = world_where()->where.kind == WHERE_DUNGEON ? travel_place_data() : 0;

    return d && (d->location_id == CASTLE_DAGGERFALL || d->location_id == CASTLE_WAYREST || d->location_id == CASTLE_SENTINEL);
}

void gfall_building_install(void)
{
    bank_set_houses(houses_for_sale);
    bank_set_port(in_port);
    bank_set_names(house_names);
    bank_set_ship_places(ship_places);
    names_set_expand(building_expand);
    activate_set_building_info(building_info);
    door_set_street_bash(gfall_building_bash);
    door_set_castle(in_castle);
}
