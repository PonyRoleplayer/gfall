#ifndef ENG_CONSOLE_H
#define ENG_CONSOLE_H

struct Camera;

/*
 * Engine console: the panel, its keys, and the engine's commands. The
 * log is engine/core/log.h and the command table is engine/ui/cmd.h.
 * Read docs/console.md before editing.
 *
 * The key immediately left of 1 toggles it. While it is open the world
 * is not drawn and the camera must not move. Call console_pump once a
 * frame before reading movement. draw_frame also pumps, and presents
 * when the panel is up. Keys reach it through win_pump.
 */

/* One reply line, at LOG_PRINT. Same as log_text(LOG_PRINT, s). */
void console_log(const char *s);
/* The camera draw_frame is drawing, for pos. */
void console_bind(const struct Camera *cam);
/* A camera setpos may move. 0 when the program has none to offer. */
void console_set_camera(struct Camera *cam);
/* The camera the program handed over with console_set_camera, or 0. */
struct Camera *console_camera(void);
int console_open(void);
void console_pump(void);
/* 1 if this Escape press closes the console, or already did and is still held. */
int console_take_escape(void);

/*
 * win_pump hands every message here before it translates it. KEEP drops
 * it. TRANSLATE makes its WM_CHAR and drops the key itself. PASS is
 * normal delivery. Only key messages are ever kept, and only while a
 * frame loop is pumping the console.
 */
enum {
    CONSOLE_PASS = 0,
    CONSOLE_KEEP = 1,
    CONSOLE_TRANSLATE = 2
};
int console_message(unsigned int msg, unsigned int wp, long lp);

void console_present(const struct Camera *cam);
/* The grabbed frame is stale once the world draws again. */
void console_release(void);

#endif
