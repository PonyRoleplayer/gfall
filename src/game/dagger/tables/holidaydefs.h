#ifndef GFALL_HOLIDAYDEFS_H
#define GFALL_HOLIDAYDEFS_H

/*
 * holiday_today (src/lifted/parse.c 1920): the holiday on a game minute in a region, the
 * original's number 1..53 (49 is the weaponsmiths', 29 a temple's, 43 free identification);
 * 0 for none. A year is 518400 minutes (360 days); the last five days hold none. A holiday
 * is for a region when its table byte is 255 or the region + 1.
 */
int holiday_today(unsigned minutes, int region);

#endif
