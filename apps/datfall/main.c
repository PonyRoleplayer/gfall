/*
 * DATFALL: a read-only construction set (docs/data.md). A normal
 * maximized Win98 window. Left, a tree. Middle, the records of the
 * node. Right, a preview and every field of the record. comctl32 and
 * GDI load at run time; the import table stays KERNEL32 and USER32.
 * Escape quits.
 *
 * This slice is the tables. Pictures, characters, creatures, items and
 * the world map hang off the same tree.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "books.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/maindat.h"

#define ID_LIST 4
#define ID_FIND 6
#define ID_TREE 7
#define ID_PROPS 8

#define LVM_FIRST 0x1000
#define LVM_SETITEMA (LVM_FIRST + 6)
#define LVM_INSERTITEMA (LVM_FIRST + 7)
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#define LVM_DELETECOLUMN (LVM_FIRST + 28)
#define LVM_INSERTCOLUMNA (LVM_FIRST + 27)
#define LVM_SETCOLUMNA (LVM_FIRST + 26)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVIF_TEXT 0x0001
#define LVIF_STATE 0x0008
#define LVIS_FOCUSED 0x0001
#define LVIS_SELECTED 0x0002
#define LVCF_WIDTH 0x0002
#define LVCF_TEXT 0x0004
#define LVS_REPORT 0x0001
#define LVS_SINGLESEL 0x0004
#define LVS_SHOWSELALWAYS 0x0008
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVN_FIRST ((unsigned int)-100)
#define LVN_ITEMCHANGEDA (LVN_FIRST - 1)

#define TV_FIRST 0x1100
#define TVM_INSERTITEMA (TV_FIRST + 0)
#define TVM_EXPAND (TV_FIRST + 2)
#define TVM_GETNEXTITEM (TV_FIRST + 10)
#define TVM_SELECTITEM (TV_FIRST + 11)
#define TVM_GETITEMA (TV_FIRST + 12)
#define TVIF_TEXT 0x0001
#define TVIF_PARAM 0x0004
#define TVGN_CARET 0x0009
#define TVS_HASBUTTONS 0x0001
#define TVS_HASLINES 0x0002
#define TVS_LINESATROOT 0x0004
#define TVS_SHOWSELALWAYS 0x0020
#define TVE_EXPAND 0x0002
#define TVN_FIRST ((unsigned int)-400)
#define TVN_SELCHANGEDA (TVN_FIRST - 2)

#define SB_SETTEXTA (WM_USER + 1)
#define SB_SETPARTS (WM_USER + 4)
#define CCS_BOTTOM 0x0003

/* Tree lParam: kind in the top byte, two 12-bit numbers under it. */
#define ND_DATA 1
#define ND_MERGED 2
#define ND_FILE 3
#define ND_CHUNK 4
#define ND_MERGED_FILE 4095

#define VIEW_NONE 0
#define VIEW_DATA 1
#define VIEW_CHUNKS 2
#define VIEW_ROWS 3

#define NODE_MAKE(k, a, b) \
    ((LPARAM)((((unsigned int)(k) & 255u) << 24) | (((unsigned int)(a) & 4095u) << 12) | ((unsigned int)(b) & 4095u)))

typedef struct LvItem {
    unsigned int mask;
    int iItem;
    int iSubItem;
    unsigned int state;
    unsigned int stateMask;
    char *pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
} LvItem;

typedef struct LvCol {
    unsigned int mask;
    int fmt;
    int cx;
    char *pszText;
    int cchTextMax;
    int iSubItem;
} LvCol;

typedef struct NmHdr {
    HWND hwndFrom;
    unsigned int idFrom;
    unsigned int code;
} NmHdr;

typedef struct NmLv {
    HWND hwndFrom;
    unsigned int idFrom;
    unsigned int code;
    int iItem;
    int iSubItem;
    unsigned int uNewState;
    unsigned int uOldState;
    unsigned int uChanged;
    POINT ptAction;
    LPARAM lParam;
} NmLv;

