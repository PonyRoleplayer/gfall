#include "game/dagger/play/state.h"
#include "game/dagger/play/input/activate.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/combat/weapon.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/travel/actions.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/travel/maps.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/items/ground.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/combatdefs.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/scene/spawn.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/sound/sound.h"
#include "engine/ui/pic.h"
#include "engine/scene/collide.h"
#include "engine/scene/interact.h"
#include "engine/scene/light.h"
#include "engine/scene/stream.h"

#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/formats/arch3d.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/rdb.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/places/wres.h"
#include "game/dagger/formats/worldmap.h"

#include "game/dagger/places/place.h"

#include "world.h"
#include "../buildings/buildings.h"
#include "../people/people.h"
#include "../quests/hooks.h"
#include "../error.h"
/*
 * doors.c's kludge_fix_dungeon_door is not re-coded. The hold's doors
 * are the ones world_load already built. A disagreement with the decomp
 * stays a disagreement for this slice.
 *
 * quests_start_initial is not called. See play/character/chargen.h.
 */

#define MARKS 1024
#define TILE 2048

static World *g_world;
static int g_place_open;
static Dungeon g_place_data;
static PlaceWhere g_saved_where;
static Level g_level;
static void magic_lighting(int enabled);
static int seam_world_art(void);
#define ENCOUNTER_ART 20
typedef struct EncounterArt { int kind, archive, rate; Entity entity; } EncounterArt;
static EncounterArt g_encounter_art[ENCOUNTER_ART];
static int g_encounter_art_count, g_original_entities;
static Entity *g_game_entities;
static void marker6(float *x, float *y, float *z)
{
    Dungeon hold;
    Bsa *blocks;
    MarkInfo marks[MARKS];
    int n;
    int b;
    float nx;
    float ny;
    float nz;

    n = 0;
    hold = g_place_data;
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) {
        return;
    }
    for (b = 0; b < hold.nblocks; b++) {
        const BsaRec *rec;
        df_u8 *bytes;
        Marker tmp[128];
        int k;
        int i;

        rec = bsa_find_name(blocks, hold.block[b].name);
        if (!rec) {
            continue;
        }
        bytes = bsa_read_scratch(blocks, rec, BSA_SCRATCH_BLOCK);
        if (!bytes) {
            continue;
        }
        k = rdb_markers(bytes, rec->size, tmp, 128);
        for (i = 0; i < k && n < MARKS; i++) {
            marks[n].record = tmp[i].record;
            marks[n].x = tmp[i].x + (float)(hold.block[b].x * TILE);
            marks[n].y = tmp[i].y;
            marks[n].z = tmp[i].z + (float)(hold.block[b].z * TILE);
            n++;
        }
    }
    bsa_close(blocks);
    if (place_marker6(marks, n, *x, *y, *z, &nx, &ny, &nz)) {
        *x = nx;
        *y = ny;
        *z = nz;
    }
}

#define HOLD_TREASURES 128
/* A loaded actor can add four five-view poses and one corpse. A
 * prepared spawn can also add its walk and idle poses. */
#define HOLD_EXTRA_IMAGES (ENCOUNTER_ART * 31 + HOLD_TREASURES + 16)
static Keep g_entity_keeps[WORLD_ROWS];
static int g_entity_keep_count;
static Image *const *g_world_images;
static int g_world_image_count;
static Flat *g_hold_flats;
static Usable *g_hold_usables;
static int g_hold_flat_base, g_hold_use_base, g_hold_capacity;
static int g_hold_loot_table, g_hold_pile_slot;
static df_u32 g_hold_pile_serial;
static Mesh *g_arrow_mesh;
static Instance *g_arrow_models;
static Bound *g_arrow_bounds;
static Span *g_arrow_spans;
static int g_arrow_base, g_arrow_span;

static void hold_arrow_model(Image **images, df_u8 *cutout, int *slots)
{
    Bsa *archive;
    const BsaRec *record;
    const df_u8 *bytes;
    Tri *tri;
    Image *image;
    int i, j, slot, count;
    g_arrow_mesh = 0;
    archive = bsa_open("ARCH3D.BSA");
    if (!archive) { return; }
    record = bsa_find_id(archive,99800);
    bytes = record ? bsa_read_scratch(archive,record,BSA_SCRATCH_MODEL) : 0;
    if (bytes) { g_arrow_mesh = arch3d_load(bytes,record->size,99800); }
    bsa_close(archive);
    if (!g_arrow_mesh) { return; }
    for (i = 0; i < g_arrow_mesh->ntris; i++) {
        tri = &g_arrow_mesh->tris[i]; slot = -1;
        for (j = 0; j < i; j++) {
            if (g_arrow_mesh->tris[j].tex_file == tri->tex_file && g_arrow_mesh->tris[j].tex_image == tri->tex_image) {
                slot = g_arrow_mesh->tris[j].slot; break;
            }
        }
        if (slot < 0) {
            image = texture_load(tri->tex_file,tri->tex_image);
            if (image) { slot = (*slots)++; images[slot] = image; cutout[slot] = 0; }
        }
        tri->slot = slot >= 0 ? (df_u16)slot : 0xffff;
    }
    g_arrow_base = g_level.scene.nmodels; count = g_arrow_base+ARROWS;
    g_arrow_models = (Instance *)mem_alloc(count*sizeof(Instance));
    g_arrow_bounds = (Bound *)mem_alloc(count*sizeof(Bound));
    g_arrow_spans = (Span *)mem_alloc((g_level.scene.nspans+1)*sizeof(Span));
    if (!g_arrow_models || !g_arrow_bounds || !g_arrow_spans) { g_arrow_mesh = 0; return; }
    mem_zero(g_arrow_models,count*sizeof(Instance)); mem_zero(g_arrow_bounds,count*sizeof(Bound));
    mem_copy(g_arrow_models,g_level.scene.models,g_arrow_base*sizeof(Instance));
    for (i = 0; i < g_arrow_base; i++) {
        if (g_level.scene.bounds) { g_arrow_bounds[i] = g_level.scene.bounds[i]; }
        else { instance_world_box(&g_arrow_models[i],&g_arrow_bounds[i]); }
    }
    for (i = g_arrow_base; i < count; i++) { g_arrow_bounds[i].ex = -1; }
    /* Movers must write the same instances that rendering and collision read. */
    for (i = 0; i < g_level.scene.nmovers; i++) {
        if (g_level.scene.movers[i].model >= 0 && g_level.scene.movers[i].model < g_arrow_base) {
            g_level.scene.movers[i].inst = &g_arrow_models[g_level.scene.movers[i].model];
        }
    }
    g_arrow_span = -1;
    if (g_level.scene.nspans && g_level.scene.spans) {
        mem_copy(g_arrow_spans,g_level.scene.spans,g_level.scene.nspans*sizeof(Span));
        g_arrow_span = g_level.scene.nspans++;
        mem_zero(&g_arrow_spans[g_arrow_span],sizeof(Span));
        g_arrow_spans[g_arrow_span].first = g_arrow_base; g_arrow_spans[g_arrow_span].count = ARROWS;
        g_arrow_spans[g_arrow_span].ex = g_arrow_spans[g_arrow_span].ey = g_arrow_spans[g_arrow_span].ez = 4096;
        g_level.scene.spans = g_arrow_spans;
    }
    g_level.scene.models = g_arrow_models; g_level.scene.bounds = g_arrow_bounds;
    g_level.scene.nmodels = count; g_level.scene.solid_models = g_arrow_base;
}

