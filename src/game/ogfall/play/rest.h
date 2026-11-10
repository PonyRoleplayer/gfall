#ifndef OGFALL_REST_H
#define OGFALL_REST_H

#include "game/ogfall/play/play.h"
#include "game/ogfall/play/lodging.h"

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
/* Where the player is, for rest_allowed: the seam fills it from the place and the building (0 for none known, which allows). */
void og_rest_set_site(int (*site)(OgRestSite *out));
/* A rented room, or an order's hall for a knight: the player is put on its bed (tavern_go_to_room) before sleeping; building, and whether it is a tavern. */
void og_rest_set_room(void (*go)(df_u32 building, int tavern));
void og_rest_frame(const OgIn *input, const Camera *camera);
/* rest_recover (0x71606), once per whole hour, indoors. */
void og_rest_recover(void);

#endif
