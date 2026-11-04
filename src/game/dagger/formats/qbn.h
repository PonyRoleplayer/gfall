#ifndef DF_QBN_H
#define DF_QBN_H

#include "engine/core/df.h"

/*
 * A quest's logic: ARENA2's .QBN, read in place. Its words are the quest's
 * .QRC, TEXT.RSC's format (textrsc_read_in). Read docs/game.md (Quests) before
 * editing.
 *
 * From UESP's Quest hacking guide and Quest pseudo-codes:
 *   header, 60 bytes: at 15 a byte, nonzero when the symbols' names follow;
 *     at 16 ten u16 record counts, at 36 eleven u16 offsets, one a section;
 *   0 items, 19 bytes a record; 3 people, 20; 4 places, 24; 6 timers, 33;
 *     7 foes, 14; 8 the logic, 87; 9 states, 8; 10 the symbols' names, 27,
 *     to a record whose name is empty (seven files have them); 1, 2 and 5
 *     are never used.
 *   A logic record: u16 opcode, u16 flags, u16 how many operands (1..5), five
 *     operands of 15 bytes, u16 a QRC message (0xFFFF none), u32 unused.
 *   An operand: a byte that negates it, a u32 that is either 0x12345678 (a
 *     constant) or the section and record it names, a u16 section, a u32
 *     value (a record, or the constant), a u32 unused.
 * Each resource has a u32 symbol: a hash of its name in the source, by which
 * the QRC's text can name it.
 *
 * Plain C: hostcheck reads every quest in ARENA2.
 */

#define QBN_SECTIONS 11
#define QBN_OPERANDS 5
/* No QRC message on a logic record; a record value meaning none. */
#define QBN_NO_MESSAGE 0xFFFF
#define QBN_NONE 0xFFFFFFFFu

enum {
    QBN_ITEMS = 0,
    QBN_PEOPLE = 3,
    QBN_PLACES = 4,
    QBN_TIMERS = 6,
    QBN_FOES = 7,
    QBN_LOGIC = 8,
    QBN_STATES = 9,
    QBN_NAMES = 10
};

typedef struct QbnItem {
    int index;
    /* 0 an artifact, 1 an item, 2 gold. */
    int reward;
    /* Its kind and which of it (0xFFFF: random, or none). */
    int category;
    int category_index;
    df_u32 symbol;
    /* QRC messages: on being given it, on using it. */
    int text_given;
    int text_used;
} QbnItem;

typedef struct QbnPerson {
    int index;
    /* 0 a man, 1 a woman, 0xFF either. */
    int gender;
    int face;
    /* 0xFFFE: a Daedra Prince, the faction which (0 any). */
    int kind;
    int faction;
    df_u32 symbol;
    int text_tell[2];
} QbnPerson;

typedef struct QbnPlace {
    int index;
    /* 0 a place named outright, 1 in this town, 2 outside it. */
    int general;
    /* Named: an object id's high and low halves. Else 0 a building, 1 a dungeon, and its type (-1 any). */
    int fine;
    int type;
    int door;
    df_u32 symbol;
    int text_tell[2];
} QbnPlace;

typedef struct QbnTimer {
    int index;
    int flags;
    /* 0 random, 1 fixed, 2.. by the distance to one or two places or people. */
    int type;
    long least;
    long most;
    long link[2];
    /* The state it sets when it runs out. */
    df_u32 symbol;
} QbnTimer;

typedef struct QbnFoe {
    int index;
    /* A creature kind (ENEMY_*: 0..0x28 a monster, 0x80.. a class). */
    int kind;
    int count;
    df_u32 symbol;
} QbnFoe;

typedef struct QbnOperand {
    int negate;
    /* 1 when value is a constant, else a record of section. */
    int constant;
    int section;
    df_u32 value;
} QbnOperand;

typedef struct QbnLogic {
    int opcode;
    int flags;
    int count;
    QbnOperand arg[QBN_OPERANDS];
    int message;
} QbnLogic;

typedef struct QbnState {
    int index;
    int global;
    int global_index;
    df_u32 symbol;
} QbnState;

typedef struct QbnName {
    char name[21];
    int section;
    int record;
} QbnName;

typedef struct Qbn {
    int count[QBN_SECTIONS];
    QbnItem *items;
    QbnPerson *people;
    QbnPlace *places;
    QbnTimer *timers;
    QbnFoe *foes;
    QbnLogic *logic;
    QbnState *states;
    QbnName *names;
} Qbn;

/*
 * A symbol's hash, as each resource carries it and the QRC's text names it
 * (UESP's Text Variable Hashes): twice the hash so far plus each character's
 * code, "reward" 0x1B14.
 */
df_u32 qbn_hash(const char *name);
/*
 * The quest file name ("M0B00Y00.QBN") read and parsed, its records in the
 * heap now current, which the caller frees with it. 0 when it does not read
 * or a section runs past the file's end.
 */
int qbn_load(const char *name, Qbn *out);
/* The same from bytes already read. */
int qbn_parse(const df_u8 *bytes, df_u32 n, Qbn *out);

#endif
