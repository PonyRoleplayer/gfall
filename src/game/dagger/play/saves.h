#ifndef DF_SAVES_H
#define DF_SAVES_H

#include "game/dagger/play/save.h"

/*
 * Where the saved games are, and whose: a folder a character under saves\
 * beside the program, a file a save (S0001.SAV, S0002.SAV, numbered, never
 * named by date). Read docs/game.md (Saves) before editing.
 *
 *   saves\Anna Smith\S0001.SAV
 *   saves\Anna Smith\S0002.SAV
 *   saves\Anna Smith (2)\S0001.SAV     (another character of the same name)
 *
 * A save says who it is for inside (SaveMeta: the character's id, name, level,
 * the real time it was saved, where the game was), and the list groups by that
 * id, never by the folder's name: two Annas stay two characters. The folder's
 * name is only the character's name made safe for a file system: letters,
 * digits, spaces and hyphens kept, anything else an underscore, trailing
 * spaces dropped, Windows' device names (CON, NUL, COM1...) given a
 * trailing underscore, and " (2)", " (3)" added when another character's
 * folder has the name (compared without case: Windows does).
 *
 * Nothing older is read: there is no import of any earlier format.
 *
 * Plain C over fs.h: hostcheck runs it in a folder of its own.
 */

#define SAVES_MAX 256
#define SAVES_CHARS 64
#define SAVES_FOLDER 48
#define SAVES_FILE 16

/* One save found by saves_scan. */
typedef struct SaveEntry {
    /* Beside the program: saves\Anna Smith\S0003.SAV (it may end .OLD when a move was cut short and the .SAV is not there). */
    char path[SAVE_PATH];
    char folder[SAVES_FOLDER];
    char file[SAVES_FILE];
    SaveMeta meta;
    /* The number in its name, S0003: 3. */
    int number;
    /* Which character (saves_char) it belongs to. */
    int char_index;
} SaveEntry;

/* A character with saves: their id and name from their saves, the folder, and which entries are theirs, in saves_entry's order. */
typedef struct SaveChar {
    df_u32 id;
    char name[SAVE_NAME];
    char folder[SAVES_FOLDER];
    int first;
    int count;
} SaveChar;

/* Another folder than saves for the files, for a test; 0 puts it back. */
void saves_use_root(const char *root);
/*
 * Reads the folders and the start of every save in them, and lists them: the
 * characters by name, each one's saves newest first. How many saves there are.
 * A file that is no save of this game, a stray temporary or a half-written
 * file is not listed.
 */
int saves_scan(void);
int saves_count(void);
const SaveEntry *saves_entry(int index);
int saves_char_count(void);
const SaveChar *saves_char(int index);
/* 1 when there is any save (scans). */
int saves_any(void);
/*
 * A new save of the character state's sheet names (its id, its name): in the
 * folder their saves are in, else a new folder, the next number; written with
 * save_write. The path it went to into path (SAVE_PATH, or 0). 0 when it was
 * not written.
 */
int saves_write(const SaveState *state, char *path);
/* A save's file deleted (and its folder, when nothing is left in it); the list is scanned again. 1 when it is gone. */
int saves_delete(int entry);
/* Every save of a character deleted, and their folder; scanned again. 1 when they are gone. */
int saves_delete_char(int character);

/* The folder name a character's name is made into (SAVES_FOLDER): the rules above, without the number. For tests and the list. */
void saves_safe_name(const char *name, char *out, int cap);

#endif
