#include "game/dagger/play/saves.h"
#include "engine/core/log.h"
#include "engine/platform/fs.h"

/* See saves.h and docs/game.md (Saves). */

#define ROOT_MAX 32
/* Folders and files one listing holds: far more than a player makes. */
#define LIST_FOLDERS 128
#define LIST_FILES 128

static char g_root[ROOT_MAX] = "saves";
static SaveEntry g_entry[SAVES_MAX];
static int g_count;
static SaveChar g_char[SAVES_CHARS];
static int g_nchar;

void saves_use_root(const char *root)
{
    int n;

    root = root ? root : "saves";
    for (n = 0; root[n] && n < ROOT_MAX - 1; n++) {
        g_root[n] = root[n];
    }
    g_root[n] = 0;
    g_count = 0;
    g_nchar = 0;
}

/* ---- Small text helpers (no C library). ---- */

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* Whole-string compare without case: below 0, 0, above 0. */
static int ci_cmp(const char *a, const char *b)
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) {
        a++;
        b++;
    }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static int length(const char *s)
{
    int n;

    for (n = 0; s[n]; n++) {
    }
    return n;
}

/* 1 when s ends with tail, without case. */
static int ends_with(const char *s, const char *tail)
{
    int ns;
    int nt;

    ns = length(s);
    nt = length(tail);
    return ns >= nt && ci_cmp(s + ns - nt, tail) == 0;
}

static void copy(char *dst, int cap, const char *src)
{
    int n;

    for (n = 0; src[n] && n < cap - 1; n++) {
        dst[n] = src[n];
    }
    dst[n] = 0;
}

/* dst = root\folder\file (the parts that are given, 0 for none). */
static void join(char *dst, int cap, const char *folder, const char *file)
{
    log_format(dst, cap, "%s%s%s%s%s", g_root, folder ? "\\" : "", folder ? folder : "", file ? "\\" : "", file ? file : "");
}

/* ---- Folder names ---- */

/* Windows' device names, which no file or folder may be called, with or without an extension. */
static int is_device(const char *name)
{
    static const char *const bare[] = { "CON", "PRN", "AUX", "NUL" };
    int i;

    for (i = 0; i < 4; i++) {
        if (ci_cmp(name, bare[i]) == 0) {
            return 1;
        }
    }
    if (length(name) == 4 && (lower((unsigned char)name[0]) == 'c' || lower((unsigned char)name[0]) == 'l')
        && name[3] >= '1' && name[3] <= '9') {
        char head[4];

        head[0] = name[0];
        head[1] = name[1];
        head[2] = name[2];
        head[3] = 0;
        return ci_cmp(head, "COM") == 0 || ci_cmp(head, "LPT") == 0;
    }
    return 0;
}

void saves_safe_name(const char *name, char *out, int cap)
{
    int n;
    int i;

    n = 0;
    /* Letters, digits, spaces and hyphens stay; anything else is an underscore. A leading space is dropped. */
    for (i = 0; name[i] && n < cap - 2 && n < SAVES_FOLDER - 8; i++) {
        unsigned char c;

        c = (unsigned char)name[i];
        if (c == ' ' && n == 0) {
            continue;
        }
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '-') {
            out[n++] = (char)c;
        } else {
            out[n++] = '_';
        }
    }
    /* Windows drops a trailing space or dot from a name: it would not be the folder made. */
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '.')) {
        n--;
    }
    if (n == 0) {
        copy(out, cap, "Hero");
        return;
    }
    out[n] = 0;
    if (is_device(out)) {
        out[n++] = '_';
        out[n] = 0;
    }
}

/* ---- Listing ---- */

typedef struct Names {
    char name[LIST_FILES][SAVES_FILE + SAVES_FOLDER];
    char is_dir[LIST_FILES];
    int count;
} Names;

static void collect(const char *name, int is_dir, void *ctx)
{
    Names *list;

    list = (Names *)ctx;
    if (list->count < LIST_FILES && length(name) < (int)sizeof(list->name[0])) {
        copy(list->name[list->count], (int)sizeof(list->name[0]), name);
        list->is_dir[list->count] = (char)is_dir;
        list->count++;
    }
}

