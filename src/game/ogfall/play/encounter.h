#ifndef OGFALL_ENCOUNTER_H
#define OGFALL_ENCOUNTER_H

/* Original dungeon rest encounters. Live actors use the engine spawn pool;
 * their identity/initial position and the rolled six choices are saved here,
 * health/position/AI in their keyed Keep rows. See docs/ogfall.md. */
#include "engine/scene/entity.h"
#include "engine/gfx/camera.h"
#define OG_ENCOUNTERS 8
typedef struct OgEncounter {
    KeepKey key;
    int kind, archive;
    float x, y, z;
} OgEncounter;
typedef struct OgEncounterState {
    df_u32 place, serial;
    int choices[6];
    OgEncounter actor[OG_ENCOUNTERS];
} OgEncounterState;
OgEncounterState *og_encounter_state(void);
/* Per-place resource preparation, before the card sees the Level. */
void og_encounter_prepare(df_u32 place, int dungeon_type);
int og_encounter_open(const Scene *scene, int (*art)(int kind, int archive, Entity *out, int *rate));
void og_encounter_close(void);
int og_encounter_try(const Camera *camera);
/* A creature of a kind placed in the dungeon by something other than the rest's chance (a quest's foe): it takes a free actor slot like a rest encounter, with this key thing, unseen 384 to 768 units
 * from the camera where there is floor. 0 when there is no room or no floor. */
int og_encounter_place(const Camera *camera, int kind, df_u32 thing);
void og_encounter_capture(void);
/* Original once-per-12-game-minutes roll while resting, never loitering. */
int og_encounter_minute(const Scene *scene, const Camera *camera, df_u32 minute, int loitering);
int og_encounter_pick(int dungeon_type, int level);

#endif
