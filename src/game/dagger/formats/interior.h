#ifndef GFALL_INTERIOR_H
#define GFALL_INTERIOR_H

#include "engine/core/df.h"

/*
 * The people standing inside a building, read from an RMB block's bytes. GFALL reads this itself (the decomp's
 * rmb_add_people and block_flat, include/records.h 719; the block's layout as DFU's RMB notes have it) rather
 * than take the dagger layer's reading: the dagger layer keeps a person's faction and picture but not their flags,
 * and the flags say who keeps the shop (rmb_add_people makes bit 4 of a person's flags the record's bit 8, the shopkeeper).
 *
 * A block is a 6776-byte header (the number of buildings, their places and data, the sizes of their lists at 1603),
 * then each building's two lists one after the other: the outside, then the inside. A list is five counts and
 * twelve bytes of pointers, then models (66 bytes), flats (17), section three (16), people (17) and doors (19).
 */

#define INTERIOR_PEOPLE 16

typedef struct InteriorPerson {
    /* The picture (archive << 7 | record) and the faction the block names, and the block's flag byte. */
    int image, faction, flags;
} InteriorPerson;

/* The people of building record's inside; how many (at most cap). 0 when the block or the record does not read. */
int interior_people(const df_u8 *rmb, df_u32 size, int record, InteriorPerson *out, int cap);
/* person flags bit 8: the shopkeeper (rmb_add_people copies the block flag 4 to it). */
int interior_shopkeeper(const InteriorPerson *person);

#endif
