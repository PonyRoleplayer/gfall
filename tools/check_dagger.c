/*
 * Daggerfall content tests. Privateer's Hold: the placed blocks, the
 * start marker, a torch sprite, and the fly frame. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>

#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/player.h"
#include "game/dagger/world.h"

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

static void check_fly(void)
{
    Camera cam;
    Input in;
    Basis basis;

    mem_zero(&in, sizeof(in));
    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    check_expect(check_near(basis.fx, 0.0f, 0.02f) && check_near(basis.fz, 1.0f, 0.02f),
           "yaw 0 forward is not +Z");
    check_expect(check_near(basis.rx, 1.0f, 0.02f) && check_near(basis.rz, 0.0f, 0.02f),
           "yaw 0 right is not +X");

    in.move_f = 1;
    player_tick(&cam, &in);
    check_expect(cam.z > 1.0f && check_near(cam.x, 0.0f, 0.02f), "W did not move toward +Z");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_r = 1;
    player_tick(&cam, &in);
    check_expect(cam.x > 1.0f && check_near(cam.z, 0.0f, 0.02f), "D did not move toward +X");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_y = 1;
    player_tick(&cam, &in);
    check_expect(cam.y > 1.0f, "Q did not raise Y");

    camera_set(&cam, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_y = -1;
    player_tick(&cam, &in);
    check_expect(cam.y < 10.0f, "E did not lower Y");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 1.5707963f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.move_f = 1;
    player_tick(&cam, &in);
    check_expect(cam.x > 1.0f, "W after a right turn did not move toward +X");

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    mem_zero(&in, sizeof(in));
    in.look_dx = 100;
    player_tick(&cam, &in);
    check_expect(cam.yaw > 0.2f, "mouse right did not increase yaw");
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
    check_expect(found_floor, "no constant-Y floor");
    check_expect(found_wall, "no upright wall");
    printf("floor y %.2f at %.1f %.1f\n", floor_y, floor_x, floor_z);
    printf("wall axis %s  low y %.2f  high y %.2f\n",
           wall_axis == 0 ? "X" : "Z", wall_y0, wall_y1);

    world_bounds(world, &minx, &miny, &minz, &maxx, &maxy, &maxz);
    printf("bounds %.1f..%.1f  %.1f..%.1f  %.1f..%.1f\n",
           minx, maxx, miny, maxy, minz, maxz);
    check_expect(maxy > miny, "vertical extent is empty");

    camera_set(&cam, floor_x, floor_y, floor_z - 20.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    camera_view(&cam, &basis, floor_x, floor_y, floor_z + 30.0f, &vx, &vy, &vz);
    check_expect(camera_project(vx, vy, vz, &sx0, &sy0, &oow, &ooz), "level point is behind the camera");
    camera_view(&cam, &basis, floor_x, floor_y + 40.0f, floor_z + 30.0f, &vx, &vy, &vz);
    check_expect(camera_project(vx, vy, vz, &sx1, &sy1, &oow, &ooz), "raised point is behind the camera");
    check_expect(sy1 > sy0 + 5.0f, "higher world Y is not higher on screen");

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
        check_expect(camera_project(vx, vy, vz, &sx0, &sy0, &oow, &ooz), "wall bottom is behind the camera");
        camera_view(&cam, &basis, wall_x1, wall_y1, wall_z1, &vx, &vy, &vz);
        check_expect(camera_project(vx, vy, vz, &sx1, &sy1, &oow, &ooz), "wall top is behind the camera");
        check_expect(sy1 > sy0 + 5.0f, "wall top is not above the wall bottom");
    }

    camera_set(&cam, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    camera_basis(&cam, &basis);
    camera_view(&cam, &basis, 10.0f, 0.0f, 20.0f, &vx, &vy, &vz);
    check_expect(check_near(vx, 10.0f, 0.02f) && check_near(vy, 0.0f, 0.02f) && check_near(vz, 20.0f, 0.02f),
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

int check_dagger(const char *root_in)
{
    World *world;
    int models;
    int tris;
    int images;
    int pix;
    const Image *shot;
    int i;
    df_u32 bytes;

    const char *root;

    root = root_in ? root_in : "daggerfall/DAGGER/ARENA2";
    mem_init();
    vfs_mount(root);
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
    check_expect(models == 365, "model count");
    check_expect(tris == 9075, "triangle count");
    check_expect(world_flat_count(world) == 130, "flat count");
    check_expect(world_editor_count(world) == 56, "editor marker count");
    check_expect(images > 0, "no textures");
    {
        int nfiles;
        int f;
        int saw_crypt;
        int saw_cave;

        /*
         * Location 50050 is woodlands. Key 122 resolves to TEXTURE.319,
         * and cave set 147 records that exist in 347 move there.
         */
        saw_crypt = 0;
        saw_cave = 0;
        nfiles = world_texture_file_count(world);
        for (f = 0; f < nfiles; f++) {
            int file;

            file = world_texture_file(world, f);
            if (file == 319) {
                saw_crypt = 1;
            }
            if (file == 347) {
                saw_cave = 1;
            }
        }
        check_expect(saw_crypt, "woodland crypt set was not selected");
        check_expect(saw_cave, "woodland cave set was not selected");
    }
    {
        const Flat *torch;
        float sx, sy, sz, syaw, spitch;
        float minx, miny, minz, maxx, maxy, maxz;
        int outside;

        torch = find_flat(world, 210, 6);
        check_expect(torch && check_near(torch->w, 16.0f, 0.01f) && check_near(torch->h, 94.0f, 0.01f),
               "torch 210/6 is not 16 by 94");
        world_start(world, &sx, &sy, &sz, &syaw, &spitch);
        printf("start %.2f %.2f %.2f yaw %.3f pitch %.3f\n", sx, sy, sz, syaw, spitch);
        check_expect(check_near(sx, 1135.0f, 0.1f) && check_near(sy, 1559.0f, 0.1f) &&
               check_near(sz, 496.0f, 0.1f),
               "start is not the marker in S0000999");
        check_expect(check_near(syaw, 0.0f, 0.001f) && check_near(spitch, 0.0f, 0.001f),
               "start look is not level along +Z");
        world_bounds(world, &minx, &miny, &minz, &maxx, &maxy, &maxz);
        check_expect(minx < -2000.0f && maxx > 4000.0f && minz < -2000.0f && maxz > 4000.0f,
               "border blocks are not around the hold");
        outside = 0;
        for (i = 0; i < models; i++) {
            const Instance *in;

            in = world_instance(world, i);
            if (in && (in->x < -1500.0f || in->x > 2500.0f || in->z < -1500.0f || in->z > 2500.0f)) {
                outside++;
            }
        }
        check_expect(outside > 40, "border models were not shifted onto their tiles");
    }

    shot = 0;
    bytes = 0;
    for (i = 0; i < images; i++) {
        const Image *img;
        df_u32 n;

        img = world_image(world, i);
        check_expect(img->pot_w == img->pot_h && img->pot_w >= 1, "upload is not square");
        check_expect(img->span_w >= 1 && img->span_h >= 1 &&
               img->span_w <= img->pot_w && img->span_h <= img->pot_h &&
               (img->pot_w % img->span_w) == 0 && (img->pot_h % img->span_h) == 0,
               "texture span does not tile the square");
        if (img->width == 64 && img->height == 32) {
            check_expect(img->pot_w == 64 && img->span_w == 64 && img->span_h == 32,
                   "wall 64x32 was not tiled into 64x64");
        }
        if (img->width == 16 && img->height == 94) {
            check_expect(img->pot_w == 128 && img->span_w == 16 && img->span_h == 128,
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
    check_expect(bytes > 0 && bytes < 1500000, "texture bytes do not fit a 2MB TMU");
    check_expect(shot != 0, "no textured image");
    if (shot) {
        pix = unique_indices(shot);
        printf("tex0 %dx%d unique %d pot %dx%d span %dx%d\n",
               shot->width, shot->height, pix, shot->pot_w, shot->pot_h,
               shot->span_w, shot->span_h);
        check_expect(write_ppm(shot, world_palette(world), "build/tex0.ppm"), "ppm write failed");
    }
    check_view(world);

    return check_failed() ? 1 : 0;
}
