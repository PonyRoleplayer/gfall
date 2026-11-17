#ifndef GFALL_OPTIONS_H
#define GFALL_OPTIONS_H

/* Classic Options and modal routing. Read docs/game.md. Geometry is shared
 * by its compositor and controller; Detail is stored without renderer effect. */
#define OPT_CONTINUE 9
#define OPT_EXIT 2
#define OPT_FULL 6
#define OPT_BOB 7
#define OPT_CONTROLS 8
#define OPT_MOUSE OPT_CONTROLS

enum { PANEL_OPTIONS, PANEL_CONTROLS, PANEL_FILES };
void options_reset(void);
int options_up(void);
int options_panel(void);
unsigned options_revision(void);
const char *options_error(void);
void options_open(void);
void options_open_files(int saving);
void options_back(void);
void options_close(void);
int options_commit(void);
int options_flags(void);
void options_set_flags(int flags);
int options_mouse(void);
void options_set_mouse(int mode);
int options_click(int x, int y, int *sound, int *quit);
void options_drag(int x, int held);
void options_cancel_drag(void);

#endif
