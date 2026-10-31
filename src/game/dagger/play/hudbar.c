#include "game/dagger/play/hudbar.h"
#include "engine/core/df.h"

/*
 * HUDLarge's control bounds, translated once by the bar's top (154).
 * Keep the narrow options strip and one-pixel seams exactly: stretching
 * neighboring hit boxes into gaps turns an otherwise harmless click into
 * a command. The two rows share their art's 47 x 23 cells.
 */
static const Rect g_rect[] = {
    { 0, 0, 0, 0 },
    { 7, 162, 33, 30 },
    { 71, 154, 12, 46 },
    { 84, 154, 47, 23 },
    { 131, 154, 47, 23 },
    { 178, 154, 47, 23 },
    { 225, 154, 47, 23 },
    { 84, 177, 47, 23 },
    { 131, 177, 47, 23 },
    { 178, 177, 47, 23 },
    { 225, 177, 47, 23 },
    { 275, 156, 43, 42 }
};
#define BUTTONS ((int)(sizeof(g_rect) / sizeof(g_rect[0])))

const Rect *hudbar_rect(int action)
{
    return action > HUDBAR_NONE && action < BUTTONS ? &g_rect[action] : 0;
}

int hudbar_contains(int x, int y)
{
    return x >= 0 && x < CANVAS_W && y >= HUDBAR_TOP && y < CANVAS_H;
}

int hudbar_hit(int x, int y)
{
    int i;

    if (!hudbar_contains(x, y)) {
        return HUDBAR_NONE;
    }
    /* Eleven disjoint rectangles, and only on a click; no per-frame scan. */
    for (i = 1; i < BUTTONS; i++) {
        if (rect_has(&g_rect[i], x, y)) {
            return i;
        }
    }
    return HUDBAR_NONE;
}

const char *hudbar_mode_name(int mode)
{
    static const char *const names[] = { "Grab", "Info", "Steal", "Talk" };

    return names[mode >= 0 && mode < HUDBAR_MODES ? mode : HUDBAR_GRAB];
}

int hudbar_mode_row(int mode)
{
    /* The file's order is Steal, Talk, Grab, Info, unlike the default-first
     * order above. Do not reorder the file or store four separate pictures. */
    static const df_u8 rows[] = { 2, 3, 0, 1 };

    return rows[mode >= 0 && mode < HUDBAR_MODES ? mode : HUDBAR_GRAB];
}

int hudbar_heading(float yaw)
{
    /* df_ifloor avoids GCC's x87 rounding-mode switch for a float-to-int
     * cast. Unsigned masking wraps negative headings and a complete turn. */
    return (int)((df_u32)df_ifloor(yaw * (32.0f / DF_TAU)) & 31u);
}
