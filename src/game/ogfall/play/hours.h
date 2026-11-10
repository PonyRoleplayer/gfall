#ifndef OGFALL_HOURS_H
#define OGFALL_HOURS_H

#include "engine/core/df.h"

/*
 * When a building is open: building_is_open and building_minutes_to_close (src/lifted/tamriel.c 317..352). A building is open between the hour its type
 * opens and the hour it shuts (FALL.EXE's table, by type 0..16), and always to the Dark Brotherhood's and the Thieves Guild's own members, and to a Fighters or
 * Mages Guild member from rank 6 in their hall. The houses (17..20) are open by day to half their numbers; the rest of them never. Whether a shop's shelves
 * are for sale or free for the taking, and a door's lock, ask this.
 */

typedef struct OgHoursPlayer {
    /* The ranks of the guilds that open their halls (-1 for no membership): the Brotherhood's (kind 0), the Thieves' (3), the Fighters' (2), the Mages' (1). */
    int brotherhood, thieves, fighters, mages;
} OgHoursPlayer;

/* 1 when open at game minute now. daytime is for the houses; id is the building's key (its low word % 100 < 50 picks the houses open by day). */
int og_building_open(int type, int faction, df_u32 id, df_u32 now, int daytime, const OgHoursPlayer *player);
/* Minutes until it shuts, 0 when it is shut. */
int og_building_minutes_to_close(int type, df_u32 now);

#endif
