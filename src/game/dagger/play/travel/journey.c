#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/character/rest.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/play/character/vitals.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/character/progress.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/tables/traveldefs.h"
#include "engine/core/mem.h"

/*
 * A journey (travel_pixel_time, travel_route, travel_trip_cost; inline in the decomp). Each pixel of land takes (TRAVEL_TERRAIN_SPAN - the terrain's modifier) times the transport's factor
 * (TRANSPORT_FACTOR_* of 256, TRAVEL_PIXEL_BASE / 256 of it) over 256; a pixel of sea TRAVEL_SEA_SHIP by ship, TRAVEL_SEA_NO_SHIP not. Camping takes TRAVEL_CAMP_NUM / TRAVEL_CAMP_DEN of the time,
 * a reckless trip halves it (TRAVEL_RECKLESS_DIV). An inn is INN_NIGHT_PRICE a night (an inn night is HOURS_PER_DAY hours less the sea) and a ship SHIP_DAY_PRICE a day of sea. Ours: a trip
 * by day for a sun-hurt class waits to SUN_ARRIVAL_MINUTE, and a cautious traveller to MORNING_ARRIVAL_MINUTE (06:10).
 */
#define TRANSPORT_HORSE 1
#define TRANSPORT_CART 2
#define TRANSPORT_FACTOR_HORSE 128
#define TRANSPORT_FACTOR_CART 192
#define TRANSPORT_FACTOR_FOOT 256
#define TRAVEL_PIXEL_BASE 102
#define TRAVEL_TERRAIN_SPAN 512
#define TRAVEL_SEA_SHIP 51
#define TRAVEL_SEA_NO_SHIP 255
#define TRAVEL_CAMP_NUM 300
#define TRAVEL_CAMP_DEN 256
#define TRAVEL_RECKLESS_DIV 2
#define INN_NIGHT_PRICE 5
#define HOURS_PER_DAY 24
#define SHIP_DAY_PRICE 25
#define SUN_ARRIVAL_MINUTE 1090
#define MORNING_ARRIVAL_MINUTE 370

static int absolute(int n) { return n < 0 ? -n : n; }
void journey_plan(int sx, int sy, int ex, int ey, int reckless, int camp,
                     int ship, int transport, JourneyPlan *out)
{
    int dx, dy, adx, ady, n, i, error, climate, minutes, factor, knight;
    mem_zero(out,sizeof(*out)); dx = ex-sx; dy = ey-sy;
    adx = absolute(dx); ady = absolute(dy); n = adx > ady ? adx : ady;
    error = 0; factor = transport == TRANSPORT_HORSE ? TRANSPORT_FACTOR_HORSE : transport == TRANSPORT_CART ? TRANSPORT_FACTOR_CART : TRANSPORT_FACTOR_FOOT;
    /* func_0009CEC4: the minor axis advances only after a strict >. */
    for (i = 0; i < n; i++) {
        if (n == adx) { sx += dx < 0 ? -1 : 1; error += ady; if (error > adx) { error -= adx; sy += dy < 0 ? -1 : 1; } }
        else { sy += dy < 0 ? -1 : 1; error += adx; if (error > ady) { error -= ady; sx += dx < 0 ? -1 : 1; } }
        climate = travel_climate(sx,sy);
        if (climate == CLIMATE_OCEAN) { out->sea_pixels++; minutes = ship || bank_state()->ship ? TRAVEL_SEA_SHIP : TRAVEL_SEA_NO_SHIP; /* an owned ship sails at 51 whatever the choice (travel_pixel_time) */ }
        else { minutes = ((TRAVEL_TERRAIN_SPAN-travel_terrain(climate))*((factor*TRAVEL_PIXEL_BASE)/256))/256; }
        if (camp) { minutes = minutes*TRAVEL_CAMP_NUM/TRAVEL_CAMP_DEN; }
        out->minutes += minutes;
    }
    /* travel_open_trip/toggle_option halve the cached estimate too,
     * before travel_trip_cost prices its nights. */
    if (reckless) { out->minutes /= TRAVEL_RECKLESS_DIV; }
    out->hours = (out->minutes+MINUTES_PER_HOUR-1)/MINUTES_PER_HOUR;
    /* travel_trip_cost: a knight of any order (membership bit KIND_KNIGHT) pays neither the inns nor the ship. */
    knight = guild_find_bits(KIND_KNIGHT) != 0;
    out->cost = camp || knight ? 0 : ((out->hours-out->sea_pixels)/HOURS_PER_DAY)*INN_NIGHT_PRICE+INN_NIGHT_PRICE;
    if (ship && out->sea_pixels && !knight && !bank_state()->ship) { /* an owned ship pays no fare */ out->cost += (out->sea_pixels/HOURS_PER_DAY+1)*SHIP_DAY_PRICE; }
}
int journey_transport(void)
{
    int i, transport;
    transport = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *item;
        item = &game_state()->object[i];
        if (item->id && item->type == RECORD_ITEM && item->parent == game_state()->inventory_id && item->group == ITEM_GROUP_TRANSPORT) {
            if (item->index == 1) { transport = TRANSPORT_HORSE; } else if (item->index == 0) { transport = TRANSPORT_CART; }
        }
    }
    return transport;
}
int journey_warning(void)
{
    KeepKey player;
    int i;
    player = magic_key(-1);
    /* health_status_add: latent ordinary disease warns too, poison only
     * after onset. Creature ailments must not warn for the traveller. */
    for (i = 0; i < AILMENTS; i++) {
        const AilmentInfo *a;
        a = &magic_state()->ailment[i];
        if (a->active && a->target.place == player.place && a->target.thing == player.thing &&
            (a->id < DISEASE_CURSE_FIRST || (a->id >= POISON_FIRST && a->stage))) { return 1; }
    }
    return 0;
}
int teleport(int region, int index)
{
    LocationInfo location;
    int i, found;

    if (!game_state()->ready || character_state()->health <= 0) { return 0; }
    found = 0;
    for (i = 0; i < travel_count(); i++) {
        if (travel_location(i,&location) && location.region == region && location.index == index) { found = 1; break; }
    }
    if (!found || !travel_known(i)) { msg_set("That location is not on your map."); return 0; }
    /* travel_begin_trip with the map's mode 100: neither the clock, nor the gold, nor the rest of a journey; only the arrival. */
    return travel_go(region,index);
}

