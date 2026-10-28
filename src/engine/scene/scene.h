#ifndef ENG_SCENE_H
#define ENG_SCENE_H

#include "engine/assets/clip.h"
#include "engine/assets/mesh.h"
#include "engine/gfx/camera.h"

/*
 * A placed mesh. m[] takes a mesh point into the world:
 *   wx = m[0]*lx + m[1]*ly + m[2]*lz + x
 *   wy = m[3]*lx + m[4]*ly + m[5]*lz + y
 *   wz = m[6]*lx + m[7]*ly + m[8]*lz + z
 * The matrix is right-hand X, then Y, then Z. A positive Y angle takes
 * local +X toward -Z. World +Y is up: a floor is one constant Y.
 */

typedef struct Instance {
    Mesh *mesh;
    float x, y, z;
    float m[9];
} Instance;

/*
 * A camera-facing quad, centered on x,y,z. w and h are the full size in
 * world units. slot is 0xffff until a loader assigns a texture.
 * file and image are loader keys. The rasterizer uses slot.
 */

typedef struct Flat {
    float x, y, z;
    float w, h;
    df_u16 file;
    df_u16 image;
    df_u16 slot;
    /* 1 for a picture that gives light, a torch or a candle: drawn full bright. */
    df_u16 glow;
    /*
     * An animated flat (its picture has frames, Image.frames): rate
     * frames a second, starting at frame phase. See docs/sprites.md.
     */
    df_u8 rate;
    df_u8 phase;
    /* The loader's mark, carried with the flat (a person, say). The engine does not read it. */
    df_u16 tag;
    /*
     * Drawn only within sight * 16 units of the camera, across the
     * ground; 0 at any distance. A streamed world's trees and rocks:
     * there are too many to draw to the far clip (docs/streaming.md).
     * Such a flat is flora to the console's switches (scene_set_flora).
     */
    df_u16 sight;
    /*
     * Flora's rank, 0..255, spread evenly: drawn when under the
     * density's share of 256 (scene_set_flora_density). In the struct's
     * padding. 0 for anything else.
     */
    df_u8 thin;
    /* 1: drawn mirrored left to right (an entity's view facing the viewer's right). */
    df_u8 flip;
    /* 1: drawn on frame of its sheet, not by its rate (an entity's held frame); 0 for its cycle. */
    df_u8 held;
    df_u8 frame;
} Flat;

/*
 * A point light, in world units. radius is where the place's data says
 * the light ends; the engine scales it (docs/lighting.md). flicker 1 is
 * a live flame, whose light wavers; 0 burns steady.
 */
typedef struct Light {
    float x, y, z;
    float radius;
    df_u8 flicker;
} Light;

/*
 * A sound from a point: a torch's fire, a cat. Full volume within
 * inner, silent past reach (world units), linear between. every 0
 * loops it while the listener is within reach; above 0 it plays now
 * and then while within reach, every that many seconds on average.
 * The clip is the game's and outlives the place. Not "near" and
 * "far": windows.h defines both as empty macros. See docs/sound.md.
 */
typedef struct Emitter {
    float x, y, z;
    const SoundClip *clip;
    /* The game's word for it ("fire", "pig"), for sndview and sndlist. */
    const char *label;
    float inner;
    float reach;
    float volume;
    float every;
} Emitter;

/*
 * A model that moves when used or set off: a door on its hinge, a
 * lever, a lift (docs/interact.md). It eases between where the place
 * put it (start) and that pose turned by rx, ry, rz about its own axes
 * (the model's origin is the hinge) and moved by tx, ty, tz in the
 * world (end), linear over secs. Starting it plays it toward whichever
 * end it is not at, and starts next with it, as Daggerfall's chained
 * actions do. inst is the scene's model, written as it moves; the rest
 * is the game's to fill, and state, t and the start pose the engine's.
 */
enum {
    MOVER_START,
    MOVER_FORWARD,
    MOVER_END,
    MOVER_BACK
};

typedef struct Mover {
    Instance *inst;
    int model;
    float rx, ry, rz;
    float tx, ty, tz;
    float secs;
    /* The mover that starts with this one, -1 for none. */
    int next;
    /* Above 0, it will not move when used (a locked door); a chain still moves it. */
    int lock;
    /* 1: only ever plays forward (a portcullis that stays up). */
    df_u8 once;
    /* 1 for a door: "locked" is said of it, and open and shut are its words. */
    df_u8 door;
    /* Played from the model when it starts toward the end, and back. 0 for none. */
    const SoundClip *go_sound;
    const SoundClip *back_sound;
    /*
     * 1 for a teleport (docs/interact.md): it does not move; used, or a
     * chain reaching it, sends the player's eye to to_x, to_y, to_z,
     * facing to_yaw (Camera.yaw), each time.
     */
    df_u8 teleport;
    float to_x, to_y, to_z, to_yaw;
    /* The engine's. */
    df_u8 state;
    float t;
    float x0, y0, z0;
    float m0[9];
} Mover;

