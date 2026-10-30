#ifndef DF_CANVAS_H
#define DF_CANVAS_H

#include "engine/core/mem.h"
#include "game/dagger/formats/smallfont.h"

/*
 * Classic's 320 x 200 screen, composed in memory: what the inventory, the
 * character sheet and character creation are all drawn with. Read
 * docs/game.md (The inventory screen, What it costs the 233).
 *
 * A screen is one picture of palette indices under ART_PAL.COL, composed
 * only when something on it changes, and put on the card as one picture
 * (screens.c). Classic's own pieces land at classic's own pixels and its
 * labels in its own font, and an open frame costs no download.
 *
 * A Composer is what one open screen holds: the picture, the small font, a
 * heap for what it keeps, and a scratch heap for pictures it only reads to
 * copy. All of it goes when the screen closes. Plain C: hostcheck runs it.
 */

#define CANVAS_W 320
#define CANVAS_H 200
#define CANVAS_BYTES (CANVAS_W * CANVAS_H)
/* One line of the small font and its gap. */
#define CANVAS_LINE 8
/* The ink classic's own labels are drawn in (INVE00I0.IMG's tab words), its shadow, and a dark box, all under ART_PAL.COL. */
#define CANVAS_INK 146
#define CANVAS_INK_SHADOW 156
#define CANVAS_DARK 222

/* A rectangle on classic's screen. */
typedef struct Rect {
    df_u16 x;
    df_u16 y;
    df_u16 w;
    df_u16 h;
} Rect;

typedef struct Composer {
    MemHeap *heap;
    MemHeap *scratch;
    df_u8 *screen;
    SmallFont font;
    /* SPOP.RCI's nine pieces of a scroll, and BUTTONS.RCI's buttons; 0 when they did not read. */
    const df_u8 *scroll;
    const df_u8 *buttons;
} Composer;

/* Its heaps, its picture and FONT0003.FNT. 0, with everything let go, when any did not come. */
int composer_open(Composer *c);
void composer_close(Composer *c);
/* A whole-screen or any IMG's pixels, in the composer's heap; 0 when it did not read. */
const df_u8 *composer_read(Composer *c, const char *name);
/* A file's bytes whole in the composer's heap, for a table read once; 0 when it did not read. */
const df_u8 *composer_table(Composer *c, const char *name, df_u32 *size);
/* The scratch heap in use until composer_scratch_end; the old heap back. Let go first, if it has grown too large. */
MemHeap *composer_scratch_begin(Composer *c);
void composer_scratch_end(MemHeap *was);

/* What the calls below draw on, until another composer takes over. */
void canvas_use(Composer *c);
/* 1 when x, y falls in r. */
int rect_has(const Rect *r, int x, int y);
/* A whole 320 x 200 picture as the screen. */
void canvas_copy_all(const df_u8 *from);
/* The same rectangle of a whole-screen picture onto the screen. */
void canvas_copy_rect(const df_u8 *from, const Rect *r);
/* A part of a picture w wide, from its sx, sy, sw x sh, onto the screen at x, y, every pixel. */
void canvas_copy_part(const df_u8 *from, int w, int sx, int sy, int sw, int sh, int x, int y);
/* A picture with index 0 clear onto the screen at x, y, clipped to it. */
void canvas_cutout(const df_u8 *from, int w, int h, int x, int y);
/*
 * Classic's parchment over r, its sides rounded up to its 22 x 22 pieces
 * (DFU's popup style: SPOP.RCI, corners, sides and a middle, three rows of
 * three). What is under its burnt edges shows.
 */
void canvas_scroll(const Rect *r);
/* One of BUTTONS.RCI's, 32 x 16, by DFU's numbers. */
enum {
    CANVAS_BUTTON_YES = 3,
    CANVAS_BUTTON_NO = 4,
    CANVAS_BUTTON_MALE = 6,
    CANVAS_BUTTON_FEMALE = 7
};
#define CANVAS_BUTTON_W 32
#define CANVAS_BUTTON_H 16
void canvas_button(int which, int x, int y);
/* The ink of the labels from here on, and its shadow (-1 for none): for a picture under its own palette. canvas_use puts classic's back. */
void canvas_set_ink(int ink, int shadow);
/* A filled rectangle of one palette index. */
void canvas_box(const Rect *r, int index);
/* A line in classic's ink with its top left at x, y. */
void canvas_label(int x, int y, const char *text);
/* A line centred across r, at r's top and an offset down. */
void canvas_label_centred(const Rect *r, int down, const char *text);
/* The width of a line, in pixels. */
int canvas_width(const char *text);
/* Text in r, cut at spaces and at its own line breaks, as many lines as fit. How many lines it took. */
int canvas_text(const Rect *r, const char *text);
/* How many lines canvas_text would take for this width, drawing nothing. */
int canvas_text_lines(int width, const char *text);

#endif
