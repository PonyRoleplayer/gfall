#ifndef ENG_VFS_H
#define ENG_VFS_H

#include "engine/core/df.h"

/*
 * Read-only files under one mounted root. The platform still does the
 * open. Games and loaders call this so they do not include the host
 * file layer. Bytes from vfs_read live until process exit.
 */

typedef struct VfsFile VfsFile;

void vfs_mount(const char *root);
void vfs_root(char *dst, int n);

VfsFile *vfs_open(const char *rel);
void vfs_close(VfsFile *file);
df_u32 vfs_size(VfsFile *file);
int vfs_read_at(VfsFile *file, df_u32 offset, void *dst, df_u32 n);
df_u8 *vfs_read(const char *rel, df_u32 *size);

#endif
