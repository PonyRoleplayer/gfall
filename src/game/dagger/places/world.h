#ifndef DF_WORLD_H
#define DF_WORLD_H

#include "engine/assets/level.h"
#include "game/dagger/formats/dungeon.h"

typedef struct World World;

World *world_load(void);

/* One already-parsed place. Marker 8, else 10, is the start when that block
   has it. Otherwise the first marker on the entrance block, otherwise
   the center of the bounds. outdoor places are RMB blocks on 4096-unit
   tiles and start above the bounds. The game path stays world_load,
   which still requires one of them. */
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
/*
 * The season the place was built in (CLOCK_*, clock_season at load):
 * a town's pictures, sky and weather odds are that season's.
 */
int world_season(const World *world);
/* A town's window pictures, whose glass is lit at night. */
int world_glass_count(const World *world);
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

/*
 * A dungeon's creatures, one entity each where its block's markers put
 * it (docs/entities.md); the scene draws them. Their kind is Daggerfall's
 * mobile id (enemies.h), passive 1 when the file says it attacks only
 * when attacked; -1 for no such entity. What they do is the game's.
 */
struct Entity;
/*
 * A dungeon's creatures are read as they come near, not at load (their
 * archives were 6 MB of the castle's disk and 2.3 s of its load on the
 * 233; docs/entities.md). Each step reads the nearest still to read
 * within radius of cam (0: any; no camera: every one), a part at a time
 * (a file's 32 KB piece, or one picture), for budget_ms (at most; one
 * part at least), or, with budget_ms 0, until none is left. A creature
 * is drawn (live) once all its pictures are read. Returns how many are
 * still to read within radius. Call it before gfx_prime for those near
 * the landing, then each frame the world is not paused.
 */
int world_creatures_step(World *world, const Camera *cam, float radius, float budget_ms);
int world_entity_count(const World *world);
const struct Entity *world_entity(const World *world, int index);
int world_entity_kind(const World *world, int index, int *passive);

/*
 * A dungeon's water (docs/water.md): rectangles of its blocks flooded to
 * each block's level, as the scene has them (Scene.water). 0 for a dry
 * place. Whether the player is in it: scene_water_body.
 */
int world_water_count(const World *world);
const Water *world_water(const World *world, int index);

/*
 * The last load's stages, in cycles (prof_now): the blocks and their
 * meshes, the pictures (the archives' table, painting the faces and
 * flats, the stretches and pads), the creatures, the water, the doors
 * and actions, and the rest (the palette, lights, sounds, lamps). A
 * viewer turns them into milliseconds (docs/viewers.md, the load line).
 */
typedef struct WorldLoadTimes {
    df_u32 blocks;
    df_u32 pictures;
    df_u32 creatures;
    df_u32 water;
    df_u32 doors;
    df_u32 rest;
} WorldLoadTimes;
void world_load_times(WorldLoadTimes *out);

/*
 * State that outlives a load (keep.c, docs/interact.md): every door,
 * lever and creature of the place is keyed, and what the player changed
 * is put back when the place is built (world_keep_restore, at the end of
 * every load). world_keep_save writes it before the place is left.
 */
void world_keep_save(World *world);
void world_keep_restore(World *world);

/* Pointers into this world. Valid until the process exits. */
void world_as_level(const World *world, Level *level);

#endif
