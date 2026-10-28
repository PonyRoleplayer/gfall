#include "game/dagger/tables/swaps.h"
#include "game/dagger/formats/climate.h"

/* See swaps.h. Set numbers are DFLocation.ClimateTextureSet's. */

#define SET_TERRAIN 2
#define SET_CASTLE 9
#define SET_MAGES 35
#define SET_DOORS 74
#define SET_MARBLE 41

/* An outside set with a snowy one after it (GetClimateTextureInfo: supportsWinter). */
static int outside_set(int set)
{
    switch (set) {
    case SET_TERRAIN:
    case 7:     /* ruins */
    case SET_CASTLE:
    case 12:    /* city A */
    case 14:    /* city B */
    case 17:    /* city walls */
    case 26:    /* farm */
    case 29:    /* fences */
    case SET_MAGES:
    case 38:    /* manor */
    case 42:    /* merchant homes */
    case 58:    /* tavern */
    case 61:    /* temple */
    case 64:    /* village */
    case 69:    /* roofs */
    case 79:    /* city specials */
    case 80:
    case 82:
    case 83:
        return 1;
    default:
        return 0;
    }
}

/* An inside set: the family, never snow. */
static int inside_set(int set)
{
    switch (set) {
    case 11: case 16: case 19: case 20: case 22: case 23: case 24: case 25:
    case 28: case 37: case 40: case SET_MARBLE: case 44: case 45: case 47: case 48:
    case 60: case 63: case 66: case 68:
        return 1;
    default:
        return 0;
    }
}

int swap_climate(int archive, int record, int family, int winter, int (*count)(int file))
{
    int set;
    int snow;
    int out;

    /* The third door keeps its picture, as in Daggerfall. */
    if (archive % 100 == SET_DOORS && record == 3) {
        return archive;
    }
    /* Nature (500 and up) is for the ground's trees, not drawn yet. */
    if (archive < 0 || archive >= 500 || family < 0) {
        return archive;
    }
    set = archive % 100;
    if (outside_set(set)) {
        snow = winter;
    } else if (set == SET_DOORS || inside_set(set)) {
        snow = 0;
    } else {
        return archive;
    }
    if (family == TEX_SWAMP && set == SET_MARBLE) {
        return archive;
    }
    /* Too many bad swaps in the desert, and no one sees its snow. */
    if (family == TEX_DESERT) {
        snow = 0;
    }
    /* Sets with no snowy picture where it would be (Daggerfall Unity's precedence kept). */
    if ((family == TEX_SWAMP && set == SET_MAGES) || (set == SET_CASTLE && record > 3)) {
        snow = 0;
    }
    if ((archive == 82 && record > 1) || archive == 77) {
        snow = 0;
    }
    /* These keep their own climate and only snow. */
    switch (archive) {
    case 75: case 76: case 77: case 79: case 80: case 82: case 83:
        out = archive + snow;
        break;
    default:
        out = family + set + snow;
        break;
    }
    if (out == archive) {
        return archive;
    }
    if (count && record >= count(out)) {
        return archive;
    }
    return out;
}

int swap_window(int archive, int record)
{
    /* Spells and lights glow by themselves. */
    if ((archive >= 375 && archive <= 379) || archive == 210 || archive == 280) {
        return 0;
    }
    if ((archive == 36 && record == 2) || (archive == 151 && record == 3)
        || (archive == 154 && record == 3) || (archive == 351 && record == 3)
        || (archive == 171 && record == 2) || (archive == 172 && record == 3)
        || (archive == 173 && record == 3)) {
        return 1;
    }
    switch (archive % 100) {
    /* The general sets have a window at record 3. */
    case 9: case 10: case 12: case 13: case 14: case 15: case 26: case 27:
    case 35: case 36: case 38: case 39: case 42: case 43: case 58: case 59:
    case 61: case 62: case 64: case 65:
        return record == 3;
    /* Gables are all window. */
    case 75: case 76: case 77:
        return 1;
    case 79: case 80:
        return record == 0 || record == 2;
    case 82: case 83:
        return record == 0;
    default:
        return 0;
    }
}
