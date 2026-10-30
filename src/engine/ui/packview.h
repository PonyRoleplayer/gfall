#ifndef ENG_PACKVIEW_H
#define ENG_PACKVIEW_H

#include "engine/scene/pack.h"

/*
 * A list screen over a pack, or over two side by side (a player's and a
 * chest's): a cursor, scrolling, wearing, moving things across. Read
 * docs/pack.md before editing.
 *
 * It holds no keys and no font. The game turns its own input into
 * PACKVIEW_* commands and passes its text call (ui_string, say) to
 * packview_draw. Plain C: hostcheck runs it.
 */

#define PACKVIEW_LINE 64

enum {
    PACKVIEW_UP,
    PACKVIEW_DOWN,
    /* To the other pack. */
    PACKVIEW_SWITCH,
    /* Wear the thing under the cursor, or take it off. */
    PACKVIEW_USE,
    /* One of it across to the other pack. */
    PACKVIEW_MOVE,
    /* Everything from this pack to the other. */
    PACKVIEW_MOVE_ALL
};

typedef struct PackView {
    /* pack[1] is 0 for one pack alone. */
    Pack *pack[2];
    int side;
    int cursor[2];
    int top[2];
    /* Lines of things shown a pack. */
    int rows;
} PackView;

typedef void (*PackViewText)(int x, int top, const char *s);

void packview_open(PackView *v, Pack *left, Pack *right, int rows);
/* One PACKVIEW_*. 1 when it changed anything. */
int packview_command(PackView *v, int command);
/* Row (of those shown) of a side as text: 0 past its end. "> * Name x3  1.5": cursor, worn, count, weight. */
int packview_line(const PackView *v, int side, int row, char *out, int cap);
/* A side's heading: its weight, its limit and its money. */
void packview_heading(const PackView *v, int side, char *out, int cap);
/* Everything drawn from x, top downward, sides each half of width, lines line_h apart. */
void packview_draw(const PackView *v, int x, int top, int width, int line_h, PackViewText text);

#endif
