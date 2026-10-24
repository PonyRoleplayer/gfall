#ifndef ENG_CMD_H
#define ENG_CMD_H

/*
 * Console commands, variables, the typed line, and its history. Plain C
 * with no Win32 and no Glide, so hostcheck runs it. console.c feeds it
 * keys and registers the engine's own commands. A game may add its own
 * with cmd_add before or after that. Read docs/console.md.
 *
 * Names are static strings, lower case, and are not copied.
 * Replies go to the log at LOG_PRINT, so a reply shows at any log level.
 */

#define CMD_ARGS 16
/* The typed line, and each history entry, including the terminator. */
#define CMD_LINE 96
#define CMD_HISTORY 32

/* argv[0] is the name as typed, lower case. */
typedef void (*cmd_fn)(int argc, char **argv);

/* 0 when the table is full or the name is taken. */
int cmd_add(const char *name, cmd_fn fn, const char *usage, const char *help);

/*
 * A number the console reads and writes. "name" prints it. "name v"
 * clamps v to lo..hi, calls set, and prints what get returns after.
 * A variable with lo 0 and hi 1 is a switch: it prints on or off, and
 * takes on, off, 1, 0, or toggle.
 */
int cmd_var(const char *name, int (*get)(void), void (*set)(int),
            int lo, int hi, const char *help);

/*
 * Runs a line. ';' and newlines separate commands. "//" and a '#' at
 * the start of a command begin a comment. Double quotes keep spaces in
 * one argument. Returns how many commands ran, unknown ones included.
 */
int cmd_run(const char *text);

/* 1 and the value when s is a whole decimal integer, sign allowed. */
int cmd_parse_int(const char *s, int *out);
/* 1 and 0 or 1 when s is on, off, 1, 0, yes, or no. */
int cmd_parse_switch(const char *s, int *out);

/* Editing keys. Characters go through cmd_line_char. */
enum {
    CMD_KEY_LEFT,
    CMD_KEY_RIGHT,
    CMD_KEY_HOME,
    CMD_KEY_END,
    CMD_KEY_BACK,
    CMD_KEY_DELETE,
    CMD_KEY_UP,
    CMD_KEY_DOWN,
    CMD_KEY_TAB,
    CMD_KEY_ENTER,
    CMD_KEY_KILL
};

/* Inserts a printable character, 32..126, at the cursor. */
void cmd_line_char(int ch);
/*
 * UP and DOWN walk the history, newest first, and DOWN past the newest
 * gives back the line that was being typed. TAB completes the word at
 * the cursor. ENTER logs "> line", keeps it in the history unless it
 * repeats the newest entry, and runs it. KILL empties the line.
 */
void cmd_line_key(int key);
const char *cmd_line_text(void);
int cmd_line_cursor(void);

int cmd_history_count(void);
/* 0 is the newest. "" past the end. */
const char *cmd_history(int index);

#endif