static void hold_arrows(const Camera *cam)
{
    const ArrowInfo *arrow;
    Instance *model;
    float horizontal;
    int i;
    if (!g_arrow_mesh) { return; }
    if (g_arrow_span >= 0 && cam) {
        g_arrow_spans[g_arrow_span].cx = cam->x;
        g_arrow_spans[g_arrow_span].cy = cam->y;
        g_arrow_spans[g_arrow_span].cz = cam->z;
    }
    for (i = 0; i < ARROWS; i++) {
        arrow = dungeon_arrow(i); model = &g_arrow_models[g_arrow_base+i];
        model->mesh = arrow->live ? g_arrow_mesh : 0;
        if (!arrow->live) { g_arrow_bounds[g_arrow_base+i].ex = -1; continue; }
        model->x = arrow->at[0]; model->y = arrow->at[1]; model->z = arrow->at[2];
        horizontal = df_sqrt(arrow->direction[0]*arrow->direction[0]+arrow->direction[2]*arrow->direction[2]);
        instance_set_rotation(model,-df_atan2(arrow->direction[1],horizontal),df_atan2(arrow->direction[0],arrow->direction[2]),0);
        instance_world_box(model,&g_arrow_bounds[g_arrow_base+i]);
    }
}

/* Original TEXTURE record header +22 is the ASCR step divisor in ms. */
int gfall_creature_rate(int archive)
{
    VfsFile *file;
    char name[16];
    df_u8 bytes[4];
    df_u32 record;
    int rate;
    texture_filename(archive,name,sizeof(name));
    file = vfs_open(name); rate = 100;
    if (file) {
        if (vfs_read_at(file,26+5*20+2,bytes,4)) {
            record = df_ru32(bytes);
            if (vfs_read_at(file,record+22,bytes,2) && df_ru16(bytes)) { rate = df_ru16(bytes); }
        }
        vfs_close(file);
    }
    return rate;
}

int seam_creature_archive(int index) { return world_entity_archive(g_world,index); }

static void hold_creature_art(Image **images, df_u8 *cutout, int *slots)
{
    Entity *e;
    Image *image;
    EntityPose *pose;
    int i, j, p, previous, kind, archive, file, record, slot;
    static const int acts[5] = {ENTITY_ATTACK,ENTITY_HURT,ENTITY_CAST,ENTITY_SHOOT,ENTITY_DEAD};
    static const int records[5] = {5,10,20,25,0};
    for (i = 0; i < g_level.scene.nentities; i++) {
        e = (Entity *)&g_level.scene.entities[i];
        kind = world_entity_kind(g_world,i,0);
        archive = world_entity_archive(g_world,i);
        previous = -1;
        for (j = 0; j < i; j++) {
            if (world_entity_kind(g_world,j,0) == kind && world_entity_archive(g_world,j) == archive) { previous = j; break; }
        }
        if (previous >= 0) {
            for (p = 0; p < 5; p++) { e->pose[acts[p]] = g_level.scene.entities[previous].pose[acts[p]]; }
            continue;
        }
        for (p = 0; p < 5; p++) {
            pose = &e->pose[acts[p]]; pose->rate = 10;
            for (j = 0; j < (p == 4 ? 1 : 5); j++) {
                file = archive; record = records[p]+j;
                if (p == 4) { foe_corpse(kind,&file,&record); }
                image = texture_load(file,record);
                if (!image) { continue; }
                if (image->frames <= 1 || !texture_sheet(image,file,record)) { texture_pad(image); }
                slot = (*slots)++; images[slot] = image; cutout[slot] = 1;
                pose->slot[j] = (df_u16)slot;
                pose->w[j] = (df_u16)texture_extent(image->width,image->xscale);
                pose->h[j] = (df_u16)texture_extent(image->height,image->yscale);
            }
        }
    }
}

static int encounter_art(int kind, int archive, Entity *out, int *rate)
{
    int i;
    for (i = 0; i < g_encounter_art_count; i++) {
        if (g_encounter_art[i].kind == kind && g_encounter_art[i].archive == archive) {
            *out = g_encounter_art[i].entity; *rate = g_encounter_art[i].rate;
            return out->pose[ENTITY_WALK].w[0] != 0;
        }
    }
    return 0;
}

