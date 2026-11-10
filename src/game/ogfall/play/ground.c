#include "game/ogfall/play/ground.h"
#include "game/ogfall/play/travel.h"
#include "game/ogfall/play/inventory.h"
#include "engine/scene/spawn.h"
#include "engine/scene/stream.h"
#include "engine/core/mem.h"

/* Four nearby piles leave room for eight encounters, eight blood effects,
 * eight spell missiles and Light in the engine's fixed 32-entry spawn pool. */
#define GROUND_VISIBLE 4
#define PIXEL_UNITS (8*STREAM_CHUNK)
static const Scene *g_scene;
static int g_slot;
static SpawnId g_picture[GROUND_VISIBLE];
static df_u32 g_container[GROUND_VISIBLE];

df_u32 og_ground_pile(float x, float y, float z, int table)
{
    const OgWhere *where;
    df_u32 place, thing;
    int px, pz, qx, qz;
    og_travel_capture(); where = &og_world_state()->where;
    place = og_object(OG_WORLD_ROOT)->place_key;
    if (where->kind == OG_PLACE_WORLD) {
        /* Origin chunks are exact integers. Reduce before converting back
         * to float, keeping sub-unit precision even at the far map edge. */
        px = where->origin_x/8; pz = where->origin_z/8;
        x += (where->origin_x%8)*STREAM_CHUNK; z += (where->origin_z%8)*STREAM_CHUNK;
        qx = df_ifloor(x/PIXEL_UNITS); qz = df_ifloor(z/PIXEL_UNITS);
        px += qx; pz += qz; x -= qx*PIXEL_UNITS; z -= qz*PIXEL_UNITS;
        if (px < 0 || px >= 1000 || pz < 0 || pz >= 500) { return 0; }
        place = 0x80000000u | (df_u32)(pz*1000+px);
    }
    qx = df_ifloor(x/64); qz = df_ifloor(z/64);
    thing = 0x80000000u | (((df_u32)qx & 32767u) << 15) | ((df_u32)qz & 32767u);
    return og_loot_container(place,thing,x,y,z,table);
}
df_u32 og_ground_container(float x, float y, float z) { return og_ground_pile(x,y,z,-1); }
int og_ground_position(const OgObject *container, float *x, float *y, float *z)
{
    const OgWhere *where;
    int pixel;
    where = &og_world_state()->where;
    if (where->kind != OG_PLACE_WORLD || !container || container->type != OG_RECORD_CONTAINER ||
        container->parent != OG_WORLD_ROOT || !(container->place_key & 0x80000000u) ||
        !(container->thing_key & 0x80000000u)) { return 0; }
    pixel = (int)(container->place_key & 0x7fffffffu);
    if (pixel >= 500000) { return 0; }
    *x = container->x+(pixel%1000*8-where->origin_x)*STREAM_CHUNK;
    *y = container->y;
    *z = container->z+(pixel/1000*8-where->origin_z)*STREAM_CHUNK;
    return 1;
}
void og_ground_close(void)
{
    int i;
    for (i = 0; i < GROUND_VISIBLE; i++) { if (g_picture[i]) { spawn_remove(g_picture[i]); } }
    mem_zero(g_picture,sizeof(g_picture)); mem_zero(g_container,sizeof(g_container));
    g_scene = 0; g_slot = -1;
}
void og_ground_open(const Scene *scene, int sprite_slot)
{
    og_ground_close(); g_scene = scene; g_slot = sprite_slot;
}
void og_ground_frame(const Camera *camera)
{
    const OgObject *item;
    Entity entity, *live;
    int i, j, k;
    float x, y, z, dx, dy, dz, distance, nearest[GROUND_VISIBLE];
    df_u32 chosen[GROUND_VISIBLE];
    if (!g_scene || g_slot < 0) { return; }
    mem_zero(chosen,sizeof(chosen));
    for (i = 0; i < GROUND_VISIBLE; i++) { nearest[i] = 320*320; }
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &og_state()->object[i];
        if (!item->id || !og_ground_position(item,&x,&y,&z)) { continue; }
        dx = x-camera->x; dy = y-camera->y; dz = z-camera->z; distance = dx*dx+dy*dy+dz*dz;
        if (distance >= nearest[GROUND_VISIBLE-1] || !og_inventory_rows(item->id,-1,0,0)) { continue; }
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
            entity.pose[ENTITY_IDLE].w[0] = entity.pose[ENTITY_IDLE].h[0] = 24;
            g_picture[i] = spawn_add(g_scene,&entity,0); live = spawn_get(g_picture[i]);
        }
        item = og_object(chosen[i]);
        if (live && og_ground_position(item,&x,&y,&z)) { live->x = x; live->y = y; live->z = z; }
    }
}
df_u32 og_ground_aim(float x, float y, float z, float dx, float dy, float dz, float reach)
{
    const Entity *entity;
    float ex, ey, ez, along, off;
    int i;
    df_u32 chosen;
    chosen = 0;
    for (i = 0; i < GROUND_VISIBLE; i++) {
        entity = spawn_get(g_picture[i]);
        if (!entity || !g_container[i]) { continue; }
        ex = entity->x-x; ey = entity->y+12-y; ez = entity->z-z;
        along = ex*dx+ey*dy+ez*dz; off = ex*ex+ey*ey+ez*ez-along*along;
        if (along < 0 || off > 12*12) { continue; }
        along -= df_sqrt(12*12-(off > 0 ? off : 0));
        if (along < 0) { along = 0; }
        if (along <= reach) { chosen = g_container[i]; reach = along; }
    }
    return chosen;
}
