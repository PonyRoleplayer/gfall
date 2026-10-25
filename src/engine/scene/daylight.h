#ifndef ENG_DAYLIGHT_H
#define ENG_DAYLIGHT_H

#include "engine/core/df.h"

/*
 * Outdoor light by the hour. Read docs/daylight.md before editing.
 *
 * One key light, the sun by day and the moon by night, and an ambient.
 * A face takes ambient + key * max(0, n . k) as one colour, through
 * Glide's constant colour register: no colour per vertex, so a lit
 * town costs the triangles it always did. Plain C with no Glide, so
 * hostcheck runs it.
 *
 * World axes: +x east, +y up, +z north. Yaw 0 looks north.
 */

typedef struct Daylight {
    /* Unit vector toward the key light. */
    float kx, ky, kz;
    /* Key light and ambient, 0..1 per channel. */
    float kr, kg, kb;
    float ar, ag, ab;
    /* Street lamps: 0 by day, 1 at night, between at dusk and dawn. */
    float lamps;
    /* 1 while the key is the sun. */
    int sun;
} Daylight;

/* The light at a minute of the day, 0..1440. */
void daylight_at(float minute, Daylight *out);

/*
 * The colour of a face at angle f to the key, f = max(0, n . k) cut to
 * DAYLIGHT_STEPS + 1 steps: table[i] is ambient + key * i / STEPS as
 * 0xRRGGBB. draw.c fills it once a frame and looks faces up in it.
 */
#define DAYLIGHT_STEPS 32
void daylight_table(const Daylight *dl, df_u32 *table);

/*
 * A sprite's colour. Flats turn to face the camera, so they all share
 * one normal: fx, fz is the horizontal way the camera looks. A flat the
 * sun is behind is not black: half the key comes round it.
 */
df_u32 daylight_sprite(const Daylight *dl, float fx, float fz);

/* Switch. Off draws outdoor places full bright, as before. Default on. */
void daylight_set_on(int on);
int daylight_on(void);

#endif
