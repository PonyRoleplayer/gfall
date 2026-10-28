#ifndef DF_SWAPS_H
#define DF_SWAPS_H

/*
 * Which picture a town's or a house's face shows in its climate and
 * season, and which pictures are windows: Daggerfall Unity's
 * ClimateSwaps, rule for rule. Read docs/daylight.md (Seasons, Windows).
 *
 * A model names its pictures in whatever climate it was drawn in; the
 * set is the archive's last two digits. The place's climate family
 * (TEX_*) replaces the hundreds, and winter adds 1 to an outside set
 * that has a snowy one (Castle 9 to 10, Farm 26 to 27). Inside sets
 * take the family and never snow. Anything else stays as named.
 */

/*
 * The archive face (archive, record) shows in family (TEX_*), in winter
 * when winter is 1. archive itself when there is no swap, or when the
 * swapped archive has no such record (count is texture_record_count).
 */
int swap_climate(int archive, int record, int family, int winter, int (*count)(int file));

/* 1 for a picture with window glass (index MIP_GLASS) that is lit at night. */
int swap_window(int archive, int record);

#endif
