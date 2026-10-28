#ifndef DF_CALENDAR_H
#define DF_CALENDAR_H

#include "engine/assets/recfile.h"
#include "engine/scene/clock.h"

/*
 * Tamriel's calendar as records (docs/data.md), which the game installs
 * in the engine's clock (clock_set_calendar). A record table holds no
 * array of text, so the calendar is three tables:
 *
 *   CMON: a month, by its index from 0: name, days, season (CLOCK_*)
 *   CWEK: a weekday, by its index from 0: name
 *   CALE: one record, id 0: era, whether each month restarts the week,
 *         the date a new game starts on
 *
 * Months and weekdays run from id 0 up to the first id missing.
 */

typedef struct CalMonth {
    df_u16 id;
    char name[CLOCK_NAME];
    df_u16 days;
    df_u8 season;
} CalMonth;

typedef struct CalWeekday {
    df_u16 id;
    char name[CLOCK_NAME];
} CalWeekday;

typedef struct CalInfo {
    df_u16 id;
    char era[8];
    df_u8 week_restarts;
    df_i32 start_year;
    df_u16 start_month;
    df_u16 start_day;
} CalInfo;

extern const RecSchema cal_month_schema;
extern const RecSchema cal_weekday_schema;
extern const RecSchema cal_info_schema;
RecTable *cal_month_table(void);
RecTable *cal_weekday_table(void);
RecTable *cal_info_table(void);

/* The merged records as an engine Calendar, zeroed first. 0 when CALE has no record 0 or no month. */
int calendar_from_records(Calendar *out);
/*
 * Installs that calendar when it differs from the one in use. An
 * unchanged calendar leaves the clock where it is (a viewer's date from
 * its cfg stays); a different one moves it to its start date. 0 when
 * the records make no calendar the engine takes; the default stays.
 */
int calendar_apply(void);

#endif
