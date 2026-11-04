#ifndef DF_LODGING_H
#define DF_LODGING_H

#include "game/dagger/play/player.h"
#include "engine/assets/recfile.h"

/*
 * Taverns: a room rented by the day, and food and drink. Read docs/game.md
 * (People, Taverns and rest) before editing.
 *
 * Daggerfall Unity's DaggerfallTavernWindow and FormulaHelper.CalculateRoomCost:
 * a room is 7 gold a day, Heart's Day free, then bargained by the tavern's
 * quality (rules_trade_price); at most 350 days ahead. Renting again where a
 * room is held adds days to it. The room is where rest is allowed in a town
 * (rest.h); when its time runs out, rest there ends.
 *
 * Food and drink: eleven things at 1 to 3 gold, each giving back twice its
 * price in health, once in four game hours. Free on the New Life Festival,
 * half (at least 1) on Harvest's End, as DFU found classic does.
 *
 * The rooms are the game's, saved as rows (ROOM). Plain C: hostcheck runs it.
 */

#define LODGING_ROOMS 32
#define LODGING_DAYS_MOST 350
#define LODGING_FOODS 11

/* A room held: the building by its town and its block and record, and the game minute it runs out (ills_now's count). */
typedef struct LodgingRoom {
    df_u16 id;
    df_i32 region;
    df_i32 index;
    df_i32 block;
    df_i32 record;
    df_i32 expires;
} LodgingRoom;

extern const RecSchema lodging_schema;

void lodging_new_game(void);
int lodging_count(void);
const LodgingRoom *lodging_at(int i);
/* A saved room, back. */
void lodging_put(const LodgingRoom *room);

/* Hours left on the room in that building at minute now; 0 when none is held or it ran out. */
int lodging_hours_left(int region, int index, int block, int record, int now);
/*
 * The gold for days from day_of_year (1..360) on, before bargaining: 7 a day,
 * Heart's Day (46) not counted when the days take it in.
 */
int lodging_room_cost(int days, int day_of_year);
/* What came of renting. */
enum {
    LODGING_DONE,
    LODGING_TOO_MANY_DAYS,
    LODGING_NOT_ENOUGH_GOLD,
    LODGING_FULL,
    LODGING_NOT_HUNGRY
};
/* Rents (or adds days to) the room here for price gold, already bargained, at minute now. */
int lodging_rent(Player *p, int region, int index, int block, int record, int days, int price, int now);

/* The menu's names and prices. */
const char *lodging_food_name(int i);
int lodging_food_price(int i);
/* Eats or drinks thing i at minute now; holiday is today's HOLIDAY_* flags. */
int lodging_eat(Player *p, int i, int now, int holiday);
/* The price thing i costs today. */
int lodging_food_cost(int i, int holiday);
/* The minute the player last ate or drank, for the save; set back on loading. */
int lodging_last_meal(void);
void lodging_set_last_meal(int minute);

#endif