int journey_begin(int region, int index, int reckless, int camp, int ship)
{
    JourneyPlan route;
    LocationInfo location;
    CharacterState *who;
    int i, found, transport, fatigue, minute, wait;
    if (world_where()->where.kind != WHERE_WORLD || !game_state()->ready || character_state()->health <= 0) {
        msg_set("You must be outside to travel."); return 0;
    }
    found = 0; transport = 0;
    for (i = 0; i < travel_count(); i++) {
        if (travel_location(i,&location) && location.region == region && location.index == index) { found = 1; break; }
    }
    if (!found || !travel_known(i)) { msg_set("That location is not on your map."); return 0; }
    transport = journey_transport();
    journey_plan(world_where()->px,world_where()->py,location.px,location.py,reckless,camp,ship,transport,&route);
    who = character_state();
    /* func_0009C3E7.c:61..64: a trip that cannot be paid takes all the gold and goes; it is not refused. */
    if (route.cost > who->gold) { route.cost = who->gold; }
    if (!travel_go(region,index)) { return 0; }
    who->gold -= route.cost; fatigue = who->fatigue;
    state_pass_minutes(route.minutes);
    if (who->health <= 0) { msg_set("You did not survive the journey."); return 1; }
    if (!reckless) {
        who->health = who->max_health;
        if (!(game_state()->career.flags & CLASS_NO_MAGICKA_REGEN)) { who->magicka = who->max_magicka; }
    }
    /* travel_begin_trip waits to 06:10 for cautious ordinary travellers,
     * or 18:10 for a daylight-damaged class. Unlike GFALL, not 07:10. */
    minute = game_state()->minutes%MINUTES_PER_DAY; wait = 0;
    if (game_state()->career.flags & CLASS_SUN_DAMAGE) { if (minute >= DAWN_MINUTE && minute < DUSK_MINUTE) { wait = SUN_ARRIVAL_MINUTE-minute; } }
    else if (!reckless) { if (minute >= DUSK_MINUTE) { wait = MINUTES_PER_DAY-minute+MORNING_ARRIVAL_MINUTE; } else if (minute < DAWN_MINUTE) { wait = MORNING_ARRIVAL_MINUTE-minute; } }
    if (wait) { state_pass_minutes(wait); }
    if (who->health > 0) { who->fatigue = reckless ? fatigue : fatigue_maximum(who->attribute[ATTR_STR],who->attribute[ATTR_END]); }
    progress_rest(); return 1;
}
