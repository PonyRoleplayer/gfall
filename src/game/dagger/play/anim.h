#ifndef GFALL_ANIM_H
#define GFALL_ANIM_H

/* XnGine's ASCR frame/event interpreter, independent of sprite loading.
 * Requests are the original action numbers. See docs/game.md. */
#include "game/dagger/tables/combatdefs.h"

typedef struct Anim {
    const AnimScript *script;
    int pc, wait, record, frame, action, events, ended;
    float time;
} Anim;

void anim_start(Anim *anim, const AnimScript *script, int action);
/* One script tick, not one render frame. Returns the new event mask. */
int anim_tick(Anim *anim);

#endif
