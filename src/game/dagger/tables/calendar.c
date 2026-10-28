#include "game/dagger/tables/calendar.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See calendar.h and docs/data.md. */

static const RecField g_month_fields[] = {
    REC_FIELD("id", REC_UINT, CalMonth, id),
    REC_FIELD("name", REC_TEXT, CalMonth, name),
    REC_FIELD("days", REC_UINT, CalMonth, days),
    REC_FIELD("season", REC_UINT, CalMonth, season)
};
const RecSchema cal_month_schema = {
    "CMON", "month", g_month_fields, 4, (df_u16)sizeof(CalMonth), 0
};

static const RecField g_weekday_fields[] = {
    REC_FIELD("id", REC_UINT, CalWeekday, id),
    REC_FIELD("name", REC_TEXT, CalWeekday, name)
};
const RecSchema cal_weekday_schema = {
    "CWEK", "weekday", g_weekday_fields, 2, (df_u16)sizeof(CalWeekday), 0
};

static const RecField g_info_fields[] = {
    REC_FIELD("id", REC_UINT, CalInfo, id),
    REC_FIELD("era", REC_TEXT, CalInfo, era),
    REC_FIELD("week_restarts", REC_UINT, CalInfo, week_restarts),
    REC_FIELD("start_year", REC_INT, CalInfo, start_year),
    REC_FIELD("start_month", REC_UINT, CalInfo, start_month),
    REC_FIELD("start_day", REC_UINT, CalInfo, start_day)
};
const RecSchema cal_info_schema = {
    "CALE", "calendar", g_info_fields, 6, (df_u16)sizeof(CalInfo), 0
};

static RecTable g_months;
static RecTable g_weekdays;
static RecTable g_info;

RecTable *cal_month_table(void)
{
    if (!g_months.schema) {
        rec_table_init(&g_months, &cal_month_schema, CLOCK_MONTHS_MAX);
    }
    return &g_months;
}

RecTable *cal_weekday_table(void)
{
    if (!g_weekdays.schema) {
        rec_table_init(&g_weekdays, &cal_weekday_schema, CLOCK_WEEKDAYS_MAX);
    }
    return &g_weekdays;
}

RecTable *cal_info_table(void)
{
    if (!g_info.schema) {
        rec_table_init(&g_info, &cal_info_schema, 1);
    }
    return &g_info;
}

int calendar_from_records(Calendar *out)
{
    const CalInfo *info;
    int i;

    mem_zero(out, sizeof(*out));
    info = (const CalInfo *)rec_row(cal_info_table(), 0);
    if (!info) {
        return 0;
    }
    for (i = 0; i < CLOCK_MONTHS_MAX; i++) {
        const CalMonth *m;

        m = (const CalMonth *)rec_row(cal_month_table(), i);
        if (!m) {
            break;
        }
        df_copy(out->month[i].name, m->name, CLOCK_NAME);
        out->month[i].days = m->days;
        out->month[i].season = m->season;
    }
    out->nmonths = i;
    for (i = 0; i < CLOCK_WEEKDAYS_MAX; i++) {
        const CalWeekday *w;

        w = (const CalWeekday *)rec_row(cal_weekday_table(), i);
        if (!w) {
            break;
        }
        df_copy(out->weekday[i], w->name, CLOCK_NAME);
    }
    out->nweekdays = i;
    out->week_restarts = info->week_restarts;
    df_copy(out->era, info->era, (int)sizeof(out->era));
    out->start_year = (int)info->start_year;
    out->start_month = (int)info->start_month;
    out->start_day = (int)info->start_day;
    return out->nmonths > 0;
}

/* Byte for byte: both calendars are zeroed before they are filled, padding too. */
static int same_bytes(const void *a, const void *b, df_u32 n)
{
    const df_u8 *x;
    const df_u8 *y;
    df_u32 i;

    x = (const df_u8 *)a;
    y = (const df_u8 *)b;
    for (i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return 0;
        }
    }
    return 1;
}

int calendar_apply(void)
{
    Calendar want;
    Calendar now;

    if (!calendar_from_records(&want)) {
        return 0;
    }
    mem_zero(&now, sizeof(now));
    clock_calendar(&now);
    if (same_bytes(&want, &now, sizeof(want))) {
        return 1;
    }
    if (!clock_set_calendar(&want)) {
        log_text(LOG_WARN, "data: the calendar's records break the clock's limits; Tamriel's stays");
        return 0;
    }
    return 1;
}
