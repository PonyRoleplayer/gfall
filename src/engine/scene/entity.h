#ifndef ENG_ENTITY_H
#define ENG_ENTITY_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * Things that move about the world on their own, or stand where the
 * game put them: townspeople, the creatures of a dungeon. Read
 * docs/entities.md before editing. The engine knows nothing of friend
 * or foe; what an entity is, and what it does, is the game's.
 *
 * An entity stands at its feet, faces its yaw (Camera.yaw's sense: 0
 * looks along +z, a quarter turn along +x), and is drawn as a sprite
 * that turns with it: Daggerfall's pictures of a body show it from
 * five sides, toward the viewer (0), turned a little (1), side on (2),
 * turned away (3) and from behind (4), each facing the viewer's left;
 * the other three of eight are those mirrored. Which one shows is the
 * angle between where the entity faces and where the camera stands,
 * as Daggerfall Unity's MobilePersonBillboard and MobileUnit choose it.
 *
 * It has poses, each five views (pose[ENTITY_*]): walking and standing
 * (idle), which moving picks, and the ones a game shows for a while
 * instead (act): an attack, being hurt, a spell, a shot, lying dead. A
 * pose of one picture (slot[1] 0xffff) is drawn facing the viewer
 * whichever way the entity faces: a townsperson's idle picture, a body.
 * An entity with no idle pose (idle's slot[0] 0xffff) stands in its
 * walking one: a creature that never stops, a bat or a ghost. A pose not
 * filled in (no width) is not shown: the walk or the stand is.
 *
 * The game moves its entities and owns them; the engine only reads
 * them, once a frame, to draw (Scene.entities). Plain C, no Glide.
 */

#define ENTITY_VIEWS 5
/*
 * Its poses. Walk and idle are picked by moving; the rest are acts the
 * game shows instead (Entity.act): Daggerfall's records 5 to 9 (attack),
 * 10 to 14 (hurt), 20 to 24 (a spell, or a bow's first shot), 25 to 29
 * (a bow's second), and a corpse.
 */
enum {
    ENTITY_WALK,
    ENTITY_IDLE,
    ENTITY_ATTACK,
    ENTITY_HURT,
    ENTITY_CAST,
    ENTITY_SHOOT,
    ENTITY_DEAD,
    ENTITY_POSES
};

typedef struct EntityPose {
    /* Picture slots, one a view (records 0..4 of its kind); 0xffff for none. */
    df_u16 slot[ENTITY_VIEWS];
    /* Each view's size in world units: a record is drawn at its own. */
    df_u16 w[ENTITY_VIEWS];
    df_u16 h[ENTITY_VIEWS];
    /* Frames a second. */
    df_u8 rate;
    /*
     * Bit k: view k's picture faces the viewer's right, not the left,
     * so its mirroring is the other way round. Daggerfall drew a few so:
     * the rat standing (records 16 to 18), the ghost's and the wraith's
     * side on (record 2).
     */
    df_u8 right;
} EntityPose;

typedef struct Entity {
    /* Its feet, in the scene's frame; its facing. */
    float x, y, z;
    float yaw;
    EntityPose pose[ENTITY_POSES];
    /*
     * How wide it stands, a circle round its feet (world units): the
     * walker's body stops at it, and entity_blocked keeps solid entities
     * apart. 0 (as zeroed) passes through and is passed through.
     */
    float radius;
    /* Its place in the cycle of frames. */
    df_u8 phase;
    /* 1 walking, 0 standing. */
    df_u8 moving;
    /*
     * A pose the game shows instead of the walk or the stand (ENTITY_
     * ATTACK and on), each frame it wants it; 0 for none. Set its pose
     * first: one not filled in is not shown.
     */
    df_u8 act;
    /*
     * 1: the shown pose stands on frame (a swing played once, a body on
     * its last frame), the game stepping it; 0 (as zeroed): its frames
     * run at its rate from phase, round and round.
     */
    df_u8 hold;
    df_u8 frame;
    /* 0 for a free entry: not drawn. */
    df_u8 live;
} Entity;

/*
 * e zeroed, with no picture in any pose (every slot 0xffff): fill in the
 * poses it has. A zeroed entity's slots are 0, picture 0: start one here.
 */
void entity_init(Entity *e);
/* The pose e shows now: its act, else walking, standing, or walking for one with no idle pose. */
const EntityPose *entity_pose(const Entity *e);

/*
 * Which view of e shows from a camera at (cx, cz): its record 0..4, and
 * 1 in *flip when that record is to be mirrored (it faces the viewer's
 * right). A pose of one picture is view 0, unflipped.
 */
int entity_view(const Entity *e, float cx, float cz, int *flip);

/*
 * e as a sprite seen from (cx, cz): its picture, size, mirroring and
 * animation in out, standing on its feet (Flat's point is its middle).
 * 0 when it has no picture for that view.
 */
int entity_flat(const Entity *e, float cx, float cz, Flat *out);

/*
 * Whether things may come and go on their own (a game's walkers): the
 * engine's say, over the game's own rules for where. 1 (default); 0:
 * none come, and those about go once out of sight. The console's
 * population switch.
 */
void entity_set_population(int on);
/*
 * 1 when entity self, standing with its feet at x, z (its own y and
 * height), would meet the walker's body or another live entity with a
 * radius: for a motor, before it takes a step. self needs a radius of
 * its own (0: never blocked). The walker's body only while it is on.
 */
int entity_blocked(const Scene *scene, const Entity *self, float x, float z);
/* How tall entity e stands now: its pose's front view. */
float entity_height(const Entity *e);
int entity_population(void);

#endif