/*
 * Something the player can use: look at it within reach and press the
 * use key (docs/interact.md). It is a model (model >= 0: its box, as it
 * stands now), a sprite (sprite >= 0), or a box of its own: centre
 * cx, cy, cz, facing nx, nz across the floor (a door's outward normal),
 * half sizes hw across that face, hh up, hd along the normal.
 *
 * kind says what using it does. USE_MOVER starts mover, which the
 * engine does itself. The others the engine hands to the game's handler
 * (interact_set_handler): a passage to another place, a person to talk
 * to, anything else; what and tag are the game's, to say which.
 */
enum {
    USE_MOVER,
    USE_PASSAGE,
    USE_TALK,
    USE_OTHER
};

typedef struct Usable {
    int kind;
    int model;
    int sprite;
    float cx, cy, cz;
    float nx, nz;
    float hw, hh, hd;
    /* From the eye, world units. */
    float reach;
    int mover;
    int what;
    int tag;
    /* The game's word for it ("door", "exit"), for useview and uselist. */
    const char *label;
    /*
     * 1: the body walking into it uses it (a collision trigger: a
     * teleporter, a pressure plate), on the frame it comes in. A model's
     * box, as it stands now, grown by the body's radius. With reach 0 it
     * is not used by hand at all.
     */
    df_u8 touch;
} Usable;

/*
 * What a frame can see. models[] is drawn first, then sprites[].
 * Storage order is submission order.
 */

/*
 * World box of one instance. Filled once the placement is final.
 * The center is cx,cy,cz. ex,ey,ez are the half-size.
 */

typedef struct Bound {
    float cx, cy, cz;
    float ex, ey, ez;
} Bound;

/*
 * One loaded block. first is an index into models[] and bounds[].
 * The box is the union of those models, so a block the camera cannot
 * see is skipped before any of its instances are read.
 */

typedef struct Span {
    int first;
    int count;
    float cx, cy, cz;
    float ex, ey, ez;
    /*
     * Its sprites too, when Scene.sprite_spans is 1 (a streamed world's
     * chunk): the box then holds them as well, and the sprite cull
     * skips a span the box says is out of view, sprites and all.
     */
    int sfirst;
    int scount;
    /*
     * And its emitters (Scene.sprite_spans 1): ambience walks these
     * ranges, not the whole array, most of whose slots are empty.
     */
    int efirst;
    int ecount;
} Span;

typedef struct Scene {
    const Instance *models;
    const Bound *bounds;
    int nmodels;
    const Span *spans;
    int nspans;
    const Flat *sprites;
    int nsprites;
    /* Point lights. RENDERFALL has none; outdoors they are the lamps. */
    const Light *lights;
    int nlights;
    /* Sounds from points (docs/sound.md). */
    const Emitter *emitters;
    int nemitters;
    /* What can be used, and the models that move (docs/interact.md). */
    const Usable *usables;
    int nusables;
    Mover *movers;
    int nmovers;
    /*
     * 1 for a place under the sky: lit by daylight, and its lights are
     * street lamps, on only at night (docs/daylight.md).
     */
    int outdoor;
    /*
     * Changes when the contents change without the counts changing: a
     * streamed chunk loaded or dropped, the origin shifted
     * (docs/streaming.md). What is built from the scene (the collision
     * grid) is built again. 0 for a place loaded whole.
     */
    int version;
    /* 1 when every sprite is in exactly one span's sfirst..scount (docs/streaming.md). */
    int sprite_spans;
    /* What walks about on its own: the game's, read to draw (docs/entities.md). 0 for none. */
    const struct Entity *entities;
    int nentities;
    /* Where the place is flooded, and to what level (docs/water.md). 0 for none. */
    const struct Water *water;
    int nwater;
} Scene;

/*
 * Water (docs/water.md): the place flooded to y over a rectangle of the
 * floor plan, x0..x1 by z0..z1. The engine draws its surface as a
 * translucent layer of color (0xRRGGBB) and, with the eye under it,
 * closes the fog in to that colour; it says how deep a body stands in
 * it (scene_water_body). What water does to the player (swimming,
 * breathing) is the game's.
 */
typedef struct Water {
    float x0, z0, x1, z1;
    float y;
    df_u32 color;
} Water;

/* How deep a body stands in water: dry, its feet in, its waist in, its eye under. */
enum {
    WATER_DRY,
    WATER_FEET,
    WATER_WAIST,
    WATER_HEAD
};

/* The highest water surface over x, z: 1, its level and colour (either may be 0); 0 for none. */
int scene_water_at(const Scene *scene, float x, float z, float *y, df_u32 *color);
/*
 * A body standing with its eye at x, eye_y, z (the walker's: its feet
 * WALK_EYE below, its waist half WALK_HEIGHT above them): WATER_*.
 */
