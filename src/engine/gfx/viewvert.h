#ifndef ENG_VIEWVERT_H
#define ENG_VIEWVERT_H

#include "engine/core/df.h"
#include "engine/gfx/modelview.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/*
 * A model's corners, each turned to the view once (docs/glide.md,
 * Corners once a model).
 *
 * A corner of an ARCH3D model belongs to 2.9 faces on average, and the
 * draw worked it out again for each: the transform, the six tests
 * against the view, and the divide that puts it on the glass. A mesh
 * numbers its distinct corners at load (Mesh.vid, mesh_index), and this
 * is the table those numbers index: what the frame has worked out for
 * each corner of the model being drawn.
 *
 * It is one table for whichever model is being drawn, not one a model.
 * view_begin, called with each model's ModelView, moves a stamp on, and
 * an entry is good only when it carries the stamp: nothing is cleared
 * between models. Each entry is worked out when a face first asks for
 * it, so a corner only back faces have costs nothing, and the divide is
 * made only for a corner of a face wholly on the glass.
 *
 * An entry is 32 bytes, one cache line of the Pentium's.
 */
typedef struct ViewVert {
    /* In view space (modelview_point). */
    float x, y, z;
    /* The planes of the view it is beyond (view_out). */
    df_u32 out;
    /* On the glass, and one over its depth: good when `projected` is the stamp. */
    float sx, sy, oow;
    /* The stamp of the model each half was worked out for. */
    df_u16 seen;
    df_u16 projected;
} ViewVert;

/*
 * The view's edges for the frame, as draw.c's clipper has them: the far
 * limit, the side slopes (half the picture and its margin over the focal
 * length), and the focal length itself.
 */
typedef struct ViewClip {
    float limit;
    float slope_x;
    float slope_y;
    float focal;
} ViewClip;

/*
 * Which of the view's six planes a view-space point is beyond, a bit a
 * plane: near, far, left, right, bottom, top. Each is the sign of a
 * float read as an integer (df_negative): no compare, no branch.
 */
static __inline__ df_u32 view_out(const ViewClip *clip, float x, float y, float z)
{
    float gx;
    float gy;

    gx = z * clip->slope_x;
    gy = z * clip->slope_y;
    return df_negative(z - DF_NEAR)
        | (df_negative(clip->limit - z) << 1)
        | (df_negative(gx + x) << 2)
        | (df_negative(gx - x) << 3)
        | (df_negative(gy + y) << 4)
        | (df_negative(gy - y) << 5);
}

/* Every plane's bit: what "beyond all of them" starts from before the corners are and-ed in. */
#define VIEW_PLANES 63u

/* The stamp no entry carries: what a cleared table holds. */
#define VIEW_NO_STAMP 0

/*
 * The next model: every entry is stale from here. stamp is the table's,
 * kept by the caller. When it comes round to VIEW_NO_STAMP, after 65535
 * models, the table is cleared, so an entry from the last time round is
 * never taken for this model's.
 */
static void view_begin(ViewVert *table, df_u16 *stamp)
{
    (*stamp)++;
    if (*stamp == VIEW_NO_STAMP) {
        int i;

        for (i = 0; i < MESH_VERTS_MAX; i++) {
            table[i].seen = VIEW_NO_STAMP;
            table[i].projected = VIEW_NO_STAMP;
        }
        *stamp = 1;
    }
}

/* Corner k of triangle t of an indexed mesh, in view space with its planes: worked out the first time. */
static __inline__ ViewVert *view_corner(ViewVert *table, df_u16 stamp, const ModelView *mv,
                                        const ViewClip *clip, const Mesh *mesh, int t, int k)
{
    ViewVert *v;

    v = &table[mesh->vid[t * 3 + k]];
    if (v->seen != stamp) {
        const Tri *tri;

        tri = &mesh->tris[t];
        v->seen = stamp;
        modelview_point(mv, tri->x[k], tri->y[k], tri->z[k], &v->x, &v->y, &v->z);
        v->out = view_out(clip, v->x, v->y, v->z);
    }
    return v;
}

/*
 * The corner on the glass: camera_project's sums, the first time. Only
 * for a corner inside the view (out == 0), where the depth is at least
 * the near plane's and the glass's guard band cannot be passed.
 */
static __inline__ void view_project(ViewVert *v, df_u16 stamp, const ViewClip *clip)
{
    if (v->projected != stamp) {
        float oow;

        v->projected = stamp;
        oow = 1.0f / v->z;
        v->oow = oow;
        v->sx = df_view_cx() + v->x * clip->focal * oow;
        v->sy = df_view_cy() + v->y * clip->focal * oow;
    }
}

#pragma GCC diagnostic pop

#endif
