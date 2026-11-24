#ifndef GFALL_APP_BUILDINGS_H
#define GFALL_APP_BUILDINGS_H

#include "game/dagger/places/town.h"
#include "game/dagger/play/travel/worldstate.h"
#include "engine/scene/interact.h"

unsigned gfall_building_id(const TownBuilding *building);
void gfall_building_name(const TownBuilding *building, int region, int index, char *out, int cap);
void gfall_building_visit(const PlaceWhere *where);
void gfall_building_note(const TownBuilding *building, int region, int index, int from_directions);
int gfall_building_map_name(const TownBuilding *building, int region, int index, char *out, int cap);
void gfall_building_announce(const PlaceWhere *where);
/* arrival_room_messages: for each tavern of the town where a room is still rented, "You have a room at <tavern> for <n> hours.". */
void gfall_building_room_lines(const PlaceWhere *where);
/* The player has come into a building (building_enter): the access grant, 5 or 255 for their own house. */
void gfall_building_entered(const PlaceWhere *where, int restoring);
/* Check the restriction at the player's feet against their access. */
void gfall_building_frame(void);
/* Give up the access grant and clear the music override before the place closes. */
void gfall_building_left(void);
const char *gfall_building_passage(const Usable *door);
int gfall_building_bash(const Usable *door);
void gfall_building_install(void);

#endif
