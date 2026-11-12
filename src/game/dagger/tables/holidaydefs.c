#include "game/dagger/tables/holidaydefs.h"
#include "engine/core/df.h"
#include "game/dagger/tables/holidaydata.h"

int og_holiday_today(unsigned minutes, int region)
{
    int day, h;

    day = (int)((minutes % 518400u) / 1440u) + 1;
    if (day > 355) {
        return 0;
    }
    region++;
    for (h = 0; h < 53; h++) {
        if ((g_holiday_region[h] == 255 || g_holiday_region[h] == region) && g_holiday_day[h] == day) {
            return h + 1;
        }
    }
    return 0;
}
