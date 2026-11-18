#ifndef GFALL_REST_H
#define GFALL_REST_H

/*
 * Resting (rest_allowed, rest_open, rest_update, rest_recover; inline in the decomp). Texts: vagrancy, a hungry vampire, the enemies about; an hour's healing is the maximum times (HEAL_BASE
 * + HEAL_RAPID with rapid healing + Medical) over HEAL_DIVISOR plus (END - ATTRIBUTE_NEUTRAL) / HEAL_END_DIV; magicka comes back by 1 / 2^MAGICKA_REST_SHIFT. Loitering is at most
 * LOITER_HOURS_MOST hours and resting REST_HOURS_MOST. Ours: a creature within ENEMIES_NEAR stops a rest (the original: any creature), and a shown hour takes LOITER_HOUR_SECONDS or
 * REST_HOUR_SECONDS.
 */
#define TEXT_VAGRANT 17
#define TEXT_VAMPIRE_HUNGRY 36
#define HEAL_BASE 60
#define HEAL_RAPID 40
/* The rapid healing flags of a class: by day, in the dark, or always. */
#define RAPID_DAYLIGHT 1
#define RAPID_DARKNESS 2
#define RAPID_ALWAYS 4
#define HEAL_DIVISOR 1000
#define HEAL_END_DIV 10
#define MAGICKA_REST_SHIFT 3
#define LOITER_HOURS_MOST 3
#define REST_HOURS_MOST 99
#define MINUTES_PER_HOUR 60
#define ENEMIES_NEAR 1024

#include "game/dagger/play/play.h"
#include "game/dagger/play/people/lodging.h"

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

/* The game's movie player for an infection's dream (a movie number, DREAM_MOVIE_*), called by rest_close. */
void rest_set_dream(void (*play)(int movie));

#endif
