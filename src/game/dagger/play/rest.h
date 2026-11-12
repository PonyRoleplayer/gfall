#ifndef GFALL_REST_H
#define GFALL_REST_H

#include "game/dagger/play/play.h"
#include "game/dagger/play/lodging.h"

enum { REST_CHOOSE, REST_HOURS, REST_RUNNING };
typedef struct RestState {
    int open, stage, loiter, until_healed, hours, elapsed;
    char input[3];
    float time;
    unsigned revision;
} RestState;

const RestState *rest_state(void);
void rest_reset(void);
int rest_open(const Scene *scene, const Camera *camera);
void rest_close(void);
/* Where the player is, for rest_allowed: the seam fills it from the place and the building (0 for none known, which allows). */
void rest_set_site(int (*site)(RestSite *out));
/* A rented room, or an order's hall for a knight: the player is put on its bed (tavern_go_to_room) before sleeping; building, and whether it is a tavern. */
void rest_set_room(void (*go)(df_u32 building, int tavern));
void rest_frame(const PlayIn *input, const Camera *camera);
/* rest_recover (0x71606), once per whole hour, indoors. */
void rest_recover(void);

#endif
