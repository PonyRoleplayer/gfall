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
