#ifndef GFALL_MAGICSTATE_H
#define GFALL_MAGICSTATE_H
/* Pointer-free original spell/ailment state. Runtime art belongs to the app. */
#include "engine/scene/scene.h"
#define EFFECTS 96
#define AILMENTS 32
#define MISSILES 8
#define MAGIC_ACTORS 64
#define MAGIC_WIRE_BYTES (40 + EFFECTS*64 + AILMENTS*108 + MISSILES*56 + MAGIC_ACTORS*20)
typedef struct MagicActor { KeepKey key; int active, magicka, fatigue; } MagicActor;
typedef struct RecallState { int valid; df_u32 place; float x, y, z; int yaw, pitch; } RecallState;
typedef struct Effect {
    KeepKey target, caster;
    int active, group, sub, rounds, magnitude, chance, change;
    int spell, level, icon, element, permanent;
} Effect;
typedef struct AilmentInfo {
    KeepKey target;
    int active, id, stat[11], delay, maximum, left, stage, drained[8];
} AilmentInfo;
typedef struct MissileInfo {
    KeepKey caster;
    int live, spell, level, element, target, burst;
    float x, y, z, dx, dy, dz;
} MissileInfo;
typedef struct MagicState {
    df_u32 ready, last;
    int paid;
    Effect effect[EFFECTS];
    AilmentInfo ailment[AILMENTS];
    MissileInfo missile[MISSILES];
    MagicActor actor[MAGIC_ACTORS];
    RecallState recall;
} MagicState;
MagicState *magic_state(void);
#endif
