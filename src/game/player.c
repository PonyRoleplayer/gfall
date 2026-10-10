#include "game/player.h"
#include "df.h"

/* A dungeon block is 2048 units on a side. 12 units per vsync crosses it
   in about three seconds at 60 Hz. */
#define FLY_SPEED 12.0f
#define LOOK_SENS 0.003f
#define PITCH_LIMIT 1.55334303f

void player_tick(Camera *cam, const Input *in)
{
    Basis basis;
    float yaw;
    float pitch;

    yaw = cam->yaw + (float)in->look_dx * LOOK_SENS;
    pitch = cam->pitch - (float)in->look_dy * LOOK_SENS;
    if (pitch > PITCH_LIMIT) {
        pitch = PITCH_LIMIT;
    }
    if (pitch < -PITCH_LIMIT) {
        pitch = -PITCH_LIMIT;
    }
    cam->yaw = df_wrap_tau(yaw);
    cam->pitch = pitch;
    camera_basis(cam, &basis);
    cam->x += (basis.fx * (float)in->move_f + basis.rx * (float)in->move_r) * FLY_SPEED;
    cam->y += (float)in->move_y * FLY_SPEED;
    cam->z += (basis.fz * (float)in->move_f + basis.rz * (float)in->move_r) * FLY_SPEED;
}
