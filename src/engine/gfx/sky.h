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

/* Reserve the tiles and send the picture for minute. 0 when there is no room. */
int sky_prepare(const Sky *sky, float minute);
/* Forget the tiles: the card was reset. */
void sky_reset(void);
/* Send the picture for minute when its key differs from the one on the card. */
void sky_update(float minute);
/* 1 when a sky is on the card. */
int sky_ready(void);
df_u32 sky_tile(int i);
/* The palette the tiles are drawn with, 768 bytes. */
const df_u8 *sky_palette(void);
/* Mean colour of the bottom rows (the fog) and of the top row (the zenith). */
void sky_horizon(int *r, int *g, int *b);
void sky_zenith(int *r, int *g, int *b);
/* Switch. Off draws outdoor places on the viewer's background, as before. */
void sky_set_on(int on);
int sky_on(void);

#endif
