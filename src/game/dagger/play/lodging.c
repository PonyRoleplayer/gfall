#include "game/dagger/play/lodging.h"
#include "game/dagger/tables/commerce.h"
#include "engine/core/mem.h"

/* See lodging.h and docs/game.md (Taverns and rest). */

#define MINUTES_A_DAY 1440
#define ROOM_PRICE_A_DAY 7
/* Heart's Day, the 16th of Sun's Dawn: classic's day of the year 46. */
#define HEARTS_DAY 46
/* Hungry again four game hours after the last meal (DFU's 240 minutes). */
#define MEAL_EVERY 240
/* The last meal's minute when none was had: long enough ago. */
#define NO_MEAL (-1000000)

static const RecField g_room_fields[] = {
    REC_FIELD("id", REC_UINT, LodgingRoom, id),
    REC_FIELD("region", REC_INT, LodgingRoom, region),
    REC_FIELD("index", REC_INT, LodgingRoom, index),
    REC_FIELD("block", REC_INT, LodgingRoom, block),
    REC_FIELD("record", REC_INT, LodgingRoom, record),
    REC_FIELD("expires", REC_INT, LodgingRoom, expires)
};
const RecSchema lodging_schema = {
    "ROOM", "room", g_room_fields, (int)(sizeof(g_room_fields) / sizeof(g_room_fields[0])), (df_u16)sizeof(LodgingRoom), 0
};

static LodgingRoom g_rooms[LODGING_ROOMS];
static int g_count;
static int g_last_meal = NO_MEAL;

/* DFU's tavern menu, in its order, and its prices. */
static const char *const g_food_names[LODGING_FOODS] = {
    "Ale", "Beer", "Mead", "Wine", "Bread", "Broth", "Cheese", "Fowl", "Gruel", "Pie", "Stew"
};
static const int g_food_prices[LODGING_FOODS] = { 1, 1, 2, 3, 1, 1, 2, 3, 2, 2, 3 };

void lodging_new_game(void)
{
    mem_zero(g_rooms, sizeof(g_rooms));
    g_count = 0;
    g_last_meal = NO_MEAL;
}

int lodging_count(void)
{
    return g_count;
}

const LodgingRoom *lodging_at(int i)
{
    return i >= 0 && i < g_count ? &g_rooms[i] : 0;
}

void lodging_put(const LodgingRoom *room)
{
    if (g_count < LODGING_ROOMS) {
        g_rooms[g_count] = *room;
        g_rooms[g_count].id = (df_u16)g_count;
        g_count++;
    }
}

/* The room held in that building, or -1. */
static int room_of(int region, int index, int block, int record)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_rooms[i].region == region && g_rooms[i].index == index && g_rooms[i].block == block
            && g_rooms[i].record == record) {
            return i;
        }
    }
    return -1;
}

int lodging_hours_left(int region, int index, int block, int record, int now)
{
    int i;

    i = room_of(region, index, block, record);
    if (i < 0 || g_rooms[i].expires <= now) {
        return 0;
    }
    /* A part of an hour left is an hour: a room with minutes on it can still be slept in. */
    return (g_rooms[i].expires - now + 59) / 60;
}

int lodging_room_cost(int days, int day_of_year)
{
    if (days <= 0) {
        return 0;
    }
    if (day_of_year <= HEARTS_DAY && day_of_year + days > HEARTS_DAY) {
        return ROOM_PRICE_A_DAY * (days - 1);
    }
    return ROOM_PRICE_A_DAY * days;
}

int lodging_rent(Player *p, int region, int index, int block, int record, int days, int price, int now)
{
    int i;
    int left;

    if (days < 1) {
        return LODGING_TOO_MANY_DAYS;
    }
    i = room_of(region, index, block, record);
    left = i >= 0 && g_rooms[i].expires > now ? (g_rooms[i].expires - now) / MINUTES_A_DAY : 0;
    if (days + left > LODGING_DAYS_MOST) {
        return LODGING_TOO_MANY_DAYS;
    }
    if (price > (int)p->pack.gold) {
        return LODGING_NOT_ENOUGH_GOLD;
    }
    if (i < 0) {
        /* A room run out elsewhere gives its place up first. */
        for (i = 0; i < g_count && g_rooms[i].expires > now; i++) {
        }
        if (i == g_count) {
            if (g_count == LODGING_ROOMS) {
                return LODGING_FULL;
            }
            g_count++;
        }
        g_rooms[i].id = (df_u16)i;
        g_rooms[i].region = region;
        g_rooms[i].index = index;
        g_rooms[i].block = block;
        g_rooms[i].record = record;
        g_rooms[i].expires = now;
    } else if (g_rooms[i].expires < now) {
        g_rooms[i].expires = now;
    }
    g_rooms[i].expires += days * MINUTES_A_DAY;
    p->pack.gold -= (df_u32)price;
    return LODGING_DONE;
}

const char *lodging_food_name(int i)
{
    return i >= 0 && i < LODGING_FOODS ? g_food_names[i] : "";
}

int lodging_food_price(int i)
{
    return i >= 0 && i < LODGING_FOODS ? g_food_prices[i] : 0;
}

int lodging_food_cost(int i, int holiday)
{
    int price;

    price = lodging_food_price(i);
    if (holiday & HOLIDAY_FREE_DRINK) {
        return 0;
    }
    if (holiday & HOLIDAY_FOOD_HALF) {
        price >>= 1;
        price = price < 1 ? 1 : price;
    }
    return price;
}

int lodging_eat(Player *p, int i, int now, int holiday)
{
    int cost;

    if (now - g_last_meal < MEAL_EVERY) {
        return LODGING_NOT_HUNGRY;
    }
    cost = lodging_food_cost(i, holiday);
    if (cost > (int)p->pack.gold) {
        return LODGING_NOT_ENOUGH_GOLD;
    }
    p->pack.gold -= (df_u32)cost;
    /* Twice the price in health (SetHealth(CurrentHealth + 2 x price)): the menu's when free, the halved one on Harvest's End. */
    p->sheet.health += 2 * lodging_food_cost(i, holiday & ~HOLIDAY_FREE_DRINK);
    p->sheet.health = p->sheet.health > p->sheet.health_max ? p->sheet.health_max : p->sheet.health;
    g_last_meal = now;
    return LODGING_DONE;
}

int lodging_last_meal(void)
{
    return g_last_meal;
}

void lodging_set_last_meal(int minute)
{
    g_last_meal = minute;
}
