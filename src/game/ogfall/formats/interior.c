#include "game/ogfall/formats/interior.h"

/* See interior.h. */

#define HEADER_BYTES 6776
#define SIZES_AT 1603
#define LIST_HEAD 17
#define MODEL_BYTES 66
#define FLAT_BYTES 17
#define SECTION_BYTES 16
#define DOOR_BYTES 19

static df_u32 u32(const df_u8 *p)
{
    return (df_u32)p[0] | (df_u32)p[1] << 8 | (df_u32)p[2] << 16 | (df_u32)p[3] << 24;
}

/* How many bytes the list at at holds, head included. */
static df_u32 list_bytes(const df_u8 *rmb, df_u32 at)
{
    return LIST_HEAD + (df_u32)rmb[at] * MODEL_BYTES + (df_u32)rmb[at + 1] * FLAT_BYTES +
           (df_u32)rmb[at + 2] * SECTION_BYTES + (df_u32)rmb[at + 3] * FLAT_BYTES + (df_u32)rmb[at + 4] * DOOR_BYTES;
}

int og_interior_people(const df_u8 *rmb, df_u32 size, int record, OgInteriorPerson *out, int cap)
{
    df_u32 pos, span, at, end;
    int i, people, n;

    if (!rmb || size < HEADER_BYTES || record < 0 || record >= 32 || record >= (int)rmb[0]) {
        return 0;
    }
    pos = HEADER_BYTES;
    span = 0;
    for (i = 0; i <= record; i++) {
        span = u32(rmb + SIZES_AT + (df_u32)i * 4u);
        if (span < LIST_HEAD || pos + span > size) {
            return 0;
        }
        if (i < record) {
            pos += span;
        }
    }
    end = pos + span;
    /* The inside follows the outside. */
    at = pos + list_bytes(rmb, pos);
    if (at + LIST_HEAD > end) {
        return 0;
    }
    people = rmb[at + 3];
    at += LIST_HEAD + (df_u32)rmb[at] * MODEL_BYTES + (df_u32)rmb[at + 1] * FLAT_BYTES + (df_u32)rmb[at + 2] * SECTION_BYTES;
    for (n = 0; n < people && n < cap && at + FLAT_BYTES <= end; n++, at += FLAT_BYTES) {
        out[n].image = (int)((unsigned)rmb[at + 12] | (unsigned)rmb[at + 13] << 8);
        out[n].faction = (int)((unsigned)rmb[at + 14] | (unsigned)rmb[at + 15] << 8);
        out[n].flags = rmb[at + 16];
    }
    return n;
}

int og_interior_shopkeeper(const OgInteriorPerson *person)
{
    return (person->flags & 4) != 0;
}