static Names g_folders;
static Names g_files;

/* The number in a name like S0003.SAV: 3; -1 when the name is not of that form. */
static int number_of(const char *file)
{
    int n;
    int i;

    if (lower((unsigned char)file[0]) != 's' || file[1] < '0' || file[1] > '9') {
        return -1;
    }
    n = 0;
    for (i = 1; file[i] >= '0' && file[i] <= '9' && i < 9; i++) {
        n = n * 10 + (file[i] - '0');
    }
    return file[i] == '.' ? n : -1;
}

/* 1 when the files list has a name equal to s without case. */
static int has_file(const char *s)
{
    int i;

    for (i = 0; i < g_files.count; i++) {
        if (ci_cmp(g_files.name[i], s) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The character with this id, or -1. */
static int char_of(df_u32 id)
{
    int i;

    for (i = 0; i < g_nchar; i++) {
        if (g_char[i].id == id) {
            return i;
        }
    }
    return -1;
}

/* A save of a folder, read and listed. */
static void add_entry(const char *folder, const char *file, int number)
{
    SaveEntry *e;
    SaveMeta meta;
    char path[SAVE_PATH];
    int ci;

    if (g_count >= SAVES_MAX) {
        return;
    }
    join(path, (int)sizeof(path), folder, file);
    if (!save_peek(path, &meta)) {
        return;
    }
    ci = char_of(meta.char_id);
    if (ci < 0) {
        if (g_nchar >= SAVES_CHARS) {
            return;
        }
        ci = g_nchar++;
        g_char[ci].id = meta.char_id;
        copy(g_char[ci].name, SAVE_NAME, meta.name);
        copy(g_char[ci].folder, SAVES_FOLDER, folder);
        g_char[ci].first = 0;
        g_char[ci].count = 0;
    }
    e = &g_entry[g_count++];
    copy(e->path, SAVE_PATH, path);
    copy(e->folder, SAVES_FOLDER, folder);
    copy(e->file, SAVES_FILE, file);
    e->meta = meta;
    e->number = number;
    e->char_index = ci;
}

/* The real time of a save as a number that sorts: the date's digits, then the time's. */
static int stamp_cmp(const SaveMeta *a, const SaveMeta *b)
{
    long da;
    long db;

    da = (long)a->year * 10000L + (long)a->month * 100L + a->day;
    db = (long)b->year * 10000L + (long)b->month * 100L + b->day;
    if (da != db) {
        return da < db ? -1 : 1;
    }
    da = (long)a->hour * 10000L + (long)a->minute * 100L + a->second;
    db = (long)b->hour * 10000L + (long)b->minute * 100L + b->second;
    return da < db ? -1 : (da > db ? 1 : 0);
}

int saves_scan(void)
{
    static SaveEntry sorted[SAVES_MAX];
    static int order[SAVES_MAX];
    static int rank[SAVES_CHARS];
    static int by_rank[SAVES_CHARS];
    int f;
    int i;
    int j;

    g_count = 0;
    g_nchar = 0;
    g_folders.count = 0;
    fs_list(g_root, collect, &g_folders);
    for (f = 0; f < g_folders.count; f++) {
        char dir[SAVE_PATH];
        char folder[SAVES_FOLDER];

        if (!g_folders.is_dir[f] || length(g_folders.name[f]) >= SAVES_FOLDER) {
            continue;
        }
        copy(folder, SAVES_FOLDER, g_folders.name[f]);
        join(dir, (int)sizeof(dir), folder, 0);
        g_files.count = 0;
        fs_list(dir, collect, &g_files);
        for (i = 0; i < g_files.count; i++) {
            char sav[SAVES_FILE + SAVES_FOLDER];
            int number;

            if (g_files.is_dir[i]) {
                continue;
            }
            number = number_of(g_files.name[i]);
            if (number < 0) {
                continue;
            }
            if (ends_with(g_files.name[i], ".SAV")) {
                add_entry(folder, g_files.name[i], number);
            } else if (ends_with(g_files.name[i], ".OLD")) {
                /* A move cut short left the previous save under .OLD: it stands in only while the .SAV is not there. */
                copy(sav, (int)sizeof(sav), g_files.name[i]);
                sav[length(sav) - 3] = 'S';
                sav[length(sav) - 2] = 'A';
                sav[length(sav) - 1] = 'V';
                if (!has_file(sav)) {
                    add_entry(folder, g_files.name[i], number);
                }
            }
        }
    }
    /* The characters by name (then id), each one's saves newest first. */
    for (i = 0; i < g_nchar; i++) {
        by_rank[i] = i;
    }
    for (i = 1; i < g_nchar; i++) {
        int key;

        key = by_rank[i];
        for (j = i - 1; j >= 0; j--) {
            int c;

            c = ci_cmp(g_char[by_rank[j]].name, g_char[key].name);
            if (c < 0 || (c == 0 && g_char[by_rank[j]].id <= g_char[key].id)) {
                break;
            }
            by_rank[j + 1] = by_rank[j];
        }
        by_rank[j + 1] = key;
    }
    for (i = 0; i < g_nchar; i++) {
        rank[by_rank[i]] = i;
    }
    for (i = 0; i < g_count; i++) {
        order[i] = i;
    }
    for (i = 1; i < g_count; i++) {
        int key;

        key = order[i];
        for (j = i - 1; j >= 0; j--) {
            const SaveEntry *a;
            const SaveEntry *b;
            int before;

            a = &g_entry[order[j]];
            b = &g_entry[key];
            before = rank[a->char_index] != rank[b->char_index] ? rank[a->char_index] < rank[b->char_index]
                                                                : (stamp_cmp(&a->meta, &b->meta) != 0 ? stamp_cmp(&a->meta, &b->meta) > 0
                                                                                                      : a->number > b->number);
            if (before) {
                break;
            }
            order[j + 1] = order[j];
        }
        order[j + 1] = key;
    }
    for (i = 0; i < g_count; i++) {
        sorted[i] = g_entry[order[i]];
    }
    for (i = 0; i < g_count; i++) {
        g_entry[i] = sorted[i];
    }
    /* The characters in that order too, and which entries are theirs. */
    {
        static SaveChar chars[SAVES_CHARS];

        for (i = 0; i < g_nchar; i++) {
            chars[rank[i]] = g_char[i];
            chars[rank[i]].first = 0;
            chars[rank[i]].count = 0;
        }
        for (i = 0; i < g_nchar; i++) {
            g_char[i] = chars[i];
        }
        for (i = 0; i < g_count; i++) {
            SaveChar *c;

            g_entry[i].char_index = rank[g_entry[i].char_index];
            c = &g_char[g_entry[i].char_index];
            if (c->count == 0) {
                c->first = i;
            }
            c->count++;
        }
    }
    return g_count;
}

int saves_count(void)
{
    return g_count;
}

const SaveEntry *saves_entry(int index)
{
    return index >= 0 && index < g_count ? &g_entry[index] : 0;
}

int saves_char_count(void)
{
    return g_nchar;
}

const SaveChar *saves_char(int index)
{
    return index >= 0 && index < g_nchar ? &g_char[index] : 0;
}

int saves_any(void)
{
    return saves_scan() > 0;
}

/* ---- Writing ---- */

/* The highest number among the S#### names in a folder, whatever they end in, so no stray file is written over; 0 when none. */
static int highest_in(const char *folder)
{
    char dir[SAVE_PATH];
    int best;
    int i;

    join(dir, (int)sizeof(dir), folder, 0);
    g_files.count = 0;
    fs_list(dir, collect, &g_files);
    best = 0;
    for (i = 0; i < g_files.count; i++) {
        int n;

        n = g_files.is_dir[i] ? -1 : number_of(g_files.name[i]);
        if (n > best) {
            best = n;
        }
    }
    return best;
}

/* The name of a folder for a new character: the safe name, with " (2)", " (3)" while another holds it. */
static void new_folder(const char *name, char *out)
{
    char base[SAVES_FOLDER];
    int n;

    saves_safe_name(name, base, (int)sizeof(base));
    for (n = 1; n < 100; n++) {
        char dir[SAVE_PATH];
        int taken;
        int i;

        if (n == 1) {
            copy(out, SAVES_FOLDER, base);
        } else {
            log_format(out, SAVES_FOLDER, "%s (%d)", base, n);
        }
        taken = 0;
        for (i = 0; i < g_nchar; i++) {
            taken = taken || ci_cmp(g_char[i].folder, out) == 0;
        }
        /* A folder there with something in it is taken too; an empty one is free. */
        join(dir, (int)sizeof(dir), out, 0);
        g_files.count = 0;
        if (!taken && fs_exists(dir) && fs_list(dir, collect, &g_files) > 0) {
            taken = 1;
        }
        if (!taken) {
            return;
        }
    }
}

int saves_write(const SaveState *state, char *path)
{
    char folder[SAVES_FOLDER];
    char file[SAVES_FILE];
    char full[SAVE_PATH];
    int ci;
    int number;

    saves_scan();
    ci = char_of(state->sheet.id);
    if (ci >= 0) {
        copy(folder, SAVES_FOLDER, g_char[ci].folder);
    } else {
        new_folder(state->sheet.name, folder);
    }
    number = highest_in(folder) + 1;
    if (number > 9999) {
        log_text(LOG_WARN, "saves: a character has more saves than the names allow");
        return 0;
    }
    log_format(file, (int)sizeof(file), "S%04d.SAV", number);
    join(full, (int)sizeof(full), folder, file);
    save_use_file(full);
    if (!save_write(state)) {
        return 0;
    }
    if (path) {
        copy(path, SAVE_PATH, full);
    }
    saves_scan();
    return 1;
}

/* ---- Deleting ---- */

/* A save's file and its stray siblings (.OLD, .TMP), gone. The path ends in a three-letter extension. */
static int delete_save_files(const char *path)
{
    char base[SAVE_PATH];
    char name[SAVE_PATH + 8];
    int n;
    int ok;

    n = length(path);
    copy(base, SAVE_PATH, path);
    if (n > 4 && base[n - 4] == '.') {
        base[n - 4] = 0;
    }
    ok = 1;
    log_format(name, (int)sizeof(name), "%s.SAV", base);
    ok = fs_delete(name) && ok;
    log_format(name, (int)sizeof(name), "%s.OLD", base);
    ok = fs_delete(name) && ok;
    log_format(name, (int)sizeof(name), "%s.SAV.TMP", base);
    ok = fs_delete(name) && ok;
    return ok;
}

/* A folder removed when nothing else is in it. */
static void remove_if_empty(const char *folder)
{
    char dir[SAVE_PATH];

    join(dir, (int)sizeof(dir), folder, 0);
    fs_remove_dir(dir);
}

int saves_delete(int entry)
{
    char folder[SAVES_FOLDER];
    char path[SAVE_PATH];
    int ok;

    if (entry < 0 || entry >= g_count) {
        return 0;
    }
    copy(folder, SAVES_FOLDER, g_entry[entry].folder);
    copy(path, SAVE_PATH, g_entry[entry].path);
    ok = delete_save_files(path);
    remove_if_empty(folder);
    saves_scan();
    return ok;
}

int saves_delete_char(int character)
{
    /* Static: the target takes no stack probe, and these are over a stack page. */
    static char folders[SAVES_MAX][SAVES_FOLDER];
    static char paths[SAVES_MAX][SAVE_PATH];
    int n;
    int i;
    int ok;
    const SaveChar *c;

    c = saves_char(character);
    if (!c) {
        return 0;
    }
    n = 0;
    for (i = c->first; i < c->first + c->count && n < SAVES_MAX; i++) {
        copy(folders[n], SAVES_FOLDER, g_entry[i].folder);
        copy(paths[n], SAVE_PATH, g_entry[i].path);
        n++;
    }
    ok = 1;
    for (i = 0; i < n; i++) {
        ok = delete_save_files(paths[i]) && ok;
    }
    for (i = 0; i < n; i++) {
        remove_if_empty(folders[i]);
    }
    saves_scan();
    return ok;
}
