#ifndef DF_GLIDE_WRAP_H
#define DF_GLIDE_WRAP_H

#include "engine/core/df.h"

/*
 * The only Glide entry points the rest of the renderer calls. What each
 * state costs the card and the CPU, and why it is set the way it is, is
 * docs/glide.md.
 */

/*
 * Most corners glide_polygon takes: an ARCH3D face of up to 24 points
 * (every point is kept, see arch3d.c), clipped by 6 planes, plus slack.
 */
#define GLIDE_POLY_MAX 32

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
/* Clear and fog colour. 0,0,0 until a viewer sets its own. */
void glide_background(int r, int g, int b);
/*
 * While on, the clear and the fog take this colour instead of the
 * background: an outdoor place's horizon. Off goes back to the
 * background. Sends the fog colour only when it changes.
 */
void glide_sky_color(int on, int r, int g, int b);
/*
 * Distance fog. 1 uses the table built at open. 0 is GR_FOG_DISABLE.
 * Default is on. Safe before glide_open; the next open obeys it.
 */
void glide_fog(int on);
int glide_fog_on(void);
/* Eye-space distances where the table is clear and where it is solid. */
void glide_fog_range(float near_z, float far_z);
void glide_swap(void);
/*
 * 1 is the W-buffer (default): depth from 1/w, about distance / 4096
 * apart at any range. 0 is the Z-buffer on camera_project's ooz. Takes
 * effect on the next clear.
 */
void glide_set_wbuffer(int on);
int glide_wbuffer(void);
/* 1 sends polygons as one planar polygon when the DLL has it (default). */
void glide_set_planar(int on);
/* 1 when polygons really go planar: on, and the DLL exports it. */
int glide_planar(void);
/* 1 waits for the retrace on each swap (default), 0 swaps at once. */
void glide_set_vsync(int on);
int glide_vsync(void);
/* Logs the DLL's board, TMU, mode, and texture room at this level. */
void glide_report(int level);
/*
 * Copy the 640x480 front buffer into dst, 16-bit 565, stride in bytes.
 * Writes that picture back to the back buffer. 0 if the driver has no
 * linear frame buffer entry, or the copy fails.
 */
int glide_lfb_read(void *dst, int stride_bytes);
int glide_lfb_write(const void *src, int stride_bytes);
void glide_triangle(const DrawVert *a, const DrawVert *b, const DrawVert *c);
/* A convex planar polygon of 3..GLIDE_POLY_MAX corners, in order. */
void glide_polygon(const DrawVert *v, int n);
void glide_line(const DrawVert *a, const DrawVert *b);
int glide_palette(const df_u8 *r, const df_u8 *g, const df_u8 *b);
/*
 * cutout uploads GR_TEXFMT_ARGB_1555. Index 0 is transparent.
 * pr/pg/pb are the 256-entry palette and are used only for a cutout.
 */
/* Bytes still free on TMU0, from the upload cursor to grTexMaxAddress. */
df_u32 glide_tex_free(void);
/*
 * Debug: let TMU0 hold at most kb KB, as a smaller card would; 0 is the
 * card's own. Takes effect for the next uploads: send the level again
 * (gfx_set_texcap does). glide_tex_card is the card's own size in KB.
 */
void glide_set_tex_cap(df_u32 kb);
df_u32 glide_tex_cap(void);
df_u32 glide_tex_card(void);
/*
 * A square picture, side 1..256, through mip_build: its first level is
 * drop levels below full size, and chain adds the smaller levels down
 * to 1 x 1. handle receives the upload's id for glide_bind. A cutout's
 * transparent texels take their neighbours' colour, so filtering does
 * not darken the edge.
 */
int glide_upload(const df_u8 *pixels, int side, int cutout, int drop, int chain,
                 const df_u8 *pr, const df_u8 *pg, const df_u8 *pb, df_u32 *handle);
/* A square RGB 565 picture, one level, not mipmapped: the light falloff. */
int glide_upload_565(const df_u16 *pixels, int side, df_u32 *handle);
/* Bytes a texture whose first level is side takes, with the 8-byte pad. */
int glide_texture_bytes(int side, int cutout, int chain, df_u32 *bytes);
/* Forget every upload: the next one starts at the bottom of TMU0. */
void glide_tex_reset(void);
/*
 * 1 (default) is GR_MIPMAP_NEAREST, and texcache uploads chains. The
 * mode changes at once; textures already on the card keep their levels
 * until they are uploaded again (gfx_set_mipmap does that).
 */
void glide_set_mipmap(int on);
int glide_mipmap(void);
/*
 * Remember the upload pointer. glide_rewind drops every map uploaded
 * after the mark so a nearer set can reuse that part of the card.
 * The small copies and the font stay put.
 */
void glide_mark(void);
void glide_rewind(void);
/* Bytes free after the mark, with a pad so a map is not packed to the edge. */
df_u32 glide_detail_room(void);
/* A handle from glide_upload. Binding the texture already bound is free. */
void glide_bind(df_u32 handle);
/*
 * Sprites and text take alpha from the texture and keep a texel past
 * half. Models use a constant alpha: no alpha travels with a triangle.
 */
void glide_sprite_alpha(int on);
/* Untextured iterated colour, drawn over the frame. on 0 restores the room. */
void glide_overlay(int on);
/* Same as overlay, at about 75% opacity, so the frame shows through. */
void glide_plate(int on);
/* Untextured colour at an explicit opacity, 0..255. 0 restores the room. */
void glide_dim(int on, int alpha);
/*
 * Blend at opacity 0..255, held in the constant colour, not in the
 * vertices. texture nonzero multiplies the texture alpha by it, for a
 * fading glyph. texture zero uses it alone, for flat glyphs. A negative
 * alpha turns blending off.
 */
void glide_opacity(int alpha, int texture);
/* Textured screen quads, point sampled, drawn over the frame. */
void glide_ui(int on);
/* Untextured iterated colour with the room's depth test left alone. */
void glide_flat(int on);
/*
 * The light the scene is drawn in, 0xRRGGBB, held in the constant
 * colour. White (default) draws textures as they are. Set white again
 * before drawing the HUD.
 */
void glide_tint(df_u32 rgb);
df_u32 glide_tinted(void);
/*
 * The light decal pass: dst + dst * texture * tint, no depth write, no
 * fog, clamped. Set the tint first: white indoors, the lamps' level
 * outdoors.
 */
void glide_decal(int on);
/*
 * A square P_8 picture of one level, drawn through the palette on TMU0
 * at the time: the sky's tiles. glide_replace sends new texels of the
 * same shape to the same place on the card.
 */
int glide_upload_p8(const df_u8 *pixels, int side, df_u32 *handle);
/*
 * The font sheet: ink (nonzero) or clear, ALPHA_INTENSITY_44, one byte a
 * texel, white; draw it with the ink's colour as the tint.
 */
int glide_upload_ai44(const df_u8 *ink, int side, df_u32 *handle);
int glide_replace(df_u32 handle, const df_u8 *pixels);
/*
 * The sky pass: rgb (768 bytes) on TMU0 as the palette, no depth test,
 * no fog, clamped. Off puts the level's palette and the state back.
 */
void glide_sky(int on, const df_u8 *rgb);
/*
 * The windows' night pass: the palette on TMU0 black but for MIP_GLASS,
 * which is rgb (0xRRGGBB); the texture times the constant colour, added
 * onto the frame (ONE, ONE); the depth test passing on equal, so the
 * same face drawn again lands on itself; no depth write, no fog. Off
 * puts the level's palette and the scene's state back.
 */
void glide_glow(int on, df_u32 rgb);

#endif
