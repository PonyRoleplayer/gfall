#ifndef DF_UI_H
#define DF_UI_H

#include "engine/core/df.h"

/*
 * The game's widgets: the stone-and-gold look every native screen is built
 * from (the settings shell, the saved games list, and the next screens), in
 * 640 x 480 top-down coordinates. Read docs/game.md (Menus, Widgets).
 *
 * Plain C: it emits atlas rectangles through a callback, which menu.c submits
 * with pic_draw_part and hostcheck rasterizes for a picture. There are no
 * words in the art: the three atlases are a 128-square stone, rust and moss
 * swatch sheet, a 256-square sheet of FONT0003.FNT enlarged, and a 128-square
 * compact face of it, built once at ui_open in a heap of their own (96 KB of
 * texels, gold ink with a one-pixel shadow). Labels are runtime strings, and
 * ui_translate lets a provider replace English ones.
 *
 * Drawing and hit-testing share one geometry. A control draws itself and says
 * where it answers (ui_region: an id and a rectangle); ui_hit asks which
 * control is under a point from what the last draw said. A frame that only
 * needs the hit (a test, or a click before the next draw) draws to no
 * target, ui_begin(0, 0), which makes the same regions without a quad.
 *
 * What a screen is made of:
 *   ui_panel, ui_button, ui_text, ui_centered, ui_paragraph, ui_flat
 *   UiList        a column of rows with the arrows and thumb at its side
 */

#define UI_ATLASES 3
#define UI_NOTHING (-1)

/* One rectangle of an atlas onto the screen, 640 x 480 top-down. */
typedef void (*UiQuad)(void *ctx, int atlas, int sx, int sy, int sw, int sh, int x, int y, int w, int h);
typedef const char *(*UiTranslate)(const char *english);

/* The atlases and their palette, built the first time; 0 when FONT0003.FNT will not read. ui_close lets them go. */
int ui_open(void);
void ui_close(void);
const df_u8 *ui_atlas(int index, int *side);
const df_u8 *ui_palette(void);
/* The screen's pixels against the layout's (e.g. 512 and 640): widths are given back in layout pixels,
 * and a glyph's quad carries its size in native pixels for the caller to draw unscaled. 1:1 by default. */
void ui_scale(int native, int layout);
void ui_translate(UiTranslate translate);
/* A label as the translator has it, else as written. */
const char *ui_tr(const char *english);

/* Where the next controls draw (quad 0: nowhere), and the regions made so far forgotten. */
void ui_begin(UiQuad quad, void *ctx);

/* The pieces. Colours and styles are the shell's: palette index, and 0 stone, 1 rust, 2 moss. */
void ui_flat(int x, int y, int w, int h, int color);
void ui_panel(int x, int y, int w, int h, int style);
/* The width of a line in a face: 0 the large one, 1 the compact one. */
int ui_width(int face, const char *s);
void ui_text(int x, int y, int max_w, const char *s, int face);
/* A label across w, in the large face when it fits and the compact when not. */
void ui_centered(int x, int y, int w, const char *s);
void ui_button(int x, int y, int w, int h, const char *s, int style, int hot);
/* Text wrapped at spaces in w, as many lines as h holds. */
void ui_paragraph(int x, int y, int w, int h, const char *s);

/* A control's answer: id for any point inside the rectangle. A later region over an earlier one wins. */
void ui_region(int id, int x, int y, int w, int h);
/* The id of the region the point is in, or UI_NOTHING. */
int ui_hit(int x, int y);
/* The digit a key types, '0' to '9', from the top row or the numeric keypad (VK_NUMPAD0..9); 0 for any other key. */
int ui_digit(int vk);

/*
 * A list of rows: where it stands, how many rows show, how tall each is, how
 * many there are in all and which is first. The arrows and the thumb stand
 * to its right in a column of their own, UI_BAR_W wide.
 */
#define UI_BAR_W 18
typedef struct UiList {
    int x;
    int y;
    int w;
    int rows;
    int row_h;
    int count;
    int scroll;
} UiList;
/* The scroll moved by delta rows and kept within the list. */
void ui_list_move(UiList *list, int delta);
/* The scroll kept so that row is on the list. */
void ui_list_show(UiList *list, int row);
/* Which row (0..count-1) a point is on, or -1: the rows' own width only. */
int ui_list_row_at(const UiList *list, int x, int y);
/* The bar to the list's right: its panel, the two arrow buttons (regions up_id and down_id) and the thumb. */
void ui_list_bar(const UiList *list, int up_id, int down_id, int up_hot, int down_hot);

#endif
