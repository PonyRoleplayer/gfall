#include "game/ogfall/tables/datedefs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "game/ogfall/tables/datedata.h"

int og_date_text(unsigned minutes, char *out, int cap)
{
    int day_of_year, month, day;

    day_of_year = (int)((minutes % 518400u) / 1440u);
    month = day_of_year / 30;
    day = day_of_year % 30;
    return log_format(out, cap, "%s the %d%s of %s", g_day_name[day_of_year % 7], day + 1, g_ordinal[day > 3 ? 3 : day], g_month_name[month]);
}
