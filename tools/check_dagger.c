/*
 * Daggerfall content tests. Privateer's Hold: the placed blocks, the
 * start marker, a torch sprite, and the fly frame. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>

#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/bsa.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/player.h"
#include "game/dagger/texture.h"
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

/* This ARENA2 tree. MAPDITEM.002 and the other empty region files are four bytes. */
#define LEVEL_COUNT 4232

static DunRef g_region61;
static int g_have_region61;
/* Daggerfall's castle, location 50027: the interior culling case. */
static DunRef g_castle;
static int g_have_castle;

static void check_levels(void)
{
    DunRef *rows;
    Dungeon from_list;
    Dungeon hold;
    int n;
    int i;
    int found;
    int blocks;

    g_have_region61 = 0;
    g_have_castle = 0;
    rows = (DunRef *)mem_alloc((df_u32)8192 * (df_u32)sizeof(DunRef));
    check_expect(rows != 0, "dungeon list allocation");
    if (!rows) {
        return;
    }
    n = dungeon_list(rows, 8192);
    printf("dungeons %d\n", n);
    check_expect(n == LEVEL_COUNT, "dungeon count");
    found = -1;
    for (i = 0; i < n && i < 8192; i++) {
        if (rows[i].region == 61) {
            g_region61 = rows[i];
            g_have_region61 = 1;
        }
        if (rows[i].region == 17 && df_eq(rows[i].name, "Privateer's Hold")) {
            found = i;
        }
        if (rows[i].location_id == 50027) {
            g_castle = rows[i];
            g_have_castle = 1;
        }
    }
    check_expect(found >= 0 && rows[found].nblocks == 5 && rows[found].location_id == 50050
           && rows[found].outdoor == 0,
           "Privateer's Hold is not region 17 record with 5 blocks");
    if (found < 0) {
        return;
    }
    check_expect(dungeon_load_at(rows[found].region, rows[found].index, &from_list),
           dungeon_error());
    check_expect(dungeon_load(&hold), dungeon_error());
    check_expect(from_list.nblocks == hold.nblocks && from_list.location_id == hold.location_id
           && from_list.climate == hold.climate,
           "listed hold does not match dungeon_load");
    blocks = from_list.nblocks;
    if (hold.nblocks < blocks) {
        blocks = hold.nblocks;
    }
    for (i = 0; i < blocks; i++) {
        check_expect(df_eq(from_list.block[i].name, hold.block[i].name)
               && from_list.block[i].x == hold.block[i].x
               && from_list.block[i].z == hold.block[i].z
               && from_list.block[i].start == hold.block[i].start,
               "hold block list mismatch");
    }
}

/*
 * texture_smooth on a 3 x 2 wall stretched to 4 x 2: black, white,
 * black across. Texel centres land at -0.125, 0.625, 1.375 and 2.125
 * source texels, wrapping, so the ends stay black and the middle two
 * are 5/8 white: nearest the grey entry. Rows do not stretch and are
 * repeated to fill the 4 x 4 square.
 */
static void check_smooth(void)
{
    df_u8 pr[256];
    df_u8 pg[256];
    df_u8 pb[256];
    df_u8 src[6];
    df_u8 pot[16];
    Image img;
    int i;
    int ok;

    for (i = 0; i < 256; i++) {
        pr[i] = 255;
        pg[i] = 0;
        pb[i] = (df_u8)i;
    }
    pr[1] = 0;   pg[1] = 0;   pb[1] = 0;
    pr[2] = 255; pg[2] = 255; pb[2] = 255;
    pr[3] = 128; pg[3] = 128; pb[3] = 128;
    src[0] = 1; src[1] = 2; src[2] = 1;
    src[3] = 1; src[4] = 2; src[5] = 1;
    img.width = 3;
    img.height = 2;
    img.pot_w = 4;
    img.pot_h = 4;
    img.span_w = 4;
    img.span_h = 2;
    img.xscale = 0;
    img.yscale = 0;
    img.index = src;
    img.pot = pot;
    for (i = 0; i < 16; i++) {
        pot[i] = 0;
    }
    texture_smooth(&img, pr, pg, pb);
    ok = 1;
    for (i = 0; i < 16; i++) {
        static const df_u8 want[4] = { 1, 3, 3, 1 };

        ok = ok && pot[i] == want[i & 3];
    }
    check_expect(ok && img.span_w == 4 && img.span_h == 2,
                 "texture_smooth did not blend a stretched wall");
}

