#ifndef GFALL_PLACE_H
#define GFALL_PLACE_H

/*
 * Where a new game stands, after world_start's marker 8 (else 10).
 * newgame_place_player (init.c) then moves to the nearest editor
 * marker 6 when any exist. This is that choice on a plain list.
 * world_start is not changed.
 */

typedef struct MarkInfo {
    int record;
    float x, y, z;
} MarkInfo;

void gfall_place_startup(void);
void gfall_place_shutdown(void);

/*
 * Nearest record-6 marker to (fx, fy, fz). Writes its point and
 * returns 1. A list with no 6 returns 0 and leaves the point alone,
 * so the caller keeps marker 8. Ties keep the earlier row.
 */
int place_marker6(const MarkInfo *marks, int n,
                     float fx, float fy, float fz,
                     float *x, float *y, float *z);

#endif