typedef struct TvItem {
    unsigned int mask;
    void *hItem;
    unsigned int state;
    unsigned int stateMask;
    char *pszText;
    int cchTextMax;
    int iImage;
    int iSelectedImage;
    int cChildren;
    LPARAM lParam;
} TvItem;

typedef struct TvIns {
    void *hParent;
    void *hInsertAfter;
    TvItem item;
} TvIns;

typedef struct Gdi {
    void *(WINAPI *CreateFontA)(int, int, int, int, int, unsigned, unsigned, unsigned,
                                unsigned, unsigned, unsigned, unsigned, unsigned, const char *);
    void *(WINAPI *GetStockObject)(int);
} Gdi;

static Gdi g_gdi;
static HWND g_hwnd;
static HWND g_tree;
static HWND g_list;
static HWND g_props;
static HWND g_find;
static HWND g_find_label;
static HWND g_status;
static void *g_font;
static Books *g_books;
static int g_filling;
static int g_nd;
static int g_a;
static int g_b;
static int g_view;
static int g_map[128];
static int g_nmap;
static void *g_pick;
static RECT g_well;
static char g_q[48];

static void *sym(HMODULE dll, const char *name)
{
    return (void *)GetProcAddress(dll, name);
}

static int gdi_load(void)
{
    HMODULE dll;

    dll = LoadLibraryA("gdi32.dll");
    if (!dll) {
        return 0;
    }
    g_gdi.CreateFontA = sym(dll, "CreateFontA");
    g_gdi.GetStockObject = sym(dll, "GetStockObject");
    return g_gdi.CreateFontA && g_gdi.GetStockObject;
}

static int common_controls(void)
{
    HMODULE dll;
    void (WINAPI *init)(void);

    dll = LoadLibraryA("comctl32.dll");
    if (!dll) {
        return 0;
    }
    init = (void (WINAPI *)(void))GetProcAddress(dll, "InitCommonControls");
    if (!init) {
        return 0;
    }
    init();
    return 1;
}

static int fold_c(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int has_text(const char *hay, const char *q)
{
    int i;
    int j;

    if (!q || !q[0]) {
        return 1;
    }
    if (!hay) {
        return 0;
    }
    for (i = 0; hay[i]; i++) {
        for (j = 0; q[j]; j++) {
            if (!hay[i + j] || fold_c(hay[i + j]) != fold_c(q[j])) {
                break;
            }
        }
        if (!q[j]) {
            return 1;
        }
    }
    return 0;
}

static void read_query(void)
{
    g_q[0] = 0;
    if (g_find) {
        GetWindowTextA(g_find, g_q, (int)sizeof(g_q));
    }
}

static int mount_arena(void)
{
    char path[260];
    DWORD n;
    int i;
    int slash;

    n = GetModuleFileNameA(0, path, (DWORD)sizeof(path));
    if (!n || n >= sizeof(path)) {
        return 0;
    }
    path[n] = 0;
    slash = -1;
    for (i = 0; path[i]; i++) {
        if (path[i] == '\\' || path[i] == '/') {
            slash = i;
        }
    }
    if (slash < 1) {
        return 0;
    }
    path[slash] = 0;
    i = slash;
    path[i++] = '\\';
    path[i++] = 'A';
    path[i++] = 'R';
    path[i++] = 'E';
    path[i++] = 'N';
    path[i++] = 'A';
    path[i++] = '2';
    path[i] = 0;
    vfs_mount(path);
    return 1;
}

static void data_path(char *dst, int cap, const char *name)
{
    int i;
    int k;

    dst[0] = 0;
    if (cap < 8) {
        return;
    }
    dst[0] = 'D';
    dst[1] = 'A';
    dst[2] = 'T';
    dst[3] = 'A';
    dst[4] = '\\';
    k = 5;
    for (i = 0; name && name[i] && k < cap - 1; i++) {
        dst[k++] = name[i];
    }
    dst[k] = 0;
}

static int load_books(void)
{
    const df_u8 *bytes[BOOKS_MAX];
    df_u32 sizes[BOOKS_MAX];
    const char *names[BOOKS_MAX];
    char storage[BOOKS_MAX][32];
    char order[BOOKS_MAX][BOOKS_NAME];
    const df_u8 *text;
    df_u32 ntext;
    int norder;
    int n;
    int i;

    if (!maindat_start()) {
        MessageBoxA(0, maindat_error() ? maindat_error() : "DATA\\MAIN.DAT was not cooked.",
                    "DATFALL", MB_OK | MB_ICONWARNING);
    }
    n = 0;
    sizes[0] = 0;
    bytes[0] = vfs_read_beside("DATA\\MAIN.DAT", &sizes[0]);
    if (!bytes[0]) {
        sizes[0] = 0;
    }
    names[0] = "MAIN.DAT";
    n = 1;
    text = vfs_read_beside("DATA\\LOADORDR.TXT", &ntext);
    norder = books_order(text, text ? ntext : 0, order, BOOKS_MAX - 1);
    for (i = 0; i < norder && n < BOOKS_MAX; i++) {
        char path[64];

        data_path(path, (int)sizeof(path), order[i]);
        df_copy(storage[n], order[i], (int)sizeof(storage[n]));
        names[n] = storage[n];
        sizes[n] = 0;
        bytes[n] = vfs_read_beside(path, &sizes[n]);
        if (!bytes[n]) {
            sizes[n] = 0;
        }
        n++;
    }
    g_books = books_open(names, bytes, sizes, n);
    return g_books != 0;
}

static void font_set(HWND hwnd)
{
    if (hwnd && g_font) {
        SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_font, 1);
    }
}

