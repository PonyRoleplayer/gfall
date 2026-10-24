#ifndef DF_WIN32_H
#define DF_WIN32_H

/*
 * 640x480 popup. The content directory is <exe dir><suffix>.
 * The frame loop pumps, then reads input.
 * title is the window caption and the dialog title. gui turns on a
 * system cursor and a window background so a child list can sit on it.
 */

int win_startup(void *instance, const char *suffix, const char *title, int gui);
void win_shutdown(void);
void win_pump(void);
/* 1 after the title bar cross (or Alt+F4, or win_quit). Stays set; Esc does not set it. */
int win_closed(void);
/*
 * Leave the program, as the title-bar cross does: win_closed turns 1,
 * the frame loop sees a quit, and a list loop sees WM_QUIT.
 */
void win_quit(void);
void *win_hwnd(void);
void win_fail(const char *text);

/* Mouse buttons and moves, in client coordinates. 0 clears the hook. */
void win_mouse_hook(void (*fn)(unsigned int msg, int x, int y));
/* Child notifications (button press). 0 clears the hook. */
void win_command_hook(void (*fn)(unsigned int id, unsigned int code));
/* WM_NOTIFY from a common control, such as a list-view header. 0 clears it. */
void win_notify_hook(void (*fn)(void *hdr));
/* Windows 98 dialog face: MS Sans Serif, not the bold System font. */
void *win_ui_font(void);
/* Hide the cursor while Glide owns the screen. */
void win_blank_cursor(int on);

#endif
