#ifndef GFALL_SCREEN_INTERNAL_H
#define GFALL_SCREEN_INTERNAL_H

#include "game/dagger/formats/smallfont.h"

/* Shared primitives and assets for the classic 320 x 200 presenters. */
df_u8 *gfall_screen_pixels(void);
int gfall_screen_prepare_pixels(void);
const SmallFont *gfall_screen_font(void);
const df_u8 *gfall_screen_art_palette(void);
int gfall_screen_have_art_palette(void);
int gfall_screen_paper_ready(void);
void gfall_screen_put_px(int x, int y, int colour);
void gfall_screen_blit_ix(int dx, int dy, const df_u8 *src, int sw, int sh, int key);
void gfall_screen_cover_screen(const df_u8 *src, int w, int h);
void gfall_screen_lay_paper(int x0, int y0, int cols, int rows);
void gfall_screen_lay_button(int frame, int x, int y);
void gfall_screen_paper_box(char lines[][96], int n, int buttons, int *x0, int *y0, int *cols, int *rows, int *text_y);
void gfall_screen_paint_lines(char lines[][96], int n, int centered, int x0, int text_y, int ink);
void gfall_screen_text_at(int x, int y, const char *text);
void gfall_screen_number_at(int x, int y, int value, int centered, int sign);
void gfall_screen_publish_surface(int kind, int cutout);
void gfall_screen_fill_box(int x, int y, int w, int h, int color);
void gfall_screen_outline(int x, int y, int w, int h, int color);
void gfall_screen_centered_text(int x, int y, const char *text, int ink);

void gfall_screen_clear_composition(void);
void gfall_screen_copy_map(const df_u8 *pixels);
void gfall_screen_attribute_arrows(int x, int y);
void gfall_screen_load_rest(void);
const df_u8 *gfall_screen_map_palette(void);
void gfall_screen_present_maps(void);
int gfall_screen_load_overlays(void);
void gfall_screen_load_possessions(void);
int gfall_screen_present_possessions(void);
void gfall_screen_reset_possessions(void);

#endif
