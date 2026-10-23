/*
 * Engine tests. No Daggerfall archives. Transforms, camera, the visibility
 * list, the loader table, and the VFS mount string.
 */

#include "check_util.h"

#include "engine/assets/loader.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

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

void check_engine(void)
{
    check_rotation();
    check_camera();
    check_scene();
    check_cull();
    check_occlusion();
    check_loader();
    check_vfs();
}
