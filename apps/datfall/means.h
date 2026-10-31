#ifndef DATFALL_MEANS_H
#define DATFALL_MEANS_H

#include "view.h"

/*
 * The plain name beside a record's own text (docs/cd-gfall.md). Only
 * the fields the browser knows are named. A mod's new field stays
 * blank. No window.
 */

/* The Means text for this field of this row. Empty when it is not named. */
void means_of(DatFile *f, int kind, int row, int field, char *dst, int cap);

/* Encounter id's kind count and the first kind id. 0 when the row is missing. */
int means_encounter(DatFile *f, int id, int *count, int *first);
/* Kind elem of that encounter, 0 past the count. */
int means_kind_at(DatFile *f, int id, int elem, int *kind_id, char *name, int namecap);

/* The ENEM row whose id this is, or -1. */
int means_creature_row(DatFile *f, int id);
/* The SKIL row whose id this is, or -1. */
int means_skill_row(DatFile *f, int id);

/* Strength through Luck, or empty. */
const char *means_attr_name(int attr);

/* 1 when the id is in the book. attr is ATTR_* or -1. */
int means_skill(DatFile *f, int id, char *name, int namecap, int *attr);
int means_class_name(DatFile *f, int id, char *name, int namecap);
int means_creature_name(DatFile *f, int id, char *name, int namecap);

/*
 * Dungeon-type words next to DUN_TYPE_*. Id 19 and 255 are not place
 * types and return empty. atlas_type_name is a different word.
 */
const char *means_dungeon_type(int type);

#endif
