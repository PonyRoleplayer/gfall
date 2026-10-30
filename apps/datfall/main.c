/*
 * DATFALL: a read-only construction set (docs/data.md). A normal
 * maximized Win98 window. Left, a tree. Middle, the records of the
 * node. Right, a preview and every field of the record. comctl32 and
 * GDI load at run time; the import table stays KERNEL32 and USER32.
 * Escape quits.
 *
 * Data is the tables. Pictures are the texture archives. Characters
 * are the races and their paper dolls. Creatures are each kind's
 * poses. Items are the weapons, recoloured by material. World is
 * the map and the location list.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "atlas.h"
#include "beast.h"
#include "books.h"
#include "made.h"
#include "paper.h"
#include "pics.h"
#include "game/dagger/formats/dungeon.h"
#include "engine/assets/palette.h"
#include "engine/assets/texture.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/palette.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/tables/maindat.h"

#define ID_LIST 4
#define ID_FIND 6
#define ID_TREE 7
#define ID_PROPS 8
#define ID_TIMER 1
#define ID_CENTRE 20
#define ID_CLIMATE 21
#define ID_DOTS 22

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
#define ND_PICTURES 5
#define ND_ARCHIVE 6
#define ND_CHARS 7
#define ND_RACE 8
#define ND_BEASTS 9
#define ND_ENEMY 10
#define ND_ITEMS 11
#define ND_WEAPON 12
#define ND_WORLD 13
#define ND_MAP 14
#define ND_PLACES 15
#define ND_MERGED_FILE 4095

#define VIEW_NONE 0
#define VIEW_DATA 1
#define VIEW_CHUNKS 2
#define VIEW_ROWS 3
#define VIEW_ARCHIVES 4
#define VIEW_RECS 5
#define VIEW_RACES 6
#define VIEW_PARTS 7
#define VIEW_ENEMIES 8
#define VIEW_POSES 9
#define VIEW_WEAPONS 10
#define VIEW_METALS 11
#define VIEW_MAP 12
#define VIEW_PLACES 13

#define ARC_CAP 160
#define GRID_CELL 32
#define GRID_COLS 8

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
    int (WINAPI *SetStretchBltMode)(HDC, int);
    int (WINAPI *StretchDIBits)(HDC, int, int, int, int, int, int, int, int,
                                const void *, const BITMAPINFO *, unsigned int, unsigned int);
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
static int g_map[512];
static int g_nmap;
static void *g_pick;
static RECT g_well;
static RECT g_large;
static char g_q[48];
static WNDPROC g_list_proc;
static Palette g_pal;
static int g_pal_ok;
static TexArc g_arcs[TEX_SCAN];
static int g_narc;
static MemHeap *g_arc_heap;
static MemHeap *g_frame_heap;
static Image *g_thumb[ARC_CAP];
static int g_fn[ARC_CAP];
static int g_src[ARC_CAP];
static int g_thumb_n;
static int g_arc_file;
static int g_rec;
static int g_frame;
static Image *g_show;
static unsigned char *g_grid;
static int g_grid_w;
static int g_grid_h;
static int g_grid_rows;
static int g_scroll;
static unsigned char *g_dib;
static CifSet g_cif;
static int g_cif_on;
static int g_head;
static Image g_fake;
static char g_part_file[PAPER_NAME];
static BeastPose g_poses[BEAST_POSES];
static int g_nposes;
static int g_metal_on;
static int g_metal;
static DunRef *g_duns;
static int g_ndun;
static DunRef *g_towns;
static int g_ntown;
static int *g_hit_i;
static unsigned char *g_hit_out;
static int g_nhit;
static unsigned char *g_mapdib;
static int g_map_ready;
static int g_ox;
static int g_oy;
static int g_zoom;
static int g_climate_on;
static int g_dots_on;
static int g_px;
static int g_py;
static int g_focus_on;
static int g_map_opened;
static HWND g_btn_climate;
static HWND g_btn_dots;
static HWND g_btn_centre;
static void *g_map_node;
static Dungeon g_place;
static int g_place_ok;

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
    g_gdi.SetStretchBltMode = sym(dll, "SetStretchBltMode");
    g_gdi.StretchDIBits = sym(dll, "StretchDIBits");
    g_gdi.GetStockObject = sym(dll, "GetStockObject");
    return g_gdi.CreateFontA && g_gdi.SetStretchBltMode && g_gdi.StretchDIBits && g_gdi.GetStockObject;
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

static void drop_frames(void);
static void drop_arc(void);
static int arc_records(int file);
static void load_archive(int file);
static int load_views(int file, int first, int count);
static void show_frame(void);
static void pic_status(void);
static void scroll_to_rec(void);
static void step_frame(int delta);
static void drop_paper(void);
static void load_part(int row, int part);
static void load_weapon(int weapon_id, int material_id);
static void show_head(void);
static void race_status(void);
static void weapon_status(void);
static void show_weapon(int weapon_id, int material_id);
static void step_head(int delta);
static void show_pixel(int px, int py);
static void show_place(int index);
static void map_rebuild(void);
static void map_centre(int px, int py);
static void load_places(void);

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
    if (g_view == VIEW_ARCHIVES) {
        int file;
        char name[24];
        char num[16];

        drop_arc();
        props_clear();
        if (index < 0 || index >= g_nmap) {
            status_set("Pictures", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        file = g_map[index];
        wsprintfA(name, "TEXTURE.%03d", file);
        wsprintfA(num, "%d", arc_records(file));
        prop_add(0, "archive", name);
        prop_add(1, "records", num);
        status_set(name, "");
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_RECS) {
        char num[16];
        int tw;
        int th;
        int frames;

        if (index < 0 || index >= g_nmap) {
            g_rec = -1;
            drop_frames();
            props_clear();
            pic_status();
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        g_rec = g_map[index];
        g_frame = 0;
        show_frame();
        scroll_to_rec();
        tw = g_show ? g_show->width : 0;
        th = g_show ? g_show->height : 0;
        frames = (g_rec >= 0 && g_rec < g_thumb_n) ? g_fn[g_rec] : 0;
        props_clear();
        wsprintfA(num, "%d", g_rec);
        prop_add(0, "record", num);
        wsprintfA(num, "%d", tw);
        prop_add(1, "width", num);
        wsprintfA(num, "%d", th);
        prop_add(2, "height", num);
        wsprintfA(num, "%d", frames);
        prop_add(3, "frames", num);
        wsprintfA(num, "TEXTURE.%03d", g_arc_file);
        prop_add(4, "archive", num);
        pic_status();
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_RACES) {
        DatFile *mf;
        int rk;
        int race;

        drop_paper();
        mf = g_books ? books_merged(g_books) : 0;
        rk = mf ? dat_find_tag(mf, "RACE") : -1;
        if (!mf || rk < 0 || index < 0) {
            props_clear();
            status_set("Characters", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        race = dat_filter_row(mf, index);
        show_fields(mf, rk, race, 1);
        dat_row_label(mf, rk, race, id, (int)sizeof(id), label, (int)sizeof(label));
        wsprintfA(status_id, "id %s", id);
        status_set("RACE", status_id);
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_PARTS) {
        int part;

        if (index < 0 || index >= g_nmap) {
            drop_paper();
            props_clear();
            status_set("Characters", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        part = g_map[index];
        load_part(g_a, part);
        f = g_books ? books_merged(g_books) : 0;
        show_fields(f, g_b, g_a, 1);
        race_status();
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_ENEMIES) {
        DatFile *mf;
        int ek;
        int erow;

        drop_arc();
        drop_paper();
        mf = g_books ? books_merged(g_books) : 0;
        ek = mf ? dat_find_tag(mf, "ENEM") : -1;
        if (!mf || ek < 0 || index < 0) {
            props_clear();
            status_set("Creatures", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        erow = dat_filter_row(mf, index);
        show_fields(mf, ek, erow, 1);
        dat_row_label(mf, ek, erow, id, (int)sizeof(id), label, (int)sizeof(label));
        wsprintfA(status_id, "id %s", id);
        status_set("ENEM", status_id);
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_POSES) {
        int pose;

        if (index < 0 || index >= g_nmap || index >= g_nposes) {
            drop_arc();
            props_clear();
            status_set("Creatures", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        pose = g_map[index];
        if (pose < 0 || pose >= g_nposes) {
            drop_arc();
            return;
        }
        g_rec = 0;
        g_frame = 0;
        if (load_views(g_poses[pose].archive, g_poses[pose].record, g_poses[pose].views)) {
            show_frame();
            scroll_to_rec();
        }
        f = g_books ? books_merged(g_books) : 0;
        show_fields(f, g_b, g_a, 1);
        if (g_arc_file > 0) {
            pic_status();
        } else {
            status_set(g_poses[pose].label, "no picture");
        }
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_WEAPONS) {
        int wid;
        char wname[32];

        drop_paper();
        drop_arc();
        if (index < 0 || index >= g_nmap) {
            props_clear();
            status_set("Items", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        wid = 0;
        wname[0] = 0;
        made_weapon_at(g_books ? books_merged(g_books) : 0, g_map[index], &wid, wname, (int)sizeof(wname));
        props_clear();
        wsprintfA(status_id, "%d", wid);
        prop_add(0, "id", status_id);
        prop_add(1, "name", wname);
        wsprintfA(label, "WEAPON%02d.CIF", made_cif(g_books ? books_merged(g_books) : 0, wid));
        prop_add(2, "cif", label);
        status_set("WEAP", status_id);
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_METALS) {
        int mat;

        if (index < 0 || index >= g_nmap) {
            drop_paper();
            props_clear();
            status_set("Items", "");
            InvalidateRect(g_hwnd, &g_well, 1);
            return;
        }
        mat = g_map[index];
        load_weapon(g_a, mat);
        show_weapon(g_a, mat);
        weapon_status();
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if (g_view == VIEW_PLACES) {
        show_place(index);
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
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

static void drop_frames(void)
{
    g_show = 0;
    if (g_frame_heap) {
        mem_heap_delete(g_frame_heap);
        g_frame_heap = 0;
    }
}

static void drop_arc(void)
{
    int i;

    drop_frames();
    for (i = 0; i < ARC_CAP; i++) {
        g_thumb[i] = 0;
        g_fn[i] = 0;
    }
    g_thumb_n = 0;
    g_grid = 0;
    g_grid_w = 0;
    g_grid_h = 0;
    g_grid_rows = 0;
    g_scroll = 0;
    if (g_arc_heap) {
        mem_heap_delete(g_arc_heap);
        g_arc_heap = 0;
    }
    g_arc_file = -1;
}

static int arc_records(int file)
{
    int i;

    for (i = 0; i < g_narc; i++) {
        if (g_arcs[i].file == file) {
            return g_arcs[i].records;
        }
    }
    return 0;
}

static void write_ix(unsigned char *p, int ix, int x, int y)
{
    int r;
    int g;
    int b;
    int on;

    if (g_metal_on && ix > 0) {
        ix = cif_metal(ix, g_metal);
    }
    if (ix <= 0) {
        on = ((x >> 3) ^ (y >> 3)) & 1;
        r = on ? 255 : 192;
        g = r;
        b = r;
    } else if (g_pal_ok && ix < 256) {
        r = g_pal.r[ix];
        g = g_pal.g[ix];
        b = g_pal.b[ix];
    } else {
        r = ix;
        g = ix;
        b = ix;
    }
    p[0] = (unsigned char)b;
    p[1] = (unsigned char)g;
    p[2] = (unsigned char)r;
}

static void build_grid(void)
{
    MemHeap *prev;
    int rows;
    int stride;
    int i;
    int cell;
    int margin;

    rows = (g_thumb_n + GRID_COLS - 1) / GRID_COLS;
    if (rows < 1) {
        rows = 1;
    }
    g_grid_rows = rows;
    g_grid_w = GRID_COLS * GRID_CELL;
    g_grid_h = rows * GRID_CELL;
    stride = (g_grid_w * 3 + 3) & ~3;
    if (!g_arc_heap) {
        return;
    }
    prev = mem_heap_use(g_arc_heap);
    g_grid = (unsigned char *)mem_alloc((df_u32)stride * (df_u32)g_grid_h);
    mem_heap_use(prev);
    if (!g_grid) {
        return;
    }
    memset(g_grid, 192, (unsigned int)stride * (unsigned int)g_grid_h);
    cell = GRID_CELL;
    margin = 2;
    for (i = 0; i < g_thumb_n; i++) {
        Image *img;
        int col;
        int row;
        int x0;
        int y0;
        int dw;
        int dh;
        int iy;
        const df_u8 *pix;
        int sw;
        int sh;

        img = g_thumb[i];
        col = i % GRID_COLS;
        row = i / GRID_COLS;
        x0 = col * cell + margin;
        y0 = row * cell + margin;
        dw = cell - margin * 2;
        dh = dw;
        if (!img || !img->index || img->width < 1 || img->height < 1) {
            continue;
        }
        pix = img->index;
        sw = img->width;
        sh = img->height;
        for (iy = 0; iy < dh; iy++) {
            unsigned char *dst;
            int sy;
            int ix;

            sy = iy * sh / dh;
            dst = g_grid + (y0 + iy) * stride + x0 * 3;
            for (ix = 0; ix < dw; ix++) {
                int sx;
                int index;

                sx = ix * sw / dw;
                index = pix[sy * sw + sx];
                write_ix(dst + ix * 3, index, x0 + ix, y0 + iy);
            }
        }
    }
}

static void load_archive(int file)
{
    MemHeap *prev;
    int n;
    int i;

    if (g_arc_heap && file == g_arc_file) {
        return;
    }
    drop_arc();
    n = arc_records(file);
    if (n > ARC_CAP) {
        n = ARC_CAP;
    }
    if (n < 0) {
        n = 0;
    }
    g_arc_heap = mem_heap_new();
    if (!g_arc_heap) {
        return;
    }
    g_arc_file = file;
    prev = mem_heap_use(g_arc_heap);
    for (i = 0; i < n; i++) {
        g_src[i] = i;
        g_fn[i] = texture_frame_count(file, i);
        if (g_fn[i] < 1) {
            g_fn[i] = 1;
        }
        g_thumb[i] = texture_load_frame(file, i, 0);
    }
    mem_heap_use(prev);
    g_thumb_n = n;
    g_rec = 0;
    g_frame = 0;
    g_scroll = 0;
    build_grid();
}

/* The pose's views only, not the whole archive. g_src maps a thumbnail to the record. */
static int load_views(int file, int first, int count)
{
    MemHeap *prev;
    int i;

    drop_arc();
    if (file <= 0 || count < 1) {
        return 0;
    }
    if (count > ARC_CAP) {
        count = ARC_CAP;
    }
    g_arc_heap = mem_heap_new();
    if (!g_arc_heap) {
        return 0;
    }
    g_arc_file = file;
    prev = mem_heap_use(g_arc_heap);
    for (i = 0; i < count; i++) {
        g_src[i] = first + i;
        g_fn[i] = texture_frame_count(file, first + i);
        if (g_fn[i] < 1) {
            g_fn[i] = 1;
        }
        g_thumb[i] = texture_load_frame(file, first + i, 0);
    }
    mem_heap_use(prev);
    g_thumb_n = count;
    g_rec = 0;
    g_frame = 0;
    g_scroll = 0;
    build_grid();
    return g_grid != 0;
}

