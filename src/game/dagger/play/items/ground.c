#include "game/dagger/play/items/ground.h"
#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/items/inventory.h"
#include "engine/scene/spawn.h"
#include "engine/scene/stream.h"
#include "engine/core/mem.h"
#include "game/dagger/formats/worldmap.h"

/* Four nearby piles leave room for eight encounters, eight blood effects,
 * eight spell missiles and Light in the engine's fixed 32-entry spawn pool. */
#define GROUND_VISIBLE 4
/* A world map pixel is GROUND_PIXEL_CHUNKS chunks wide; a pile's row is keyed by GROUND_KEY_BIT, the pixel and a GROUND_CELL-unit cell (ours: the original keeps a
 * dropped item as a child of the place's record). Shown within GROUND_SHOW_RANGE as a GROUND_SPRITE-unit sprite, picked within GROUND_PICK_RADIUS. */
#define GROUND_PIXEL_CHUNKS 8
#define GROUND_KEY_BIT 0x80000000u
#define GROUND_CELL 64
#define GROUND_CELL_BITS 15
#define GROUND_CELL_MASK 32767u
#define GROUND_SHOW_RANGE 320
#define GROUND_SPRITE 24
#define GROUND_PICK_RADIUS 12
#define PIXEL_UNITS (GROUND_PIXEL_CHUNKS*STREAM_CHUNK)
static const Scene *g_scene;
static int g_slot;
static SpawnId g_picture[GROUND_VISIBLE];
static df_u32 g_container[GROUND_VISIBLE];

