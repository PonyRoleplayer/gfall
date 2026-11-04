#ifndef DF_TEXTRSC_H
#define DF_TEXTRSC_H

/*
 * TEXT.RSC: classic's own words, by record number. Read from ARENA2 when
 * wanted, never copied. The races' descriptions are records 2000 and on
 * (RACE's description), the classes' 2100 and on.
 *
 * The file: a u16 header length, then a u16 id and a u32 offset for each
 * record, then the records. Inside one, 0xFD ends a line and 0xFE ends the
 * record, 0xFC ends a line too in the longest ones; other bytes past ASCII
 * steer classic's text box, and are dropped. 0xFF between lines parts a
 * record's variants: what a person says is one of them, chosen at random.
 *
 * A quest's QRC (formats/qbn.h) is the same file format, its messages by
 * number: the calls ending _in read one of those by its name.
 *
 * Plain C: hostcheck runs it.
 */

/*
 * Record id as plain text in out (cap bytes), a line break for each of
 * classic's, always ended. Its length, or -1 when it does not read.
 */
int textrsc_read(int id, char *out, int cap);
/* How many variants record id has; 0 when it does not read. */
int textrsc_variants(int id);
/* Variant which of record id, counted round its variants, in out as textrsc_read. Its length, or -1. */
int textrsc_variant(int id, int which, char *out, int cap);
/* The same three, from another file of this format in ARENA2 (a quest's "M0B00Y00.QRC"). */
int textrsc_read_in(const char *name, int id, char *out, int cap);
int textrsc_variants_in(const char *name, int id);
int textrsc_variant_in(const char *name, int id, int which, char *out, int cap);

#endif
