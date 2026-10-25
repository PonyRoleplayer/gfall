#include "engine/gfx/camera.h"
#include "engine/core/df.h"

#define PITCH_LIMIT 1.55334303f
/* 75 degrees across 640. 320 / tan(37.5 deg). */
#define FOV_DEFAULT 75.0f
#define FOCAL_DEFAULT 417.042f

static float g_fov = FOV_DEFAULT;
static float g_focal = FOCAL_DEFAULT;

void camera_set_fov(float degrees)
{
    float half;
    float s;
    float c;

    if (degrees < 45.0f) {
        degrees = 45.0f;
    }
    if (degrees > 120.0f) {
        degrees = 120.0f;
    }
    half = degrees * (DF_TAU / 720.0f);
    s = df_sin(half);
    c = df_cos(half);
    if (c < 0.05f) {
        c = 0.05f;
    }
    if (s < 0.05f) {
        s = 0.05f;
    }
    g_fov = degrees;
    g_focal = ((float)DF_VIEW_W * 0.5f) * (c / s);
}

float camera_fov(void)
{
    return g_fov;
}

float camera_focal(void)
{
    return g_focal;
}

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
    /*
     * The Z-buffer fallback compares ooz. vz is never nearer than
     * DF_NEAR, so scaling by it uses the whole 16 bits: 65535 at the
     * near plane. 65535 / z left three quarters of the range unused.
     * The default W-buffer reads oow instead. See docs/glide.md.
     */
    *ooz = (65535.0f * DF_NEAR) * w;
    *sx = (float)DF_VIEW_W * 0.5f + vx * g_focal * w;
    *sy = (float)DF_VIEW_H * 0.5f + vy * g_focal * w;
    return 1;
}
