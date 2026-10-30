#ifndef ENG_RPGVIEW_H
#define ENG_RPGVIEW_H

#include "engine/scene/rpg.h"

/*
 * A character screen: creation's point-buy and a level's points on one
 * list. Read docs/rpg.md before editing.
 *
 * It holds no keys and no font. The game turns its own input into
 * RPGVIEW_* commands and passes its text call (ui_string, say) to
 * rpgview_draw. Plain C: hostcheck runs it.
 */

#define RPGVIEW_LINE 64

enum {
    RPGVIEW_UP,
    RPGVIEW_DOWN,
    /* A point into the attribute under the cursor, or back out. */
    RPGVIEW_RAISE,
    RPGVIEW_LOWER,
    /* The spending made final. */
    RPGVIEW_CONFIRM
};

typedef struct RpgView {
    RpgSheet *sheet;
    const RpgRules *rules;
    int cursor;
} RpgView;

typedef void (*RpgViewText)(int x, int top, const char *s);

void rpgview_open(RpgView *v, RpgSheet *sheet, const RpgRules *rules);
/* One RPGVIEW_*. 1 when it changed anything. */
int rpgview_command(RpgView *v, int command);
/* Lines the screen has: its heading, the attributes, the vitals. */
int rpgview_lines(const RpgView *v);
/* Line i as text; 0 past the end. "Level 2  XP 40/150  Points 2", ">Might 16", " Health 36/36". */
int rpgview_line(const RpgView *v, int i, char *out, int cap);
/* Everything drawn from x, top downward, lines line_h apart. */
void rpgview_draw(const RpgView *v, int x, int top, int line_h, RpgViewText text);

#endif
