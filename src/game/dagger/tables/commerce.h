#ifndef DF_COMMERCE_H
#define DF_COMMERCE_H

#include "engine/assets/recfile.h"

/*
 * Buildings and crimes, as records (docs/data.md), classic's as Daggerfall
 * Unity carries them. Read docs/game.md (People) before editing.
 *
 *   BLDG: a building type, by the number in a block's file (DFU's
 *         DFLocation.BuildingTypes): what it is called in talk, the hours it
 *         is open (close 25: never shuts; open and close 0: always shut), and
 *         the chance numbers its shelves stock weapons and armour by.
 *   CRIM: a crime, by DFU's PlayerEntity.Crimes: what the court calls it, what
 *         it costs the legal reputation, and the sums of its sentence.
 */

/* The building types the code names. */
enum {
    BUILDING_ALCHEMIST = 0,
    BUILDING_ARMORER = 2,
    BUILDING_BANK = 3,
    BUILDING_BOOKSELLER = 5,
    BUILDING_CLOTHING = 6,
    BUILDING_FURNITURE = 7,
    BUILDING_GEMS = 8,
    BUILDING_GENERAL = 9,
    BUILDING_LIBRARY = 10,
    BUILDING_GUILD_HALL = 11,
    BUILDING_PAWN = 12,
    BUILDING_WEAPONS = 13,
    BUILDING_TEMPLE = 14,
    BUILDING_TAVERN = 15,
    BUILDING_PALACE = 16,
    BUILDING_HOUSE1 = 17,
    BUILDING_HOUSE4 = 20,
    BUILDING_TYPES = 25
};

/* The crimes the code names. */
enum {
    CRIME_NONE = 0,
    CRIME_BREAKING_IN = 3,
    CRIME_THEFT = 13,
    CRIME_LOAN_DEFAULT = 15,
    CRIME_COUNT = 16
};

typedef struct BuildingKind {
    df_u16 id;
    char name[24];
    df_u8 open;
    df_u8 close;
    df_u8 stock_weapons;
    df_u8 stock_armor;
} BuildingKind;

typedef struct CrimeKind {
    df_u16 id;
    char name[32];
    /* What committing it takes off the legal reputation. */
    df_u8 loss;
    /* The sentence: base, plus per a point of legal reputation either way, within min and max (DaggerfallCourtWindow). */
    df_u8 per_point;
    df_u16 base;
    df_u16 low;
    df_u16 high;
} CrimeKind;

extern const RecSchema building_schema;
extern const RecSchema crime_schema;
RecTable *building_table(void);
RecTable *crime_table(void);
/* By their numbers; 0 for none. */
const BuildingKind *building_kind(int type);
const CrimeKind *crime_kind(int crime);
/* A building type's shelves can be bought from (its stock numbers, for what the game has yet). */
int building_is_shop(int type);
/* Open at this hour, by its record. */
int building_open_at(int type, int hour);

#endif
