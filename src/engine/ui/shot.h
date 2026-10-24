#ifndef ENG_SHOT_H
#define ENG_SHOT_H

/*
 * Screenshots. The console only dispatches the command. F12 and the
 * fading note are here. Read docs/console.md before editing.
 */

/* Write the front buffer as a BMP beside the program. A null or empty
   name takes the F12 time-stamp name. 0 on a bad name or a failed
   write. shot_status() is the line to log either way. */
int shot_save(const char *name);
const char *shot_status(void);

/* F12. Null when the key did not edge. Otherwise the log line. */
const char *shot_pump(void);

/* "name taken..." at the top right. Solid, then it fades. */
void shot_note(void);

#endif
