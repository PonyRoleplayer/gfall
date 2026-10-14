#ifndef DF_WIN32_H
#define DF_WIN32_H

/*
 * 640x480 popup. The content directory is <exe dir><suffix>,
 * for example suffix "/ARENA2". The frame loop pumps, then reads input.
 */

int win_startup(void *instance, const char *suffix);
void win_shutdown(void);
void win_pump(void);
void *win_hwnd(void);
void win_fail(const char *text);

#endif
