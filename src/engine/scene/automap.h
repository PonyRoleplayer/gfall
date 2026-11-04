#ifndef ENG_AUTOMAP_H
#define ENG_AUTOMAP_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * The indoor automap: which of a place's models the player has seen, and
 * that geometry as flat polygons for a screen rectangle, from straight
 * above or from a camera. Read docs/automap.md before editing.
 *
 * Seen. The game names the place it is in (automap_set_place); 0 is
 * none, and nothing is marked or drawn. While a place is named, the
 * frame marks a model the camera drew within AUTOMAP_SEEN_RANGE, or
 * passed within AUTOMAP_NEAR_RANGE of (draw_frame calls automap_frame).
 * A model is remembered by a row: the place, its mesh's id and where it
 * stands in the place's frame, rounded to a unit (a mover by where it
 * starts). So it stays seen when a streamed block drops and fills again,
 * when the stream's origin moves, and when the place is left and come
 * back to. The rows are a save game's: count, at, put, clear, as keep.h.
 *
 * Drawn. automap_polygons builds the seen models' faces, cut to a slice
 * of height (bottom..top: a dungeon's floors told apart), projected and
 * cut to the view's rectangle, coloured by height, and hands each
 * polygon to an emit callback. gfx_automap draws them on the card; the
 * tests count them. From above only the faces turned up are drawn, the
 * floors; from a camera every face turned toward it.
 *
 * Plain C, no Glide: hostcheck runs it.
 */

/* Drawn within this of the eye, a model is seen; passed within this, whether drawn or not. */
#define AUTOMAP_SEEN_RANGE 1024.0f
#define AUTOMAP_NEAR_RANGE 320.0f

/* A seen model, as a save writes it. */
typedef struct AutomapRow {
    df_u32 place;
    df_u32 mesh;
    /* Where it stands in the place's frame, rounded to a unit: a mover where it starts. */
    df_i32 x, y, z;
} AutomapRow;

/* The place being marked and drawn; 0 for none (outdoors, a menu). */
void automap_set_place(df_u32 place);
df_u32 automap_place(void);
/*
 * One frame from cam (draw_frame's): drawn[0..ndrawn-1] are the models
 * the frame drew. Marks what is seen or near. Nothing with no place.
 */
void automap_frame(const Scene *scene, const Camera *cam, const int *drawn, int ndrawn);
/* 1 when model i of scene is seen in the place named. */
int automap_seen(const Scene *scene, int model);
/* Every model of scene seen: the console's mapreveal. */
void automap_reveal(const Scene *scene);

/* The rows, for a save file: how many, the i-th in key order, one put back, all gone (a new game). */
int automap_count(void);
const AutomapRow *automap_at(int i);
int automap_put(const AutomapRow *row);
void automap_clear(void);

enum {
    /* Straight down on the map's centre, turned so yaw is up. */
    AUTOMAP_TOP,
    /* From a camera of the game's: behind and above the player, say. */
    AUTOMAP_3D
};

typedef struct AutomapView {
    int mode;
    /* The rectangle drawn into, in screen pixels from the lower left (ui_string's frame). */
    float x0, y0, x1, y1;
    /* AUTOMAP_TOP: the world point at the rectangle's centre, the way up on it (Camera.yaw's sense), pixels a unit. */
    float cx, cz;
    float yaw;
    float scale;
    /* AUTOMAP_3D: the camera, and its field of view across the rectangle in degrees. */
    Camera cam;
    float fov;
    /* The slice of height drawn, world y: nothing above top or below bottom. */
    float bottom, top;
} AutomapView;

/* A polygon corner: screen pixels, and 1/w for the depth test (larger is nearer). */
typedef struct AutomapCorner {
    float x, y;
    float oow;
} AutomapCorner;

/* One convex polygon of n corners (3..AUTOMAP_CORNERS), all inside the rectangle, in colour rgb (0xRRGGBB). */
#define AUTOMAP_CORNERS 48
typedef void (*AutomapEmit)(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb);
/* The seen geometry of scene in view, polygon by polygon. Returns how many. */
int automap_polygons(const Scene *scene, const AutomapView *view, AutomapEmit emit, void *ctx);
/* Where a world point falls in view (the game's player marker): 1 when inside the rectangle. */
int automap_to_screen(const AutomapView *view, float x, float y, float z, float *sx, float *sy);

#endif
