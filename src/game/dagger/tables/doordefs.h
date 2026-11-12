#ifndef GFALL_DOORDEFS_H
#define GFALL_DOORDEFS_H

/* FALL.EXE's lock messages, extracted by tools/gfall-data.py.
 * See docs/game.md for the source and interaction rules. */
enum { LOCK_MAGIC = 13, LOCK_UNLOCKED, LOCK_FAILED, LOCK_OPEN };
const char *lock_text(int message);
const char *lock_hint(int skill, int lock);

#endif
