#ifndef DF_WORLD_H
#define DF_WORLD_H

#include "engine/assets/level.h"
#include "game/dagger/dungeon.h"

typedef struct World World;

World *world_load(void);

/* One already-parsed place. Marker 10 is the start when that block
   has it. Otherwise the first marker on the entrance block, otherwise
   the center of the bounds. outdoor places are RMB blocks on 4096-unit
   tiles and start above the bounds. The game path stays world_load,
   which still requires marker 10. */
World *world_load_dungeon(const Dungeon *dungeon);
/*
 * The inside of building record of block (an index into town's blocks),
 * placed where the building stands in town, so its doors meet the
 * street's. Starts on its enter marker. docs/interact.md.
 */
World *world_load_inside(const Dungeon *town, int block, int record);
const char *world_error(void);
const char *world_block_name(void);

int world_model_count(const World *world);
int world_triangle_count(const World *world);
const Instance *world_instance(const World *world, int index);

int world_flat_count(const World *world);
int world_editor_count(const World *world);
const Flat *world_flat(const World *world, int index);

int world_image_count(const World *world);
const Image *world_image(const World *world, int slot);
int world_image_cutout(const World *world, int slot);
/* RDB light objects placed in the world; outdoors, one per lamp or fire flat. */
int world_light_count(const World *world);
/* Flats that make a sound, and the place's soundscape (docs/sound.md). */
int world_emitter_count(const World *world);
const Emitter *world_emitter_at(const World *world, int index);
const Soundscape *world_soundscape(const World *world);
/* PLACE_* (doors.h): a dungeon, a town, or a building's inside. */
int world_place(const World *world);
/* What can be used, and what moves (docs/interact.md). */
int world_usable_count(const World *world);
const Usable *world_usable(const World *world, int index);
int world_mover_count(const World *world);
const Mover *world_mover(const World *world, int index);
/* The passage of what (DOOR_*) nearest x, y, z; -1 for none. */
int world_passage(const World *world, int what, float x, float y, float z);
/* Inside: what the building is (BuildingTypes) and its faction; -1 elsewhere. */
int world_building(const World *world, int *faction);
/* 1 for an RMB place, under the sky. */
int world_outdoor(const World *world);
/* The sky an outdoor place draws; 0 indoors. */
const Sky *world_sky(const World *world);
const Palette *world_palette(const World *world);

void world_start(const World *world, float *x, float *y, float *z, float *yaw, float *pitch);
void world_bounds(const World *world,
                  float *minx, float *miny, float *minz,
                  float *maxx, float *maxy, float *maxz);

int world_texture_file_count(const World *world);
int world_texture_file(const World *world, int index);

/* Pointers into this world. Valid until the process exits. */
void world_as_level(const World *world, Level *level);

#endif
