#ifndef ENG_MODELVIEW_H
#define ENG_MODELVIEW_H

#include "engine/core/df.h"
#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/*
 * One model's way to the screen, worked out once for the instance and
 * used for each of its corners and faces (docs/glide.md, The model's
 * transform).
 *
 * A corner used to go model -> world -> view: the instance's matrix and
 * place, less the camera, then the camera's yaw and pitch, 17 multiplies
 * through three pointers. The two are one affine map, so they are
 * multiplied together here once and a corner is 9 multiplies:
 *
 *     view = m * local + t
 *
 * m's rows are the view's x (right), y (up) and z (forward) axes written
 * in the model's frame; t is where the model's origin is in view space.
 * The camera is taken from the model's place before anything is
 * multiplied, so a model far from the world's origin loses no bits.
 *
 * The camera's place in the model's frame, eye, is kept beside it: a
 * face points at the camera when the camera is on the front of its
 * plane, which is one dot product there, with no normal turned to the
 * view. That needs the instance's matrix to be a rotation (its columns
 * unit and square), as every instance's is: instance_set_rotation
 * builds them, and instance_ray leans on the same.
 */
typedef struct ModelView {
    float m[9];
    float tx, ty, tz;
    float ex, ey, ez;
} ModelView;

static void modelview_build(ModelView *mv, const Instance *in, const Camera *cam, const Basis *b)
{
    const float *im;
    float dx, dy, dz;
    float z1;
    /* The camera's rows: view = v * (world - camera). Row x has no y part. */
    float yx, yz;
    float zx, zz;
    int c;

    im = in->m;
    yx = -b->sp * b->fx;
    yz = -b->sp * b->fz;
    zx = b->cp * b->fx;
    zz = b->cp * b->fz;
    for (c = 0; c < 3; c++) {
        mv->m[c] = b->rx * im[c] + b->rz * im[6 + c];
        mv->m[3 + c] = yx * im[c] + b->cp * im[3 + c] + yz * im[6 + c];
        mv->m[6 + c] = zx * im[c] + b->sp * im[3 + c] + zz * im[6 + c];
    }
    dx = in->x - cam->x;
    dy = in->y - cam->y;
    dz = in->z - cam->z;
    /* camera_view's arithmetic, so the origin lands where it always did. */
    z1 = dx * b->fx + dz * b->fz;
    mv->tx = dx * b->rx + dz * b->rz;
    mv->ty = dy * b->cp - z1 * b->sp;
    mv->tz = dy * b->sp + z1 * b->cp;
    /* The inverse of a rotation is its transpose: columns dotted with (camera - model). */
    mv->ex = -(im[0] * dx + im[3] * dy + im[6] * dz);
    mv->ey = -(im[1] * dx + im[4] * dy + im[7] * dz);
    mv->ez = -(im[2] * dx + im[5] * dy + im[8] * dz);
}

/* A corner of the model, in view space. */
static __inline__ void modelview_point(const ModelView *mv, float lx, float ly, float lz,
                                       float *vx, float *vy, float *vz)
{
    *vx = mv->m[0] * lx + mv->m[1] * ly + mv->m[2] * lz + mv->tx;
    *vy = mv->m[3] * lx + mv->m[4] * ly + mv->m[5] * lz + mv->ty;
    *vz = mv->m[6] * lx + mv->m[7] * ly + mv->m[8] * lz + mv->tz;
}

/*
 * 1 when the face with local normal n through local point p shows its
 * front to the camera: instance_faces' answer, in the model's frame. A
 * face with no normal to speak of is kept, as there.
 */
static __inline__ int modelview_faces(const ModelView *mv, float nx, float ny, float nz,
                                      float px, float py, float pz)
{
    if (nx * (px - mv->ex) + ny * (py - mv->ey) + nz * (pz - mv->ez) < 0.0f) {
        return 1;
    }
    return nx * nx + ny * ny + nz * nz < 1.0e-8f;
}

#pragma GCC diagnostic pop

#endif
