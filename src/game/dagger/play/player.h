#ifndef DF_PLAYER_H
#define DF_PLAYER_H

#include "engine/gfx/camera.h"
#include "engine/platform/input.h"
#include "engine/scene/body.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * The player in the world: a body that walks the place under gravity,
 * and the way it looks. First person, as classic: the camera is put at
 * the body's eye (rig_first). Read docs/game.md (The player).
 *
 * Its speeds are classic's (rules.c) from its character's Speed and its
 * Running and Jumping skills.
 */

typedef struct Player {
    Body body;
    float yaw;
    float pitch;
    Character sheet;
    /* What it carries, and the weapon in hand. */
    Inventory pack;
    /* 1 while collision is off: the eye flies (player_step's collide 0). */
    int flying;
    /* 1: nothing takes its health (the console's tgm). Off at player_init. */
    int god;
} Player;

/* A person's body (WALK_* in walker.h) for this character, standing nowhere yet, carrying nothing. */
void player_init(Player *p, const Character *sheet);
/* Its eye at x, eye_y, z (a start marker's point, or a landing), looking yaw, pitch. */
void player_place(Player *p, float x, float eye_y, float z, float yaw, float pitch);
/*
 * One frame: the mouse turns it, WASD walk it (run held, faster), jump
 * leaves the floor, then the body steps through the scene. With collide
 * 0 (the console's tcl 0) it flies through everything instead, along
 * the view, Q up and E down, at the same speeds. dt is real seconds.
 */
void player_step(Player *p, const Input *in, const Scene *scene, float dt, int collide);
/*
 * Health taken from it, the one way the game hurts the player (a fall,
 * a blow): 1 when it dies, 0 when it lives, -1 when nothing was taken
 * (amount 0 or less, or god on).
 */
int player_hurt(Player *p, int amount);
/* The camera at its eye, looking its way. */
void player_camera(const Player *p, Camera *cam);

#endif
