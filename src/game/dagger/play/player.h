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
 * the body's eye (rig_first). With head bobbing on, a walk shifts that
 * eye (viewopt.h). Read docs/game.md (The player).
 *
 * Its speeds are classic's (rules.c) from its character's Speed and its
 * Running and Jumping skills.
 *
 * In water over its head it swims, along the view, at a swimmer's speed, jump
 * rising, no gravity. Under water it spends breath, and out of breath it drowns.
 * An Argonian's lasts longer. Each game minute tires it, more for running and
 * swimming; out of fatigue it collapses (player_minute).
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
    /* Seconds running towards the next use of Running. */
    float run_time;
    /* 1 while under water over its head; its breath left, and time toward the next breath. */
    int swimming;
    int breath;
    float breath_time;
    /* 1 while the run key takes it along: the minute's fatigue goes by it. */
    /* RIDE_* (ride.h): on foot, on a horse, driving a cart. */
    int riding;
    int running;
    /*
     * Crouching (player_crouch): 1 while the body is at crouch size, and 1 when the player
     * wants to be, which differs only while a low ceiling keeps them down.
     */
    int crouched;
    int crouch_want;
    /*
     * Head bobbing's phase (radians) and 1 while this step is a walk on
     * the ground. The camera reads them; standing still leaves the eye.
     */
    float bob_phase;
    int bob_walk;
} Player;

/* A person's body (WALK_* in walker.h) for this character, standing nowhere yet, carrying nothing. */
void player_init(Player *p, const Character *sheet);
/* Its eye at x, eye_y, z (a start marker's point, or a landing), looking yaw, pitch. */
void player_place(Player *p, float x, float eye_y, float z, float yaw, float pitch);
/* A stream's origin moved by this: the body stays where it stands in the world. */
void player_shift(Player *p, float dx, float dz);
/*
 * One frame: the mouse turns it, WASD walk it (run held, faster), jump
 * leaves the floor, then the body steps through the scene. With collide
 * 0 (the console's tcl 0) it flies through everything instead, along
 * the view, Q up and E down, at the same speeds. dt is real seconds.
 */
void player_step(Player *p, const Input *in, const Scene *scene, float dt, int collide);
/* Controller-facing entry points. Turn is in radians this frame; motion is
 * a forward/right fraction (-1..1), normalized together without changing dt.
 * Actions still supply jump, running and no-clip rise. player_step remains
 * the legacy relative-look wrapper used by existing callers and tests. */
void player_turn(Player *p, float yaw, float pitch);
void player_step_motion(Player *p, const Input *in, const Scene *scene, float dt, int collide, float forward, float right);
/*
 * Crouching, DFU's: half the height, half the walking speed, no running, no jump. The body's size
 * is the engine's (body_set_size); standing again only where the head has room (body_can_stand),
 * so under a low ceiling the player stays down and rises when they are out from under it.
 */
#define PLAYER_CROUCH_HEIGHT 36.0f
#define PLAYER_CROUCH_EYE 34.2f
#define PLAYER_CROUCH_SPEED 0.5f
/*
 * One frame's crouch input: held is the crouch key down, pressed its going down this frame. With
 * toggle the key flips the wish and its release does nothing; without it the wish is the key
 * held. Riding, and nothing under the player to ask (scene 0), stand them up. Call between
 * body steps (body_can_stand gathers).
 */
void player_crouch(Player *p, const Scene *scene, int toggle, int held, int pressed);
/*
 * Health taken from it, the one way the game hurts the player (a fall,
 * a blow): 1 when it dies, 0 when it lives, -1 when nothing was taken
 * (amount 0 or less, or god on).
 */
int player_hurt(Player *p, int amount);
/* What it wears changed: its armour values follow (inventory_armor). */
void player_wear_changed(Player *p);
/* A blow of this much damage at a body part wears down what covers it; broken, it comes off, saying so. */
void player_armor_struck(Player *p, int part, int damage, int roll);
/* The weapon in hand, if it just broke, comes off, saying so. */
void player_check_weapon(Player *p);
/*
 * Its breath this frame, under water or not: 1 when it just ran out, which kills
 * (PlayerEntity: a point each 19 classic ticks, and an Argonian has an even chance
 * of keeping it; back at the surface it starts full next time).
 */
int player_breath(Player *p, float dt);
/* What the player's state does to it */
enum {
    PLAYER_FINE,
    /* Out of fatigue with nothing to fear: it dropped, and an hour passed. */
    PLAYER_COLLAPSED,
    /* Out of fatigue with enemies about or in the water: dead. */
    PLAYER_DIED
};
/*
 * A game minute passed: fatigue goes by what it is doing, and Swimming is
 * used. Out of it, enemies_near or swimming kills; otherwise it drops, an
 * hour's rest given back and Medical used (PlayerEntity_OnExhausted).
 */
int player_minute(Player *p, int enemies_near);
/* The camera at its eye, looking its way. A walk with head bobbing on shifts it. */
void player_camera(const Player *p, Camera *cam);

#endif
