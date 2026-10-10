#ifndef DF_GLIDE_WRAP_H
#define DF_GLIDE_WRAP_H

#include "df.h"

/* The only Glide entry points the rest of the renderer calls. */

typedef struct DrawVert {
    float x, y;
    float oow, ooz;
    float sow, tow;
    float r, g, b;
} DrawVert;

int glide_open(void *hwnd);
void glide_close(void);
const char *glide_error(void);
void glide_clear(void);
void glide_swap(void);
void glide_triangle(const DrawVert *a, const DrawVert *b, const DrawVert *c);
int glide_palette(const df_u8 *r, const df_u8 *g, const df_u8 *b);
/*
 * cutout uploads GR_TEXFMT_ARGB_1555. Index 0 is transparent.
 * pr/pg/pb are the 256-entry palette and are used only for a cutout.
 */
int glide_upload(const df_u8 *pixels, int pot_w, int pot_h, int cutout,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u32 *addr);
void glide_bind(df_u32 addr);
/* Sprites sample texture alpha and reject alpha 0. Models use vertex alpha. */
void glide_sprite_alpha(int on);
/* Untextured iterated colour, drawn over the frame. on 0 restores the room. */
void glide_overlay(int on);

#endif
