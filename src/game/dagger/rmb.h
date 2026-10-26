#ifndef DF_RMB_H
#define DF_RMB_H

#include "game/dagger/rdb.h"

/*
 * One RMB block: the exterior shell of a town or wilderness location.
 * Building models, props, and flats. A house's inside loads on its own
 * (rmb_load_interior), when its door is used.
 * ground_file is the climate's ground archive (texture family + 2).
 * Pass 0 to skip the ground plane.
 */

struct Bsa;

Block *rmb_load(const df_u8 *rmb, df_u32 size, struct Bsa *arch, int ground_file,
                ArchEnt *cache, int *ncache);
/*
 * The ARCH3D ids of every outside model rmb_load would place, in its
 * order, repeats included, at most cap. A streamed world parses them
 * into the cache one a frame (arch_cached) before rmb_load, which then
 * only places them.
 */
int rmb_model_ids(const df_u8 *rmb, df_u32 size, df_u32 *ids, int cap);

/*
 * The inside of building record (its subrecord): models, flats, people,
 * the editor markers (199: 8 is where a visitor enters), and the hinged
 * doors of its door list, ModelAct.hinged. Placed as the building's
 * outside is, so the two meet at its doors. 0 when it has no inside.
 */
Block *rmb_load_interior(const df_u8 *rmb, df_u32 size, struct Bsa *arch, int record,
                         ArchEnt *cache, int *ncache);
/*
 * What building record is (Daggerfall Unity's BuildingTypes: 15 a
 * tavern, 14 a temple, 17 and up houses) and its faction. 0 for none.
 */
int rmb_building(const df_u8 *rmb, df_u32 size, int record, int *type, int *faction);

#endif