int scene_water_body(const Scene *scene, float x, float eye_y, float z);

void instance_set_rotation(Instance *in, float rx, float ry, float rz);
void instance_point(const Instance *in, float lx, float ly, float lz,
                    float *x, float *y, float *z);

/* Local box from the triangles. A second call with the same count is free. */
void mesh_measure(Mesh *mesh);

/* World box. 0 when the mesh has no triangles. */
int instance_world_box(const Instance *in, Bound *out);

/*
 * 1 when the plane faces the camera, or the normal is degenerate.
 * nx,ny,nz and lx,ly,lz are in mesh space. The normal uses the same
 * winding as the triangle. A camera on the front side sees dot < 0
 * after both are transformed into view space.
 */
int instance_faces(const Instance *in, const Camera *cam, const Basis *basis,
                   float nx, float ny, float nz,
                   float lx, float ly, float lz);

/*
 * Where the segment from o along unit d meets the model's triangles,
 * from either side, within len: 1 and the distance in *t, the nearest.
 * Its world box is not tested; a caller with many models does that
 * first. For the use key's ray and the lights' line of sight.
 */
int instance_ray(const Instance *in, const float *o, const float *d, float len, float *t);

/*
 * How far a frame can see. clip_far is the card's far clip, in world
 * units. fog nonzero means distance fog is on, so the visible limit is
 * the solid fog distance: clip_far * (DF_FOG_FAR / DF_FAR), the same
 * fraction gfx uses, and never closer than the fog start. fog zero
 * uses the clip itself. Defaults are fog on and DF_FAR, matching a
 * fresh device. gfx calls this when either value changes.
 */
void scene_set_limits(float clip_far, int fog);
/*
 * Where the fog is meant to go solid, when something nearer than the far
 * clip's own (df_fog_far) sets it: the weather. 0 goes back to the far
 * clip's. draw.c sets it with the card's fog table, so the cull and the
 * fog agree. See docs/weather.md.
 */
void scene_set_fog_far(float fog_far);
/* 1 keeps the frustum. 0 lists every entity, as a null camera does. */
void scene_set_cull(int on);
int scene_cull(void);
/*
 * Flora, the flats with a sight (Flat.sight), at no cost to reload
 * (docs/streaming.md, Flora). Off: none drawn. Density: the percent of
 * what the place stood that draws, 0..100, by each flat's rank
 * (Flat.thin); default 40. Sight: units across the ground they draw
 * within, in place of each flat's own; 0 (default) keeps theirs.
 */
#define SCENE_FLORA_DENSITY 40
void scene_set_flora(int on);
int scene_flora(void);
void scene_set_flora_density(int percent);
int scene_flora_density(void);
void scene_set_flora_sight(int units);
int scene_flora_sight(void);

/*
 * Visibility. Writes storage indices into out, in storage order, and
 * returns how many. A null camera lists every entity, so a test can
 * ignore the frustum. scene_set_cull(0) does the same with a camera.
 * With culling on, a block span is tested first, then
 * each model's baked box. A model or sprite is dropped when its box
 * lies wholly outside the view: behind the near plane, past the fog or
 * the far clip, or past a screen edge.
 *
 * Then occlusion: the near front faces of the surviving models are
 * rasterized into a grid of 16x16-pixel cells, and a box is dropped only
 * when every cell under its screen rectangle is covered, by one face or
 * by the faces that meet in it, nearer than the whole box. It never drops a box that has a pixel
 * on the glass. See docs/culling.md. The sprite call reads the cells
 * the model call wrote, so call scene_visible_models first.
 *
 * Without baked bounds, the box is
 * measured from the mesh and a model with no triangles is omitted.
 * A count above cap is truncated, so pass a cap of at least nmodels
 * or nsprites.
 */

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap);
int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap);

/*
 * 1 when a world box passes the frustum of the last
 * scene_visible_models call and is not behind the cells it wrote.
 * For the light pass, after the model pass. 1 when culling is off.
 */
int scene_box_visible(float cx, float cy, float cz, float ex, float ey, float ez);

/* 1 runs the occlusion step after the frustum. Default on. */
void scene_set_occlude(int on);
int scene_occlude(void);
/* Faces drawn into the cells, and models plus sprites they hid, last frame. */
void scene_occ_stats(int *faces, int *hidden);

/*
 * Draw the covered cells of the last frame. Default off. A count call
 * gathers each row of cells into runs; scene_cover_at reads run index
 * in screen pixels, the same coordinates camera_project writes.
 */
void scene_set_cover_draw(int on);
int scene_cover_draw(void);
int scene_cover_count(void);
void scene_cover_at(int index, float *x0, float *y0, float *x1, float *y1);

#endif
