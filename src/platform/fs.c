#include "platform/fs.h"
#include "platform/mem.h"

#define FS_PATH 260

static char g_root[FS_PATH];

static int fs_join(char *dst, int n, const char *rel)
{
    int i;
    int j;

    i = 0;
    while (g_root[i] && i < n - 1) {
        dst[i] = g_root[i];
        i++;
    }
    if (i > 0 && dst[i - 1] != '/' && dst[i - 1] != '\\' && i < n - 1) {
        dst[i++] = '/';
    }
    j = 0;
    while (rel[j] && i < n - 1) {
        dst[i++] = rel[j++];
    }
    dst[i] = 0;
    return rel[j] == 0 && g_root[0] != 0;
}

void fs_set_root(const char *root)
{
    df_copy(g_root, root, FS_PATH);
}

void fs_root(char *dst, int n)
{
    df_copy(dst, g_root, n);
}

#ifdef DF_HOST

#include <fcntl.h>
#include <unistd.h>

struct FsFile {
    int fd;
    df_u32 size;
};

FsFile *fs_open(const char *rel)
{
    char path[FS_PATH];
    FsFile *file;
    off_t end;

    if (!fs_join(path, FS_PATH, rel)) {
        return 0;
    }
    file = (FsFile *)mem_alloc(sizeof(FsFile));
    if (!file) {
        return 0;
    }
    file->fd = open(path, O_RDONLY);
    if (file->fd < 0) {
        return 0;
    }
    end = lseek(file->fd, 0, SEEK_END);
    if (end < 0 || end > 0x7fffffff) {
        close(file->fd);
        return 0;
    }
    file->size = (df_u32)end;
    return file;
}

void fs_close(FsFile *file)
{
    if (file && file->fd >= 0) {
        close(file->fd);
        file->fd = -1;
    }
}

df_u32 fs_size(FsFile *file)
{
    return file ? file->size : 0;
}

int fs_read_at(FsFile *file, df_u32 offset, void *dst, df_u32 n)
{
    df_u8 *out;
    df_u32 got;

    if (!file || file->fd < 0) {
        return 0;
    }
    if (offset > file->size || n > file->size - offset) {
        return 0;
    }
    out = (df_u8 *)dst;
    got = 0;
    while (got < n) {
        ssize_t chunk;

        chunk = pread(file->fd, out + got, n - got, (off_t)offset + (off_t)got);
        if (chunk <= 0) {
            return 0;
        }
        got += (df_u32)chunk;
    }
    return 1;
}

#else

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct FsFile {
    HANDLE handle;
    df_u32 size;
};

FsFile *fs_open(const char *rel)
{
    char path[FS_PATH];
    FsFile *file;
    DWORD sz;

    if (!fs_join(path, FS_PATH, rel)) {
        return 0;
    }
    file = (FsFile *)mem_alloc(sizeof(FsFile));
    if (!file) {
        return 0;
    }
    file->handle = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0);
    if (file->handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    sz = GetFileSize(file->handle, 0);
    if (sz == 0xffffffff) {
        CloseHandle(file->handle);
        file->handle = INVALID_HANDLE_VALUE;
        return 0;
    }
    file->size = (df_u32)sz;
    return file;
}

void fs_close(FsFile *file)
{
    if (file && file->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(file->handle);
        file->handle = INVALID_HANDLE_VALUE;
    }
}

df_u32 fs_size(FsFile *file)
{
    return file ? file->size : 0;
}

int fs_read_at(FsFile *file, df_u32 offset, void *dst, df_u32 n)
{
    df_u8 *out;
    df_u32 got;

    if (!file || file->handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (offset > file->size || n > file->size - offset) {
        return 0;
    }
    if (SetFilePointer(file->handle, (LONG)offset, 0, FILE_BEGIN) == 0xffffffff) {
        return 0;
    }
    out = (df_u8 *)dst;
    got = 0;
    while (got < n) {
        DWORD chunk;

        if (!ReadFile(file->handle, out + got, n - got, &chunk, 0) || chunk == 0) {
            return 0;
        }
        got += (df_u32)chunk;
    }
    return 1;
}

#endif

df_u8 *fs_read_all(const char *rel, df_u32 *size)
{
    FsFile *file;
    df_u8 *bytes;
    df_u32 n;

    file = fs_open(rel);
    if (!file) {
        return 0;
    }
    n = fs_size(file);
    bytes = (df_u8 *)mem_alloc(n ? n : 1);
    if (!bytes || (n && !fs_read_at(file, 0, bytes, n))) {
        fs_close(file);
        return 0;
    }
    fs_close(file);
    if (size) {
        *size = n;
    }
    return bytes;
}
