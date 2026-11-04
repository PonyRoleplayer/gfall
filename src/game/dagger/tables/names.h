#ifndef DF_NAMES_H
#define DF_NAMES_H

#include "engine/assets/recfile.h"

/*
 * People's names, as records (docs/data.md), cooked from ARENA2's
 * NAMEGEN.DAT (formats/namegen.h). Read docs/game.md (People, Names) before
 * editing.
 *
 *   NAME: one piece of a name: its bank (a race's, or monsters'), its set in
 *         the bank, its place in the set, and its text. Row id is
 *         bank x 512 + set x 64 + place. A mod adds pieces by adding rows.
 *
 * Names are put together by Daggerfall Unity's NameHelper patterns, from a
 * seed so the same person keeps one name:
 *   most races: first name set 0 + set 1 (a man) or 2 + 3 (a woman), surname
 *     set 4 + set 5;
 *   Nords: the same first names, surname set 0 + set 1 + "sen";
 *   Redguards: one name, sets 0 + 1 + 2, then set 3 (a man, three times in
 *     four) or set 4 (a woman).
 */

#define NAME_TEXT 12
#define NAME_BANKS 11
#define NAME_SETS 6
#define NAME_PER_SET 64

/* The banks (DFU's NameHelper.BankTypes). */
enum {
    NAME_BANK_BRETON,
    NAME_BANK_REDGUARD,
    NAME_BANK_NORD,
    NAME_BANK_DARK_ELF,
    NAME_BANK_HIGH_ELF,
    NAME_BANK_WOOD_ELF,
    NAME_BANK_KHAJIIT,
    /* Classic's file has Imperial names where Argonian ones would be. */
    NAME_BANK_IMPERIAL
};

typedef struct NamePiece {
    df_u16 id;
    df_u8 bank;
    df_u8 set;
    char text[NAME_TEXT];
} NamePiece;

extern const RecSchema name_schema;
RecTable *name_table(void);

/* The bank for a race (RACE_*): an Argonian's is the Imperial bank, as classic's file has it. */
int name_bank_of_race(int race);
/*
 * The bank of a map region's people: classic's region races from FALL.EXE
 * (DFU's MapsFile.RegionRaces), 0 Breton or 1 Redguard; Breton for none.
 */
int name_bank_of_region(int region);
/* A full name for a person of bank, a woman or not, from seed, into out. Empty when the bank has no pieces. */
void name_make(int bank, int female, unsigned seed, char *out, int cap);

#endif
