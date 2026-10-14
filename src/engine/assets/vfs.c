#include "engine/assets/vfs.h"
#include "engine/platform/fs.h"

/*
 * The mount is the platform root. One content directory, no overlays.
 */

void vfs_mount(const char *root)
{
    fs_set_root(root);
}

void vfs_root(char *dst, int n)
{
    fs_root(dst, n);
}

VfsFile *vfs_open(const char *rel)
{
    return (VfsFile *)fs_open(rel);
}

void vfs_close(VfsFile *file)
{
    fs_close((FsFile *)file);
}

df_u32 vfs_size(VfsFile *file)
{
    return fs_size((FsFile *)file);
}

int vfs_read_at(VfsFile *file, df_u32 offset, void *dst, df_u32 n)
{
    return fs_read_at((FsFile *)file, offset, dst, n);
}

df_u8 *vfs_read(const char *rel, df_u32 *size)
{
    return fs_read_all(rel, size);
}
