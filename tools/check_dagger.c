/*
 * Daggerfall content tests. Privateer's Hold: the placed blocks, the
 * start marker, a torch sprite, and the fly frame. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>
#include <string.h>

#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/bsa.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/player.h"
#include "game/dagger/skyfile.h"
#include "game/dagger/climate.h"
#include "game/dagger/forecast.h"
#include "engine/scene/weather.h"
#include "engine/scene/clock.h"
#include "engine/scene/collide.h"
#include "engine/scene/light.h"
#include "game/dagger/texture.h"
#include "game/dagger/world.h"
#include "game/dagger/doors.h"
#include "engine/scene/interact.h"
#include "game/dagger/hmi.h"
#include "game/dagger/sndfile.h"
#include "game/dagger/sounds.h"
#include "engine/assets/loader.h"
#include "engine/sound/ambience.h"
#include "engine/sound/mixer.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"

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

/*
 * Flats that stand on the floor under them: their bottom (centre less
 * half the height) within a few units above it. Dungeon flats are placed
 * by their centre and town flats by their base; lifting a dungeon flat
 * as if by its base floated every one half its height up.
 */
static int flats_on_floor(const World *world, int *over_floor)
{
    Level lv;
    int i;
    int on;

    world_as_level(world, &lv);
    on = 0;
    *over_floor = 0;
    if (!collide_use(&lv.scene)) {
        return 0;
    }
    collide_set_shape(1.0f, 1.0f);
    for (i = 0; i < lv.scene.nsprites; i++) {
        const Flat *f;
        float bottom;
        float fy;

        f = &lv.scene.sprites[i];
        bottom = f->y - f->h * 0.5f;
        collide_gather(f->x - 1.0f, bottom - 400.0f, f->z - 1.0f, f->x + 1.0f, bottom + 2.0f, f->z + 1.0f);
        if (!collide_floor(f->x, f->z, bottom + 1.0f, bottom - 400.0f, &fy)) {
            continue;
        }
        (*over_floor)++;
        if (bottom - fy > -1.0f && bottom - fy < 8.0f) {
            on++;
        }
    }
    return on;
}

/*
 * SKY##.DAT and NITE: which file a climate draws, the frame by the
 * hour, and how the halves land on the circle (docs/daylight.md).
 */
static void put_u32(FILE *f, df_u32 v)
{
    fputc((int)(v & 255u), f);
    fputc((int)((v >> 8) & 255u), f);
    fputc((int)((v >> 16) & 255u), f);
    fputc((int)((v >> 24) & 255u), f);
}

/* 16-bit stereo at 11025 Hz, to listen to what a test mixed. */
static void write_wav(const char *path, const df_i16 *buf, int frames)
{
    FILE *f;
    int i;

    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fwrite("RIFF", 1, 4, f);
    put_u32(f, 36u + (df_u32)frames * 4u);
    fwrite("WAVEfmt ", 1, 8, f);
    put_u32(f, 16);
    put_u32(f, 1u | (2u << 16));
    put_u32(f, 11025);
    put_u32(f, 11025u * 4u);
    put_u32(f, 4u | (16u << 16));
    fwrite("data", 1, 4, f);
    put_u32(f, (df_u32)frames * 4u);
    for (i = 0; i < frames * 2; i++) {
        fputc(buf[i] & 255, f);
        fputc((buf[i] >> 8) & 255, f);
    }
    fclose(f);
}

/*
 * Run a place's sound for secs at 20 frames a second from cam, mixing
 * into out (frames of 11025 Hz), as draw_frame would. strokes: a
 * lightning stroke every this many frames, 0 for none.
 */
static int run_sound(const Level *level, const Camera *cam, float night, int strokes,
                     float secs, df_i16 *out, int cap)
{
    Weather w;
    int frames;
    int k;
    int n;

    frames = 0;
    n = df_iround(secs * 20.0f);
    for (k = 0; k < n; k++) {
        int per;

        weather_state(&w);
        ambience_frame(level, cam, &w, night, strokes > 0 && k % strokes == 1, 0.05f);
        per = 551 + (k % 20 == 0 ? 5 : 0);
        if (frames + per > cap) {
            break;
        }
        mix_clear(out + frames * 2, per);
        sound_mix(out + frames * 2, per);
        frames += per;
    }
    return frames;
}

static double rms_of(const df_i16 *buf, int frames)
{
    double e;
    int i;

    e = 0.0;
    for (i = 0; i < frames * 2; i++) {
        e += (double)buf[i] * buf[i];
    }
    if (frames <= 0) {
        return 0.0;
    }
    e /= frames * 2.0;
    {
        /* Newton's square root: the host build links no libm. */
        double r;
        int k;

        r = e > 1.0 ? e : 1.0;
        for (k = 0; k < 60; k++) {
            r = 0.5 * (r + e / r);
        }
        return r;
    }
}