static void streamed_foe_release(const Entity *entity)
{
    int pose, view;

    for (pose = 0; pose < ENTITY_POSES; pose++) {
        for (view = 0; view < 5; view++) {
            if (entity->pose[pose].w[view]) { wres_release(entity->pose[pose].slot[view]); }
        }
    }
}

/* Each actor holds its pose slots until it is retired. */
static int streamed_foe_art(int kind, int archive, Entity *entity, int *rate)
{
    static const int first[ENTITY_POSES] = {0,15,5,10,20,25,0};
    int p, view, file, record, slot;
    const Image *img;
    EntityPose *pose;

    entity_init(entity); *rate = gfall_creature_rate(archive);
    for (p = 0; p < ENTITY_POSES; p++) {
        pose = &entity->pose[p]; pose->rate = p == ENTITY_IDLE ? 5 : 10;
        for (view = 0; view < (p == ENTITY_DEAD ? 1 : 5); view++) {
            file = archive; record = first[p]+view;
            if (p == ENTITY_DEAD) { foe_corpse(kind,&file,&record); }
            if (p == ENTITY_IDLE && archive == 483 && view == 1) { record = 11; }
            slot = wres_slot(file,record,WRES_SPRITE);
            if (slot < 0 || !wres_acquire(slot)) {
                wres_release(slot);
                continue;
            }
            img = wres_image(slot);
            pose->slot[view] = (df_u16)slot;
            pose->w[view] = (df_u16)texture_extent(img->width,img->xscale);
            pose->h[view] = (df_u16)texture_extent(img->height,img->yscale);
        }
    }
    if (kind == 0) { entity->pose[ENTITY_IDLE].right = 14; }
    if (kind == 18 || kind == 23) { entity->pose[ENTITY_WALK].right = 4; }
    if (entity->pose[ENTITY_WALK].w[0]) { return 1; }
    streamed_foe_release(entity);
    return 0;
}

/* Enchantment summons share a place-long cache. Random encounters use
 * the same reader but release their own references when they leave range. */
static int streamed_soul_art(int kind, Entity *entity, int *rate)
{
    EncounterArt *art;
    int archive;

    archive = 255+kind;
    if (encounter_art(kind,archive,entity,rate)) { return 1; }
    if (g_encounter_art_count >= ENCOUNTER_ART || !streamed_foe_art(kind,archive,entity,rate)) { return 0; }
    art = &g_encounter_art[g_encounter_art_count++];
    art->kind = kind; art->archive = archive; art->rate = *rate; art->entity = *entity;
    return 1;
}

static int outdoor_encounter_site(const Camera *camera, int *settlement)
{
    double wx, wz, x0, z0, x1, z1;
    int px, py, blocks, climate, type;

    wild_here(camera,&px,&py);
    stream_world(camera->x,camera->z,&wx,&wz);
    *settlement = wild_town(wx,wz,&x0,&z0,&x1,&z1,&blocks,&climate,&type) &&
                  wx >= x0 && wx < x1 && wz >= z0 && wz < z1;
    return worldmap_climate(px,py);
}

static int ench_spawn_creature(int kind, int team)
{
    Camera cam;
    SpawnSpot spot;
    Entity entity, *live;
    Body *body;
    SpawnId id;
    int rate, foe;

    if (!g_place_open || kind < 0 || kind >= 43) { return 0; }
    move_camera(&cam);
    if (world_where()->where.kind == WHERE_DUNGEON) {
        return encounter_summon(&cam,kind,team);
    }
    if (g_level.streamed ? !streamed_soul_art(kind,&entity,&rate) :
        !encounter_art(kind,255+kind,&entity,&rate)) { return 0; }
    if (!entity.pose[ENTITY_WALK].w[0]) { return 0; }
    mem_zero(&spot,sizeof(spot));
    body = move_body();
    spot.x = body->feet[0]; spot.y = body->feet[1]; spot.z = body->feet[2];
    spot.inner = 128; spot.outer = 512; spot.climb = 128;
    spot.height = entity.pose[ENTITY_WALK].h[0];
    if (spot.height < 24) { spot.height = 24; }
    if (spot.height > 128) { spot.height = 128; }
    spot.radius = 12; spot.flags = SPAWN_DRY;
    if (!spawn_find(&g_level.scene,&cam,&spot,&entity.x,&entity.y,&entity.z)) { return 0; }
    entity.live = 1; entity.flags |= ENT_MOVES;
    id = spawn_add(&g_level.scene,&entity,0);
    live = spawn_get(id);
    if (!live) { return 0; }
    foe = dungeon_add(live,kind,0,rate);
    if (foe < 0) { spawn_remove(id); return 0; }
    dungeon_loaded(foe);
    dungeon_set_team(foe,team);
    dungeon_wake(foe);
    return 1;
}

void gfall_release_soul(int kind) { ench_spawn_creature(kind,TEAM_HOSTILE); }
int gfall_summon_ally(int kind) { return ench_spawn_creature(kind,TEAM_PLAYER); }