static void cols_clear(HWND list)
{
    while (SendMessageA(list, LVM_DELETECOLUMN, 0, 0)) {
    }
}

/* Inserts at column 0. Call from the rightmost column back to the left. */
static void col_add(HWND list, const char *text, int width)
{
    LvCol col;

    memset(&col, 0, sizeof(col));
    col.mask = LVCF_WIDTH | LVCF_TEXT;
    col.cx = width;
    col.pszText = (char *)text;
    SendMessageA(list, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
}

static void row_text(HWND list, int index, int sub, const char *text)
{
    LvItem item;

    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = sub;
    item.pszText = (char *)(text ? text : "");
    if (sub == 0) {
        SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);
    } else {
        SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&item);
    }
}

static void select_row(HWND list, int index)
{
    LvItem item;

    memset(&item, 0, sizeof(item));
    item.mask = LVIF_STATE;
    item.iItem = index;
    item.state = LVIS_SELECTED | LVIS_FOCUSED;
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&item);
}

static void props_clear(void)
{
    SendMessageA(g_props, LVM_DELETEALLITEMS, 0, 0);
}

static void prop_add(int index, const char *name, const char *value)
{
    row_text(g_props, index, 0, name);
    row_text(g_props, index, 1, value);
}

static DatFile *file_at(int which)
{
    if (!g_books) {
        return 0;
    }
    if (which == ND_MERGED_FILE) {
        return books_merged(g_books);
    }
    return books_file(g_books, which);
}

static const char *file_title(int which)
{
    if (which == ND_MERGED_FILE) {
        return "Merged";
    }
    if (!g_books) {
        return "";
    }
    return books_name(g_books, which);
}

static void status_set(const char *file, const char *id)
{
    char mid[96];
    const char *merged;

    merged = g_books ? books_status(g_books) : "";
    mid[0] = 0;
    if (id) {
        lstrcpynA(mid, id, 80);
    }
    if (g_books && books_cut(g_books)) {
        lstrcatA(mid, "  rows cut");
    }
    SendMessageA(g_status, SB_SETTEXTA, 0, (LPARAM)(file ? file : ""));
    SendMessageA(g_status, SB_SETTEXTA, 1, (LPARAM)mid);
    SendMessageA(g_status, SB_SETTEXTA, 2, (LPARAM)merged);
}

