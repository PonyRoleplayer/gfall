#include "engine/gfx/rig.h"
#include "engine/scene/collide.h"

/* See rig.h and docs/collision.md. */

void rig_first(Camera *cam, const Body *b)
{
    float x, y, z;

    body_eye(b, &x, &y, &z);
    cam->x = x;
    cam->y = y;
    cam->z = z;
}

void rig_third(Camera *cam, const Body *b, float yaw, float pitch, float distance, const Scene *scene)
{
    float eye[3];
    float back[3];
    float cp;
    float t;
    float reach;

    body_eye(b, &eye[0], &eye[1], &eye[2]);
    /* From the eye backwards along the view: yaw 0 looks along +z, pitch up positive (camera.h). */
    cp = df_cos(pitch);
    back[0] = -df_sin(yaw) * cp;
    back[1] = -df_sin(pitch);
    back[2] = -df_cos(yaw) * cp;
    reach = distance;
    if (scene && distance > 0.0f && collide_use(scene)) {
        float lo[3];
        float hi[3];
        int k;

        /* The gather's scale is the shape's: a unit one, so the ray reads the world as it is. */
        collide_set_shape(1.0f, 1.0f);
        for (k = 0; k < 3; k++) {
            float end;

            end = eye[k] + back[k] * distance;
            lo[k] = (end < eye[k] ? end : eye[k]) - RIG_MARGIN;
            hi[k] = (end > eye[k] ? end : eye[k]) + RIG_MARGIN;
        }
        collide_gather(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        if (collide_ray(eye, back, distance + RIG_MARGIN, &t)) {
            reach = t - RIG_MARGIN;
        }
    }
    reach = reach < RIG_NEAREST ? RIG_NEAREST : (reach > distance ? distance : reach);
    camera_set(cam, eye[0] + back[0] * reach, eye[1] + back[1] * reach, eye[2] + back[2] * reach, yaw, pitch);
}
