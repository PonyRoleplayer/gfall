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
#include "engine/ui/pic.h"

#include "game/dagger/formats/factionfile.h"
#include "game/dagger/play/ui/words.h"
#include "game/dagger/places/town.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/travel/travelmap.h"

#include "game/dagger/play/play.h"

#include "game/dagger/formats/rmb.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/play/people/entry.h"
#include "game/dagger/play/watcom.h"
#include "game/dagger/tables/sounds.h"
#include "engine/sound/ambience.h"
#include "buildings.h"
unsigned gfall_building_id(const TownBuilding *t)
{
    return ((unsigned)(t->block + 1) << 8) | (unsigned)t->record;
}

static Words g_building_words;

static void building_expand(const char *in, char *out, int cap)
{
    words_use(&g_building_words);
    words_expand(in, out, cap);
}

/* Resolve the sign from the building's own town, including outside conversations. */
void gfall_building_name(const TownBuilding *t, int region, int index, char *out, int cap)
{
    BuildingName b;
    const Faction *province;
    const FactionDef *f;
    char surname[24];

    province = faction_province_of(region);
    mem_zero(&g_building_words, sizeof(g_building_words));
    g_building_words.city = town_name();
    g_building_words.region = province ? province->name : "";
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

static df_u32 g_in_building;
/* The draw taken at each entry that picks the visit's sneaking song (maplogic.c 395). */
static unsigned g_sneak_pick;
static int g_in_type, g_in_block, g_in_record, g_in_region, g_in_index, g_trespassing;
/* The interior's restriction points, read when the building is entered. */
#define RESTRICTIONS_MOST 192
static RmbRestriction g_restrict[RESTRICTIONS_MOST];
static int g_restrictions;

void gfall_building_entered(const PlaceWhere *where)
{
    const TownBuilding *t;
    const unsigned char *bytes;
    unsigned size;
    static int town_region = -1, town_index = -1;

    t = town_building(where->region, where->index, where->block, where->record);
    if (!t) { return; }
    /* The bashed doors and the picks tried last as long as the town stays loaded (building_try_enter's flags, support.c). */
    if (where->region != town_region || where->index != town_index) { entry_reset(); town_region = where->region; town_index = where->index; }
    g_in_building = gfall_building_id(t); g_in_type = t->type; g_in_block = where->block; g_in_record = where->record;
    g_in_region = where->region; g_in_index = where->index; g_trespassing = 0;
    g_sneak_pick = (unsigned)random_range(0, RAND_MAX_WATCOM);
    entry_grant(g_in_building, t->type, g_in_building == (df_u32)bank_state()->house ? ACCESS_OWNER : ACCESS_VISITOR, game_state()->minutes);
    bytes = town_block_bytes(where->region, where->index, where->block, &size);
    g_restrictions = bytes ? rmb_restrictions(bytes, size, where->record, g_restrict, RESTRICTIONS_MOST) : 0;
}

void gfall_building_frame(const Camera *cam)
{
    int type, faction, quality, i, best, restriction, crime;
    unsigned place;
    float best_distance, dx, dy, dz, d;

    if (!g_in_building) { return; }
    if (!travel_building(&type, &faction, &quality, &place)) {
        entry_leave(g_in_building); g_in_building = 0; g_restrictions = 0; g_trespassing = 0; ambience_set_song(0);
        return;
    }
    best = -1; best_distance = 0;
    for (i = 0; i < g_restrictions; i++) {
        dx = g_restrict[i].x-cam->x; dy = g_restrict[i].y-cam->y; dz = g_restrict[i].z-cam->z;
        d = dx*dx+dy*dy+dz*dz;
        if (best < 0 || d < best_distance) { best = i; best_distance = d; }
    }
    restriction = best >= 0 ? g_restrict[best].level : 0;
    crime = entry_trespass(restriction, entry_access(g_in_building, game_state()->minutes), g_trespassing);
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
    names_set_expand(building_expand);
    door_set_street_bash(gfall_building_bash);
    door_set_castle(in_castle);
}
