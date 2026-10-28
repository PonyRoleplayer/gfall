/*
 * Engine tests. No Daggerfall archives. Transforms, camera, the visibility
 * list, the loader table, the VFS mount string, and the console's log,
 * command table, history, and line editing.
 */

#include <stdio.h>
#include <string.h>

#include "check_util.h"

#include "engine/assets/loader.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/mip.h"
#include "engine/scene/light.h"
#include "engine/scene/clock.h"
#include "engine/scene/daylight.h"
#include "engine/scene/weather.h"
#include "engine/scene/walker.h"
#include "engine/scene/body.h"
#include "engine/gfx/rig.h"
#include "engine/scene/interact.h"
#include "engine/core/prof.h"
#include "engine/gfx/texres.h"
#include "engine/scene/collide.h"
#include "engine/scene/scene.h"
#include "engine/scene/entity.h"
#include "engine/ui/cmd.h"
#include "engine/sound/mixer.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"

static int probe_hits;

static int probe_load(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    int *flag;

    (void)name;
    (void)bytes;
    (void)size;
    flag = (int *)out;
    if (flag) {
        *flag = 1;
    }
    probe_hits++;
    return 1;
}

static void check_rotation(void)
{
    Instance in;
    float x, y, z;

    mem_zero(&in, sizeof(in));
    instance_set_rotation(&in, 0.0f, 0.0f, 0.0f);
    instance_point(&in, 3.0f, 4.0f, 5.0f, &x, &y, &z);
    check_expect(check_near(x, 3.0f, 0.02f) && check_near(y, 4.0f, 0.02f) &&
                 check_near(z, 5.0f, 0.02f),
                 "zero rotation moved the point");

    /* Right-hand +90 degrees about Y takes local +X to world -Z. */
    instance_set_rotation(&in, 0.0f, 1.5707963f, 0.0f);
    instance_point(&in, 1.0f, 0.0f, 0.0f, &x, &y, &z);
    check_expect(check_near(x, 0.0f, 0.02f) && check_near(y, 0.0f, 0.02f) &&
                 check_near(z, -1.0f, 0.02f),
                 "+Y rotation did not take +X toward -Z");
}

static void check_camera(void)
{
    Camera cam;
    Basis basis;
    float vx, vy, vz, sx, sy, oow, ooz;

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    check_expect(check_near(basis.fx, 0.0f, 0.02f) && check_near(basis.fz, 1.0f, 0.02f),
                 "yaw 0 forward is not +Z");
    check_expect(check_near(basis.rx, 1.0f, 0.02f) && check_near(basis.rz, 0.0f, 0.02f),
                 "yaw 0 right is not +X");
    camera_view(&cam, &basis, 10.0f, 0.0f, 20.0f, &vx, &vy, &vz);
    check_expect(check_near(vx, 10.0f, 0.02f) && check_near(vy, 0.0f, 0.02f) &&
                 check_near(vz, 20.0f, 0.02f),
                 "yaw 0 did not put +X on the right and +Z in front");
    check_expect(camera_project(vx, vy, vz, &sx, &sy, &oow, &ooz),
                 "point in front did not project");
}

static void check_scene(void)
{
    Instance models[2];
    Flat sprites[1];
    Scene scene;
    int order[4];
    int n;

    mem_zero(models, sizeof(models));
    mem_zero(sprites, sizeof(sprites));
    mem_zero(&scene, sizeof(scene));
    scene.models = models;
    scene.nmodels = 2;
    scene.sprites = sprites;
    scene.nsprites = 1;
    n = scene_visible_models(&scene, 0, order, 4);
    check_expect(n == 2 && order[0] == 0 && order[1] == 1,
                 "models were not listed in storage order");
    n = scene_visible_sprites(&scene, 0, order, 4);
    check_expect(n == 1 && order[0] == 0,
                 "sprites were not listed in storage order");
}

/* Quad in local XY, normal -Z, so it faces a camera looking along +Z. */
static Mesh *wall_mesh(float half)
{
    Mesh *mesh;
    Tri *tris;
    Face *face;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    tris = (Tri *)mem_alloc((df_u32)sizeof(Tri) * 2u);
    face = (Face *)mem_alloc(sizeof(Face));
    if (!mesh || !tris || !face) {
        return 0;
    }
    tris[0].x[0] = -half;
    tris[0].y[0] = -half;
    tris[0].z[0] = 0.0f;
    tris[0].x[1] = half;
    tris[0].y[1] = -half;
    tris[0].z[1] = 0.0f;
    tris[0].x[2] = half;
    tris[0].y[2] = half;
    tris[0].z[2] = 0.0f;
    tris[1].x[0] = -half;
    tris[1].y[0] = -half;
    tris[1].z[0] = 0.0f;
    tris[1].x[1] = half;
    tris[1].y[1] = half;
    tris[1].z[1] = 0.0f;
    tris[1].x[2] = -half;
    tris[1].y[2] = half;
    tris[1].z[2] = 0.0f;
    face->nx = 0.0f;
    face->ny = 0.0f;
    face->nz = -1.0f;
    face->first = 0;
    face->ntris = 2;
    mesh->tris = tris;
    mesh->ntris = 2;
    mesh->faces = face;
    mesh->nfaces = 1;
    mesh->face_cap = 1;
    return mesh;
}

static Mesh *tri_mesh(float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float x2, float y2, float z2)
{
    Mesh *mesh;
    Tri *tri;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    tri = (Tri *)mem_alloc(sizeof(Tri));
    if (!mesh || !tri) {
        return 0;
    }
    tri->x[0] = x0;
    tri->y[0] = y0;
    tri->z[0] = z0;
    tri->x[1] = x1;
    tri->y[1] = y1;
    tri->z[1] = z1;
    tri->x[2] = x2;
    tri->y[2] = y2;
    tri->z[2] = z2;
    mesh->tris = tri;
    mesh->ntris = 1;
    return mesh;
}

/* Left wall plus a roof. Unioning those two faces fills the picture. */
static Mesh *porch_mesh(void)
{
    Mesh *mesh;
    Tri *tris;
    Face *faces;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    tris = (Tri *)mem_alloc((df_u32)sizeof(Tri) * 4u);
    faces = (Face *)mem_alloc((df_u32)sizeof(Face) * 2u);
    if (!mesh || !tris || !faces) {
        return 0;
    }
    tris[0].x[0] = -30.0f; tris[0].y[0] = -50.0f; tris[0].z[0] = 20.0f;
    tris[0].x[1] = -30.0f; tris[0].y[1] = -50.0f; tris[0].z[1] = 100.0f;
    tris[0].x[2] = -30.0f; tris[0].y[2] = 50.0f; tris[0].z[2] = 100.0f;
    tris[1].x[0] = -30.0f; tris[1].y[0] = -50.0f; tris[1].z[0] = 20.0f;
    tris[1].x[1] = -30.0f; tris[1].y[1] = 50.0f; tris[1].z[1] = 100.0f;
    tris[1].x[2] = -30.0f; tris[1].y[2] = 50.0f; tris[1].z[2] = 20.0f;
    tris[2].x[0] = -100.0f; tris[2].y[0] = 40.0f; tris[2].z[0] = 10.0f;
    tris[2].x[1] = 100.0f; tris[2].y[1] = 40.0f; tris[2].z[1] = 10.0f;
    tris[2].x[2] = 100.0f; tris[2].y[2] = 40.0f; tris[2].z[2] = 140.0f;
    tris[3].x[0] = -100.0f; tris[3].y[0] = 40.0f; tris[3].z[0] = 10.0f;
    tris[3].x[1] = 100.0f; tris[3].y[1] = 40.0f; tris[3].z[1] = 140.0f;
    tris[3].x[2] = -100.0f; tris[3].y[2] = 40.0f; tris[3].z[2] = 140.0f;
    faces[0].nx = 1.0f;
    faces[0].ny = 0.0f;
    faces[0].nz = 0.0f;
    faces[0].first = 0;
    faces[0].ntris = 2;
    faces[1].nx = 0.0f;
    faces[1].ny = -1.0f;
    faces[1].nz = 0.0f;
    faces[1].first = 2;
    faces[1].ntris = 2;
    mesh->tris = tris;
    mesh->ntris = 4;
    mesh->faces = faces;
    mesh->nfaces = 2;
    mesh->face_cap = 2;
    mesh->id = 1;
    return mesh;
}

/* Trapezoid in local XY. The outer square includes the empty corner
   above the short edge. A cover that uses that square fails the test. */
static Mesh *slope_mesh(void)
{
    Mesh *mesh;
    Tri *tris;
    Face *face;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    tris = (Tri *)mem_alloc((df_u32)sizeof(Tri) * 2u);
    face = (Face *)mem_alloc(sizeof(Face));
    if (!mesh || !tris || !face) {
        return 0;
    }
    tris[0].x[0] = -50.0f;
    tris[0].y[0] = -30.0f;
    tris[0].z[0] = 0.0f;
    tris[0].x[1] = 50.0f;
    tris[0].y[1] = -50.0f;
    tris[0].z[1] = 0.0f;
    tris[0].x[2] = 50.0f;
    tris[0].y[2] = 50.0f;
    tris[0].z[2] = 0.0f;
    tris[1].x[0] = -50.0f;
    tris[1].y[0] = -30.0f;
    tris[1].z[0] = 0.0f;
    tris[1].x[1] = 50.0f;
    tris[1].y[1] = 50.0f;
    tris[1].z[1] = 0.0f;
    tris[1].x[2] = -50.0f;
    tris[1].y[2] = 30.0f;
    tris[1].z[2] = 0.0f;
    face->nx = 0.0f;
    face->ny = 0.0f;
    face->nz = -1.0f;
    face->first = 0;
    face->ntris = 2;
    mesh->tris = tris;
    mesh->ntris = 2;
    mesh->faces = face;
    mesh->nfaces = 1;
    mesh->face_cap = 1;
    mesh->id = 1;
    return mesh;
}

/* 1 when (x, y) is inside the projected trapezoid. A point on the edge counts. */
static int quad_inside(float x, float y, const float *qx, const float *qy)
{
    int i;
    int sign;

    sign = 0;
    for (i = 0; i < 4; i++) {
        int j;
        float cross;
        int s;

        j = (i + 1) & 3;
        cross = (qx[j] - qx[i]) * (y - qy[i]) - (qy[j] - qy[i]) * (x - qx[i]);
        if (cross > 1.0f) {
            s = 1;
        } else if (cross < -1.0f) {
            s = -1;
        } else {
            continue;
        }
        if (sign == 0) {
            sign = s;
        } else if (s != sign) {
            return 0;
        }
    }
    return sign != 0;
}

/*
 * One ARCH3D-style plane: a fan of n points, triangle k is
 * (0, k + 1, k + 2), the same order the loader fans a plane into.
 * The normal is the plane's front. id 1, so it is not ground.
 */
static Mesh *fan_mesh(const float *x, const float *y, const float *z, int n,
                      float nx, float ny, float nz)
{
    Mesh *mesh;
    Tri *tris;
    Face *face;
    int k;

    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    tris = (Tri *)mem_alloc((df_u32)sizeof(Tri) * (df_u32)(n - 2));
    face = (Face *)mem_alloc(sizeof(Face));
    if (!mesh || !tris || !face) {
        return 0;
    }
    for (k = 0; k < n - 2; k++) {
        tris[k].x[0] = x[0];
        tris[k].y[0] = y[0];
        tris[k].z[0] = z[0];
        tris[k].x[1] = x[k + 1];
        tris[k].y[1] = y[k + 1];
        tris[k].z[1] = z[k + 1];
        tris[k].x[2] = x[k + 2];
        tris[k].y[2] = y[k + 2];
        tris[k].z[2] = z[k + 2];
    }
    face->nx = nx;
    face->ny = ny;
    face->nz = nz;
    face->first = 0;
    face->ntris = n - 2;
    mesh->tris = tris;
    mesh->ntris = n - 2;
    mesh->faces = face;
    mesh->nfaces = 1;
    mesh->face_cap = 1;
    mesh->id = 1;
    return mesh;
}

static void place_at(Instance *in, Mesh *mesh, float x, float y, float z)
{
    mem_zero(in, sizeof(*in));
    in->mesh = mesh;
    in->x = x;
    in->y = y;
    in->z = z;
    instance_set_rotation(in, 0.0f, 0.0f, 0.0f);
}

