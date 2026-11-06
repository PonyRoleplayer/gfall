#ifndef ENG_MIP_H
#define ENG_MIP_H

#include "engine/core/df.h"

/*
 * Mip chains and the texture budget. No Glide or OS dependencies, so
 * hostcheck tests it; glide.c uploads what it builds. See docs/glide.md.
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
 * Texture detail, the player's (docs/memory.md, Detail): how sharp the
 * world's pictures may be, walls and sprites apart. A level is how many
 * top levels every picture of its kind gives up before any budget runs:
 * High none, Medium one (half the side, a quarter of the bytes), Low two
 * (a quarter of the side, a sixteenth of the bytes). The smaller levels
 * stay below it for distance, as a chain does.
 *
 * Walls are opaque pictures: a model's walls, floors and ceilings, the
 * land's tiles. Sprites are cutouts: flats, creatures, people, flora.
 * The engine's own pictures (the sky, the font, the light falloff) and
 * a game's screen pictures (pic.c: the weapon, the HUD, every screen)
 * never pass through here, so the setting leaves them as they are.
 */
#define MIP_DETAIL_LOW 0
#define MIP_DETAIL_MEDIUM 1
#define MIP_DETAIL_HIGH 2

/* Each 0..2 (clamped), MIP_DETAIL_HIGH at the start. Later builds and budgets use it. */
void mip_set_detail(int walls, int sprites);
int mip_wall_detail(void);
int mip_sprite_detail(void);
/*
 * The top levels the detail takes off a picture of side: a cutout's by
 * the sprites' detail, else the walls'. Never below MIP_MIN_SIDE, so a
 * small picture keeps more of itself (an 8 x 8 loses nothing, a 16 one
 * level at most); 0 at High.
 */
int mip_detail_drop(int side, int cutout);

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
 * Optional original-MMX averaging, on by default when the CPU has it.
 * The getter reports effective use, so it stays false on unsupported
 * CPUs even after set(1). Changes affect later builds, not stored chains.
 */
void mip_set_mmx(int on);
int mip_mmx(void);

/* Independent of mip_mmx: optional MMX RGBA-to-1555 packing, same defaults. */
void mip_set_pack_mmx(int on);
int mip_pack_mmx(void);

/* Independent optional MMX indexed rectangle sums, on when supported. */
void mip_set_box_mmx(int on);
int mip_box_mmx(void);

/*
 * Fixed-box indexed reduction to cols*rows RGB byte tuples. Source is
 * tightly packed width*height indices, each dimension 1..256; output
 * dimensions are 1..width and 1..height. Each output covers exactly
 * floor(width/cols) by floor(height/rows) source pixels. Right/bottom
 * remainders are IGNORED, not distributed or wrapped. Sum original
 * palette colours, divide once with truncation; index 0 is ordinary
 * colour, with no alpha or cutout rule. Installs mip_palette so callers
 * may map these means through mip_nearest, which remains scalar.
 * Buffers do not overlap; any byte alignment. No image-sized scratch
 * or allocation. Returns 1, or 0 for bad shape/null without output.
 * See docs/streaming.md, Far ground, before changing these semantics.
 */
int mip_box_rgb(const df_u8 *index, int width, int height, int cols, int rows,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u8 *rgb);

/*
 * Pack count RGBA byte tuples to little-endian ARGB1555: discard the
 * low three colour bits and set bit 15 exactly when alpha >= 128.
 * Colour survives even when transparent (the cutout's bleed). Input
 * has 4*count bytes, output room for 2*count; buffers do not overlap.
 * Both pointers may have any byte alignment. NULL or count 0 does
 * nothing. The bulk path and scalar tail never read beyond count.
 * emit uses this primitive; cutout_top keeps its indexed shortcut.
 */
void mip_pack_1555(const df_u8 *rgba, df_u32 count, df_u8 *out);

/*
 * Shrink a square RGBA buffer in place, keeping exactly
 * (a + b + c + d + 2) >> 2 in each byte, including alpha. Each df_u32
 * holds four independent byte lanes; only the first (side/2)^2 words
 * are overwritten. side is a power of two, 2..256; other sizes (and
 * NULL) do nothing. No extra buffer or 8-byte alignment is required.
 * mip_build uses this same primitive for every smaller level.
 */
void mip_halve_rgba(df_u32 *rgba, int side);

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
 * that is not uploaded. Starting from the detail's size (mip_detail_drop;
 * full size at High), a wall (cutout 0) that has lost fewer than
 * MIP_WALL_GRACE levels past it and takes the most bytes of those loses
 * its top level; when none is left, the picture that takes the most
 * bytes, wall or sprite; until the total fits or every picture is at
 * MIP_MIN_SIDE. Ties go to the lower index. Returns the total bytes
 * chosen.
 */
df_u32 mip_budget(const int *side, const df_u8 *cutout, int n, df_u32 room,
                  df_u32 (*bytes)(int side, int cutout), int *drop);

#endif
