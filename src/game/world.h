#ifndef DF_WORLD_H
#define DF_WORLD_H

#include "assets/palette.h"
#include "assets/rdb.h"
#include "assets/texture.h"

typedef struct World World;

World *world_load(void);
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

#endif
