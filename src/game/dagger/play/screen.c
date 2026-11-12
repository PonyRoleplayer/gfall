#include "game/dagger/play/screen.h"
#include "engine/core/df.h"
#include "engine/ui/pic.h"

void size_fit(int *width, int *height, int max_width, int max_height)
{
    int sx, sy;
    if (*width <= 0 || *height <= 0 || max_width <= 0 || max_height <= 0) {
        *width = *height = 0; return;
    }
    if (*width <= max_width && *height <= max_height) { return; }
    sx = (max_width << 8)/ *width; sy = (max_height << 8)/ *height;
    if (sy < sx) { *width = (*width*sy) >> 8; *height = max_height; }
    else { *height = (*height*sx) >> 8; *width = max_width; }
    if (*width < 1) { *width = 1; }
    if (*height < 1) { *height = 1; }
}

void screen_rect(int view_w, int view_h, int *x, int *top, int *w, int *h)
{
    int scale;
    int pw;
    int ph;
    int bottom;

    scale = view_w / 320;
    if (scale < 1) {
        scale = 1;
    }
    pw = 320 * scale;
    ph = 200 * scale;
    if (pw > view_w) {
        pw = view_w;
    }
    if (ph > view_h) {
        ph = view_h;
    }
    bottom = (view_h - ph) / 2;
    if (x) {
        *x = (view_w - pw) / 2;
    }
    if (top) {
        *top = bottom + ph;
    }
    if (w) {
        *w = pw;
    }
    if (h) {
        *h = ph;
    }
}

int pointer_classic(int sx, int sy, int view_w, int view_h, int *cx, int *cy)
{
    int left;
    int top;
    int pw;
    int ph;
    int bottom;

    screen_rect(view_w, view_h, &left, &top, &pw, &ph);
    bottom = top - ph;
    if (sx < left || sx >= left + pw || sy < bottom || sy >= top) {
        return 0;
    }
    if (cx) {
        *cx = (sx - left) * 320 / pw;
    }
    if (cy) {
        *cy = (top - sy) * 200 / ph;
    }
    return 1;
}

void classic_engine(int cx, int cy, int view_w, int view_h, int *sx, int *sy)
{
    int left;
    int top;
    int pw;
    int ph;

    screen_rect(view_w, view_h, &left, &top, &pw, &ph);
    if (cx < 0) {
        cx = 0;
    }
    if (cy < 0) {
        cy = 0;
    }
    if (cx > 319) {
        cx = 319;
    }
    if (cy > 199) {
        cy = 199;
    }
    if (sx) {
        *sx = left + cx * pw / 320;
    }
    if (sy) {
        *sy = top - cy * ph / 200;
    }
}

void blit(int pic, int cx, int cy, int cw, int ch)
{
    int sx;
    int sy;
    int sw;
    int sh;
    int left;
    int top;
    int pw;
    int ph;

    if (pic <= 0 || cw < 1 || ch < 1) {
        return;
    }
    screen_rect(df_view_w(), df_view_h(), &left, &top, &pw, &ph);
    sx = left + cx * pw / 320;
    sy = top - cy * ph / 200;
    sw = cw * pw / 320;
    sh = ch * ph / 200;
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }
    pic_draw(pic, sx, sy, sw, sh);
}

void blit_part(int pic, int sx, int sy, int sw, int sh, int cx, int cy, int cw, int ch)
{
    int dx;
    int dy;
    int dw;
    int dh;
    int left;
    int top;
    int pw;
    int ph;

    if (pic <= 0 || sw < 1 || sh < 1 || cw < 1 || ch < 1) {
        return;
    }
    screen_rect(df_view_w(), df_view_h(), &left, &top, &pw, &ph);
    dx = left + cx * pw / 320;
    dy = top - cy * ph / 200;
    dw = cw * pw / 320;
    dh = ch * ph / 200;
    if (dw < 1) {
        dw = 1;
    }
    if (dh < 1) {
        dh = 1;
    }
    pic_draw_part(pic, sx, sy, sw, sh, dx, dy, dw < 1 ? 1 : dw, dh, 255);
}

void hud_screen_rect(int view_w, int view_h, int *top, int *height)
{
    int h;
    h = view_w * 46 / 320;
    if (h >= view_h) {
        h = view_h - 1;
    }
    if (h < 1) {
        h = 1;
    }
    if (top) {
        *top = h;
    }
    if (height) {
        *height = h;
    }
}

int world_pointer(int sx, int sy, int view_w, int view_h, int bar, int *cx, int *cy)
{
    int bh;
    int y;
    if (view_w < 1 || view_h < 2 || sx < 0 || sx >= view_w || sy < 0 || sy >= view_h) {
        return 0;
    }
    hud_screen_rect(view_w, view_h, 0, &bh);
    if (bar && sy < bh) {
        y = 154 + (bh - 1 - sy) * 46 / bh;
    } else {
        y = (view_h - 1 - sy) * (bar ? 154 : 200) / (view_h - (bar ? bh : 0));
    }
    if (cx) {
        *cx = sx * 320 / view_w;
    }
    if (cy) {
        *cy = y;
    }
    return 1;
}

void world_engine(int cx, int cy, int view_w, int view_h, int bar, int *sx, int *sy)
{
    int bh;
    hud_screen_rect(view_w, view_h, 0, &bh);
    if (sx) {
        *sx = cx * view_w / 320;
    }
    if (sy) {
        *sy = bar && cy >= 154 ? bh - 1 - (cy - 154) * bh / 46
            : view_h - 1 - cy * (view_h - (bar ? bh : 0)) / (bar ? 154 : 200);
    }
}

void hud_blit_part(int pic, int sx, int sy, int sw, int sh, int cx, int cy, int cw, int ch)
{
    int bottom;
    int top;
    int width;
    int scale_h;
    width = df_view_w();
    scale_h = width * 200 / 320;
    top = (200 - cy) * scale_h / 200;
    bottom = (200 - cy - ch) * scale_h / 200;
    pic_draw_part(pic, sx, sy, sw, sh, cx * width / 320, top,
                  (cx + cw) * width / 320 - cx * width / 320, top - bottom, 255);
}

void hud_blit(int pic, int cx, int cy, int cw, int ch)
{
    int w, h;
    if (pic_size(pic, &w, &h)) {
        hud_blit_part(pic, 0, 0, w, h, cx, cy, cw, ch);
    }
}