static int prepare_encounter_art(int kind, int archive, Image **images, df_u8 *cutout, int *slots)
{
    static const int first[ENTITY_POSES] = {0,15,5,10,20,25,0};
    EncounterArt *art;
    const Entity *existing;
    EntityPose *pose;
    Image *image;
    int i, p, view, file, record, slot;

    for (i = 0; i < g_encounter_art_count; i++) {
        if (g_encounter_art[i].kind == kind && g_encounter_art[i].archive == archive) { return 1; }
    }
    if (g_encounter_art_count == ENCOUNTER_ART) { return 0; }
    art = &g_encounter_art[g_encounter_art_count++];
    mem_zero(art,sizeof(*art)); art->kind = kind; art->archive = archive; art->rate = gfall_creature_rate(archive);
    entity_init(&art->entity);
    for (i = 0; i < world_entity_count(g_world); i++) {
        if (world_entity_kind(g_world,i,0) != kind || world_entity_archive(g_world,i) != archive) { continue; }
        existing = world_entity(g_world,i);
        for (p = 0; p < ENTITY_POSES; p++) { art->entity.pose[p] = existing->pose[p]; }
        break;
    }
    /* Reuse decoded poses without activating a distant placed creature.
     * Missing walk/idle art gets its own slots: the world's reserved slots
     * still belong to its lazy loader. Prepare everything before gfx_prepare. */
    for (p = 0; p < ENTITY_POSES; p++) {
        pose = &art->entity.pose[p]; pose->rate = p == ENTITY_IDLE ? 5 : 10;
        for (view = 0; view < (p == ENTITY_DEAD ? 1 : 5); view++) {
            if (pose->w[view]) { continue; }
            file = archive; record = first[p]+view;
            if (p == ENTITY_DEAD) { foe_corpse(kind,&file,&record); }
            if (p == ENTITY_IDLE && archive == 483 && view == 1) { record = 11; }
            image = texture_load(file,record);
            if (!image) { continue; }
            if (image->frames <= 1 || !texture_sheet(image,file,record)) { texture_pad(image); }
            slot = (*slots)++; images[slot] = image; cutout[slot] = 1;
            pose->slot[view] = (df_u16)slot;
            pose->w[view] = (df_u16)texture_extent(image->width,image->xscale);
            pose->h[view] = (df_u16)texture_extent(image->height,image->yscale);
        }
    }
    if (kind == 0) { art->entity.pose[ENTITY_IDLE].right = 14; }
    if (kind == 18 || kind == 23) { art->entity.pose[ENTITY_WALK].right = 4; }
    return art->entity.pose[ENTITY_WALK].w[0] > 0;
}

/* Kept dungeon souls use encounter art. Prime it from bound items and from
 * captured souls the player can bind at this place, within the encounter budget. */
static void prepare_soul_art(Image **images, df_u8 *cutout, int *slots)
{
    const ObjectInfo *item;
    int i, k, type, kind, skull;

    skull = 0;

    for (i = 0; i < OBJECTS && g_encounter_art_count < ENCOUNTER_ART; i++) {
        item = &game_state()->object[i];
        if (!item->id) { continue; }
        if (item->parent == game_state()->inventory_id) {
            if (magic_artifact_power(item->id) == 4) { prepare_encounter_art(27,282,images,cutout,slots); }
            if (magic_artifact_power(item->id) == 8) { skull = 1; }
        }
        if (item->type == RECORD_SOUL && item->index >= 0 && item->index < 43) {
            prepare_encounter_art(item->index,255+item->index,images,cutout,slots);
        }
        for (k = 0; g_encounter_art_count < ENCOUNTER_ART && item_enchantment(item->id,k,&type,&kind); k++) {
            if (type == 15 && kind >= 0 && kind < 43) { prepare_encounter_art(kind,255+kind,images,cutout,slots); }
        }
    }
    if (skull) {
        for (i = 0; i < world_entity_count(g_world) && g_encounter_art_count < ENCOUNTER_ART; i++) {
            kind = world_entity_kind(g_world,i,0);
            if (kind >= 0 && kind < 43) { prepare_encounter_art(kind,255+kind,images,cutout,slots); }
        }
    }
}

static int hold_encounter_art(Image **images, df_u8 *cutout, int *slots)
{
    EncounterState *state;
    int i, kind, archive, variants;

    g_encounter_art_count = 0; state = encounter_state();
    for (i = 0; i < 6; i++) {
        kind = state->choices[i]; archive = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+foe_texture_kind(kind);
        variants = kind >= 128 && kind < 146 ? 2 : 1;
        if (!prepare_encounter_art(kind,archive,images,cutout,slots)) { return 0; }
        if (variants == 2 && !prepare_encounter_art(kind,archive+1,images,cutout,slots)) { return 0; }
    }
    /* The creatures of earlier visits and saves are kept rows (made 2) of this place: their art, by the kind and archive their owner says (encounter.c). */
    for (i = 0; i < keep_count(); i++) {
        const Keep *row;

        row = keep_at(i);
        if (row && row->made == 2 && row->key.place == state->place && (row->key.thing & 0x80000000u) &&
            !prepare_encounter_art((int)(row->owner & 255u),(int)((row->owner >> 8) & 4095u),images,cutout,slots)) { return 0; }
    }
    prepare_soul_art(images,cutout,slots);
    g_original_entities = g_level.scene.nentities;
    g_game_entities = (Entity *)mem_alloc((g_original_entities+g_encounter_art_count+1)*sizeof(Entity));
    if (!g_game_entities) { return 0; }
    mem_copy(g_game_entities,g_level.scene.entities,g_original_entities*sizeof(Entity));
    /* Invisible reference entities let the generic renderer budget the art
     * for future spawns as creature resources, rather than shrinking walls. */
    for (i = 0; i < g_encounter_art_count; i++) { g_game_entities[g_original_entities+i] = g_encounter_art[i].entity; }
    g_level.scene.entities = g_game_entities;
    g_level.scene.nentities = g_original_entities+g_encounter_art_count;
    dungeon_rebind(g_game_entities,g_original_entities);
    mem_copy(images,g_world_images,g_world_image_count*sizeof(Image *));
    return 1;
}

