#ifndef DF_SCREEN_H
#define DF_SCREEN_H

/*
 * Classic's 320 x 200 screen on the card's 640 x 480: x twice, y 2.4
 * times, and the card's y counting up from the bottom (ui_string's and
 * pic_draw's frame). The HUD, the menus and their pointer all place
 * classic's pieces where classic put them through these.
 */

#define SCREEN_W 320
#define SCREEN_H 200

/* A classic x or width on the card. */
int screen_x(int x);
/* A classic height on the card. */
int screen_h(int h);
/* The card's top for a classic y (the distance from the bottom of the card's screen). */
int screen_top(int y);
/* The pointer (input_pointer: the card's pixels from the lower left) as a classic x, y. */
void screen_from_pointer(int px, int py, int *x, int *y);
/* 1 when classic's x, y falls in the rectangle left, top, w, h. */
int screen_in(int x, int y, int left, int top, int w, int h);
/* A picture at classic's left, top, its own size scaled. */
void screen_draw(int pic, int left, int top, int w, int h);
/* A line of text at classic's left, top (the font's top). */
void screen_text(int left, int top, const char *s);

#endif
