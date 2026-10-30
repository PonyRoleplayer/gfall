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
    int running;
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
/* The camera at its eye, looking its way. */
void player_camera(const Player *p, Camera *cam);

#endif
