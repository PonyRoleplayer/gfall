#include "game/ogfall/play/lodging.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/bank.h"
#include "game/ogfall/tables/lodgingdata.h"

/* See lodging.h. */

int og_lodging_valid(const OgLodging *l)
{
    int i;

    for (i = 0; i < OG_ROOMS_MOST; i++) {
        if (l->room[i].building && l->room[i].room < 0) {
            return 0;
        }
    }
    return 1;
}

static OgRoom *find(df_u32 building)
{
    int i;

    for (i = 0; i < OG_ROOMS_MOST; i++) {
        if (building && og_lodging()->room[i].building == building) {
            return &og_lodging()->room[i];
        }
    }
    return 0;
}

int og_room_rented(df_u32 building, df_u32 now)
{
    const OgRoom *r;

    r = find(building);
    return r && r->expires > now;
}

int og_room_days_left(df_u32 building, df_u32 now)
{
    const OgRoom *r;

    r = find(building);
    if (!r || r->expires <= now) {
        return 0;
    }
    return (int)((r->expires - now + 1439u) / OG_MINUTES_A_DAY);
}

int og_room_expired(df_u32 building, df_u32 now)
{
    const OgRoom *r;

    r = find(building);
    return r && now > r->expires;
}

int og_room_offer(df_u32 building, int days, const OgShop *shop, int mercantile, int personality, int knight, df_u32 now, OgRoomOffer *out)
{
    int day, raw;

    out->days = days;
    out->price = 0;
    out->line = 0;
    out->text = 0;
    if (og_room_days_left(building, now) + days > OG_ROOM_DAYS_MOST) {
        out->text = 16;
        return OG_ROOM_REFUSED;
    }
    if (days == 0) {
        return OG_ROOM_NOTHING;
    }
    day = (int)((now % 518400u) / OG_MINUTES_A_DAY) + 1;
    /* The festival on day 46 (Heart's Day) takes a night off the bill. */
    raw = day <= 46 && day + days > 46 ? (days - 1) * OG_ROOM_DAY_PRICE : days * OG_ROOM_DAY_PRICE;
    if (knight) {
        out->line = g_msg_knight;
        return OG_ROOM_FREE;
    }
    if (raw == 0) {
        out->line = g_msg_heart;
        return OG_ROOM_FREE;
    }
    out->price = og_trade_adjust(shop, mercantile, personality, raw, 0);
    return OG_ROOM_PRICED;
}

int og_room_take(df_u32 building, const OgRoomOffer *offer, int room, df_u32 now, int *text)
{
    OgRoom *r;
    int i;

    *text = 0;
    if (offer->price > 0 && og_character()->gold < offer->price) {
        *text = OG_BANK_TEXT_CANNOT_AFFORD;
        return 0;
    }
    r = find(building);
    if (!r) {
        for (i = 0; i < OG_ROOMS_MOST && og_lodging()->room[i].building; i++) {
        }
        if (i == OG_ROOMS_MOST) {
            return 0;
        }
        r = &og_lodging()->room[i];
    }
    og_character()->gold -= offer->price;
    if (r->building == building && r->expires > now) {
        /* tavern_extend_room: the days are added to what is held. */
        r->expires += (df_u32)offer->days * OG_MINUTES_A_DAY;
    } else {
        /* tavern_rent: from now, the room the caller picked. */
        r->building = building;
        r->expires = now + (df_u32)offer->days * OG_MINUTES_A_DAY;
        r->room = room;
    }
    return 1;
}

const char *og_food_name(int food)
{
    return food >= 0 && food < OG_FOODS ? g_food_name[food] : "";
}

int og_meal(int food, int holiday, df_u32 now, const char **line, int *text)
{
    OgCharacter *c;
    int price;

    *line = 0;
    *text = 0;
    if (food < 0 || food >= OG_FOODS) {
        return 0;
    }
    if (now - og_lodging()->last_meal < OG_MEAL_GAP) {
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
    c = og_character();
    if (holiday != 1 && c->gold < price) {
        *text = OG_BANK_TEXT_CANNOT_AFFORD;
        return 0;
    }
    if (holiday != 1) {
        c->gold -= price;
    }
    c->health += price * 2;
    if (c->health > c->max_health) {
        c->health = c->max_health;
    }
    og_lodging()->last_meal = now;
    return 1;
}

int og_rest_allowed(const OgRestSite *site, df_u32 now, int *to_room)
{
    *to_room = 0;
    if (site->ship_berth) {
        return OG_REST_OK;
    }
    if (site->kind == OG_SITE_TOWN) {
        if (site->location_kind == 4 || site->location_kind == 7 || site->location_kind > 9) {
            return OG_REST_OK;
        }
        return OG_REST_VAGRANT;
    }
    if (site->kind == OG_SITE_INSIDE) {
        if (site->building_type == 15) {
            if (!og_room_rented(site->building, now)) {
                return OG_REST_NO_ROOM;
            }
            *to_room = 1;
            return OG_REST_OK;
        }
        if (site->building_faction == 41 && site->knight_member) {
            *to_room = 1;
            return OG_REST_OK;
        }
        if (site->building != site->own_house) {
            return OG_REST_TRESPASS;
        }
    }
    return OG_REST_OK;
}

const char *og_no_room_line(void)
{
    return g_msg_no_room;
}

const char *og_room_expired_line(void)
{
    return g_msg_expired;
}