static void show_frame(void)
{
    int frames;
    MemHeap *prev;

    drop_frames();
    if (g_rec < 0 || g_rec >= g_thumb_n) {
        return;
    }
    frames = g_fn[g_rec];
    if (frames < 1) {
        frames = 1;
    }
    if (g_frame < 0) {
        g_frame = 0;
    }
    if (g_frame >= frames) {
        g_frame = 0;
    }
    if (g_frame == 0) {
        g_show = g_thumb[g_rec];
        return;
    }
    g_frame_heap = mem_heap_new();
    if (!g_frame_heap) {
        g_show = g_thumb[g_rec];
        return;
    }
    prev = mem_heap_use(g_frame_heap);
    g_show = texture_load_frame(g_arc_file, g_src[g_rec], g_frame);
    mem_heap_use(prev);
}

static void pic_status(void)
{
    char file[24];
    char id[72];
    int w;
    int h;
    int n;

    w = g_show ? g_show->width : 0;
    h = g_show ? g_show->height : 0;
    n = (g_rec >= 0 && g_rec < g_thumb_n) ? g_fn[g_rec] : 0;
    wsprintfA(file, "TEXTURE.%03d", g_arc_file);
    wsprintfA(id, "record %d   %d x %d   frame %d / %d",
              (g_rec >= 0 && g_rec < ARC_CAP) ? g_src[g_rec] : g_rec,
              w, h, n ? g_frame + 1 : 0, n);
    status_set(file, id);
}

