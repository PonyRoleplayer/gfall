#include "engine/platform/fs.h"
#include "engine/core/mem.h"

#define FS_PATH 260
/* The largest size a df_u32 holds, FAT32's largest file: what big sizes and free bytes are clamped to. */
#define FS_U32_MAX 0xffffffffu
/*
 * A write that failed for a reason the system did not name, on a drive
 * with less free than this (or than the write still needed), was the
 * drive filling: two clusters of the largest FAT32 or FAT16 drives use.
 */
#define FS_FULL_SLACK 65536u

static char g_root[FS_PATH];
/* fs_last_error: what the latest fs_create, fs_append, fs_finish or fs_move came to. */
static int g_err;

int fs_last_error(void)
{
    return g_err;
}

/* Free bytes on the drive holding path (full); each half's. */
static int path_free(const char *path, df_u32 *bytes);

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

/*
 * 1 when name stays beside the program: not absolute (a leading slash or
 * backslash), no drive (a colon anywhere), no ".." folder. Every beside
 * call asks it, so a game's file name never reaches outside that folder.
 */
static int name_stays(const char *name)
{
    int i;
    int start;

    if (!name || !name[0] || name[0] == '/' || name[0] == '\\') {
        return 0;
    }
    start = 0;
    for (i = 0;; i++) {
        char c;

        c = name[i];
        if (c == ':') {
            return 0;
        }
        if (c == '/' || c == '\\' || c == 0) {
            if (i - start == 2 && name[start] == '.' && name[start + 1] == '.') {
                return 0;
            }
            if (c == 0) {
                return 1;
            }
            start = i + 1;
        }
    }
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

struct FsFile {
    int fd;
    df_u32 size;
};

static FsFile *open_path(const char *path)
{
    FsFile *file;
    off_t end;

    file = (FsFile *)mem_alloc(sizeof(FsFile));
    if (!file) {
        return 0;
    }
    file->fd = open(path, O_RDONLY);
    if (file->fd < 0) {
        return 0;
    }
    end = lseek(file->fd, 0, SEEK_END);
    if (end < 0 || (unsigned long long)end > FS_U32_MAX) {
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

static int g_out = -1;
static char g_out_path[FS_PATH];

/* An errno as fs_last_error tells it. EFBIG, a file past what the drive takes, is no room as well. */
static int err_of(int e)
{
    switch (e) {
    case ENOSPC:
#ifdef EDQUOT
    case EDQUOT:
#endif
    case EFBIG:
        return FS_ERR_DISK_FULL;
    case EACCES:
    case EPERM:
    case EROFS:
    case ETXTBSY:
        return FS_ERR_ACCESS;
    }
    return FS_ERR_OTHER;
}

/* A write that failed with e, need bytes short: a reason not named, on a drive short of room, is no room (FS_FULL_SLACK). */
static int write_err(int e, df_u32 need)
{
    df_u32 free_bytes;
    int err;

    err = err_of(e);
    if (err == FS_ERR_OTHER && path_free(g_out_path, &free_bytes)
        && free_bytes < (need > FS_FULL_SLACK ? need : FS_FULL_SLACK)) {
        err = FS_ERR_DISK_FULL;
    }
    return err;
}

static int path_free(const char *path, df_u32 *bytes)
{
    char dir[FS_PATH];
    struct statvfs st;
    unsigned long long avail;
    int k;

    df_copy(dir, path, FS_PATH);
    /* The nearest folder that is there: statvfs wants a path that exists. */
    while (statvfs(dir, &st) != 0) {
        for (k = 0; dir[k]; k++) {
        }
        while (k > 0 && dir[k - 1] != '/') {
            k--;
        }
        if (k <= 1) {
            return 0;
        }
        dir[k - 1] = 0;
    }
    avail = (unsigned long long)st.f_bavail * (unsigned long long)st.f_frsize;
    *bytes = avail > FS_U32_MAX ? FS_U32_MAX : (df_u32)avail;
    return 1;
}

static int beside_path(char *dst, int n, const char *name)
{
    int i;
    int slash;
    int k;

    if (!name_stays(name) || !g_root[0]) {
        return 0;
    }
    slash = -1;
    for (i = 0; g_root[i]; i++) {
        if (g_root[i] == '/' || g_root[i] == '\\') {
            slash = i;
        }
    }
    if (slash < 1) {
        return fs_join(dst, n, name);
    }
    k = 0;
    for (i = 0; i < slash && k < n - 1; i++) {
        dst[k++] = g_root[i];
    }
    if (k < n - 1) {
        dst[k++] = g_root[slash];
    }
    i = 0;
    while (name[i] && k < n - 1) {
        /* DATA\\MAIN.DAT as the game writes it: a backslash is a plain character here. */
        dst[k++] = name[i] == '\\' ? '/' : name[i];
        i++;
    }
    dst[k] = 0;
    return name[i] == 0;
}

int fs_beside(char *dst, int n, const char *name)
{
    return beside_path(dst, n, name);
}

int fs_make_dir(const char *name)
{
    char path[FS_PATH];
    struct stat st;

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    if (mkdir(path, 0755) == 0) {
        return 1;
    }
    return errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int fs_create(const char *name)
{
    char path[FS_PATH];

    if (g_out >= 0) {
        close(g_out);
        g_out = -1;
    }
    g_err = FS_ERR_OTHER;
    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    g_out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_out < 0) {
        g_err = err_of(errno);
        return 0;
    }
    df_copy(g_out_path, path, FS_PATH);
    g_err = FS_ERR_NONE;
    return 1;
}

int fs_append(const void *src, df_u32 n)
{
    const df_u8 *p;
    df_u32 got;

    g_err = FS_ERR_OTHER;
    if (g_out < 0 || !src) {
        return 0;
    }
    p = (const df_u8 *)src;
    got = 0;
    while (got < n) {
        ssize_t chunk;

        /* A short write goes round again: the next says why (ENOSPC, EFBIG), or writes nothing, no room. */
        chunk = write(g_out, p + got, n - got);
        if (chunk < 0) {
            g_err = write_err(errno, n - got);
            return 0;
        }
        if (chunk == 0) {
            g_err = FS_ERR_DISK_FULL;
            return 0;
        }
        got += (df_u32)chunk;
    }
    g_err = FS_ERR_NONE;
    return 1;
}

int fs_finish(void)
{
    int e;

    if (g_out < 0) {
        g_err = FS_ERR_OTHER;
        return 0;
    }
    e = 0;
    if (fsync(g_out) != 0) {
        e = errno;
    }
    if (close(g_out) != 0 && !e) {
        e = errno;
    }
    g_out = -1;
    g_err = e ? write_err(e, 0) : FS_ERR_NONE;
    return !e;
}

void fs_abort(void)
{
    if (g_out >= 0) {
        close(g_out);
        g_out = -1;
        if (g_out_path[0]) {
            unlink(g_out_path);
        }
    }
    g_out_path[0] = 0;
}

static int path_exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

int fs_exists(const char *name)
{
    char path[FS_PATH];

    return beside_path(path, FS_PATH, name) && path_exists(path);
}

int fs_list(const char *folder, FsListFn fn, void *ctx)
{
    char path[FS_PATH];
    char entry[FS_PATH];
    DIR *dir;
    struct dirent *e;
    int n;

    if (!fn || !beside_path(path, FS_PATH, folder)) {
        return 0;
    }
    dir = opendir(path);
    if (!dir) {
        return 0;
    }
    n = 0;
    while ((e = readdir(dir)) != 0) {
        struct stat st;
        int k;
        int i;

        if (df_eq(e->d_name, ".") || df_eq(e->d_name, "..")) {
            continue;
        }
        k = 0;
        for (i = 0; path[i] && k < FS_PATH - 2; i++) {
            entry[k++] = path[i];
        }
        entry[k++] = '/';
        for (i = 0; e->d_name[i] && k < FS_PATH - 1; i++) {
            entry[k++] = e->d_name[i];
        }
        entry[k] = 0;
        fn(e->d_name, stat(entry, &st) == 0 && S_ISDIR(st.st_mode), ctx);
        n++;
    }
    closedir(dir);
    return n;
}

int fs_delete(const char *name)
{
    char path[FS_PATH];
    struct stat st;

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 1;
    }
    return !S_ISDIR(st.st_mode) && unlink(path) == 0;
}

int fs_remove_dir(const char *name)
{
    char path[FS_PATH];

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    return rmdir(path) == 0 || !path_exists(path);
}

/*
 * from to to, failing when to is there, as Windows 98's MoveFile does
 * (ERROR_ALREADY_EXISTS, an other): fs_move's steps run the same on both.
 * Each says why it failed in g_err.
 */
static int rename_plain(const char *from, const char *to)
{
    if (path_exists(to)) {
        g_err = FS_ERR_OTHER;
        return 0;
    }
    if (rename(from, to) != 0) {
        g_err = err_of(errno);
        return 0;
    }
    return 1;
}

static int remove_plain(const char *path)
{
    if (unlink(path) != 0) {
        g_err = err_of(errno);
        return 0;
    }
    return 1;
}

void fs_local_time(FsTime *t)
{
    time_t now;
    struct tm tm;

    now = time(0);
    localtime_r(&now, &tm);
    t->year = tm.tm_year + 1900;
    t->month = tm.tm_mon + 1;
    t->day = tm.tm_mday;
    t->hour = tm.tm_hour;
    t->minute = tm.tm_min;
    t->second = tm.tm_sec;
}

#include <stdlib.h>

/* Where the original game's CD may be: only for tests, by the environment (GFALL_MEDIA, a folder that holds the movies). */
static int media_roots(char roots[][FS_PATH], int max)
{
    const char *dir;

    dir = getenv("GFALL_MEDIA");
    if (!dir || max < 1) { return 0; }
    df_copy(roots[0], dir, FS_PATH);
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

static FsFile *open_path(const char *path)
{
    FsFile *file;
    DWORD sz;

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
    {
        DWORD high;

        /*
         * The high word asked for, so a file of 4 GB - 1 (FAT32's largest,
         * INVALID_FILE_SIZE as a low word) is told from a failure; one past
         * what a df_u32 holds is not opened.
         */
        high = 0;
        SetLastError(NO_ERROR);
        sz = GetFileSize(file->handle, &high);
        if ((sz == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) || high != 0) {
            CloseHandle(file->handle);
            file->handle = INVALID_HANDLE_VALUE;
            return 0;
        }
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
    {
        LONG high;

        /*
         * A high word of 0 given, so the offset is unsigned, good to 4 GB - 1:
         * with none, Windows takes the low word as signed and every offset
         * past 2 GB fails. INVALID_SET_FILE_POINTER is then also a real
         * place, told apart by GetLastError.
         */
        high = 0;
        SetLastError(NO_ERROR);
        if (SetFilePointer(file->handle, (LONG)offset, &high, FILE_BEGIN) == INVALID_SET_FILE_POINTER
            && GetLastError() != NO_ERROR) {
            return 0;
        }
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

static HANDLE g_out = INVALID_HANDLE_VALUE;
static char g_out_path[FS_PATH];

/* GetLastError as fs_last_error tells it. */
static int err_of(DWORD code)
{
    switch (code) {
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return FS_ERR_DISK_FULL;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_WRITE_PROTECT:
        return FS_ERR_ACCESS;
    }
    return FS_ERR_OTHER;
}

/*
 * A write that failed with code, need bytes short. Windows 98 does not
 * always name a full disk (a flush or a close can fail with another
 * code), so a reason not named, on a drive short of room
 * (FS_FULL_SLACK), is no room.
 */
static int write_err(DWORD code, df_u32 need)
{
    df_u32 free_bytes;
    int err;

    err = err_of(code);
    if (err == FS_ERR_OTHER && path_free(g_out_path, &free_bytes)
        && free_bytes < (need > FS_FULL_SLACK ? need : FS_FULL_SLACK)) {
        err = FS_ERR_DISK_FULL;
    }
    return err;
}

/* A 64-bit count as GetDiskFreeSpaceExA writes it (ULARGE_INTEGER), low word first. */
typedef struct FsBig {
    DWORD lo;
    DWORD hi;
} FsBig;
typedef BOOL (WINAPI *free_ex_fn)(LPCSTR dir, FsBig *avail, FsBig *total, FsBig *free_all);

/*
 * The drive's root of a full path: "C:\", or a share's "\\server\share\".
 * Both calls below take it, and it is always there.
 */
static int drive_root(const char *path, char *root, int n)
{
    int k;
    int slashes;

    if (path[0] && path[1] == ':') {
        if (n < 4) {
            return 0;
        }
        root[0] = path[0];
        root[1] = ':';
        root[2] = '\\';
        root[3] = 0;
        return 1;
    }
    if (path[0] != '\\' || path[1] != '\\') {
        return 0;
    }
    slashes = 0;
    for (k = 0; path[k] && k < n - 2; k++) {
        root[k] = path[k];
        if (path[k] == '\\' && ++slashes == 4) {
            root[k + 1] = 0;
            return 1;
        }
    }
    /* "\\server\share" with no backslash after it: the root is the share, ended with one. */
    if (slashes == 3 && k < n - 2) {
        root[k] = '\\';
        root[k + 1] = 0;
        return 1;
    }
    return 0;
}

/*
 * Free bytes for this user (quotas counted) on path's drive, clamped to a
 * df_u32. GetDiskFreeSpaceExA is Windows 95 OSR2's and later, so it is
 * looked for, not imported: the first Windows 95 has only
 * GetDiskFreeSpaceA, whose 32-bit counts stop near 2 GB on a big FAT32
 * drive (it says less, never more).
 */
static int path_free(const char *path, df_u32 *bytes)
{
    static free_ex_fn free_ex;
    static int looked;
    char root[FS_PATH];
    DWORD per_cluster;
    DWORD sectors;
    DWORD sector_bytes;
    DWORD free_clusters;
    DWORD clusters;

    if (!drive_root(path, root, FS_PATH)) {
        return 0;
    }
    if (!looked) {
        HMODULE kernel;

        looked = 1;
        kernel = GetModuleHandleA("KERNEL32.DLL");
        free_ex = kernel ? (free_ex_fn)(void *)GetProcAddress(kernel, "GetDiskFreeSpaceExA") : 0;
    }
    if (free_ex) {
        FsBig avail;
        FsBig total;
        FsBig free_all;

        if (free_ex(root, &avail, &total, &free_all)) {
            *bytes = avail.hi ? FS_U32_MAX : (df_u32)avail.lo;
            return 1;
        }
    }
    if (!GetDiskFreeSpaceA(root, &sectors, &sector_bytes, &free_clusters, &clusters)) {
        return 0;
    }
    per_cluster = sectors * sector_bytes;
    if (!per_cluster) {
        *bytes = 0;
        return 1;
    }
    *bytes = free_clusters > FS_U32_MAX / per_cluster ? FS_U32_MAX : (df_u32)(free_clusters * per_cluster);
    return 1;
}

static int beside_path(char *dst, int n, const char *name)
{
    int i;
    int slash;
    int k;

    if (!name_stays(name) || !g_root[0]) {
        return 0;
    }
    slash = -1;
    for (i = 0; g_root[i]; i++) {
        if (g_root[i] == '/' || g_root[i] == '\\') {
            slash = i;
        }
    }
    if (slash < 1) {
        return fs_join(dst, n, name);
    }
    k = 0;
    for (i = 0; i < slash && k < n - 1; i++) {
        dst[k++] = g_root[i];
    }
    if (k < n - 1) {
        dst[k++] = g_root[slash];
    }
    i = 0;
    while (name[i] && k < n - 1) {
        dst[k++] = name[i++];
    }
    dst[k] = 0;
    return name[i] == 0;
}

int fs_beside(char *dst, int n, const char *name)
{
    return beside_path(dst, n, name);
}

int fs_make_dir(const char *name)
{
    char path[FS_PATH];
    DWORD attr;

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    if (CreateDirectoryA(path, 0)) {
        return 1;
    }
    /* Already there is fine; a file of that name is not. */
    attr = GetFileAttributesA(path);
    return attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int fs_create(const char *name)
{
    char path[FS_PATH];

    if (g_out != INVALID_HANDLE_VALUE) {
        CloseHandle(g_out);
        g_out = INVALID_HANDLE_VALUE;
    }
    g_err = FS_ERR_OTHER;
    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    g_out = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        0,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        0);
    if (g_out == INVALID_HANDLE_VALUE) {
        g_err = err_of(GetLastError());
        return 0;
    }
    df_copy(g_out_path, path, FS_PATH);
    g_err = FS_ERR_NONE;
    return 1;
}

int fs_append(const void *src, df_u32 n)
{
    DWORD done;

    g_err = FS_ERR_OTHER;
    if (g_out == INVALID_HANDLE_VALUE || !src) {
        return 0;
    }
    if (!n) {
        g_err = FS_ERR_NONE;
        return 1;
    }
    if (!WriteFile(g_out, src, n, &done, 0)) {
        g_err = write_err(GetLastError(), n);
        return 0;
    }
    /* Windows 98 answers a full disk with a short write and no error: one call writes it all or the disk is full. */
    if (done < n) {
        g_err = FS_ERR_DISK_FULL;
        return 0;
    }
    g_err = FS_ERR_NONE;
    return 1;
}

int fs_finish(void)
{
    DWORD code;

    if (g_out == INVALID_HANDLE_VALUE) {
        g_err = FS_ERR_OTHER;
        return 0;
    }
    /*
     * Windows 98 keeps what was written in its cache and can find the
     * disk full only now, at the flush or the close: both are checked.
     */
    code = NO_ERROR;
    if (!FlushFileBuffers(g_out)) {
        code = GetLastError();
    }
    if (!CloseHandle(g_out) && code == NO_ERROR) {
        code = GetLastError();
    }
    g_out = INVALID_HANDLE_VALUE;
    g_err = code == NO_ERROR ? FS_ERR_NONE : write_err(code, 0);
    return code == NO_ERROR;
}

void fs_abort(void)
{
    if (g_out != INVALID_HANDLE_VALUE) {
        CloseHandle(g_out);
        g_out = INVALID_HANDLE_VALUE;
        if (g_out_path[0]) {
            DeleteFileA(g_out_path);
        }
    }
    g_out_path[0] = 0;
}

static int path_exists(const char *path)
{
    return GetFileAttributesA(path) != 0xffffffff;
}

int fs_exists(const char *name)
{
    char path[FS_PATH];

    return beside_path(path, FS_PATH, name) && path_exists(path);
}

int fs_list(const char *folder, FsListFn fn, void *ctx)
{
    char path[FS_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE find;
    int k;
    int n;

    if (!fn || !beside_path(path, FS_PATH, folder)) {
        return 0;
    }
    /* folder\*: every entry, whatever its name. */
    for (k = 0; path[k]; k++) {
    }
    if (k + 2 >= FS_PATH) {
        return 0;
    }
    path[k] = '\\';
    path[k + 1] = '*';
    path[k + 2] = 0;
    find = FindFirstFileA(path, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }
    n = 0;
    do {
        if (df_eq(fd.cFileName, ".") || df_eq(fd.cFileName, "..")) {
            continue;
        }
        fn(fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, ctx);
        n++;
    } while (FindNextFileA(find, &fd));
    FindClose(find);
    return n;
}

int fs_delete(const char *name)
{
    char path[FS_PATH];
    DWORD attr;

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    attr = GetFileAttributesA(path);
    if (attr == 0xffffffff) {
        return 1;
    }
    return !(attr & FILE_ATTRIBUTE_DIRECTORY) && DeleteFileA(path);
}

int fs_remove_dir(const char *name)
{
    char path[FS_PATH];

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    return RemoveDirectoryA(path) || !path_exists(path);
}

/* MoveFile fails when to is there: fs_move's steps make room first. Each says why it failed in g_err. */
static int rename_plain(const char *from, const char *to)
{
    if (!MoveFileA(from, to)) {
        g_err = err_of(GetLastError());
        return 0;
    }
    return 1;
}

static int remove_plain(const char *path)
{
    if (!DeleteFileA(path)) {
        g_err = err_of(GetLastError());
        return 0;
    }
    return 1;
}

void fs_local_time(FsTime *t)
{
    SYSTEMTIME st;

    GetLocalTime(&st);
    t->year = st.wYear;
    t->month = st.wMonth;
    t->day = st.wDay;
    t->hour = st.wHour;
    t->minute = st.wMinute;
    t->second = st.wSecond;
}

/* The CD-ROM drives: the original game's disc keeps its files under DAGGER\ARENA2 (some copies directly under ARENA2). */
static int media_roots(char roots[][FS_PATH], int max)
{
    char drives[128];
    DWORD n, i;
    int count;

    n = GetLogicalDriveStringsA(sizeof(drives) - 1, drives);
    count = 0;
    for (i = 0; i + 3 <= n && drives[i] && count + 2 <= max; i += 4) {
        if (GetDriveTypeA(drives + i) == DRIVE_CDROM) {
            df_copy(roots[count], drives + i, FS_PATH);
            df_copy(roots[count] + 3, "DAGGER\\ARENA2", FS_PATH - 3);
            count++;
            df_copy(roots[count], drives + i, FS_PATH);
            df_copy(roots[count] + 3, "ARENA2", FS_PATH - 3);
            count++;
        }
    }
    return count;
}

#endif

int fs_make_dirs(const char *path)
{
    char part[FS_PATH];
    int i;

    if (!name_stays(path)) {
        return 0;
    }
    /* Each folder from the first: "saves", then "saves\\Anna Smith". */
    for (i = 0; path[i] && i < FS_PATH - 1; i++) {
        if ((path[i] == '\\' || path[i] == '/') && i > 0) {
            part[i] = 0;
            if (!fs_make_dir(part)) {
                return 0;
            }
        }
        part[i] = path[i];
    }
    if (path[i]) {
        return 0;
    }
    part[i] = 0;
    return fs_make_dir(part);
}

int fs_move(const char *from, const char *to)
{
    char src[FS_PATH];
    char dst[FS_PATH];
    char old[FS_PATH];
    int had;
    int k;
    int err;

    g_err = FS_ERR_OTHER;
    if (!beside_path(src, FS_PATH, from) || !beside_path(dst, FS_PATH, to) || !path_exists(src)) {
        return 0;
    }
    /* to + ".OLD": where the file being replaced waits until the new one is in. */
    df_copy(old, dst, FS_PATH);
    for (k = 0; old[k]; k++) {
    }
    if (k + 4 >= FS_PATH) {
        return 0;
    }
    old[k] = '.';
    old[k + 1] = 'O';
    old[k + 2] = 'L';
    old[k + 3] = 'D';
    old[k + 4] = 0;
    had = path_exists(dst);
    if (had) {
        /* A ".OLD" from a crash before is older than the to now: it goes. */
        if (path_exists(old) && !remove_plain(old)) {
            return 0;
        }
        if (!rename_plain(dst, old)) {
            return 0;
        }
    }
    if (!rename_plain(src, dst)) {
        /* Why the move failed, not how the old one's way back went. */
        err = g_err;
        if (had) {
            rename_plain(old, dst);
        }
        g_err = err;
        return 0;
    }
    if (had) {
        remove_plain(old);
    }
    g_err = FS_ERR_NONE;
    return 1;
}

int fs_free_bytes(const char *name, df_u32 *bytes)
{
    char path[FS_PATH];

    if (!bytes || !beside_path(path, FS_PATH, name)) {
        return 0;
    }
    return path_free(path, bytes);
}

FsFile *fs_open(const char *rel)
{
    char path[FS_PATH];

    if (!fs_join(path, FS_PATH, rel)) {
        return 0;
    }
    return open_path(path);
}

FsFile *fs_open_beside(const char *name)
{
    char path[FS_PATH];

    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    return open_path(path);
}

#ifdef DF_HOST
#define MEDIA_SEP '/'
#else
#define MEDIA_SEP '\\'
#endif

FsFile *fs_open_media(const char *rel)
{
    char path[FS_PATH];
    char roots[8][FS_PATH];
    FsFile *file;
    int n, i, len;

    /* A big movie file is read from the installation first (the whole game is meant to be installed). */
    file = fs_open(rel);
    if (file || !name_stays(rel)) {
        return file;
    }
    n = media_roots(roots, 8);
    for (i = 0; i < n; i++) {
        df_copy(path, roots[i], FS_PATH);
        for (len = 0; path[len]; len++) {
        }
        if (len + 2 >= FS_PATH) {
            continue;
        }
        if (path[len - 1] != '\\' && path[len - 1] != '/') {
            path[len++] = MEDIA_SEP;
        }
        df_copy(path + len, rel, FS_PATH - len);
        file = open_path(path);
        if (file) {
            return file;
        }
    }
    return 0;
}

static df_u8 *read_whole(FsFile *file, df_u32 *size)
{
    df_u8 *bytes;
    df_u32 n;

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

df_u8 *fs_read_all(const char *rel, df_u32 *size)
{
    return read_whole(fs_open(rel), size);
}

df_u8 *fs_read_beside(const char *name, df_u32 *size)
{
    return read_whole(fs_open_beside(name), size);
}
