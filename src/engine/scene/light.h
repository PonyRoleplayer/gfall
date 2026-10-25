#ifndef ENG_LIGHT_H
#define ENG_LIGHT_H

#include "engine/scene/scene.h"

/*
 * Point lights on a Voodoo 1. Read docs/lighting.md before editing.
 *
 * The scene is drawn as texture times a dark ambient colour, held in
 * Glide's constant colour, so no triangle carries colour gradients.
 * Then each light that the cull says is visible lays light decals over
 * the faces it reaches: polygons cut from those faces at load, in their
 * planes, textured with a baked falloff and blended dst + dst * src.
 * A decal's edge texel is black, so it leaves the wall exactly as it
 * was; its centre doubles the wall. Sprites take one colour each, from
 * the lights around them. Flats that give light are drawn full bright.
 *
 * Plain C with no Glide, so hostcheck runs it. The decals live on the
 * heap the level was loaded on and die with it.
 */

/* Falloff pictures, one per band of the light's height over the face. */
#define LIGHT_VARIANTS 8
#define LIGHT_TEX 64

/*
 * One cut of one face by one light. corners index light_corners():
 * x, y, z in the world, then u, v across the falloff picture, 0..1.
 * The face is lit on the side where nx * x + ny * y + nz * z + d > 0,
 * and a camera sees the decal only from there.
 */
typedef struct Decal {
    int first;
    int n;
    int variant;
    float nx, ny, nz, d;
    /* World box around the corners, for the per-frame cull. */
    float cx, cy, cz, ex, ey, ez;
} Decal;

/* Floats per decal corner in light_corners(). */
#define LIGHT_CORNER 5

/*
 * Cuts the decals for every light of the scene, and works out each
 * sprite's light. Returns the decal count, or -1 when the heap ran
 * out (the place is then unlit). A second call replaces the first.
 */
int light_build(const Scene *scene);
/* Lights of the built scene; 0 outdoors. */
int light_count(void);
/* World box around light i's decals. 0 when it lights nothing. */
int light_box(int i, float *cx, float *cy, float *cz, float *ex, float *ey, float *ez);
/* How far light i reaches: its radius times light_range(). */
float light_reach(int i);
const Light *light_at(int i);
/* Light i's decals. Returns how many. */
int light_decals(int i, const Decal **out);
const float *light_corners(void);
int light_decal_total(void);

/*
 * 1 when the scene should be drawn lit: lights on, and the built scene
 * has lights. Outdoor places and RENDERFALL are never lit.
 */
int light_active(void);

/*
 * The colour sprite s is drawn with, as 0xRRGGBB: the ambient raised
 * by the lights around it, or white for a flat that gives light. White
 * when the scene is not lit.
 */
df_u32 light_sprite_rgb(int s);
/* The ambient as 0xRRGGBB. White when the scene is not lit. */
df_u32 light_ambient_rgb(void);

/* LIGHT_TEX x LIGHT_TEX texels of RGB 565, row 0 first: falloff picture v. */
void light_texture(int variant, df_u16 *out);

/* Switches. Default on. Off draws every place full bright, as before. */
void light_set_on(int on);
int light_on(void);
/* Ambient brightness, 0..255. Default 112, a little under half. */
void light_set_ambient(int level);
int light_ambient(void);
/*
 * How many times its data radius a light reaches, 1..8. Default 3, as
 * Daggerfall Unity's range. The decals are cut for one value; a change
 * needs light_build again (gfx_set_light_range does that).
 */
void light_set_range(int scale);
int light_range(void);
/*
 * The debug picture: a square at each light, a ring where it ends, for
 * lights within this many world units of the camera, 0..LIGHT_VIEW_MAX.
 * 0 (default) is off. Drawing every light up to an 8192 far clip filled
 * the glass with rings and could send the card a line it hangs on.
 */
#define LIGHT_VIEW_MAX 2048
void light_set_view(int units);
int light_view(void);

#endif
