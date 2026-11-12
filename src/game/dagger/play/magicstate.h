#ifndef OGFALL_MAGICSTATE_H
#define OGFALL_MAGICSTATE_H
/* Pointer-free original spell/ailment state. Runtime art belongs to the app. */
#include "engine/scene/scene.h"
#define OG_EFFECTS 96
#define OG_AILMENTS 32
#define OG_MISSILES 8
#define OG_MAGIC_ACTORS 64
#define OG_MAGIC_WIRE_BYTES (40 + OG_EFFECTS*64 + OG_AILMENTS*108 + OG_MISSILES*56 + OG_MAGIC_ACTORS*20)
typedef struct OgMagicActor { KeepKey key; int active, magicka, fatigue; } OgMagicActor;
typedef struct OgRecall { int valid; df_u32 place; float x, y, z; int yaw, pitch; } OgRecall;
typedef struct OgEffect {
    KeepKey target, caster;
    int active, group, sub, rounds, magnitude, chance, change;
    int spell, level, icon, element, permanent;
} OgEffect;
typedef struct OgAilment {
    KeepKey target;
    int active, id, stat[11], delay, maximum, left, stage, drained[8];
} OgAilment;
typedef struct OgMissile {
    KeepKey caster;
    int live, spell, level, element, target, burst;
    float x, y, z, dx, dy, dz;
} OgMissile;
typedef struct OgMagicState {
    df_u32 ready, last;
    int paid;
    OgEffect effect[OG_EFFECTS];
    OgAilment ailment[OG_AILMENTS];
    OgMissile missile[OG_MISSILES];
    OgMagicActor actor[OG_MAGIC_ACTORS];
    OgRecall recall;
} OgMagicState;
OgMagicState *og_magic_state(void);
#endif
