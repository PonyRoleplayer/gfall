#ifndef GFALL_CONTROLS_H
#define GFALL_CONTROLS_H

/* The original 38-key control grid and mouse panel. Read docs/game.md. */
typedef struct ControlRect { int x0, y0, x1, y1; } ControlRect;
const ControlRect *control_rect(int index);
void controls_open(void);
int controls_editing(void);
int controls_mouse(void);
const char *controls_notice(void);
unsigned controls_revision(void);
int controls_duplicate(int action);
/* Returns 1 to leave Controls. Escape cancels capture before leaving. */
int controls_key(int key);
int controls_click(int x, int y);
void controls_drag(int x, int y, int held);
void controls_cancel_drag(void);
void control_key_name(int action, char *text, int capacity);

#endif
