#ifndef GFALL_ENTRY_H
#define GFALL_ENTRY_H

#include "engine/core/df.h"

/*
 * Entering a building from the street: building_try_enter (src/lifted/maplogic.c 335..367) and lockpick_action_door (src/lifted/archive.c 243..273). Part of
 * people and law, after the guilds. A building of a type under 17 is open by its hours (hours.c); otherwise the door is shut, and the way in is a spell that opens
 * locks, a guild's own door, a house by day (half of them, with record 256 said), or picking the lock in Steal mode. The lock's level is the building's name seed mod 10
 * plus 3; Lockpicking (skill 13) is used once, the chance is the skill less five a level (5..95, none at 20 or over), one try for each skill value; a door picked is a crime
 * of attempted breaking (1) if anyone saw or a roll of 1..300 is under 100 less Stealth, and is counted by the Thieves Guild (a crime of 5). Anything else shows how hard the lock is.
 */

typedef struct EntryBuilding {
    df_u32 id;
    int type, faction, name_seed;
    /* The building's own flag 16 (never open to the street) and its access level (10 and over opens its door to a picker). Not read from the blocks yet: 0. */
    int flags, access;
    /* A quest has marked the building open to it (op 81's place flag 64 or person flag 0x4000: quest_find_site_for_building). */
    int quest_open;
} EntryBuilding;

typedef struct EntryPlayer {
    df_u32 house;
    /* The ranks of the Brotherhood's and the Thieves' memberships (-1 for none), and of the Fighters' and Mages' (their halls open to them from rank 6). */
    int brotherhood, thieves, fighters, mages;
    int daytime, steal;
    /* A spell that opens a lock was cast (conditions 0x40): its chance, and whether it is waiting. */
    int lock_spell, lock_chance;
} EntryPlayer;

typedef struct Entry {
    int allow;
    /* What to say: a TEXT.RSC record (256), or the lock's own line. */
    int text;
    const char *line;
    /* The pick's outcome: tried and worked, a crime to note (1) and the watch to call. */
    int picked, crime;
    /* The open-lock spell was spent. */
    int spent_spell;
} Entry;

/* 1 when the door lets the player in. The lock's try is remembered by building and Lockpicking value (not saved). */
int building_try_enter(const EntryBuilding *building, const EntryPlayer *player, df_u32 now, int witnessed, Entry *out);
void entry_reset(void);

#endif
