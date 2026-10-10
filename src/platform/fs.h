#ifndef DF_FS_H
#define DF_FS_H

#include "df.h"

/* Paths are relative to the ARENA2 root set with fs_set_root. */

typedef struct FsFile FsFile;

void fs_set_root(const char *root);
void fs_root(char *dst, int n);

FsFile *fs_open(const char *rel);
void fs_close(FsFile *file);
df_u32 fs_size(FsFile *file);
int fs_read_at(FsFile *file, df_u32 offset, void *dst, df_u32 n);

/* Whole file. The bytes live until process exit. */
df_u8 *fs_read_all(const char *rel, df_u32 *size);

#endif
