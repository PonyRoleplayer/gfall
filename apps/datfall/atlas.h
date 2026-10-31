#ifndef DATFALL_ATLAS_H
#define DATFALL_ATLAS_H

#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/worldmap.h"

/*
 * The world map as one 24-bit picture (docs/data.md). Built once and
 * blitted: a height shade, sea where the byte is 1 or less, an optional
 * climate tint, and optional location dots. Never walked per frame.
 */

#define ATLAS_STRIDE (((WORLDMAP_W * 3) + 3) & ~3)

const char *atlas_climate_name(int id);
const char *atlas_type_name(int type);
/*
 * Places whose name or region contains q, case folded. An empty q is
 * every place. hit_out is 0 for a dungeon and 1 for a town. Writes at
 * most cap hits and returns how many were written. No window.
 */
int atlas_place_hits(const DunRef *duns, int ndun, const DunRef *towns, int ntown,
                     const char *q, int *hit_i, unsigned char *hit_out, int cap);
/*
 * The other half of a place. A dungeon writes "Above: <town>". A town
 * writes "Below: <dungeon>". 0 when that half is not on the map.
 * other_index is the list index of the half. other_out is 1 for a town.
 */
int atlas_other_line(const DunRef *duns, int ndun, const DunRef *towns, int ntown,
                     int region, int outdoor, int location_id, int exterior_id,
                     char *dst, int cap, int *other_index);
/* dib is top-down, ATLAS_STRIDE bytes a row, WORLDMAP_H rows. */
void atlas_build(unsigned char *dib, int climate_on, int dots);

#endif
