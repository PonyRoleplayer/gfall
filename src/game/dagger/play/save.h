#ifndef DF_SAVE_H
#define DF_SAVE_H

#include "game/dagger/places/travel.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * A saved game: one file, a record file (docs/data.md) of self-describing
 * chunks, so `dfrec dump` reads it. Where the files are, which character
 * they belong to and the list screen are saves.h; this module is one file's
 * content. Read docs/game.md (Saves) before editing. Its chunks:
 *   META  who and when, for the list: the character's id, name and level, the
 *         real time it was saved, where the game was and its clock (one row,
 *         first, so the list reads only the start of each file)
 *   THMB  the snapshot, a picture of the view, 160 x 120 in RGB 565, as rows
 *         of 240 pixels (a row holds at most 512 bytes)
 *   GAME  which place, the player's place in it and facing, the way back
 *         out of its door, the clock, the game's seed
 *   CHAR  the character sheet
 *   PACK  what is carried, and GAME says what is worn and the gold
 *   KEEP  every door, lever and creature not as its file has it (keep.h)
 *   PILE  what lies dropped (pile.h)
 *   FACT  each faction whose reputation has moved
 *   SOCL  the social groups' reputations and the lasting bonus
 *   REGN  each region's law and bank (standing.h)
 *   DISC  the places the map knows that a new game's does not (atlas.h)
 *   AMAP  every place's models the automap has seen (engine/scene/automap.h)
 *   WAGN  what is in the cart's wagon (wagon.h)
 *
 * A body's and a heap's things are not written: they come again from the
 * seed and their keys (loot.h), and their keep rows say what was taken.
 *
 * Plain C: hostcheck runs a round trip.
 */

#define SAVE_NAME 24
#define SAVE_WHERE 40
#define SAVE_THUMB_W 160
#define SAVE_THUMB_H 120
/* A file's name beside the program, saves\Anna Smith\S0003.SAV, at most. */
#define SAVE_PATH 160

/* Everything a save holds that is not already in a table of its own. */
typedef struct SaveState {
    /* Which place, and the player's feet in its frame, with the stream's origin that frame has (0, 0 with no stream). */
    TravelWhere where;
    int origin[2];
    float feet[3];
    float yaw;
    float pitch;
    /* Classic Alt look is independent of movement heading. Missing in older
     * saves means centered (zero), without changing SAVE_VERSION. */
    float view_yaw;
    float view_pitch;
    /* The clock: its minute and its day; and the day the game's own count of minutes began. */
    float minute;
    int day;
    int day_zero;
    df_u32 seed;
    /* Where the game was, in words, for the list ("Daggerfall, Wayrest"). */
    char where_words[SAVE_WHERE];
    /* The snapshot to keep, SAVE_THUMB_W x SAVE_THUMB_H in RGB 565 with row 0 at the top, or 0 for none. Not read back (save_peek_thumb). */
    const df_u16 *thumb;
    Character sheet;
    Inventory pack;
} SaveState;

/* What the list shows of a save, read from the start of its file. */
typedef struct SaveMeta {
    df_u32 char_id;
    char name[SAVE_NAME];
    int level;
    /* When it was saved, by the clock on the wall. */
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    char where[SAVE_WHERE];
    /* The game's own day and minute of the day. */
    int game_day;
    int game_minute;
    int has_thumb;
} SaveMeta;

/* The file the calls below read and write: beside the program, e.g. saves\Anna Smith\S0003.SAV. */
void save_use_file(const char *name);
/* 1 when that file is there. */
int save_exists(void);
/* A file's META, only the start of it read: 1 when it is a save of this game, with its meta in out. */
int save_peek(const char *name, SaveMeta *out);
/* A file's snapshot into out (SAVE_THUMB_W x SAVE_THUMB_H, RGB 565): 1 when it has one. */
int save_peek_thumb(const char *name, df_u16 *out);
/*
 * Writes state, and the keep table and the piles as they are now: the caller
 * has flushed what is loaded into the keep table first (stream_keep_now). It
 * goes to a temporary name and is moved into place, so a failure leaves any
 * file that was there as it was: 0 then.
 */
int save_write(const SaveState *state);
/*
 * Reads a save into state, and puts its keep rows and piles back in place of
 * what is there. 0, with nothing changed, when it is missing or not one.
 */
int save_read(SaveState *state);

#endif
