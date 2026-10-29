#ifndef ENG_PIC_H
#define ENG_PIC_H

#include "engine/core/df.h"

/*
 * Pictures on the screen: a game's 2D pictures (Daggerfall's IMG and
 * CIF screens, the weapon's swing, the compass) drawn into a screen
 * rectangle. Read docs/pictures.md before editing.
 *
 * A picture is index pixels, row 0 at the top, under a palette of its
 * own (768 bytes, RGB). It goes on the card as P_8 and is drawn through
 * its palette. A cutout's index 0 is clear: its palette's entry 0 is the
 * key colour (PIC_KEY), which the card's chroma key drops; on a card
 * without the key it goes as ARGB 1555 instead, alpha-tested as sprites
 * are, twice the bytes. It is cut in square tiles of a
 * power of two, each its own texture, in a room on the card the game
 * sets (pic_set_room) and the place's pictures do without. The engine
 * keeps the tiles in memory, so they go back on the card with every
 * place.
 *
 * Draw a picture from gfx_set_under (under the HUD's text) or
 * gfx_set_extra (over it); the order of the calls is the order on the
 * screen. Coordinates are ui_string's: screen pixels from the lower
 * left, a rectangle given by its left and its top.
 *
 * Plain C: the card is reached through PicOps, so hostcheck runs it.
 */

/* Pictures loaded at once, tiles among them, palettes among the opaque ones. */
#define PIC_MAX 64
#define PIC_TILES 1024
#define PIC_PALETTES 16
/* The largest picture's sides. */
#define PIC_SIDE_MAX 1024
/* The largest tile a card takes, and the smallest a thin strip is padded to. */
#define PIC_TILE_MAX 256
#define PIC_TILE_MIN 16
/* A cutout's key, its palette's entry 0: magenta, which no other entry is let land on. */
#define PIC_KEY 0xff00ffu
/* How a picture is drawn (PicOps.begin): through its palette, keyed through it, or alpha-tested 1555. */
enum {
    PIC_OPAQUE,
    PIC_KEYED,
    PIC_ALPHA
};

/*
 * The room on the card for pictures, in KB, 0 (default) to 1024: a
 * viewer that draws none gives the place all of it. It is taken at the
 * next place (gfx_prepare), before the place's own pictures, so they
 * are budgeted to what is left. A picture that does not fit is kept
 * and not drawn, with a warning.
 */
void pic_set_room(int kb);
int pic_room(void);

/*
 * A picture of w x h (1..PIC_SIDE_MAX each), copied: the caller may free
 * its pixels and palette after. cutout: index 0 is clear. Returns its
 * id (1..PIC_MAX), 0 when there is no slot or memory; on the card now
 * when there is room, at the next place otherwise.
 */
int pic_load(const df_u8 *pixels, int w, int h, const df_u8 *rgb, int cutout);
void pic_free(int id);
/* Its size; 0 for an id not loaded. */
int pic_size(int id, int *w, int *h);
/* The whole picture into the rectangle w x h whose top left is x, top. */
void pic_draw(int id, int x, int top, int w, int h);
/*
 * Part of it, sx, sy (from its top left), sw x sh, into the rectangle,
 * at opacity alpha 0..255 (255 is opaque, no blend): a frame of a
 * sheet, a strip of the compass, a fade.
 */
void pic_draw_part(int id, int sx, int sy, int sw, int sh, int x, int top, int w, int h,
                   int alpha);
/* Pictures loaded, tiles on the card, the card bytes they use, the room's bytes. */
void pic_stats(int *pics, int *tiles_on, df_u32 *used, df_u32 *room);

/*
 * The card's side, for gfx.c and the host test. Each tile is a square
 * of side, one level: P_8 indices, or ARGB 1555 little-endian (wide: a
 * cutout on a card without the chroma key).
 */
typedef struct PicOps {
    /* The card's bytes for a tile. */
    df_u32 (*bytes)(int side, int wide);
    /* Send a tile to addr; its handle. 0 when it failed. */
    int (*upload)(const df_u8 *texels, int side, int wide, df_u32 addr, df_u32 *handle);
    void (*release)(df_u32 handle);
    /*
     * A draw: its state on (how: PIC_OPAQUE, PIC_KEYED, PIC_ALPHA), its
     * palette (id 0..PIC_PALETTES-1; not for PIC_ALPHA), quads, off.
     */
    void (*begin)(int how, int alpha);
    void (*palette)(int id, const df_u32 *table);
    /* A tile's part: screen left, top, right, bottom; texel s, t (0..256 across the tile) at those edges. */
    void (*quad)(df_u32 handle, float x0, float y0, float x1, float y1,
                 float s0, float t0, float s1, float t1);
    void (*end)(void);
    /* 1: the card has the chroma key, and a cutout goes as P_8 keyed. */
    int chroma;
} PicOps;

/*
 * The card was emptied and its room for pictures is lo..hi (both 0:
 * none): every picture goes back on it, as many as fit. gfx_prepare.
 */
void pic_begin(df_u32 lo, df_u32 hi, const PicOps *ops);
/* The card is gone: no picture is on it. gfx_shutdown. */
void pic_end(void);

#endif