/* Future missile pictures are budgeted as entity art at place opening. */
static int hold_magic_art(Image **images, df_u8 *cutout, int *slots)
{
    int element, pose, slot, ids[5][2];
    Image *image;
    Entity *reference;
    reference = &g_game_entities[g_level.scene.nentities++]; entity_init(reference);
    for (element = 0; element < 5; element++) {
        for (pose = 0; pose < 2; pose++) {
            image = texture_load(375+element,pose); if (!image) { return 0; }
            if (image->frames <= 1 || !texture_sheet(image,375+element,pose)) { texture_pad(image); }
            slot = (*slots)++; images[slot] = image; cutout[slot] = 1; ids[element][pose] = slot;
            reference->pose[pose].slot[element] = (df_u16)slot;
            reference->pose[pose].w[element] = reference->pose[pose].h[element] = 64;
        }
    }
    image = texture_load(210,3); if (!image) { return 0; }
    if (image->frames <= 1 || !texture_sheet(image,210,3)) { texture_pad(image); }
    slot = (*slots)++; images[slot] = image; cutout[slot] = 1;
    reference->pose[2].slot[0] = (df_u16)slot; reference->pose[2].w[0] = reference->pose[2].h[0] = 64;
    magic_light_sprite(slot,texture_extent(image->width,image->xscale),texture_extent(image->height,image->yscale));
    magic_slots(ids); return 1;
}

static void treasure_usable(Usable *out, const Flat *flat, int sprite, df_u32 key, int table)
{
    mem_zero(out,sizeof(*out));
    out->kind = USE_OTHER; out->model = -1; out->sprite = sprite; out->mover = -1;
    out->nz = 1;
    out->cx = flat->x; out->cy = flat->y; out->cz = flat->z;
    out->hw = flat->w / 2; out->hh = flat->h / 2; out->hd = 12;
    if (out->hw < 12) { out->hw = 12; }
    if (out->hh < 12) { out->hh = 12; }
    out->reach = 128;
    out->what = USE_TREASURE + table + 1;
    out->tag = (int)key;
    out->label = "Treasure";
}

/* An emptied container stays in the state with no rows; a pile or corpse whose container is that is gone from the view and from the use key. */
static int container_emptied(df_u32 thing)
{
    const ObjectInfo *o;
    int i;

    for (i = 0; i < OBJECTS; i++) {
        o = &game_state()->object[i];
        if (o->id && o->type == RECORD_CONTAINER && o->parent == WORLD_ROOT &&
            o->place_key == object_at(WORLD_ROOT)->place_key && o->thing_key == thing) {
            return !inventory_rows(o->id,-1,0,0);
        }
    }
    return 0;
}

static void hold_dropped_piles(void)
{
    const ObjectInfo *container;
    Flat *flat;
    int i, n, uses, changed;
    if (!g_world || !g_hold_flats || g_hold_pile_serial == game_state()->event_serial) { return; }
    g_hold_pile_serial = game_state()->event_serial;
    n = g_hold_flat_base; uses = g_hold_use_base; changed = 0;
    for (i = 0; i < g_hold_use_base; i++) {
        Usable *u;

        u = &g_hold_usables[i];
        if (u->kind != USE_OTHER || u->what < USE_TREASURE || u->what >= USE_TREASURE+22 || u->sprite < 0 || u->sprite >= g_hold_flat_base) { continue; }
        if (g_hold_flats[u->sprite].w > 0 && container_emptied((df_u32)u->tag)) {
            g_hold_flats[u->sprite].w = g_hold_flats[u->sprite].h = 0;
            u->reach = 0; u->hw = u->hh = u->hd = 0;
            changed = 1;
        }
    }
    for (i = 0; i < dungeon_count(); i++) {
        const Entity *e;
        Flat corpse;
        e = dungeon_entity(i);
        if (!e->dead || !e->live || container_emptied(e->key.thing)) { continue; }
        mem_zero(&corpse,sizeof(corpse));
        corpse.x = e->x; corpse.y = e->y+12; corpse.z = e->z;
        corpse.w = 64; corpse.h = 32;
        treasure_usable(&g_hold_usables[uses],&corpse,-1,e->key.thing,dungeon_loot_list(i));
        g_hold_usables[uses++].label = "Corpse";
    }
    for (i = 0; i < OBJECTS && n < g_hold_capacity && g_hold_pile_slot >= 0; i++) {
        container = &game_state()->object[i];
        if (!container->id || container->type != RECORD_CONTAINER || container->parent != WORLD_ROOT ||
            container->place_key != object_at(WORLD_ROOT)->place_key || !(container->thing_key & 0x80000000u) ||
            !inventory_rows(container->id,-1,0,0)) { continue; }
        flat = &g_hold_flats[n];
        if (n >= g_level.scene.nsprites || flat->x != container->x || flat->y != container->y ||
            flat->z != container->z || (df_u32)g_hold_usables[uses].tag != container->thing_key) { changed = 1; }
        mem_zero(flat,sizeof(*flat));
        flat->x = container->x; flat->y = container->y; flat->z = container->z;
        flat->w = flat->h = 24; flat->file = 216; flat->image = 0;
        flat->slot = (df_u16)g_hold_pile_slot;
        treasure_usable(&g_hold_usables[uses++],flat,n,container->thing_key,-1);
        n++;
    }
    if (changed || n != g_level.scene.nsprites || uses != g_level.scene.nusables) { g_level.scene.version++; }
    g_level.scene.nsprites = n; g_level.scene.nusables = uses;
}

