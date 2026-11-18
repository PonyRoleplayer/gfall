#include "game/dagger/play/people/hours.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/tables/hoursdata.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/character/rest.h"

/* See hours.h. */

int building_open(int type, int faction, df_u32 id, df_u32 now, int daytime, const HoursPlayer *player)
{
    int hour;

    if (faction == 108 && player->brotherhood >= 0) { return 1; }
    if (faction == 42 && player->thieves >= 0) { return 1; }
    if (faction == 41 && player->fighters >= 6) { return 1; }
    if (faction == 40 && player->mages >= 6) { return 1; }
    if (type >= BUILDING_HOUSE1 && type <= BUILDING_HOUSE4 && daytime && (id & 65535u) % 100u < 50u) { return 1; }
    if (type < 0 || type > 16) { return 0; }
    hour = (int)((now % MINUTES_PER_DAY) / MINUTES_PER_HOUR);
    return g_open_hour[type] <= hour && g_close_hour[type] > hour;
}

int building_minutes_to_close(int type, df_u32 now)
{
    int hour;

    if (type < 0 || type > 16) { return 0; }
    hour = (int)((now % MINUTES_PER_DAY) / MINUTES_PER_HOUR);
    if (g_open_hour[type] <= hour && g_close_hour[type] > hour) {
        return g_close_hour[type] * MINUTES_PER_HOUR - (int)(now % MINUTES_PER_DAY);
    }
    return 0;
}

int building_hours(int type, int *open, int *close)
{
    if (type < 0 || type > 16) { return 0; }
    *open = g_open_hour[type]; *close = g_close_hour[type];
    return 1;
}
