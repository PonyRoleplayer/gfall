#include "game/dagger/play/journey.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/formats/worldmap.h"

/* See journey.h. TravelTimeCalculator's numbers. */

/* How each way of going slows the walk, per 256. */
static const int g_transport[3] = { 256, 128, 192 };
/* A climate past the ocean (its id less CLIMATE_OCEAN) to its terrain's row, and each row's ease (512 less it slows). */
static const int g_terrain_of_climate[10] = { 0, 0, 0, 1, 2, 3, 4, 5, 5, 5 };
static const int g_terrain_ease[6] = { 240, 220, 200, 200, 230, 250 };
#define SEA_BY_SHIP 51
#define SEA_SWIMMING 255
#define MINUTES_A_DAY 1440
/* Cautious arrival: no sooner than 7:10, no later than 17:00. */
#define FIRST_LIGHT (7.0f * 60.0f + 10.0f)
#define LAST_LIGHT (17.0f * 60.0f)

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

/* One pixel's minutes, before camping or recklessness. */
static int pixel_minutes(int climate, int by_ship, int transport, int *sea)
{
    int offset;
    int row;

    if (climate == CLIMATE_OCEAN) {
        (*sea)++;
        return by_ship ? SEA_BY_SHIP : SEA_SWIMMING;
    }
    offset = climate - CLIMATE_OCEAN;
    row = offset >= 0 && offset < 10 ? g_terrain_of_climate[offset] : 0;
    return (((102 * g_transport[transport]) >> 8) * (512 - g_terrain_ease[row])) >> 8;
}

void journey_plan(int sx, int sy, int ex, int ey, int reckless, int inns, int by_ship, int transport,
                  int (*climate_of)(int px, int py), Journey *out)
{
    int dx;
    int dy;
    int steps;
    int major_x;
    int minor;
    int x;
    int y;
    int i;
    int total;

    if (!climate_of) {
        climate_of = worldmap_climate;
    }
    transport = transport >= 0 && transport < 3 ? transport : JOURNEY_FOOT;
    dx = ex - sx;
    dy = ey - sy;
    steps = iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy);
    major_x = iabs(dx) == steps;
    minor = 0;
    x = sx;
    y = sy;
    total = 0;
    out->sea_pixels = 0;
    for (i = 0; i < steps; i++) {
        int m;

        /* The longer axis a pixel a step; the shorter when its share has passed a whole one. */
        if (major_x) {
            x += dx > 0 ? 1 : -1;
            minor += iabs(dy);
            if (minor > iabs(dx)) {
                minor -= iabs(dx);
                y += dy > 0 ? 1 : -1;
            }
        } else {
            y += dy > 0 ? 1 : -1;
            minor += iabs(dx);
            if (minor > iabs(dy)) {
                minor -= iabs(dy);
                x += dx > 0 ? 1 : -1;
            }
        }
        m = pixel_minutes(climate_of(x, y), by_ship, transport, &out->sea_pixels);
        if (!inns) {
            m = (300 * m) >> 8;
        }
        total += m;
    }
    if (reckless) {
        total >>= 1;
    }
    out->minutes = total;
    out->hours = (total + 59) / 60;
    out->days = total / MINUTES_A_DAY + (total % MINUTES_A_DAY ? 1 : 0);
    out->inn_gold = 0;
    if (inns) {
        /* Classic could make this negative over the sea; DFU stops it at none. The first night is always paid. */
        out->inn_gold = 5 * ((out->hours - out->sea_pixels) / 24);
        if (out->inn_gold < 0) {
            out->inn_gold = 0;
        }
        out->inn_gold += 5;
    }
    out->fare = by_ship && out->sea_pixels > 0 ? 25 * (out->sea_pixels / 24 + 1) : 0;
    out->cost = out->inn_gold + out->fare;
}

void journey_arrival(int day, float minute, int minutes, int cautious, int *arrive_day, float *arrive_minute)
{
    int whole;

    whole = (int)minute + minutes;
    day += whole / MINUTES_A_DAY;
    minute = (float)(whole % MINUTES_A_DAY) + (minute - (float)(int)minute);
    if (cautious) {
        if (minute < FIRST_LIGHT) {
            minute = FIRST_LIGHT;
        } else if (minute > LAST_LIGHT) {
            minute = FIRST_LIGHT;
            day++;
        }
    }
    *arrive_day = day;
    *arrive_minute = minute;
}
