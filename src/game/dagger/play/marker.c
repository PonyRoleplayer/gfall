#include "game/dagger/play/marker.h"
#include "engine/core/df.h"

/* See marker.h. */

/* The arrow in its own unit square, pointing up: the tip, the wings, and the notch between them at the back. */
#define TIP_Y -0.92f
#define WING_X 0.70f
#define WING_Y 0.78f
#define NOTCH_Y 0.38f

int marker_step(float angle)
{
    int s;

    angle = df_wrap_tau(angle);
    s = df_iround(angle * (float)MARKER_STEPS / DF_TAU);
    return s % MARKER_STEPS;
}

/* Which side of the line a-b point p is on: >= 0 on its left. */
static float side(float ax, float ay, float bx, float by, float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

/* 1 when (x, y) of the unit square is inside the arrow: the two triangles tip-left-notch and tip-notch-right. */
static int inside(float x, float y)
{
    float s1;
    float s2;
    float s3;

    /* Tip, left wing, notch: the same sign on all three edges is inside. */
    s1 = side(0.0f, TIP_Y, -WING_X, WING_Y, x, y);
    s2 = side(-WING_X, WING_Y, 0.0f, NOTCH_Y, x, y);
    s3 = side(0.0f, NOTCH_Y, 0.0f, TIP_Y, x, y);
    if ((s1 <= 0.0f && s2 <= 0.0f && s3 <= 0.0f) || (s1 >= 0.0f && s2 >= 0.0f && s3 >= 0.0f)) {
        return 1;
    }
    s1 = side(0.0f, TIP_Y, 0.0f, NOTCH_Y, x, y);
    s2 = side(0.0f, NOTCH_Y, WING_X, WING_Y, x, y);
    s3 = side(WING_X, WING_Y, 0.0f, TIP_Y, x, y);
    return (s1 <= 0.0f && s2 <= 0.0f && s3 <= 0.0f) || (s1 >= 0.0f && s2 >= 0.0f && s3 >= 0.0f);
}

void marker_compose(unsigned char *out, int step)
{
    unsigned char fill[MARKER_SIDE * MARKER_SIDE];
    float c;
    float s;
    float angle;
    int x;
    int y;

    angle = (float)step * DF_TAU / (float)MARKER_STEPS;
    c = df_cos(angle);
    s = df_sin(angle);
    /* Each texel's middle, turned back by the angle into the arrow's own square. */
    for (y = 0; y < MARKER_SIDE; y++) {
        for (x = 0; x < MARKER_SIDE; x++) {
            float u;
            float v;

            u = ((float)x + 0.5f) / (float)MARKER_SIDE * 2.0f - 1.0f;
            v = ((float)y + 0.5f) / (float)MARKER_SIDE * 2.0f - 1.0f;
            /* Screen y grows down, so clockwise on the screen is this turn. */
            fill[y * MARKER_SIDE + x] = (unsigned char)inside(u * c + v * s, -u * s + v * c);
        }
    }
    /* Inside it is the fill; a texel of it beside one outside, the outline, two texels thick. */
    for (y = 0; y < MARKER_SIDE; y++) {
        for (x = 0; x < MARKER_SIDE; x++) {
            int edge;
            int dy;
            int dx;

            if (!fill[y * MARKER_SIDE + x]) {
                out[y * MARKER_SIDE + x] = 0;
                continue;
            }
            edge = 0;
            for (dy = -2; dy <= 2 && !edge; dy++) {
                for (dx = -2; dx <= 2 && !edge; dx++) {
                    int nx;
                    int ny;

                    nx = x + dx;
                    ny = y + dy;
                    edge = nx < 0 || ny < 0 || nx >= MARKER_SIDE || ny >= MARKER_SIDE || !fill[ny * MARKER_SIDE + nx];
                }
            }
            out[y * MARKER_SIDE + x] = (unsigned char)(edge ? MARKER_EDGE : MARKER_FILL);
        }
    }
}
