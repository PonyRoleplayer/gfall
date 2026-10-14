#include "engine/gfx/camera.h"
#include "engine/core/df.h"

#define PITCH_LIMIT 1.55334303f

void camera_set(Camera *cam, float x, float y, float z, float yaw, float pitch)
{
    cam->x = x;
    cam->y = y;
    cam->z = z;
    cam->yaw = df_wrap_tau(yaw);
    if (pitch > PITCH_LIMIT) {
        pitch = PITCH_LIMIT;
    }
    if (pitch < -PITCH_LIMIT) {
        pitch = -PITCH_LIMIT;
    }
    cam->pitch = pitch;
}

void camera_basis(const Camera *cam, Basis *basis)
{
    float sy;
    float cy;

    sy = df_sin(cam->yaw);
    cy = df_cos(cam->yaw);
    /* yaw 0: forward (0, 0, 1), right (1, 0, 0). */
    basis->fx = sy;
    basis->fy = 0.0f;
    basis->fz = cy;
    basis->rx = cy;
    basis->ry = 0.0f;
    basis->rz = -sy;
    basis->sp = df_sin(cam->pitch);
    basis->cp = df_cos(cam->pitch);
}

void camera_view(const Camera *cam, const Basis *basis,
                 float wx, float wy, float wz,
                 float *vx, float *vy, float *vz)
{
    float dx;
    float dy;
    float dz;
    float x1;
    float z1;

    dx = wx - cam->x;
    dy = wy - cam->y;
    dz = wz - cam->z;
    x1 = dx * basis->rx + dz * basis->rz;
    z1 = dx * basis->fx + dz * basis->fz;
    *vx = x1;
    *vy = dy * basis->cp - z1 * basis->sp;
    *vz = dy * basis->sp + z1 * basis->cp;
}

int camera_project(float vx, float vy, float vz,
                   float *sx, float *sy, float *oow, float *ooz)
{
    float w;

    if (vz < DF_NEAR) {
        return 0;
    }
    w = 1.0f / vz;
    *oow = w;
    *ooz = 65535.0f * w;
    *sx = (float)DF_VIEW_W * 0.5f + vx * DF_FOCAL * w;
    *sy = (float)DF_VIEW_H * 0.5f + vy * DF_FOCAL * w;
    return 1;
}
