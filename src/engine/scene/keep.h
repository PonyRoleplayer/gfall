#ifndef ENG_KEEP_H
#define ENG_KEEP_H

#include "engine/scene/scene.h"
#include "engine/scene/entity.h"

/*
 * State that outlives a load (docs/interact.md): a door opened, a lever
 * pulled, a creature killed or moved stay so when its place is left and
 * come back to, or its block dropped by the stream and filled again.
 *
 * One table for the whole game, a row per thing that is not as its file
 * has it, by the KeepKey the game gives each mover and entity at build
 * (Mover.key, Entity.key). A thing with no row is as the file has it.
 * Written out by the game, the table is a save game's world.
 *
 * Positions are in the place's frame, not the scene's: a stream draws
 * in a frame whose origin moves by whole chunks, so keep_save and
 * keep_restore take where the scene's origin is in the place (0 for a
 * place loaded whole).
 *
 * Plain C: hostcheck runs it.
 */

#define KEEP_GAME_BYTES 16

enum {
    KEEP_MOVER = 1,
    KEEP_ENTITY = 2
};

typedef struct Keep {
    KeepKey key;
    df_u8 kind;
    /* A mover: 1 at its end (one in motion at the end it was heading for). */
    df_u8 at_end;
    df_u8 pad[2];
    /* A mover's lock. */
    df_i32 lock;
    /* An entity: its feet in the place's frame, its facing, killed or not, and what it shows (a body stays a body). */
    float x, y, z, yaw;
    df_u8 dead;
    df_u8 moving;
    df_u8 act;
    df_u8 hold;
    df_u8 frame;
    /* 1 once keep_save wrote the entity's fields; 0 for a row the game made for its bytes alone. */
    df_u8 placed;
    df_u8 pad2[2];
    /* The game's own (health, a flag): carried, never read by the engine. A row with any set is kept. */
    df_u8 game[KEEP_GAME_BYTES];
} Keep;

/*
 * Movers first..first+nmovers-1 of scene, and the game's entities
 * ents[0..nents-1] (its own array, which the scene only reads): each
 * keyed one that differs from its file gets its row, and one back as its
 * file has it loses its row (unless the game's bytes are set). A mover
 * differs at its end or with another lock than its file's; an entity
 * when it is dead (Entity.dead, the game's), shows an act, holds a
 * frame, or stands more than a unit from its home. Entity.live, which
 * is only "drawn", is never kept or put back. o is the scene's origin in the place's frame.
 * Before a place or a block is dropped.
 */
void keep_save(Scene *scene, int first_mover, int nmovers, const Entity *ents, int nents,
               float ox, float oy, float oz);
/*
 * The same after they are built (after mover_init), before they show:
 * each entity's home noted (where the file put it), then each keyed
 * thing that has a row put back. A mover goes straight to its end or
 * start, with no motion, sound or chain.
 */
void keep_restore(Scene *scene, int first_mover, int nmovers, Entity *ents, int nents,
                  float ox, float oy, float oz);

/* The row for key, or 0. */
Keep *keep_find(KeepKey key);
/* The row for key, made (as its file has it: at its start, alive, at home) when there is none. 0 with no memory. */
Keep *keep_get(KeepKey key, int kind);
/* Rows, for a save file: how many, the i-th in key order. */
int keep_count(void);
const Keep *keep_at(int i);
/* A row back from a save file, replacing one of its key. 0 with no memory. */
int keep_put(const Keep *row);
/* Every row gone: a new game. */
void keep_clear(void);

#endif
