#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/rules.h"
#include "engine/core/df.h"
#include "engine/gfx/camera.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"

/* See viewopt.h. HeadBobber.cs (MIT, Daggerfall Workshop): PlotPath and
 * PlotRotation. Metres become this world's units. The decompilation is
 * not the source of the wave. */

typedef struct ViewRow {
    df_u16 id;
    df_u8 full;
    df_u8 bob;
    df_u16 fov;
    df_u8 sfx_volume;
    df_u8 music_volume;
    df_u8 crouch_toggle;
    df_u8 tex_detail;
    df_u8 sprite_detail;
    df_u16 mode_w;
    df_u16 mode_h;
} ViewRow;

static const ViewRow g_default_row = { 0, 0, 0, 75, 80, 80, 0, 2, 2, 640, 480 };
static const RecField g_fields[] = {
    REC_FIELD("id", REC_UINT, ViewRow, id),
    REC_FIELD("full", REC_UINT, ViewRow, full),
    REC_FIELD("bob", REC_UINT, ViewRow, bob),
    REC_FIELD("fov", REC_UINT, ViewRow, fov),
    REC_FIELD("sfx_volume", REC_UINT, ViewRow, sfx_volume),
    REC_FIELD("music_volume", REC_UINT, ViewRow, music_volume),
    REC_FIELD("crouch_toggle", REC_UINT, ViewRow, crouch_toggle),
    REC_FIELD("tex_detail", REC_UINT, ViewRow, tex_detail),
    REC_FIELD("sprite_detail", REC_UINT, ViewRow, sprite_detail),
    REC_FIELD("mode_w", REC_UINT, ViewRow, mode_w),
    REC_FIELD("mode_h", REC_UINT, ViewRow, mode_h)
};
static const RecSchema g_schema = { "OPTS", "options", g_fields, 11, (df_u16)sizeof(ViewRow), &g_default_row };

/* Metres, HeadBobber's walking and running amounts. */
#define WALK_X 0.045f
#define WALK_Y 0.062f
#define RUN_X 0.09f
#define RUN_Y 0.11f
/* Degrees, PlotRotation. */
#define WALK_NOD_X 0.25f
#define WALK_NOD_Y 0.1f
#define RUN_NOD_X 0.6f
#define RUN_NOD_Y 0.15f
#define DEG (DF_TAU / 360.0f)

static int g_full;
static int g_bob;
static int g_crouch_toggle;
static int g_mode_w = 640;
static int g_mode_h = 480;
static int g_tex_detail = 2;
static int g_sprite_detail = 2;

void viewopt_defaults(void)
{
    g_full = 0;
    g_bob = 0;
    g_crouch_toggle = 0;
    g_tex_detail = 2;
    g_sprite_detail = 2;
    g_mode_w = 640;
    g_mode_h = 480;
    camera_set_fov(75.0f);
    sound_set_volume(80);
    music_set_volume(80);
}

int viewopt_full(void)
{
    return g_full;
}

void viewopt_set_full(int on)
{
    g_full = on ? 1 : 0;
}

int viewopt_bob(void)
{
    return g_bob;
}

void viewopt_set_bob(int on)
{
    g_bob = on ? 1 : 0;
}

int viewopt_crouch_toggle(void)
{
    return g_crouch_toggle;
}

void viewopt_set_crouch_toggle(int on)
{
    g_crouch_toggle = on != 0;
}

void viewopt_eye(float phase, int walking, int running, float *right, float *up, float *pitch, float *yaw)
{
    float s;
    float c;
    float xamt;
    float yamt;
    float nx;
    float ny;

    *right = 0.0f;
    *up = 0.0f;
    *pitch = 0.0f;
    *yaw = 0.0f;
    if (!g_bob || !walking) {
        return;
    }
    s = df_sin(phase);
    c = df_cos(phase);
    if (running) {
        xamt = RUN_X;
        yamt = RUN_Y;
        nx = RUN_NOD_X;
        ny = RUN_NOD_Y;
    } else {
        xamt = WALK_X;
        yamt = WALK_Y;
        nx = WALK_NOD_X;
        ny = WALK_NOD_Y;
    }
    /* PlotPath: lateral cos, vertical |sin| so a step rises and falls. */
    *right = c * xamt * RULES_UNITS_PER_M;
    *up = df_absf(s * yamt) * RULES_UNITS_PER_M;
    /* PlotRotation, degrees, as an offset for this phase rather than a spin. */
    *pitch = df_absf(s * nx) * DEG;
    *yaw = -s * ny * DEG;
}

