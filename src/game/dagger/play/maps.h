#ifndef GFALL_MAPS_H
#define GFALL_MAPS_H
/* Modal map routing; presentation/data stays in the app adapter. */
#include "game/dagger/play/play.h"
typedef struct MapsHooks {
    int (*open)(int travel);
    int (*frame)(const PlayIn *input);
    void (*close)(void);
    void (*overlay)(void);
} MapsHooks;
void maps_install(const MapsHooks *hooks);
int maps_open(int travel);
void maps_close(void);
int maps_up(void);
void maps_frame(const PlayIn *input);
void maps_overlay(void);
#endif