static void check_cull(void)
{
    Mesh *mesh;
    Mesh *span;
    Mesh *behind;
    Instance models[4];
    Flat sprites[2];
    Bound bounds[2];
    Span spans[2];
    Scene scene;
    Camera cam;
    Basis basis;
    int order[4];
    int n;

    mesh = tri_mesh(-1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f);
    /* Local Z from -400 to 0, so a placement past the fog can still reach back. */
    span = tri_mesh(0.0f, 0.0f, -400.0f, 1.0f, 0.0f, -400.0f, 0.0f, 1.0f, 0.0f);
    /* Local Z around -800. A half turn about Y brings it in front of the camera. */
    behind = tri_mesh(0.0f, 0.0f, -800.0f, 1.0f, 0.0f, -800.0f, 0.0f, 1.0f, -790.0f);
    check_expect(mesh && span && behind, "cull meshes were not allocated");
    if (!mesh || !span || !behind) {
        return;
    }

    mem_zero(&scene, sizeof(scene));
    scene_set_limits(DF_FAR, 1);
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    place_at(&models[0], mesh, 0.0f, 0.0f, 200.0f);
    place_at(&models[1], mesh, 0.0f, 0.0f, -80.0f);
    place_at(&models[2], mesh, 900.0f, 0.0f, 40.0f);
    place_at(&models[3], mesh, 0.0f, 0.0f, DF_FOG_FAR + 80.0f);
    scene.models = models;
    scene.nmodels = 4;
    scene.sprites = 0;
    scene.nsprites = 0;
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1 && order[0] == 0,
                 "frustum kept a model outside the view");

    place_at(&models[0], mesh, 0.0f, 900.0f, 30.0f);
    scene.nmodels = 1;
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 0, "model above the view was kept");

    /* Positive yaw looks toward world +X. The side model is then in front. */
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 1.5707963f, 0.0f);
    place_at(&models[0], mesh, 0.0f, 0.0f, 200.0f);
    place_at(&models[1], mesh, 0.0f, 0.0f, -80.0f);
    place_at(&models[2], mesh, 900.0f, 0.0f, 40.0f);
    place_at(&models[3], mesh, 0.0f, 0.0f, DF_FOG_FAR + 80.0f);
    scene.nmodels = 4;
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1 && order[0] == 2,
                 "yaw did not keep the model in front");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    place_at(&models[0], span, 0.0f, 0.0f, DF_FOG_FAR + 100.0f);
    scene.nmodels = 1;
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1 && order[0] == 0,
                 "model crossing the fog was dropped");

    /*
     * The fog table is solid only from its first entry past 1100,
     * 8192 / 7. Between the two the card shows a few percent of the
     * model, so the cull keeps it.
     */
    check_expect(check_near(df_fog_solid(df_fog_far(DF_FAR)), 8192.0f / 7.0f, 0.01f),
                 "fog's first solid entry is not 8192 / 7 for the default far clip");
    place_at(&models[0], mesh, 0.0f, 0.0f, DF_FOG_FAR + 40.0f);
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1, "a model fog does not yet hide was culled");

    place_at(&models[0], behind, 0.0f, 0.0f, 50.0f);
    instance_set_rotation(&models[0], 0.0f, 3.14159265f, 0.0f);
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1 && order[0] == 0,
                 "rotated model in front was dropped");

    scene_set_limits(DF_FAR, 0);
    place_at(&models[0], mesh, 0.0f, 0.0f, (DF_FOG_FAR + DF_FAR) * 0.5f);
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1, "fog off hid a model inside the far clip");
    place_at(&models[0], mesh, 0.0f, 0.0f, DF_FAR + 80.0f);
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 0, "fog off kept a model past the far clip");

    scene_set_limits(DF_FAR, 1);
    mem_zero(sprites, sizeof(sprites));
    sprites[0].x = 0.0f;
    sprites[0].y = 0.0f;
    sprites[0].z = 150.0f;
    sprites[0].w = 32.0f;
    sprites[0].h = 64.0f;
    sprites[1].x = 0.0f;
    sprites[1].y = 0.0f;
    sprites[1].z = DF_FOG_FAR + 200.0f;
    sprites[1].w = 32.0f;
    sprites[1].h = 64.0f;
    scene.models = 0;
    scene.nmodels = 0;
    scene.sprites = sprites;
    scene.nsprites = 2;
    n = scene_visible_sprites(&scene, &cam, order, 4);
    check_expect(n == 1 && order[0] == 0,
                 "sprite past the fog was kept");

    place_at(&models[0], mesh, 0.0f, 0.0f, 100.0f);
    place_at(&models[1], mesh, 0.0f, 0.0f, -100.0f);
    place_at(&models[2], mesh, 0.0f, 0.0f, 180.0f);
    scene.models = models;
    scene.nmodels = 3;
    scene.sprites = 0;
    scene.nsprites = 0;
    n = scene_visible_models(&scene, &cam, order, 1);
    check_expect(n == 1 && order[0] == 0,
                 "cap did not keep the first visible model");

    /* Normal +Z on a point in front points away from a camera looking +Z. */
    place_at(&models[0], mesh, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    check_expect(!instance_faces(&models[0], &cam, &basis,
                                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 10.0f),
                 "back face was kept");
    check_expect(instance_faces(&models[0], &cam, &basis,
                                0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 10.0f),
                 "front face was dropped");

    place_at(&models[0], mesh, 0.0f, 0.0f, 200.0f);
    place_at(&models[1], mesh, 0.0f, 0.0f, 8000.0f);
    check_expect(instance_world_box(&models[0], &bounds[0]), "near box was not baked");
    check_expect(instance_world_box(&models[1], &bounds[1]), "far box was not baked");
    spans[0].first = 0;
    spans[0].count = 1;
    spans[0].cx = bounds[0].cx;
    spans[0].cy = bounds[0].cy;
    spans[0].cz = bounds[0].cz;
    spans[0].ex = bounds[0].ex;
    spans[0].ey = bounds[0].ey;
    spans[0].ez = bounds[0].ez;
    spans[1].first = 1;
    spans[1].count = 1;
    spans[1].cx = bounds[1].cx;
    spans[1].cy = bounds[1].cy;
    spans[1].cz = bounds[1].cz;
    spans[1].ex = bounds[1].ex;
    spans[1].ey = bounds[1].ey;
    spans[1].ez = bounds[1].ez;
    scene.models = models;
    scene.bounds = bounds;
    scene.nmodels = 2;
    scene.spans = spans;
    scene.nspans = 2;
    scene.sprites = 0;
    scene.nsprites = 0;
    n = scene_visible_models(&scene, &cam, order, 4);
    check_expect(n == 1 && order[0] == 0,
                 "block past the fog was kept");
    scene_set_limits(DF_FAR, 1);

    /*
     * A wall at 100 hides the model straight behind it and leaves the
     * one beside it. Cull off lists all three.
     */
    {
        Mesh *wall;

        wall = wall_mesh(40.0f);
        check_expect(wall != 0, "wall mesh was not allocated");
        if (wall) {
            scene.bounds = 0;
            scene.spans = 0;
            scene.nspans = 0;
            camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            /* Farther than the close-wall test, so it does not cover the street. */
            wall->id = 1;
            place_at(&models[0], wall, 0.0f, 0.0f, 100.0f);
            place_at(&models[1], mesh, 10.0f, -20.0f, 400.0f);
            place_at(&models[2], mesh, 150.0f, 0.0f, 250.0f);
            scene.models = models;
            scene.nmodels = 3;
            scene.sprites = 0;
            scene.nsprites = 0;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2 && order[0] == 0 && order[1] == 2,
                         "model beside the wall was dropped");

            mem_zero(sprites, sizeof(sprites));
            sprites[0].x = 10.0f;
            sprites[0].y = -20.0f;
            sprites[0].z = 400.0f;
            sprites[0].w = 8.0f;
            sprites[0].h = 8.0f;
            sprites[1].x = 150.0f;
            sprites[1].y = 0.0f;
            sprites[1].z = 250.0f;
            sprites[1].w = 8.0f;
            sprites[1].h = 8.0f;
            scene.sprites = sprites;
            scene.nsprites = 2;
            n = scene_visible_sprites(&scene, &cam, order, 4);
            check_expect(n == 1 && order[0] == 1,
                         "sprite beside the wall was dropped");

            scene_set_cull(0);
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 3, "cull off still hid the model behind the wall");
            scene_set_cull(1);
            /*
             * Nose in a tall mesh. The wall slants from z=40 at its
             * bottom edge to behind the eye at its top, so its box
             * reaches from behind the eye to z=40 and it fills the
             * picture. The model at z=80 is behind it, so it is not
             * drawn. Both triangles stay one plane, fanned from the
             * bottom-left corner as the loader fans one.
             */
            wall->tris[0].z[0] = 40.0f;
            wall->tris[0].z[1] = 40.0f;
            wall->tris[0].z[2] = -20.0f;
            wall->tris[1].z[0] = 40.0f;
            wall->tris[1].z[1] = -20.0f;
            wall->tris[1].z[2] = -20.0f;
            wall->bound_n = 0;
            place_at(&models[0], wall, 0.0f, 0.0f, 0.0f);
            place_at(&models[1], mesh, 0.0f, 0.0f, 80.0f);
            scene.nmodels = 2;
            scene.nsprites = 0;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 1 && order[0] == 0,
                         "model past the lens wall was kept");
            /*
             * A closer wall that does not fill the picture. The model
             * straight behind it goes. The one beside it stays.
             */
            {
                Mesh *part;

                part = wall_mesh(32.0f);
                check_expect(part != 0, "partial wall was not allocated");
                if (part) {
                    part->id = 1;
                    place_at(&models[0], part, 0.0f, 0.0f, 55.0f);
                    place_at(&models[1], mesh, 0.0f, 0.0f, 120.0f);
                    place_at(&models[2], mesh, 55.0f, 0.0f, 80.0f);
                    scene.nmodels = 3;
                    n = scene_visible_models(&scene, &cam, order, 4);
                    check_expect(n == 2 && order[0] == 0 && order[1] == 2,
                                 "model beside the wall was dropped");
                }
            }
            /*
             * Under a porch. The roof and the side wall must not
             * combine into one rectangle the size of the picture.
             * The model in the opening stays.
             */
            {
                Mesh *porch;

                porch = porch_mesh();
                check_expect(porch != 0, "porch was not allocated");
                if (porch) {
                    place_at(&models[0], porch, 0.0f, 0.0f, 0.0f);
                    place_at(&models[1], mesh, 0.0f, 0.0f, 180.0f);
                    scene.nmodels = 2;
                    scene.nsprites = 0;
                    n = scene_visible_models(&scene, &cam, order, 4);
                    check_expect(n == 2 && order[0] == 0 && order[1] == 1,
                                 "model under the roof was dropped");
                }
            }
            /*
             * A trapezoid. The outer square covers the empty corner
             * above the short edge. The cover has to stay on the
             * trapezoid, and a model in that corner stays.
             */
            {
                Mesh *slope;
                float qx[4];
                float qy[4];
                float oow;
                float ooz;
                int ci;
                int ok;
                static const float lx[4] = {-50.0f, 50.0f, 50.0f, -50.0f};
                static const float ly[4] = {-30.0f, -50.0f, 50.0f, 30.0f};

                slope = slope_mesh();
                check_expect(slope != 0, "slope was not allocated");
                if (slope) {
                    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                    for (ci = 0; ci < 4; ci++) {
                        camera_project(lx[ci], ly[ci], 100.0f,
                                       &qx[ci], &qy[ci], &oow, &ooz);
                    }
                    place_at(&models[0], slope, 0.0f, 0.0f, 100.0f);
                    place_at(&models[1], mesh, 0.0f, 0.0f, 250.0f);
                    place_at(&models[2], mesh, -125.0f, 125.0f, 250.0f);
                    scene.nmodels = 3;
                    scene.nsprites = 0;
                    n = scene_visible_models(&scene, &cam, order, 4);
                    check_expect(n == 2 && order[0] == 0 && order[1] == 2,
                                 "model past the slanted edge was dropped");
                    /* Every covered cell has to sit on the trapezoid. */
                    scene_set_cover_draw(1);
                    ok = scene_cover_count() > 0;
                    for (ci = 0; ci < scene_cover_count(); ci++) {
                        float x0, y0, x1, y1;

                        scene_cover_at(ci, &x0, &y0, &x1, &y1);
                        if (!quad_inside(x0, y0, qx, qy) ||
                            !quad_inside(x1, y0, qx, qy) ||
                            !quad_inside(x0, y1, qx, qy) ||
                            !quad_inside(x1, y1, qx, qy)) {
                            ok = 0;
                        }
                    }
                    scene_set_cover_draw(0);
                    check_expect(ok, "slope cover left the trapezoid");
                }
            }
        }
    }
}

/*
 * Occlusion. Each case is a shape the old wall rectangles got wrong:
 * the model that has a pixel on the glass has to stay.
 */
static void check_occlusion(void)
{
    Mesh *dot;
    Mesh *tall;
    Instance models[4];
    Scene scene;
    Camera cam;
    int order[4];
    int n;

    dot = tri_mesh(-1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f);
    /* A post from y -60 to 200. Its center is low, its top is high. */
    tall = tri_mesh(-2.0f, -60.0f, 0.0f, 2.0f, -60.0f, 0.0f, 0.0f, 200.0f, 0.0f);
    check_expect(dot && tall, "occlusion meshes were not allocated");
    if (!dot || !tall) {
        return;
    }
    mem_zero(&scene, sizeof(scene));
    scene.models = models;
    scene_set_limits(DF_FAR, 1);
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    /*
     * A wall down the right side of the street, x = 60, from z 10 to
     * 400. A post on the street at z 150 is in front of the far end of
     * the wall, though it is farther than the wall's nearest point. The
     * old test hid it. A post past the wall at z 200 goes.
     */
    {
        static const float wx[4] = {60.0f, 60.0f, 60.0f, 60.0f};
        static const float wy[4] = {-50.0f, -50.0f, 50.0f, 50.0f};
        static const float wz[4] = {10.0f, 400.0f, 400.0f, 10.0f};
        Mesh *side;

        side = fan_mesh(wx, wy, wz, 4, -1.0f, 0.0f, 0.0f);
        check_expect(side != 0, "side wall was not allocated");
        if (side) {
            place_at(&models[0], side, 0.0f, 0.0f, 0.0f);
            place_at(&models[1], dot, 50.0f, 0.0f, 150.0f);
            place_at(&models[2], dot, 120.0f, 0.0f, 200.0f);
            scene.nmodels = 3;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2 && order[0] == 0 && order[1] == 1,
                         "post in front of a wall seen at an angle was wrong");
        }
    }

    /*
     * Two panels of one facade, x -80..0 and 0..80, y -40..40, at z 100.
     * The post behind them has its center inside the facade and its top
     * above it. The old joined rectangle tested the center and hid it.
     * A small model behind the left panel goes.
     */
    {
        static const float lx[4] = {-80.0f, 0.0f, 0.0f, -80.0f};
        static const float rx[4] = {0.0f, 80.0f, 80.0f, 0.0f};
        static const float py[4] = {-40.0f, -40.0f, 40.0f, 40.0f};
        static const float pz[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        Mesh *left;
        Mesh *right;

        left = fan_mesh(lx, py, pz, 4, 0.0f, 0.0f, -1.0f);
        right = fan_mesh(rx, py, pz, 4, 0.0f, 0.0f, -1.0f);
        check_expect(left && right, "facade was not allocated");
        if (left && right) {
            place_at(&models[0], left, 0.0f, 0.0f, 100.0f);
            place_at(&models[1], right, 0.0f, 0.0f, 100.0f);
            place_at(&models[2], tall, 10.0f, 0.0f, 300.0f);
            place_at(&models[3], dot, -40.0f, 0.0f, 300.0f);
            scene.nmodels = 4;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 3 && order[2] == 2,
                         "post above the facade was dropped or the panel hid nothing");
            /*
             * The seam between the panels is x = 0, the edge of two
             * cells, so neither panel covers those cells alone. Cut
             * together they do, and a model straight behind the seam
             * goes.
             */
            place_at(&models[2], dot, 0.0f, 0.0f, 300.0f);
            scene.nmodels = 3;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2, "model behind the seam of two panels was kept");
        }
    }

    /*
     * The same panels a unit apart: four pixels of open glass, with
     * samples in it. The model behind the gap stays.
     */
    {
        static const float lx[4] = {-80.0f, -0.5f, -0.5f, -80.0f};
        static const float rx[4] = {0.5f, 80.0f, 80.0f, 0.5f};
        static const float py[4] = {-40.0f, -40.0f, 40.0f, 40.0f};
        static const float pz[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        Mesh *left;
        Mesh *right;

        left = fan_mesh(lx, py, pz, 4, 0.0f, 0.0f, -1.0f);
        right = fan_mesh(rx, py, pz, 4, 0.0f, 0.0f, -1.0f);
        check_expect(left && right, "split facade was not allocated");
        if (left && right) {
            place_at(&models[0], left, 0.0f, 0.0f, 100.0f);
            place_at(&models[1], right, 0.0f, 0.0f, 100.0f);
            place_at(&models[2], dot, 0.0f, 0.0f, 300.0f);
            scene.nmodels = 3;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 3, "model seen through a gap between panels was dropped");
        }
    }

    /*
     * A wall at z 100 standing on a floor at y -50 that ends at the
     * wall. Their edge is one line on the glass, and every cell along
     * it is part wall, part floor. A model behind that line goes.
     */
    {
        static const float wx[4] = {-100.0f, 100.0f, 100.0f, -100.0f};
        static const float wy[4] = {-50.0f, -50.0f, 50.0f, 50.0f};
        static const float wz[4] = {100.0f, 100.0f, 100.0f, 100.0f};
        static const float fx[4] = {-100.0f, -100.0f, 100.0f, 100.0f};
        static const float fy[4] = {-50.0f, -50.0f, -50.0f, -50.0f};
        static const float fz[4] = {0.0f, 100.0f, 100.0f, 0.0f};
        Mesh *wall;
        Mesh *floor;

        wall = fan_mesh(wx, wy, wz, 4, 0.0f, 0.0f, -1.0f);
        floor = fan_mesh(fx, fy, fz, 4, 0.0f, 1.0f, 0.0f);
        check_expect(wall && floor, "wall and floor were not allocated");
        if (wall && floor) {
            place_at(&models[0], wall, 0.0f, 0.0f, 0.0f);
            place_at(&models[1], floor, 0.0f, 0.0f, 0.0f);
            /* y -150 at z 300 is on the line y -50 at z 100 from the eye. */
            place_at(&models[2], dot, 0.0f, -150.0f, 300.0f);
            scene.nmodels = 3;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2, "model behind the edge of a wall and its floor was kept");
        }
    }

    /*
     * An L: the fan of a plane with a notch at the top right. Its
     * outline is not convex, and a cell filled from it could sit in
     * the notch, so the plane is not a cover at all. A model seen
     * through the notch stays.
     */
    {
        static const float lx[6] = {-40.0f, 40.0f, 40.0f, 0.0f, 0.0f, -40.0f};
        static const float ly[6] = {-40.0f, -40.0f, 0.0f, 0.0f, 40.0f, 40.0f};
        static const float lz[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        Mesh *ell;

        ell = fan_mesh(lx, ly, lz, 6, 0.0f, 0.0f, -1.0f);
        check_expect(ell != 0, "notched plane was not allocated");
        if (ell) {
            place_at(&models[0], ell, 0.0f, 0.0f, 100.0f);
            place_at(&models[1], dot, 20.0f, 20.0f, 300.0f);
            scene.nmodels = 2;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2, "model seen through the notch was dropped");
        }
    }

    /* A plane with no texture is never drawn, so it hides nothing. */
    {
        Mesh *bare;

        bare = wall_mesh(40.0f);
        check_expect(bare != 0, "bare wall was not allocated");
        if (bare) {
            bare->id = 1;
            bare->tris[0].slot = 0xffff;
            bare->tris[1].slot = 0xffff;
            place_at(&models[0], bare, 0.0f, 0.0f, 100.0f);
            place_at(&models[1], dot, 0.0f, 0.0f, 300.0f);
            scene.nmodels = 2;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2, "untextured wall hid a model");
        }
    }

    /*
     * Looking down at a floor, y -50. A model under it goes, a model
     * standing on it stays. The old test skipped floors entirely.
     */
    {
        static const float fx[4] = {-400.0f, 400.0f, 400.0f, -400.0f};
        static const float fy[4] = {-50.0f, -50.0f, -50.0f, -50.0f};
        static const float fz[4] = {0.0f, 0.0f, 600.0f, 600.0f};
        Mesh *floor;

        floor = fan_mesh(fx, fy, fz, 4, 0.0f, 1.0f, 0.0f);
        check_expect(floor != 0, "floor was not allocated");
        if (floor) {
            camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, -0.8f);
            place_at(&models[0], floor, 0.0f, 0.0f, 0.0f);
            place_at(&models[1], dot, 0.0f, -150.0f, 150.0f);
            place_at(&models[2], dot, 0.0f, -40.0f, 45.0f);
            scene.nmodels = 3;
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 2 && order[0] == 0 && order[1] == 2,
                         "floor did not hide the model under it");
            /* Occlusion off keeps the frustum and lists all three. */
            scene_set_occlude(0);
            n = scene_visible_models(&scene, &cam, order, 4);
            check_expect(n == 3, "occlude off still hid the model under the floor");
            scene_set_occlude(1);
            camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

