#include "engine/platform/fs.h"
#include "engine/core/mem.h"

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

static int g_out = -1;
static char g_out_path[FS_PATH];

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
    if (!beside_path(path, FS_PATH, name)) {
        return 0;
    }
    g_out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_out < 0) {
        return 0;
    }
    df_copy(g_out_path, path, FS_PATH);
    return 1;
}

int fs_append(const void *src, df_u32 n)
{
    const df_u8 *p;
    df_u32 got;

    if (g_out < 0 || !src) {
        return 0;
    }
    p = (const df_u8 *)src;
    got = 0;
    while (got < n) {
        ssize_t chunk;

        chunk = write(g_out, p + got, n - got);
        if (chunk <= 0) {
            return 0;
        }
        got += (df_u32)chunk;
    }
    return 1;
}

void fs_finish(void)
{
    if (g_out >= 0) {
        close(g_out);
        g_out = -1;
    }
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

/* from to to, failing when to is there, as Windows 98's MoveFile does: fs_move's steps run the same on both. */
static int rename_plain(const char *from, const char *to)
{
    if (path_exists(to)) {
        return 0;
    }
    return rename(from, to) == 0;
}

static int remove_plain(const char *path)
{
    return unlink(path) == 0;
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

static HANDLE g_out = INVALID_HANDLE_VALUE;
static char g_out_path[FS_PATH];

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
        return 0;
    }
    df_copy(g_out_path, path, FS_PATH);
    return 1;
}

int fs_append(const void *src, df_u32 n)
{
    const df_u8 *p;
    df_u32 got;

    if (g_out == INVALID_HANDLE_VALUE || !src) {
        return 0;
    }
    p = (const df_u8 *)src;
    got = 0;
    while (got < n) {
        DWORD chunk;

        if (!WriteFile(g_out, p + got, n - got, &chunk, 0) || chunk == 0) {
            return 0;
        }
        got += (df_u32)chunk;
    }
    return 1;
}

void fs_finish(void)
{
    if (g_out != INVALID_HANDLE_VALUE) {
        CloseHandle(g_out);
        g_out = INVALID_HANDLE_VALUE;
    }
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

/* MoveFile fails when to is there: fs_move's steps make room first. */
static int rename_plain(const char *from, const char *to)
{
    return MoveFileA(from, to) ? 1 : 0;
}

static int remove_plain(const char *path)
{
    return DeleteFileA(path) ? 1 : 0;
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
        if (had) {
            rename_plain(old, dst);
        }
        return 0;
    }
    if (had) {
        remove_plain(old);
    }
    return 1;
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
