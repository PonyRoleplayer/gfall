#ifndef GFALL_APP_BUILDINGS_H
#define GFALL_APP_BUILDINGS_H

#include "game/dagger/places/town.h"
#include "game/dagger/play/travel/worldstate.h"
#include "engine/scene/interact.h"

unsigned gfall_building_id(const TownBuilding *building);
void gfall_building_name(const TownBuilding *building, int region, int index, char *out, int cap);
void gfall_building_announce(const PlaceWhere *where);
const char *gfall_building_passage(const Usable *door);
void gfall_building_install(void);

#endif
