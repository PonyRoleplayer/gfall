#ifndef ENG_CONSOLE_H
#define ENG_CONSOLE_H

struct Camera;

/*
 * Engine console. The key immediately left of 1 toggles it.
 * While it is open the world is not drawn and the camera must not move.
 * console_log keeps a fixed number of lines. Call console_pump once a
 * frame before reading movement. draw_frame also pumps, and presents
 * when the panel is up.
 */

void console_log(const char *s);
void console_bind(const struct Camera *cam);
int console_open(void);
void console_pump(void);
/* 1 if this Escape press closes the console instead of quitting. */
int console_take_escape(void);
void console_present(const struct Camera *cam);
/* The grabbed frame is stale once the world draws again. */
void console_release(void);

#endif