static void scroll_to_rec(void)
{
    int row;
    int y;
    int view;

    if (g_rec < 0) {
        return;
    }
    row = g_rec / GRID_COLS;
    y = row * GRID_CELL;
    view = g_well.bottom - g_well.top;
    view = view / 2;
    if (view < GRID_CELL) {
        view = GRID_CELL;
    }
    if (y < g_scroll) {
        g_scroll = y;
    }
    if (y + GRID_CELL > g_scroll + view) {
        g_scroll = y + GRID_CELL - view;
    }
    if (g_scroll < 0) {
        g_scroll = 0;
    }
    if (g_grid_h > view && g_scroll > g_grid_h - view) {
        g_scroll = g_grid_h - view;
    }
}

static void step_frame(int delta)
{
    int n;

    if ((g_view != VIEW_RECS && g_view != VIEW_POSES) || g_rec < 0 || g_rec >= g_thumb_n) {
        return;
    }
    n = g_fn[g_rec];
    if (n < 2) {
        return;
    }
    g_frame += delta;
    while (g_frame < 0) {
        g_frame += n;
    }
    while (g_frame >= n) {
        g_frame -= n;
    }
    show_frame();
    pic_status();
    if (g_hwnd) {
        InvalidateRect(g_hwnd, &g_large, 0);
    }
}

static int ends_cif(const char *name)
{
    int n;
    char a;
    char b;
    char c;

    n = 0;
    if (!name) {
        return 0;
    }
    while (name[n]) {
        n++;
    }
    if (n < 4 || name[n - 4] != '.') {
        return 0;
    }
    a = name[n - 3];
    b = name[n - 2];
    c = name[n - 1];
    if (a >= 'a' && a <= 'z') {
        a = (char)(a - 'a' + 'A');
    }
    if (b >= 'a' && b <= 'z') {
        b = (char)(b - 'a' + 'A');
    }
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    return a == 'C' && b == 'I' && c == 'F';
}

static void drop_paper(void)
{
    g_show = 0;
    memset(&g_fake, 0, sizeof(g_fake));
    g_head = 0;
    g_part_file[0] = 0;
    g_metal_on = 0;
    if (g_cif_on) {
        cif_close(&g_cif);
        g_cif_on = 0;
    }
}

static void show_head(void)
{
    const CifFrame *fr;

    fr = cif_frame(&g_cif, g_head);
    if (!fr || !fr->pixels || fr->w < 1 || fr->h < 1) {
        g_show = 0;
        return;
    }
    memset(&g_fake, 0, sizeof(g_fake));
    g_fake.width = fr->w;
    g_fake.height = fr->h;
    g_fake.index = (df_u8 *)fr->pixels;
    g_show = &g_fake;
}

static void load_part(int row, int part)
{
    char name[PAPER_NAME];
    ImgPicture pic;
    DatFile *f;
    int i;

    drop_frames();
    drop_paper();
    f = g_books ? books_merged(g_books) : 0;
    name[0] = 0;
    if (!paper_file(f, row, part, name, (int)sizeof(name)) || !name[0]) {
        return;
    }
    for (i = 0; i < PAPER_NAME - 1 && name[i]; i++) {
        g_part_file[i] = name[i];
    }
    g_part_file[i] = 0;
    if (ends_cif(name)) {
        if (!cif_open(name, &g_cif)) {
            return;
        }
        g_cif_on = 1;
        g_head = 0;
        show_head();
        return;
    }
    if (!img_load(name, &pic) || !pic.pixels || pic.width < 1 || pic.height < 1) {
        return;
    }
    memset(&g_fake, 0, sizeof(g_fake));
    g_fake.width = pic.width;
    g_fake.height = pic.height;
    g_fake.index = (df_u8 *)pic.pixels;
    g_show = &g_fake;
}

static void load_weapon(int weapon_id, int material_id)
{
    char name[16];
    int cif;
    int i;
    DatFile *f;

    drop_frames();
    drop_paper();
    f = g_books ? books_merged(g_books) : 0;
    cif = made_cif(f, weapon_id);
    g_metal = material_id;
    g_metal_on = 1;
    if (cif < 0) {
        return;
    }
    wsprintfA(name, "WEAPON%02d.CIF", cif);
    for (i = 0; i < PAPER_NAME - 1 && name[i]; i++) {
        g_part_file[i] = name[i];
    }
    g_part_file[i] = 0;
    if (!cif_open(name, &g_cif)) {
        return;
    }
    g_cif_on = 1;
    g_head = 0;
    show_head();
}

static void race_status(void)
{
    DatFile *f;
    char id[32];
    char label[64];
    char extra[80];

    f = g_books ? books_merged(g_books) : 0;
    id[0] = 0;
    label[0] = 0;
    if (f && g_b >= 0) {
        dat_row_label(f, g_b, g_a, id, (int)sizeof(id), label, (int)sizeof(label));
    }
    if (g_cif_on && g_cif.n > 0) {
        wsprintfA(extra, "id %s  %s  head %d/%d", id, label, g_head + 1, g_cif.n);
    } else {
        wsprintfA(extra, "id %s  %s", id, label);
    }
    status_set(g_part_file[0] ? g_part_file : "Characters", extra);
}

static int add_fields(DatFile *f, int kind, int row, const char *prefix, int at)
{
    int n;
    int i;
    char name[32];
    char shown[48];
    char value[192];
    int type;
    int size;
    int count;

    if (!f || kind < 0 || row < 0) {
        return at;
    }
    n = dat_fields(f, kind);
    for (i = 0; i < n; i++) {
        name[0] = 0;
        if (!dat_field(f, kind, i, name, (int)sizeof(name), &type, &size, &count)) {
            break;
        }
        value[0] = 0;
        dat_values(f, kind, row, i, value, (int)sizeof(value));
        wsprintfA(shown, "%s %s", prefix, name);
        prop_add(at, shown, value);
        at++;
    }
    return at;
}

static void show_weapon(int weapon_id, int material_id)
{
    DatFile *f;
    int kind;
    int row;
    int at;
    MadeItem made;
    char text[32];
    char id[16];

    props_clear();
    f = g_books ? books_merged(g_books) : 0;
    at = 0;
    wsprintfA(id, "%d", weapon_id);
    kind = f ? dat_find_tag(f, "ITEM") : -1;
    row = -1;
    if (f && kind >= 0) {
        int i;
        int n;
        char got[16];

        n = dat_rows(f, kind);
        for (i = 0; i < n; i++) {
            got[0] = 0;
            dat_value(f, kind, i, 0, 0, got, (int)sizeof(got));
            if (df_eq(got, id)) {
                row = i;
                break;
            }
        }
    }
    prop_add(at++, "ITEM", id);
    at = add_fields(f, kind, row, "item", at);
    kind = f ? dat_find_tag(f, "WEAP") : -1;
    row = -1;
    if (f && kind >= 0) {
        int i;
        int n;
        char got[16];

        n = dat_rows(f, kind);
        for (i = 0; i < n; i++) {
            got[0] = 0;
            dat_value(f, kind, i, 0, 0, got, (int)sizeof(got));
            if (df_eq(got, id)) {
                row = i;
                break;
            }
        }
    }
    at = add_fields(f, kind, row, "weap", at);
    wsprintfA(id, "%d", material_id);
    kind = f ? dat_find_tag(f, "MATL") : -1;
    row = -1;
    if (f && kind >= 0) {
        int i;
        int n;
        char got[16];

        n = dat_rows(f, kind);
        for (i = 0; i < n; i++) {
            got[0] = 0;
            dat_value(f, kind, i, 0, 0, got, (int)sizeof(got));
            if (df_eq(got, id)) {
                row = i;
                break;
            }
        }
    }
    at = add_fields(f, kind, row, "matl", at);
    if (made_of(f, weapon_id, material_id, &made)) {
        wsprintfA(text, "%d.%02d kg", made.quarters / 4, (made.quarters % 4) * 25);
        prop_add(at++, "made weight", text);
        wsprintfA(text, "%d", made.value);
        prop_add(at++, "made value", text);
        wsprintfA(text, "%d", made.condition);
        prop_add(at++, "made condition", text);
    }
}

