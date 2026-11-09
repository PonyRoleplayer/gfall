#ifndef OGFALL_OPTIONS_H
#define OGFALL_OPTIONS_H

/* Classic Options and modal routing. Read docs/ogfall.md. Geometry is shared
 * by its compositor and controller; Detail is stored without renderer effect. */
#define OG_OPT_CONTINUE 9
#define OG_OPT_EXIT 2
#define OG_OPT_FULL 6
#define OG_OPT_BOB 7
#define OG_OPT_CONTROLS 8
#define OG_OPT_MOUSE OG_OPT_CONTROLS

enum { OG_PANEL_OPTIONS, OG_PANEL_CONTROLS, OG_PANEL_FILES };
void og_options_reset(void);
int og_options_up(void);
int og_options_panel(void);
unsigned og_options_revision(void);
const char *og_options_error(void);
void og_options_open(void);
void og_options_open_files(int saving);
void og_options_back(void);
void og_options_close(void);
int og_options_commit(void);
int og_options_flags(void);
void og_options_set_flags(int flags);
int og_options_mouse(void);
void og_options_set_mouse(int mode);
int og_options_click(int x, int y, int *sound, int *quit);
void og_options_drag(int x, int held);
void og_options_cancel_drag(void);

#endif
