/*
 * Load the one room kept from N0000012. Proves that chamber, its two
 * torches, the floor axis, and the fly frame. Does not open Glide.
 */

#include <stdio.h>

#include "assets/arch3d.h"
#include "game/player.h"
#include "game/world.h"
#include "platform/fs.h"
#include "platform/mem.h"

static int g_fail;

static void expect(int ok, const char *msg)
{
    if (!ok) {
        fprintf(stderr, "fail: %s\n", msg);
        g_fail = 1;
    }
}

static int near(float a, float b, float tol)
{
    float d;

    d = a - b;
    if (d < 0.0f) {
        d = -d;
    }
    return d <= tol;
}

static int write_ppm(const Image *img, const Palette *pal, const char *path)
{
    FILE *f;
    int y;
    int x;

    f = fopen(path, "wb");
    if (!f) {
        return 0;
    }
    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);
    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
            df_u8 index;
            df_u8 rgb[3];

            index = img->index[y * img->width + x];
            rgb[0] = pal->r[index];
            rgb[1] = pal->g[index];
            rgb[2] = pal->b[index];
            if (fwrite(rgb, 1, 3, f) != 3) {
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return 1;
}

static int unique_indices(const Image *img)
{
    df_u8 seen[256];
    int n;
    int i;
    int count;

    for (i = 0; i < 256; i++) {
        seen[i] = 0;
    }
    n = img->width * img->height;
    for (i = 0; i < n; i++) {
        seen[img->index[i]] = 1;
    }
    count = 0;
    for (i = 0; i < 256; i++) {
        count += seen[i];
    }
    return count;
}

static void check_rotation(void)
{
    Instance in;
    float x, y, z;

    mem_zero(&in, sizeof(in));
    instance_set_rotation(&in, 0.0f, 0.0f, 0.0f);
    instance_point(&in, 3.0f, 4.0f, 5.0f, &x, &y, &z);
    expect(near(x, 3.0f, 0.02f) && near(y, 4.0f, 0.02f) && near(z, 5.0f, 0.02f),
           "zero rotation moved the point");

    /* Right-hand +90 degrees about Y takes local +X to world -Z. */
    instance_set_rotation(&in, 0.0f, 1.5707963f, 0.0f);
    instance_point(&in, 1.0f, 0.0f, 0.0f, &x, &y, &z);
    expect(near(x, 0.0f, 0.02f) && near(y, 0.0f, 0.02f) && near(z, -1.0f, 0.02f),
           "+Y rotation did not take +X toward -Z");
}

