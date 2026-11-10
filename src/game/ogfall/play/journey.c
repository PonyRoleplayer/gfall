#include "game/ogfall/play/travel.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/progress.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/tables/traveldefs.h"
#include "engine/core/mem.h"

static int absolute(int n) { return n < 0 ? -n : n; }
void og_journey_plan(int sx, int sy, int ex, int ey, int reckless, int camp,
                     int ship, int transport, OgJourney *out)
{
    int dx, dy, adx, ady, n, i, error, climate, minutes, factor;
    mem_zero(out,sizeof(*out)); dx = ex-sx; dy = ey-sy;
    adx = absolute(dx); ady = absolute(dy); n = adx > ady ? adx : ady;
    error = 0; factor = transport == 1 ? 128 : transport == 2 ? 192 : 256;
    /* func_0009CEC4: the minor axis advances only after a strict >. */
    for (i = 0; i < n; i++) {
        if (n == adx) { sx += dx < 0 ? -1 : 1; error += ady; if (error > adx) { error -= adx; sy += dy < 0 ? -1 : 1; } }
        else { sy += dy < 0 ? -1 : 1; error += adx; if (error > ady) { error -= ady; sx += dx < 0 ? -1 : 1; } }
        climate = og_travel_climate(sx,sy);
        if (climate == 223) { out->sea_pixels++; minutes = ship ? 51 : 255; }
        else { minutes = ((512-og_travel_terrain(climate))*((factor*102)/256))/256; }
        if (camp) { minutes = minutes*300/256; }
        out->minutes += minutes;
    }
    /* travel_open_trip/toggle_option halve the cached estimate too,
     * before travel_trip_cost prices its nights. */
    if (reckless) { out->minutes /= 2; }
    out->hours = (out->minutes+59)/60;
    out->cost = camp ? 0 : ((out->hours-out->sea_pixels)/24)*5+5;
    if (ship && out->sea_pixels) { out->cost += (out->sea_pixels/24+1)*25; }
}
int og_journey_transport(void)
{
    int i, transport;
    transport = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        const OgObject *item;
        item = &og_state()->object[i];
        if (item->id && item->type == OG_RECORD_ITEM && item->parent == og_state()->inventory_id && item->group == 23) {
            if (item->index == 1) { transport = 1; } else if (item->index == 0) { transport = 2; }
        }
    }
    return transport;
}
int og_journey_warning(void)
{
    KeepKey player;
    int i;
    player = og_magic_key(-1);
    /* health_status_add: latent ordinary disease warns too, poison only
     * after onset. Creature ailments must not warn for the traveller. */
    for (i = 0; i < OG_AILMENTS; i++) {
        const OgAilment *a;
        a = &og_magic_state()->ailment[i];
        if (a->active && a->target.place == player.place && a->target.thing == player.thing &&
            (a->id < 100 || (a->id >= 128 && a->stage))) { return 1; }
    }
    return 0;
}
int og_journey_begin(int region, int index, int reckless, int camp, int ship)
{
    OgJourney route;
    OgLocation location;
    OgCharacter *who;
    int i, found, transport, fatigue, minute, wait;
    if (og_world_state()->where.kind != OG_PLACE_WORLD || !og_state()->ready || og_character()->health <= 0) {
        og_msg_set("You must be outside to travel."); return 0;
    }
    found = 0; transport = 0;
    for (i = 0; i < og_travel_count(); i++) {
        if (og_travel_location(i,&location) && location.region == region && location.index == index) { found = 1; break; }
    }
    if (!found || !og_travel_known(i)) { og_msg_set("That location is not on your map."); return 0; }
    transport = og_journey_transport();
    og_journey_plan(og_world_state()->px,og_world_state()->py,location.px,location.py,reckless,camp,ship,transport,&route);
    who = og_character();
    if (route.cost > who->gold) { og_msg_set("You do not have enough gold for that journey."); return 0; }
    if (!og_travel_go(region,index)) { return 0; }
    who->gold -= route.cost; fatigue = who->fatigue;
    og_state_pass_minutes(route.minutes);
    if (who->health <= 0) { og_msg_set("You did not survive the journey."); return 1; }
    if (!reckless) {
        who->health = who->max_health;
        if (!(og_state()->career.flags & 8)) { who->magicka = who->max_magicka; }
    }
    /* travel_begin_trip waits to 06:10 for cautious ordinary travellers,
     * or 18:10 for a daylight-damaged class. Unlike GFALL, not 07:10. */
    minute = og_state()->minutes%1440; wait = 0;
    if (og_state()->career.flags & 16) { if (minute >= 360 && minute < 1080) { wait = 1090-minute; } }
    else if (!reckless) { if (minute >= 1080) { wait = 1440-minute+370; } else if (minute < 360) { wait = 370-minute; } }
    if (wait) { og_state_pass_minutes(wait); }
    if (who->health > 0) { who->fatigue = reckless ? fatigue : (who->attribute[OG_STR]+who->attribute[OG_END])*64; }
    og_progress_rest(); return 1;
}
