#ifndef DF_JOURNEY_H
#define DF_JOURNEY_H

/*
 * A fast journey's sums, Daggerfall Unity's TravelTimeCalculator: what the walk
 * from one map pixel to another costs in time and gold. Read docs/game.md (The
 * wider world) before editing.
 *
 * The path is a line of map pixels (one a step along the longer axis). Each land
 * pixel costs minutes by its climate's terrain and how the player goes; a sea
 * pixel 51 by ship, 255 swimming it. Camping out costs 300/256 of that; going
 * recklessly halves the whole. An inn is 5 gold a night of land, the first
 * night always; a ship's fare 25 a day at sea and one more, for one without a
 * ship of their own.
 *
 * Plain C: hostcheck works it by hand.
 */

enum {
    JOURNEY_FOOT,
    JOURNEY_HORSE,
    JOURNEY_CART
};

typedef struct Journey {
    int minutes;
    int sea_pixels;
    int hours;
    /* Whole days, a part counted as one: what the map's popup shows. */
    int days;
    int inn_gold;
    int fare;
    int cost;
} Journey;

/*
 * From pixel (sx, sy) to (ex, ey): reckless or cautious, at inns or camping,
 * by ship over the sea or not, on foot, horse or cart (JOURNEY_*). climate_of
 * gives a pixel's climate (worldmap_climate); 0 uses the world map's.
 */
void journey_plan(int sx, int sy, int ex, int ey, int reckless, int inns, int by_ship, int transport,
                  int (*climate_of)(int px, int py), Journey *out);
/* A voyage on the player's own ship: no fare (TravelTimeCalculator.CalculateTripCost's hasShip). */
void journey_own_ship(Journey *j);
/*
 * When one sets out on day, at minute of the day, and journeys this long: the
 * day and minute one arrives. A cautious traveller rests on the way and arrives
 * in daylight: before 7:10 waits for it, past 17:00 waits for the next morning's.
 */
void journey_arrival(int day, float minute, int minutes, int cautious, int *arrive_day, float *arrive_minute);

#endif
