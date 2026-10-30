#ifndef DF_MAINDAT_H
#define DF_MAINDAT_H

#include "engine/assets/recfile.h"

/*
 * MAIN.DAT and the .DAT files over it: Daggerfall's tables as records.
 * Read docs/data.md before editing.
 *
 * The first run cooks MAIN.DAT in DATA\ beside the exe: classic's tables that
 * live inside FALL.EXE (classic.c), and later what ARENA2's files hold,
 * read through formats/. Models, pictures, blocks and sounds stay in
 * ARENA2. At every start the game merges MAIN.DAT, then each file
 * LOADORDR.TXT names, into its tables by id (recfile.h): a new id adds a
 * record, an existing one takes the fields that file carries. Every one
 * of them sits in DATA\, LOADORDR.TXT too.
 *
 * Plain C: the programs and hostcheck run it. Reads go through the VFS
 * (ARENA2 must be mounted); maindat_start reads and writes beside it.
 */

/* The folder beside the program that holds MAIN.DAT, LOADORDR.TXT and the mods. */
#define MAINDAT_DIR "DATA"
#define MAINDAT_NAME "MAIN.DAT"
#define MAINDAT_ORDER "LOADORDR.TXT"
/* The importer's version. A change makes every MAIN.DAT stale, so the next start cooks it again. */
#define MAINDAT_VERSION 4

/* The tables a .DAT can carry, in the order the importer writes them. */
int maindat_table_count(void);
RecTable *maindat_table(int i);
/* A table by its chunk tag (ENEM) or its name in a tool's text (enemy); 0 for none. */
RecTable *maindat_find(const char *tag_or_name);

/* Writes MAIN.DAT's bytes: its HEAD, then each table classic's data fills. */
int maindat_import(RecPut put, void *ctx);
/* 1 when bytes are not a MAIN.DAT this importer made from the ARENA2 mounted now. */
int maindat_stale(const df_u8 *bytes, df_u32 n);

/* Every table empty, and no file merged. */
void maindat_reset(void);
/*
 * One .DAT into the tables, after those merged so far. A file whose HEAD
 * needs one not merged yet is skipped whole. 0 when skipped or not a
 * record file; the log says why.
 */
int maindat_merge(const df_u8 *bytes, df_u32 n, const char *name);
/*
 * 1 when the tables hold what MAIN.DAT must give (every table some
 * records, the calendar its record) and no chunk was refused as damaged
 * since the last reset.
 */
int maindat_whole(void);
/* The files merged so far, in order. */
int maindat_merged(void);
const char *maindat_merged_name(int i);
/* After the last merge: hands the engine the values it takes from data: the weather's looks, the calendar. */
void maindat_apply(void);

/*
 * A program's start: DATA\MAIN.DAT beside the exe (the folder made when
 * missing), cooked when missing or stale, and cooked again once when it
 * merges damaged; then the files DATA\LOADORDR.TXT names, from DATA\,
 * then maindat_apply. A damaged mod is skipped with a log line. 0 when
 * MAIN.DAT cannot be written, or is damaged even as cooked:
 * maindat_error says what to do, for a person.
 */
int maindat_start(void);
const char *maindat_error(void);

#endif
