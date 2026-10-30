#ifndef DATFALL_ATLAS_H
#define DATFALL_ATLAS_H

#include "game/dagger/formats/worldmap.h"

/*
 * The world map as one 24-bit picture (docs/data.md). Built once and
 * blitted: a height shade, sea where the byte is 1 or less, an optional
 * climate tint, and optional location dots. Never walked per frame.
 */

#define ATLAS_STRIDE (((WORLDMAP_W * 3) + 3) & ~3)

const char *atlas_climate_name(int id);
const char *atlas_type_name(int type);
/* dib is top-down, ATLAS_STRIDE bytes a row, WORLDMAP_H rows. */
void atlas_build(unsigned char *dib, int climate_on, int dots);

#endif
