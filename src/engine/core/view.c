#include "engine/core/df.h"

/*
 * The view's size (df.h). Two globals rather than calls: the projection
 * reads the centre for every vertex, and on the 233 a call there costs
 * more than the multiply it feeds. 640 x 480 until Glide opens a mode.
 */
int df_view_size[2] = { 640, 480 };
float df_view_centre[2] = { 320.0f, 240.0f };

void df_set_view(int w, int h)
{
    if (w < 1 || h < 1 || w > DF_VIEW_MAX_W || h > DF_VIEW_MAX_H) {
        return;
    }
    df_view_size[0] = w;
    df_view_size[1] = h;
    df_view_centre[0] = (float)w * 0.5f;
    df_view_centre[1] = (float)h * 0.5f;
}
