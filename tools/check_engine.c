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
    check_loader();
    check_vfs();
}
