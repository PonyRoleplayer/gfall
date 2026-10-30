#ifndef DF_TEXTRSC_H
#define DF_TEXTRSC_H

/*
 * TEXT.RSC: classic's own words, by record number. Read from ARENA2 when
 * wanted, never copied. The races' descriptions are records 2000 and on
 * (RACE's description), the classes' 2100 and on.
 *
 * The file: a u16 header length, then a u16 id and a u32 offset for each
 * record, then the records. Inside one, 0xFD ends a line and 0xFE ends the
 * record; other bytes past ASCII steer classic's text box, and are dropped.
 *
 * Plain C: hostcheck runs it.
 */

/*
 * Record id as plain text in out (cap bytes), a line break for each of
 * classic's, always ended. Its length, or -1 when it does not read.
 */
int textrsc_read(int id, char *out, int cap);

#endif