static void check_loader(void)
{
    int flag;

    flag = 0;
    check_expect(loader_register("probe", probe_load), "probe loader was not registered");
    check_expect(loader_call("probe", "x", 0, 0, &flag) && flag == 1 && probe_hits == 1,
                 "probe loader did not run");
    check_expect(!loader_call("missing", 0, 0, 0, 0), "unknown loader returned success");
}

static void check_vfs(void)
{
    char root[64];

    vfs_mount("engine-mount");
    vfs_root(root, (int)sizeof(root));
    check_expect(root[0] == 'e' && root[6] == '-' && root[12] == 0,
                 "vfs mount did not keep the root string");
}

static int same(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] && a[i] == b[i]; i++) {
    }
    return a[i] == b[i];
}

static void check_format(void)
{
    char buf[64];

    log_format(buf, (int)sizeof(buf), "%d %u %x %s %c %%", -42, 7u, 0xbeefu, "ok", 'z');
    check_expect(same(buf, "-42 7 beef ok z %"), "log_format plain conversions");
    log_format(buf, (int)sizeof(buf), "[%5d][%-5d][%05d][%3s][%-3s]", 12, 12, -12, "a", "b");
    check_expect(same(buf, "[   12][12   ][-0012][  a][b  ]"), "log_format width and flags");
    log_format(buf, (int)sizeof(buf), "%.2f %.1f %.0f %f %.2f", 3.14159, -0.06, 2.5, 1.0, -0.001);
    check_expect(same(buf, "3.14 -0.1 3 1.00 0.00"), "log_format fixed point");
    log_format(buf, (int)sizeof(buf), "%.3s|%d", "abcdef", -2147483647 - 1);
    check_expect(same(buf, "abc|-2147483648"), "log_format precision and INT_MIN");
    check_expect(log_format(buf, 6, "%s", "longer than six") == 5 && same(buf, "longe"),
                 "log_format ran past its cap");
}

static int g_var;
static int g_hits;
static char g_last_arg[32];

static int get_var(void) { return g_var; }
static void set_var(int v) { g_var = v; }

static void probe_cmd(int argc, char **argv)
{
    g_hits++;
    g_last_arg[0] = 0;
    if (argc > 1) {
        int i;

        for (i = 0; argv[1][i] && i < 31; i++) {
            g_last_arg[i] = argv[1][i];
        }
        g_last_arg[i] = 0;
    }
}

static void type(const char *s)
{
    while (*s) {
        cmd_line_char(*s++);
    }
}

static void check_console(void)
{
    int i;

    check_format();

    /* Levels: a line past the level is not kept, a reply always is. */
    log_clear();
    log_set_level(LOG_WARN);
    log_print(LOG_INFO, "hidden %d", 1);
    log_print(LOG_WARN, "shown %d", 2);
    log_text(LOG_PRINT, "reply");
    check_expect(log_count() == 2, "log kept a line above its level");
    check_expect(same(log_get(0), "reply") && same(log_get(1), "warning: shown 2"),
                 "log lines or prefix wrong");
    check_expect(log_get_level(1) == LOG_WARN, "log lost a line's level");
    log_set_level(LOG_INFO);
    for (i = 0; i < LOG_LINES + 5; i++) {
        log_print(LOG_INFO, "line %d", i);
    }
    check_expect(log_count() == LOG_LINES, "log ring did not stop at LOG_LINES");
    check_expect(same(log_get(0), "line 260") && same(log_get(LOG_LINES - 1), "line 5"),
                 "log ring dropped the wrong end");
    log_text(LOG_PRINT, "tab\there");
    check_expect(same(log_get(0), "tab here"), "log kept a tab");

    /* Variables clamp, switches print on and off, and a toggle flips. */
    check_expect(cmd_var("probe_n", get_var, set_var, 10, 20, "test"), "cmd_var failed");
    check_expect(!cmd_var("probe_n", get_var, set_var, 10, 20, "test"), "cmd_var took a name twice");
    check_expect(cmd_add("probe", probe_cmd, "word", "test"), "cmd_add failed");
    log_clear();
    cmd_run("probe_n 99");
    check_expect(g_var == 20 && same(log_get(0), "probe_n 20"), "variable did not clamp");
    cmd_run("PROBE_N -5");
    check_expect(g_var == 10, "a command name was case sensitive");
    cmd_run("probe_n x");
    check_expect(g_var == 10 && same(log_get(0), "usage: probe_n 10..20"), "bad number was taken");
    check_expect(cmd_run("probe a; probe \"two words\" // probe c\n# probe d\nprobe e") == 3,
                 "cmd_run did not split commands and comments");
    check_expect(g_hits == 3 && same(g_last_arg, "e"), "cmd_run ran the wrong commands");
    cmd_run("probe \"two words\"");
    check_expect(same(g_last_arg, "two words"), "quotes did not keep one argument");
    /* A toggling switch flips on its bare name; a plain one only says what it is. */
    check_expect(cmd_var("zflip", get_var, set_var, 0, 1, "test") && cmd_toggles("zflip")
                 && !cmd_toggles("probe_n"), "cmd_toggles took a number, or refused a switch");
    g_var = 0;
    cmd_run("zflip");
    check_expect(g_var == 1 && same(log_get(0), "zflip on"), "a bare toggling switch did not flip on");
    cmd_run("zflip");
    check_expect(g_var == 0 && same(log_get(0), "zflip off"), "a bare toggling switch did not flip off");
    cmd_run("zflip on");
    cmd_run("zflip 1");
    check_expect(g_var == 1, "a toggling switch did not take a value");
    g_var = 15;
    cmd_run("probe_n");
    check_expect(g_var == 15, "a bare plain variable changed");
    cmd_run("nosuch");
    check_expect(same(log_get(0), "unknown command nosuch, help lists them"), "unknown command reply");

    /* The typed line: editing, Enter, history walk, and the saved draft. */
    type("probe one");
    cmd_line_key(CMD_KEY_HOME);
    cmd_line_key(CMD_KEY_DELETE);
    cmd_line_char('P');
    cmd_line_key(CMD_KEY_END);
    cmd_line_key(CMD_KEY_BACK);
    cmd_line_char('E');
    check_expect(same(cmd_line_text(), "Probe onE") && cmd_line_cursor() == 9, "line editing");
    cmd_line_key(CMD_KEY_ENTER);
    check_expect(same(cmd_line_text(), "") && same(g_last_arg, "onE"), "Enter did not run the line");
    type("probe two");
    cmd_line_key(CMD_KEY_ENTER);
    type("probe two");
    cmd_line_key(CMD_KEY_ENTER);
    check_expect(cmd_history_count() == 2 && same(cmd_history(0), "probe two")
                 && same(cmd_history(1), "Probe onE"), "history kept a repeat or lost a line");
    type("draft");
    cmd_line_key(CMD_KEY_UP);
    check_expect(same(cmd_line_text(), "probe two"), "Up did not recall the newest");
    cmd_line_key(CMD_KEY_UP);
    cmd_line_key(CMD_KEY_UP);
    check_expect(same(cmd_line_text(), "Probe onE"), "Up went past the oldest");
    cmd_line_key(CMD_KEY_DOWN);
    cmd_line_key(CMD_KEY_DOWN);
    check_expect(same(cmd_line_text(), "draft"), "Down did not give back the draft");
    cmd_line_key(CMD_KEY_KILL);

    /* Completion: one match fills in, several stop at their common start. */
    type("probe_");
    cmd_line_key(CMD_KEY_TAB);
    check_expect(same(cmd_line_text(), "probe_n "), "Tab did not complete a single match");
    cmd_line_key(CMD_KEY_KILL);
    type("pro");
    cmd_line_key(CMD_KEY_TAB);
    check_expect(same(cmd_line_text(), "probe"), "Tab did not stop at the common start");
    cmd_line_key(CMD_KEY_KILL);
    type("help probe_");
    cmd_line_key(CMD_KEY_TAB);
    check_expect(same(cmd_line_text(), "help probe_n "), "Tab did not complete after help");
    cmd_line_key(CMD_KEY_KILL);
    type("probe probe_");
    cmd_line_key(CMD_KEY_TAB);
    check_expect(same(cmd_line_text(), "probe probe_"), "Tab completed a plain argument");
    cmd_line_key(CMD_KEY_KILL);
    log_clear();
}

/* Bytes of one level-0 upload, no chain, 8-bit: the side squared. */
static df_u32 square_bytes(int side, int cutout)
{
    return (df_u32)side * (df_u32)side * (cutout ? 2u : 1u);
}

static df_u8 g_mip_out[MIP_MAX_BYTES];
static df_u8 g_mip_ref[MIP_MAX_BYTES];

/*
 * A cutout's full level the plain way, every texel's 3 x 3 gathered,
 * wrapping: what mip_build's span-limited bleed (only within each row's
 * opaque span and a texel either side) must write, bit for bit.
 */
static void mip_reference(const df_u8 *pic, int side, const df_u8 *pr, const df_u8 *pg,
                          const df_u8 *pb, df_u8 *out)
{
    int m;
    int x;
    int y;

    m = side - 1;
    for (y = 0; y < side; y++) {
        for (x = 0; x < side; x++) {
            df_u16 pix;
            df_u8 c;

            c = pic[y * side + x];
            if (c) {
                pix = (df_u16)(0x8000u | ((pr[c] >> 3) << 10) | ((pg[c] >> 3) << 5) | (pb[c] >> 3));
            } else {
                int r, g, b, n, dx, dy;

                r = g = b = n = 0;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        df_u8 k;

                        k = pic[((y + dy) & m) * side + ((x + dx) & m)];
                        if (k) {
                            r += pr[k];
                            g += pg[k];
                            b += pb[k];
                            n++;
                        }
                    }
                }
                pix = n ? (df_u16)((((r / n) >> 3) << 10) | (((g / n) >> 3) << 5) | ((b / n) >> 3)) : 0;
            }
            out[(y * side + x) * 2] = (df_u8)(pix & 0xff);
            out[(y * side + x) * 2 + 1] = (df_u8)(pix >> 8);
        }
    }
}

/*
 * Sprites made up, of every side, small and wide, in the corner (their
 * bleed wraps round the edges) and in the middle, with gaps: the full
 * level alone (the card's sharpen) and the first of a chain are the
 * reference's.
 */
static void check_mip_spans(void)
{
    static df_u8 pic[256 * 256];
    df_u8 pr[256];
    df_u8 pg[256];
    df_u8 pb[256];
    df_u32 seed;
    int side;
    int k;
    int ok;

    seed = 12345u;
    for (k = 0; k < 256; k++) {
        pr[k] = (df_u8)(k * 7);
        pg[k] = (df_u8)(k * 13);
        pb[k] = (df_u8)(255 - k);
    }
    ok = 1;
    for (side = 1; side <= 256; side <<= 1) {
        for (k = 0; k < 6; k++) {
            int x0, y0, w, h, x, y, i;

            for (i = 0; i < side * side; i++) {
                pic[i] = 0;
            }
            seed = seed * 1664525u + 1013904223u;
            w = 1 + (int)((seed >> 8) % (df_u32)side);
            h = 1 + (int)((seed >> 16) % (df_u32)side);
            x0 = (k & 1) ? 0 : (int)((seed >> 4) % (df_u32)side);
            y0 = (k & 2) ? 0 : (int)((seed >> 12) % (df_u32)side);
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    seed = seed * 1664525u + 1013904223u;
                    /* A third clear inside too, as a sprite's outline is. */
                    pic[((y0 + y) % side) * side + (x0 + x) % side] = (df_u8)((seed >> 24) % 3u ? (seed >> 16) & 255u : 0u);
                }
            }
            mip_reference(pic, side, pr, pg, pb, g_mip_ref);
            ok = ok && mip_build(pic, side, 1, 0, 0, pr, pg, pb, g_mip_out) == (df_u32)side * (df_u32)side * 2u;
            for (i = 0; ok && i < side * side * 2; i++) {
                ok = g_mip_out[i] == g_mip_ref[i];
            }
            ok = ok && mip_build(pic, side, 1, 0, 1, pr, pg, pb, g_mip_out) > 0;
            for (i = 0; ok && i < side * side * 2; i++) {
                ok = g_mip_out[i] == g_mip_ref[i];
            }
        }
    }
    check_expect(ok, "a cutout's full level, alone or atop its chain, is not the plain 3 x 3 bleed");
}

