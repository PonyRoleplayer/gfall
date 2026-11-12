#include "check_lodging.h"
#include "check_util.h"
#include "game/dagger/play/lodging.h"
#include "game/dagger/play/state.h"
#include <string.h>

/* A neutral shop (quality 10, normal region) and a buyer of 50 and 50: 7 gold of raw price is 7 * 144 / 256 = 3. */
void check_lodging(void)
{
    ShopState shop;
    RoomOffer o;
    const char *line;
    int text, to_room, i;
    RestSite site;
    const df_u32 tavern = 700, now0 = 0;

    shop.region_adjust = 1000; shop.quality = 10; shop.type = 15; shop.holiday = 0; shop.half = 0; shop.scale = 256;
    memset(lodging_state(), 0, sizeof(*lodging_state()));
    character_state()->gold = 100;
    character_state()->max_health = 50;
    character_state()->health = 10;

    /* An offer: days at 7, bought through the buyer's side; no region, quality base or scale. */
    check_expect(room_offer(tavern, 1, &shop, 50, 50, 0, now0, &o) == ROOM_PRICED && o.price == 3, "lodging: a day is not 7 raw, 3 to a neutral buyer");
    check_expect(room_offer(tavern, 3, &shop, 50, 50, 0, now0, &o) == ROOM_PRICED && o.price == 11, "lodging: three days are not 21 raw, 11");
    check_expect(room_offer(tavern, 0, &shop, 50, 50, 0, now0, &o) == ROOM_NOTHING, "lodging: no days had a price");
    /* A knight pays nothing, with the original's line. */
    check_expect(room_offer(tavern, 3, &shop, 50, 50, 1, now0, &o) == ROOM_FREE && o.line && strstr(o.line, "knight"), "lodging: a knight did not stay free");
    /* Heart's Day, day 46: one night's price off a stay across it, and a single night there nothing. */
    check_expect(room_offer(tavern, 2, &shop, 50, 50, 0, 44u * 1440u, &o) == ROOM_PRICED && o.price == 3, "lodging: a stay across day 46 did not lose a night");
    check_expect(room_offer(tavern, 1, &shop, 50, 50, 0, 45u * 1440u, &o) == ROOM_FREE && o.line && strstr(o.line, "Heart"), "lodging: the night of day 46 was not free");

    /* Renting: pay, a room until the days are out; asking again adds to it; a day is the rounded-up count. */
    room_offer(tavern, 3, &shop, 50, 50, 0, now0, &o);
    check_expect(room_take(tavern, &o, 2, now0, &text) && character_state()->gold == 100 - 11, "lodging: renting did not take the price");
    check_expect(room_rented(tavern, now0 + 1) && room_days_left(tavern, now0) == 3 && room_days_left(tavern, 1440u + 1u) == 2,
                 "lodging: three days rented do not read as 3, then 2 days left");
    room_offer(tavern, 2, &shop, 50, 50, 0, 1440u, &o);
    room_take(tavern, &o, 9, 1440u, &text);
    check_expect(room_days_left(tavern, 1440u) == 4 && lodging_state()->room[0].room == 2, "lodging: more days were not added to the room held");
    check_expect(!room_expired(tavern, 4u * 1440u) && room_expired(tavern, 5u * 1440u + 1u) && !room_rented(tavern, 5u * 1440u + 1u),
                 "lodging: the room did not run out after its days");
    /* A room kept to 350 days left is the most: a longer stay is refused with record 16. */
    room_offer(tavern, 300, &shop, 50, 50, 1, 6u * 1440u, &o);
    room_take(tavern, &o, 2, 6u * 1440u, &text);
    check_expect(room_offer(tavern, 60, &shop, 50, 50, 0, 6u * 1440u, &o) == ROOM_REFUSED && o.text == 16, "lodging: a stay past 350 days was not refused with 16");
    /* No gold: 454, and nothing taken. */
    character_state()->gold = 1;
    room_offer(700 + 1, 3, &shop, 50, 50, 0, now0, &o);
    check_expect(!room_take(700 + 1, &o, 1, now0, &text) && text == 454 && character_state()->gold == 1 && !room_rented(701, now0), "lodging: a room with no gold was not refused with 454");

    /* Food: price by the list, health twice that; refused inside 240 minutes with the original's line. */
    character_state()->gold = 10;
    lodging_state()->last_meal = 0;
    check_expect(meal_at(3, 0, 1000u, &line, &text) && character_state()->gold == 7 && character_state()->health == 16, "lodging: wine at 3 did not cost 3 and heal 6");
    check_expect(!meal_at(0, 0, 1100u, &line, &text) && line && strstr(line, "not hungry"), "lodging: a second meal inside 240 minutes was not refused");
    check_expect(meal_at(7, 37, 1300u, &line, &text) && character_state()->gold == 6 && character_state()->health == 18, "lodging: holiday 37 did not halve 3 to 1 and heal 2");
    check_expect(meal_at(0, 1, 2000u, &line, &text) && character_state()->gold == 6, "lodging: holiday 1 was not free");
    character_state()->gold = 0;
    check_expect(!meal_at(0, 0, 3000u, &line, &text) && text == 454, "lodging: a meal with no gold was not refused with 454");
    check_expect(!strcmp(food_name(0), "Ale (1 gold)") && !strcmp(food_name(10), "Stew (3 gold)"), "lodging: the food list");

    /* Where rest is allowed: wild and dungeon always; a town's streets only where its kind lets it; a tavern with a room, the knights' hall to a member, one's own house. */
    memset(&site, 0, sizeof(site));
    site.kind = SITE_WILD;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_OK && !to_room, "lodging: the wilds refused rest");
    site.kind = SITE_DUNGEON;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_OK, "lodging: a dungeon refused rest");
    site.kind = SITE_TOWN;
    site.location_kind = 5;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_VAGRANT, "lodging: sleeping in a town's streets was allowed");
    site.location_kind = 4;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_OK, "lodging: a location of kind 4 refused rest");
    site.kind = SITE_INSIDE;
    site.building_type = 15;
    site.building = 900;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_NO_ROOM, "lodging: a tavern with no room allowed rest");
    room_offer(900, 1, &shop, 50, 50, 1, now0, &o);
    room_take(900, &o, 3, now0, &text);
    check_expect(rest_allowed(&site, now0, &to_room) == REST_OK && to_room, "lodging: a rented room did not allow rest in the room");
    site.building_type = 3;
    site.building_faction = 41;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_TRESPASS, "lodging: another's building allowed rest");
    site.knight_member = 1;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_OK && to_room, "lodging: a knight was refused the order's room");
    site.knight_member = 0;
    site.own_house = 900;
    check_expect(rest_allowed(&site, now0, &to_room) == REST_OK && !to_room, "lodging: one's own house refused rest");
    for (i = 0; i < ROOMS_MOST; i++) {
        lodging_state()->room[i].building = 0;
    }
    check_expect(lodging_valid(lodging_state()), "lodging: the state is not valid after use");
}