static void check_fly(void)
{
    Camera cam;
    Input in;
    Basis basis;

    mem_zero(&in, sizeof(in));
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    expect(near(basis.fx, 0.0f, 0.02f) && near(basis.fz, 1.0f, 0.02f),
           "yaw 0 forward is not +Z");
    expect(near(basis.rx, 1.0f, 0.02f) && near(basis.rz, 0.0f, 0.02f),
           "yaw 0 right is not +X");

    in.move_f = 1;
    player_tick(&cam, &in);
    expect(cam.z > 1.0f && near(cam.x, 0.0f, 0.02f), "W did not move toward +Z");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_r = 1;
    player_tick(&cam, &in);
    expect(cam.x > 1.0f && near(cam.z, 0.0f, 0.02f), "D did not move toward +X");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_y = 1;
    player_tick(&cam, &in);
    expect(cam.y > 1.0f, "Q did not raise Y");

    camera_set(&cam, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_y = -1;
    player_tick(&cam, &in);
    expect(cam.y < 10.0f, "E did not lower Y");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 1.5707963f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_f = 1;
    player_tick(&cam, &in);
    expect(cam.x > 1.0f, "W after a right turn did not move toward +X");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.look_dx = 100;
    player_tick(&cam, &in);
    expect(cam.yaw > 0.2f, "mouse right did not increase yaw");
}

static void check_view(const World *world)
{
    int i;
    int found_floor;
    int found_wall;
    float floor_y;
    float floor_x;
    float floor_z;
    float wall_x0, wall_y0, wall_z0;
    float wall_x1, wall_y1, wall_z1;
    int wall_axis;
    Camera cam;
    Basis basis;
    float vx, vy, vz, sx0, sy0, sx1, sy1, oow, ooz;
    float minx, miny, minz, maxx, maxy, maxz;

    found_floor = 0;
    found_wall = 0;
    floor_y = 0.0f;
    floor_x = 0.0f;
    floor_z = 0.0f;
    wall_axis = 0;
    wall_x0 = wall_y0 = wall_z0 = 0.0f;
    wall_x1 = wall_y1 = wall_z1 = 0.0f;

    for (i = 0; i < world_model_count(world) && (!found_floor || !found_wall); i++) {
        const Instance *in;
        int t;

        in = world_instance(world, i);
        for (t = 0; t < in->mesh->ntris && (!found_floor || !found_wall); t++) {
            const Tri *tri;
            float x[3], y[3], z[3];
            int k;
            float yspan, xspan, zspan;

            tri = &in->mesh->tris[t];
            for (k = 0; k < 3; k++) {
                instance_point(in, tri->x[k], tri->y[k], tri->z[k], &x[k], &y[k], &z[k]);
            }
            yspan = df_absf(y[0] - y[1]) + df_absf(y[1] - y[2]);
            xspan = df_absf(x[0] - x[1]) + df_absf(x[1] - x[2]);
            zspan = df_absf(z[0] - z[1]) + df_absf(z[1] - z[2]);
            if (!found_floor && yspan < 0.05f && xspan > 2.0f && zspan > 2.0f) {
                found_floor = 1;
                floor_y = y[0];
                floor_x = (x[0] + x[1] + x[2]) / 3.0f;
                floor_z = (z[0] + z[1] + z[2]) / 3.0f;
            }
            if (!found_wall && yspan > 2.0f && (xspan < 0.05f || zspan < 0.05f)) {
                int hi;
                int lo;

                hi = 0;
                lo = 0;
                for (k = 1; k < 3; k++) {
                    if (y[k] > y[hi]) {
                        hi = k;
                    }
                    if (y[k] < y[lo]) {
                        lo = k;
                    }
                }
                if (y[hi] - y[lo] > 2.0f) {
                    found_wall = 1;
                    wall_axis = (xspan < 0.05f) ? 0 : 2;
                    wall_x0 = x[lo];
                    wall_y0 = y[lo];
                    wall_z0 = z[lo];
                    wall_x1 = x[hi];
                    wall_y1 = y[hi];
                    wall_z1 = z[hi];
                }
            }
        }
    }
    expect(found_floor, "no constant-Y floor");
    expect(found_wall, "no upright wall");
    printf("floor y %.2f at %.1f %.1f\n", floor_y, floor_x, floor_z);
    printf("wall axis %s  low y %.2f  high y %.2f\n",
           wall_axis == 0 ? "X" : "Z", wall_y0, wall_y1);

    world_bounds(world, &minx, &miny, &minz, &maxx, &maxy, &maxz);
    printf("bounds %.1f..%.1f  %.1f..%.1f  %.1f..%.1f\n",
           minx, maxx, miny, maxy, minz, maxz);
    expect(maxy > miny, "vertical extent is empty");

    camera_set(&cam, floor_x, floor_y, floor_z - 20.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    camera_view(&cam, &basis, floor_x, floor_y, floor_z + 30.0f, &vx, &vy, &vz);
    expect(camera_project(vx, vy, vz, &sx0, &sy0, &oow, &ooz), "level point is behind the camera");
    camera_view(&cam, &basis, floor_x, floor_y + 40.0f, floor_z + 30.0f, &vx, &vy, &vz);
    expect(camera_project(vx, vy, vz, &sx1, &sy1, &oow, &ooz), "raised point is behind the camera");
    expect(sy1 > sy0 + 5.0f, "higher world Y is not higher on screen");

    if (found_wall) {
        float cx, cy, cz;

        if (wall_axis == 0) {
            cx = wall_x0 - 30.0f;
            cy = (wall_y0 + wall_y1) * 0.5f;
            cz = (wall_z0 + wall_z1) * 0.5f;
            camera_set(&cam, cx, cy, cz, 1.5707963f, 0.0f);
        } else {
            cx = (wall_x0 + wall_x1) * 0.5f;
            cy = (wall_y0 + wall_y1) * 0.5f;
            cz = wall_z0 - 30.0f;
            camera_set(&cam, cx, cy, cz, 0.0f, 0.0f);
        }
        camera_basis(&cam, &basis);
        camera_view(&cam, &basis, wall_x0, wall_y0, wall_z0, &vx, &vy, &vz);
        expect(camera_project(vx, vy, vz, &sx0, &sy0, &oow, &ooz), "wall bottom is behind the camera");
        camera_view(&cam, &basis, wall_x1, wall_y1, wall_z1, &vx, &vy, &vz);
        expect(camera_project(vx, vy, vz, &sx1, &sy1, &oow, &ooz), "wall top is behind the camera");
        expect(sy1 > sy0 + 5.0f, "wall top is not above the wall bottom");
    }

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    camera_view(&cam, &basis, 10.0f, 0.0f, 20.0f, &vx, &vy, &vz);
    expect(near(vx, 10.0f, 0.02f) && near(vy, 0.0f, 0.02f) && near(vz, 20.0f, 0.02f),
           "yaw 0 did not put +X on the right and +Z in front");
}

static const Flat *find_flat(const World *world, int file, int image)
{
    int i;

    for (i = 0; i < world_flat_count(world); i++) {
        const Flat *fl;

        fl = world_flat(world, i);
        if (fl && (int)fl->file == file && (int)fl->image == image) {
            return fl;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *root;
    World *world;
    int models;
    int tris;
    int images;
    int pix;
    const Image *shot;
    int i;
    df_u32 bytes;

    root = (argc > 1) ? argv[1] : "daggerfall/DAGGER/ARENA2";
    mem_init();
    fs_set_root(root);
    check_rotation();
    check_fly();

    world = world_load();
    if (!world) {
        fprintf(stderr, "fail: %s\n", world_error());
        return 1;
    }
    models = world_model_count(world);
    tris = world_triangle_count(world);
    images = world_image_count(world);
    printf("block %s\n", world_block_name());
    printf("models %d\n", models);
    printf("triangles %d\n", tris);
    printf("flats %d\n", world_flat_count(world));
    printf("editor skipped %d\n", world_editor_count(world));
    printf("textures %d\n", images);
    printf("uv failures %d\n", arch3d_uv_failures());
    expect(models == 1, "chamber is not one model");
    expect(tris == 12, "triangle count");
    expect(world_flat_count(world) == 2, "flat count");
    expect(world_editor_count(world) == 21, "editor marker count");
    expect(images > 0, "no textures");
    {
        const Instance *room;
        const Flat *torch;
        float sx, sy, sz, syaw, spitch;

        room = world_instance(world, 0);
        expect(room && room->mesh && room->mesh->id == 62022u, "room is not model 62022");
        expect(room && near(room->x, 896.0f, 0.01f) && near(room->y, -512.0f, 0.01f) &&
               near(room->z, 1152.0f, 0.01f),
               "room is not at 896 -512 1152");
        torch = find_flat(world, 210, 6);
        expect(torch && near(torch->w, 16.0f, 0.01f) && near(torch->h, 94.0f, 0.01f),
               "torch 210/6 is not 16 by 94");
        world_start(world, &sx, &sy, &sz, &syaw, &spitch);
        expect(near(sx, 768.0f, 0.1f) && near(sy, -610.0f, 0.1f) && near(sz, 1152.0f, 0.1f),
               "start is not inside the room");
        expect(near(syaw, 0.714f, 0.001f) && near(spitch, 0.28f, 0.001f),
               "start look is not aimed between the torches");
    }

    shot = 0;
    bytes = 0;
    for (i = 0; i < images; i++) {
        const Image *img;
        df_u32 n;

        img = world_image(world, i);
        expect(img->pot_w == img->pot_h && img->pot_w >= 1, "upload is not square");
        expect(img->span_w >= 1 && img->span_h >= 1 &&
               img->span_w <= img->pot_w && img->span_h <= img->pot_h &&
               (img->pot_w % img->span_w) == 0 && (img->pot_h % img->span_h) == 0,
               "texture span does not tile the square");
        if (img->width == 64 && img->height == 32) {
            expect(img->pot_w == 64 && img->span_w == 64 && img->span_h == 32,
                   "wall 64x32 was not tiled into 64x64");
        }
        if (img->width == 16 && img->height == 94) {
            expect(img->pot_w == 128 && img->span_w == 16 && img->span_h == 128,
                   "torch 16x94 was not tiled into 128x128");
        }
        n = (df_u32)img->pot_w * (df_u32)img->pot_h;
        if (world_image_cutout(world, i)) {
            n *= 2;
        }
        bytes += n;
        if (!shot && unique_indices(img) > 1) {
            shot = img;
        }
    }
    printf("tmu bytes %u\n", bytes);
    expect(bytes > 0 && bytes < 1500000, "texture bytes do not fit a 2MB TMU");
    expect(shot != 0, "no textured image");
    if (shot) {
        pix = unique_indices(shot);
        printf("tex0 %dx%d unique %d pot %dx%d span %dx%d\n",
               shot->width, shot->height, pix, shot->pot_w, shot->pot_h,
               shot->span_w, shot->span_h);
        expect(write_ppm(shot, world_palette(world), "build/tex0.ppm"), "ppm write failed");
    }
    check_view(world);

    if (g_fail) {
        fprintf(stderr, "hostcheck failed\n");
        return 1;
    }
    printf("hostcheck ok\n");
    return 0;
}
