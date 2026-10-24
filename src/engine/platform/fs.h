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
 * Replace one file in the directory that contains the content root,
 * beside the program rather than inside ARENA2. name is a single
 * component. A file already there is replaced. fs_create opens it,
 * fs_append writes, fs_finish keeps it, fs_abort deletes the partial.
 */
int fs_create(const char *name);
/* Full path of name in that directory beside the program. 0 if too long. */
int fs_beside(char *dst, int n, const char *name);
int fs_append(const void *src, df_u32 n);
void fs_finish(void);
void fs_abort(void);

#endif