/* DAGGER.SND and MIDI.BSA as the game reads them (docs/sound.md). */
static void check_sound_files(void)
{
    const SoundClip *rain;
    int i;
    int ok;
    int most_events;
    int most_notes;
    int stolen_any;
    static df_u32 words[(MUSIC_EVENTS + MUSIC_EVENTS / 8 + 64) * 3];

    printf("sound clips %d\n", sndfile_count());
    check_expect(sndfile_count() == 459, "DAGGER.SND does not have 459 records");
    rain = sndfile_clip(SND_RAIN);
    check_expect(rain && rain->frames == 31346 && rain->rate == 11025 && rain->data[0] == -3,
                 "the rain clip (389) is not 31346 signed samples at 11025 Hz");
    check_expect(sndfile_clip(SND_RAIN) == rain, "a clip loaded twice");
    check_expect(sndfile_by_id(385) == SND_RAIN && sndfile_clip(5) == 0,
                 "record id 385 is not position 389, or the empty record at 5 gave a clip");
    printf("songs %d\n", hmi_count());
    check_expect(hmi_count() == 131, "MIDI.BSA does not have 131 songs");
    ok = 1;
    most_events = 0;
    most_notes = 0;
    stolen_any = 0;
    for (i = 0; i < hmi_count(); i++) {
        Song song;
        int n;
        int stolen;
        int peak;

        mem_zero(&song, sizeof(song));
        if (!hmi_load(hmi_name(i), 0, &song) || song.division != 480 || song.length == 0) {
            printf("song %s did not read\n", hmi_name(i));
            ok = 0;
            continue;
        }
        n = music_render(&song, words, (int)(sizeof(words) / 4), MUSIC_VOICES_MAX, 100, &stolen, &peak);
        if (n == 0) {
            ok = 0;
        }
        /* No song waits long after its music before it loops (SUNNYDAY's stray tail). */
        {
            df_u32 note;
            int k;

            note = 0;
            for (k = 0; k < song.count; k++) {
                if ((song.events[k].msg & 0xf0u) == 0x80u && song.events[k].tick > note) {
                    note = song.events[k].tick;
                }
            }
            if (song.length > note + 960u) {
                printf("song %s ends %u ticks after its last note\n", hmi_name(i),
                       (unsigned int)(song.length - note));
                ok = 0;
            }
            if (df_eq(hmi_name(i), "SUNNYDAY.HMI")) {
                printf("song SUNNYDAY.HMI %.1f s\n", (double)song.length / 120.0);
                check_expect(song.length / 120u >= 55u && song.length / 120u <= 58u,
                             "SUNNYDAY.HMI is not about 57 seconds");
            }
        }
        most_events = song.count > most_events ? song.count : most_events;
        most_notes = peak > most_notes ? peak : most_notes;
        stolen_any += stolen;
        if (df_eq(hmi_name(i), "D1.HMI")) {
            printf("song D1.HMI %d events, %.1f s, %d notes at most\n", song.count,
                   (double)song.length / 120.0, peak);
            check_expect(song.length / 120u >= 222u && song.length / 120u <= 226u,
                         "D1.HMI is not about 224 seconds");
        }
    }
    printf("songs: most events %d, most notes at once %d, cut by 32 voices %d\n",
           most_events, most_notes, stolen_any);
    check_expect(ok, "a song of MIDI.BSA did not read or render");
    check_expect(most_notes <= 32 && stolen_any == 0 && most_events < MUSIC_EVENTS,
                 "a Daggerfall song needs more than 32 voices or MUSIC_EVENTS");
    {
        Song fm;

        mem_zero(&fm, sizeof(fm));
        check_expect(hmi_load("D1FM.HMI", 1, &fm) && fm.count > 0, "the FM tracks of D1FM.HMI did not read");
    }
}