/*
 * Model 457 has door-jamb faces 2 units wide. The old loader dropped a
 * real corner from three of them, by angle, and lost 12 square units
 * of each: 3281346 in all. Every corner kept, the faces' triangles add
 * up to the file's polygons, 3281382.
 */
static void check_jambs(void)
{
    Bsa *bsa;
    const BsaRec *rec;
    Mesh *mesh;
    double area;
    int t;

    bsa = bsa_open("ARCH3D.BSA");
    rec = bsa ? bsa_find_id(bsa, 457) : 0;
    mesh = rec ? arch3d_load(bsa_read(bsa, rec), rec->size, 457) : 0;
    check_expect(mesh != 0, "model 457 did not load");
    if (!mesh) {
        return;
    }
    area = 0.0;
    for (t = 0; t < mesh->ntris; t++) {
        const Tri *q;
        double ax, ay, az, bx, by, bz, cx, cy, cz;

        q = &mesh->tris[t];
        ax = q->x[1] - q->x[0];
        ay = q->y[1] - q->y[0];
        az = q->z[1] - q->z[0];
        bx = q->x[2] - q->x[0];
        by = q->y[2] - q->y[0];
        bz = q->z[2] - q->z[0];
        cx = ay * bz - az * by;
        cy = az * bx - ax * bz;
        cz = ax * by - ay * bx;
        cx = cx * cx + cy * cy + cz * cz;
        /* x87, as arch3d does: hostcheck links no libm. */
        __asm__ __volatile__("fsqrt" : "=t"(cx) : "0"(cx));
        area += cx * 0.5;
    }
    printf("model 457 faces %d triangles %d area %.0f\n", mesh->nfaces, mesh->ntris, area);
    check_expect(mesh->nfaces == 498 && area > 3281380.0 && area < 3281384.0,
                 "model 457's door jambs are not whole");
}

/*
 * ARCH3D v2.5 counts a point reference in 4-byte steps. Read as an
 * index, model 41500's references ran past its 11 points into the rest
 * of the record. Its farthest point has to be its header radius, 246.
 */
