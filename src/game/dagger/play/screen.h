#ifndef GFALL_SCREEN_H
#define GFALL_SCREEN_H

/*
 * Classic's 320 x 200, origin top-left, drawn at 2x on 640 x 480:
 * pic_draw(id, 0, 440, 640, 400). The pointer comes back from the
 * engine's lower-left frame. Letterbox pixels are outside the picture.
 */

/* 1 when the engine point lands on the picture. Classic pixels out. */
int pointer_classic(int sx, int sy, int view_w, int view_h, int *cx, int *cy);
/* The engine point of a classic pixel, lower-left origin. */
void classic_engine(int cx, int cy, int view_w, int view_h, int *sx, int *sy);
/* Where the 320 x 200 picture sits: left, top (lower-left origin), width, height. */
void screen_rect(int view_w, int view_h, int *x, int *top, int *w, int *h);
/* HUD art keeps its scale and is anchored to the bottom of the glass.
 * World pointer coordinates use the visible area above it, never menu letterboxing. */
void hud_screen_rect(int view_w, int view_h, int *top, int *height);
int world_pointer(int sx, int sy, int view_w, int view_h, int bar, int *cx, int *cy);
void world_engine(int cx, int cy, int view_w, int view_h, int bar, int *sx, int *sy);
void hud_blit(int pic, int cx, int cy, int cw, int ch);
void hud_blit_part(int pic, int sx, int sy, int sw, int sh, int cx, int cy, int cw, int ch);
/* A classic rectangle, scaled with the 320 x 200 picture. pic 0 draws nothing. */
void blit(int pic, int cx, int cy, int cw, int ch);
/* A source rectangle of pic into a classic rectangle. */
void blit_part(int pic, int sx, int sy, int sw, int sh, int cx, int cy, int cw, int ch);
/* Classic size_fit: shrink only, with its original 8-bit scale factor. */
void size_fit(int *width, int *height, int max_width, int max_height);

#endif
