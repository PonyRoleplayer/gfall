#include "game/dagger/wild.h"
#include "game/dagger/bind.h"
#include "game/dagger/climate.h"
#include "game/dagger/forecast.h"
#include "game/dagger/skyfile.h"
#include "game/dagger/sounds.h"
#include "game/dagger/texture.h"
#include "game/dagger/worldmap.h"
#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/clock.h"
#include "engine/scene/stream.h"

/*
 * See wild.h and docs/streaming.md. A chunk's ground is 4 x 4 patches of
 * 1024 units, each one quad over 4 x 4 of Daggerfall's 256-unit tiles,
 * its picture repeated (64 texels a tile, as rmb.c lays them): 32
 * triangles a chunk. Every patch of one picture is the same mesh, placed
 * 16 times a chunk, so the ground costs no memory as it streams.
 */

#define PAL_NAME "ART_PAL.COL"
#define PATCH 1024.0f
#define PATCHES 4
#define TILE_TEXELS 64.0f
#define TILES_A_PATCH 4.0f
/* The ground archive's records: 0 water, 2 grass (Daggerfall Unity's tile ids). */
#define REC_WATER 0
#define REC_GRASS 2
#define FAMILIES 4
/* The eye above the ground at the start: the walker's. */
#define START_EYE 68.4f

static const int g_family[FAMILIES] = { TEX_DESERT, TEX_MOUNTAIN, TEX_WOODLAND, TEX_SWAMP };

static int g_open;
static Scene *g_scene;
/* Two pictures a family: its grass, its water. */
static Image *g_images[FAMILIES * 2];
static df_u8 g_cutout[FAMILIES * 2];
static Mesh *g_patch[FAMILIES * 2];
static Palette g_pal;
static Sky g_sky;
static Soundscape g_scape;
static int g_winter;

/* One patch: a quad at y 0 from (0, 0) to (PATCH, PATCH), its picture repeated. */
static Mesh *make_patch(int slot, int file, int record)
{
    Mesh *mesh;
    Tri *t;
    Face *face;
    float span;
    int k;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    t = (Tri *)mem_alloc(2u * (df_u32)sizeof(Tri));
    face = (Face *)mem_alloc(sizeof(Face));
    if (!mesh || !t || !face) {
        return 0;
    }
    mem_zero(mesh, sizeof(Mesh));
    mem_zero(t, 2u * (df_u32)sizeof(Tri));
    span = TILE_TEXELS * TILES_A_PATCH;
    /* As rmb.c's ground: corners low x low z, high x low z, high x high z, low x high z; v falls with z. */
    t[0].x[0] = 0.0f;  t[0].z[0] = 0.0f;  t[0].u[0] = 0.0f;  t[0].v[0] = span;
    t[0].x[1] = PATCH; t[0].z[1] = 0.0f;  t[0].u[1] = span;  t[0].v[1] = span;
    t[0].x[2] = PATCH; t[0].z[2] = PATCH; t[0].u[2] = span;  t[0].v[2] = 0.0f;
    t[1].x[0] = 0.0f;  t[1].z[0] = 0.0f;  t[1].u[0] = 0.0f;  t[1].v[0] = span;
    t[1].x[1] = PATCH; t[1].z[1] = PATCH; t[1].u[1] = span;  t[1].v[1] = 0.0f;
    t[1].x[2] = 0.0f;  t[1].z[2] = PATCH; t[1].u[2] = 0.0f;  t[1].v[2] = 0.0f;
    for (k = 0; k < 2; k++) {
        t[k].tex_file = (df_u16)file;
        t[k].tex_image = (df_u16)record;
        t[k].slot = (df_u16)slot;
    }
    face->nx = 0.0f;
    face->ny = 1.0f;
    face->nz = 0.0f;
    face->first = 0;
    face->ntris = 2;
    mesh->tris = t;
    mesh->ntris = 2;
    mesh->faces = face;
    mesh->nfaces = 1;
    mesh->face_cap = 1;
    mesh->slot_keys = (df_u16 *)mem_alloc(sizeof(df_u16));
    if (mesh->slot_keys) {
        mesh->slot_keys[0] = (df_u16)slot;
        mesh->nslot_keys = 1;
        mesh->have_keys = 1;
    }
    mesh_measure(mesh);
    return mesh;
}

/* The patch mesh a pixel's ground takes: its climate's grass, or water for the sea. */
static Mesh *ground_of(int px, int py)
{
    int climate;
    int family;
    int f;

    climate = worldmap_climate(px, py);
    family = climate_family(climate);
    for (f = 0; f < FAMILIES && g_family[f] != family; f++) {
    }
    if (climate == CLIMATE_OCEAN || f == FAMILIES) {
        /* The sea: the woodland's water, as a sea has no family of its own. */
        return g_patch[2 * 2 + 1];
    }
    return g_patch[f * 2];
}

