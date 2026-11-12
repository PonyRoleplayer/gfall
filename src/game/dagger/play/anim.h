#ifndef OGFALL_ANIM_H
#define OGFALL_ANIM_H

/* XnGine's ASCR frame/event interpreter, independent of sprite loading.
 * Requests are the original action numbers. See docs/ogfall.md. */
#include "game/dagger/tables/combatdefs.h"

typedef struct OgAnim {
    const OgAnimScript *script;
    int pc, wait, record, frame, action, events, ended;
    float time;
} OgAnim;

void og_anim_start(OgAnim *anim, const OgAnimScript *script, int action);
/* One script tick, not one render frame. Returns the new event mask. */
int og_anim_tick(OgAnim *anim);

#endif
