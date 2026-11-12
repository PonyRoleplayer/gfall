#ifndef GFALL_QBN_H
#define GFALL_QBN_H

/*
 * A quest's logic: ARENA2's .QBN, read in place and parsed into plain records (docs/quest-machine.md of the decompilation, section 1.2; docs/game-quests.md here).
 * The original uses the file as its own record and fixes pointers up; GFALL keeps no pointers: an operand names a record by section and index, so a quest can be
 * saved as it is. Nothing here knows the world.
 *
 * Sections: 0 items (19 bytes a record), 3 persons (20), 4 places (24), 6 timers (33), 7 foes (14), 8 ops (87), 9 states (8), and the text variables after
 * them (27); 1, 2 and 5 are never used. An operand is a literal number, "always" (-1), "none" (-2), or a record of a section.
 */
#include "engine/core/df.h"

#define QBN_ITEMS 16
#define QBN_PERSONS 16
#define QBN_PLACES 8
#define QBN_TIMERS 10
#define QBN_FOES 8
#define QBN_OPS 136
#define QBN_STATES 64
#define QBN_TEXTVARS 8

typedef struct QbnArg {
    int negate;
    /* 1 for a literal number (value), else a record: section and index (value), -1 always, -2 none. */
    int literal, section, value;
} QbnArg;

typedef struct QbnOp {
    int opcode, flags, argc;
    QbnArg arg[5];
    /* -1 none; bit 15 set: a TEXT.RSC id in the low 15 bits, else a QRC id. */
    int message;
    df_u32 last_minutes;
} QbnOp;

typedef struct QbnItem { int flags, group, index; df_i32 symbol; int message[2]; } QbnItem;
typedef struct QbnPerson { int flags, kind, faction; df_i32 symbol; int message[2]; } QbnPerson;
typedef struct QbnPlace { int flags, scope, p1, p2, p3; df_i32 symbol; int message[2]; } QbnPlace;
typedef struct QbnTimer { int flags, type; df_i32 minimum, maximum, start; df_u32 delay; int link1, link2; df_i32 hash; } QbnTimer;
typedef struct QbnFoe { int type, count, killed; df_i32 symbol; } QbnFoe;
typedef struct QbnState { int index, is_global, value; df_i32 hash; } QbnState;
typedef struct QbnTextVar { char name[20]; int section, index; } QbnTextVar;

typedef struct Qbn {
    int id, faction, text_file, flags;
    char name[9];
    int n_item, n_person, n_place, n_timer, n_foe, n_op, n_state, n_textvar;
    QbnItem item[QBN_ITEMS];
    QbnPerson person[QBN_PERSONS];
    QbnPlace place[QBN_PLACES];
    QbnTimer timer[QBN_TIMERS];
    QbnFoe foe[QBN_FOES];
    QbnOp op[QBN_OPS];
    QbnState state[QBN_STATES];
    QbnTextVar textvar[QBN_TEXTVARS];
} Qbn;

/* The number of arguments of an opcode (COUNT.DAT: one ASCII digit an opcode, arg 0 included), 0 for an opcode past the table. */
int qbn_arg_count(int opcode);
/* A file's bytes parsed; 0 when it is not a quest, or holds more than a record table can. */
int qbn_parse(const df_u8 *bytes, df_u32 size, Qbn *quest);
/* NAME.QBN from ARENA2, parsed; the name goes in. */
int qbn_load(const char *name, Qbn *quest);
/* string_hash: hash * 2 + byte, from 0. */
df_i32 qbn_hash(const char *name);

#endif
