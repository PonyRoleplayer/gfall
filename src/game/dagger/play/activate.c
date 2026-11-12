#include "game/dagger/play/activate.h"
#include "engine/core/mem.h"
#include "engine/scene/ent.h"
#include "engine/scene/entity.h"

#include "game/dagger/play/hud.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/options.h"
#include "game/dagger/play/doors.h"
#include "game/dagger/play/ground.h"

#include "engine/core/df.h"
#include "engine/scene/interact.h"

void activate_pixel(int cursor_mode, int cursor_active, int at_centre,
                       int classic_x, int classic_y, int *px, int *py)
{
    int x;
    int y;

    if (at_centre) {
        x = 160;
        y = 100;
    } else if (cursor_mode && cursor_active) {
        x = classic_x;
        y = classic_y;
    } else {
        x = classic_x + 6;
        y = classic_y + 6;
    }
    if (px) {
        *px = x;
    }
    if (py) {
        *py = y;
    }
}

static void ray_from_pixel(const Camera *cam, int view_w, int view_h, int cx, int cy,
                           float *dx, float *dy, float *dz)
{
    Basis basis;
    int sx;
    int sy;
    float vx;
    float vy;
    float vz;
    float z1;
    float len;
    float focal;

    world_engine(cx, cy, view_w, view_h, !(options_flags() & 1), &sx, &sy);
    focal = camera_focal();
    if (focal < 1.0f) {
        focal = 1.0f;
    }
    vx = ((float)sx - df_view_cx()) / focal;
    vy = ((float)sy - df_view_cy()) / focal;
    vz = 1.0f;
    camera_basis(cam, &basis);
    z1 = -vy * basis.sp + vz * basis.cp;
    *dx = vx * basis.rx + z1 * basis.fx;
    *dy = vy * basis.cp + vz * basis.sp;
    *dz = vx * basis.rz + z1 * basis.fz;
    len = df_sqrt((*dx) * (*dx) + (*dy) * (*dy) + (*dz) * (*dz));
    if (len < 0.0001f) {
        *dx = basis.fx;
        *dy = basis.sp;
        *dz = basis.fz;
        return;
    }
    *dx /= len;
    *dy /= len;
    *dz /= len;
}

void activate_aim(const Camera *camera, int cursor_mode, int cursor_active,
                     int at_centre, int x, int y, Camera *aim)
{
    float dx, dy, dz;
    int px, py;
    *aim = *camera;
    if (at_centre) { return; }
    activate_pixel(cursor_mode,cursor_active,0,x,y,&px,&py);
    ray_from_pixel(camera,df_view_w(),df_view_h(),px,py,&dx,&dy,&dz);
    aim->yaw = df_atan2(dx,dz); aim->pitch = df_atan2(dy,df_sqrt(dx*dx+dz*dz));
}

static int g_container_what = -1;
static int (*g_container_use)(int index);
static int g_global_what = -1;
static void (*g_global_use)(int state);
static int g_shelf_what = -1;
static int (*g_shelf_use)(int index, int tag);
static int (*g_person_use)(const Camera *aim, int mode);
static int (*g_talk_use)(const Usable *person, int mode, float dist, const Camera *cam);

void activate_set_talk_use(int (*use)(const Usable *person, int mode, float dist, const Camera *cam))
{
    g_talk_use = use;
}

void activate_set_person(int (*use)(const Camera *aim, int mode))
{
    g_person_use = use;
}

void activate_set_container(int what, int (*use)(int index))
{
    g_container_what = what;
    g_container_use = use;
}

void activate_set_global(int what, void (*use)(int state))
{
    g_global_what = what;
    g_global_use = use;
}

void activate_set_shelf(int what, int (*use)(int index, int tag))
{
    g_shelf_what = what;
    g_shelf_use = use;
}