/* The hold: torches loop their fire, the dungeon's noises come now and then. */
static void check_sound_hold(const World *world)
{
    static df_i16 buf[11025 * 6 * 2];
    const Soundscape *sc;
    const Emitter *torch;
    Level hl;
    Camera cam;
    int heard;
    int loops;
    int frames;
    int k;
    const char *name;

    world_as_level(world, &hl);
    sc = world_soundscape(world);
    printf("hold emitters %d\n", world_emitter_count(world));
    check_expect(world_emitter_count(world) > 10 && hl.scene.nemitters == world_emitter_count(world)
                 && hl.sound == sc, "the hold's torches make no sound");
    check_expect(sc && sc->kind == SCAPE_DUNGEON && sc->naround == 14 && !sc->rain,
                 "the hold's soundscape is not the dungeon's fourteen noises");
    name = sc ? sc->song(sc, -1, 0) : 0;
    {
        Song song;

        mem_zero(&song, sizeof(song));
        check_expect(name && loader_call("song", name, 0, 0, &song), "the hold's song does not load");
    }
    torch = world_emitter_at(world, 0);
    for (k = 0; k < world_emitter_count(world); k++) {
        if (world_emitter_at(world, k)->every == 0.0f) {
            torch = world_emitter_at(world, k);
            break;
        }
    }
    camera_set(&cam, torch->x + 60.0f, torch->y, torch->z, 0.0f, 0.0f);
    sound_set_mix_rate(11025);
    sound_set_on(1);
    music_stop();
    frames = run_sound(&hl, &cam, 0.0f, 0, 5.0f, buf, 11025 * 6);
    ambience_stats(&heard, &loops);
    printf("hold sound: song %s, %d heard, %d loops, %d voices, rms %.0f\n",
           music_name(), heard, loops, sound_busy(), rms_of(buf, frames));
    check_expect(loops >= 1 && rms_of(buf, frames) > 50.0, "beside a torch the fire is silent");
    check_expect(df_eq(music_name(), name), "the hold's song did not start");
    {
        const Emitter *list;
        int playing;

        playing = 0;
        for (k = 0; k < ambience_emitters(&list); k++) {
            playing += ambience_emitter_playing(k);
        }
        check_expect(ambience_emitters(&list) == world_emitter_count(world) && list == hl.scene.emitters
                     && playing == loops && torch->label && df_eq(torch->label, "fire"),
                     "sndlist and sndview do not see the hold's emitters, or the playing torch");
    }
    write_wav("build/snd0.wav", buf, frames);
    camera_set(&cam, torch->x + 5000.0f, torch->y, torch->z, 0.0f, 0.0f);
    run_sound(&hl, &cam, 0.0f, 0, 0.5f, buf, 11025 * 6);
    ambience_stats(&heard, &loops);
    check_expect(loops == 0, "a torch out of reach still holds a voice");
    sound_stop_all();
    music_stop();
}

/* A farm: songs for every weather, the rain and thunder of a storm. */
/* How many of world's usables are of kind and, when what >= 0, lead there. */
static int count_use(const World *world, int kind, int what)
{
    int n;
    int i;

    n = 0;
    for (i = 0; i < world_usable_count(world); i++) {
        const Usable *u;

        u = world_usable(world, i);
        if (u->kind == kind && (what < 0 || u->what == what)) {
            n++;
        }
    }
    return n;
}

/*
 * Stand d units in front of usable i, facing it, and use what the eye
 * meets. The index used, or -1.
 */
static int use_from_front(const Level *level, const World *world, int i, float d)
{
    const Usable *u;
    Camera cam;
    UseHit hit;
    float x, y, z;
    float nx, nz;

    u = world_usable(world, i);
    if (u->model >= 0) {
        Bound b;
        const Instance *in;

        in = world_instance(world, u->model);
        if (!instance_world_box(in, &b)) {
            return -1;
        }
        x = b.cx;
        y = b.cy;
        z = b.cz;
        /* A door model is thin along its own Z or X: face it across the thin side. */
        if (b.ex < b.ez) {
            nx = 1.0f;
            nz = 0.0f;
        } else {
            nx = 0.0f;
            nz = 1.0f;
        }
    } else {
        x = u->cx;
        y = u->cy;
        z = u->cz;
        nx = u->nx;
        nz = u->nz;
    }
    camera_set(&cam, x + nx * d, y, z + nz * d, df_atan2(-nx, -nz), 0.0f);
    if (!interact_aim(&level->scene, &cam, &hit)) {
        camera_set(&cam, x - nx * d, y, z - nz * d, df_atan2(nx, nz), 0.0f);
        if (!interact_aim(&level->scene, &cam, &hit)) {
            return -1;
        }
    }
    interact_use(&level->scene, hit.usable);
    return hit.usable;
}

/*
 * The hold's doors and its lever: what moves, what is locked, the way
 * out, and a door that swings a quarter turn when used.
 */