df_u32 ground_pile(float x, float y, float z, int table)
{
    const PlaceWhere *where;
    df_u32 place, thing;
    int px, pz, qx, qz;
    travel_capture(); where = &world_where()->where;
    place = object_at(WORLD_ROOT)->place_key;
    if (where->kind == WHERE_WORLD) {
        /* Origin chunks are exact integers. Reduce before converting back
         * to float, keeping sub-unit precision even at the far map edge. */
        px = where->origin_x/GROUND_PIXEL_CHUNKS; pz = where->origin_z/GROUND_PIXEL_CHUNKS;
        x += (where->origin_x%GROUND_PIXEL_CHUNKS)*STREAM_CHUNK; z += (where->origin_z%GROUND_PIXEL_CHUNKS)*STREAM_CHUNK;
        qx = df_ifloor(x/PIXEL_UNITS); qz = df_ifloor(z/PIXEL_UNITS);
        px += qx; pz += qz; x -= qx*PIXEL_UNITS; z -= qz*PIXEL_UNITS;
        if (px < 0 || px >= WORLDMAP_W || pz < 0 || pz >= WORLDMAP_H) { return 0; }
        place = GROUND_KEY_BIT | (df_u32)(pz*WORLDMAP_W+px);
    }
    qx = df_ifloor(x/GROUND_CELL); qz = df_ifloor(z/GROUND_CELL);
    thing = GROUND_KEY_BIT | (((df_u32)qx & GROUND_CELL_MASK) << GROUND_CELL_BITS) | ((df_u32)qz & GROUND_CELL_MASK);
    return loot_container(place,thing,x,y,z,table);
}
df_u32 ground_container(float x, float y, float z) { return ground_pile(x,y,z,-1); }
int ground_position(const ObjectInfo *container, float *x, float *y, float *z)
{
    const PlaceWhere *where;
    int pixel;
    where = &world_where()->where;
    if (where->kind != WHERE_WORLD || !container || container->type != RECORD_CONTAINER ||
        container->parent != WORLD_ROOT || !(container->place_key & GROUND_KEY_BIT) ||
        !(container->thing_key & GROUND_KEY_BIT)) { return 0; }
    pixel = (int)(container->place_key & 0x7fffffffu);
    if (pixel >= WORLDMAP_W*WORLDMAP_H) { return 0; }
    *x = container->x+(pixel%WORLDMAP_W*GROUND_PIXEL_CHUNKS-where->origin_x)*STREAM_CHUNK;
    *y = container->y;
    *z = container->z+(pixel/WORLDMAP_W*GROUND_PIXEL_CHUNKS-where->origin_z)*STREAM_CHUNK;
    return 1;
}
void ground_close(void)
{
    int i;
    for (i = 0; i < GROUND_VISIBLE; i++) { if (g_picture[i]) { spawn_remove(g_picture[i]); } }
    mem_zero(g_picture,sizeof(g_picture)); mem_zero(g_container,sizeof(g_container));
    g_scene = 0; g_slot = -1;
}
void ground_open(const Scene *scene, int sprite_slot)
{
    ground_close(); g_scene = scene; g_slot = sprite_slot;
}
void ground_frame(const Camera *camera)
{
    const ObjectInfo *item;
    Entity entity, *live;
    int i, j, k;
    float x, y, z, dx, dy, dz, distance, nearest[GROUND_VISIBLE];
    df_u32 chosen[GROUND_VISIBLE];
    if (!g_scene || g_slot < 0) { return; }
    mem_zero(chosen,sizeof(chosen));
    for (i = 0; i < GROUND_VISIBLE; i++) { nearest[i] = GROUND_SHOW_RANGE*GROUND_SHOW_RANGE; }
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (!item->id || !ground_position(item,&x,&y,&z)) { continue; }
        dx = x-camera->x; dy = y-camera->y; dz = z-camera->z; distance = dx*dx+dy*dy+dz*dz;
        if (distance >= nearest[GROUND_VISIBLE-1] || !inventory_rows(item->id,-1,0,0)) { continue; }
        for (j = 0; j < GROUND_VISIBLE && distance >= nearest[j]; j++) { }
        for (k = GROUND_VISIBLE-1; k > j; k--) { nearest[k] = nearest[k-1]; chosen[k] = chosen[k-1]; }
        nearest[j] = distance; chosen[j] = item->id;
    }
    for (i = 0; i < GROUND_VISIBLE; i++) {
        live = spawn_get(g_picture[i]); g_container[i] = chosen[i];
        if (!chosen[i]) { if (live) { spawn_remove(g_picture[i]); } g_picture[i] = 0; continue; }
        if (!live) {
            entity_init(&entity); entity.live = 1;
            entity.pose[ENTITY_IDLE].slot[0] = (df_u16)g_slot;
            entity.pose[ENTITY_IDLE].w[0] = entity.pose[ENTITY_IDLE].h[0] = GROUND_SPRITE;
            g_picture[i] = spawn_add(g_scene,&entity,0); live = spawn_get(g_picture[i]);
        }
        item = object_at(chosen[i]);
        if (live && ground_position(item,&x,&y,&z)) { live->x = x; live->y = y; live->z = z; }
    }
}
df_u32 ground_aim(float x, float y, float z, float dx, float dy, float dz, float reach)
{
    const Entity *entity;
    float ex, ey, ez, along, off;
    int i;
    df_u32 chosen;
    chosen = 0;
    for (i = 0; i < GROUND_VISIBLE; i++) {
        entity = spawn_get(g_picture[i]);
        if (!entity || !g_container[i]) { continue; }
        ex = entity->x-x; ey = entity->y+GROUND_SPRITE/2-y; ez = entity->z-z;
        along = ex*dx+ey*dy+ez*dz; off = ex*ex+ey*ey+ez*ez-along*along;
        if (along < 0 || off > GROUND_PICK_RADIUS*GROUND_PICK_RADIUS) { continue; }
        along -= df_sqrt(GROUND_PICK_RADIUS*GROUND_PICK_RADIUS-(off > 0 ? off : 0));
        if (along < 0) { along = 0; }
        if (along <= reach) { chosen = g_container[i]; reach = along; }
    }
    return chosen;
}
