#ifndef DF_PLAYER_H
#define DF_PLAYER_H

#include "engine/gfx/camera.h"
#include "engine/platform/input.h"
#include "engine/scene/body.h"

/*
 * The player in the world: a body that walks the place under gravity,
 * and the way it looks. First person, as classic: the camera is put at
 * the body's eye (rig_first). Read docs/game.md (The player).
 *
 * Its speeds are classic's (rules.c) from its Speed and its Running and
 * Jumping skills. Until there is a character (P2) they are a level-1
 * one's, PLAYER_SPEED and the rest below.
 */

#define PLAYER_SPEED 50
#define PLAYER_RUNNING 20
#define PLAYER_JUMPING 20

typedef struct Player {
    Body body;
    float yaw;
    float pitch;
    int speed;
    int running;
    int jumping;
} Player;

/* A person's body (WALK_* in walker.h), the level-1 numbers above, standing nowhere yet. */
void player_init(Player *p);
/* Its eye at x, eye_y, z (a start marker's point, or a landing), looking yaw, pitch. */
void player_place(Player *p, float x, float eye_y, float z, float yaw, float pitch);
/*
 * One frame: the mouse turns it, WASD walk it (run held, faster), jump
 * leaves the floor, then the body steps through the scene. dt is real
 * seconds.
 */
void player_step(Player *p, const Input *in, const Scene *scene, float dt);
/* The camera at its eye, looking its way. */
void player_camera(const Player *p, Camera *cam);

#endif
