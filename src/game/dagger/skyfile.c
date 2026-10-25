#include "game/dagger/skyfile.h"
#include "game/dagger/climate.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"

#define FRAME_W 512
#define FRAME_H 220
#define NIGHT_H 219
#define PAL_BYTES 776
#define PAL_HEAD 8
#define FRAMES 32
#define MAP_BYTES 16384
#define PICTURES_AT ((df_u32)FRAMES * PAL_BYTES + (df_u32)FRAMES * MAP_BYTES)
#define FRAME_BYTES ((df_u32)FRAME_W * FRAME_H)
/* The key for the night picture; day keys are frames 0..63. */
#define KEY_NIGHT 64
#define DAWN 360.0f
#define DUSK 1080.0f

/*
 * Which sky the climate draws, as Daggerfall Unity's climate table
 * gives it: mountains 0, desert 8, the temperate north 16, the south
 * and the sea 24. The season adds 0..3.
 */
int skyfile_index(int climate, int season)
{
    int base;

    switch (climate) {
    case CLIMATE_MOUNTAIN:
        base = 0;
        break;
    case CLIMATE_DESERT:
    case CLIMATE_DESERT2:
        base = 8;
        break;
    case CLIMATE_OCEAN:
    case CLIMATE_RAINFOREST:
    case CLIMATE_SWAMP:
    case CLIMATE_SUBTROPICAL:
        base = 24;
        break;
    default:
        base = 16;
        break;
    }
    if (season < 0 || season > 3) {
        season = SEASON_SUMMER;
    }
    return base + season;
}

/*
 * Frames through the day, hours after dawn against frame. The original
 * does not spread them evenly: the sky climbs fast from six to eight
 * and comes down fast from four to six, and hardly moves around noon.
 */
#define CURVE_N 9
static const float g_curve[CURVE_N][2] = {
    { 0.0f, 0.0f }, { 1.0f, 12.0f }, { 2.0f, 20.0f }, { 4.0f, 28.0f }, { 6.0f, 32.0f },
    { 8.0f, 36.0f }, { 10.0f, 44.0f }, { 11.0f, 52.0f }, { 12.0f, 64.0f },
};

int skyfile_frame(float minute)
{
    float h;
    float f;
    int i;
    int frame;

    h = (minute - DAWN) * (1.0f / 60.0f);
    if (h <= 0.0f) {
        return 0;
    }
    if (h >= 12.0f) {
        return 63;
    }
    i = 0;
    while (i < CURVE_N - 2 && g_curve[i + 1][0] <= h) {
        i++;
    }
    f = g_curve[i][1] + (g_curve[i + 1][1] - g_curve[i][1])
        * (h - g_curve[i][0]) / (g_curve[i + 1][0] - g_curve[i][0]);
    /* Once a frame from draw_frame: no C cast, which would switch the FPU's rounding. */
    frame = df_ifloor(f);
    if (frame < 0) {
        frame = 0;
    }
    if (frame > 63) {
        frame = 63;
    }
    return frame;
}

static int pick(const Sky *sky, float minute)
{
    (void)sky;
    if (minute < DAWN || minute >= DUSK) {
        return KEY_NIGHT;
    }
    return skyfile_frame(minute);
}

static int read_at(const char *name, df_u32 at, void *dst, df_u32 n)
{
    VfsFile *f;
    int ok;

    f = vfs_open(name);
    if (!f) {
        return 0;
    }
    ok = vfs_size(f) >= at + n && vfs_read_at(f, at, dst, n);
    vfs_close(f);
    return ok;
}

/* One half picture into columns x0.. of the circle. */
static void place_half(df_u8 *pixels, const df_u8 *half, int rows, int x0)
{
    int y;

    for (y = 0; y < SKY_H; y++) {
        const df_u8 *src;
        df_u8 *dst;
        int x;

        src = &half[(y < rows ? y : rows - 1) * FRAME_W];
        dst = &pixels[y * SKY_W + x0];
        for (x = 0; x < FRAME_W; x++) {
            dst[x] = src[x];
        }
    }
}

static df_u8 g_half[FRAME_BYTES];
static df_u8 g_col[PAL_BYTES];
static df_u32 g_seed = 0x2545f491u;

static int read_night(const Sky *sky, df_u8 *pixels, df_u8 *rgb)
{
    char name[16] = "NITE00I0.IMG";
    df_u32 i;
    int y;

    name[5] = (char)('0' + sky->night);
    if (!read_at(name, 0, g_half, (df_u32)FRAME_W * NIGHT_H)
        || !read_at("NIGHTSKY.COL", 0, g_col, PAL_BYTES)) {
        return 0;
    }
    /*
     * Stars, as Daggerfall Unity drops them: 4 in 1000 pixels of clear
     * sky (index 17..31) become one of six bright entries. A new night
     * scatters them again.
     */
    for (i = 0; i < (df_u32)FRAME_W * NIGHT_H; i++) {
        static const df_u8 star[6] = { 16, 32, 74, 105, 112, 120 };

        g_seed = g_seed * 1664525u + 1013904223u;
        if (g_half[i] > 16 && g_half[i] < 32 && (g_seed >> 16) % 1000u < 4u) {
            g_half[i] = star[(g_seed >> 8) % 6u];
        }
    }
    /* The right column of the picture does not meet its left: copy its neighbour. */
    for (y = 0; y < NIGHT_H; y++) {
        g_half[y * FRAME_W + FRAME_W - 1] = g_half[y * FRAME_W + FRAME_W - 2];
    }
    place_half(pixels, g_half, NIGHT_H, 0);
    place_half(pixels, g_half, NIGHT_H, FRAME_W);
    for (i = 0; i < 768; i++) {
        rgb[i] = g_col[PAL_HEAD + i];
    }
    return 1;
}

static int read_sky(const Sky *sky, int key, df_u8 *pixels, df_u8 *rgb)
{
    char name[16] = "SKY00.DAT";
    int frame;
    int flip;
    int rec;
    int i;

    if (key == KEY_NIGHT) {
        return read_night(sky, pixels, rgb);
    }
    if (key < 0 || key > 63) {
        return 0;
    }
    name[3] = (char)('0' + sky->index / 10);
    name[4] = (char)('0' + sky->index % 10);
    flip = key >= FRAMES;
    frame = flip ? 63 - key : key;
    if (!read_at(name, (df_u32)frame * PAL_BYTES, g_col, PAL_BYTES)) {
        return 0;
    }
    for (i = 0; i < 768; i++) {
        rgb[i] = g_col[PAL_HEAD + i];
    }
    /* North through east to south first; the afternoon swaps the halves. */
    for (rec = 0; rec < 2; rec++) {
        int east;

        east = rec == 1;
        if (!read_at(name, PICTURES_AT + FRAME_BYTES * (df_u32)(rec * FRAMES + frame),
                     g_half, FRAME_BYTES)) {
            return 0;
        }
        place_half(pixels, g_half, FRAME_H, (east != flip) ? 0 : FRAME_W);
    }
    return 1;
}

void skyfile_open(int climate, int season, Sky *sky)
{
    int index;

    index = skyfile_index(climate, season);
    sky->pick = pick;
    sky->read = read_sky;
    sky->index = index;
    /* The night picture by the sky's group, as Daggerfall Unity pairs them. */
    if (index <= 7) {
        sky->night = 3;
    } else if (index <= 15) {
        sky->night = 1;
    } else if (index <= 23) {
        sky->night = 2;
    } else {
        sky->night = 0;
    }
    log_print(LOG_INFO, "sky: SKY%d%d.DAT, night NITE0%dI0.IMG",
              index / 10, index % 10, sky->night);
}