static void check_doors_hold(const World *world)
{
    Level level;
    int doors, actions, locked, exits;
    int door;
    int lever;
    int i;

    world_as_level(world, &level);
    doors = count_use(world, USE_MOVER, -1);
    actions = 0;
    locked = 0;
    for (i = 0; i < world_mover_count(world); i++) {
        const Mover *m;

        m = world_mover(world, i);
        actions += !m->door;
        locked += m->lock > 0;
    }
    exits = count_use(world, USE_PASSAGE, DOOR_EXIT);
    printf("hold doors: %d to use, %d movers (%d not doors, %d locked), %d ways out\n",
           doors, world_mover_count(world), actions, locked, exits);
    check_expect(world_place(world) == PLACE_DUNGEON, "the hold is not a dungeon");
    check_expect(exits >= 1, "the hold has no way out");
    /* A door that is not locked, used from in front: it swings a quarter turn. */
    door = -1;
    lever = -1;
    for (i = 0; i < world_usable_count(world); i++) {
        const Usable *u;
        const Mover *m;

        u = world_usable(world, i);
        if (u->kind != USE_MOVER) {
            continue;
        }
        m = world_mover(world, u->mover);
        if (m->door && !m->lock && door < 0) {
            door = i;
        }
        if (!m->door && m->next >= 0 && lever < 0) {
            lever = i;
        }
    }
    check_expect(door >= 0, "the hold has no unlocked door to use");
    if (door >= 0) {
        const Usable *u;
        const Mover *m;
        float m0[9];
        int used;
        int k;

        u = world_usable(world, door);
        m = world_mover(world, u->mover);
        for (k = 0; k < 9; k++) {
            m0[k] = m->inst->m[k];
        }
        used = use_from_front(&level, world, door, 64.0f);
        check_expect(used == door, "a door in front of the eye was not the one used");
        mover_step(&level.scene, 0.75f);
        check_expect(m->state == MOVER_FORWARD && m->t > 0.4f && m->t < 0.6f,
                     "a used door is not half open after 0.75 s");
        mover_step(&level.scene, 1.0f);
        /* A quarter turn back about its up: its local X now points where its local Z did. */
        printf("hold door %d: state %d, x axis %.2f %.2f %.2f was %.2f %.2f %.2f\n", door,
               m->state, m->inst->m[0], m->inst->m[3], m->inst->m[6], m0[0], m0[3], m0[6]);
        check_expect(m->state == MOVER_END
                     && check_near(m->inst->m[0], m0[2], 0.01f)
                     && check_near(m->inst->m[6], m0[8], 0.01f),
                     "a used door did not swing a quarter turn");
        interact_use(&level.scene, door);
        mover_step(&level.scene, 2.0f);
        check_expect(m->state == MOVER_START && check_near(m->inst->m[0], m0[0], 0.001f),
                     "a door used again did not shut");
    }
    /* The lever: it starts the chain after it, which moves together. */
    check_expect(lever >= 0, "the hold has no chained action");
    if (lever >= 0) {
        const Usable *u;
        const Mover *m;
        const Mover *n;

        u = world_usable(world, lever);
        m = world_mover(world, u->mover);
        n = world_mover(world, m->next);
        interact_use(&level.scene, lever);
        printf("hold action %d: next %d moving %d, %.2f s, turn %.2f %.2f %.2f\n", lever,
               m->next, n->state == MOVER_FORWARD, n->secs, n->rx, n->ry, n->rz);
        check_expect(n->state == MOVER_FORWARD, "a used action did not start the next in its chain");
        check_expect(!interact_use(&level.scene, lever), "a chain started again while it moved");
        mover_step(&level.scene, 10.0f);
        check_expect(n->state == MOVER_END, "a chained action did not finish");
        /* And back, so the tests after see the hold as it was built. */
        interact_use(&level.scene, lever);
        mover_step(&level.scene, 10.0f);
        check_expect(n->state == MOVER_START, "a chained action used again did not go back");
    }
    /* The way out: a face of the exit model, a door's size, upright, and used from in front. */
    i = world_passage(world, DOOR_EXIT, 0.0f, 0.0f, 0.0f);
    if (i >= 0) {
        const Usable *u;

        u = world_usable(world, i);
        printf("hold exit: at %.0f %.0f %.0f facing %.2f %.2f, %.0f by %.0f\n",
               u->cx, u->cy, u->cz, u->nx, u->nz, u->hw * 2.0f, u->hh * 2.0f);
        check_expect(u->hw * 2.0f > 40.0f && u->hw * 2.0f < 200.0f
                     && u->hh * 2.0f > 60.0f && u->hh * 2.0f < 200.0f,
                     "the hold's way out is not a door's size");
        check_expect(use_from_front(&level, world, i, 48.0f) == i,
                     "the hold's way out was not used from in front of it");
    }
}

/*
 * Stand where a passage lets one out (doors_landing, d out, at eye
 * height), turn round and use what the eye meets. Its index, or -1.
 */