static int hold_treasures(void)
{
    Dungeon hold;
    Bsa *blocks;
    const BsaRec *rec;
    const df_u8 *bytes;
    Marker marks[128];
    Image **images;
    df_u8 *cutout, *glass;
    Image *image;
    int i, b, k, count, uses, slot, images_max;
    hold = g_place_data;
    object_edit(WORLD_ROOT)->place_key = travel_place_key();
    if (world_where()->where.kind == WHERE_DUNGEON) {
        encounter_prepare(object_at(WORLD_ROOT)->place_key,hold.dungeon_type);
    }
    g_hold_loot_table = dungeon_loot_kind(hold.dungeon_type);
    g_hold_capacity = g_level.scene.nsprites + HOLD_TREASURES + OBJECTS;
    g_hold_flats = (Flat *)mem_alloc(g_hold_capacity * sizeof(Flat));
    g_hold_usables = (Usable *)mem_alloc((g_level.scene.nusables+g_hold_capacity)*sizeof(Usable));
    images_max = g_level.nimages + g_level.scene.nentities*21 + HOLD_EXTRA_IMAGES;
    images = (Image **)mem_alloc(images_max*sizeof(Image *));
    cutout = (df_u8 *)mem_alloc(images_max);
    glass = (df_u8 *)mem_alloc(images_max);
    if (!g_hold_flats || !g_hold_usables || !images || !cutout || !glass) { return 0; }
    count = g_level.scene.nsprites; uses = g_level.scene.nusables;
    mem_copy(g_hold_flats,g_level.scene.sprites,count*sizeof(Flat));
    mem_copy(g_hold_usables,g_level.scene.usables,uses*sizeof(Usable));
    mem_copy(images,g_level.images,g_level.nimages*sizeof(Image *));
    mem_copy(cutout,g_level.cutout,g_level.nimages);
    mem_zero(glass,images_max);
    if (g_level.glass) { mem_copy(glass,g_level.glass,g_level.nimages); }
    g_hold_pile_slot = -1;
    for (i = 0; i < count; i++) {
        if (g_hold_flats[i].file != 216) { continue; }
        treasure_usable(&g_hold_usables[uses++],&g_hold_flats[i],i,0x100000u+i,g_hold_loot_table);
        if (g_hold_flats[i].image == 0) { g_hold_pile_slot = g_hold_flats[i].slot; }
    }
    g_world_images = g_level.images;
    g_world_image_count = g_level.nimages;
    slot = g_level.nimages;
    hold_creature_art(images,cutout,&slot);
    if (!hold_encounter_art(images,cutout,&slot) || !hold_magic_art(images,cutout,&slot)) { return 0; }
    hold_arrow_model(images,cutout,&slot);
    if (g_hold_pile_slot < 0) {
        image = texture_load(216,0);
        if (image) { texture_pad(image); images[slot] = image; cutout[slot] = 1; g_hold_pile_slot = slot++; }
    }
    {
        int blood[2];
        for (i = 0; i < 2; i++) {
            blood[i] = -1; image = texture_load(380,i ? 2 : 0);
            if (image) { texture_pad(image); blood[i] = slot; images[slot] = image; cutout[slot++] = 1; }
        }
        dungeon_blood(blood[0],blood[1]);
    }
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) { return 0; }
    collide_use(&g_level.scene);
    for (b = 0; world_where()->where.kind == WHERE_DUNGEON && b < hold.nblocks && count < g_hold_capacity-OBJECTS; b++) {
        rec = bsa_find_name(blocks,hold.block[b].name);
        bytes = rec ? bsa_read_scratch(blocks,rec,BSA_SCRATCH_BLOCK) : 0;
        k = bytes ? rdb_markers(bytes,rec->size,marks,128) : 0;
        for (i = 0; i < k && count < g_hold_capacity-OBJECTS; i++) {
            Flat *flat;
            if (marks[i].record != RDB_MARKER_TREASURE || g_hold_pile_slot < 0) { continue; }
            flat = &g_hold_flats[count]; mem_zero(flat,sizeof(*flat));
            flat->x = marks[i].x + hold.block[b].x*TILE;
            flat->y = marks[i].y; flat->z = marks[i].z + hold.block[b].z*TILE;
            flat->file = 216; flat->image = 0; flat->slot = g_hold_pile_slot;
            image = images[g_hold_pile_slot];
            flat->w = (float)texture_extent(image->width,image->xscale);
            flat->h = (float)texture_extent(image->height,image->yscale);
            {
                float floor;
                /* The floor under the marker: a little above it, down two blocks. */
                if (collide_floor_at(&g_level.scene,flat->x,flat->z,flat->y+16,flat->y-2048,&floor)) {
                    flat->y = floor + flat->h/2;
                }
            }
            treasure_usable(&g_hold_usables[uses++],flat,count,((df_u32)(b+1)<<24)|marks[i].pos,g_hold_loot_table);
            count++;
        }
    }
    bsa_close(blocks);
    g_level.images = images; g_level.cutout = cutout; g_level.glass = glass; g_level.nimages = slot;
    g_level.scene.sprites = g_hold_flats; g_level.scene.usables = g_hold_usables;
    /* Appended sprites have no block spans: use the engine's whole-list path. */
    g_level.scene.sprite_spans = 0;
    g_hold_flat_base = count; g_hold_use_base = uses;
    g_hold_pile_serial = game_state()->event_serial-1;
    hold_dropped_piles();
    return 1;
}

static int hold_actions(void)
{
    static ActionInfo actions[ACTIONS];
    static int next[ACTIONS];
    const ModelAct *raw;
    ActionInfo *out;
    int i, j, count, block;

    count = 0;
    for (i = 0; i < world_model_count(g_world); i++) {
        raw = world_model_action(g_world,i,&block);
        if (!raw || !raw->flags) { continue; }
        if (count == ACTIONS) { gfall_error("Too many dungeon actions"); return 0; }
        out = &actions[count]; mem_zero(out,sizeof(*out));
        out->model = i; out->mover = out->next = -1;
        out->key.place = object_at(WORLD_ROOT)->place_key;
        out->key.thing = ((df_u32)block << 24) | raw->pos;
        out->trigger = raw->trigger & 15; out->kind = raw->flags;
        out->axis = raw->axis; out->duration = raw->duration;
        out->magnitude = raw->magnitude; out->sound = raw->sound;
        next[count++] = raw->next;
        for (j = 0; j < g_level.scene.nmovers; j++) {
            if (g_level.scene.movers[j].model == i) { out->mover = j; out->key = g_level.scene.movers[j].key; break; }
        }
    }
    for (i = 0; i < count; i++) {
        if (next[i] < 0) { continue; }
        for (j = 0; j < count; j++) {
            if ((actions[i].key.thing >> 24) == (actions[j].key.thing >> 24) &&
                (actions[j].key.thing & 0xffffffu) == (df_u32)next[i]) { actions[i].next = j; break; }
        }
    }
    return actions_open(&g_level.scene,actions,count);
}

