#include "game/dagger/play/lodging.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/tables/lodgingdata.h"

/* See lodging.h. */

int lodging_valid(const LodgingState *l)
{
    int i;

    for (i = 0; i < ROOMS_MOST; i++) {
        if (l->room[i].building && l->room[i].room < 0) {
            return 0;
        }
    }
    return 1;
}

static RoomInfo *find(df_u32 building)
{
    int i;

    for (i = 0; i < ROOMS_MOST; i++) {
        if (building && lodging_state()->room[i].building == building) {
            return &lodging_state()->room[i];
        }
    }
    return 0;
}

int room_rented(df_u32 building, df_u32 now)
{
    const RoomInfo *r;

    r = find(building);
    return r && r->expires > now;
}

int room_days_left(df_u32 building, df_u32 now)
{
    const RoomInfo *r;

    r = find(building);
    if (!r || r->expires <= now) {
        return 0;
    }
    return (int)((r->expires - now + 1439u) / MINUTES_A_DAY);
}

int room_expired(df_u32 building, df_u32 now)
{
    const RoomInfo *r;

    r = find(building);
    return r && now > r->expires;
}

int room_offer(df_u32 building, int days, const ShopState *shop, int mercantile, int personality, int knight, df_u32 now, RoomOffer *out)
{
    int day, raw;

    out->days = days;
    out->price = 0;
    out->line = 0;
    out->text = 0;
    if (room_days_left(building, now) + days > ROOM_DAYS_MOST) {
        out->text = 16;
        return ROOM_REFUSED;
    }
    if (days == 0) {
        return ROOM_NOTHING;
    }
    day = (int)((now % 518400u) / MINUTES_A_DAY) + 1;
    /* The festival on day 46 (Heart's Day) takes a night off the bill. */
    raw = day <= 46 && day + days > 46 ? (days - 1) * ROOM_DAY_PRICE : days * ROOM_DAY_PRICE;
    if (knight) {
        out->line = g_msg_knight;
        return ROOM_FREE;
    }
    if (raw == 0) {
        out->line = g_msg_heart;
        return ROOM_FREE;
    }
    out->price = trade_adjust(shop, mercantile, personality, raw, 0);
    return ROOM_PRICED;
}

int room_take(df_u32 building, const RoomOffer *offer, int room, df_u32 now, int *text)
{
    RoomInfo *r;
    int i;

    *text = 0;
    if (offer->price > 0 && character_state()->gold < offer->price) {
        *text = BANK_TEXT_CANNOT_AFFORD;
        return 0;
    }
    r = find(building);
    if (!r) {
        for (i = 0; i < ROOMS_MOST && lodging_state()->room[i].building; i++) {
        }
        if (i == ROOMS_MOST) {
            return 0;
        }
        r = &lodging_state()->room[i];
    }
    character_state()->gold -= offer->price;
    if (r->building == building && r->expires > now) {
        /* tavern_extend_room: the days are added to what is held. */
        r->expires += (df_u32)offer->days * MINUTES_A_DAY;
    } else {
        /* tavern_rent: from now, the room the caller picked. */
        r->building = building;
        r->expires = now + (df_u32)offer->days * MINUTES_A_DAY;
        r->room = room;
    }
    return 1;
}

const char *food_name(int food)
{
    return food >= 0 && food < FOODS ? g_food_name[food] : "";
}

int meal_at(int food, int holiday, df_u32 now, const char **line, int *text)
{
    CharacterState *c;
    int price;

    *line = 0;
    *text = 0;
    if (food < 0 || food >= FOODS) {
        return 0;
    }
    if (now - lodging_state()->last_meal < MEAL_GAP) {
        *line = g_msg_not_hungry;
        return 0;
    }
    price = g_food_price[food];
    if (holiday == 37) {
        price >>= 1;
        if (price == 0) {
            price++;
        }
    }
    c = character_state();
    if (holiday != 1 && c->gold < price) {
        *text = BANK_TEXT_CANNOT_AFFORD;
        return 0;
    }
    if (holiday != 1) {
        c->gold -= price;
    }
    c->health += price * 2;
    if (c->health > c->max_health) {
        c->health = c->max_health;
    }
    lodging_state()->last_meal = now;
    return 1;
}

int rest_allowed(const RestSite *site, df_u32 now, int *to_room)
{
    *to_room = 0;
    if (site->ship_berth) {
        return REST_OK;
    }
    if (site->kind == SITE_TOWN) {
        if (site->location_kind == 4 || site->location_kind == 7 || site->location_kind > 9) {
            return REST_OK;
        }
        return REST_VAGRANT;
    }
    if (site->kind == SITE_INSIDE) {
        if (site->building_type == 15) {
            if (!room_rented(site->building, now)) {
                return REST_NO_ROOM;
            }
            *to_room = 1;
            return REST_OK;
        }
        if (site->building_faction == 41 && site->knight_member) {
            *to_room = 1;
            return REST_OK;
        }
        if (site->building != site->own_house) {
            return REST_TRESPASS;
        }
    }
    return REST_OK;
}

const char *no_room_line(void)
{
    return g_msg_no_room;
}

const char *room_expired_line(void)
{
    return g_msg_expired;
}