static void weapon_status(void)
{
    char extra[80];

    if (g_cif_on && g_cif.n > 0) {
        wsprintfA(extra, "metal %d  frame %d/%d", g_metal, g_head + 1, g_cif.n);
    } else {
        wsprintfA(extra, "metal %d", g_metal);
    }
    status_set(g_part_file[0] ? g_part_file : "Items", extra);
}

static void map_span(int *sw, int *sh)
{
    int w;
    int h;

    if (g_zoom < 1) {
        g_zoom = 1;
    }
    w = g_well.right - g_well.left;
    h = g_well.bottom - g_well.top;
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    *sw = w / g_zoom;
    *sh = h / g_zoom;
    if (*sw < 1) {
        *sw = 1;
    }
    if (*sh < 1) {
        *sh = 1;
    }
    if (*sw > WORLDMAP_W) {
        *sw = WORLDMAP_W;
    }
    if (*sh > WORLDMAP_H) {
        *sh = WORLDMAP_H;
    }
}

static void map_clamp(void)
{
    int sw;
    int sh;

    map_span(&sw, &sh);
    if (g_ox < 0) {
        g_ox = 0;
    }
    if (g_oy < 0) {
        g_oy = 0;
    }
    if (g_ox > WORLDMAP_W - sw) {
        g_ox = WORLDMAP_W - sw;
    }
    if (g_oy > WORLDMAP_H - sh) {
        g_oy = WORLDMAP_H - sh;
    }
}

static void map_centre(int px, int py)
{
    int sw;
    int sh;

    map_span(&sw, &sh);
    g_ox = px - sw / 2;
    g_oy = py - sh / 2;
    map_clamp();
}

static void map_rebuild(void)
{
    if (!g_mapdib) {
        g_mapdib = (unsigned char *)mem_keep((df_u32)ATLAS_STRIDE * (df_u32)WORLDMAP_H);
    }
    if (!g_mapdib) {
        return;
    }
    if (!worldmap_open()) {
        memset(g_mapdib, 40, (unsigned int)ATLAS_STRIDE * (unsigned int)WORLDMAP_H);
        g_map_ready = 0;
        return;
    }
    atlas_build(g_mapdib, g_climate_on, g_dots_on);
    g_map_ready = 1;
}

static void show_pixel(int px, int py)
{
    char text[64];
    char name[40];
    int region;
    int index;
    int type;
    int i;

    props_clear();
    if (px < 0 || py < 0 || px >= WORLDMAP_W || py >= WORLDMAP_H || !g_map_ready) {
        prop_add(0, "map", g_map_ready ? "outside" : "unread");
        status_set("WOODS.WLD", "");
        return;
    }
    g_px = px;
    g_py = py;
    wsprintfA(text, "%d, %d", px, py);
    prop_add(0, "pixel", text);
    wsprintfA(text, "%d", worldmap_height(px, py));
    prop_add(1, "height", text);
    prop_add(2, "climate", atlas_climate_name(worldmap_climate(px, py)));
    type = worldmap_location_type(px, py);
    name[0] = 0;
    region = -1;
    index = -1;
    if (worldmap_location(px, py, &region, &index)) {
        for (i = 0; i < g_ntown; i++) {
            if (g_towns[i].region == region && g_towns[i].index == index) {
                lstrcpynA(name, g_towns[i].name, (int)sizeof(name));
                break;
            }
        }
        if (!name[0]) {
            for (i = 0; i < g_ndun; i++) {
                if (g_duns[i].region == region && g_duns[i].index == index) {
                    lstrcpynA(name, g_duns[i].name, (int)sizeof(name));
                    break;
                }
            }
        }
    }
    prop_add(3, "location", name[0] ? name : (type >= 0 ? atlas_type_name(type) : "none"));
    if (region >= 0) {
        wsprintfA(text, "%d", region);
    } else {
        text[0] = 0;
    }
    prop_add(4, "region", text);
    prop_add(5, "type", atlas_type_name(type));
    wsprintfA(text, "%d, %d", px, py);
    status_set("WOODS.WLD", text);
}

static const DunRef *hit_ref(int index)
{
    if (index < 0 || index >= g_nhit || !g_hit_i || !g_hit_out) {
        return 0;
    }
    if (g_hit_out[index]) {
        return &g_towns[g_hit_i[index]];
    }
    return &g_duns[g_hit_i[index]];
}

static void show_place(int index)
{
    const DunRef *ref;
    int px;
    int py;
    int at;
    char text[64];

    props_clear();
    g_place_ok = 0;
    ref = hit_ref(index);
    if (!ref) {
        status_set("Locations", "");
        return;
    }
    if (ref->outdoor) {
        g_place_ok = exterior_load_at(ref->region, ref->index, &g_place);
    } else {
        g_place_ok = dungeon_load_at(ref->region, ref->index, &g_place);
    }
    at = 0;
    prop_add(at++, "name", ref->name);
    wsprintfA(text, "%d", ref->region);
    prop_add(at++, "region", text);
    if (g_place_ok) {
        prop_add(at++, "climate", atlas_climate_name(g_place.climate));
    }
    px = -1;
    py = -1;
    if (worldmap_open() && worldmap_pixel_of(ref->region, ref->index, &px, &py)) {
        g_px = px;
        g_py = py;
        wsprintfA(text, "%d, %d", px, py);
        prop_add(at++, "pixel", text);
        prop_add(at++, "type", atlas_type_name(worldmap_location_type(px, py)));
    } else {
        prop_add(at++, "pixel", "none");
    }
    wsprintfA(text, "%d", g_place_ok ? g_place.nblocks : ref->nblocks);
    prop_add(at++, "blocks", text);
    status_set(ref->name, text);
}

static void load_places(void)
{
    int n;

    worldmap_open();
    n = dungeon_list(0, 0);
    if (n > 0) {
        g_duns = (DunRef *)mem_keep((df_u32)n * (df_u32)sizeof(DunRef));
        if (g_duns) {
            g_ndun = dungeon_list(g_duns, n);
        }
    }
    n = exterior_list(0, 0);
    if (n > 0) {
        g_towns = (DunRef *)mem_keep((df_u32)n * (df_u32)sizeof(DunRef));
        if (g_towns) {
            g_ntown = exterior_list(g_towns, n);
        }
    }
    n = g_ndun + g_ntown;
    if (n > 0) {
        g_hit_i = (int *)mem_keep((df_u32)n * (df_u32)sizeof(int));
        g_hit_out = (unsigned char *)mem_keep((df_u32)n);
    }
}

