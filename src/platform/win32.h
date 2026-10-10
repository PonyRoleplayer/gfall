#ifndef DF_WIN32_H
#define DF_WIN32_H

/*
 * 640x480 popup. Sets the file root to ARENA2 beside the exe.
 * The frame loop in main.c pumps, then reads input.
 */

int win_startup(void *instance);
void win_shutdown(void);
void win_pump(void);
void *win_hwnd(void);
void win_fail(const char *text);

#endif
