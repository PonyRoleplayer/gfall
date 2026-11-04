#include "game/dagger/play/msgbox.h"
#include "game/dagger/play/canvas.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See msgbox.h. */

#define TEXT_MOST 2048
/* The scroll: this wide, centred; its words this far in from its edges; at most this tall. */
#define BOX_W 280
#define BOX_X ((CANVAS_W - BOX_W) / 2)
#define MARGIN 12
#define BOX_MOST_H 176
#define BUTTON_GAP 40

static struct {
    int open;
    int yes_no;
    Composer composer;
    char text[TEXT_MOST];
    Rect box;
    int button_y;
    int changed;
} g;

int msgbox_open(const char *text, int yes_no)
{
    int lines;
    int h;

    msgbox_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.yes_no = yes_no;
    df_copy(g.text, text, TEXT_MOST);
    /* The scroll as tall as its words and the buttons want, centred up and down; the font is the composer's. */
    canvas_use(&g.composer);
    lines = canvas_text_lines(BOX_W - 2 * MARGIN, g.text);
    h = lines * CANVAS_LINE + 2 * MARGIN + (yes_no ? CANVAS_BUTTON_H + 4 : 0);
    h = h > BOX_MOST_H ? BOX_MOST_H : h;
    g.box.x = BOX_X;
    g.box.w = BOX_W;
    g.box.h = (df_u16)h;
    g.box.y = (df_u16)((CANVAS_H - h) / 2);
    g.button_y = g.box.y + h - MARGIN - CANVAS_BUTTON_H + 4;
    g.changed = 1;
    return 1;
}

void msgbox_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

void msgbox_buttons(int *yes_x, int *no_x, int *y)
{
    *yes_x = CANVAS_W / 2 - BUTTON_GAP / 2 - CANVAS_BUTTON_W;
    *no_x = CANVAS_W / 2 + BUTTON_GAP / 2;
    *y = g.button_y;
}

void msgbox_compose(void)
{
    static const Rect whole = { 0, 0, CANVAS_W, CANVAS_H };
    Rect inside;

    canvas_use(&g.composer);
    canvas_box(&whole, 0);
    canvas_scroll(&g.box);
    inside.x = (df_u16)(g.box.x + MARGIN);
    inside.y = (df_u16)(g.box.y + MARGIN);
    inside.w = (df_u16)(g.box.w - 2 * MARGIN);
    inside.h = (df_u16)(g.box.h - 2 * MARGIN - (g.yes_no ? CANVAS_BUTTON_H + 4 : 0));
    canvas_text(&inside, g.text);
    if (g.yes_no) {
        int yes_x;
        int no_x;
        int y;

        msgbox_buttons(&yes_x, &no_x, &y);
        canvas_button(CANVAS_BUTTON_YES, yes_x, y);
        canvas_button(CANVAS_BUTTON_NO, no_x, y);
    }
    g.changed = 0;
}

const unsigned char *msgbox_pixels(void)
{
    return g.composer.screen;
}

int msgbox_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

int msgbox_click(int x, int y)
{
    Rect yes;
    Rect no;
    int yes_x;
    int no_x;
    int by;

    if (!g.yes_no) {
        return MSGBOX_OK;
    }
    msgbox_buttons(&yes_x, &no_x, &by);
    yes.x = (df_u16)yes_x;
    yes.y = (df_u16)by;
    yes.w = CANVAS_BUTTON_W;
    yes.h = CANVAS_BUTTON_H;
    no = yes;
    no.x = (df_u16)no_x;
    if (rect_has(&yes, x, y)) {
        return MSGBOX_YES;
    }
    return rect_has(&no, x, y) ? MSGBOX_NO : MSGBOX_STAY;
}

int msgbox_key(int vk)
{
    if (!g.yes_no) {
        return MSGBOX_OK;
    }
    if (vk == 0x0D || vk == 'Y') {
        return MSGBOX_YES;
    }
    return vk == 0x1B || vk == 'N' ? MSGBOX_NO : MSGBOX_STAY;
}
