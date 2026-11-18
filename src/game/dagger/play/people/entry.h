#ifndef GFALL_ENTRY_H
#define GFALL_ENTRY_H

/*
 * building_try_enter and the door's lock (maplogic.c, archive.c; inline there): a building of type BUILDING_ALWAYS_OPEN is always open, those under the first house open by the hours (with
 * BUILDING_FLAG_OPEN a shop is shut and a house open), a house by day lets in HOUSE_OPEN_CHANCE in 100 by its id (TEXT_HOUSE_OPEN), and a door's lock is DOOR_LOCK_MIN plus the sign's seed
 * mod DOOR_LOCK_SPREAD. An access level of ACCESS_OPEN opens it; a lock of LOCK_IMPOSSIBLE or more cannot be picked; a pick's chance is the skill less LOCK_PER_LEVEL a level of lock; it is seen
 * when a roll of 1..PICK_SEEN_ROLL is under PICK_SEEN_BASE less Stealth.
 */
#define BUILDING_ALWAYS_OPEN 24
#define BUILDING_FLAG_OPEN 16
#define HOUSE_OPEN_CHANCE 50
#define TEXT_HOUSE_OPEN 256
#define DOOR_LOCK_SPREAD 10
#define DOOR_LOCK_MIN 3
#define ACCESS_OPEN 10
#define LOCK_IMPOSSIBLE 20
#define LOCK_PER_LEVEL 5
#define PICK_SEEN_ROLL 300
#define PICK_SEEN_BASE 100

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
/*
 * A building's street door bashed (weapon_melee_strike): a roll of 1..100 under BASH_OPEN_UNDER sets its flag 16 (open to the street: a house for good, a shop only
 * in its hours and no longer to be picked) and the player goes in; else, if 1..100 is over Stealth and no guard is about, a crime of none (0) and the watch comes (the court
 * ignores crime 0). Kept while the town stays loaded.
 */
#define BASH_OPEN_UNDER 10
void entry_set_bashed(df_u32 building);
int entry_bashed(df_u32 building);

#endif
