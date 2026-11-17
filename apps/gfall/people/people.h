#ifndef GFALL_APP_PEOPLE_H
#define GFALL_APP_PEOPLE_H

#include "engine/gfx/camera.h"

int gfall_people_load_factions(void);
void gfall_people_set_camera(const Camera *camera);
void gfall_people_install(void);
void gfall_street_open(void);
void gfall_street_tick(float seconds);
void gfall_street_install(void);

#endif
