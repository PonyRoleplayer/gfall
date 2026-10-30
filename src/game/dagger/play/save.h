#ifndef DF_SAVE_H
#define DF_SAVE_H

#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * One saved game, DATA\SAVE0.SAV. Read docs/game.md (Saves) before editing.
 *
 * It is a record file (docs/data.md), like KEYS.DAT: self-describing chunks,
 * so `dfrec dump` reads it, and a later program reading an older save keeps
 * a new field's zero. Its chunks:
 *   GAME  the player's place and facing, the clock, the game's seed
 *   CHAR  the character sheet
 *   PACK  what is carried, and GAME says what is worn and the gold
 *   KEEP  every door, lever and creature not as its file has it (keep.h)
 *   PILE  what lies dropped (pile.h)
 *
 * A body's and a heap's things are not written: they come again from the
 * seed and their keys (loot.h), and their keep rows say what was taken.
 *
 * Plain C: hostcheck runs a round trip.
 */

/* Everything a save holds that is not already in a table of its own. */
typedef struct SaveState {
    /* The player's feet, in the place's frame: a stream's origin moves. */
    float feet[3];
    float yaw;
    float pitch;
    /* The clock: its minute and its day; and the day the game's own count of minutes began. */
    float minute;
    int day;
    int day_zero;
    df_u32 seed;
    Character sheet;
    Inventory pack;
} SaveState;

/* The name saves go by, DATA\\SAVE0.SAV until told otherwise: hostcheck's own, so it never touches a player's. */
void save_use_file(const char *name);
/* 1 when there is a save to load. */
int save_exists(void);
/*
 * Writes state, and the keep table and the piles as they are now: the caller
 * has flushed what is loaded into the keep table first (stream_keep_now).
 * 0, with any old save left as it was, when it could not be written.
 */
int save_write(const SaveState *state);
/*
 * Reads a save into state, and puts its keep rows and piles back in place of
 * what is there. 0, with nothing changed, when it is missing or not one.
 */
int save_read(SaveState *state);

#endif