int seam_open_place(int restoring, int changing, float *x, float *y, float *z)
{
    float sx;
    float sy;
    float sz;
    float yaw;
    float pitch;

    gfall_screen_enter_world();
    if (changing && g_place_open) {
        Camera from;
        move_camera(&from); travel_came_from(&from);
        if (!seam_capture_world()) { gfall_error(state_error()); return 0; }
        mem_zero(magic_state()->missile,sizeof(magic_state()->missile));
    }
    seam_close_hold();
    if (!state_restore_world()) {
        gfall_error("No memory for the saved place");
        return 0;
    }
    /* The borrowed builder suppresses dead mobiles. GFALL keeps their bodies
     * drawable, so restore entity state after it has built every sprite slot. */
    {
        int i;
        g_entity_keep_count = 0;
        for (i = keep_count()-1; i >= 0; i--) {
            if (keep_at(i)->kind == KEEP_ENTITY) {
                g_entity_keeps[g_entity_keep_count] = *keep_at(i);
                keep_forget(g_entity_keeps[g_entity_keep_count++].key);
            }
        }
    }
    if (restoring) {
        TravelWhere where;
        const PlaceWhere *saved;
        int origin[2];
        float feet[3];
        g_saved_where = world_where()->where; saved = &g_saved_where; mem_zero(&where,sizeof(where));
        where.kind = saved->kind; where.located = saved->located; where.region = saved->region; where.index = saved->index;
        where.block = saved->block; where.record = saved->record; where.has_back = saved->has_back;
        where.back_x = saved->back_x; where.back_z = saved->back_z; where.back_yaw = saved->back_yaw*(DF_TAU/2048);
        origin[0] = saved->origin_x; origin[1] = saved->origin_z;
        feet[0] = game_state()->x; feet[1] = game_state()->eye_y-WALK_EYE; feet[2] = game_state()->z;
        travel_from_save(&where,feet,origin,game_state()->yaw*(DF_TAU/2048),game_state()->pitch*(DF_TAU/2048));
    } else if (!changing) { travel_start_game(); }
    travel_set_dungeon_streaming(0);
    g_place_open = travel_open(&g_level,1);
    g_world = travel_whole_world();
    {
        int i;
        for (i = 0; i < g_entity_keep_count; i++) { keep_put(&g_entity_keeps[i]); }
    }
    if (!g_place_open) { gfall_error(travel_error()); return 0; }
    world_restore(); travel_capture();
    if (!g_world) {
        if (!seam_world_art()) { gfall_error("Cannot prepare world gameplay pictures"); return 0; }
        if (x) { *x = 0; } if (y) { *y = WALK_EYE; } if (z) { *z = 0; }
        actions_open(&g_level.scene,0,0);
        return 1;
    }
    g_place_data = *travel_place_data();
    keep_restore(&g_level.scene,0,0,(Entity *)g_level.scene.entities,g_level.scene.nentities,0,0,0);
    {
        int kind[FOES], passive[FOES], rates[FOES], i;
        for (i = 0; i < g_level.scene.nentities && i < FOES; i++) {
            kind[i] = world_entity_kind(g_world,i,&passive[i]);
            rates[i] = gfall_creature_rate(world_entity_archive(g_world,i));
        }
        dungeon_open(&g_level.scene,kind,passive,rates);
        for (i = 0; i < g_level.scene.nentities && i < FOES; i++) {
            if (kind[i] >= 128 && kind[i] < 146) {
                dungeon_variant(i,world_entity_archive(g_world,i)-432);
            }
        }
    }
    if (!hold_treasures()) { gfall_error("Cannot build the Hold treasure containers"); return 0; }
    if (!hold_actions()) { return 0; }
    if (world_where()->where.kind == WHERE_DUNGEON && !encounter_open(&g_level.scene,encounter_art)) {
        gfall_error("Cannot restore the dungeon encounters"); return 0;
    }
    /* A dungeon loaded: the foes of the quests placed in it are made (func_0004C759 links them as a location loads). */
    if (world_where()->where.kind == WHERE_DUNGEON) { quest_location_loaded(world_where()->where.region, gfall_quest_outside()); }
    world_start(g_world, &sx, &sy, &sz, &yaw, &pitch);
    if (!restoring && !changing && world_where()->where.kind == WHERE_DUNGEON) { marker6(&sx,&sy,&sz); }
    if (x) {
        *x = sx;
    }
    if (y) {
        *y = sy;
    }
    if (z) {
        *z = sz;
    }
    return 1;
}

int seam_open_hold(float *x, float *y, float *z) { return seam_open_place(0,0,x,y,z); }
void seam_land(Camera *camera, int restoring, int changing)
{
    TravelPose at;
    MotionState motion;
    int ox, oz, sx, sz;
    travel_land(&g_level,camera,WALK_EYE,&at);
    if (restoring && world_where()->where.kind == WHERE_WORLD) {
        stream_origin(&ox,&oz);
        magic_shift((ox-g_saved_where.origin_x)*STREAM_CHUNK,(oz-g_saved_where.origin_z)*STREAM_CHUNK);
    }
    if (!restoring && !changing && g_world && world_where()->where.kind == WHERE_DUNGEON) { marker6(&at.x,&at.y,&at.z); }
    if (restoring && game_state()->motion_valid) {
        motion = game_state()->motion; stream_origin(&ox,&oz);
        sx = g_saved_where.origin_x-ox; sz = g_saved_where.origin_z-oz;
        if (world_where()->where.kind == WHERE_WORLD) {
            motion.feet[0] += sx*STREAM_CHUNK; motion.safe[0] += sx*STREAM_CHUNK;
            motion.feet[2] += sz*STREAM_CHUNK; motion.safe[2] += sz*STREAM_CHUNK;
        }
        move_restore(&motion);
    } else {
        move_init(); move_place(at.x,at.y,at.z,df_iround(at.yaw*2048/DF_TAU));
        move_capture(&motion); motion.pitch = df_iround(at.pitch*2048/DF_TAU); move_restore(&motion);
    }
    move_camera(camera);
    state_pose(camera->x,move_body()->feet[1]+move_body()->eye_now,camera->z,motion.yaw,motion.pitch);
    travel_capture();
    {
        const char *said;
        said = travel_said(); if (said) { msg_set(said); }
    }
    /* Intentional departure from FALL.EXE: show the building's name on entry.
     * The original shows the name when the player inspects the building from outside. */
    if (changing && !restoring && travel_entered_building()) {
        gfall_building_announce(&world_where()->where);
        gfall_building_entered(&world_where()->where);
    }
}

