#ifndef OGFALL_CONTROLS_H
#define OGFALL_CONTROLS_H

/* The original 38-key control grid and mouse panel. Read docs/ogfall.md. */
typedef struct OgControlRect { int x0, y0, x1, y1; } OgControlRect;
const OgControlRect *og_control_rect(int index);
void og_controls_open(void);
int og_controls_editing(void);
int og_controls_mouse(void);
const char *og_controls_notice(void);
unsigned og_controls_revision(void);
int og_controls_duplicate(int action);
/* Returns 1 to leave Controls. Escape cancels capture before leaving. */
int og_controls_key(int key);
int og_controls_click(int x, int y);
void og_controls_drag(int x, int y, int held);
void og_controls_cancel_drag(void);
void og_control_key_name(int action, char *text, int capacity);

#endif