static void show_fields(DatFile *f, int kind, int row, int with_values)
{
    int n;
    int i;
    char name[32];
    char value[192];
    int type;
    int size;
    int count;

    props_clear();
    if (!f) {
        prop_add(0, "file", "no file");
        return;
    }
    if (dat_damaged(f, kind)) {
        prop_add(0, "chunk", "damaged");
        if (dat_warning(f)[0]) {
            prop_add(1, "warning", dat_warning(f));
        }
        return;
    }
    n = dat_fields(f, kind);
    for (i = 0; i < n; i++) {
        if (!dat_field(f, kind, i, name, (int)sizeof(name), &type, &size, &count)) {
            break;
        }
        value[0] = 0;
        if (with_values) {
            dat_values(f, kind, row, i, value, (int)sizeof(value));
        } else {
            wsprintfA(value, "%c  %d x %d", type ? type : '?', size, count);
        }
        prop_add(i, name, value);
    }
}

static void on_row(int index)
{
    DatFile *f;
    char id[32];
    char label[64];
    char status_id[80];
    int row;
    int kind;

    if (g_filling) {
        return;
    }
    read_query();
    if (g_view == VIEW_DATA) {
        if (index < 0 || index >= g_nmap) {
            props_clear();
            status_set("Data", "");
            return;
        }
        f = file_at(g_map[index]);
        props_clear();
        prop_add(0, "file", file_title(g_map[index]));
        prop_add(1, "state", f && dat_ok(f) ? "read" : (f ? dat_error(f) : "missing"));
        if (f && dat_warning(f)[0]) {
            prop_add(2, "warning", dat_warning(f));
        }
        if (f) {
            wsprintfA(label, "%d", dat_kinds(f));
            prop_add(3, "chunks", label);
            wsprintfA(label, "%u", (unsigned int)dat_size(f));
            prop_add(4, "bytes", label);
        }
        status_set(file_title(g_map[index]), "");
        return;
    }
    if (g_view == VIEW_CHUNKS) {
        f = file_at(g_a == ND_MERGED_FILE || g_nd == ND_MERGED ? ND_MERGED_FILE : g_a);
        if (index < 0 || index >= g_nmap) {
            props_clear();
            status_set(file_title(g_nd == ND_MERGED ? ND_MERGED_FILE : g_a), "");
            return;
        }
        kind = g_map[index];
        show_fields(f, kind, 0, 0);
        dat_kind(f, kind, id, (int)sizeof(id), label, (int)sizeof(label));
        wsprintfA(status_id, "%s  %d rows", id, f ? dat_rows(f, kind) : 0);
        status_set(file_title(g_nd == ND_MERGED ? ND_MERGED_FILE : g_a), status_id);
        return;
    }
    if (g_view != VIEW_ROWS) {
        props_clear();
        status_set("", "");
        return;
    }
    f = file_at(g_a);
    if (!f || index < 0) {
        props_clear();
        status_set(file_title(g_a), "");
        return;
    }
    row = dat_filter_row(f, index);
    show_fields(f, g_b, row, 1);
    dat_row_label(f, g_b, row, id, (int)sizeof(id), label, (int)sizeof(label));
    wsprintfA(status_id, "id %s", id);
    status_set(file_title(g_a), status_id);
}