static int turn_and_use(const World *world, const Usable *u, float d)
{
    Level level;
    Camera cam;
    UseHit hit;
    float x, y, z, yaw;

    world_as_level(world, &level);
    doors_landing(u, d, 68.4f, &x, &y, &z, &yaw);
    camera_set(&cam, x, y, z, yaw + DF_TAU * 0.5f, 0.0f);
    if (!interact_aim(&level.scene, &cam, &hit)) {
        return -1;
    }
    return hit.usable;
}

/*
 * A town's doors: its buildings' doors lead in, and the inside of the
 * first, loaded where the building stands, has a way out by that door.
 */
static void check_doors_town(const World *town, const Dungeon *ext)
{
    const Usable *in_door;
    World *inside;
    int enter;
    int people;
    int leave;
    int k;
    float ix, iy, iz;

    enter = count_use(town, USE_PASSAGE, DOOR_ENTER);
    people = count_use(town, USE_TALK, -1);
    printf("farm doors: %d into buildings, %d people, %d ways down\n",
           enter, people, count_use(town, USE_PASSAGE, DOOR_DUNGEON));
    check_expect(world_place(town) == PLACE_TOWN && enter >= 1, "a farm has no door into a building");
    k = world_passage(town, DOOR_ENTER, 0.0f, 0.0f, 0.0f);
    if (k < 0) {
        return;
    }
    in_door = world_usable(town, k);
    ix = in_door->cx;
    iy = in_door->cy;
    iz = in_door->cz;
    printf("farm door %d: building %d of block %d, at %.0f %.0f %.0f facing %.2f %.2f, %.0f by %.0f\n",
           k, in_door->tag & 0xff, in_door->tag >> 8, ix, iy, iz, in_door->nx, in_door->nz,
           in_door->hw * 2.0f, in_door->hh * 2.0f);
    check_expect(in_door->hw * 2.0f > 20.0f && in_door->hw * 2.0f < 200.0f
                 && in_door->hh * 2.0f > 40.0f && in_door->hh * 2.0f < 200.0f,
                 "a farm's door is not a door's size");
    inside = world_load_inside(ext, in_door->tag >> 8, in_door->tag & 0xff);
    if (!inside) {
        fprintf(stderr, "fail: %s\n", world_error());
    }
    check_expect(inside != 0, "a farm building's inside did not load");
    if (!inside) {
        return;
    }
    leave = world_passage(inside, DOOR_LEAVE, ix, iy, iz);
    {
        int fac;
        int bld;
        float sx, sy, sz, syaw, spitch;

        bld = world_building(inside, &fac);
        world_start(inside, &sx, &sy, &sz, &syaw, &spitch);
        printf("farm inside: building type %d faction %d, %d models, %d to use (%d doors, %d out), "
               "start %.0f %.0f %.0f, song %s\n",
               bld, fac, world_model_count(inside), world_usable_count(inside),
               count_use(inside, USE_MOVER, -1), count_use(inside, USE_PASSAGE, DOOR_LEAVE),
               sx, sy, sz, world_soundscape(inside)->song(world_soundscape(inside), 0, 0));
    }
    check_expect(world_place(inside) == PLACE_INSIDE && !world_outdoor(inside) && !world_sky(inside),
                 "a building's inside is under the sky");
    {
        Level il;
        int lit;
        int through;

        /* A house's lamps light their own storey, not the ones above and below through the floors. */
        world_as_level(inside, &il);
        light_set_shadow(0);
        through = light_build(&il.scene);
        light_set_shadow(1);
        lit = light_build(&il.scene);
        printf("farm inside: %d lights, %d decals, %d through floors and walls\n",
               il.scene.nlights, lit, through);
        check_expect(lit < through, "a house's lamps still light through its floors");
    }
    check_expect(leave >= 0, "a building's inside has no way out");
    if (leave >= 0) {
        const Usable *out;
        float dx, dz;

        out = world_usable(inside, leave);
        dx = out->cx - ix;
        dz = out->cz - iz;
        printf("farm way out: at %.0f %.0f %.0f facing %.2f %.2f, %.0f from the street's door\n",
               out->cx, out->cy, out->cz, out->nx, out->nz, df_sqrt(dx * dx + dz * dz));
        /* The same door seen from the other side: close by, facing the other way. */
        check_expect(dx * dx + dz * dz < 64.0f * 64.0f, "a building's way out is not by its door");
        check_expect(out->nx * in_door->nx + out->nz * in_door->nz < -0.7f,
                     "a building's way out does not face in");
        /* Landed in by it as LEVELFALL lands, turned round, the eye meets it. */
        check_expect(turn_and_use(inside, out, 32.0f) == leave,
                     "turned round on coming in, the way out is not what is used");
    }
    check_expect(turn_and_use(town, in_door, 48.0f) == k,
                 "turned round on coming out, the street's door is not what is used");
}

