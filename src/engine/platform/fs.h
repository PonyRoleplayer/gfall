#ifndef DF_FS_H
#define DF_FS_H

#include "engine/core/df.h"

/* Paths are relative to the root set with fs_set_root. */

typedef struct FsFile FsFile;

void fs_set_root(const char *root);
void fs_root(char *dst, int n);

FsFile *fs_open(const char *rel);
void fs_close(FsFile *file);
df_u32 fs_size(FsFile *file);
int fs_read_at(FsFile *file, df_u32 offset, void *dst, df_u32 n);

/* Whole file. The bytes live until process exit. */
df_u8 *fs_read_all(const char *rel, df_u32 *size);

/*
 * Beside the program: the directory that contains the content root,
 * not ARENA2 itself. A name there is a path below it, as deep as it
 * goes ("DATA\\MAIN.DAT", "saves\\Anna Smith\\S0003.SAV"), a backslash
 * between folders on either platform. Names are passed to the file
 * system as they are: not mangled, not case-folded. A name that could
 * leave that directory is refused by every call below: one that starts
 * with a slash or a backslash, holds a colon (a drive), or has a ".."
 * folder in it.
 *
 * fs_make_dir makes the folder name there; 1 when it is there after,
 * made now or already (a file of that name is 0). Its parent must be.
 */
int fs_make_dir(const char *name);
/* Every folder of path, the last included, made as fs_make_dir does; 1 when all are there after. */
int fs_make_dirs(const char *path);
/* 1 when name is a file or a folder there. */
int fs_exists(const char *name);
/*
 * Each entry of folder: fn(name, is_dir, ctx), name as the file system
 * holds it, without "." or "..", in no promised order. A folder that is
 * not there lists nothing, and is not an error. How many were listed.
 */
typedef void (*FsListFn)(const char *name, int is_dir, void *ctx);
int fs_list(const char *folder, FsListFn fn, void *ctx);
/* name, a file, gone: 1 when it is not there after, deleted now or never there. */
int fs_delete(const char *name);
/* name, an empty folder, gone: 1 when it is not there after. A folder with anything in it stays (0). */
int fs_remove_dir(const char *name);
/*
 * File from renamed to, replacing a to already there, never losing it:
 * Windows 98 has no replacing rename (MoveFileEx is NT's), so the old to
 * is first renamed to the same name with ".OLD" added, then from becomes
 * to, then the ".OLD" is deleted. If from cannot become to, the old one
 * is put back. Only a crash between the two renames leaves to missing,
 * its old self under to + ".OLD": a reader that finds to missing and the
 * ".OLD" there should take that. 1 when from is now to. from and to in
 * one folder is the way to use it (a file written under a name of its
 * own, then moved over the real one).
 */
int fs_move(const char *from, const char *to);

/* The local date and time: 1..12 for the month, 1..31 for the day, 0..23 for the hour. */
typedef struct FsTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} FsTime;
/* The computer's local time now (GetLocalTime). Always there on Windows 98. */
void fs_local_time(FsTime *t);
/* Open or read a file beside the program, as fs_open and fs_read_all do inside the root. */
FsFile *fs_open_beside(const char *name);
df_u8 *fs_read_beside(const char *name, df_u32 *size);
/*
 * Replace one file beside the program. A file already there is replaced. fs_create opens it,
 * fs_append writes, fs_finish keeps it, fs_abort deletes the partial.
 */
int fs_create(const char *name);
/* Full path of name in that directory beside the program. 0 if too long. */
int fs_beside(char *dst, int n, const char *name);
int fs_append(const void *src, df_u32 n);
void fs_finish(void);
void fs_abort(void);

#endif
