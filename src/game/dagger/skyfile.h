#ifndef DF_SKYFILE_H
#define DF_SKYFILE_H

#include "engine/assets/sky.h"

/*
 * Daggerfall's skies, for an outdoor place. Read docs/daylight.md.
 *
 * SKY00.DAT .. SKY31.DAT: 32 palettes of 776 bytes (8 of header, then
 * 768 of 8-bit RGB), 32 colour maps of 16384 bytes, then 64 pictures of
 * 512 x 220, one byte per pixel: record 0 is frames 0..31, record 1
 * frames 32..63 of the same sky. Record 1 is the half from north
 * through east to south, record 0 the half from south through west to
 * north. Frame 0 is dawn, 31 noon; the afternoon runs the same frames
 * back with the halves swapped, so the glow is in the west.
 *
 * The file is the climate's sky base plus the season. Night is one of
 * NITE00I0.IMG .. NITE03I0.IMG, 512 x 219 with no header, palette
 * NIGHTSKY.COL, the same picture on both halves, with stars dropped on.
 */

#define SEASON_FALL 0
#define SEASON_SPRING 1
#define SEASON_SUMMER 2
#define SEASON_WINTER 3

/* The sky file for a climate id and season, 0..31. */
int skyfile_index(int climate, int season);
/* Fills sky for that climate and season. */
void skyfile_open(int climate, int season, Sky *sky);
/* The day frame, 0..63, at a minute between 06:00 and 18:00. */
int skyfile_frame(float minute);

#endif
