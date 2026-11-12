#include "game/dagger/tables/datedefs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "game/dagger/tables/datedata.h"

int og_date_text(unsigned minutes, char *out, int cap)
{
    int day_of_year, month, day;

    day_of_year = (int)((minutes % 518400u) / 1440u);
    month = day_of_year / 30;
    day = day_of_year % 30;
    return log_format(out, cap, "%s the %d%s of %s", g_day_name[day_of_year % 7], day + 1, g_ordinal[day > 3 ? 3 : day], g_month_name[month]);
}

const char *og_legal_title(int reputation)
{
    int i;

    reputation = reputation < -100 ? -100 : reputation > 100 ? 100 : reputation;
    for (i = 0; i < 13; i++) {
        if (reputation >= g_legal_range[i][0] && reputation <= g_legal_range[i][1]) { return g_legal_name[i]; }
    }
    return g_legal_name[6];
}