static void check_v25(void)
{
    Bsa *bsa;
    const BsaRec *rec;
    const df_u8 *bytes;
    Mesh *mesh;
    double far2;
    double radius;
    int t;
    int k;

    bsa = bsa_open("ARCH3D.BSA");
    rec = bsa ? bsa_find_id(bsa, 41500) : 0;
    bytes = rec ? bsa_read(bsa, rec) : 0;
    mesh = bytes ? arch3d_load(bytes, rec->size, 41500) : 0;
    check_expect(mesh != 0, "model 41500 did not load");
    if (!mesh) {
        return;
    }
    far2 = 0.0;
    for (t = 0; t < mesh->ntris; t++) {
        for (k = 0; k < 3; k++) {
            double d;

            d = (double)mesh->tris[t].x[k] * mesh->tris[t].x[k]
                + (double)mesh->tris[t].y[k] * mesh->tris[t].y[k]
                + (double)mesh->tris[t].z[k] * mesh->tris[t].z[k];
            if (d > far2) {
                far2 = d;
            }
        }
    }
    radius = (double)df_ri32(bytes + 12) / 256.0;
    printf("model 41500 (v2.5) faces %d farthest^2 %.0f radius^2 %.0f\n",
           mesh->nfaces, far2, radius * radius);
    check_expect(mesh->nfaces == 8 && far2 <= radius * radius * 1.01 && far2 >= radius * radius * 0.98,
                 "v2.5 model 41500 does not reach its header radius");
    check_expect(arch3d_bad_refs() == 0, "an ARCH3D face referred past its point table");
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
    int padded;
    df_u32 bytes;

    const char *root;

    root = root_in ? root_in : "daggerfall/DAGGER/ARENA2";
    mem_init();
    vfs_mount(root);
    check_smooth();
    check_jambs();
    check_v25();
    check_fly();
    check_levels();

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
    /*
     * 9075 before arch3d kept every real corner and welded T-junctions:
     * the extra triangles are the corners of thin jambs it used to drop,
     * and the corners it inserts where faces meet partway along an edge.
     */
    check_expect(tris == 9315, "triangle count");
    /* RDB object type 2: 71 lights in the hold's five blocks. See docs/lighting.md. */
    printf("lights %d\n", world_light_count(world));
    check_expect(world_light_count(world) == 71, "light count");
    {
        Level level;
        Camera cam;
        Basis basis;
        int order[512];
        int vis;
        int front;
        int faces;
        int mi;
        float x, y, z, yaw, pitch;

        world_as_level(world, &level);
        world_start(world, &x, &y, &z, &yaw, &pitch);
        camera_set(&cam, x, y, z, yaw, pitch);
        vis = scene_visible_models(&level.scene, &cam, order, 512);
        check_expect(vis > 0 && vis < models, "hold cull kept every model");
        /* Occlusion only ever removes from the frustum's list. */
        {
            int frustum;
            int occ_faces;
            int occ_hidden;

            scene_occ_stats(&occ_faces, &occ_hidden);
            scene_set_occlude(0);
            frustum = scene_visible_models(&level.scene, &cam, order, 512);
            scene_set_occlude(1);
            printf("hold visible %d frustum %d occluders %d\n", vis, frustum, occ_faces);
            check_expect(vis <= frustum && frustum - vis == occ_hidden,
                         "hold occlusion added models");
            vis = scene_visible_models(&level.scene, &cam, order, 512);
        }
        camera_basis(&cam, &basis);
        front = 0;
        faces = 0;
        for (mi = 0; mi < models; mi++) {
            const Instance *in;
            int f;

            in = world_instance(world, mi);
            if (!in || !in->mesh || !in->mesh->faces) {
                continue;
            }
            for (f = 0; f < in->mesh->nfaces; f++) {
                const Face *face;
                const Tri *tri;

                face = &in->mesh->faces[f];
                if (face->ntris < 2 || face->first < 0) {
                    continue;
                }
                tri = &in->mesh->tris[face->first];
                faces++;
                if (instance_faces(in, &cam, &basis,
                                   face->nx, face->ny, face->nz,
                                   tri->x[0], tri->y[0], tri->z[0])) {
                    front++;
                }
            }
        }
        check_expect(faces > 0 && front > 0 && front < faces,
                     "hold faces did not split into front and back");
    }
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
    padded = 0;
    for (i = 0; i < images; i++) {
        const Image *img;
        df_u32 n;

        img = world_image(world, i);
        check_expect(img->pot_w == img->pot_h && img->pot_w >= 1, "upload is not square");
        check_expect(img->span_w >= 1 && img->span_h >= 1 &&
               img->span_w <= img->pot_w && img->span_h <= img->pot_h,
               "texture span is outside the square");
        /*
         * A wall repeats, so its copies must tile the square. A sprite
         * is its own size in the corner, padded clear: nothing tiles.
         */
        if (world_image_cutout(world, i) && img->span_w == img->width
            && img->span_h == img->height) {
            padded++;
        } else {
            check_expect((img->pot_w % img->span_w) == 0 && (img->pot_h % img->span_h) == 0,
                   "wall texture span does not tile the square");
        }
        if (img->width == 64 && img->height == 32) {
            check_expect(img->pot_w == 64 && img->span_w == 64 && img->span_h == 32,
                   "wall 64x32 was not tiled into 64x64");
        }
        if (img->width == 16 && img->height == 94) {
            check_expect(img->pot_w == 128 && img->span_w == 16 && img->span_h == 94
                         && img->pot[0] == img->index[0] && img->pot[127] == 0
                         && img->pot[127 * 128] == 0,
                   "torch 16x94 was not padded at its own size into 128x128");
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
    printf("tmu bytes %u, sprites padded %d\n", bytes, padded);
    check_expect(padded == 33, "every sprite in the hold should be padded, none shared with a wall");
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

    {
        Dungeon one;
        World *other;
        float sx, sy, sz, syaw, spitch;
        float minx, miny, minz, maxx, maxy, maxz;

        check_expect(g_have_region61, "region 61 has no dungeon");
        if (g_have_region61 && dungeon_load_at(g_region61.region, g_region61.index, &one)) {
            printf("other %s blocks %d\n", one.title, one.nblocks);
            other = world_load_dungeon(&one);
            if (!other) {
                fprintf(stderr, "fail: %s\n", world_error());
            }
            check_expect(other && world_model_count(other) > 0, "region 61 did not load");
            if (other) {
                world_start(other, &sx, &sy, &sz, &syaw, &spitch);
                world_bounds(other, &minx, &miny, &minz, &maxx, &maxy, &maxz);
                check_expect(sx >= minx && sx <= maxx && sy >= miny && sy <= maxy
                       && sz >= minz && sz <= maxz,
                       "region 61 start is outside the dungeon");
            }
        } else if (g_have_region61) {
            check_expect(0, dungeon_error());
        }
    }

    /*
     * Inside Daggerfall's castle, facing a closed door down a hall. The
     * frustum keeps 38 models at the default far clip; 4 have a pixel
     * on the glass. Occlusion has to get near that, with walls, floor
     * and the door leaves covering each other's seams.
     */
    {
        Dungeon castle;
        World *inside;

        check_expect(g_have_castle, "Daggerfall castle is not in the dungeon list");
        if (g_have_castle && dungeon_load_at(g_castle.region, g_castle.index, &castle)) {
            inside = world_load_dungeon(&castle);
            check_expect(inside != 0, "Daggerfall castle did not load");
            if (inside) {
                Level level;
                Camera cam;
                int order[2048];
                int vis;
                int frustum;

                world_as_level(inside, &level);
                scene_set_limits(DF_FAR, 1);
                camera_set(&cam, 2002.0f, 1081.0f, 1022.0f,
                           268.0f * (DF_TAU / 360.0f), 2.0f * (DF_TAU / 360.0f));
                scene_set_occlude(0);
                frustum = scene_visible_models(&level.scene, &cam, order, 2048);
                scene_set_occlude(1);
                vis = scene_visible_models(&level.scene, &cam, order, 2048);
                printf("castle door visible %d frustum %d\n", vis, frustum);
                check_expect(frustum > 30 && vis <= 8, "castle door view kept the rooms behind it");
            }
        } else if (g_have_castle) {
            check_expect(0, dungeon_error());
        }
    }

    {
        DunRef *places;
        Dungeon ext;
        World *town;
        int n;
        int i;
        int dagger;
        int farm;
        float sx, sy, sz, syaw, spitch;
        float minx, miny, minz, maxx, maxy, maxz;

        places = (DunRef *)mem_alloc((df_u32)16384 * (df_u32)sizeof(DunRef));
        check_expect(places != 0, "outdoor list allocation");
        if (places) {
            n = exterior_list(places, 16384);
            printf("outdoors %d\n", n);
            check_expect(n == 15251, "outdoor count");
            dagger = -1;
            farm = -1;
            for (i = 0; i < n && i < 16384; i++) {
                if (places[i].region != 17) {
                    continue;
                }
                if (df_eq(places[i].name, "Daggerfall")) {
                    dagger = i;
                }
                if (df_eq(places[i].name, "The Ashwing Farmstead")) {
                    farm = i;
                }
            }
            check_expect(dagger >= 0 && places[dagger].nblocks == 64 && places[dagger].outdoor,
                   "Daggerfall is not an 8 by 8 outdoor");
            if (dagger >= 0 && exterior_load_at(places[dagger].region, places[dagger].index, &ext)) {
                check_expect(ext.nblocks == 64 && ext.outdoor
                       && df_eq(ext.block[0].name, "WALLAA02.RMB")
                       && ext.block[0].x == 0 && ext.block[0].z == 0,
                       "Daggerfall block 0 is not WALLAA02.RMB");
            } else if (dagger >= 0) {
                check_expect(0, dungeon_error());
            }
            check_expect(farm >= 0 && places[farm].nblocks == 1, "Ashwing Farmstead missing");
            if (farm >= 0 && exterior_load_at(places[farm].region, places[farm].index, &ext)) {
                printf("outdoor %s blocks %d\n", ext.title, ext.nblocks);
                town = world_load_dungeon(&ext);
                if (!town) {
                    fprintf(stderr, "fail: %s\n", world_error());
                }
                check_expect(town && world_model_count(town) > 0, "outdoor did not load");
                if (town) {
                    world_start(town, &sx, &sy, &sz, &syaw, &spitch);
                    world_bounds(town, &minx, &miny, &minz, &maxx, &maxy, &maxz);
                    check_expect(sx >= minx && sx <= maxx && sz >= minz && sz <= maxz
                           && sy >= maxy,
                           "outdoor start is not above the place");
                }
            } else if (farm >= 0) {
                check_expect(0, dungeon_error());
            }
        }
    }

    return check_failed() ? 1 : 0;
}
