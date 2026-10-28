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

/*
 * A dungeon's pictures, as classic builds its table from the location id
 * and the climate: the model keys 119, 120, 122, 123 and 124 each draw
 * an offset 0..4 (a draw of 2 becomes 4: archive numbers skip a slot)
 * onto the climate's crypt set (family + 19), by Watcom's rand seeded
 * with the location id. Doors (records 0..2), sewers and caves take the
 * climate's family. One table a dungeon: the whole load and its stream
 * paint from the same.
 */
typedef struct SwapDungeon {
    int family;
    int arch[5];
} SwapDungeon;

/* The table for a dungeon of climate id (223..232) and location id; 0 for no climate or id. */
int swap_dungeon_init(SwapDungeon *d, int climate, int location_id);
/* The archive face (archive, record) shows in that dungeon; count is texture_record_count. */
int swap_dungeon(const SwapDungeon *d, int archive, int record, int (*count)(int file));

#endif
