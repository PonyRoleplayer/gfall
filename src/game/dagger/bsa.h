#ifndef DF_BSA_H
#define DF_BSA_H

#include "engine/core/df.h"

/*
 * BSA directory. Records are packed from byte 4 in directory order.
 * Numbered (type 0x0200): uint32 id, uint32 size. The high half of a large
 * id is part of the id, not a compression flag. The first copy of a
 * duplicated id is the one that is kept.
 * Named (type 0x0100): 12-byte name, uint16, uint32 size.
 */

typedef struct BsaRec {
    df_u32 id;
    char name[13];
    df_u32 offset;
    df_u32 size;
} BsaRec;

typedef struct Bsa Bsa;

Bsa *bsa_open(const char *rel);
void bsa_close(Bsa *bsa);
int bsa_named(const Bsa *bsa);
int bsa_count(const Bsa *bsa);
/* Directory order after the numbered sort. 0 when index is out of range. */
const BsaRec *bsa_at(const Bsa *bsa, int index);

const BsaRec *bsa_find_id(const Bsa *bsa, df_u32 id);
const BsaRec *bsa_find_name(const Bsa *bsa, const char *name);
df_u8 *bsa_read(Bsa *bsa, const BsaRec *rec);

#endif