int activate(const Scene *scene, const Camera *cam, int mode,
                int cursor_mode, int cursor_active, int at_centre,
                int classic_x, int classic_y)
{
    UseHit hit;
    float x;
    float y;
    float z;
    float dx;
    float dy;
    float dz;
    int px;
    int py;

    if (!scene || !cam) {
        return 0;
    }
    activate_pixel(cursor_mode, cursor_active, at_centre, classic_x, classic_y, &px, &py);
    if (at_centre) {
        Basis basis;

        camera_basis(cam, &basis);
        dx = basis.fx * basis.cp;
        dy = basis.sp;
        dz = basis.fz * basis.cp;
    } else {
        ray_from_pixel(cam, df_view_w(), df_view_h(), px, py, &dx, &dy, &dz);
    }
    x = cam->x;
    y = cam->y;
    z = cam->z;
    if (world_where()->where.kind == WHERE_WORLD) {
        df_u32 pile;
        float reach;
        interact_cast(scene,x,y,z,dx,dy,dz,128,&hit);
        reach = hit.wall < 128 ? hit.wall : 128;
        if (hit.usable >= 0 && hit.dist < reach) { reach = hit.dist; }
        pile = ground_aim(x,y,z,dx,dy,dz,reach);
        if (pile) {
            if (mode == MODE_GRAB) { return possessions_open(VIEW_INVENTORY,pile); }
            if (mode == MODE_INFO) { interact_say("Treasure"); }
            return 1;
        }
    }
    if (!interact_aim_from(scene, x, y, z, dx, dy, dz, &hit)) {
        /* Nothing usable in the way: whoever stands in the view is the game's to talk to. */
        if (g_person_use) {
            Camera aim;

            activate_aim(cam, cursor_mode, cursor_active, at_centre, classic_x, classic_y, &aim);
            return g_person_use(&aim, mode);
        }
        return 0;
    }
    if (hit.entity && hit.usable < 0) {
        /* A person standing in a building is an entity (docs/entities.md): the use ray took it, and the game answers as it did to a person's usable. */
        const Entity *e;

        e = ent_get(hit.entity);
        if (e && e->use.kind == USE_TALK && g_talk_use) {
            Usable person;

            mem_zero(&person, sizeof(person));
            person.kind = USE_TALK; person.model = -1; person.sprite = -1; person.mover = -1;
            person.cx = e->x; person.cy = e->y + (float)e->pose[ENTITY_IDLE].h[0] * 0.5f; person.cz = e->z;
            person.hw = e->radius; person.hd = e->radius; person.hh = (float)e->pose[ENTITY_IDLE].h[0] * 0.5f;
            person.reach = (float)e->use.reach; person.what = e->use.what; person.tag = e->use.tag; person.label = e->use.label;
            if (g_talk_use(&person, mode, hit.dist, cam)) { return 1; }
        }
    }
    if (hit.usable >= 0 && hit.usable < scene->nusables) {
        const Usable *usable;
        if (door_use(scene,hit.usable,mode)) { return 1; }
        usable = &scene->usables[hit.usable];
        if (usable->kind == USE_TALK && g_talk_use && g_talk_use(usable, mode, hit.dist, cam)) { return 1; }
        if (usable->kind == USE_OTHER && g_container_use && usable->what == g_container_what) {
            if (mode == MODE_GRAB) { return g_container_use(hit.usable); }
            if (mode == MODE_INFO) { interact_say("container"); }
            return 1;
        }
        if (usable->kind == USE_OTHER && g_global_use && usable->what == g_global_what) {
            if (mode == MODE_GRAB) { g_global_use(usable->tag); }
            return 1;
        }
        if (usable->kind == USE_OTHER && g_shelf_use && usable->what == g_shelf_what) {
            if (mode == MODE_GRAB) { return g_shelf_use(hit.usable, usable->tag); }
            if (mode == MODE_INFO) { interact_say("shelf"); }
            return 1;
        }
        if (usable->kind == USE_OTHER && usable->what >= USE_TREASURE && usable->what < USE_TREASURE+22) {
            if (mode == MODE_GRAB) {
                df_u32 container;
                container = loot_container(object_at(WORLD_ROOT)->place_key,(df_u32)usable->tag,
                                              usable->cx,usable->cy,usable->cz,usable->what-USE_TREASURE-1);
                if (container) { return possessions_open(VIEW_INVENTORY,container); }
                interact_say("There is no room for another container.");
            } else if (mode == MODE_INFO) { interact_say("Treasure"); }
            return 1;
        }
    }
    if (g_person_use && hit.usable < 0) {
        Camera aim;

        activate_aim(cam, cursor_mode, cursor_active, at_centre, classic_x, classic_y, &aim);
        if (g_person_use(&aim, mode)) { return 1; }
    }
    if (mode == MODE_GRAB) {
        interact_request_use_from(x, y, z, dx, dy, dz);
    } else if (mode == MODE_INFO && hit.usable >= 0 && hit.usable < scene->nusables) {
        if (scene->usables[hit.usable].label) {
            interact_say(scene->usables[hit.usable].label);
        }
    }
    return 1;
}