/* A chunk: its pixel's ground, 4 x 4 patches. Nothing off the map. */
static int fill(void *user, int cx, int cz, StreamOut *out)
{
    Mesh *mesh;
    int px;
    int py;
    int i;

    (void)user;
    out->count = 0;
    if (cx < 0 || cz < 0 || cx >= WORLDMAP_W * WORLDMAP_BLOCKS || cz >= WORLDMAP_H * WORLDMAP_BLOCKS) {
        return 1;
    }
    px = cx / WORLDMAP_BLOCKS;
    py = WORLDMAP_H - 1 - cz / WORLDMAP_BLOCKS;
    mesh = ground_of(px, py);
    if (!mesh) {
        return 1;
    }
    for (i = 0; i < PATCHES * PATCHES && out->count < out->cap; i++) {
        Instance *in;

        in = &out->models[out->count++];
        in->mesh = mesh;
        in->x = (float)(i % PATCHES) * PATCH;
        in->y = 0.0f;
        in->z = (float)(i / PATCHES) * PATCH;
        instance_set_rotation(in, 0.0f, 0.0f, 0.0f);
    }
    return 1;
}

int wild_open(int px, int py, int radius, Level *level, float *x, float *y, float *z)
{
    StreamSource src;
    df_u8 *pal_bytes;
    df_u32 pal_size;
    int climate;
    int season;
    int f;

    wild_close();
    dagger_bind_loaders();
    mem_zero(level, sizeof(Level));
    if (!worldmap_open()) {
        return 0;
    }
    pal_bytes = vfs_read(PAL_NAME, &pal_size);
    if (!pal_bytes || !loader_call("col", PAL_NAME, pal_bytes, pal_size, &g_pal)) {
        log_text(LOG_ERROR, "wild: " PAL_NAME " did not read");
        return 0;
    }
    season = clock_season();
    climate = worldmap_climate(px, py);
    /* Each family's ground, its winter archive (+1) in winter, but the desert's never snows. */
    for (f = 0; f < FAMILIES; f++) {
        int file;
        int k;

        file = g_family[f] + 2 + (season == CLOCK_WINTER && g_family[f] != TEX_DESERT ? 1 : 0);
        for (k = 0; k < 2; k++) {
            int slot;
            int record;

            slot = f * 2 + k;
            record = k ? REC_WATER : REC_GRASS;
            g_images[slot] = texture_load(file, record);
            g_cutout[slot] = 0;
            if (!g_images[slot]) {
                log_print(LOG_ERROR, "wild: TEXTURE.%d record %d did not load", file, record);
                return 0;
            }
            g_patch[slot] = make_patch(slot, file, record);
            if (!g_patch[slot]) {
                return 0;
            }
        }
    }
    g_winter = season == CLOCK_WINTER;
    level->images = g_images;
    level->cutout = g_cutout;
    level->nimages = FAMILIES * 2;
    level->palette = &g_pal;
    /* The start's sky and weather odds; they follow the camera in phase 3. */
    if (climate_family(climate) >= 0) {
        skyfile_open(climate, season, &g_sky);
        forecast_set(climate, season);
        level->sky = &g_sky;
    }
    sounds_scape(&g_scape, SCAPE_TOWN, 0);
    level->sound = &g_scape;
    level->scene.outdoor = 1;
    g_scene = &level->scene;
    src.fill = fill;
    src.user = 0;
    src.models = PATCHES * PATCHES;
    /* The pixel's middle is the corner of its fifth chunk, east and north. */
    if (!stream_open(&level->scene, &src, radius, px * WORLDMAP_BLOCKS + WORLDMAP_BLOCKS / 2,
                     (WORLDMAP_H - 1 - py) * WORLDMAP_BLOCKS + WORLDMAP_BLOCKS / 2)) {
        return 0;
    }
    *x = 0.0f;
    *y = START_EYE;
    *z = 0.0f;
    g_open = 1;
    log_print(LOG_INFO, "wild: open at pixel %d %d, climate %d, %s", px, py, climate,
              clock_season_name(season));
    return 1;
}

void wild_close(void)
{
    if (g_open) {
        stream_close();
    }
    g_open = 0;
    g_scene = 0;
}

void wild_update(Camera *cam, float budget_ms)
{
    if (g_open) {
        stream_update(cam, budget_ms);
    }
}

void wild_here(const Camera *cam, int *px, int *py)
{
    int cx;
    int cz;

    stream_chunk_at(cam, &cx, &cz);
    *px = cx >= 0 ? cx / WORLDMAP_BLOCKS : -1;
    *py = cz >= 0 ? WORLDMAP_H - 1 - cz / WORLDMAP_BLOCKS : -1;
}
