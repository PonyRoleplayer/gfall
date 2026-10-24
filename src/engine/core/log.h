#ifndef ENG_LOG_H
#define ENG_LOG_H

#include <stdarg.h>

#include "engine/core/df.h"

/*
 * The engine log. Every subsystem writes here and the console shows it.
 * It is plain C with no Win32, so hostcheck links it too.
 *
 * A line has a level. LOG_PRINT is a console reply and is always kept.
 * The others are kept only up to the level log_set_level chose, so a
 * debug line costs one compare when debug is off: nothing is formatted.
 * Default is LOG_INFO. Error and warning lines get an "error: " or
 * "warning: " prefix in the text itself, so the file mirror carries it.
 *
 * The ring holds LOG_LINES lines of LOG_COLS - 1 characters. Past that
 * the oldest line is dropped. A longer line is cut, not wrapped. Callers
 * write one line per call. Do not log from a per-frame loop except on a
 * change of state.
 */

#define LOG_LINES 256
#define LOG_COLS 96

enum {
    LOG_PRINT = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4
};

void log_set_level(int level);
int log_level(void);
/* "error", "warn", "info", "debug". Unknown levels read "print". */
const char *log_level_name(int level);
/* 1 if a line at this level would be kept. */
int log_wants(int level);

void log_text(int level, const char *s);
void log_print(int level, const char *fmt, ...);

/*
 * printf subset into dst, always terminated, never past cap. Returns
 * the length written. %d %i %u %x %c %s %f %%, with '-' and '0' flags,
 * a width, and a precision (%.2f, %.8s). %f takes at most 4 decimals
 * and prints "big" once value times 10^decimals passes 4e9. l is
 * accepted and ignored.
 */
int log_vformat(char *dst, int cap, const char *fmt, va_list ap);
int log_format(char *dst, int cap, const char *fmt, ...);

/* Lines kept, newest first: index 0 is the last line logged. */
int log_count(void);
const char *log_get(int index);
int log_get_level(int index);
/* Counts every line ever kept. The console uses it to hold its scroll. */
df_u32 log_serial(void);
void log_clear(void);

/*
 * Called with every kept line, after its prefix. The console points it
 * at the log file while logfile is on. 0 clears it.
 */
void log_set_sink(void (*fn)(const char *line));

#endif
