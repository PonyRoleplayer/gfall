#ifndef OGFALL_REST_H
#define OGFALL_REST_H

#include "game/ogfall/play/play.h"

enum { OG_REST_CHOOSE, OG_REST_HOURS, OG_REST_RUNNING };
typedef struct OgRest {
    int open, stage, loiter, until_healed, hours, elapsed;
    char input[3];
    float time;
    unsigned revision;
} OgRest;

const OgRest *og_rest(void);
void og_rest_reset(void);
int og_rest_open(const Scene *scene, const Camera *camera);
void og_rest_close(void);
void og_rest_frame(const OgIn *input, const Camera *camera);
/* rest_recover (0x71606), once per whole hour, indoors. */
void og_rest_recover(void);

#endif
