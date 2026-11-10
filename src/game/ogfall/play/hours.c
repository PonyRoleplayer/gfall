#include "game/ogfall/play/hours.h"
#include "game/ogfall/tables/hoursdata.h"

/* See hours.h. */

int og_building_open(int type, int faction, df_u32 id, df_u32 now, int daytime, const OgHoursPlayer *player)
{
    int hour;

    if (faction == 108 && player->brotherhood >= 0) { return 1; }
    if (faction == 42 && player->thieves >= 0) { return 1; }
    if (faction == 41 && player->fighters >= 6) { return 1; }
    if (faction == 40 && player->mages >= 6) { return 1; }
    if (type >= 17 && type <= 20 && daytime && (id & 65535u) % 100u < 50u) { return 1; }
    if (type < 0 || type > 16) { return 0; }
    hour = (int)((now % 1440u) / 60u);
    return g_open_hour[type] <= hour && g_close_hour[type] > hour;
}

int og_building_minutes_to_close(int type, df_u32 now)
{
    int hour;

    if (type < 0 || type > 16) { return 0; }
    hour = (int)((now % 1440u) / 60u);
    if (g_open_hour[type] <= hour && g_close_hour[type] > hour) {
        return g_close_hour[type] * 60 - (int)(now % 1440u);
    }
    return 0;
}
