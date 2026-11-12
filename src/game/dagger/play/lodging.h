#ifndef GFALL_LODGING_H
#define GFALL_LODGING_H

#include "engine/core/df.h"
#include "game/dagger/play/trade.h"

/*
 * Taverns and where the player may sleep. Slice 4 of people and law (docs/gfall-people.md). The
 * rules are the decomp's: tavern_room_offer, tavern_room_pay, tavern_rent, tavern_extend_room,
 * tavern_room_rented, tavern_room_days_left, tavern_buy_food, tavern_food_button (src/lifted/tavern.c)
 * and rest_allowed, rest_room_expired (src/lifted/rest.c 197..244), with the notes in
 * docs/economy-math.md ("Tavern"). Pure rules over the session; the screens come with talk.
 *
 * A room is the building's: it belongs to the tavern until it runs out, is rented by the day (7 gold
 * a day at a neutral shop), and the first night is dearer than the rest when the festival of Heart's
 * Day falls inside it (a free night).
 */

#define ROOMS_MOST 16
/* A room can be kept up to a year less a fortnight (the original refuses past 350 days left). */
#define ROOM_DAYS_MOST 350
#define ROOM_DAY_PRICE 7
#define MINUTES_A_DAY 1440u
/* Hunger: a meal is refused for 240 minutes after the last. */
#define MEAL_GAP 240u
#define FOODS 11

typedef struct RoomInfo {
    df_u32 building, expires;
    int room;
} RoomInfo;

typedef struct LodgingState {
    RoomInfo room[ROOMS_MOST];
    df_u32 last_meal;
} LodgingState;

LodgingState *lodging_state(void);
int lodging_valid(const LodgingState *l);

/* tavern_room_rented: a room of this building that has not yet run out. */
int room_rented(df_u32 building, df_u32 now);
/* tavern_room_days_left: whole days, rounded up; 0 for none. */
int room_days_left(df_u32 building, df_u32 now);
/* rest_room_expired: a rented room's time is up (the player is woken and put out). */
int room_expired(df_u32 building, df_u32 now);

/* What asking for a room comes to. */
enum { ROOM_NOTHING, ROOM_REFUSED, ROOM_FREE, ROOM_PRICED };
typedef struct RoomOffer {
    int days, price;
    /* The original's own line for a free room; the TEXT.RSC record (16) for a refusal. */
    const char *line;
    int text;
} RoomOffer;

/*
 * tavern_room_offer: days asked of this building at a shop's prices. Past 350 days kept it is refused
 * (record 16); a local knight pays nothing, and neither does a stay with nothing to pay (the day of
 * Heart's Day); else the price is days * 7 (one day less when the stay spans day 46) through the
 * buyer's side of the adjustment, with no region, base or scale term.
 */
int room_offer(df_u32 building, int days, const ShopState *shop, int mercantile, int personality, int knight, df_u32 now, RoomOffer *out);
/*
 * Take the room: pay out.price (the gold alone; 454 when short, 0 returned) and rent it, or add the
 * days to the room already held. room is the marker's number in the building's interior, which the
 * caller picks. 0 when the table of rooms is full.
 */
int room_take(df_u32 building, const RoomOffer *offer, int room, df_u32 now, int *text);

/*
 * tavern_buy_food: a meal's price (the festival of Sun's Rest, holiday 37, halves it, at least 1;
 * holiday 1 is free), taken from the gold, and health gains twice the price up to its most. A meal inside
 * 240 minutes of the last is refused with the original's line. 1 when eaten.
 */
int meal_at(int food, int holiday, df_u32 now, const char **line, int *text);
const char *food_name(int food);

/* Where a bed is allowed (rest_allowed). The seam says where the player is; the rule says whether they may sleep. */
enum { SITE_WILD, SITE_TOWN, SITE_INSIDE, SITE_DUNGEON };
typedef struct RestSite {
    int kind;
    /* In a town's streets: the location's kind (4, 7 and over 9 are no town: its people leave a sleeper be). */
    int location_kind;
    /* Inside a building: its type (15 a tavern), its faction (41, the knights' order, whose members may use its rooms), its id and the player's own house. */
    int building_type, building_faction;
    df_u32 building, own_house;
    int knight_member;
    /* A ship owned and the place's id under 1000 (a port at sea): always welcome. */
    int ship_berth;
} RestSite;

enum {
    REST_OK,
    /* In a town's streets: crime 8 (vagrancy), the watch is called, record 17 is said. */
    REST_VAGRANT,
    /* A tavern with no room rented: the original's line. */
    REST_NO_ROOM,
    /* Another's house: crime 8, the watch is called. */
    REST_TRESPASS
};
/* *to_room says the player is moved to their room (a tavern's, or the knights'); crime ids belong to the law slice. */
int rest_allowed(const RestSite *site, df_u32 now, int *to_room);
const char *no_room_line(void);
/* The line a rest in an expired room is woken with (rest_update's D_001760B1). */
const char *room_expired_line(void);

#endif
