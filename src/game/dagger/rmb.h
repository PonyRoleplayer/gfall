#ifndef DF_RMB_H
#define DF_RMB_H

#include "game/dagger/rdb.h"

/*
 * One RMB block: the exterior shell of a town or wilderness location.
 * Building models, props, and flats. House interiors stay in the file.
 * ground_file is the climate's ground archive (texture family + 2).
 * Pass 0 to skip the ground plane.
 */

struct Bsa;

Block *rmb_load(const df_u8 *rmb, df_u32 size, struct Bsa *arch, int ground_file);

#endif
