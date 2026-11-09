#include "game/ogfall/places/place.h"

void ogfall_place_startup(void)
{
}

void ogfall_place_shutdown(void)
{
}

int og_place_marker6(const OgMark *marks, int n,
                     float fx, float fy, float fz,
                     float *x, float *y, float *z)
{
    int i;
    int found;
    float best;

    found = 0;
    best = 0.0f;
    if (!marks || n < 1 || !x || !y || !z) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        float dx;
        float dy;
        float dz;
        float d;

        if (marks[i].record != 6) {
            continue;
        }
        dx = marks[i].x - fx;
        dy = marks[i].y - fy;
        dz = marks[i].z - fz;
        d = dx * dx + dy * dy + dz * dz;
        if (!found || d < best) {
            best = d;
            *x = marks[i].x;
            *y = marks[i].y;
            *z = marks[i].z;
            found = 1;
        }
    }
    return found;
}
