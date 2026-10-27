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
     * The picture for a minute of the day, 0..1440, and the weather's
     * sky (WEATHER_SKY_*, engine/scene/weather.h), as a key that is the
     * same for every moment that shows the same picture. The engine
     * reads the picture again only when the key changes.
     */
    int (*pick)(const struct Sky *sky, float minute, int weather);
    /* Fills pixels (SKY_W * SKY_H) and rgb (768) for a key. 0 on failure. */
    int (*read)(const struct Sky *sky, int key, df_u8 *pixels, df_u8 *rgb);
    /*
     * The same a piece a call, part 0 first, so a picture changing as
     * the hour turns is not 224 KB of the 233's disk in one frame: 1
     * when more parts are to come, 0 when pixels and rgb are whole, -1
     * when it failed. 0 (none): the engine reads with read, at once.
     */
    int (*read_part)(const struct Sky *sky, int key, int part, df_u8 *pixels, df_u8 *rgb);
    /* The game's own: the fair-weather sky file, the climate's first, the night picture. */
    int index;
    int base;
    int night;
} Sky;

#endif
