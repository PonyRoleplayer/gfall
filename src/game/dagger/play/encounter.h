#ifndef GFALL_ENCOUNTER_H
#define GFALL_ENCOUNTER_H

/* Original dungeon rest encounters. Live actors are kept entities of the engine's pool (docs/entities.md, Kept): the rolled six choices are saved here, the creatures themselves
 * (what they are, where they stand, health and AI) in their keyed Keep rows. See docs/gfall.md. */
#include "engine/scene/entity.h"
#include "engine/gfx/camera.h"
#define ENCOUNTERS 8
/* The saved actor table of earlier schemas: no longer filled (the keep rows say what each creature is), but kept in the layout so a save's bytes stay as they were. */
typedef struct EncounterRoll {
    KeepKey key;
    int kind, archive;
    float x, y, z;
} EncounterRoll;
typedef struct EncounterState {
    df_u32 place, serial;
    int choices[6];
    EncounterRoll actor[ENCOUNTERS];
} EncounterState;
EncounterState *encounter_state(void);
/* Per-place resource preparation, before the card sees the Level. */
void encounter_prepare(df_u32 place, int dungeon_type);
int encounter_open(const Scene *scene, int (*art)(int kind, int archive, Entity *out, int *rate));
void encounter_close(void);
int encounter_try(const Camera *camera);
/* A creature of a kind placed in the dungeon by something other than the rest's chance (a quest's foe): it takes a free actor slot like a rest encounter, with this key thing, unseen 384 to 768 units
 * from the camera where there is floor. 0 when there is no room or no floor. */
int encounter_place(const Camera *camera, int kind, df_u32 thing);
/* The same on the floor near a point (a quest marker's), not unseen; 0 while the point has no floor loaded, to be tried again. */
int encounter_place_at(const Camera *camera, int kind, df_u32 thing, float x, float y, float z);
void encounter_capture(void);
/* Original once-per-12-game-minutes roll while resting, never loitering. */
int encounter_minute(const Scene *scene, const Camera *camera, df_u32 minute, int loitering);
int encounter_pick(int dungeon_type, int level);

#endif
