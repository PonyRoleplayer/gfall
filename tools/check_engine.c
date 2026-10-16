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
    check_loader();
    check_vfs();
}