static void fill_list(void)
{
    DatFile *f;
    int i;
    int n;
    int shown;
    char a[64];
    char b[64];
    char c[32];
    RECT rc;
    int width;

    if (!g_list) {
        return;
    }
    read_query();
    g_filling = 1;
    SendMessageA(g_list, LVM_DELETEALLITEMS, 0, 0);
    cols_clear(g_list);
    GetClientRect(g_list, &rc);
    width = rc.right > 40 ? rc.right : 180;
    g_nmap = 0;
    shown = 0;
    if (g_view == VIEW_DATA) {
        col_add(g_list, "State", width / 4);
        col_add(g_list, "Chunks", width / 4);
        col_add(g_list, "File", width / 2);
        if (g_books) {
            f = books_merged(g_books);
            if (has_text("Merged", g_q) && g_nmap < 128) {
                g_map[g_nmap] = ND_MERGED_FILE;
                row_text(g_list, shown, 0, "Merged");
                wsprintfA(b, "%d", f ? dat_kinds(f) : 0);
                row_text(g_list, shown, 1, b);
                row_text(g_list, shown, 2, f && dat_ok(f) ? "merged" : "unread");
                g_nmap++;
                shown++;
            }
            n = books_count(g_books);
            for (i = 0; i < n && g_nmap < 128; i++) {
                if (!has_text(books_name(g_books, i), g_q)) {
                    continue;
                }
                g_map[g_nmap] = i;
                row_text(g_list, shown, 0, books_name(g_books, i));
                f = books_file(g_books, i);
                wsprintfA(b, "%d", f ? dat_kinds(f) : 0);
                row_text(g_list, shown, 1, b);
                row_text(g_list, shown, 2, f && dat_ok(f) ? "read" : "skipped");
                g_nmap++;
                shown++;
            }
        }
    } else if (g_view == VIEW_CHUNKS) {
        col_add(g_list, "Rows", width / 5);
        col_add(g_list, "Table", width / 3);
        col_add(g_list, "Tag", width / 3);
        f = file_at(g_nd == ND_MERGED ? ND_MERGED_FILE : g_a);
        n = f ? dat_kinds(f) : 0;
        for (i = 0; i < n && g_nmap < 128; i++) {
            dat_kind(f, i, a, (int)sizeof(a), b, (int)sizeof(b));
            if (!has_text(a, g_q) && !has_text(b, g_q)) {
                continue;
            }
            g_map[g_nmap] = i;
            row_text(g_list, shown, 0, a);
            row_text(g_list, shown, 1, b);
            wsprintfA(c, "%d%s", dat_rows(f, i), dat_damaged(f, i) ? " damaged" : "");
            row_text(g_list, shown, 2, c);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_ROWS) {
        col_add(g_list, "Name", width / 2);
        col_add(g_list, "Id", width / 3);
        f = file_at(g_a);
        n = f ? dat_filter(f, g_b, g_q) : 0;
        for (i = 0; i < n; i++) {
            int row;

            row = dat_filter_row(f, i);
            dat_row_label(f, g_b, row, a, (int)sizeof(a), b, (int)sizeof(b));
            row_text(g_list, i, 0, a);
            row_text(g_list, i, 1, b[0] ? b : "");
        }
        shown = n;
    }
    g_filling = 0;
    if (shown > 0) {
        select_row(g_list, 0);
        on_row(0);
    } else {
        props_clear();
        status_set(g_view == VIEW_ROWS || g_view == VIEW_CHUNKS ? file_title(g_nd == ND_MERGED ? ND_MERGED_FILE : g_a) : "Data", "");
    }
}

static int node_param(void *item, int *a, int *b)
{
    TvItem tv;
    unsigned int lp;

    memset(&tv, 0, sizeof(tv));
    tv.mask = TVIF_PARAM;
    tv.hItem = item;
    if (!item || !SendMessageA(g_tree, TVM_GETITEMA, 0, (LPARAM)&tv)) {
        return 0;
    }
    lp = (unsigned int)tv.lParam;
    *a = (int)((lp >> 12) & 4095u);
    *b = (int)(lp & 4095u);
    return (int)(lp >> 24);
}

static void on_tree(void *item)
{
    int a;
    int b;
    int nd;

    if (g_filling || !item) {
        return;
    }
    nd = node_param(item, &a, &b);
    if (!nd) {
        return;
    }
    g_nd = nd;
    g_a = a;
    g_b = b;
    if (nd == ND_DATA) {
        g_view = VIEW_DATA;
    } else if (nd == ND_MERGED || nd == ND_FILE) {
        g_view = VIEW_CHUNKS;
    } else if (nd == ND_CHUNK) {
        g_view = VIEW_ROWS;
    } else {
        g_view = VIEW_NONE;
    }
    fill_list();
}

static void *tree_add(void *parent, const char *text, int kind, int a, int b)
{
    TvIns ins;

    memset(&ins, 0, sizeof(ins));
    ins.hParent = parent ? parent : (void *)(long)-0x10000;
    ins.hInsertAfter = (void *)(long)-0x0FFFE;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = (char *)text;
    ins.item.lParam = NODE_MAKE(kind, a, b);
    return (void *)SendMessageA(g_tree, TVM_INSERTITEMA, 0, (LPARAM)&ins);
}

static void add_chunks(void *parent, DatFile *f, int which)
{
    int i;
    int n;
    char tag[8];
    char table[24];
    char line[40];

    n = f ? dat_kinds(f) : 0;
    for (i = 0; i < n; i++) {
        void *item;

        dat_kind(f, i, tag, (int)sizeof(tag), table, (int)sizeof(table));
        wsprintfA(line, "%s  %s", tag, table);
        item = tree_add(parent, line, ND_CHUNK, which, i);
        if (!g_pick && which == ND_MERGED_FILE && df_eq(tag, "ENEM")) {
            g_pick = item;
        }
    }
}

static void build_tree(void)
{
    void *data;
    void *merged;
    int i;
    int n;

    g_filling = 1;
    g_pick = 0;
    data = tree_add(0, "Data", ND_DATA, 0, 0);
    merged = tree_add(data, "Merged", ND_MERGED, ND_MERGED_FILE, 0);
    add_chunks(merged, g_books ? books_merged(g_books) : 0, ND_MERGED_FILE);
    n = g_books ? books_count(g_books) : 0;
    for (i = 0; i < n; i++) {
        void *file;

        file = tree_add(data, books_name(g_books, i), ND_FILE, i, 0);
        add_chunks(file, books_file(g_books, i), i);
    }
    SendMessageA(g_tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)data);
    SendMessageA(g_tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)merged);
    g_filling = 0;
    if (!g_pick) {
        g_pick = data;
    }
    SendMessageA(g_tree, TVM_SELECTITEM, TVGN_CARET, (LPARAM)g_pick);
}

