#ifndef OGFALL_DOORDEFS_H
#define OGFALL_DOORDEFS_H

/* FALL.EXE's lock messages, extracted by tools/ogfall-data.py.
 * See docs/ogfall.md for the source and interaction rules. */
enum { OG_LOCK_MAGIC = 13, OG_LOCK_UNLOCKED, OG_LOCK_FAILED, OG_LOCK_OPEN };
const char *og_lock_text(int message);
const char *og_lock_hint(int skill, int lock);

#endif