static void check_mip(void)
{
    df_u8 pr[256];
    df_u8 pg[256];
    df_u8 pb[256];
    df_u8 pic[16];
    int side[3];
    df_u8 cut[3];
    int drop[3];
    df_u32 n;
    int i;

    check_expect(mip_texels(256, 0, 1) == 87381u && mip_texels(256, 1, 0) == 16384u
                 && mip_texels(4, 0, 1) == 21u && mip_texels(3, 0, 1) == 0u,
                 "mip_texels counted the levels wrong");
    check_expect(MIP_MAX_BYTES >= 2u * mip_texels(256, 0, 1), "MIP_MAX_BYTES is short");
    for (i = 0; i < 256; i++) {
        pr[i] = 255;
        pg[i] = 0;
        pb[i] = (df_u8)i;
    }
    pr[1] = 0;   pg[1] = 0;   pb[1] = 0;
    pr[2] = 255; pg[2] = 255; pb[2] = 255;
    pr[3] = 128; pg[3] = 128; pb[3] = 128;
    pr[4] = 200; pg[4] = 0;   pb[4] = 0;
    /* A 4 x 4 checker of black and white texels. */
    for (i = 0; i < 16; i++) {
        pic[i] = (df_u8)((((i & 3) + (i >> 2)) & 1) ? 2 : 1);
    }
    n = mip_build(pic, 4, 0, 0, 1, pr, pg, pb, g_mip_out);
    check_expect(n == 21u, "8-bit chain was not 16 + 4 + 1 bytes");
    for (i = 0; i < 16 && g_mip_out[i] == pic[i]; i++) {
    }
    check_expect(i == 16, "8-bit chain changed the full level");
    check_expect(g_mip_out[16] == 3 && g_mip_out[19] == 3 && g_mip_out[20] == 3,
                 "a black and white texel mean did not map to the grey entry");
    n = mip_build(pic, 4, 0, 1, 0, pr, pg, pb, g_mip_out);
    check_expect(n == 4u && g_mip_out[0] == 3, "drop 1 without a chain was not one 2 x 2 level");
    /* A cutout: one red texel in a transparent 4 x 4. */
    for (i = 0; i < 16; i++) {
        pic[i] = 0;
    }
    pic[5] = 4;
    n = mip_build(pic, 4, 1, 0, 1, pr, pg, pb, g_mip_out);
    check_expect(n == 42u, "cutout chain was not (16 + 4 + 1) x 2 bytes");
    check_expect(g_mip_out[10] == 0x00 && g_mip_out[11] == (0x8000u | (25u << 10)) >> 8,
                 "the opaque cutout texel lost its colour or alpha");
    check_expect(g_mip_out[0] == 0x00 && g_mip_out[1] == (25u << 10) >> 8,
                 "a transparent neighbour did not take the red, or kept alpha");
    check_expect(g_mip_out[30] == 0x00 && g_mip_out[31] == 0x00,
                 "a transparent texel two away from the sprite was not black");
    check_expect((g_mip_out[33] & 0x80) == 0, "a quarter-covered texel came out opaque");
    /* The budget: the largest picture loses its top level first. */
    side[0] = 256;
    side[1] = 256;
    side[2] = 64;
    cut[0] = 0;
    cut[1] = 0;
    cut[2] = 0;
    n = mip_budget(side, cut, 3, 65536u + 65536u + 4096u, square_bytes, drop);
    check_expect(n == 65536u + 65536u + 4096u && drop[0] == 0 && drop[1] == 0 && drop[2] == 0,
                 "budget shrank pictures that fit");
    n = mip_budget(side, cut, 3, 65536u + 65536u + 4095u, square_bytes, drop);
    check_expect(n == 16384u + 65536u + 4096u && drop[0] == 1 && drop[1] == 0 && drop[2] == 0,
                 "budget did not drop the first largest picture once");
    /*
     * A sprite last: a 256 wall and a 256 sprite (twice its bytes) a
     * little short of room. The wall gives first, though the sprite is
     * larger; only past MIP_WALL_GRACE levels does the sprite give one.
     */
    side[0] = 256;
    side[1] = 256;
    side[2] = 0;
    cut[0] = 0;
    cut[1] = 1;
    n = mip_budget(side, cut, 3, 65536u + 131072u - 1u, square_bytes, drop);
    check_expect(drop[0] == 1 && drop[1] == 0 && n == 16384u + 131072u,
                 "the budget took the sprite's level before the wall's");
    n = mip_budget(side, cut, 3, 4096u + 131072u - 1u, square_bytes, drop);
    check_expect(drop[0] == MIP_WALL_GRACE && drop[1] == 1 && n == 4096u + 32768u,
                 "the wall did not give its grace before the sprite gave one level");
    side[1] = 256;
    side[2] = 64;
    cut[1] = 0;
    cut[2] = 1;
    side[1] = 0;
    n = mip_budget(side, cut, 3, 100u, square_bytes, drop);
    check_expect(drop[0] == 5 && drop[1] == 0 && drop[2] == 3 && n == 64u + 128u,
                 "budget went below MIP_MIN_SIDE or counted a missing picture");
    /*
     * Glass: Daggerfall's palette has black at 0 and at MIP_GLASS. A
     * window's glass must stay MIP_GLASS down the chain, where at least
     * half a texel is glass, and filtering must never make it elsewhere.
     */
    {
        static df_u8 chain[MIP_MAX_BYTES];
        df_u8 win[16];
        int k;

        for (i = 0; i < 256; i++) {
            pr[i] = (df_u8)i;
            pg[i] = (df_u8)i;
            pb[i] = (df_u8)i;
        }
        pr[0] = pg[0] = pb[0] = 0;
        pr[MIP_GLASS] = pg[MIP_GLASS] = pb[MIP_GLASS] = 0;
        /* Rows of 4: the left half glass, the right half grey 200, and one glass texel there. */
        for (k = 0; k < 16; k++) {
            win[k] = (df_u8)((k % 4) < 2 ? MIP_GLASS : 200);
        }
        win[3] = MIP_GLASS;
        n = mip_build(win, 4, 0, 0, 1, pr, pg, pb, chain);
        check_expect(n == 21u && chain[16] == MIP_GLASS && chain[18] == MIP_GLASS
                     && chain[17] != MIP_GLASS && chain[19] != MIP_GLASS,
                     "a window's glass did not stay glass at half size, or a quarter became glass");
        check_expect(chain[20] == MIP_GLASS || chain[20] == 0, "the last level is not dark");
    }
}

/*
 * A 400-unit wall at z = 100 seen from the origin, a light 40 units in
 * front of it with radius 50 (reach 150 at the default x3): one decal,
 * in the wall's plane, cut to the square around the 145-unit circle,
 * seen from the camera's side. The same light behind the wall lights
 * nothing. The falloff picture is 0 at its corner and warm at its
 * centre. A sprite at the light is brighter than the ambient; a flat
 * that gives light is white; lights off is white.
 */
