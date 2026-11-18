#include "game/dagger/play/state.h"
#include "game/dagger/play/travel/doors.h"
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

void gfall_building_install(void)
{
    names_set_expand(building_expand);
    door_set_street_bash(gfall_building_bash);
}