/* 1 when world loaded pictures of TEXTURE.file. */
static int uses_file(const World *world, int file)
{
    int k;

    for (k = 0; k < world_texture_file_count(world); k++) {
        if (world_texture_file(world, k) == file) {
            return 1;
        }
    }
    return 0;
}

/*
 * The seasons: the farm (woodland family) in summer is drawn on ground
 * 302 and the manor set 338; in winter on 303 and 339, their snowy sets,
 * and its window still marked. Back to summer after.
 */
static void check_seasons(const Dungeon *ext)
{
    World *w;
    int s;

    for (s = 0; s < 2; s++) {
        clock_set_season(s ? CLOCK_WINTER : CLOCK_SUMMER);
        w = world_load_dungeon(ext);
        check_expect(w != 0, "the farm did not load in a season");
        if (!w) {
            continue;
        }
        printf("farm in %s: ground %d, manor set %d, %d windows\n", clock_season_name(world_season(w)),
               uses_file(w, 303) ? 303 : (uses_file(w, 302) ? 302 : -1),
               uses_file(w, 339) ? 339 : (uses_file(w, 338) ? 338 : -1), world_glass_count(w));
        check_expect(world_season(w) == (s ? CLOCK_WINTER : CLOCK_SUMMER), "the farm was not built in the clock's season");
        check_expect(s ? (uses_file(w, 303) && uses_file(w, 339) && !uses_file(w, 302) && !uses_file(w, 338))
                       : (uses_file(w, 302) && uses_file(w, 338) && !uses_file(w, 303) && !uses_file(w, 339)),
                     "a farm's ground and walls are not its season's");
        check_expect(world_glass_count(w) >= 1, "a farm's window is not marked to light at night");
    }
    clock_set_season(CLOCK_SUMMER);
}

static void check_sound_town(const World *town)
{
    static df_i16 buf[11025 * 8 * 2];
    const Soundscape *sc;
    Level tl;
    Camera cam;
    double quiet;
    double storm;
    int frames;
    int kind;
    int ok;

    world_as_level(town, &tl);
    sc = world_soundscape(town);
    {
        int animals;
        int k;

        animals = 0;
        for (k = 0; k < world_emitter_count(town); k++) {
            animals += world_emitter_at(town, k)->every > 0.0f;
        }
        printf("farm emitters %d, animals %d\n", world_emitter_count(town), animals);
    }
    check_expect(sc && sc->kind == SCAPE_TOWN && sc->rain && sc->night && sc->wind
                 && sc->nthunder == 3 && sc->nday == 2,
                 "the farm's soundscape lacks rain, crickets, wind, thunder or birds");
    ok = 1;
    for (kind = 0; sc && kind < WEATHER_KINDS; kind++) {
        int night;
        int index;

        for (night = 0; night < 2; night++) {
            for (index = 0; index < 40; index++) {
                Soundscape probe;
                Song song;
                const char *name;

                probe = *sc;
                probe.index = index;
                name = probe.song(&probe, kind, night);
                mem_zero(&song, sizeof(song));
                if (!name || !hmi_load(name, 0, &song)) {
                    printf("no song for weather %d night %d: %s\n", kind, night, name ? name : "-");
                    ok = 0;
                }
            }
        }
    }
    check_expect(ok, "a playlist names a song MIDI.BSA does not have");
    world_start(town, &cam.x, &cam.y, &cam.z, &cam.yaw, &cam.pitch);
    sound_stop_all();
    weather_set(WEATHER_SUNNY);
    frames = run_sound(&tl, &cam, 0.0f, 0, 1.0f, buf, 11025 * 8);
    quiet = rms_of(buf, frames);
    weather_set(WEATHER_THUNDER);
    frames = run_sound(&tl, &cam, 0.0f, 30, 7.0f, buf, 11025 * 8);
    storm = rms_of(buf, frames);
    printf("farm sound: sunny rms %.0f, storm rms %.0f, song %s\n", quiet, storm, music_name());
    check_expect(storm > quiet + 100.0, "a thunderstorm is not louder than a sunny day");
    write_wav("build/snd1.wav", buf, frames);
    weather_set(WEATHER_SUNNY);
    sound_stop_all();
    music_stop();
}