void seam_close_hold(void)
{
    float x, y, z, yaw;

    if (!g_place_open) { return; }
    maps_close();
    ground_close();
    magic_lighting(0);
    magic_close();
    encounter_close(); g_game_entities = 0; g_original_entities = g_encounter_art_count = 0;
    actions_close();
    /* No queued arrival may carry a position into a newly loaded place. */
    interact_teleport(&x,&y,&z,&yaw);
    interact_set_body(0);
    dungeon_close();
    g_arrow_mesh = 0; g_arrow_models = 0; g_arrow_bounds = 0; g_arrow_spans = 0;
    weapon_cancel();
    gfall_screen_leave_world();
    sound_stop_all();
    texture_cache_clear();
    g_world = 0; g_place_open = 0;
    g_hold_flats = 0; g_hold_usables = 0;
    travel_drop();
    mem_zero(&g_level, sizeof(g_level));
}

int seam_capture_world(void)
{
    if (!g_place_open) { return 0; }
    world_capture();
    actions_capture();
    encounter_capture();
    travel_keep_now();
    keep_save(&g_level.scene,0,g_level.scene.nmovers,g_level.scene.entities,g_level.scene.nentities,0,0,0);
    return state_capture_world();
}

void seam_creatures(const Camera *cam)
{
    ground_frame(cam);
    if (!g_world) {
        return;
    }
    hold_dropped_piles();
    magic_lighting((character_state()->conditions & 16) != 0);
    hold_arrows(cam);
    world_creatures_step(g_world, cam, 2048.0f, 2.0f);
    {
        const Entity *loaded;
        Entity *game;
        int i;
        for (i = 0; g_game_entities && i < g_original_entities; i++) {
            loaded = world_entity(g_world,i); game = &g_game_entities[i];
            if (!game->live && loaded->live) {
                game->pose[ENTITY_WALK] = loaded->pose[ENTITY_WALK]; game->pose[ENTITY_IDLE] = loaded->pose[ENTITY_IDLE];
                game->y = loaded->y; game->live = 1;
            }
        }
    }
    {
        int i;
        for (i = 0; i < dungeon_count(); i++) {
            if ((i < g_original_entities ? dungeon_loaded_marker(i) : dungeon_loaded(i)) && dungeon_entity(i)->dead) { g_hold_pile_serial--; }
        }
    }
    hold_dropped_piles();
    /* The world fills reserved creature picture slots as they stream in. */
    if (g_world_images) { mem_copy((void *)g_level.images,g_world_images,g_world_image_count*sizeof(Image *)); }
}

Level *seam_level(void)
{
    return g_place_open ? &g_level : 0;
}

/* The original moving Light spell is a candle ahead of the player.
 * The current static-decal renderer uses an ambient lift for its light. */
static void magic_lighting(int enabled)
{
    static int active, base, applied;
    int current;
    current = light_ambient();
    if (active && current != applied) { base = current; }
    if (enabled) {
        if (!active) { base = current; }
        applied = base+64; if (applied > 255) { applied = 255; }
        light_set_ambient(applied); active = 1;
    } else if (active) { light_set_ambient(base); active = 0; }
}

static int seam_world_art(void)
{
    static const EncounterOutdoors outdoors = { outdoor_encounter_site, streamed_foe_art, streamed_foe_release };
    int element, frame, slots[5][2], slot;
    const Image *image;
    for (element = 0; element < 5; element++) {
        for (frame = 0; frame < 2; frame++) {
            slot = wres_slot(375+element,frame,WRES_SPRITE);
            if (slot < 0 || !wres_acquire(slot)) { return 0; }
            slots[element][frame] = slot;
        }
    }
    magic_slots(slots); slot = wres_slot(210,3,WRES_SPRITE);
    if (slot < 0 || !wres_acquire(slot)) { return 0; }
    image = wres_image(slot);
    magic_light_sprite(slot,texture_extent(image->width,image->xscale),texture_extent(image->height,image->yscale));
    slot = wres_slot(216,0,WRES_SPRITE);
    if (slot < 0 || !wres_acquire(slot)) { return 0; }
    ground_open(&g_level.scene,slot);
    object_edit(WORLD_ROOT)->place_key = 0x80000000u;
    gfall_street_open();
    dungeon_open_street(&g_level.scene); dungeon_blood(-1,-1);
    encounter_open_outdoors(&g_level.scene,&outdoors);
    return 1;
}
int seam_next_world(void) { return travel_next_is_world(); }
void seam_world_step(Camera *camera, float seconds)
{
    float dx, dz;

    gfall_people_set_camera(camera);
    gfall_street_tick(seconds);
    gfall_building_frame(camera);
    if (!g_place_open || !g_level.streamed) { return; }
    travel_update(camera,1,2,seconds,&dx,&dz);
    if (dx || dz) {
        move_shift(dx,dz); dungeon_shift(dx,dz);
        magic_shift(dx,dz);
    }
    travel_capture();
}
int seam_world_needs_reload(void) { return g_place_open && travel_must_reload(); }
void seam_world_reload(Camera *camera) { if (seam_world_needs_reload()) { travel_reload(camera); } }
