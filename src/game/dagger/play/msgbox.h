#ifndef DF_MSGBOX_H
#define DF_MSGBOX_H

/*
 * Classic's message box: words on a scroll, and Yes and No when it asks, as
 * DFU's DaggerfallMessageBox. Read docs/game.md (Quests) before editing.
 *
 * The text is cut at spaces and its own line breaks and the scroll sized to
 * it, up to most of the screen; longer text is cut. Without Yes and No any
 * click or key closes it. Plain C: hostcheck drives it by clicks.
 */

enum {
    MSGBOX_STAY,
    MSGBOX_OK,
    MSGBOX_YES,
    MSGBOX_NO
};

/* 0 when a picture is missing. The text is copied. */
int msgbox_open(const char *text, int yes_no);
void msgbox_close(void);
void msgbox_compose(void);
const unsigned char *msgbox_pixels(void);
int msgbox_changed(void);
int msgbox_click(int x, int y);
/* Enter (Yes), Escape (No), Y and N; without Yes and No, any key is OK. */
int msgbox_key(int vk);

/* For the tests: where Yes and No are drawn, classic's pixels. */
void msgbox_buttons(int *yes_x, int *no_x, int *y);

#endif