static void layout(void)
{
    RECT client;
    int w;
    int h;
    int tree_w;
    int right_w;
    int list_x;
    int list_w;
    int right_x;
    int top;
    int body;
    int well_h;
    int edges[3];

    if (!g_hwnd || !GetClientRect(g_hwnd, &client)) {
        return;
    }
    w = client.right;
    h = client.bottom;
    if (w < 32) {
        w = 32;
    }
    if (h < 32) {
        h = 32;
    }
    SendMessageA(g_status, WM_SIZE, 0, 0);
    edges[0] = w / 3;
    edges[1] = (w * 2) / 3;
    edges[2] = -1;
    SendMessageA(g_status, SB_SETPARTS, 3, (LPARAM)edges);
    top = 6;
    tree_w = 220;
    right_w = 300;
    if (tree_w > w / 3) {
        tree_w = w / 3;
    }
    if (right_w > w / 3) {
        right_w = w / 3;
    }
    list_x = tree_w + 6;
    list_w = w - list_x - right_w - 12;
    if (list_w < 80) {
        list_w = 80;
    }
    right_x = list_x + list_w + 6;
    body = h - 28 - top;
    if (body < 40) {
        body = 40;
    }
    well_h = body * 2 / 5;
    if (well_h < 48) {
        well_h = 48;
    }
    MoveWindow(g_tree, 4, top, tree_w - 4, h - 28 - top, 1);
    MoveWindow(g_find_label, list_x, top, 32, 18, 1);
    MoveWindow(g_find, list_x + 34, top, list_w - 34, 20, 1);
    MoveWindow(g_list, list_x, top + 24, list_w, h - 28 - top - 24, 1);
    g_well.left = right_x;
    g_well.top = top;
    g_well.right = right_x + right_w;
    g_well.bottom = top + well_h;
    MoveWindow(g_props, right_x, top + well_h + 4, right_w, h - 28 - (top + well_h + 4), 1);
    InvalidateRect(g_hwnd, &g_well, 1);
}

