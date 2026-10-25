#ifndef ENG_SKY_H
#define ENG_SKY_H

#include "engine/core/df.h"

/*
 * The sky an outdoor place draws behind everything. The game owns the
 * pictures; the engine asks for one when the hour changes it. Read
 * docs/daylight.md before editing.
 *
 * A picture is the whole circle of the horizon: SKY_W columns, column 0
 * looking north and the columns running clockwise seen from above (east
 * at SKY_W / 4), and SKY_H rows, row 0 the top and the last row on the
 * horizon. One byte per pixel, indices into its own 256-colour palette,
 * 8 bits per channel.
 */
#define SKY_W 1024
#define SKY_H 220

typedef struct Sky {
    /*
     * The picture for a minute of the day, 0..1440, as a key that is
     * the same for every minute that shows the same picture. The engine
     * reads the picture again only when the key changes.
     */
    int (*pick)(const struct Sky *sky, float minute);
    /* Fills pixels (SKY_W * SKY_H) and rgb (768) for a key. 0 on failure. */
    int (*read)(const struct Sky *sky, int key, df_u8 *pixels, df_u8 *rgb);
    /* The game's own: which sky file, which night picture. */
    int index;
    int night;
} Sky;

#endif