static void check_skies(void)
{
    static df_u8 morning[SKY_W * SKY_H];
    static df_u8 evening[SKY_W * SKY_H];
    static df_u8 half[512];
    df_u8 rgb[768];
    df_u8 rgb2[768];
    Sky sky;
    VfsFile *f;
    int same;
    int k;

    check_expect(skyfile_index(CLIMATE_WOODLANDS, SEASON_SUMMER) == 18
                 && skyfile_index(CLIMATE_DESERT, SEASON_FALL) == 8
                 && skyfile_index(CLIMATE_MOUNTAIN, SEASON_WINTER) == 3
                 && skyfile_index(CLIMATE_SWAMP, SEASON_SPRING) == 25,
                 "a climate draws the wrong sky file");
    {
        df_u8 odds[WEATHER_KINDS];
        int total;
        int k;

        /* The Chronicles' table: woodlands in winter snow, deserts in summer never rain. */
        forecast_odds(CLIMATE_WOODLANDS, SEASON_WINTER, odds);
        total = 0;
        for (k = 0; k < WEATHER_KINDS; k++) {
            total += odds[k];
        }
        check_expect(total == 100 && odds[WEATHER_SNOW] == 25, "woodland winter odds are not the table's");
        forecast_odds(CLIMATE_DESERT, SEASON_SUMMER, odds);
        check_expect(odds[WEATHER_SUNNY] == 85 && odds[WEATHER_RAIN] == 0 && odds[WEATHER_SNOW] == 0,
                     "the summer desert can rain");
    }
    check_expect(skyfile_frame(360.0f) == 0 && skyfile_frame(720.0f) == 32
                 && skyfile_frame(1079.0f) == 63 && skyfile_frame(480.0f) == 20,
                 "the sky frames do not follow the hour");
    skyfile_open(CLIMATE_WOODLANDS, SEASON_SUMMER, &sky);
    /* Keys are the file times 128 plus the frame; 64 is the night picture. */
    check_expect(sky.pick(&sky, 120.0f, WEATHER_SKY_CLEAR) == 18 * 128 + 64
                 && sky.pick(&sky, 1080.0f, WEATHER_SKY_CLEAR) == 18 * 128 + 64
                 && sky.pick(&sky, 720.0f, WEATHER_SKY_CLEAR) == 18 * 128 + 32 && sky.night == 2,
                 "night is not its own picture");
    check_expect(sky.pick(&sky, 720.0f, WEATHER_SKY_RAIN) == 20 * 128 + 32
                 && sky.pick(&sky, 720.0f, WEATHER_SKY_OVERCAST) == 21 * 128 + 32
                 && sky.pick(&sky, 720.0f, WEATHER_SKY_SNOW) == 22 * 128 + 32
                 && sky.pick(&sky, 120.0f, WEATHER_SKY_RAIN) == 20 * 128,
                 "bad weather does not draw the climate's rain or snow sky, frame 0 at night");
    check_expect(sky.read(&sky, 21 * 128 + 31, morning, rgb), "SKY21.DAT did not read");
    check_expect(sky.read(&sky, 18 * 128 + 20, morning, rgb) && sky.read(&sky, 18 * 128 + 43, evening, rgb2),
                 "SKY18.DAT frame 20 did not read");
    /* Record 1, frame 20, row 100: the eastern half, columns 0..511 in the morning. */
    f = vfs_open("SKY18.DAT");
    check_expect(f && vfs_read_at(f, 32u * 776u + 32u * 16384u + 112640u * 52u + 100u * 512u, half, 512)
                 && vfs_read_at(f, 20u * 776u + 8u, rgb2, 768),
                 "SKY18.DAT is not there");
    if (f) {
        vfs_close(f);
    }
    same = 1;
    for (k = 0; k < 768; k++) {
        same = same && rgb[k] == rgb2[k];
    }
    check_expect(same, "the sky palette is not frame 20's, after its 8-byte header");
    same = 1;
    for (k = 0; k < 512; k++) {
        same = same && morning[100 * SKY_W + k] == half[k]
            && evening[100 * SKY_W + 512 + k] == half[k];
    }
    check_expect(same, "the eastern half is not first in the morning and second in the evening");
    check_expect(sky.read(&sky, 18 * 128 + 64, morning, rgb) && morning[219 * SKY_W] == morning[218 * SKY_W]
                 && morning[10 * SKY_W + 5] == morning[10 * SKY_W + 517],
                 "the night picture does not fill both halves and the last row");
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
    check_sound_files();

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
    check_expect(!world_outdoor(world) && !world_sky(world), "the hold has a sky");
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

        {
            int over;
            int on;

            on = flats_on_floor(world, &over);
            printf("hold flats standing on the floor: %d of %d over one\n", on, over);
            check_expect(over > 100 && on * 3 > over, "the hold's flats do not stand on its floors");
        }
        torch = find_flat(world, 210, 6);
        check_expect(torch && check_near(torch->w, 16.0f, 0.01f) && check_near(torch->h, 94.0f, 0.01f),
               "torch 210/6 is not 16 by 94");
        /* Its four frames in one sheet, the square it had anyway, and they differ. */
        if (torch) {
            const Image *ti;

            ti = world_image(world, torch->slot);
            check_expect(ti && ti->frames == 4 && ti->pot_w == 128 && ti->frame_cols >= 4
                         && ti->frame_dx == 18 && torch->rate == 12
                         && memcmp(ti->pot, ti->pot + ti->frame_dx, 16) != 0,
                         "the torch's frames are not a sheet of four at 12 a second");
        }
        {
            Level hl;
            int k;
            int flick;

            world_as_level(world, &hl);
            flick = 0;
            for (k = 0; k < hl.scene.nlights; k++) {
                flick += hl.scene.lights[k].flicker;
            }
            printf("hold lights flickering: %d of %d\n", flick, hl.scene.nlights);
            {
                int lit;
                int through;

                /* Through its walls the hold's lights cut 2282 decals; seeing only what they reach, fewer. */
                light_set_shadow(0);
                through = light_build(&hl.scene);
                light_set_shadow(1);
                lit = light_build(&hl.scene);
                printf("hold decals: %d, %d through walls\n", lit, through);
                check_expect(through == 2282 && lit > 1000 && lit < through * 4 / 5,
                             "the hold's lights still reach through its walls");
            }
            check_expect(flick > 20 && flick < hl.scene.nlights, "the hold's flames do not flicker, or all do");
        }
        check_sound_hold(world);
        check_doors_hold(world);
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
        int hold_out;
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
            hold_out = -1;
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
                if (df_eq(places[i].name, "Privateer's Hold")) {
                    hold_out = i;
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
            /* The hold's outside: its way down, by the location the dungeon shares. */
            check_expect(hold_out >= 0, "Privateer's Hold has no outside");
            if (hold_out >= 0 && exterior_load_at(places[hold_out].region, places[hold_out].index, &ext)) {
                World *top;

                top = world_load_dungeon(&ext);
                check_expect(top != 0, "Privateer's Hold's outside did not load");
                if (top) {
                    int down;

                    down = world_passage(top, DOOR_DUNGEON, 0.0f, 0.0f, 0.0f);
                    printf("hold outside: location %d, %d blocks, %d ways down",
                           ext.location_id, ext.nblocks, count_use(top, USE_PASSAGE, DOOR_DUNGEON));
                    if (down >= 0) {
                        const Usable *u;

                        u = world_usable(top, down);
                        printf(", at %.0f %.0f %.0f facing %.2f %.2f, %.0f by %.0f",
                               u->cx, u->cy, u->cz, u->nx, u->nz, u->hw * 2.0f, u->hh * 2.0f);
                    }
                    printf("\n");
                    check_expect(down >= 0, "Privateer's Hold's outside has no way down");
                }
                {
                    Dungeon below;

                    /* The way down leads to the dungeon of the same location: the viewer pairs them so. */
                    check_expect(dungeon_load(&below), dungeon_error());
                    printf("hold below: location %d, under %d\n", below.location_id, below.exterior_id);
                    check_expect(below.exterior_id == ext.exterior_id,
                                 "Privateer's Hold's dungeon is not under its outside");
                }
            }
            if (farm >= 0 && exterior_load_at(places[farm].region, places[farm].index, &ext)) {
                printf("outdoor %s blocks %d\n", ext.title, ext.nblocks);
                town = world_load_dungeon(&ext);
                if (!town) {
                    fprintf(stderr, "fail: %s\n", world_error());
                }
                check_expect(town && world_model_count(town) > 0, "outdoor did not load");
                if (town) {
                    Level tl;
                    int glow;
                    int k;

                    world_as_level(town, &tl);
                    glow = 0;
                    for (k = 0; k < world_flat_count(town); k++) {
                        glow += world_flat(town, k)->glow ? 1 : 0;
                    }
                    {
                        int over;
                        int on;

                        on = flats_on_floor(town, &over);
                        check_expect(over > 10 && on * 10 > over * 8, "a farm's flats do not stand on the ground");
                    }
                    check_expect(world_outdoor(town) && tl.sky && tl.scene.outdoor
                                 && world_light_count(town) == glow,
                                 "an outdoor place has no sky, or a lamp flat has no light");
                }
                if (town) {
                    check_sound_town(town);
                    check_doors_town(town, &ext);
                    check_seasons(&ext);
                }
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

    check_skies();
    return check_failed() ? 1 : 0;
}
