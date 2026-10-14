#include "engine/scene/scene.h"
#include "engine/core/df.h"

/*
 * Turns a local point into a world point. The three columns of m[] are
 * the rotated axes. Built once when the entity is placed.
 */

static void rot_apply(float rx, float ry, float rz, float *x, float *y, float *z)
{
    float c, s, x1, y1, z1;

    c = df_cos(rx);
    s = df_sin(rx);
    y1 = *y * c - *z * s;
    z1 = *y * s + *z * c;
    *y = y1;
    *z = z1;

    c = df_cos(ry);
    s = df_sin(ry);
    x1 = *x * c + *z * s;
    z1 = -*x * s + *z * c;
    *x = x1;
    *z = z1;

    c = df_cos(rz);
    s = df_sin(rz);
    x1 = *x * c - *y * s;
    y1 = *x * s + *y * c;
    *x = x1;
    *y = y1;
}

void instance_set_rotation(Instance *in, float rx, float ry, float rz)
{
    float x, y, z;

    rx = df_wrap_tau(rx);
    ry = df_wrap_tau(ry);
    rz = df_wrap_tau(rz);

    x = 1.0f;
    y = 0.0f;
    z = 0.0f;
    rot_apply(rx, ry, rz, &x, &y, &z);
    in->m[0] = x;
    in->m[3] = y;
    in->m[6] = z;

    x = 0.0f;
    y = 1.0f;
    z = 0.0f;
    rot_apply(rx, ry, rz, &x, &y, &z);
    in->m[1] = x;
    in->m[4] = y;
    in->m[7] = z;

    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    rot_apply(rx, ry, rz, &x, &y, &z);
    in->m[2] = x;
    in->m[5] = y;
    in->m[8] = z;
}

void instance_point(const Instance *in, float lx, float ly, float lz,
                    float *x, float *y, float *z)
{
    *x = in->m[0] * lx + in->m[1] * ly + in->m[2] * lz + in->x;
    *y = in->m[3] * lx + in->m[4] * ly + in->m[5] * lz + in->y;
    *z = in->m[6] * lx + in->m[7] * ly + in->m[8] * lz + in->z;
}

static int fill_order(int count, const Camera *cam, int *out, int cap)
{
    int i;
    int n;

    (void)cam;
    if (!out || cap < 1 || count < 1) {
        return 0;
    }
    n = count;
    if (n > cap) {
        n = cap;
    }
    for (i = 0; i < n; i++) {
        out[i] = i;
    }
    return n;
}

int scene_visible_models(const Scene *scene, const Camera *cam, int *out, int cap)
{
    if (!scene) {
        return 0;
    }
    return fill_order(scene->nmodels, cam, out, cap);
}

int scene_visible_sprites(const Scene *scene, const Camera *cam, int *out, int cap)
{
    if (!scene) {
        return 0;
    }
    return fill_order(scene->nsprites, cam, out, cap);
}
