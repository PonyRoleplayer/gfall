#ifndef OGFALL_QBN_H
#define OGFALL_QBN_H

/*
 * A quest's logic: ARENA2's .QBN, read in place and parsed into plain records (docs/quest-machine.md of the decompilation, section 1.2; docs/ogfall-quests.md here).
 * The original uses the file as its own record and fixes pointers up; OGFALL keeps no pointers: an operand names a record by section and index, so a quest can be
 * saved as it is. Nothing here knows the world.
 *
 * Sections: 0 items (19 bytes a record), 3 persons (20), 4 places (24), 6 timers (33), 7 foes (14), 8 ops (87), 9 states (8), and the text variables after
 * them (27); 1, 2 and 5 are never used. An operand is a literal number, "always" (-1), "none" (-2), or a record of a section.
 */
#include "engine/core/df.h"

#define OG_QBN_ITEMS 16
#define OG_QBN_PERSONS 16
#define OG_QBN_PLACES 8
#define OG_QBN_TIMERS 10
#define OG_QBN_FOES 8
#define OG_QBN_OPS 136
#define OG_QBN_STATES 64
#define OG_QBN_TEXTVARS 8

typedef struct OgQbnArg {
    int negate;
    /* 1 for a literal number (value), else a record: section and index (value), -1 always, -2 none. */
    int literal, section, value;
} OgQbnArg;

typedef struct OgQbnOp {
    int opcode, flags, argc;
    OgQbnArg arg[5];
    /* -1 none; bit 15 set: a TEXT.RSC id in the low 15 bits, else a QRC id. */
    int message;
    df_u32 last_minutes;
} OgQbnOp;

typedef struct OgQbnItem { int flags, group, index; df_i32 symbol; int message[2]; } OgQbnItem;
typedef struct OgQbnPerson { int flags, kind, faction; df_i32 symbol; int message[2]; } OgQbnPerson;
typedef struct OgQbnPlace { int flags, scope, p1, p2, p3; df_i32 symbol; int message[2]; } OgQbnPlace;
typedef struct OgQbnTimer { int flags, type; df_i32 minimum, maximum, start; df_u32 delay; int link1, link2; df_i32 hash; } OgQbnTimer;
typedef struct OgQbnFoe { int type, count, killed; df_i32 symbol; } OgQbnFoe;
typedef struct OgQbnState { int index, is_global, value; df_i32 hash; } OgQbnState;
typedef struct OgQbnTextVar { char name[20]; int section, index; } OgQbnTextVar;

typedef struct OgQbn {
    int id, faction, text_file, flags;
    char name[9];
    int n_item, n_person, n_place, n_timer, n_foe, n_op, n_state, n_textvar;
    OgQbnItem item[OG_QBN_ITEMS];
    OgQbnPerson person[OG_QBN_PERSONS];
    OgQbnPlace place[OG_QBN_PLACES];
    OgQbnTimer timer[OG_QBN_TIMERS];
    OgQbnFoe foe[OG_QBN_FOES];
    OgQbnOp op[OG_QBN_OPS];
    OgQbnState state[OG_QBN_STATES];
    OgQbnTextVar textvar[OG_QBN_TEXTVARS];
} OgQbn;

/* The number of arguments of an opcode (COUNT.DAT: one ASCII digit an opcode, arg 0 included), 0 for an opcode past the table. */
int og_qbn_arg_count(int opcode);
/* A file's bytes parsed; 0 when it is not a quest, or holds more than a record table can. */
int og_qbn_parse(const df_u8 *bytes, df_u32 size, OgQbn *quest);
/* NAME.QBN from ARENA2, parsed; the name goes in. */
int og_qbn_load(const char *name, OgQbn *quest);
/* string_hash: hash * 2 + byte, from 0. */
df_i32 og_qbn_hash(const char *name);

#endif
