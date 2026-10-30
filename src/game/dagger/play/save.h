#ifndef DF_SAVE_H
#define DF_SAVE_H

#include "game/dagger/places/travel.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * Saved games, six slots with a name each, DATA\SAVE0.SAV to SAVE5.SAV. Read
 * docs/game.md (Saves) before editing.
 *
 * It is a record file (docs/data.md), like KEYS.DAT: self-describing chunks,
 * so `dfrec dump` reads it, and a later program reading an older save keeps
 * a new field's zero. Its chunks:
 *   GAME  which place, the player's place in it and facing, the way back
 *         out of its door, the clock, the game's seed
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

#define SAVE_SLOTS 6
#define SAVE_NAME 24

/* Everything a save holds that is not already in a table of its own. */
typedef struct SaveState {
    /* Which place, and the player's feet in its frame, with the stream's origin that frame has (0, 0 with no stream). */
    TravelWhere where;
    int origin[2];
    float feet[3];
    float yaw;
    float pitch;
    /* The clock: its minute and its day; and the day the game's own count of minutes began. */
    float minute;
    int day;
    int day_zero;
    df_u32 seed;
    /* What the player called this save. */
    char name[SAVE_NAME];
    Character sheet;
    Inventory pack;
} SaveState;

/* What the files' names start with, DATA\\SAVE until told otherwise: hostcheck's own, so it never touches a player's. */
void save_use_prefix(const char *prefix);
/* The slot the calls below read and write, 0..SAVE_SLOTS-1. */
void save_use_slot(int slot);
/* 1 when the slot in use has a save. */
int save_exists(void);
/* 1 when any slot has. */
int save_any(void);
/* A slot's name, into out (SAVE_NAME); 0, and empty, when it has no save. */
int save_peek_name(int slot, char *out);
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
