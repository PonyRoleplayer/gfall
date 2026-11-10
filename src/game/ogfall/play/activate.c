#include "game/ogfall/play/activate.h"

#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/screen.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/play/doors.h"
#include "game/ogfall/play/ground.h"

#include "engine/core/df.h"
#include "engine/scene/interact.h"

void og_activate_pixel(int cursor_mode, int cursor_active, int at_centre,
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

    og_world_engine(cx, cy, view_w, view_h, !(og_options_flags() & 1), &sx, &sy);
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

void og_activate_aim(const Camera *camera, int cursor_mode, int cursor_active,
                     int at_centre, int x, int y, Camera *aim)
{
    float dx, dy, dz;
    int px, py;
    *aim = *camera;
    if (at_centre) { return; }
    og_activate_pixel(cursor_mode,cursor_active,0,x,y,&px,&py);
    ray_from_pixel(camera,df_view_w(),df_view_h(),px,py,&dx,&dy,&dz);
    aim->yaw = df_atan2(dx,dz); aim->pitch = df_atan2(dy,df_sqrt(dx*dx+dz*dz));
}

static int g_shelf_what = -1;
static int (*g_shelf_use)(int index, int tag);

void og_activate_set_shelf(int what, int (*use)(int index, int tag))
{
    g_shelf_what = what;
    g_shelf_use = use;
}

int og_activate(const Scene *scene, const Camera *cam, int mode,
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
    og_activate_pixel(cursor_mode, cursor_active, at_centre, classic_x, classic_y, &px, &py);
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
    if (og_world_state()->where.kind == OG_PLACE_WORLD) {
        df_u32 pile;
        float reach;
        interact_cast(scene,x,y,z,dx,dy,dz,128,&hit);
        reach = hit.wall < 128 ? hit.wall : 128;
        if (hit.usable >= 0 && hit.dist < reach) { reach = hit.dist; }
        pile = og_ground_aim(x,y,z,dx,dy,dz,reach);
        if (pile) {
            if (mode == OG_MODE_GRAB) { return og_possessions_open(OG_VIEW_INVENTORY,pile); }
            if (mode == OG_MODE_INFO) { interact_say("Treasure"); }
            return 1;
        }
    }
    if (!interact_aim_from(scene, x, y, z, dx, dy, dz, &hit)) {
        return 0;
    }
    if (hit.usable >= 0 && hit.usable < scene->nusables) {
        const Usable *usable;
        if (og_door_use(scene,hit.usable,mode)) { return 1; }
        usable = &scene->usables[hit.usable];
        if (usable->kind == USE_OTHER && g_shelf_use && usable->what == g_shelf_what) {
            if (mode == OG_MODE_GRAB) { return g_shelf_use(hit.usable, usable->tag); }
            if (mode == OG_MODE_INFO) { interact_say("shelf"); }
            return 1;
        }
        if (usable->kind == USE_OTHER && usable->what >= OG_USE_TREASURE && usable->what < OG_USE_TREASURE+22) {
            if (mode == OG_MODE_GRAB) {
                df_u32 container;
                container = og_loot_container(og_object(OG_WORLD_ROOT)->place_key,(df_u32)usable->tag,
                                              usable->cx,usable->cy,usable->cz,usable->what-OG_USE_TREASURE-1);
                if (container) { return og_possessions_open(OG_VIEW_INVENTORY,container); }
                interact_say("There is no room for another container.");
            } else if (mode == OG_MODE_INFO) { interact_say("Treasure"); }
            return 1;
        }
    }
    if (mode == OG_MODE_GRAB) {
        interact_request_use_from(x, y, z, dx, dy, dz);
    } else if (mode == OG_MODE_INFO && hit.usable >= 0 && hit.usable < scene->nusables) {
        if (scene->usables[hit.usable].label) {
            interact_say(scene->usables[hit.usable].label);
        }
    }
    return 1;
}
