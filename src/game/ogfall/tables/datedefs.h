#ifndef OGFALL_DATEDEFS_H
#define OGFALL_DATEDEFS_H

/*
 * macro_dat_date (src/lifted/parse.c 448): a game minute as "Sundas the 3rd of Morning Star". The year
 * is 360 days of twelve months of thirty; the weekday is the day of the year mod 7 (the original's own
 * reckoning, not the month's); the ordinal is st, nd, rd for the 1st to 3rd and th after.
 * Returns the length written.
 */
int og_date_text(unsigned minutes, char *out, int cap);

#endif