static void step_head(int delta)
{
    int n;

    if ((g_view != VIEW_PARTS && g_view != VIEW_METALS) || !g_cif_on) {
        return;
    }
    n = g_cif.n;
    if (n < 2) {
        return;
    }
    g_head += delta;
    while (g_head < 0) {
        g_head += n;
    }
    while (g_head >= n) {
        g_head -= n;
    }
    show_head();
    if (g_view == VIEW_METALS) {
        weapon_status();
    } else {
        race_status();
    }
    if (g_hwnd) {
        InvalidateRect(g_hwnd, &g_well, 0);
    }
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
            if (has_text("Merged", g_q) && g_nmap < 512) {
                g_map[g_nmap] = ND_MERGED_FILE;
                row_text(g_list, shown, 0, "Merged");
                wsprintfA(b, "%d", f ? dat_kinds(f) : 0);
                row_text(g_list, shown, 1, b);
                row_text(g_list, shown, 2, f && dat_ok(f) ? "merged" : "unread");
                g_nmap++;
                shown++;
            }
            n = books_count(g_books);
            for (i = 0; i < n && g_nmap < 512; i++) {
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
        for (i = 0; i < n && g_nmap < 512; i++) {
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
    } else if (g_view == VIEW_ARCHIVES) {
        col_add(g_list, "Records", width / 4);
        col_add(g_list, "Archive", width / 2);
        for (i = 0; i < g_narc && g_nmap < 512; i++) {
            wsprintfA(a, "TEXTURE.%03d", g_arcs[i].file);
            if (!has_text(a, g_q)) {
                continue;
            }
            g_map[g_nmap] = g_arcs[i].file;
            row_text(g_list, shown, 0, a);
            wsprintfA(b, "%d", g_arcs[i].records);
            row_text(g_list, shown, 1, b);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_RECS) {
        int recs;

        load_archive(g_a);
        col_add(g_list, "Frames", width / 5);
        col_add(g_list, "Size", width / 3);
        col_add(g_list, "Record", width / 4);
        recs = g_thumb_n;
        for (i = 0; i < recs && g_nmap < 512; i++) {
            int tw;
            int th;

            wsprintfA(a, "%d", i);
            if (!has_text(a, g_q)) {
                continue;
            }
            g_map[g_nmap] = i;
            tw = g_thumb[i] ? g_thumb[i]->width : 0;
            th = g_thumb[i] ? g_thumb[i]->height : 0;
            row_text(g_list, shown, 0, a);
            wsprintfA(b, "%d x %d", tw, th);
            row_text(g_list, shown, 1, b);
            wsprintfA(c, "%d", g_fn[i]);
            row_text(g_list, shown, 2, c);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_RACES) {
        int kind;

        col_add(g_list, "Name", width / 2);
        col_add(g_list, "Id", width / 4);
        f = g_books ? books_merged(g_books) : 0;
        kind = f ? dat_find_tag(f, "RACE") : -1;
        n = (f && kind >= 0) ? dat_filter(f, kind, g_q) : 0;
        for (i = 0; i < n; i++) {
            int row;

            row = dat_filter_row(f, i);
            dat_row_label(f, kind, row, a, (int)sizeof(a), b, (int)sizeof(b));
            row_text(g_list, i, 0, a);
            row_text(g_list, i, 1, b[0] ? b : "");
        }
        shown = n;
    } else if (g_view == VIEW_PARTS) {
        col_add(g_list, "File", width / 2);
        col_add(g_list, "Part", width / 3);
        f = g_books ? books_merged(g_books) : 0;
        for (i = 0; i < PAPER_PARTS && g_nmap < 512; i++) {
            paper_file(f, g_a, i, b, (int)sizeof(b));
            if (g_q[0] && !has_text(paper_label(i), g_q) && !has_text(b, g_q)) {
                continue;
            }
            g_map[g_nmap] = i;
            row_text(g_list, shown, 0, paper_label(i));
            row_text(g_list, shown, 1, b);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_ENEMIES) {
        int kind;

        col_add(g_list, "Name", width / 2);
        col_add(g_list, "Id", width / 4);
        f = g_books ? books_merged(g_books) : 0;
        kind = f ? dat_find_tag(f, "ENEM") : -1;
        n = (f && kind >= 0) ? dat_filter(f, kind, g_q) : 0;
        for (i = 0; i < n; i++) {
            int row;

            row = dat_filter_row(f, i);
            dat_row_label(f, kind, row, a, (int)sizeof(a), b, (int)sizeof(b));
            row_text(g_list, i, 0, a);
            row_text(g_list, i, 1, b[0] ? b : "");
        }
        shown = n;
    } else if (g_view == VIEW_POSES) {
        col_add(g_list, "Records", width / 3);
        col_add(g_list, "Archive", width / 4);
        col_add(g_list, "Pose", width / 3);
        for (i = 0; i < g_nposes && g_nmap < 512; i++) {
            if (g_q[0] && !has_text(g_poses[i].label, g_q)) {
                continue;
            }
            g_map[g_nmap] = i;
            row_text(g_list, shown, 0, g_poses[i].label);
            if (g_poses[i].archive > 0) {
                wsprintfA(b, "TEXTURE.%03d", g_poses[i].archive);
            } else {
                lstrcpynA(b, "none", (int)sizeof(b));
            }
            row_text(g_list, shown, 1, b);
            if (g_poses[i].views > 1) {
                wsprintfA(c, "%d-%d", g_poses[i].record, g_poses[i].record + g_poses[i].views - 1);
            } else {
                wsprintfA(c, "%d", g_poses[i].record);
            }
            row_text(g_list, shown, 2, c);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_WEAPONS) {
        int id;

        col_add(g_list, "Cif", width / 5);
        col_add(g_list, "Name", width / 2);
        col_add(g_list, "Id", width / 5);
        f = g_books ? books_merged(g_books) : 0;
        for (i = 0; g_nmap < 512; i++) {
            int cif;

            if (!made_weapon_at(f, i, &id, a, (int)sizeof(a))) {
                break;
            }
            wsprintfA(c, "%d", id);
            cif = made_cif(f, id);
            wsprintfA(b, "WEAPON%02d.CIF", cif);
            if (g_q[0] && !has_text(a, g_q) && !has_text(c, g_q) && !has_text(b, g_q)) {
                continue;
            }
            g_map[g_nmap] = i;
            row_text(g_list, shown, 0, c);
            row_text(g_list, shown, 1, a);
            row_text(g_list, shown, 2, b);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_METALS) {
        int id;

        col_add(g_list, "Name", width / 2);
        col_add(g_list, "Id", width / 5);
        f = g_books ? books_merged(g_books) : 0;
        for (i = 0; g_nmap < 512; i++) {
            if (!made_material_at(f, i, &id, a, (int)sizeof(a))) {
                break;
            }
            wsprintfA(b, "%d", id);
            if (g_q[0] && !has_text(a, g_q) && !has_text(b, g_q)) {
                continue;
            }
            g_map[g_nmap] = id;
            row_text(g_list, shown, 0, b);
            row_text(g_list, shown, 1, a);
            g_nmap++;
            shown++;
        }
    } else if (g_view == VIEW_PLACES && g_hit_i && g_hit_out) {
        int di;
        int ti;

        col_add(g_list, "Kind", width / 5);
        col_add(g_list, "Region", width / 5);
        col_add(g_list, "Name", width / 2);
        g_nhit = 0;
        for (di = 0; di < g_ndun; di++) {
            wsprintfA(b, "%d", g_duns[di].region);
            if (g_q[0] && !has_text(g_duns[di].name, g_q) && !has_text(b, g_q)) {
                continue;
            }
            g_hit_out[g_nhit] = 0;
            g_hit_i[g_nhit] = di;
            row_text(g_list, g_nhit, 0, g_duns[di].name);
            row_text(g_list, g_nhit, 1, b);
            row_text(g_list, g_nhit, 2, "Dungeon");
            g_nhit++;
        }
        for (ti = 0; ti < g_ntown; ti++) {
            wsprintfA(b, "%d", g_towns[ti].region);
            if (g_q[0] && !has_text(g_towns[ti].name, g_q) && !has_text(b, g_q)) {
                continue;
            }
            g_hit_out[g_nhit] = 1;
            g_hit_i[g_nhit] = ti;
            row_text(g_list, g_nhit, 0, g_towns[ti].name);
            row_text(g_list, g_nhit, 1, b);
            row_text(g_list, g_nhit, 2, "Town");
            g_nhit++;
        }
        shown = g_nhit;
    }
    g_filling = 0;
    if (shown > 0) {
        select_row(g_list, 0);
        on_row(0);
    } else if (g_view == VIEW_MAP) {
        show_pixel(g_px, g_py);
    } else {
        const char *title;

        title = "Data";
        if (g_view == VIEW_ARCHIVES || g_view == VIEW_RECS) {
            title = "Pictures";
        } else if (g_view == VIEW_RACES || g_view == VIEW_PARTS) {
            title = "Characters";
        } else if (g_view == VIEW_ENEMIES || g_view == VIEW_POSES) {
            title = "Creatures";
        } else if (g_view == VIEW_WEAPONS || g_view == VIEW_METALS) {
            title = "Items";
        } else if (g_view == VIEW_PLACES) {
            title = "Locations";
        } else if (g_view == VIEW_ROWS || g_view == VIEW_CHUNKS) {
            title = file_title(g_nd == ND_MERGED ? ND_MERGED_FILE : g_a);
        }
        props_clear();
        status_set(title, "");
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
    } else if (nd == ND_PICTURES) {
        g_view = VIEW_ARCHIVES;
    } else if (nd == ND_ARCHIVE) {
        g_view = VIEW_RECS;
    } else if (nd == ND_CHARS) {
        drop_paper();
        g_view = VIEW_RACES;
    } else if (nd == ND_RACE) {
        g_view = VIEW_PARTS;
    } else if (nd == ND_BEASTS) {
        drop_arc();
        drop_paper();
        g_nposes = 0;
        g_view = VIEW_ENEMIES;
    } else if (nd == ND_ENEMY) {
        DatFile *mf;

        mf = g_books ? books_merged(g_books) : 0;
        g_nposes = beast_poses(mf, a, g_poses, BEAST_POSES);
        g_view = VIEW_POSES;
    } else if (nd == ND_ITEMS) {
        drop_paper();
        drop_arc();
        g_view = VIEW_WEAPONS;
    } else if (nd == ND_WEAPON) {
        g_view = VIEW_METALS;
    } else if (nd == ND_WORLD || nd == ND_MAP) {
        g_view = VIEW_MAP;
        if (!g_map_ready) {
            map_rebuild();
        }
        if (g_focus_on) {
            map_centre(g_px, g_py);
            g_focus_on = 0;
            g_map_opened = 1;
        } else if (!g_map_opened) {
            g_px = 207;
            g_py = 213;
            g_zoom = 1;
            map_centre(g_px, g_py);
            g_map_opened = 1;
        }
        SetFocus(g_hwnd);
    } else if (nd == ND_PLACES) {
        g_place_ok = 0;
        g_view = VIEW_PLACES;
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
    {
        void *pics;
        int i;

        pics = tree_add(0, "Pictures", ND_PICTURES, 0, 0);
        for (i = 0; i < g_narc; i++) {
            char line[24];

            wsprintfA(line, "TEXTURE.%03d", g_arcs[i].file);
            tree_add(pics, line, ND_ARCHIVE, g_arcs[i].file, 0);
        }
    }
    {
        void *chars;
        DatFile *f;
        int kind;
        int rows;
        int row;

        chars = tree_add(0, "Characters", ND_CHARS, 0, 0);
        f = g_books ? books_merged(g_books) : 0;
        kind = f ? dat_find_tag(f, "RACE") : -1;
        rows = kind >= 0 ? dat_rows(f, kind) : 0;
        for (row = 0; row < rows; row++) {
            char id[32];
            char label[64];

            dat_row_label(f, kind, row, id, (int)sizeof(id), label, (int)sizeof(label));
            tree_add(chars, label[0] ? label : id, ND_RACE, row, kind);
        }
        SendMessageA(g_tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)chars);
    }
    {
        void *beasts;
        DatFile *f;
        int kind;
        int rows;
        int row;

        beasts = tree_add(0, "Creatures", ND_BEASTS, 0, 0);
        f = g_books ? books_merged(g_books) : 0;
        kind = f ? dat_find_tag(f, "ENEM") : -1;
        rows = kind >= 0 ? dat_rows(f, kind) : 0;
        for (row = 0; row < rows; row++) {
            char id[32];
            char label[64];

            dat_row_label(f, kind, row, id, (int)sizeof(id), label, (int)sizeof(label));
            tree_add(beasts, label[0] ? label : id, ND_ENEMY, row, kind);
        }
    }
    {
        void *items;
        DatFile *f;
        int i;
        int id;
        char name[32];

        items = tree_add(0, "Items", ND_ITEMS, 0, 0);
        f = g_books ? books_merged(g_books) : 0;
        for (i = 0; i < 64; i++) {
            if (!made_weapon_at(f, i, &id, name, (int)sizeof(name))) {
                break;
            }
            tree_add(items, name[0] ? name : "weapon", ND_WEAPON, id, 0);
        }
        SendMessageA(g_tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)items);
    }
    {
        void *world;

        world = tree_add(0, "World", ND_WORLD, 0, 0);
        g_map_node = tree_add(world, "Map", ND_MAP, 0, 0);
        tree_add(world, "Locations", ND_PLACES, 0, 0);
        SendMessageA(g_tree, TVM_EXPAND, TVE_EXPAND, (LPARAM)world);
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
    body = h - 52 - top;
    if (body < 40) {
        body = 40;
    }
    well_h = body * 2 / 5;
    if (well_h < 48) {
        well_h = 48;
    }
    MoveWindow(g_tree, 4, top, tree_w - 4, h - 52 - top, 1);
    if (g_view == VIEW_MAP) {
        ShowWindow(g_find_label, SW_HIDE);
        ShowWindow(g_find, SW_HIDE);
        ShowWindow(g_list, SW_HIDE);
        g_well.left = list_x;
        g_well.top = top;
        g_well.right = right_x - 6;
        g_well.bottom = top + body;
        if (g_well.right < g_well.left + 32) {
            g_well.right = g_well.left + 32;
        }
        MoveWindow(g_props, right_x, top, right_w, body, 1);
    } else {
        ShowWindow(g_find_label, SW_SHOW);
        ShowWindow(g_find, SW_SHOW);
        ShowWindow(g_list, SW_SHOW);
        MoveWindow(g_find_label, list_x, top, 32, 18, 1);
        MoveWindow(g_find, list_x + 34, top, list_w - 34, 20, 1);
        MoveWindow(g_list, list_x, top + 24, list_w, h - 52 - top - 24, 1);
        g_well.left = right_x;
        g_well.top = top;
        g_well.right = right_x + right_w;
        g_well.bottom = top + well_h;
        MoveWindow(g_props, right_x, top + well_h + 4, right_w, h - 52 - (top + well_h + 4), 1);
    }
    if (g_btn_climate) {
        MoveWindow(g_btn_climate, list_x, h - 48, 100, 18, 1);
    }
    if (g_btn_dots) {
        MoveWindow(g_btn_dots, list_x + 104, h - 48, 110, 18, 1);
    }
    if (g_btn_centre) {
        MoveWindow(g_btn_centre, list_x + 218, h - 48, 100, 18, 1);
    }
    InvalidateRect(g_hwnd, 0, 1);
}

static void blit_dib(HDC dc, int dx, int dy, int dw, int dh, const unsigned char *dib,
                     int full_w, int full_h, int sx, int sy, int sw, int sh)
{
    BITMAPINFOHEADER info;

    if (!dib || dw < 1 || dh < 1 || sw < 1 || sh < 1 || !g_gdi.StretchDIBits) {
        return;
    }
    memset(&info, 0, sizeof(info));
    info.biSize = sizeof(info);
    info.biWidth = full_w;
    /* Top-down, so row 0 of dib is the top of the picture. */
    info.biHeight = -full_h;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    g_gdi.SetStretchBltMode(dc, 3);
    g_gdi.StretchDIBits(dc, dx, dy, dw, dh, sx, sy, sw, sh, dib, (const BITMAPINFO *)&info, 0, SRCCOPY);
}

static void paint_large(HDC dc, const RECT *box)
{
    int w;
    int h;
    int stride;
    int y;
    int dw;
    int dh;
    int s;
    int s2;
    int dx;
    int dy;
    const df_u8 *pix;
    BITMAPINFOHEADER info;

    g_large = *box;
    if (!g_show || !g_show->index || g_show->width < 1 || g_show->height < 1) {
        DrawTextA(dc, "No picture", -1, &g_large, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    w = g_show->width;
    h = g_show->height;
    if (w > 512) {
        w = 512;
    }
    if (h > 512) {
        h = 512;
    }
    stride = (w * 3 + 3) & ~3;
    if (!g_dib) {
        /* Wide enough for a 512-wide picture, whatever this frame's width is. */
        g_dib = (unsigned char *)mem_keep(((512 * 3 + 3) & ~3) * 512u);
    }
    if (!g_dib) {
        return;
    }
    pix = g_show->index;
    for (y = 0; y < h; y++) {
        unsigned char *row;
        int x;
        int sy;

        sy = y * g_show->height / h;
        row = g_dib + y * stride;
        for (x = 0; x < w; x++) {
            int sx;

            sx = x * g_show->width / w;
            write_ix(row + x * 3, pix[sy * g_show->width + sx], x, y);
        }
    }
    dw = box->right - box->left;
    dh = box->bottom - box->top;
    if (dw < 1) {
        dw = 1;
    }
    if (dh < 1) {
        dh = 1;
    }
    s = dw / w;
    s2 = dh / h;
    if (s2 < s) {
        s = s2;
    }
    if (s < 1) {
        s = 1;
        if (dw * h / w > dh) {
            dw = dh * w / h;
            if (dw < 1) {
                dw = 1;
            }
        } else {
            dh = dw * h / w;
            if (dh < 1) {
                dh = 1;
            }
        }
    } else {
        if (s > 4) {
            s = 4;
        }
        dw = w * s;
        dh = h * s;
    }
    dx = box->left + (box->right - box->left - dw) / 2;
    dy = box->top + (box->bottom - box->top - dh) / 2;
    memset(&info, 0, sizeof(info));
    info.biSize = sizeof(info);
    info.biWidth = w;
    info.biHeight = -h;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    g_gdi.SetStretchBltMode(dc, 3);
    g_gdi.StretchDIBits(dc, dx, dy, dw, dh, 0, 0, w, h, g_dib, (const BITMAPINFO *)&info, 0, SRCCOPY);
}

static void paint_pics(HDC dc)
{
    RECT grid;
    RECT large;
    int view;
    int vis;
    int dx;
    RECT mark;
    int col;
    int row;

    view = g_well.bottom - g_well.top;
    grid = g_well;
    large = g_well;
    grid.bottom = g_well.top + view / 2;
    large.top = grid.bottom + 2;
    g_large = large;
    if (g_grid && g_grid_h > 0 && g_grid_w > 0) {
        vis = grid.bottom - grid.top;
        if (vis > g_grid_h - g_scroll) {
            vis = g_grid_h - g_scroll;
        }
        if (vis < 1) {
            vis = 1;
        }
        dx = grid.left;
        if (grid.right - grid.left > g_grid_w) {
            dx += (grid.right - grid.left - g_grid_w) / 2;
        }
        blit_dib(dc, dx, grid.top, g_grid_w, vis, g_grid, g_grid_w, g_grid_h, 0, g_scroll, g_grid_w, vis);
        if (g_rec >= 0 && g_rec < g_thumb_n) {
            col = g_rec % GRID_COLS;
            row = g_rec / GRID_COLS;
            mark.left = dx + col * GRID_CELL;
            mark.top = grid.top + row * GRID_CELL - g_scroll;
            mark.right = mark.left + GRID_CELL;
            mark.bottom = mark.top + GRID_CELL;
            if (mark.bottom > grid.top && mark.top < grid.bottom) {
                FrameRect(dc, &mark, (HBRUSH)g_gdi.GetStockObject(BLACK_BRUSH));
            }
        }
    }
    paint_large(dc, &large);
}

static int head_cell(void)
{
    int n;
    int gap;
    int cell;

    n = g_cif_on ? g_cif.n : 0;
    if (n < 1) {
        return 40;
    }
    gap = g_well.right - g_well.left;
    cell = 40;
    if (gap > 0 && n * cell > gap) {
        cell = gap / n;
    }
    if (cell < 8) {
        cell = 8;
    }
    return cell;
}

static void blit_index(HDC dc, int dx, int dy, int dw, int dh, const df_u8 *pix, int sw, int sh)
{
    int w;
    int h;
    int stride;
    int y;
    BITMAPINFOHEADER info;

    if (!pix || sw < 1 || sh < 1 || dw < 1 || dh < 1 || !g_gdi.StretchDIBits) {
        return;
    }
    w = sw;
    h = sh;
    if (w > 128) {
        w = 128;
    }
    if (h > 128) {
        h = 128;
    }
    stride = (w * 3 + 3) & ~3;
    if (!g_dib) {
        g_dib = (unsigned char *)mem_keep(((512 * 3 + 3) & ~3) * 512u);
    }
    if (!g_dib) {
        return;
    }
    for (y = 0; y < h; y++) {
        unsigned char *row;
        int x;
        int sy;

        sy = y * sh / h;
        row = g_dib + y * stride;
        for (x = 0; x < w; x++) {
            int sx;

            sx = x * sw / w;
            write_ix(row + x * 3, pix[sy * sw + sx], x, y);
        }
    }
    memset(&info, 0, sizeof(info));
    info.biSize = sizeof(info);
    info.biWidth = w;
    info.biHeight = -h;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    g_gdi.SetStretchBltMode(dc, 3);
    g_gdi.StretchDIBits(dc, dx, dy, dw, dh, 0, 0, w, h, g_dib, (const BITMAPINFO *)&info, 0, SRCCOPY);
}

static void paint_race(HDC dc)
{
    RECT large;
    int n;
    int i;
    int cell;
    int x;

    if (!g_cif_on) {
        paint_large(dc, &g_well);
        return;
    }
    n = g_cif.n;
    cell = head_cell();
    x = g_well.left;
    for (i = 0; i < n; i++) {
        const CifFrame *fr;
        RECT mark;

        if (x + cell > g_well.right) {
            break;
        }
        fr = cif_frame(&g_cif, i);
        if (fr && fr->pixels) {
            blit_index(dc, x + 1, g_well.top + 1, cell - 2, 42, fr->pixels, fr->w, fr->h);
        }
        if (i == g_head) {
            mark.left = x;
            mark.top = g_well.top;
            mark.right = x + cell;
            mark.bottom = g_well.top + 44;
            FrameRect(dc, &mark, (HBRUSH)g_gdi.GetStockObject(BLACK_BRUSH));
        }
        x += cell;
    }
    large = g_well;
    large.top = g_well.top + 48;
    if (large.top >= large.bottom) {
        large.top = g_well.top;
    }
    paint_large(dc, &large);
}

static void paint_map(HDC dc)
{
    int sw;
    int sh;
    RECT mark;

    if (!g_mapdib || !g_map_ready) {
        DrawTextA(dc, "No map", -1, &g_well, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    map_clamp();
    map_span(&sw, &sh);
    blit_dib(dc, g_well.left, g_well.top, sw * g_zoom, sh * g_zoom, g_mapdib,
             WORLDMAP_W, WORLDMAP_H, g_ox, g_oy, sw, sh);
    if (g_px >= g_ox && g_py >= g_oy && g_px < g_ox + sw && g_py < g_oy + sh) {
        mark.left = g_well.left + (g_px - g_ox) * g_zoom;
        mark.top = g_well.top + (g_py - g_oy) * g_zoom;
        mark.right = mark.left + g_zoom;
        mark.bottom = mark.top + g_zoom;
        if (mark.right < mark.left + 3) {
            mark.right = mark.left + 3;
        }
        if (mark.bottom < mark.top + 3) {
            mark.bottom = mark.top + 3;
        }
        FrameRect(dc, &mark, (HBRUSH)g_gdi.GetStockObject(WHITE_BRUSH));
    }
}

static void paint_blocks(HDC dc)
{
    int i;
    int minx;
    int maxx;
    int minz;
    int maxz;
    int cols;
    int rows;
    int cw;
    int ch;

    if (!g_place_ok || g_place.nblocks < 1) {
        DrawTextA(dc, "No blocks", -1, &g_well, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    minx = g_place.block[0].x;
    maxx = minx;
    minz = g_place.block[0].z;
    maxz = minz;
    for (i = 1; i < g_place.nblocks; i++) {
        if (g_place.block[i].x < minx) {
            minx = g_place.block[i].x;
        }
        if (g_place.block[i].x > maxx) {
            maxx = g_place.block[i].x;
        }
        if (g_place.block[i].z < minz) {
            minz = g_place.block[i].z;
        }
        if (g_place.block[i].z > maxz) {
            maxz = g_place.block[i].z;
        }
    }
    cols = maxx - minx + 1;
    rows = maxz - minz + 1;
    if (cols < 1) {
        cols = 1;
    }
    if (rows < 1) {
        rows = 1;
    }
    cw = (g_well.right - g_well.left) / cols;
    ch = (g_well.bottom - g_well.top) / rows;
    if (cw < 4) {
        cw = 4;
    }
    if (ch < 4) {
        ch = 4;
    }
    for (i = 0; i < g_place.nblocks; i++) {
        RECT cell;

        cell.left = g_well.left + (g_place.block[i].x - minx) * cw;
        cell.top = g_well.top + (g_place.block[i].z - minz) * ch;
        cell.right = cell.left + cw - 1;
        cell.bottom = cell.top + ch - 1;
        DrawTextA(dc, g_place.block[i].name, -1, &cell, DT_CENTER | DT_WORDBREAK | 0x00000800);
    }
}

static void paint_well(HDC dc)
{
    RECT well;

    if (g_well.right - g_well.left < 8 || g_well.bottom - g_well.top < 8) {
        return;
    }
    well = g_well;
    DrawEdge(dc, &well, EDGE_SUNKEN, BF_RECT);
    well.left += 2;
    well.top += 2;
    well.right -= 2;
    well.bottom -= 2;
    if ((g_view == VIEW_RECS || g_view == VIEW_POSES) && g_grid) {
        paint_pics(dc);
        return;
    }
    if ((g_view == VIEW_PARTS || g_view == VIEW_METALS) && g_show) {
        paint_race(dc);
        return;
    }
    if (g_view == VIEW_MAP) {
        paint_map(dc);
        return;
    }
    if (g_view == VIEW_PLACES) {
        paint_blocks(dc);
        return;
    }
    DrawTextA(dc, "No picture", -1, &well, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void well_click(int x, int y)
{
    int origin;
    int view;
    int col;
    int row;
    int rec;
    int i;

    if (g_view == VIEW_MAP && g_zoom > 0) {
        int px;
        int py;

        if (x < g_well.left || y < g_well.top || x >= g_well.right || y >= g_well.bottom) {
            return;
        }
        px = g_ox + (x - g_well.left) / g_zoom;
        py = g_oy + (y - g_well.top) / g_zoom;
        show_pixel(px, py);
        InvalidateRect(g_hwnd, &g_well, 0);
        return;
    }
    if ((g_view == VIEW_PARTS || g_view == VIEW_METALS) && g_cif_on) {
        int cell;
        int i;

        cell = head_cell();
        if (cell < 1 || y < g_well.top || y >= g_well.top + 48 || x < g_well.left) {
            return;
        }
        i = (x - g_well.left) / cell;
        if (i < 0 || i >= g_cif.n) {
            return;
        }
        g_head = i;
        show_head();
        if (g_view == VIEW_METALS) {
            weapon_status();
        } else {
            race_status();
        }
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    if ((g_view != VIEW_RECS && g_view != VIEW_POSES) || !g_grid_w) {
        return;
    }
    view = (g_well.bottom - g_well.top) / 2;
    if (y < g_well.top || y >= g_well.top + view) {
        return;
    }
    origin = g_well.left;
    if (g_well.right - g_well.left > g_grid_w) {
        origin += (g_well.right - g_well.left - g_grid_w) / 2;
    }
    if (x < origin || x >= origin + g_grid_w) {
        return;
    }
    col = (x - origin) / GRID_CELL;
    row = (y - g_well.top + g_scroll) / GRID_CELL;
    rec = row * GRID_COLS + col;
    if (rec < 0 || rec >= g_thumb_n) {
        return;
    }
    if (g_view == VIEW_POSES) {
        g_rec = rec;
        g_frame = 0;
        show_frame();
        scroll_to_rec();
        pic_status();
        InvalidateRect(g_hwnd, &g_well, 1);
        return;
    }
    for (i = 0; i < g_nmap; i++) {
        if (g_map[i] == rec) {
            select_row(g_list, i);
            return;
        }
    }
    g_rec = rec;
    g_frame = 0;
    show_frame();
    scroll_to_rec();
    pic_status();
    InvalidateRect(g_hwnd, &g_well, 1);
}

static LRESULT CALLBACK list_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN && (wp == VK_LEFT || wp == VK_RIGHT)) {
        if ((g_view == VIEW_PARTS || g_view == VIEW_METALS) && g_cif_on) {
            step_head(wp == VK_LEFT ? -1 : 1);
        } else {
            step_frame(wp == VK_LEFT ? -1 : 1);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        PostQuitMessage(0);
        return 0;
    }
    return CallWindowProcA(g_list_proc, hwnd, msg, wp, lp);
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
        } else if (HIWORD(wp) == 0 && (int)LOWORD(wp) == ID_CLIMATE) {
            g_climate_on = SendMessageA(g_btn_climate, 0x00F0, 0, 0) == 1;
            map_rebuild();
            InvalidateRect(hwnd, &g_well, 0);
        } else if (HIWORD(wp) == 0 && (int)LOWORD(wp) == ID_DOTS) {
            g_dots_on = SendMessageA(g_btn_dots, 0x00F0, 0, 0) == 1;
            map_rebuild();
            InvalidateRect(hwnd, &g_well, 0);
        } else if (HIWORD(wp) == 0 && (int)LOWORD(wp) == ID_CENTRE && g_px >= 0) {
            g_focus_on = 1;
            if (g_map_node) {
                SendMessageA(g_tree, TVM_SELECTITEM, TVGN_CARET, (LPARAM)g_map_node);
            } else {
                map_centre(g_px, g_py);
                InvalidateRect(hwnd, &g_well, 0);
            }
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
    case WM_LBUTTONDOWN:
        well_click((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            PostQuitMessage(0);
        } else if (g_view == VIEW_MAP && (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN
                                          || wp == '1' || wp == '2' || wp == '4')) {
            if (wp == VK_LEFT) {
                g_ox -= 32;
            } else if (wp == VK_RIGHT) {
                g_ox += 32;
            } else if (wp == VK_UP) {
                g_oy -= 32;
            } else if (wp == VK_DOWN) {
                g_oy += 32;
            } else if (wp == '1') {
                g_zoom = 1;
            } else if (wp == '2') {
                g_zoom = 2;
            } else if (wp == '4') {
                g_zoom = 4;
            }
            map_clamp();
            InvalidateRect(hwnd, &g_well, 0);
        } else if (wp == VK_LEFT || wp == VK_RIGHT) {
            if ((g_view == VIEW_PARTS || g_view == VIEW_METALS) && g_cif_on) {
                step_head(wp == VK_LEFT ? -1 : 1);
            } else {
                step_frame(wp == VK_LEFT ? -1 : 1);
            }
        }
        return 0;
    case WM_TIMER:
        if ((g_view == VIEW_PARTS || g_view == VIEW_METALS) && g_cif_on) {
            step_head(1);
        } else {
            step_frame(1);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
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
    {
        const df_u8 *bytes;
        df_u32 n;

        bytes = vfs_read("ART_PAL.COL", &n);
        g_pal_ok = palette_load(bytes, n, &g_pal);
        texture_cache_limit(2u * 1024u * 1024u);
        g_narc = tex_list(g_arcs, TEX_SCAN);
        g_arc_file = -1;
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
    g_btn_climate = CreateWindowExA(
        0, "BUTTON", "Climate",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | 0x0003,
        0, 0, 10, 10, hwnd, (HMENU)(UINT_PTR)ID_CLIMATE, inst, 0);
    g_btn_dots = CreateWindowExA(
        0, "BUTTON", "Locations",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | 0x0003,
        0, 0, 10, 10, hwnd, (HMENU)(UINT_PTR)ID_DOTS, inst, 0);
    g_btn_centre = CreateWindowExA(
        0, "BUTTON", "Centre",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, hwnd, (HMENU)(UINT_PTR)ID_CENTRE, inst, 0);
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
    font_set(g_btn_climate);
    font_set(g_btn_dots);
    font_set(g_btn_centre);
    cols_clear(g_props);
    /* col_add inserts at column 0, so the last call is the left column. */
    col_add(g_props, "Value", 160);
    col_add(g_props, "Field", 110);
    g_list_proc = (WNDPROC)SetWindowLongA(g_list, GWL_WNDPROC, (LONG)list_proc);
    g_zoom = 1;
    g_px = 207;
    g_py = 213;
    load_places();
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    layout();
    build_tree();
    SetTimer(hwnd, ID_TIMER, 160, 0);
    SetFocus(g_list);
    while (GetMessageA(&msg, 0, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    drop_paper();
    drop_arc();
    texture_cache_clear();
    books_close(g_books);
    g_books = 0;
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)win_main(GetModuleHandleA(0)));
}
