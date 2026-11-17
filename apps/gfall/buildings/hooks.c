#include "game/dagger/play/state.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/ui/hud.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"

#include "game/dagger/places/town.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/people/hours.h"
#include "game/dagger/play/people/entry.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/places/travel.h"

#include "game/dagger/play/play.h"

#include "buildings.h"
/*
 * A door from the street into a building (building_try_enter): its hours, a guild's own door, a house by day, a lock picked in Steal mode (and the crime of it), else how hard
 * the lock is. Anything else, a dungeon's door, the way out of a building, goes straight to travel_use.
 */
const char *gfall_building_passage(const Usable *u)
{
    const TownBuilding *t;
    Usable in;
    EntryBuilding b;
    EntryPlayer p;
    Entry e;
    Membership *m;
    int region, index, minute;

    if (u->kind == USE_PASSAGE && travel_door_in_town(u, &in, &region, &index) && (t = town_building(region, index, u->tag >> 8, u->tag & 0xff)) != 0) {
        mem_zero(&b, sizeof(b));
        b.id = gfall_building_id(t);
        b.type = t->type; b.faction = t->faction; b.name_seed = t->name_seed;
        b.quest_open = quest_building_open(region, index, t->block, t->record);
        b.access = quest_building_access(region, index, t->block, t->record, game_state()->minutes);
        mem_zero(&p, sizeof(p));
        p.house = (df_u32)bank_state()->house;
        m = guild_find_kind(0); p.brotherhood = m ? m->rank : -1;
        m = guild_find_kind(3); p.thieves = m ? m->rank : -1;
        m = guild_find_kind(2); p.fighters = m ? m->rank : -1;
        m = guild_find_kind(1); p.mages = m ? m->rank : -1;
        minute = (int)(game_state()->minutes % 1440u);
        p.daytime = minute >= 360 && minute < 1080;
        p.steal = hud_mode() == MODE_STEAL;
        p.lock_spell = (character_state()->conditions & 0x40) != 0;
        if (!building_try_enter(&b, &p, game_state()->minutes, (guards_witnesses() & 3) != 0, &e)) {
            /* click_show_building_info: a shop shut for the hour says when it opens (FALL.EXE's "Store is closed. Open from %d:00 to %d:00."). In Steal mode the lock's own line is what matters. */
            if (b.type < BUILDING_TEMPLE && b.type != BUILDING_HOUSE_FOR_SALE && p.steal == 0) {
                static char hours[64];
                int from, to;

                if (building_hours(b.type, &from, &to)) {
                    log_format(hours, sizeof(hours), "Store is closed. Open from %d:00 to %d:00.", from, to);
                    return hours;
                }
            }
            return e.line ? e.line : "The door is locked.";
        }
        if (e.spent_spell) { character_state()->conditions &= ~0x40; }
        if (e.crime) { law_note(CRIME_ATTEMPTED_BREAKING); guards_summon(1); }
        if (e.text) { msg_set(words_state(e.text)); }
        else if (e.picked && e.line) { msg_set(e.line); }
    }
    return travel_use(u);
}
