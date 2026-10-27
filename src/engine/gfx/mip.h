#ifndef ENG_MIP_H
#define ENG_MIP_H

#include "engine/core/df.h"

/*
 * Mip chains and the texture budget. Plain C, no Glide, so hostcheck
 * tests it; glide.c uploads what it builds. See docs/glide.md.
 *
 * A picture is a square power-of-two index image, 1..256 on a side.
 * drop is how many levels below full size its first level is: 0 is the
 * picture itself, 1 is half the side. chain nonzero adds every smaller
 * level down to 1 x 1, largest first, packed with no gap, the layout
 * grTexDownloadMipMap reads.
 */

/* Most bytes mip_build writes: a full 256 chain at 16 bits. */
#define MIP_MAX_BYTES (2u * (256u * 256u + 128u * 128u + 64u * 64u + 32u * 32u \
                             + 16u * 16u + 8u * 8u + 4u * 4u + 2u * 2u + 1u))
/*
 * The one index filtering keeps: a window's glass, which a night pass
 * draws again through a palette of its own (docs/daylight.md). It is
 * black in Daggerfall's palette, as index 0 is, and the nearest search
 * always answers 0 for black, so filtering never makes glass. A texel
 * of a smaller level, or of a wall stretched smoothly, is glass when at
 * least half of what it was made from was.
 */
#define MIP_GLASS 255
/* The budget never shrinks a picture below this side. */
#define MIP_MIN_SIDE 8
/*
 * Levels each wall gives before any sprite gives one: a blurry wall
 * reads as distance, a blurry hand-drawn sprite as a fault. Past that
 * the largest gives first, wall or sprite, so no wall is ruined for a
 * sprite's sake.
 */
#define MIP_WALL_GRACE 2

/*
 * The palette mip_nearest answers for. A different palette empties the
 * remembered answers; the same one keeps them. mip_build sets it.
 */
void mip_palette(const df_u8 *pr, const df_u8 *pg, const df_u8 *pb);
/*
 * The palette entry nearest r, g, b (0..255 each), searched once per
 * 5-bit-per-channel cell and remembered: the cell's centre stands for
 * every colour in it. For colours made by filtering. A colour that came
 * from the palette should keep its own index instead. 0 before any
 * mip_palette.
 */
df_u8 mip_nearest(int r, int g, int b);

/* Texels in the levels mip_build would write. */
df_u32 mip_texels(int side, int drop, int chain);

/*
 * Writes the levels into out, at least MIP_MAX_BYTES. A cutout is
 * ARGB 1555, little-endian, index 0 transparent: transparent texels
 * take the mean colour of their opaque neighbours, so filtering does
 * not pull an edge toward black, and a smaller level is opaque where
 * at least half its area was. Otherwise it is 8-bit indices: the full
 * level is the picture itself, and a smaller level is each 2 x 2 mean
 * colour mapped back to the nearest palette entry. Returns the bytes
 * written, 0 for a bad shape.
 */
df_u32 mip_build(const df_u8 *index, int side, int cutout, int drop, int chain,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u8 *out);

/*
 * Chooses drop[i] for n pictures so their bytes fit in room. bytes(side,
 * cutout) is what one level-0 upload of that side takes on the card,
 * chain included when the caller uses chains. side[i] < 1 is a picture
 * that is not uploaded. Starting from full size, a wall (cutout 0) that
 * has lost fewer than MIP_WALL_GRACE levels and takes the most bytes of
 * those loses its top level; when none is left, the picture that takes
 * the most bytes, wall or sprite; until the total fits or every picture
 * is at MIP_MIN_SIDE. Ties go to the lower index. Returns the total
 * bytes chosen.
 */
df_u32 mip_budget(const int *side, const df_u8 *cutout, int n, df_u32 room,
                  df_u32 (*bytes)(int side, int cutout), int *drop);

#endif
