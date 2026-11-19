#ifndef GFALL_APP_BUILDINGS_H
#define GFALL_APP_BUILDINGS_H

#include "game/dagger/places/town.h"
#include "game/dagger/play/travel/worldstate.h"
#include "engine/scene/interact.h"

unsigned gfall_building_id(const TownBuilding *building);
void gfall_building_name(const TownBuilding *building, int region, int index, char *out, int cap);
void gfall_building_announce(const PlaceWhere *where);
/* The player has come into a building (building_enter): the access grant, 5 or 255 for their own house. */
void gfall_building_entered(const PlaceWhere *where);
/* Each frame inside (player_frame_update): the interior's restriction at the player against the grant; trespass is a crime and the watch. Outside, a grant held is given up (building_exit). */
void gfall_building_frame(const Camera *cam);
const char *gfall_building_passage(const Usable *door);
int gfall_building_bash(const Usable *door);
void gfall_building_install(void);

#endif
