/*
 * Engine tests. No Daggerfall archives. Transforms, camera, the visibility
 * list, the loader table, the VFS mount string, and the console's log,
 * command table, history, and line editing.
 */

#include "check_util.h"

#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/mip.h"
#include "engine/scene/light.h"
#include "engine/scene/scene.h"
#include "engine/ui/cmd.h"

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
    cut[2] = 1;
    side[1] = 0;
    n = mip_budget(side, cut, 3, 100u, square_bytes, drop);
    check_expect(drop[0] == 5 && drop[1] == 0 && drop[2] == 3 && n == 64u + 128u,
                 "budget went below MIP_MIN_SIDE or counted a missing picture");
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
    check_lights();
}
