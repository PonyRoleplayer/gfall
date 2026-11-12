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

/*
 * What an entity is to the engine beyond its picture (Entity.flags), all
 * 0 as zeroed, which is how every entity behaved before them. Who it is
 * (a person, a creature, a quest's) is the game's, in Entity.owner.
 * docs/entities.md, The registry.
 */
enum {
    /* Not static: it may move and may have a body. The game's to say; the engine reads it nowhere yet. */
    ENT_MOVES = 0x01,
    /* Bodies pass through it although it has a radius (its radius still sizes its use box). */
    ENT_GHOST = 0x02,
    /* The game's friend or foe, stored here for it and shown by the viewers; the engine acts on nothing by it. */
    ENT_HOSTILE = 0x04,
    /* The use ray takes it (Entity.use): interact_cast, EntityUseHandler. */
    ENT_USABLE = 0x08,
    /* Keyed and kept even though the game made it, not the file (keep.h): revived by its key when its chunk fills. */
    ENT_KEPT = 0x10,
    /* Not drawn, not solid, not usable: a quest's hidden person. */
    ENT_HIDDEN = 0x20
};

/* Where an entity belongs (Entity.residence): what takes it away. */
enum {
    /* As its array says: a stream chunk's goes with the chunk, a place's with the place, a transient with its leash. */
    ENT_HOME_CHUNK,
    /* Stays until the place closes. */
    ENT_HOME_PLACE,
    /* Follows the camera through the stream: kept under the chunk it stands in when that chunk drops. */
    ENT_HOME_ROAM
};

/*
 * How the use ray takes an entity with ENT_USABLE: as a Usable is taken,
 * but on the entity's own box (its radius round its feet, its pose's
 * height). kind is USE_TALK or USE_OTHER (a mover or a passage is a
 * model's, never an entity's); what, tag and label are the game's, as
 * Usable's are. reach is from the eye in world units; 0 is not usable by
 * hand.
 */
typedef struct EntityUse {
    df_u8 kind;
    df_u8 pad;
    df_u16 reach;
    int what;
    int tag;
    const char *label;
} EntityUse;

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
    /* 0 for a free entry: not drawn. Only "drawn": a creature whose pictures are not read yet is not live. */
    df_u8 live;
    /* Emissive art is drawn full bright, like Flat.glow. Not a light source. */
    df_u8 glow;
    /*
     * The game's: 1 once it was killed. Kept across a load (keep.h) where
     * live is not, so a creature saved before its pictures were read is
     * not taken for dead, nor one restored drawn before they are.
     */
    df_u8 dead;
    /* Its key, from the game; 0, 0 is not kept (keep.h). */
    KeepKey key;
    /* Where the file put it, noted by keep_restore, so a kept row is only what changed. */
    float home[3];
    df_u8 homed;
    /* ENT_HOME_*: what takes it away. 0, as its array says. */
    df_u8 residence;
    /* ENT_* above; 0 as before them. */
    df_u32 flags;
    /* The game's: who it is (its table's index, or an identity packed as it likes). The engine never reads it. */
    df_u32 owner;
    /* With ENT_USABLE: how the use ray takes it. */
    EntityUse use;
    /*
     * A transient's (ent_add, spawn_add): freed once the camera is
     * further than this across the ground; 0 never by distance. An
     * entity of an array goes with its array.
     */
    float leash;
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
/*
 * 1 when bodies stop at e and solid entities keep apart from it: live,
 * a radius, and neither ENT_GHOST nor ENT_HIDDEN.
 */
int entity_solid(const Entity *e);
/* How tall entity e stands now: its pose's front view. */
float entity_height(const Entity *e);
int entity_population(void);

#endif
