#ifndef OGFALL_MAPS_H
#define OGFALL_MAPS_H
/* Modal map routing; presentation/data stays in the app adapter. */
#include "game/dagger/play/play.h"
typedef struct OgMapsHooks {
    int (*open)(int travel);
    int (*frame)(const OgIn *input);
    void (*close)(void);
    void (*overlay)(void);
} OgMapsHooks;
void og_maps_install(const OgMapsHooks *hooks);
int og_maps_open(int travel);
void og_maps_close(void);
int og_maps_up(void);
void og_maps_frame(const OgIn *input);
void og_maps_overlay(void);
#endif