/* The OPTS row of a record file, over the schema's defaults: 0 for no file or no row. One table, reused. */
static const ViewRow *parse_row(const df_u8 *bytes, df_u32 n)
{
    static RecTable table;
    RecChunk chunk;
    df_u32 pos;

    if (!bytes || !rec_is_file(bytes, n)) {
        return 0;
    }
    if (!table.schema && !rec_table_init(&table, &g_schema, 1)) {
        return 0;
    }
    rec_table_clear(&table);
    pos = 0;
    while (rec_next(bytes, n, &pos, &chunk)) {
        if (df_eq(chunk.tag, "OPTS")) {
            rec_merge(&table, &chunk, 0);
        }
    }
    return (const ViewRow *)rec_row(&table, 0);
}

int viewopt_mode_in(const df_u8 *bytes, df_u32 n, int *w, int *h)
{
    const ViewRow *row;

    row = parse_row(bytes, n);
    if (!row || row->mode_w < 64 || row->mode_h < 64) {
        return 0;
    }
    *w = row->mode_w;
    *h = row->mode_h;
    return 1;
}

void viewopt_read(const df_u8 *bytes, df_u32 n)
{
    const ViewRow *row;

    row = parse_row(bytes, n);
    /* A missing row leaves the caller's default. Anything but 0 or 1 is ignored. */
    if (!row) {
        return;
    }
    /* Missing fields inherit the schema default, so pre-FOV files remain
     * at 75 degrees. Invalid stored angles cannot corrupt the projection. */
    if (row->fov >= 45 && row->fov <= 120) camera_set_fov((float)row->fov);
    /* Named-field merging gives pre-audio files 80/80. Zero is a valid
     * mute level, not an absent field. Invalid percentages keep defaults. */
    if (row->sfx_volume <= 100) sound_set_volume(row->sfx_volume);
    if (row->music_volume <= 100) music_set_volume(row->music_volume);
    if (row->full <= 1) {
        g_full = row->full;
    }
    if (row->bob <= 1) {
        g_bob = row->bob;
    }
    if (row->crouch_toggle <= 1) {
        g_crouch_toggle = row->crouch_toggle;
    }
    /* Older files have no detail fields and take High; anything above 2 is ignored. */
    if (row->tex_detail <= 2) {
        g_tex_detail = row->tex_detail;
    }
    if (row->sprite_detail <= 2) {
        g_sprite_detail = row->sprite_detail;
    }
    /* The mode is the engine's to accept: only a sane size is kept; an old file says 640 x 480. */
    if (row->mode_w >= 64 && row->mode_h >= 64) {
        g_mode_w = row->mode_w;
        g_mode_h = row->mode_h;
    }
}

int viewopt_write(RecPut put, void *ctx)
{
    ViewRow row;

    row.id = 0;
    row.full = (df_u8)g_full;
    row.bob = (df_u8)g_bob;
    row.crouch_toggle = (df_u8)g_crouch_toggle;
    row.tex_detail = (df_u8)g_tex_detail;
    row.sprite_detail = (df_u8)g_sprite_detail;
    row.mode_w = (df_u16)g_mode_w;
    row.mode_h = (df_u16)g_mode_h;
    row.fov = (df_u16)df_iround(camera_fov());
    row.sfx_volume = (df_u8)sound_volume();
    row.music_volume = (df_u8)music_volume();
    return rec_put_rows(put, ctx, &g_schema, &row, 1, 0, 0);
}

int viewopt_tex_detail(void)
{
    return g_tex_detail;
}

void viewopt_set_tex_detail(int level)
{
    g_tex_detail = level < 0 ? 0 : (level > 2 ? 2 : level);
}

int viewopt_sprite_detail(void)
{
    return g_sprite_detail;
}

void viewopt_set_sprite_detail(int level)
{
    g_sprite_detail = level < 0 ? 0 : (level > 2 ? 2 : level);
}

void viewopt_mode(int *w, int *h)
{
    *w = g_mode_w;
    *h = g_mode_h;
}

void viewopt_set_mode(int w, int h)
{
    if (w >= 64 && h >= 64) {
        g_mode_w = w;
        g_mode_h = h;
    }
}