/* A light does not reach a face past another model (lightshadow). */
static void check_light_shadow(void)
{
    static const float wx[4] = { -200.0f, 200.0f, 200.0f, -200.0f };
    static const float wy[4] = { -200.0f, -200.0f, 200.0f, 200.0f };
    static const float wz[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Mesh *sheet;
    Instance models[2];
    Light light;
    Scene scene;

    sheet = fan_mesh(wx, wy, wz, 4, 0.0f, 0.0f, -1.0f);
    check_expect(sheet != 0, "shadow meshes were not allocated");
    if (!sheet) {
        return;
    }
    /* The lit wall at z 100, the light at z 60, and a wall between at z 80. */
    place_at(&models[0], sheet, 0.0f, 0.0f, 100.0f);
    place_at(&models[1], sheet, 0.0f, 0.0f, 80.0f);
    mem_zero(&light, sizeof(light));
    light.z = 60.0f;
    light.radius = 50.0f;
    mem_zero(&scene, sizeof(scene));
    scene.models = models;
    scene.nmodels = 1;
    scene.lights = &light;
    scene.nlights = 1;
    light_set_on(1);
    light_set_range(3);
    light_set_shadow(1);
    check_expect(light_build(&scene) == 1, "a wall in the open did not get its decal");
    scene.nmodels = 2;
    check_expect(light_build(&scene) == 1, "a wall in front of the light did not get its own decal");
    {
        const Decal *d;
        int n;
        int k;
        int far_lit;

        n = light_decals(0, &d);
        far_lit = 0;
        for (k = 0; k < n; k++) {
            far_lit += d[k].cz > 90.0f;
        }
        check_expect(n == 1 && !far_lit, "a light reached a wall through the wall in front of it");
    }
    light_set_shadow(0);
    check_expect(light_build(&scene) == 2, "with lightshadow off the light does not reach through");
    light_set_shadow(1);
    light_build(0);
}

static void check_lights(void)
{
    static const float wx[4] = { -200.0f, 200.0f, 200.0f, -200.0f };
    static const float wy[4] = { -200.0f, -200.0f, 200.0f, 200.0f };
    static const float wz[4] = { 100.0f, 100.0f, 100.0f, 100.0f };
    static df_u16 tex[LIGHT_TEX * LIGHT_TEX];
    Instance model;
    Flat sprites[2];
    Light light;
    Scene scene;
    Mesh *wall;
    const Decal *d;
    const float *c;
    df_u32 amb;
    df_u32 lit;
    int n;
    int k;
    int ok;

    wall = fan_mesh(wx, wy, wz, 4, 0.0f, 0.0f, -1.0f);
    check_expect(wall != 0, "light wall was not allocated");
    if (!wall) {
        return;
    }
    place_at(&model, wall, 0.0f, 0.0f, 0.0f);
    mem_zero(sprites, sizeof(sprites));
    sprites[0].z = 60.0f;
    sprites[0].w = 16.0f;
    sprites[0].h = 16.0f;
    sprites[1] = sprites[0];
    sprites[1].glow = 1;
    mem_zero(&light, sizeof(light));
    light.z = 60.0f;
    light.radius = 50.0f;
    mem_zero(&scene, sizeof(scene));
    scene.models = &model;
    scene.nmodels = 1;
    scene.sprites = sprites;
    scene.nsprites = 2;
    scene.lights = &light;
    scene.nlights = 1;
    light_set_on(1);
    light_set_range(3);
    check_expect(light_build(&scene) == 1 && light_count() == 1, "the wall did not get one decal");
    {
        const float *kept;
        df_u32 epoch;

        /* A rebuild of the same place reuses its buffers... */
        kept = light_corners();
        light_build(&scene);
        check_expect(light_corners() == kept, "a rebuild of the same place did not reuse its decals");
        /* ...but not after a temp heap ended: they died with it (mem_epoch). */
        epoch = mem_epoch();
        mem_temp_begin();
        mem_temp_end();
        check_expect(mem_epoch() != epoch, "a temp heap did not move the heap epoch");
        light_build(&scene);
        check_expect(light_corners() != kept && light_decal_total() == 1,
                     "the next place's decals were written into the last place's buffers");
    }
    n = light_decals(0, &d);
    ok = n == 1 && d[0].n == 4 && d[0].nz < -0.99f && d[0].d > 0.0f;
    c = light_corners();
    for (k = 0; ok && k < d[0].n; k++) {
        const float *q;

        q = &c[(d[0].first + k) * LIGHT_CORNER];
        ok = check_near(q[2], 100.0f, 0.01f) && q[0] > -146.0f && q[0] < 146.0f
            && q[3] >= -0.001f && q[3] <= 1.001f && q[4] >= -0.001f && q[4] <= 1.001f;
    }
    check_expect(ok, "the decal is not the wall's square around the light");
    /* A steady light is full; a flame wavers between its low and full, and moves. */
    {
        int lo;
        int hi;
        int k;

        check_expect(light_flicker(0, 1.234f) == 255, "a steady light flickers");
        light.flicker = 1;
        lo = 255;
        hi = 0;
        for (k = 0; k < 200; k++) {
            int v;

            v = light_flicker(0, (float)k * 0.037f);
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        check_expect(lo >= LIGHT_FLICKER_LOW && hi <= 255 && hi - lo > 30,
                     "a flame does not waver within its range");
        light.flicker = 0;
    }
    amb = light_ambient_rgb();
    lit = light_sprite_rgb(0);
    check_expect(((lit >> 16) & 255) > ((amb >> 16) & 255) && ((lit >> 16) & 255) >= ((lit >> 8) & 255)
                 && light_sprite_rgb(1) == 0xffffffu,
                 "sprite light is not ambient raised by a warm light, or a glowing flat is not white");
    light_set_on(0);
    check_expect(light_sprite_rgb(0) == 0xffffffu && light_ambient_rgb() == 0xffffffu,
                 "lights off did not draw full bright");
    light_set_on(1);
    light.z = 140.0f;
    check_expect(light_build(&scene) == 0, "a light behind the wall lit its front");
    light_texture(0, tex);
    check_expect(tex[0] == 0 && tex[LIGHT_TEX * LIGHT_TEX - 1] == 0
                 && (tex[(LIGHT_TEX / 2) * LIGHT_TEX + LIGHT_TEX / 2] >> 11) == 31,
                 "the falloff picture is not 0 at its corner and full red at its centre");
    light_build(0);
}

static int red_of(df_u32 c)
{
    return (int)((c >> 16) & 255u);
}

static int blue_of(df_u32 c)
{
    return (int)(c & 255u);
}

/* The world clock and the light by the hour (docs/daylight.md). */
static void check_daylight(void)
{
    Daylight dl;
    df_u32 table[DAYLIGHT_STEPS + 1];
    int h;
    int m;
    int i;
    int ok;

    clock_set_scale(60);
    /* The calendar: the 1st of Midyear, 3E 405, is summer and a Sundas. */
    {
        int y, mo, d, day;

        clock_set_date(405, 5, 0);
        clock_date(&y, &mo, &d);
        check_expect(y == 405 && mo == 5 && d == 0 && clock_season() == CLOCK_SUMMER
                     && df_eq(clock_month_name(mo), "Midyear") && df_eq(clock_weekday_name(d), "Sundas"),
                     "the 1st of Midyear 405 is not a summer Sundas");
        day = clock_day();
        clock_set_minute(1439.0f);
        clock_advance(1.0f);
        clock_advance(1.0f);
        check_expect(clock_day() == day + 1, "midnight did not turn the day");
        clock_set_minute(-30.0f);
        check_expect(clock_day() == day + 1, "setting the hour moved the date");
        clock_set_date(405, 11, 29);
        clock_set_minute(1439.5f);
        clock_advance(1.0f);
        clock_date(&y, &mo, &d);
        check_expect(y == 406 && mo == 0 && d == 0 && clock_season() == CLOCK_WINTER,
                     "the last day of Evening Star did not turn into a winter new year");
        clock_set_season(CLOCK_FALL);
        clock_date(&y, &mo, &d);
        check_expect(y == 406 && mo == 8 && d == 0 && clock_season() == CLOCK_FALL
                     && df_eq(clock_month_name(mo), "Hearthfire"),
                     "season fall is not the 1st of Hearthfire");
        clock_set_date(405, 5, 0);
    }
    clock_set_minute(1439.0f);
    clock_advance(2.0f);
    check_expect(check_near(clock_minute(), 1439.0f, 0.01f), "a two second frame moved the clock");
    clock_advance(0.75f);
    clock_advance(0.75f);
    check_expect(check_near(clock_minute(), 0.5f, 0.01f), "the clock did not run past midnight");
    clock_set_minute(-30.0f);
    clock_hm(&h, &m);
    check_expect(h == 23 && m == 30, "minus half an hour is not 23:30");
    clock_set_scale(0);
    clock_advance(0.5f);
    check_expect(check_near(clock_minute(), 1410.0f, 0.01f), "a stopped clock moved");
    clock_set_scale(CLOCK_SCALE);
    clock_set_minute(720.0f);

    daylight_at(720.0f, &dl);
    check_expect(dl.sun && dl.ky > 0.8f && dl.kz < -0.4f && dl.lamps == 0.0f,
                 "noon is not a high sun in the south with the lamps out");
    check_expect(dl.kr > 0.9f && dl.ar > 0.4f && dl.ar < 0.6f, "noon is not a white sun on a mid ambient");
    daylight_at(420.0f, &dl);
    check_expect(dl.sun && dl.kx > 0.9f && dl.ky > 0.0f && dl.kr > dl.kb * 1.5f,
                 "seven is not a low orange sun in the east");
    daylight_at(1020.0f, &dl);
    check_expect(dl.sun && dl.kx < -0.9f, "seventeen is not a sun in the west");
    daylight_at(0.0f, &dl);
    check_expect(!dl.sun && dl.ky > 0.7f && dl.lamps == 1.0f && dl.ab > dl.ar,
                 "midnight is not a high moon, lamps lit, a blue ambient");
    daylight_table(&dl, table);
    ok = 1;
    for (i = 1; i <= DAYLIGHT_STEPS; i++) {
        ok = ok && red_of(table[i]) >= red_of(table[i - 1]) && blue_of(table[i]) >= blue_of(table[i - 1]);
    }
    check_expect(ok && table[DAYLIGHT_STEPS] != table[0], "a face nearer the key is not brighter");
    daylight_at(420.0f, &dl);
    /* Looking west, the low eastern sun is behind the camera and on the flats' faces. */
    check_expect(red_of(daylight_sprite(&dl, -1.0f, 0.0f)) > red_of(daylight_sprite(&dl, 1.0f, 0.0f)),
                 "a flat facing the sun is not brighter than one with the sun behind it");
    /* The day and the night rows meet at midnight. */
    daylight_at(0.0f, &dl);
    {
        Daylight late;

        daylight_at(1439.9f, &late);
        check_expect(check_near(late.ar, dl.ar, 0.01f) && check_near(late.kb, dl.kb, 0.01f),
                     "the light jumps at midnight");
    }
}

/* A course for the walker: corners of quads, two triangles each. */
static Tri g_course[64];
static int g_ncourse;

static void course_quad(float x0, float y0, float z0, float x1, float y1, float z1,
                        float x2, float y2, float z2, float x3, float y3, float z3)
{
    Tri *t;
    int k;

    for (k = 0; k < 2 && g_ncourse < 64; k++) {
        t = &g_course[g_ncourse++];
        mem_zero(t, sizeof(*t));
        t->slot = 1;
        t->x[0] = x0;
        t->y[0] = y0;
        t->z[0] = z0;
        t->x[1] = k ? x2 : x1;
        t->y[1] = k ? y2 : y1;
        t->z[1] = k ? z2 : z1;
        t->x[2] = k ? x3 : x2;
        t->y[2] = k ? y3 : y2;
        t->z[2] = k ? z3 : z2;
    }
}

/* A box, its six sides. */
static void course_box(float x0, float y0, float z0, float x1, float y1, float z1)
{
    course_quad(x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1);
    course_quad(x0, y0, z0, x0, y0, z1, x1, y0, z1, x1, y0, z0);
    course_quad(x0, y0, z0, x0, y1, z0, x0, y1, z1, x0, y0, z1);
    course_quad(x1, y0, z0, x1, y0, z1, x1, y1, z1, x1, y1, z0);
    course_quad(x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0);
    course_quad(x0, y0, z1, x0, y1, z1, x1, y1, z1, x1, y0, z1);
}

static void walk(Camera *cam, const Scene *scene, float dx, float dz, int frames)
{
    int f;

    for (f = 0; f < frames; f++) {
        cam->x += dx;
        cam->z += dz;
        walker_step(cam, scene, 1.0f / 30.0f);
    }
}

/* Collision and the walker (docs/collision.md). */
static void check_collision(void)
{
    static Mesh mesh;
    Instance in;
    Scene scene;
    Camera cam;
    float eye;

    g_ncourse = 0;
    /* A floor 1000 square at y 0, a wall at x 200, a step 20 high and a block 48 high. */
    course_quad(-500.0f, 0.0f, -500.0f, -500.0f, 0.0f, 500.0f, 500.0f, 0.0f, 500.0f, 500.0f, 0.0f, -500.0f);
    course_quad(200.0f, 0.0f, -500.0f, 200.0f, 200.0f, -500.0f, 200.0f, 200.0f, 500.0f, 200.0f, 0.0f, 500.0f);
    course_box(-400.0f, 0.0f, -100.0f, -300.0f, 20.0f, 100.0f);
    course_box(-100.0f, 0.0f, 300.0f, 0.0f, 48.0f, 400.0f);
    mem_zero(&mesh, sizeof(mesh));
    mesh.tris = g_course;
    mesh.ntris = g_ncourse;
    mesh_measure(&mesh);
    mem_zero(&in, sizeof(in));
    in.mesh = &mesh;
    instance_set_rotation(&in, 0.0f, 0.0f, 0.0f);
    mem_zero(&scene, sizeof(scene));
    scene.models = &in;
    scene.nmodels = 1;
    eye = WALK_EYE;

    walker_set_on(1);
    camera_set(&cam, 0.0f, 300.0f, 0.0f, 0.0f, 0.0f);
    walk(&cam, &scene, 0.0f, 0.0f, 150);
    check_expect(check_near(cam.y, eye, 0.5f) && walker_grounded(),
                 "a body dropped on the floor does not stand on it at eye height");
    walk(&cam, &scene, 10.0f, 0.0f, 60);
    check_expect(cam.x < 200.0f - WALK_RADIUS + 0.5f && cam.x > 200.0f - WALK_RADIUS - 2.0f
                 && check_near(cam.y, eye, 0.5f),
                 "walking into the wall does not stop a radius short of it");
    /* Just under the teleport distance, in one frame: the sweep still meets the wall. */
    walk(&cam, &scene, -400.0f, 0.0f, 1);
    walk(&cam, &scene, 700.0f, 0.0f, 1);
    check_expect(cam.x < 200.0f - WALK_RADIUS + 0.5f, "700 units in one frame went through the wall");
    camera_set(&cam, -250.0f, eye, 0.0f, 0.0f, 0.0f);
    walk(&cam, &scene, 0.0f, 0.0f, 2);
    walk(&cam, &scene, -6.0f, 0.0f, 25);
    check_expect(check_near(cam.y, 20.0f + eye, 1.0f) && cam.x < -320.0f,
                 "the body did not walk up a 20-unit step");
    camera_set(&cam, -50.0f, eye, 200.0f, 0.0f, 0.0f);
    walk(&cam, &scene, 0.0f, 0.0f, 2);
    walk(&cam, &scene, 0.0f, 6.0f, 30);
    check_expect(check_near(cam.y, eye, 1.0f) && cam.z < 300.0f - WALK_RADIUS + 0.5f,
                 "the body walked up a 48-unit block");
    /*
     * A jump: up at 260 a second it rises about 86 (260^2 / (2 g), less
     * a little at 30 frames a second), clears the 48-unit block it moves
     * onto and lands there, the drop from its top reported once; from the
     * floor, a jump in place lands at the speed it left with, from its
     * height; off the block's top, the fall is 48.
     */
    {
        float speed;
        float height;
        float top;
        int f;

        camera_set(&cam, -50.0f, eye, 230.0f, 0.0f, 0.0f);
        walker_place();
        walk(&cam, &scene, 0.0f, 0.0f, 30);
        walker_landed(&speed, &height);
        check_expect(walker_jump(260.0f) && !walker_jump(260.0f), "a jump did not start, or started again in the air");
        top = 0.0f;
        for (f = 0; f < 60 && (f == 0 || !walker_grounded()); f++) {
            walk(&cam, &scene, 0.0f, 4.0f, 1);
            top = cam.y - eye > top ? cam.y - eye : top;
        }
        check_expect(walker_grounded() && check_near(cam.y, 48.0f + eye, 1.0f) && top > 78.0f && top < 90.0f,
                     "a jump of 260 a second did not clear the 48-unit block and land on it");
        check_expect(walker_landed(&speed, &height) && check_near(height, top - 48.0f, 2.0f)
                     && !walker_landed(&speed, &height),
                     "landing on the block was not reported once, as the drop from the arc's top");
        camera_set(&cam, -300.0f, eye, 200.0f, 0.0f, 0.0f);
        walker_place();
        walk(&cam, &scene, 0.0f, 0.0f, 30);
        walker_landed(&speed, &height);
        walker_jump(200.0f);
        for (f = 0; f < 60 && (f == 0 || !walker_grounded()); f++) {
            walk(&cam, &scene, 0.0f, 0.0f, 1);
        }
        check_expect(walker_landed(&speed, &height) && check_near(speed, 200.0f, 15.0f)
                     && check_near(height, 200.0f * 200.0f / (2.0f * WALK_GRAVITY), 4.0f),
                     "a jump in place did not land at its own speed from its own height");
        camera_set(&cam, -50.0f, 48.0f + eye, 350.0f, 0.0f, 0.0f);
        walker_place();
        walk(&cam, &scene, 0.0f, 0.0f, 30);
        walker_landed(&speed, &height);
        speed = 0.0f;
        height = 0.0f;
        for (f = 0; f < 40 && !walker_landed(&speed, &height); f++) {
            walk(&cam, &scene, 0.0f, -6.0f, 1);
        }
        check_expect(check_near(cam.y, eye, 1.0f) && check_near(height, 48.0f, 2.0f) && speed > 100.0f,
                     "walking off the block did not land as a 48-unit fall");
    }
    /*
     * Entities in the way (Entity.radius): walking at one of radius 12,
     * the body stops its own radius and the entity's short of it; one of
     * radius 0 is walked through. entity_blocked: a step into the body is
     * blocked, one away is not; two solid entities do not step into each
     * other, but two already overlapping may walk apart.
     */
    {
        static Entity ents[2];
        int ok;

        mem_zero(ents, sizeof(ents));
        ents[0].x = 0.0f;
        ents[0].z = -200.0f;
        ents[0].live = 1;
        ents[0].radius = 12.0f;
        ents[0].pose[ENTITY_WALK].slot[0] = 0;
        ents[0].pose[ENTITY_WALK].w[0] = 30;
        ents[0].pose[ENTITY_WALK].h[0] = 70;
        ents[0].pose[ENTITY_IDLE].slot[0] = 0xffff;
        ents[0].moving = 1;
        ents[1] = ents[0];
        ents[1].z = -320.0f;
        scene.entities = ents;
        scene.nentities = 2;
        camera_set(&cam, 0.0f, eye, -100.0f, 0.0f, 0.0f);
        walker_place();
        walk(&cam, &scene, 0.0f, 0.0f, 2);
        walk(&cam, &scene, 0.0f, -5.0f, 40);
        check_expect(cam.z > -200.0f + 12.0f + WALK_RADIUS - 1.5f && cam.z < -200.0f + 12.0f + WALK_RADIUS + 1.5f,
                     "the body did not stop at a solid entity, its radius and the entity's short of it");
        ok = entity_blocked(&scene, &ents[0], 0.0f, cam.z - 20.0f) && !entity_blocked(&scene, &ents[0], 0.0f, -260.0f)
            && entity_blocked(&scene, &ents[1], 0.0f, -215.0f) && !entity_blocked(&scene, &ents[1], 0.0f, -260.0f);
        ents[1].z = -210.0f;
        ok = ok && !entity_blocked(&scene, &ents[1], 0.0f, -215.0f);
        ents[1].z = -320.0f;
        check_expect(ok, "entity_blocked does not see the body, or another solid entity, or holds two that overlap");
        ents[0].radius = 0.0f;
        walk(&cam, &scene, 0.0f, -5.0f, 20);
        check_expect(cam.z < -230.0f, "an entity of radius 0 stopped the body");
        scene.entities = 0;
        scene.nentities = 0;
    }
    /*
     * Bodies the game owns (body.h): two, each moving its own entity,
     * stop at each other, their radii apart; a rat's body (8 round, 24
     * tall, steps of 8) does not climb the 20-unit step a person walks
     * up. Rigs (rig.h): third person behind a body with a wall behind it
     * comes in short of the wall; with open ground behind, it is the
     * whole distance back.
     */
    {
        static Entity ents[2];
        Body a;
        Body b;
        Body rat;
        Camera rc;
        int f;

        walker_set_on(0);
        mem_zero(ents, sizeof(ents));
        for (f = 0; f < 2; f++) {
            ents[f].live = 1;
            ents[f].radius = 16.0f;
            ents[f].pose[ENTITY_WALK].w[0] = 32;
            ents[f].pose[ENTITY_WALK].h[0] = 72;
            ents[f].pose[ENTITY_IDLE].slot[0] = 0xffff;
            ents[f].moving = 1;
        }
        scene.entities = ents;
        scene.nentities = 2;
        body_init(&a, 16.0f, 72.0f, 24.0f, 68.4f);
        body_init(&b, 16.0f, 72.0f, 24.0f, 68.4f);
        a.self = &ents[0];
        b.self = &ents[1];
        body_place(&a, 0.0f, 0.0f, 0.0f);
        body_place(&b, 0.0f, 0.0f, -150.0f);
        for (f = 0; f < 40; f++) {
            body_step(&a, &scene, 0.0f, -5.0f, 1.0f / 30.0f);
            body_step(&b, &scene, 0.0f, 5.0f, 1.0f / 30.0f);
            ents[0].x = a.feet[0];
            ents[0].y = a.feet[1];
            ents[0].z = a.feet[2];
            ents[1].x = b.feet[0];
            ents[1].y = b.feet[1];
            ents[1].z = b.feet[2];
        }
        check_expect(a.feet[2] - b.feet[2] > 32.0f - 2.0f && a.feet[2] - b.feet[2] < 32.0f + 6.0f
                     && a.grounded && b.grounded,
                     "two bodies walking at each other did not stop their radii apart");
        scene.entities = 0;
        scene.nentities = 0;
        body_init(&rat, 8.0f, 24.0f, 8.0f, 20.0f);
        body_place(&rat, -250.0f, 0.0f, 0.0f);
        for (f = 0; f < 40; f++) {
            body_step(&rat, &scene, -5.0f, 0.0f, 1.0f / 30.0f);
        }
        check_expect(rat.feet[0] > -300.0f + 1.0f && rat.feet[1] < 1.0f,
                     "a rat's body climbed a step a person's height of steps over its own");
        body_place(&a, 150.0f, 0.0f, 0.0f);
        body_step(&a, &scene, 0.0f, 0.0f, 1.0f / 30.0f);
        rig_third(&rc, &a, -DF_TAU * 0.25f, 0.0f, 150.0f, &scene);
        check_expect(rc.x < 200.0f - RIG_MARGIN + 0.5f && rc.x > 150.0f + RIG_NEAREST - 0.5f
                     && check_near(rc.y, 68.4f, 0.5f),
                     "a third-person camera with a wall behind the body went into the wall");
        rig_third(&rc, &a, DF_TAU * 0.25f, 0.0f, 150.0f, &scene);
        check_expect(check_near(rc.x, 0.0f, 0.5f) && check_near(rc.z, 0.0f, 0.5f),
                     "a third-person camera with open ground behind did not sit the whole distance back");
        rig_first(&rc, &a);
        check_expect(check_near(rc.x, 150.0f, 0.01f) && check_near(rc.y, 68.4f, 0.01f),
                     "a first-person camera is not at the body's eye");
        walker_set_on(1);
    }
    /* Off the floor's edge: it falls out, and comes back to where it last stood. */
    camera_set(&cam, -50.0f, eye, -450.0f, 0.0f, 0.0f);
    walk(&cam, &scene, 0.0f, 0.0f, 2);
    walk(&cam, &scene, 0.0f, -8.0f, 20);
    walk(&cam, &scene, 0.0f, 0.0f, 150);
    check_expect(cam.y > 0.0f && cam.z > -520.0f, "a body that fell out of the place was not put back");
    /* Set inside the block: it is moved out to a free place with a floor. */
    camera_set(&cam, -50.0f, 30.0f, 350.0f, 0.0f, 0.0f);
    walk(&cam, &scene, 0.0f, 0.0f, 3);
    {
        float body[3];

        /* The upper body's centre, from the feet. */
        body[0] = cam.x;
        body[1] = cam.y - WALK_EYE + WALK_STEP + WALK_UPPER;
        body[2] = cam.z;
        collide_gather(body[0] - 40.0f, body[1] - 80.0f, body[2] - 40.0f,
                       body[0] + 40.0f, body[1] + 80.0f, body[2] + 40.0f);
        check_expect(!collide_overlap(body) && (cam.y - WALK_EYE > 47.0f || cam.z < 300.0f - WALK_RADIUS + 1.0f
                                                || cam.z > 400.0f + WALK_RADIUS - 1.0f
                                                || cam.x < -100.0f - WALK_RADIUS + 1.0f
                                                || cam.x > WALK_RADIUS - 1.0f),
                     "a body set inside a block was not moved out");
    }
    /*
     * Through a door to another place: the camera lands 100 units on, the
     * other side of the wall at x 200. Under the teleport distance, so only
     * the new place tells the walker to take the body there, not walk it.
     */
    {
        Scene next;

        camera_set(&cam, 150.0f, eye, 0.0f, 0.0f, 0.0f);
        walk(&cam, &scene, 0.0f, 0.0f, 30);
        next = scene;
        cam.x = 250.0f;
        walk(&cam, &next, 0.0f, 0.0f, 1);
        check_expect(check_near(cam.x, 250.0f, 0.5f),
                     "a body set past a wall in a new place was walked into the wall");
        cam.x = 150.0f;
        walker_place();
        walk(&cam, &next, 0.0f, 0.0f, 1);
        check_expect(check_near(cam.x, 150.0f, 0.5f), "walker_place did not take the body to the camera");
    }
    walker_set_on(0);
}

/* The weather's rolls and blends (docs/weather.md). */
static void check_weather(void)
{
    static const df_u8 only_fog[WEATHER_KINDS] = { 0, 0, 0, 100, 0, 0, 0 };
    static const df_u8 desert[WEATHER_KINDS] = { 85, 15, 0, 0, 0, 0, 0 };
    Weather w;
    float before;

    weather_seed(1234u);
    weather_set(WEATHER_SUNNY);
    weather_state(&w);
    check_expect(w.kind == WEATHER_SUNNY && w.blend == 1.0f && w.fog >= WEATHER_CLEAR
                 && w.rain == 0.0f && w.sky == WEATHER_SKY_CLEAR,
                 "sunny is not clear");
    weather_force(WEATHER_RAIN);
    weather_advance(WEATHER_BLEND * 0.4f);
    weather_state(&w);
    check_expect(w.kind == WEATHER_RAIN && w.from == WEATHER_SUNNY && w.rain > 0.1f && w.rain < 0.9f
                 && w.sky == WEATHER_SKY_CLEAR && w.fog < WEATHER_CLEAR && w.fog > 2400.0f,
                 "rain does not blend in, or its sky came before half way");
    before = w.rain;
    weather_advance(WEATHER_BLEND * 0.2f);
    weather_state(&w);
    check_expect(w.rain > before && w.sky == WEATHER_SKY_RAIN, "the rain sky did not come at half way");
    weather_advance(WEATHER_BLEND);
    weather_state(&w);
    check_expect(w.blend == 1.0f && w.rain == 1.0f && check_near(w.fog, 2400.0f, 1.0f),
                 "rain did not arrive whole");
    /* Held: rolls do not move it. */
    weather_set_odds(only_fog);
    weather_advance(WEATHER_HOLD_MAX * 2.0f);
    weather_state(&w);
    check_expect(w.kind == WEATHER_RAIN, "a held weather was rolled away");
    /* Given back to the odds: the next roll is fog. */
    weather_force(-1);
    weather_advance(WEATHER_HOLD_MAX + 1.0f);
    weather_advance(WEATHER_BLEND);
    weather_state(&w);
    check_expect(w.kind == WEATHER_FOG && w.fog < 700.0f && w.sky == WEATHER_SKY_OVERCAST,
                 "the odds did not roll fog");
    /* Odds that rule the weather out end it at once. */
    weather_set_odds(desert);
    weather_state(&w);
    check_expect((w.kind == WEATHER_SUNNY || w.kind == WEATHER_CLOUDY) && w.blend == 1.0f,
                 "fog stayed in a place where it cannot happen");
    weather_set_on(0);
    weather_force(WEATHER_SNOW);
    weather_advance(WEATHER_BLEND);
    weather_state(&w);
    check_expect(w.snow == 0.0f && w.fog >= WEATHER_CLEAR, "weather off still snows");
    weather_set_on(1);
    /* The simulated view: a fog's is short and fogged from near the camera, a clear day's long and hazy at the end. */
    weather_set(WEATHER_SUNNY);
    weather_state(&w);
    check_expect(w.view > 4000.0f && w.haze > 0.6f, "a clear day's view is not long with a late haze");
    weather_force(WEATHER_FOG);
    weather_advance(WEATHER_BLEND * 0.5f);
    weather_state(&w);
    check_expect(w.view < 4000.0f && w.view > 700.0f, "the view does not close in during a change");
    weather_advance(WEATHER_BLEND);
    weather_state(&w);
    check_expect(check_near(w.view, 700.0f, 1.0f) && w.haze < 0.2f, "a fog's view is not short");
    check_expect(df_fog_below(1400.0f) <= 1400.0f && df_fog_solid(df_fog_below(1400.0f)) <= 1400.0f
                 && df_fog_below(1400.0f) > 1200.0f,
                 "the simulated fog end is not a table entry just inside the clip");
    weather_force(-1);
    check_expect(weather_by_name("thunder") == WEATHER_THUNDER && weather_by_name("hail") == WEATHER_KINDS,
                 "weather names do not match the kinds");
    weather_force(-1);
    weather_set_odds(0);
    weather_set(WEATHER_SUNNY);
}

static df_u32 g_snd_seed = 12345u;

static int snd_rand(void)
{
    g_snd_seed = g_snd_seed * 1103515245u + 12345u;
    return (int)((g_snd_seed >> 16) & 0x7fff);
}

/* Left and right energy of a stereo buffer. */
static void snd_energy(const df_i16 *buf, int frames, double *l, double *r)
{
    int i;

    *l = 0.0;
    *r = 0.0;
    for (i = 0; i < frames; i++) {
        *l += (double)buf[i * 2] * buf[i * 2];
        *r += (double)buf[i * 2 + 1] * buf[i * 2 + 1];
    }
}

/* The mixer, the voices and the music's events (docs/sound.md). */
static void check_sound(void)
{
    static signed char samples[1001];
    static df_i16 a[2600 * 2];
    static df_i16 b[2600 * 2];
    static SongEvent ev[128];
    static df_u32 out[4096];
    SoundClip clip;
    MixVoice va;
    MixVoice vb;
    Song song;
    int i;

    for (i = 0; i < 1001; i++) {
        samples[i] = (signed char)(snd_rand() & 255);
    }
    samples[0] = 127;
    samples[1] = -128;
    for (i = 0; i < 2600 * 2; i++) {
        a[i] = (df_i16)(snd_rand() - 16384);
        if (i % 7 == 0) {
            a[i] = (df_i16)(i % 2 ? -32700 : 32700);
        }
        b[i] = a[i];
    }
    mem_zero(&va, sizeof(va));
    va.data = samples;
    va.frames = 1001;
    va.step = MIX_ONE;
    va.gl = 32767;
    va.gr = 12345;
    va.on = 1;
    vb = va;
    mix_set_mmx(0);
    mix_voice(&va, a, 1200);
    mix_set_mmx(1);
    mix_voice(&vb, b, 1200);
    check_expect(memcmp(a, b, sizeof(a)) == 0, "the MMX mixer does not match the C mixer");
    check_expect(!va.on && !vb.on, "a voice did not stop at the end of its clip");
    va.on = vb.on = 1;
    va.pos = vb.pos = 0;
    va.loop = vb.loop = 1;
    mix_set_mmx(0);
    mix_voice(&va, a, 2600);
    mix_set_mmx(1);
    mix_voice(&vb, b, 2600);
    check_expect(memcmp(a, b, sizeof(a)) == 0 && va.pos == 2600u % 1001u && vb.pos == va.pos,
                 "a looping voice differs between the mixers or lost its place");
    printf("mixer: MMX %s, C and MMX agree over 3800 frames\n", mix_has_mmx() ? "found" : "missing");
    mix_clear(a, 4);
    va.pos = 0;
    va.loop = 0;
    va.on = 1;
    mix_voice(&va, a, 2);
    check_expect(a[0] == 16255 && a[2] == -16384,
                 "a full sample at full gain is not half of full scale ((s << 8) * g >> 16)");
    a[0] = 32000;
    va.pos = 0;
    va.on = 1;
    mix_voice(&va, a, 1);
    check_expect(a[0] == 32767, "the mix did not saturate");
    mix_clear(a, 100);
    va.pos = 0;
    va.frac = 0;
    va.on = 1;
    va.step = MIX_ONE / 2;
    mix_voice(&va, a, 100);
    check_expect(va.pos == 50 && a[0] == a[2] && a[4] == a[6],
                 "a clip at half the output's rate did not hold each sample two frames");

    /* Voices from points: louder on the side they are on, quiet past reach, the quietest taken. */
    clip.data = samples;
    clip.frames = 1001;
    clip.rate = 11025;
    sound_set_mix_rate(11025);
    sound_set_on(1);
    sound_stop_all();
    sound_listen(0.0f, 0.0f, 0.0f, 0.0f);
    {
        double l;
        double r;
        int h;

        h = sound_play_at(&clip, 500.0f, 0.0f, 0.0f, 100.0f, 1000.0f, 1.0f, SOUND_LOOP);
        mix_clear(a, 1000);
        sound_mix(a, 1000);
        snd_energy(a, 1000, &l, &r);
        check_expect(h != 0 && r > l * 4.0, "a sound on the right is not louder on the right");
        sound_listen(0.0f, 0.0f, 0.0f, DF_TAU * 0.5f);
        sound_update();
        mix_clear(a, 1000);
        sound_mix(a, 1000);
        snd_energy(a, 1000, &l, &r);
        check_expect(l > r * 4.0, "turned round, the sound did not move to the left");
        sound_stop(h);
        check_expect(!sound_playing(h) && sound_busy() == 0, "a stopped voice still plays");
        check_expect(sound_play_at(&clip, 5000.0f, 0.0f, 0.0f, 100.0f, 1000.0f, 1.0f, 0) == 0,
                     "a sound out of reach took a voice");
        for (i = 0; i < SOUND_VOICES; i++) {
            sound_play(&clip, 0.5f, 0.0f, SOUND_LOOP);
        }
        check_expect(sound_busy() == SOUND_VOICES, "the voices did not fill");
        check_expect(sound_play(&clip, 0.1f, 0.0f, 0) == 0, "a quiet sound took a louder one's voice");
        h = sound_play(&clip, 1.0f, 0.0f, 0);
        check_expect(h != 0 && sound_playing(h) && sound_busy() == SOUND_VOICES,
                     "a loud sound did not take the quietest voice");
        sound_stop_all();
    }

    /* The events: forty notes at once under a limit of 32, the oldest ended first. */
    for (i = 0; i < 40; i++) {
        ev[i].tick = (df_u32)i;
        ev[i].msg = 0x90u | ((df_u32)(20 + i) << 8) | (100u << 16);
        ev[40 + i].tick = 100;
        ev[40 + i].msg = 0x80u | ((df_u32)(20 + i) << 8);
    }
    ev[80].tick = 50;
    ev[80].msg = 0xb0u | (7u << 8) | (100u << 16);
    /* Out of order on purpose: the sort puts it, and at tick 100 the note-offs first. */
    ev[81].tick = 100;
    ev[81].msg = 0x91u | (60u << 8) | (90u << 16);
    ev[82].tick = 120;
    ev[82].msg = 0x81u | (60u << 8);
    ev[83] = ev[0];
    ev[0].tick = 30;
    ev[0].msg = 0xc0u | (5u << 8);
    song.events = ev;
    song.count = 84;
    song.division = 480;
    song.tempo = 4000000u;
    song.length = 200;
    song_sort(&song);
    {
        int ok;

        ok = 1;
        for (i = 1; i < song.count; i++) {
            if (ev[i].tick < ev[i - 1].tick) {
                ok = 0;
            }
        }
        for (i = 0; i < song.count; i++) {
            if (ev[i].tick == 100 && ev[i].msg == (0x91u | (60u << 8) | (90u << 16))) {
                ok &= ev[i - 1].tick == 100 && (ev[i - 1].msg & 0xf0u) == 0x80u
                    && (i + 1 >= song.count || ev[i + 1].tick > 100);
            }
        }
        check_expect(ok, "the song's events are not in time order with note-offs first");
    }
    {
        int words;
        int stolen;
        int peak;
        int sounding;
        int most;
        int vol_ok;
        df_u32 t;

        words = music_render(&song, out, 4096, 32, 50, &stolen, &peak);
        sounding = 0;
        most = 0;
        vol_ok = 0;
        t = 0;
        for (i = 0; i < words; i += 3) {
            df_u32 e;

            t += out[i];
            e = out[i + 2];
            if ((e >> 24) != 0) {
                continue;
            }
            if ((e & 0xf0u) == 0x90u && ((e >> 16) & 0x7fu) > 0) {
                sounding++;
            } else if ((e & 0xf0u) == 0x80u) {
                sounding--;
            }
            most = sounding > most ? sounding : most;
            if ((e & 0xffffu) == 0x07b0u && t == 50) {
                vol_ok = ((e >> 16) & 0x7fu) == 50;
            }
        }
        check_expect(words > 0 && (out[2] >> 24) == 1 && (out[2] & 0xffffffu) == 4000000u,
                     "the stream does not open with the song's tempo");
        check_expect(stolen == 8 && peak == 32 && most <= 32 && sounding == 0,
                     "forty notes under a 32-voice limit did not end the oldest eight");
        check_expect(vol_ok, "a channel volume was not scaled by the music volume");
        check_expect(t == 200 && (out[words - 1] >> 24) == 2, "the stream does not end at the song's length");
        check_expect(music_render(&song, out, 30, 32, 100, 0, 0) == 0, "a stream too long for its room was made");
    }
}

static int g_handled;
static int g_handled_what;

static int take_use(const Scene *scene, const Usable *u, int index)
{
    (void)scene;
    (void)index;
    g_handled++;
    g_handled_what = u->what;
    return 1;
}

/* Using things and the models that move (docs/interact.md). */
/*
 * Teleports (docs/interact.md): a pad walked into sends the eye to its
 * target, once as the body comes in, not each frame it stays; a pad at
 * the far end does not send it straight back; a hand use of one does
 * the same; not in reach of the hand, a walk-into pad is never aimed at.
 */
static void check_teleport(void)
{
    static const float px[4] = { -40.0f, 40.0f, 40.0f, -40.0f };
    static const float py[4] = { 0.0f, 0.0f, 80.0f, 80.0f };
    static const float pz[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Mesh *pad;
    Instance models[2];
    Bound bounds[2];
    Mover movers[2];
    Usable uses[2];
    Scene scene;
    Camera cam;
    float x, y, z, yaw;
    int k;
    int ok;

    pad = fan_mesh(px, py, pz, 4, 0.0f, 0.0f, -1.0f);
    check_expect(pad != 0, "the teleport pad was not allocated");
    if (!pad) {
        return;
    }
    /* Pad 0 at z 100 sends to (1000, 68, 1000) facing +x; pad 1 stands there and sends back. */
    place_at(&models[0], pad, 0.0f, 0.0f, 100.0f);
    place_at(&models[1], pad, 1000.0f, 0.0f, 1000.0f);
    for (k = 0; k < 2; k++) {
        instance_world_box(&models[k], &bounds[k]);
    }
    mem_zero(movers, sizeof(movers));
    mem_zero(uses, sizeof(uses));
    for (k = 0; k < 2; k++) {
        movers[k].inst = &models[k];
        movers[k].model = k;
        movers[k].next = -1;
        movers[k].teleport = 1;
        uses[k].kind = USE_MOVER;
        uses[k].model = k;
        uses[k].sprite = -1;
        uses[k].mover = k;
        uses[k].touch = 1;
    }
    movers[0].to_x = 1000.0f;
    movers[0].to_y = 68.0f;
    movers[0].to_z = 1000.0f;
    movers[0].to_yaw = DF_TAU * 0.25f;
    movers[1].to_x = 0.0f;
    movers[1].to_y = 68.0f;
    movers[1].to_z = 100.0f;
    mover_init(movers, 2);
    mem_zero(&scene, sizeof(scene));
    scene.models = models;
    scene.bounds = bounds;
    scene.nmodels = 2;
    scene.usables = uses;
    scene.nusables = 2;
    scene.movers = movers;
    scene.nmovers = 2;
    interact_teleport(&x, &y, &z, &yaw);
    /* Walking up to the pad: nothing until the body is in its box. */
    camera_set(&cam, 0.0f, 68.0f, 0.0f, 0.0f, 0.0f);
    interact_frame(&scene, &cam, 0.016f, 0);
    ok = !interact_teleport(&x, &y, &z, &yaw);
    camera_set(&cam, 0.0f, 68.0f, 90.0f, 0.0f, 0.0f);
    interact_frame(&scene, &cam, 0.016f, 0);
    ok = ok && interact_teleport(&x, &y, &z, &yaw) && x == 1000.0f && z == 1000.0f && y == 68.0f
        && yaw == DF_TAU * 0.25f;
    check_expect(ok, "walking into a teleport pad did not send the eye to its target");
    /* Taken: the eye is on the far pad, which does not send it back; it stays there. */
    camera_set(&cam, x, y, z, yaw, 0.0f);
    ok = 1;
    for (k = 0; k < 5; k++) {
        interact_frame(&scene, &cam, 0.016f, 0);
        ok = ok && !interact_teleport(&x, &y, &z, &yaw);
    }
    check_expect(ok, "a teleport landed on another pad, which sent it back, or went again each frame");
    /* trigview draws the pad the body stands in red: the far one now. */
    check_expect(interact_touching() == 1, "the pad the body stands in is not the one touched");
    /* Off it and on again: then it sends back. */
    camera_set(&cam, 1000.0f, 68.0f, 800.0f, 0.0f, 0.0f);
    interact_frame(&scene, &cam, 0.016f, 0);
    camera_set(&cam, 1000.0f, 68.0f, 1000.0f, 0.0f, 0.0f);
    interact_frame(&scene, &cam, 0.016f, 0);
    check_expect(interact_teleport(&x, &y, &z, &yaw) && x == 0.0f && z == 100.0f,
                 "stepping onto the far pad again did not send the eye back");
    /* By hand: a pad with no reach is never aimed at; given one, a use sends the eye. */
    camera_set(&cam, 0.0f, 40.0f, 30.0f, 0.0f, 0.0f);
    interact_frame(&scene, &cam, 0.016f, 1);
    ok = !interact_teleport(&x, &y, &z, &yaw);
    uses[0].reach = 128.0f;
    interact_frame(&scene, &cam, 0.016f, 1);
    ok = ok && interact_teleport(&x, &y, &z, &yaw) && x == 1000.0f;
    check_expect(ok, "a teleport by hand went with no reach, or not with one");
    /*
     * Third person: a use cast from the body's eye along its facing, not
     * the camera's (interact_request_use_from); a pad set off by the
     * game's named body walking into it, the camera elsewhere.
     */
    {
        UseHit hit;
        Body pb;

        ok = interact_aim_from(&scene, 0.0f, 40.0f, 30.0f, 0.0f, 0.0f, 1.0f, &hit) && hit.usable == 0
            && !interact_aim_from(&scene, 0.0f, 40.0f, 30.0f, 0.0f, 0.0f, -1.0f, &hit);
        camera_set(&cam, 5000.0f, 68.0f, 5000.0f, 0.0f, 0.0f);
        interact_request_use_from(0.0f, 40.0f, 30.0f, 0.0f, 0.0f, 1.0f);
        interact_frame(&scene, &cam, 0.016f, interact_take_use());
        ok = ok && interact_teleport(&x, &y, &z, &yaw) && x == 1000.0f;
        check_expect(ok, "a use from the body's own eye and facing did not take what it faced");
        interact_frame(&scene, &cam, 0.016f, 0);
        body_init(&pb, 16.0f, 72.0f, 24.0f, 68.4f);
        pb.feet[0] = 0.0f;
        pb.feet[1] = 0.0f;
        pb.feet[2] = 95.0f;
        interact_set_body(&pb);
        interact_frame(&scene, &cam, 0.016f, 0);
        ok = interact_teleport(&x, &y, &z, &yaw) && x == 1000.0f;
        interact_set_body(0);
        check_expect(ok, "a named body walking onto a pad did not set it off, the camera far off");
    }
}

/*
 * Water (docs/water.md): a body's depth by its eye, the walker's heights
 * (feet WALK_EYE under the eye, waist half WALK_HEIGHT over the feet);
 * dry off the rectangle; where two meet, the higher wins.
 */
static void check_water(void)
{
    Water w[2];
    Scene scene;
    float y;
    df_u32 c;
    int ok;

    mem_zero(w, sizeof(w));
    w[0].x0 = 0.0f;
    w[0].x1 = 1000.0f;
    w[0].z0 = 0.0f;
    w[0].z1 = 1000.0f;
    w[0].y = 100.0f;
    w[0].color = 0x102030u;
    w[1] = w[0];
    w[1].x0 = 800.0f;
    w[1].x1 = 2000.0f;
    w[1].y = 300.0f;
    w[1].color = 0x405060u;
    mem_zero(&scene, sizeof(scene));
    scene.water = w;
    scene.nwater = 2;
    ok = scene_water_body(&scene, 500.0f, 100.0f + WALK_EYE + 1.0f, 500.0f) == WATER_DRY
        && scene_water_body(&scene, 500.0f, 100.0f + WALK_EYE - 1.0f, 500.0f) == WATER_FEET
        && scene_water_body(&scene, 500.0f, 100.0f + WALK_EYE - WALK_HEIGHT * 0.5f - 1.0f, 500.0f) == WATER_WAIST
        && scene_water_body(&scene, 500.0f, 99.0f, 500.0f) == WATER_HEAD
        && scene_water_body(&scene, -10.0f, 0.0f, 500.0f) == WATER_DRY;
    check_expect(ok, "a body's depth in water is not dry, feet, waist, head by its eye");
    ok = scene_water_at(&scene, 900.0f, 500.0f, &y, &c) && y == 300.0f && c == 0x405060u
        && scene_water_at(&scene, 100.0f, 500.0f, &y, &c) && y == 100.0f
        && !scene_water_at(&scene, 3000.0f, 500.0f, &y, &c);
    scene.nwater = 0;
    ok = ok && !scene_water_at(&scene, 100.0f, 500.0f, &y, &c);
    check_expect(ok, "the water over a point is not the highest there, or there is some where none is");
}

static void check_interact(void)
{
    static const float qx[4] = { 0.0f, 40.0f, 40.0f, 0.0f };
    static const float qy[4] = { 0.0f, 0.0f, 80.0f, 80.0f };
    static const float qz[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static const float wx[4] = { -500.0f, 500.0f, 500.0f, -500.0f };
    static const float wy[4] = { -10.0f, -10.0f, 200.0f, 200.0f };
    Mesh *door;
    Mesh *wall;
    Instance models[3];
    Mover movers[2];
    Usable uses[2];
    Scene scene;
    Camera cam;
    UseHit hit;
    Bound b;

    door = fan_mesh(qx, qy, qz, 4, 0.0f, 0.0f, -1.0f);
    wall = fan_mesh(wx, wy, qz, 4, 0.0f, 0.0f, -1.0f);
    check_expect(door && wall, "interact meshes were not allocated");
    if (!door || !wall) {
        return;
    }
    /* A door 40 wide hinged at its origin, at z 100; a wall at z 100 behind a passage at x 200. */
    place_at(&models[0], door, 0.0f, 0.0f, 100.0f);
    place_at(&models[1], wall, 700.0f, 0.0f, 100.0f);
    place_at(&models[2], wall, 0.0f, 0.0f, 80.0f);
    mem_zero(movers, sizeof(movers));
    movers[0].inst = &models[0];
    movers[0].model = 0;
    movers[0].ry = -DF_TAU * 0.25f;
    movers[0].secs = 1.0f;
    movers[0].next = -1;
    movers[0].door = 1;
    movers[1].inst = &models[0];
    movers[1].model = 0;
    movers[1].next = -1;
    mover_init(movers, 1);
    mem_zero(uses, sizeof(uses));
    uses[0].kind = USE_MOVER;
    uses[0].model = 0;
    uses[0].sprite = -1;
    uses[0].mover = 0;
    uses[0].reach = 128.0f;
    uses[1].kind = USE_PASSAGE;
    uses[1].model = -1;
    uses[1].sprite = -1;
    uses[1].mover = -1;
    uses[1].cx = 400.0f;
    uses[1].cy = 40.0f;
    uses[1].cz = 100.0f;
    uses[1].nx = 0.0f;
    uses[1].nz = -1.0f;
    uses[1].hw = 20.0f;
    uses[1].hh = 40.0f;
    uses[1].hd = 8.0f;
    uses[1].reach = 128.0f;
    uses[1].what = 7;
    mem_zero(&scene, sizeof(scene));
    scene.models = models;
    scene.nmodels = 2;
    scene.usables = uses;
    scene.nusables = 2;
    scene.movers = movers;
    scene.nmovers = 1;

    camera_set(&cam, 20.0f, 40.0f, 50.0f, 0.0f, 0.0f);
    check_expect(interact_aim(&scene, &cam, &hit) && hit.usable == 0 && check_near(hit.dist, 50.0f, 0.5f),
                 "the door in the middle of the view, 50 away, was not aimed at");
    camera_set(&cam, 20.0f, 40.0f, -100.0f, 0.0f, 0.0f);
    check_expect(!interact_aim(&scene, &cam, &hit), "a door 200 away was in reach");
    camera_set(&cam, 20.0f, 40.0f, 50.0f, DF_TAU * 0.25f, 0.0f);
    check_expect(!interact_aim(&scene, &cam, &hit), "a door beside the view was aimed at");
    /* The passage is a face of the wall: its box is met before the wall behind it. */
    camera_set(&cam, 400.0f, 40.0f, 30.0f, 0.0f, 0.0f);
    check_expect(interact_aim(&scene, &cam, &hit) && hit.usable == 1 && hit.model == 1
                 && hit.dist < hit.wall,
                 "a door painted on a wall was not used before the wall");
    /* A wall in front hides the door. */
    scene.nmodels = 3;
    camera_set(&cam, 20.0f, 40.0f, 50.0f, 0.0f, 0.0f);
    check_expect(!interact_aim(&scene, &cam, &hit) && hit.model == 2,
                 "a door was used through a wall");
    scene.nmodels = 2;

    /* The passage goes to the handler; nothing happens without one. */
    g_handled = 0;
    interact_set_handler(take_use);
    check_expect(interact_use(&scene, 1) && g_handled == 1 && g_handled_what == 7,
                 "a passage did not reach the game's handler");
    interact_set_handler(0);
    check_expect(!interact_use(&scene, 1) && g_handled == 1, "a passage was taken with no handler");

    /* A locked door says so and stays; with locks off it opens. */
    movers[0].lock = 10;
    check_expect(!interact_use(&scene, 0) && movers[0].state == MOVER_START,
                 "a locked door opened");
    check_expect(interact_saying(0) != 0, "a locked door said nothing");
    interact_set_locks(0);
    check_expect(interact_use(&scene, 0) && movers[0].state == MOVER_FORWARD,
                 "with locks off a locked door did not open");
    interact_set_locks(1);
    movers[0].lock = 0;
    mover_step(&scene, 0.5f);
    check_expect(check_near(movers[0].t, 0.5f, 0.001f) && mover_busy(&scene, 0),
                 "a door is not half open half way");
    check_expect(!mover_start(&scene, 0), "a moving door started again");
    mover_step(&scene, 0.6f);
    check_expect(movers[0].state == MOVER_END && check_near(models[0].m[0], 0.0f, 0.001f)
                 && check_near(models[0].m[6], 1.0f, 0.001f)
                 && check_near(models[0].x, 0.0f, 0.001f) && check_near(models[0].z, 100.0f, 0.001f),
                 "an open door is not a quarter turn about its hinge");
    /* Open, its box swung back: the eye that met it now misses it. */
    camera_set(&cam, 20.0f, 40.0f, 50.0f, 0.0f, 0.0f);
    check_expect(!interact_aim(&scene, &cam, &hit), "an open door is still in the doorway");
    check_expect(mover_sweep(&movers[0], &b) && b.ex > 19.0f && b.ez > 19.0f
                 && b.cx - b.ex < 0.5f && b.cz + b.ez > 139.0f,
                 "a door's sweep box does not hold its quarter turn");
    mover_start(&scene, 0);
    mover_step(&scene, 2.0f);
    check_expect(movers[0].state == MOVER_START && check_near(models[0].m[0], 1.0f, 0.001f),
                 "a door used again did not shut");

    /* A chain: starting the first starts the second; a once mover stays at its end. */
    movers[0].next = 1;
    movers[1].inst = 0;
    movers[1].once = 1;
    movers[1].secs = 0.0f;
    scene.nmovers = 2;
    check_expect(mover_start(&scene, 0) && movers[1].state == MOVER_FORWARD,
                 "the next mover in a chain did not start");
    mover_step(&scene, 1.5f);
    check_expect(movers[0].state == MOVER_END && movers[1].state == MOVER_END,
                 "a chain did not reach its end");
    mover_start(&scene, 0);
    check_expect(movers[0].state == MOVER_BACK && movers[1].state == MOVER_END,
                 "a once mover went back");
    mover_step(&scene, 1.5f);
    movers[0].next = -1;
}

/* The frame profile's sums (docs/profile.md): fed cycles at a known rate. */
static void check_prof(void)
{
    df_u32 c[PROF_STAGES];
    int f;
    int i;

    /* At 100 MHz a millisecond is 100000 cycles. Frames of 10 ms, models 4, one frame of models 9. */
    for (f = 0; f < 60 && prof_frames() == 0; f++) {
        for (i = 0; i < PROF_STAGES; i++) {
            c[i] = 0;
        }
        c[PROF_FRAME] = 1000000u;
        c[PROF_MODELS] = f == 3 ? 900000u : 400000u;
        prof_feed(c, 100);
    }
    check_expect(prof_frames() == 50 && prof_mhz() == 100, "the profile's half second is not 50 frames of 10 ms");
    check_expect(check_near(prof_ms(PROF_FRAME), 10.0f, 0.01f)
                 && check_near(prof_ms(PROF_MODELS), 4.1f, 0.01f)
                 && check_near(prof_peak_ms(PROF_MODELS), 9.0f, 0.01f)
                 && prof_ms(PROF_SWAP) == 0.0f,
                 "the profile's means or worst frame are wrong");
    check_expect(df_eq(prof_name(PROF_MODELS), "models") && df_eq(prof_name(PROF_STREAM), "stream")
                 && df_eq(prof_name(PROF_PLAY), "play"),
                 "the profile's stage names are wrong");
}

/* A fake card for the residency test: a P8 chain is 4/3 of its top level, a cutout twice that. */
static int g_fake_h;
static int g_fake_released;

static df_u32 fake_bytes(int side, int cutout)
{
    df_u32 b;

    b = (df_u32)side * (df_u32)side * 4u / 3u + 8u;
    return cutout ? b * 2u : b;
}

static int fake_upload(int slot, int drop, df_u32 addr, df_u32 *handle)
{
    (void)slot;
    (void)drop;
    (void)addr;
    *handle = (df_u32)++g_fake_h;
    return 1;
}

static void fake_release(df_u32 handle)
{
    (void)handle;
    g_fake_released++;
}

/* The card's residency for a streamed world (docs/streaming.md, The card). */
static void check_texres(void)
{
    TexResOps ops;
    df_u32 used, room;
    int added, sharp, uploads, evictions;
    int i;

    ops.bytes = fake_bytes;
    ops.upload = fake_upload;
    ops.release = fake_release;
    /* 256 KB: one 256 chain (a 128 KB block) beside the small copies, not two. */
    texres_begin(4096u, 4096u + 256u * 1024u, &ops);
    for (i = 0; i < 4; i++) {
        check_expect(texres_add(i, 256, 0, 0.25f), "a picture's small copy did not go on the card");
    }
    check_expect(texres_drop(0) == 5, "a 256 picture's small copy is not its 8 x 8 level");
    /* Near the camera (focal 400, density 0.25: full size within 1600 units). */
    texres_note(0, 100.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(0) == 0, "a picture drawn near did not get its full size");
    texres_note(1, 3000.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(1) == 1, "a picture drawn at 3000 did not get half size");
    /* Slot 2 near, this frame: the full card frees the copy drawn longest ago, slot 0's. */
    texres_note(1, 3000.0f);
    texres_service(400.0f, 1024u * 1024u);
    texres_note(2, 50.0f);
    texres_note(1, 3000.0f);
    texres_service(400.0f, 1024u * 1024u);
    texres_stats(&added, &sharp, &used, &room, &uploads, &evictions);
    check_expect(texres_drop(2) == 0 && texres_drop(1) == 1 && texres_drop(0) == 5 && evictions >= 1,
                 "a full card did not free the sharp copy drawn longest ago");
    /* Three near at once with room for one: the others settle a level or more down. */
    texres_note(0, 10.0f);
    texres_note(1, 10.0f);
    texres_note(2, 10.0f);
    texres_service(400.0f, 1024u * 1024u);
    texres_note(0, 10.0f);
    texres_note(1, 10.0f);
    texres_note(2, 10.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(0) < 5 && texres_drop(1) < 5 && texres_drop(2) < 5
                 && texres_drop(0) + texres_drop(1) + texres_drop(2) >= 2,
                 "three pictures near on a card for one were not downsized to fit");
    /* A budget too small for a 256 chain still sends one copy a frame, never two. */
    texres_note(3, 10.0f);
    texres_service(400.0f, 16u);
    check_expect(texres_drop(3) < 5, "a small budget sent nothing at all");
    for (i = 0; i < 4; i++) {
        texres_remove(i);
    }
    texres_stats(&added, &sharp, &used, &room, &uploads, &evictions);
    check_expect(added == 0 && sharp == 0 && used == 0 && room == 256u * 1024u,
                 "removing every picture did not give the whole card back");
    /* Joined again: the small copy in one 128 KB half, the full chain in the other, which was cut up. */
    check_expect(texres_add(0, 256, 0, 0.25f), "a small copy did not fit an empty card");
    texres_note(0, 10.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(0) == 0, "freed blocks did not join back into a 256 KB block");
    texres_remove(0);
    texres_end();
    /*
     * A sprite first (docs/streaming.md, The card): a 384 KB card has a
     * 256 KB block for one sprite's chain or a wall's, not both sharp.
     * The sprite is served first though the wall is nearer; then a sharp
     * wall drawn this frame keeps its room from a farther sprite, and
     * gives it to a nearer one.
     */
    texres_begin(4096u, 4096u + 384u * 1024u, &ops);
    check_expect(texres_add(0, 256, 0, 0.25f) && texres_add(1, 256, 1, 0.25f),
                 "a wall's or a sprite's small copy did not go on the card");
    texres_note(0, 10.0f);
    texres_note(1, 20.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(1) == 0 && texres_drop(0) > 0 && texres_drop(0) < 5,
                 "the nearer wall took the sprite's room, or the wall got nothing");
    texres_remove(1);
    texres_note(0, 10.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(0) == 0, "alone, the wall did not get its full size");
    check_expect(texres_add(1, 256, 1, 0.25f), "the sprite's small copy did not come back");
    /* A nearer wall (the ground under the camera) keeps its room: the sprite settles lower. */
    texres_note(0, 10.0f);
    texres_note(1, 500.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(0) == 0 && texres_drop(1) > 0 && texres_drop(1) < 5,
                 "a sprite took the room of a wall nearer than itself");
    /* A farther wall gives its room to a near sprite. */
    texres_note(0, 1000.0f);
    texres_note(1, 10.0f);
    texres_service(400.0f, 1024u * 1024u);
    check_expect(texres_drop(1) == 0 && texres_drop(0) > 0,
                 "a sharp wall farther than a near sprite did not give it its room");
    texres_remove(0);
    texres_remove(1);
    texres_end();
    /*
     * A walk's churn: pictures come and go and sharpen at all sizes, in
     * no order. The small blocks keep to the top of the card, so with
     * half of it free there is still a whole 128 KB block for a sprite.
     */
    {
        df_u32 seed;
        df_u32 largest;
        df_u32 free_bytes;
        int live[200];
        int step;

        texres_begin(4096u, 4096u + 1536u * 1024u, &ops);
        seed = 12345u;
        for (i = 0; i < 200; i++) {
            live[i] = 0;
        }
        for (step = 0; step < 4000; step++) {
            int k;

            seed = seed * 1664525u + 1013904223u;
            k = (int)((seed >> 8) % 200u);
            if (!live[k]) {
                live[k] = texres_add(k, 16 << ((seed >> 20) % 4u), (int)((seed >> 24) & 1u), 0.25f);
            } else if (((seed >> 16) & 7u) == 0) {
                texres_remove(k);
                live[k] = 0;
            } else {
                texres_note(k, (float)((seed >> 12) % 3000u));
                texres_service(400.0f, 1024u * 1024u);
            }
        }
        /* Every sharp copy given back, the small copies of 100 or so pictures stay. */
        for (i = 0; i < 200; i++) {
            if (live[i] && i % 2) {
                texres_remove(i);
                live[i] = 0;
            }
        }
        texres_stats(0, 0, 0, &free_bytes, 0, 0);
        largest = texres_largest_free();
        printf("texres: after a walk's churn, %u KB free, the largest block %u KB\n",
               (unsigned int)(free_bytes / 1024u), (unsigned int)(largest / 1024u));
        check_expect(free_bytes < 768u * 1024u || largest >= 128u * 1024u,
                     "half the card free and no 128 KB block whole: the small copies split it all");
        /* A sprite comes close: a region is freed whole for it, not a copy here and there. */
        check_expect(texres_add(200, 256, 1, 0.25f), "a sprite's small copy did not go on the card after the churn");
        texres_note(200, 10.0f);
        texres_service(400.0f, 1024u * 1024u);
        check_expect(texres_drop(200) == 0, "after a walk's churn a close sprite could not get its full size");
        texres_remove(200);
        for (i = 0; i < 200; i++) {
            if (live[i]) {
                texres_remove(i);
            }
        }
        texres_end();
    }
    /* Fair: three sprites wanting and a budget for nothing; the wall still gets its copy this frame. */
    texres_begin(4096u, 4096u + 1024u * 1024u, &ops);
    for (i = 0; i < 4; i++) {
        check_expect(texres_add(i, 128, i > 0, 0.25f), "a picture's small copy did not go on the card");
        texres_note(i, 10.0f);
    }
    texres_service(400.0f, 16u);
    check_expect(texres_drop(0) < 4 && (texres_drop(1) < 4 || texres_drop(2) < 4 || texres_drop(3) < 4),
                 "with sprites wanting and no budget, the wall or the sprites got no copy this frame");
    for (i = 0; i < 4; i++) {
        texres_remove(i);
    }
    texres_end();
}

/*
 * An entity's view (docs/entities.md): one facing north, seen from each
 * side, shows the record and mirroring Daggerfall's walking pictures
 * need; its sprite stands on its feet; idle, it faces the viewer.
 */
static void check_entities(void)
{
    static const float at[8][2] = {
        { 0.0f, 100.0f }, { -70.0f, 70.0f }, { -100.0f, 0.0f }, { -70.0f, -70.0f },
        { 0.0f, -100.0f }, { 70.0f, -70.0f }, { 100.0f, 0.0f }, { 70.0f, 70.0f }
    };
    /* From the north (in front), round by the west to the east: front, the left side, back, the right mirrored. */
    static const int view[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
    static const int flip[8] = { 0, 0, 0, 0, 0, 1, 1, 1 };
    Entity e;
    Flat fl;
    int ok;
    int k;

    mem_zero(&e, sizeof(e));
    e.yaw = 0.0f;
    e.moving = 1;
    e.live = 1;
    e.y = 10.0f;
    for (k = 0; k < ENTITY_VIEWS; k++) {
        e.pose[ENTITY_WALK].slot[k] = (df_u16)(100 + k);
        e.pose[ENTITY_WALK].w[k] = 40;
        e.pose[ENTITY_WALK].h[k] = 80;
        e.pose[ENTITY_IDLE].slot[k] = 0xffff;
    }
    e.pose[ENTITY_IDLE].slot[0] = 200;
    e.pose[ENTITY_IDLE].w[0] = 35;
    e.pose[ENTITY_IDLE].h[0] = 78;
    ok = 1;
    for (k = 0; k < 8; k++) {
        int f;
        int v;

        v = entity_view(&e, at[k][0], at[k][1], &f);
        ok = ok && v == view[k] && f == flip[k];
        ok = ok && entity_flat(&e, at[k][0], at[k][1], &fl) && fl.slot == 100 + view[k] && fl.flip == flip[k];
    }
    check_expect(ok, "an entity's view does not turn with it");
    /* Turned to face east, the camera in the east sees its front. */
    e.yaw = DF_TAU * 0.25f;
    check_expect(entity_flat(&e, 100.0f, 0.0f, &fl) && fl.slot == 100 && !fl.flip
                 && fl.y == 10.0f + 40.0f && fl.w == 40.0f,
                 "an entity turned east is not seen from the front from the east, or not on its feet");
    e.moving = 0;
    check_expect(entity_flat(&e, -100.0f, 0.0f, &fl) && fl.slot == 200 && !fl.flip && fl.h == 78.0f,
                 "an idle entity of one picture does not face the viewer in it");
    /*
     * A creature's idle pose turns as its walk does, each view at its own
     * size; one whose side-on picture faces the viewer's right (the
     * ghost's record 2) mirrors the other way.
     */
    e.yaw = 0.0f;
    for (k = 0; k < ENTITY_VIEWS; k++) {
        e.pose[ENTITY_IDLE].slot[k] = (df_u16)(300 + k);
        e.pose[ENTITY_IDLE].w[k] = (df_u16)(30 + k);
        e.pose[ENTITY_IDLE].h[k] = 60;
    }
    e.pose[ENTITY_IDLE].right = 1u << 2;
    ok = entity_flat(&e, -100.0f, 0.0f, &fl) && fl.slot == 302 && fl.flip && fl.w == 32.0f;
    ok = ok && entity_flat(&e, 100.0f, 0.0f, &fl) && fl.slot == 302 && !fl.flip;
    ok = ok && entity_flat(&e, -70.0f, 70.0f, &fl) && fl.slot == 301 && !fl.flip;
    ok = ok && entity_flat(&e, 0.0f, -100.0f, &fl) && fl.slot == 304 && !fl.flip;
    check_expect(ok, "a standing creature's views do not turn, or a right-facing record is not mirrored the other way");
    /* With no idle pose it stands in its walking one. */
    e.pose[ENTITY_IDLE].slot[0] = 0xffff;
    check_expect(entity_flat(&e, -100.0f, 0.0f, &fl) && fl.slot == 102 && !fl.flip && fl.h == 80.0f,
                 "an entity with no idle pose does not stand in its walking pictures");
    /*
     * Acts (Entity.act): an attack pose shows instead, turning as the
     * walk does, at its own rate; one not filled in is not shown (the
     * stand or walk is); a body (one picture) faces the viewer; a held
     * frame goes to the sprite. entity_init leaves no picture anywhere.
     */
    {
        Entity f;
        int k2;

        entity_init(&f);
        ok = 1;
        for (k2 = 0; k2 < ENTITY_POSES; k2++) {
            int v;

            for (v = 0; v < ENTITY_VIEWS; v++) {
                ok = ok && f.pose[k2].slot[v] == 0xffff;
            }
        }
        check_expect(ok && f.act == 0 && f.hold == 0 && f.radius == 0.0f, "entity_init left a picture in a pose");
        f.live = 1;
        f.moving = 1;
        for (k2 = 0; k2 < ENTITY_VIEWS; k2++) {
            f.pose[ENTITY_WALK].slot[k2] = (df_u16)(100 + k2);
            f.pose[ENTITY_WALK].w[k2] = 40;
            f.pose[ENTITY_WALK].h[k2] = 80;
            f.pose[ENTITY_ATTACK].slot[k2] = (df_u16)(500 + k2);
            f.pose[ENTITY_ATTACK].w[k2] = 50;
            f.pose[ENTITY_ATTACK].h[k2] = 82;
        }
        f.pose[ENTITY_WALK].rate = 6;
        f.pose[ENTITY_ATTACK].rate = 10;
        f.pose[ENTITY_DEAD].slot[0] = 900;
        f.pose[ENTITY_DEAD].w[0] = 90;
        f.pose[ENTITY_DEAD].h[0] = 30;
        f.act = ENTITY_ATTACK;
        ok = entity_flat(&f, -100.0f, 0.0f, &fl) && fl.slot == 502 && !fl.flip && fl.w == 50.0f && fl.rate == 10
            && entity_flat(&f, 100.0f, 0.0f, &fl) && fl.slot == 502 && fl.flip;
        check_expect(ok, "an attack does not show its own pose, turning as the walk does");
        f.act = ENTITY_HURT;
        check_expect(entity_flat(&f, -100.0f, 0.0f, &fl) && fl.slot == 102,
                     "a hurt pose not filled in was shown, not the walk");
        f.act = ENTITY_DEAD;
        f.hold = 1;
        f.frame = 3;
        check_expect(entity_flat(&f, 100.0f, 0.0f, &fl) && fl.slot == 900 && !fl.flip && fl.h == 30.0f
                     && fl.held && fl.frame == 3,
                     "a body does not face the viewer, or its held frame does not reach the sprite");
    }
}

/* A table of the test's own, and a wider one a file of another program might carry. */
typedef struct RecT {
    df_u16 id;
    char name[8];
    signed char a;
    df_u8 arr[3];
    float f;
} RecT;

typedef struct RecW {
    df_u32 id;
    char name[16];
    df_i16 a;
    df_u8 arr[2];
    df_i32 f;
    df_u8 extra;
} RecW;

static const RecField g_rect_fields[] = {
    REC_FIELD("id", REC_UINT, RecT, id),
    REC_FIELD("name", REC_TEXT, RecT, name),
    REC_FIELD("a", REC_INT, RecT, a),
    REC_ARRAY("arr", REC_UINT, RecT, arr),
    REC_FIELD("f", REC_FLOAT, RecT, f)
};
static const RecSchema g_rect = { "TEST", "test", g_rect_fields, 5, (df_u16)sizeof(RecT), 0 };

static const RecField g_recw_fields[] = {
    REC_FIELD("id", REC_UINT, RecW, id),
    REC_FIELD("name", REC_TEXT, RecW, name),
    REC_FIELD("a", REC_INT, RecW, a),
    REC_ARRAY("arr", REC_UINT, RecW, arr),
    REC_FIELD("f", REC_INT, RecW, f),
    REC_FIELD("extra", REC_UINT, RecW, extra)
};
static const RecSchema g_recw = { "TEST", "test", g_recw_fields, 6, (df_u16)sizeof(RecW), 0 };

static df_u8 g_rec_buf[2048];
static df_u32 g_rec_n;

static int rec_put_test(void *ctx, const void *src, df_u32 n)
{
    (void)ctx;
    if (g_rec_n + n > sizeof(g_rec_buf)) {
        return 0;
    }
    memcpy(g_rec_buf + g_rec_n, src, n);
    g_rec_n += n;
    return 1;
}

/*
 * Record files (recfile.c, docs/data.md): rows written and merged back
 * the same; a chunk of some fields changes those alone; another
 * program's wider types are clamped, converted and cut; a field the
 * schema lacks is skipped; a damaged chunk is refused.
 */
static void check_recfile(void)
{
    RecT rows[2];
    RecW w;
    RecTable t;
    RecChunk c;
    RecMerge st;
    const RecT *r;
    df_u32 pos;
    int some[2];

    memset(rows, 0, sizeof(rows));
    rows[0].id = 7;
    strcpy(rows[0].name, "seven");
    rows[0].a = -5;
    rows[0].arr[0] = 1;
    rows[0].arr[1] = 2;
    rows[0].arr[2] = 3;
    rows[0].f = 0.25f;
    rows[1].id = 9;
    strcpy(rows[1].name, "nine");
    g_rec_n = 0;
    check_expect(rec_put_begin(rec_put_test, 0) && rec_put_rows(rec_put_test, 0, &g_rect, rows, 2, 0, 0),
                 "recfile: rows were not written");
    /* The id and a alone, for row 7. */
    rows[0].a = 42;
    some[0] = 0;
    some[1] = 2;
    check_expect(rec_put_rows(rec_put_test, 0, &g_rect, rows, 1, some, 2), "recfile: some fields were not written");
    check_expect(rec_is_file(g_rec_buf, g_rec_n), "recfile: the bytes are not a record file");
    rec_table_init(&t, &g_rect, 1);
    memset(&st, 0, sizeof(st));
    pos = 0;
    while (rec_next(g_rec_buf, g_rec_n, &pos, &c)) {
        check_expect(rec_merge(&t, &c, &st), "recfile: a chunk did not merge");
    }
    r = (const RecT *)rec_row(&t, 7);
    check_expect(t.count == 2 && st.added == 2 && st.changed == 1 && r && strcmp(r->name, "seven") == 0
                     && r->a == 42 && r->arr[2] == 3 && r->f == 0.25f && rec_row(&t, 9) != 0,
                 "recfile: the rows did not merge back with a's change alone");

    /* Another program's wider row: clamped, made a float, cut, an array in part, extra skipped. */
    memset(&w, 0, sizeof(w));
    w.id = 7;
    strcpy(w.name, "averylongname");
    w.a = 300;
    w.arr[0] = 9;
    w.f = 3;
    w.extra = 1;
    g_rec_n = 0;
    rec_put_begin(rec_put_test, 0);
    rec_put_rows(rec_put_test, 0, &g_recw, &w, 1, 0, 0);
    memset(&st, 0, sizeof(st));
    pos = 0;
    check_expect(rec_next(g_rec_buf, g_rec_n, &pos, &c) && rec_merge(&t, &c, &st), "recfile: a wider row did not merge");
    r = (const RecT *)rec_row(&t, 7);
    check_expect(r && r->a == 127 && r->f == 3.0f && strcmp(r->name, "averylo") == 0 && r->arr[0] == 9
                     && r->arr[2] == 3 && st.skipped == 1,
                 "recfile: a wider row was not clamped, converted, cut and skipped");

    /* Damaged: a chunk running past the bytes, and a field past its row. */
    pos = 0;
    check_expect(!rec_next(g_rec_buf, g_rec_n - 4, &pos, &c), "recfile: a chunk past the bytes was read");
    pos = 0;
    rec_next(g_rec_buf, g_rec_n, &pos, &c);
    g_rec_buf[8 + 8 + 12 + 20] = 200;
    check_expect(!rec_merge(&t, &c, &st), "recfile: a field past its row was merged");
}

void check_engine(void)
{
    check_rotation();
    check_camera();
    check_scene();
    check_cull();
    check_occlusion();
    check_loader();
    check_vfs();
    check_console();
    check_mip();
    check_mip_spans();
    check_lights();
    check_light_shadow();
    check_daylight();
    check_weather();
    check_collision();
    check_interact();
    check_teleport();
    check_water();
    check_prof();
    check_texres();
    check_entities();
    check_sound();
    check_recfile();
}
