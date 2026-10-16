#ifndef DF_CAMERA_H
#define DF_CAMERA_H

/*
 * Yaw 0 looks down world +Z. Positive yaw turns right, toward world +X.
 * At yaw 0, world +X is screen right and world +Y is screen up.
 * Positive pitch looks up.
 *   yaw 0, pitch 0, point (0, 0, +1) -> view (0, 0, +1)
 *   yaw 0, pitch 0, point (+1, 0, 0) -> view (+1, 0, 0)
 * Movement reads camera_basis. That is the same forward and right.
 * A floor is constant world Y. A higher Y is higher on the screen.
 */

typedef struct Camera {
    float x, y, z;
    float yaw, pitch;
} Camera;

typedef struct Basis {
    float fx, fy, fz;
    float rx, ry, rz;
    float cp, sp;
} Basis;

void camera_set(Camera *cam, float x, float y, float z, float yaw, float pitch);
void camera_basis(const Camera *cam, Basis *basis);
void camera_view(const Camera *cam, const Basis *basis,
                 float wx, float wy, float wz,
                 float *vx, float *vy, float *vz);
/* Horizontal field of view in degrees. Default 75. Clamped to 45..120. */
void camera_set_fov(float degrees);
float camera_fov(void);
/* Focal length that matches camera_fov. Projection and the cull share it. */
float camera_focal(void);
int camera_project(float vx, float vy, float vz,
                   float *sx, float *sy, float *oow, float *ooz);

#endif