static void paint_well(HDC dc)
{
    RECT well;
    const char *text;

    if (g_well.right - g_well.left < 8 || g_well.bottom - g_well.top < 8) {
        return;
    }
    well = g_well;
    DrawEdge(dc, &well, EDGE_SUNKEN, BF_RECT);
    well.left += 4;
    well.top += 4;
    well.right -= 4;
    well.bottom -= 4;
    text = "No picture";
    DrawTextA(dc, text, -1, &well, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        layout();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc;

        dc = BeginPaint(hwnd, &ps);
        paint_well(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE && (int)LOWORD(wp) == ID_FIND && !g_filling) {
            fill_list();
        }
        return 0;
    case WM_NOTIFY:
        if (((NmHdr *)lp)->idFrom == ID_TREE && ((NmHdr *)lp)->code == TVN_SELCHANGEDA) {
            void *item;

            item = (void *)SendMessageA(g_tree, TVM_GETNEXTITEM, TVGN_CARET, 0);
            on_tree(item);
        } else if (((NmHdr *)lp)->idFrom == ID_LIST && ((NmHdr *)lp)->code == LVN_ITEMCHANGEDA) {
            NmLv *nm;

            nm = (NmLv *)lp;
            if ((nm->uNewState & LVIS_SELECTED) && !g_filling && nm->iItem >= 0) {
                on_row(nm->iItem);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HWND make_list(HINSTANCE inst, int id)
{
    HWND hwnd;

    hwnd = CreateWindowExA(
        WS_EX_CLIENTEDGE, "SysListView32", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, g_hwnd, (HMENU)(UINT_PTR)id, inst, 0);
    if (hwnd) {
        SendMessageA(hwnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    }
    return hwnd;
}

static int win_main(HINSTANCE inst)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    mem_init();
    if (!gdi_load() || !common_controls()) {
        MessageBoxA(0, "The window controls did not load.", "DATFALL", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!mount_arena()) {
        MessageBoxA(0, "ARENA2 was not found beside the program.", "DATFALL", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!load_books()) {
        MessageBoxA(0, "The tables did not load.", "DATFALL", MB_OK | MB_ICONERROR);
        return 1;
    }
    g_font = g_gdi.CreateFontA(-11, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
    if (!g_font) {
        g_font = g_gdi.GetStockObject(SYSTEM_FONT);
    }
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(0, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "DATFALL";
    if (!RegisterClassA(&wc)) {
        return 1;
    }
    hwnd = CreateWindowExA(
        0, "DATFALL", "DATFALL",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 640,
        0, 0, inst, 0);
    if (!hwnd) {
        return 1;
    }
    g_hwnd = hwnd;
    g_tree = CreateWindowExA(
        WS_EX_CLIENTEDGE, "SysTreeView32", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)(UINT_PTR)ID_TREE, inst, 0);
    g_find_label = CreateWindowExA(
        0, "STATIC", "Find",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 10, 10, hwnd, 0, inst, 0);
    g_find = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 10, 10, hwnd, (HMENU)(UINT_PTR)ID_FIND, inst, 0);
    g_list = make_list(inst, ID_LIST);
    g_props = make_list(inst, ID_PROPS);
    g_status = CreateWindowExA(
        0, "msctls_statusbar32", "",
        WS_CHILD | WS_VISIBLE | CCS_BOTTOM,
        0, 0, 10, 10, hwnd, 0, inst, 0);
    if (!g_tree || !g_list || !g_props || !g_status) {
        MessageBoxA(0, "The list did not open.", "DATFALL", MB_OK | MB_ICONERROR);
        return 1;
    }
    font_set(g_tree);
    font_set(g_find_label);
    font_set(g_find);
    font_set(g_list);
    font_set(g_props);
    font_set(g_status);
    cols_clear(g_props);
    /* col_add inserts at column 0, so the last call is the left column. */
    col_add(g_props, "Value", 160);
    col_add(g_props, "Field", 110);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    layout();
    build_tree();
    SetFocus(g_list);
    while (GetMessageA(&msg, 0, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    books_close(g_books);
    g_books = 0;
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)win_main(GetModuleHandleA(0)));
}
