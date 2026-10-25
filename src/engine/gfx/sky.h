#ifndef ENG_GFX_SKY_H
#define ENG_GFX_SKY_H

#include "engine/assets/sky.h"

/*
 * The sky on the card. Read docs/daylight.md before editing.
 *
 * The circle of the horizon, SKY_W by SKY_H, goes up as SKY_TILES
 * squares of 256, P_8 in the sky's own palette: 64 KB each, 256 KB in
 * all, uploaded before the level's pictures so the texture budget sees
 * the room left. Rows past SKY_H repeat the horizon row, so a clamped
 * read below the picture is the horizon. A new picture is read and
 * sent to the same places only when the hour changes the sky's key.
 */
#define SKY_TILES 4
#define SKY_TILE 256
/*
 * The dome above the picture: SKY_CAP squared RGB 565 (32 KB), made from
 * the picture each time it changes. Column u is the top rows' colour of
 * the picture's SKY_W / SKY_CAP columns at u; it blends down the rows to
 * the top rows' mean at the last row, the zenith. Stretching the top row
 * up to the zenith instead streaked it into a starburst overhead.
 */
#define SKY_CAP 128

/*
 * Reserve the tiles and send the picture for minute and the weather's
 * sky (WEATHER_SKY_*). 0 when there is no room.
 */
int sky_prepare(const Sky *sky, float minute, int weather);
/* Forget the tiles: the card was reset. */
void sky_reset(void);
/* Send the picture for minute and weather when its key differs from the one on the card. */
void sky_update(float minute, int weather);
/* 1 when a sky is on the card. */
int sky_ready(void);
df_u32 sky_tile(int i);
df_u32 sky_cap(void);
/* The palette the tiles are drawn with, 768 bytes. */
const df_u8 *sky_palette(void);
/* Mean colour of the bottom rows (the fog) and of the top row (the zenith). */
void sky_horizon(int *r, int *g, int *b);
void sky_zenith(int *r, int *g, int *b);
/* Switch. Off draws outdoor places on the viewer's background, as before. */
void sky_set_on(int on);
int sky_on(void);

#endif
