#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/rules.h"
#include "engine/core/df.h"
#include "engine/gfx/camera.h"

/* See viewopt.h. HeadBobber.cs (MIT, Daggerfall Workshop): PlotPath and
 * PlotRotation. Metres become this world's units. The decompilation is
 * not the source of the wave. */

typedef struct ViewRow {
    df_u16 id;
    df_u8 full;
    df_u8 bob;
    df_u16 fov;
} ViewRow;

static const ViewRow g_default_row = { 0, 0, 0, 75 };
static const RecField g_fields[] = {
    REC_FIELD("id", REC_UINT, ViewRow, id),
    REC_FIELD("full", REC_UINT, ViewRow, full),
    REC_FIELD("bob", REC_UINT, ViewRow, bob),
    REC_FIELD("fov", REC_UINT, ViewRow, fov)
};
static const RecSchema g_schema = { "OPTS", "options", g_fields, 4, (df_u16)sizeof(ViewRow), &g_default_row };

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

void viewopt_defaults(void)
{
    g_full = 0;
    g_bob = 0;
    camera_set_fov(75.0f);
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

void viewopt_read(const df_u8 *bytes, df_u32 n)
{
    static RecTable table;
    RecChunk chunk;
    df_u32 pos;
    const ViewRow *row;

    if (!bytes || !rec_is_file(bytes, n)) {
        return;
    }
    if (!table.schema && !rec_table_init(&table, &g_schema, 1)) {
        return;
    }
    rec_table_clear(&table);
    pos = 0;
    while (rec_next(bytes, n, &pos, &chunk)) {
        if (df_eq(chunk.tag, "OPTS")) {
            rec_merge(&table, &chunk, 0);
        }
    }
    row = (const ViewRow *)rec_row(&table, 0);
    /* A missing row leaves the caller's default. Anything but 0 or 1 is ignored. */
    if (!row) {
        return;
    }
    /* Missing fields inherit the schema default, so pre-FOV files remain
     * at 75 degrees. Invalid stored angles cannot corrupt the projection. */
    if (row->fov >= 45 && row->fov <= 120) camera_set_fov((float)row->fov);
    if (row->full <= 1) {
        g_full = row->full;
    }
    if (row->bob <= 1) {
        g_bob = row->bob;
    }
}

int viewopt_write(RecPut put, void *ctx)
{
    ViewRow row;

    row.id = 0;
    row.full = (df_u8)g_full;
    row.bob = (df_u8)g_bob;
    row.fov = (df_u16)df_iround(camera_fov());
    return rec_put_rows(put, ctx, &g_schema, &row, 1, 0, 0);
}
