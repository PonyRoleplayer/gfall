#ifndef DATFALL_LOOK_H
#define DATFALL_LOOK_H

#include "pics.h"
#include "view.h"
#include "game/dagger/formats/dungeon.h"

/*
 * A search across the merged book, the places and the texture archives
 * (docs/cd-gfall.md). Names are read from the rows already parsed. No
 * window, and no second copy of a file. Exact names are kept. A name
 * that only contains the text stops at LOOK_SUB_MAX.
 */

#define LOOK_SUB_MAX 200

enum {
    LOOK_RACE = 0,
    LOOK_CREATURE,
    LOOK_WEAPON,
    LOOK_ARMOUR,
    LOOK_CLASS,
    LOOK_SKILL,
    LOOK_PLACE,
    LOOK_TABLE,
    LOOK_PICTURE
};

typedef struct LookHit {
    int where;
    int exact;
    int row;
    int id;
    int extra;
} LookHit;

/*
 * q of one character does not return places. *more is 1 when a containing name was
 * left out. Returns how many hits were stored.
 */
int look_search(DatFile *book, const DunRef *duns, int ndun, const DunRef *towns, int ntown,
                const TexArc *arcs, int narc, const char *q, LookHit *hits, int cap, int *more);

const char *look_where_name(int where);

/* The name the hit list shows. */
void look_hit_name(DatFile *book, const DunRef *duns, const DunRef *towns, const LookHit *hit,
                   char *dst, int cap);

#endif
