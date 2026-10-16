/*
 * levelfall: list every indoor and outdoor place and fly through one.
 * Indoors are MAPDITEM records (RDB blocks). Outdoors are MAPPITEM
 * locations (RMB blocks). The index is built once at startup.
 * Type a name to filter. Esc clears the find box, then quits.
 * Esc in the view returns to the list. The title-bar cross quits.
 * W/S and Up/Down fly along the aim. A/D strafe. Left/Right turn.
 * Page Up and Page Down pitch, staying in place.
 * The wheel changes the field of view.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/platform/input.h"
#include "engine/platform/win32.h"
#include "engine/ui/console.h"
#include "engine/ui/font.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/texture.h"
#include "game/dagger/world.h"

#define MAX_LEVELS 24576
#define LOOK_SENS 0.003f
/* Game arrow look is 8 pixels per frame. At 60 Hz that is this many radians. */
#define TURN_RATE 1.44f
#define PITCH_LIMIT 1.55334303f
/* One wheel notch. Forward narrows the view. camera_set_fov clamps 45..120. */
#define FOV_STEP 5.0f
/* 12 units per vsync at 60 Hz, same pace as the game's fly. Shift is 4x. */
#define FLY_SPEED 720.0f

#define ID_LIST 1
#define ID_OPEN 2
#define ID_FIND 3
#define ID_KIND 4

#define LVM_FIRST 0x1000
#define LVM_GETITEMA (LVM_FIRST + 5)
#define LVM_SETITEMA (LVM_FIRST + 6)
#define LVM_INSERTITEMA (LVM_FIRST + 7)
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#define LVM_GETNEXTITEM (LVM_FIRST + 12)
#define LVM_ENSUREVISIBLE (LVM_FIRST + 19)
#define LVM_INSERTCOLUMNA (LVM_FIRST + 27)
#define LVM_SETCOLUMNA (LVM_FIRST + 26)
#define LVM_GETHEADER (LVM_FIRST + 31)
#define LVM_SETITEMCOUNT (LVM_FIRST + 47)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVIF_TEXT 0x0001
#define LVIF_PARAM 0x0004
#define LVIF_STATE 0x0008
#define LVIS_FOCUSED 0x0001
#define LVIS_SELECTED 0x0002
#define LVCF_FMT 0x0001
#define LVCF_WIDTH 0x0002
#define LVCF_TEXT 0x0004
#define LVCF_SUBITEM 0x0008
#define LVCFMT_LEFT 0x0000
#define LVS_REPORT 0x0001
#define LVS_SINGLESEL 0x0004
#define LVS_SHOWSELALWAYS 0x0008
#define LVS_OWNERDATA 0x1000
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVNI_SELECTED 0x0002
#define NM_DBLCLK (-3)
#define LVN_COLUMNCLICK (-108)
#define LVN_GETDISPINFOA (-150)
#define EN_CHANGE 0x0300
#define CBN_SELCHANGE 1
#define CB_ADDSTRING 0x0143
#define CB_SETCURSEL 0x014E
#define CB_GETCURSEL 0x0147

static HANDLE g_log = INVALID_HANDLE_VALUE;
static HWND g_list;
static HWND g_label;
static HWND g_find;
static HWND g_find_cap;
static HWND g_kind;
static HWND g_kind_cap;
static HWND g_open;
static int g_sort_col;
static int g_sort_desc;
static int g_open_req;
static int g_kind_filter;
static DunRef *g_rows;
static int *g_view;
static int g_nrows;
static int g_nview;
static int g_nindoor;
static int g_noutdoor;
static char g_query[64];
static int g_help;
static int g_fog = 1;
/* Two outdoor blocks. One block is 4096. The engine caps this at DF_FAR_MAX. */
static int g_far = 8192;
static int g_bg_r = 128;
static int g_bg_g = 128;
static int g_bg_b = 128;
static char g_view_name[40];

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

typedef struct LfNotify {
    HWND hwndFrom;
    unsigned int idFrom;
    unsigned int code;
    int iItem;
    int iSubItem;
} LfNotify;

/* NMHDR then LVITEM. The list writes pszText into its own buffer. */
typedef struct LfDisp {
    HWND hwndFrom;
    unsigned int idFrom;
    unsigned int code;
    unsigned int mask;
    int iItem;
    int iSubItem;
    unsigned int state;
    unsigned int stateMask;
    char *pszText;
    int cchTextMax;
} LfDisp;

static int key_down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

static void add_str(char *dst, int *n, int cap, const char *s)
{
    int i;

    i = 0;
    while (s[i] && *n < cap - 1) {
        dst[*n] = s[i];
        *n += 1;
        i++;
    }
    dst[*n] = 0;
}

static void add_u(char *dst, int *n, int cap, df_u32 v)
{
    char tmp[16];
    int k;

    k = 0;
    do {
        tmp[k++] = (char)('0' + (int)(v % 10u));
        v /= 10u;
    } while (v && k < (int)sizeof(tmp));
    while (k > 0 && *n < cap - 1) {
        dst[*n] = tmp[--k];
        *n += 1;
    }
    dst[*n] = 0;
}

static void log_line(const char *s)
{
    DWORD wrote;
    int n;

    if (g_log == INVALID_HANDLE_VALUE || !s) {
        return;
    }
    n = 0;
    while (s[n]) {
        n++;
    }
    WriteFile(g_log, s, (DWORD)n, &wrote, 0);
    WriteFile(g_log, "\r\n", 2, &wrote, 0);
    FlushFileBuffers(g_log);
}

static void log_open(void)
{
    char exe[260];
    char path[260];
    DWORD len;
    int slash;
    int i;
    const char *name;
    int n;

    len = GetModuleFileNameA(0, exe, 260);
    if (len == 0 || len >= 259) {
        return;
    }
    exe[len] = 0;
    slash = -1;
    for (i = 0; exe[i]; i++) {
        if (exe[i] == '\\' || exe[i] == '/') {
            slash = i;
        }
    }
    if (slash < 1 || slash + 20 >= 260) {
        return;
    }
    for (i = 0; i < slash; i++) {
        path[i] = exe[i];
    }
    name = "\\levelfall.log";
    n = slash;
    for (i = 0; name[i] && n < 259; i++) {
        path[n++] = name[i];
    }
    path[n] = 0;
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
}

static int cmp_str(const char *a, const char *b)
{
    int i;

    i = 0;
    while (a[i] && a[i] == b[i]) {
        i++;
    }
    if (a[i] == b[i]) {
        return 0;
    }
    if ((unsigned char)a[i] < (unsigned char)b[i]) {
        return -1;
    }
    return 1;
}

static int fold_ch(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}

static int same_text(const char *a, const char *b)
{
    int i;

    i = 0;
    while (a[i] || b[i]) {
        if (fold_ch((unsigned char)a[i]) != fold_ch((unsigned char)b[i])) {
            return 0;
        }
        i++;
    }
    return 1;
}

static int has_text(const char *name, const char *q)
{
    int i;
    int j;

    if (!q[0]) {
        return 1;
    }
    for (i = 0; name[i]; i++) {
        for (j = 0; q[j]; j++) {
            if (!name[i + j]) {
                break;
            }
            if (fold_ch((unsigned char)name[i + j]) != fold_ch((unsigned char)q[j])) {
                break;
            }
        }
        if (!q[j]) {
            return 1;
        }
    }
    return 0;
}

static int row_visible(const DunRef *row)
{
    if (g_kind_filter == 1 && row->outdoor) {
        return 0;
    }
    if (g_kind_filter == 2 && !row->outdoor) {
        return 0;
    }
    return has_text(row->name, g_query);
}

static int row_before(const DunRef *a, const DunRef *b)
{
    int c;
    int less;

    c = 0;
    less = 0;
    if (g_sort_col == 0) {
        if (a->outdoor != b->outdoor) {
            less = a->outdoor < b->outdoor;
            c = 1;
        }
    } else if (g_sort_col == 1) {
        if (a->region != b->region) {
            less = a->region < b->region;
            c = 1;
        }
    } else if (g_sort_col == 3) {
        if (a->nblocks != b->nblocks) {
            less = a->nblocks < b->nblocks;
            c = 1;
        }
    } else if (cmp_str(a->name, b->name) != 0) {
        less = cmp_str(a->name, b->name) < 0;
        c = 1;
    }
    if (!c) {
        if (cmp_str(a->name, b->name) != 0) {
            return cmp_str(a->name, b->name) < 0;
        }
        if (a->outdoor != b->outdoor) {
            return a->outdoor < b->outdoor;
        }
        if (a->region != b->region) {
            return a->region < b->region;
        }
        return a->index < b->index;
    }
    if (g_sort_desc) {
        return !less;
    }
    return less;
}

static void sort_view(void)
{
    int gap;

    gap = g_nview / 2;
    while (gap > 0) {
        int i;

        for (i = gap; i < g_nview; i++) {
            int tmp;
            int j;

            tmp = g_view[i];
            j = i;
            while (j >= gap && row_before(&g_rows[tmp], &g_rows[g_view[j - gap]])) {
                g_view[j] = g_view[j - gap];
                j -= gap;
            }
            g_view[j] = tmp;
        }
        gap /= 2;
    }
}

static void set_column(int index, const char *text, int width, int insert)
{
    LvCol col;
    char buf[32];
    int i;

    i = 0;
    while (text[i] && i < 31) {
        buf[i] = text[i];
        i++;
    }
    buf[i] = 0;
    mem_zero(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_FMT | LVCF_SUBITEM;
    if (width > 0) {
        col.mask |= LVCF_WIDTH;
    }
    col.fmt = LVCFMT_LEFT;
    col.cx = width;
    col.pszText = buf;
    col.iSubItem = index;
    if (insert) {
        SendMessageA(g_list, LVM_INSERTCOLUMNA, (WPARAM)index, (LPARAM)&col);
    } else {
        SendMessageA(g_list, LVM_SETCOLUMNA, (WPARAM)index, (LPARAM)&col);
    }
}

static void sort_caption(void)
{
    const char *kind;
    const char *region;
    const char *name;
    const char *blocks;

    kind = "Kind";
    region = "Region";
    name = "Name";
    blocks = "Blocks";
    if (g_sort_col == 0) {
        kind = g_sort_desc ? "Kind  -" : "Kind  +";
    } else if (g_sort_col == 1) {
        region = g_sort_desc ? "Region  -" : "Region  +";
    } else if (g_sort_col == 2) {
        name = g_sort_desc ? "Name  -" : "Name  +";
    } else {
        blocks = g_sort_desc ? "Blocks  -" : "Blocks  +";
    }
    set_column(0, kind, 0, 0);
    set_column(1, region, 0, 0);
    set_column(2, name, 0, 0);
    set_column(3, blocks, 0, 0);
}

static int selected_view(void)
{
    int i;

    if (!g_list) {
        return -1;
    }
    i = (int)SendMessageA(g_list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (i < 0 || i >= g_nview) {
        return -1;
    }
    return i;
}

/* Index into g_rows, or -1. */
static int selected_row(void)
{
    int i;

    i = selected_view();
    if (i < 0) {
        return -1;
    }
    return g_view[i];
}

static void select_row(int index)
{
    LvItem item;

    mem_zero(&item, sizeof(item));
    item.mask = LVIF_STATE;
    item.iItem = index;
    item.state = LVIS_SELECTED | LVIS_FOCUSED;
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageA(g_list, LVM_SETITEMA, 0, (LPARAM)&item);
    SendMessageA(g_list, LVM_ENSUREVISIBLE, (WPARAM)index, 0);
}

static void put_region(char *dst, int region)
{
    dst[0] = (char)('0' + region / 100);
    dst[1] = (char)('0' + (region / 10) % 10);
    dst[2] = (char)('0' + region % 10);
    dst[3] = 0;
}

static void put_text(char *dst, int cap, const char *s)
{
    int i;

    if (!dst || cap < 2) {
        return;
    }
    for (i = 0; s[i] && i < cap - 1; i++) {
        dst[i] = s[i];
    }
    dst[i] = 0;
}

static void cell_text(const DunRef *row, int sub, char *dst, int cap)
{
    int n;

    dst[0] = 0;
    if (sub == 0) {
        put_text(dst, cap, row->outdoor ? "Outdoor" : "Indoor");
    } else if (sub == 1) {
        if (cap >= 4) {
            put_region(dst, row->region);
        }
    } else if (sub == 2) {
        put_text(dst, cap, row->name);
    } else {
        n = 0;
        add_u(dst, &n, cap, (df_u32)row->nblocks);
    }
}

static void write_status(void)
{
    char line[96];
    int n;

    if (!g_label) {
        return;
    }
    n = 0;
    line[0] = 0;
    if (g_query[0] || g_kind_filter != 0) {
        add_u(line, &n, (int)sizeof(line), (df_u32)g_nview);
        add_str(line, &n, (int)sizeof(line), " match");
        if (g_nview != 1) {
            add_str(line, &n, (int)sizeof(line), "es");
        }
        add_str(line, &n, (int)sizeof(line), "    Enter opens    Esc clears the find");
    } else {
        add_u(line, &n, (int)sizeof(line), (df_u32)g_nindoor);
        add_str(line, &n, (int)sizeof(line), " indoors, ");
        add_u(line, &n, (int)sizeof(line), (df_u32)g_noutdoor);
        add_str(line, &n, (int)sizeof(line), " outdoors    type a name, Enter opens, Esc quits");
    }
    SetWindowTextA(g_label, line);
}

static void refill_list(void)
{
    int keep;
    int have_keep;
    int i;
    int sel;
    int exact;

    if (!g_list || !g_rows || !g_view) {
        return;
    }
    keep = selected_row();
    have_keep = keep >= 0;
    g_nview = 0;
    for (i = 0; i < g_nrows; i++) {
        if (row_visible(&g_rows[i])) {
            g_view[g_nview++] = i;
        }
    }
    sort_view();
    sort_caption();
    exact = -1;
    sel = 0;
    if (g_query[0]) {
        for (i = 0; i < g_nview; i++) {
            if (same_text(g_rows[g_view[i]].name, g_query)) {
                exact = i;
                break;
            }
        }
    }
    if (exact >= 0) {
        sel = exact;
    } else if (have_keep) {
        for (i = 0; i < g_nview; i++) {
            if (g_view[i] == keep) {
                sel = i;
                break;
            }
        }
    }
    SendMessageA(g_list, LVM_SETITEMCOUNT, (WPARAM)g_nview, 0);
    if (g_nview > 0) {
        select_row(sel);
    }
    write_status();
}

static void sort_flip(int col)
{
    if (col < 0 || col > 3) {
        col = 2;
    }
    if (g_sort_col == col) {
        g_sort_desc = g_sort_desc ? 0 : 1;
    } else {
        g_sort_col = col;
        g_sort_desc = 0;
    }
    refill_list();
}

static int common_controls(void)
{
    HMODULE dll;
    void (WINAPI *init)(void);

    dll = GetModuleHandleA("comctl32.dll");
    if (!dll) {
        dll = LoadLibraryA("comctl32.dll");
    }
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

static void apply_font(HWND hwnd)
{
    void *font;
    HWND header;

    font = win_ui_font();
    if (!font || !hwnd) {
        return;
    }
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)font, 1);
    header = (HWND)SendMessageA(hwnd, LVM_GETHEADER, 0, 0);
    if (header) {
        SendMessageA(header, WM_SETFONT, (WPARAM)font, 1);
    }
}

static int make_list(void)
{
    RECT rc;
    HWND hwnd;
    HINSTANCE inst;
    int y;
    int width;

    hwnd = (HWND)win_hwnd();
    inst = GetModuleHandleA(0);
    win_ui_font();
    if (!common_controls()) {
        log_line("comctl32.dll failed");
        return 0;
    }
    if (!GetClientRect(hwnd, &rc)) {
        return 0;
    }
    g_find_cap = CreateWindowExA(
        0, "STATIC", "Find",
        WS_CHILD | WS_VISIBLE,
        8, 10, 32, 16,
        hwnd, 0, inst, 0);
    g_find = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        42, 6, 220, 22,
        hwnd, (HMENU)ID_FIND, inst, 0);
    g_kind_cap = CreateWindowExA(
        0, "STATIC", "Show",
        WS_CHILD | WS_VISIBLE,
        272, 10, 36, 16,
        hwnd, 0, inst, 0);
    g_kind = CreateWindowExA(
        0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        310, 6, 110, 160,
        hwnd, (HMENU)ID_KIND, inst, 0);
    g_open = CreateWindowExA(
        0, "BUTTON", "&Open",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        rc.right - 83, 6, 75, 23,
        hwnd, (HMENU)ID_OPEN, inst, 0);
    g_label = CreateWindowExA(
        0, "STATIC", "Indexing places...",
        WS_CHILD | WS_VISIBLE,
        8, 34, rc.right - 16, 16,
        hwnd, 0, inst, 0);
    y = 54;
    width = rc.right - 16;
    g_list = CreateWindowExA(
        WS_EX_CLIENTEDGE, "SysListView32", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL
            | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
        8, y, width, rc.bottom - y - 8,
        hwnd, (HMENU)ID_LIST, inst, 0);
    if (!g_find || !g_kind || !g_label || !g_list || !g_open) {
        return 0;
    }
    SendMessageA(g_kind, CB_ADDSTRING, 0, (LPARAM)"All");
    SendMessageA(g_kind, CB_ADDSTRING, 0, (LPARAM)"Indoors");
    SendMessageA(g_kind, CB_ADDSTRING, 0, (LPARAM)"Outdoors");
    SendMessageA(g_kind, CB_SETCURSEL, 0, 0);
    SendMessageA(g_find, EM_SETLIMITTEXT, 60, 0);
    SendMessageA(g_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    set_column(0, "Kind", 72, 1);
    set_column(1, "Region", 64, 1);
    set_column(2, "Name  +", width - 216, 1);
    set_column(3, "Blocks", 64, 1);
    apply_font(g_find_cap);
    apply_font(g_find);
    apply_font(g_kind_cap);
    apply_font(g_kind);
    apply_font(g_label);
    apply_font(g_list);
    apply_font(g_open);
    return 1;
}

static void show_list(int on)
{
    int cmd;

    cmd = on ? SW_SHOW : SW_HIDE;
    ShowWindow(g_find_cap, cmd);
    ShowWindow(g_find, cmd);
    ShowWindow(g_kind_cap, cmd);
    ShowWindow(g_kind, cmd);
    ShowWindow(g_label, cmd);
    ShowWindow(g_list, cmd);
    ShowWindow(g_open, cmd);
    if (on) {
        SetFocus(g_find);
    } else if (win_hwnd()) {
        SetFocus((HWND)win_hwnd());
    }
}

static void focus_step(int back)
{
    HWND order[4];
    HWND cur;
    int n;
    int i;
    int j;

    order[0] = g_find;
    order[1] = g_kind;
    order[2] = g_list;
    order[3] = g_open;
    n = 4;
    cur = GetFocus();
    j = 0;
    for (i = 0; i < n; i++) {
        if (order[i] == cur) {
            j = i;
        }
    }
    if (back) {
        j = (j + n - 1) % n;
    } else {
        j = (j + 1) % n;
    }
    SetFocus(order[j]);
}

static void read_query(void)
{
    if (!g_find) {
        g_query[0] = 0;
        return;
    }
    GetWindowTextA(g_find, g_query, (int)sizeof(g_query));
}

static void on_command(unsigned int id, unsigned int code)
{
    int kind;

    if (code == BN_CLICKED && id == ID_OPEN) {
        g_open_req = 1;
        return;
    }
    if (id == ID_FIND && code == EN_CHANGE) {
        read_query();
        refill_list();
        return;
    }
    if (id == ID_KIND && code == CBN_SELCHANGE) {
        kind = (int)SendMessageA(g_kind, CB_GETCURSEL, 0, 0);
        if (kind < 0) {
            kind = 0;
        }
        g_kind_filter = kind;
        refill_list();
    }
}

/*
 * Owner-data asks for cell text with LVN_GETDISPINFO. Paint passes a
 * real buffer. Selecting a row passes LPSTR_TEXTCALLBACK ((char *)-1)
 * and a garbage cchTextMax: writing through that pointer is the
 * illegal-operation crash. Hand back a stable string instead.
 */
static char g_cell[64];

static void on_disp(LfDisp *nm)
{
    char *dst;

    if (!nm || (nm->mask & LVIF_TEXT) == 0) {
        return;
    }
    if (nm->iItem < 0 || nm->iItem >= g_nview) {
        g_cell[0] = 0;
    } else {
        cell_text(&g_rows[g_view[nm->iItem]], nm->iSubItem, g_cell, (int)sizeof(g_cell));
    }
    dst = nm->pszText;
    if (dst && dst != (char *)-1 && nm->cchTextMax > 1 && nm->cchTextMax < 4096) {
        put_text(dst, nm->cchTextMax, g_cell);
        return;
    }
    nm->pszText = g_cell;
}

static void on_notify(void *hdr)
{
    LfNotify *nm;

    nm = (LfNotify *)hdr;
    if (!nm || nm->idFrom != ID_LIST) {
        return;
    }
    if ((int)nm->code == LVN_GETDISPINFOA) {
        on_disp((LfDisp *)hdr);
        return;
    }
    if ((int)nm->code == NM_DBLCLK) {
        g_open_req = 1;
        return;
    }
    if ((int)nm->code == LVN_COLUMNCLICK) {
        sort_flip(nm->iSubItem);
    }
}

static void nudge_sel(int dir)
{
    int i;

    if (g_nview < 1) {
        return;
    }
    i = selected_view();
    if (i < 0) {
        i = 0;
    } else {
        i += dir;
    }
    if (i < 0) {
        i = 0;
    }
    if (i >= g_nview) {
        i = g_nview - 1;
    }
    select_row(i);
}

static void list_wheel(int delta)
{
    int notches;
    int lines;
    int step;
    WPARAM code;

    if (delta == 0 || !g_list) {
        return;
    }
    notches = delta / 120;
    if (notches == 0) {
        notches = delta > 0 ? 1 : -1;
    }
    if (notches > 0) {
        code = SB_LINEUP;
        lines = notches * 3;
    } else {
        code = SB_LINEDOWN;
        lines = -notches * 3;
    }
    for (step = 0; step < lines; step++) {
        SendMessageA(g_list, WM_VSCROLL, code, 0);
    }
}

/* 1 = open the selection, 0 = quit the program. */
static int wait_list(void)
{
    int enter_was;
    int esc_was;

    input_clear_quit();
    enter_was = key_down(VK_RETURN);
    esc_was = key_down(VK_ESCAPE);
    g_open_req = 0;
    win_command_hook(on_command);
    win_notify_hook(on_notify);
    SetFocus(g_find);
    for (;;) {
        MSG msg;
        int enter_now;
        int esc_now;

        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT || msg.message == WM_CLOSE) {
                win_command_hook(0);
                win_notify_hook(0);
                return 0;
            }
            if (msg.message == WM_MOUSEWHEEL) {
                list_wheel((int)(short)HIWORD(msg.wParam));
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB) {
                focus_step((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0);
                continue;
            }
            if (msg.message == WM_KEYDOWN && GetFocus() == g_find
                && (msg.wParam == VK_DOWN || msg.wParam == VK_UP)) {
                nudge_sel(msg.wParam == VK_UP ? -1 : 1);
                continue;
            }
            if (msg.message == WM_SYSCHAR) {
                char ch;

                ch = (char)msg.wParam;
                if (ch == 'o' || ch == 'O') {
                    g_open_req = 1;
                }
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_open_req) {
            win_command_hook(0);
            win_notify_hook(0);
            return 1;
        }
        esc_now = key_down(VK_ESCAPE);
        if (esc_now && !esc_was) {
            if (g_query[0]) {
                SetWindowTextA(g_find, "");
            } else {
                win_command_hook(0);
                win_notify_hook(0);
                return 0;
            }
        }
        esc_was = esc_now;
        enter_now = key_down(VK_RETURN);
        if (enter_now && !enter_was && GetFocus() != g_kind) {
            win_command_hook(0);
            win_notify_hook(0);
            return 1;
        }
        enter_was = enter_now;
        Sleep(10);
    }
}

static void help_rect(float x, float y, float w, float h, float r, float g, float b)
{
    DrawVert v[4];
    int i;

    for (i = 0; i < 4; i++) {
        v[i].oow = 1.0f;
        v[i].ooz = 1.0f;
        v[i].sow = 0.0f;
        v[i].tow = 0.0f;
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
    }
    v[0].x = x;
    v[0].y = y + h;
    v[1].x = x + w;
    v[1].y = y + h;
    v[2].x = x + w;
    v[2].y = y;
    v[3].x = x;
    v[3].y = y;
    glide_triangle(&v[0], &v[1], &v[2]);
    glide_triangle(&v[0], &v[2], &v[3]);
}

static void draw_lines(const char **lines, int nlines, int x, int top)
{
    int i;
    int wide;
    int line_h;
    int block;
    int y;

    line_h = ui_line_height() + 2;
    wide = 0;
    for (i = 0; i < nlines; i++) {
        int w;

        w = ui_string_width(lines[i]);
        if (w > wide) {
            wide = w;
        }
    }
    block = nlines * line_h;
    if (x < 0) {
        x = DF_VIEW_W - wide - 16;
        if (x < 8) {
            x = 8;
        }
    }
    glide_plate(1);
    help_rect((float)x, (float)(top - block), (float)(wide + 16), (float)(block + 8),
              0.0f, 0.0f, 0.0f);
    glide_plate(0);
    y = top;
    for (i = 0; i < nlines; i++) {
        ui_string(x + 8, y, lines[i]);
        y -= line_h;
    }
}

static int take_wheel(void)
{
    MSG msg;
    int sum;

    sum = 0;
    while (PeekMessageA(&msg, 0, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE)) {
        sum += (int)(short)HIWORD(msg.wParam);
    }
    return sum;
}

static void draw_overlay(void)
{
    static const char *help[] = {
        "mouse: look",
        "w s up down: along the aim",
        "a d: strafe",
        "left right: turn",
        "pgup pgdn: look up and down",
        "wheel: field of view",
        "shift: faster",
        "f1: hide this",
        "esc: back to the list",
        "close: quit"
    };
    const char *lines[12];
    int n;
    int line_h;
    int i;

    n = 0;
    lines[n++] = g_view_name;
    if (g_help) {
        for (i = 0; i < (int)(sizeof(help) / sizeof(help[0])); i++) {
            lines[n++] = help[i];
        }
    }
    line_h = ui_line_height() + 2;
    draw_lines(lines, n, 8, 12 + n * line_h);
}

static void fly_step(Camera *cam, int dx, int dy, float dt)
{
    Basis basis;
    float mf;
    float mr;
    float speed;

    cam->yaw = df_wrap_tau(cam->yaw + (float)dx * LOOK_SENS
        + (float)(key_down(VK_RIGHT) - key_down(VK_LEFT)) * TURN_RATE * dt);
    /* Page Up pitches toward the sky. The camera stays where it is. */
    cam->pitch -= (float)dy * LOOK_SENS;
    cam->pitch += (float)(key_down(VK_PRIOR) - key_down(VK_NEXT)) * TURN_RATE * dt;
    if (cam->pitch > PITCH_LIMIT) {
        cam->pitch = PITCH_LIMIT;
    }
    if (cam->pitch < -PITCH_LIMIT) {
        cam->pitch = -PITCH_LIMIT;
    }
    camera_basis(cam, &basis);
    mf = (float)((key_down('W') || key_down(VK_UP))
        - (key_down('S') || key_down(VK_DOWN)));
    mr = (float)(key_down('D') - key_down('A'));
    speed = FLY_SPEED * dt;
    if (key_down(VK_SHIFT)) {
        speed *= 4.0f;
    }
    /* W and Up follow the aim, including pitch. */
    cam->x += (basis.fx * basis.cp * mf + basis.rx * mr) * speed;
    cam->y += basis.sp * mf * speed;
    cam->z += (basis.fz * basis.cp * mf + basis.rz * mr) * speed;
}

static void run_view(const Level *level, Camera *cam)
{
    int help_was;
    DWORD tick;

    help_was = key_down(VK_F1);
    g_help = 1;
    tick = GetTickCount();
    input_clear_quit();
    input_begin(win_hwnd());
    gfx_set_extra(draw_overlay);
    for (;;) {
        DWORD now;
        DWORD dt;
        int help_now;
        int dx;
        int dy;
        int wheel;

        wheel = take_wheel();
        if (wheel && !console_open()) {
            int notches;

            notches = wheel / 120;
            if (notches == 0) {
                notches = wheel > 0 ? 1 : -1;
            }
            camera_set_fov(camera_fov() - (float)notches * FOV_STEP);
        }
        win_pump();
        if (input_quit_requested()) {
            break;
        }
        now = GetTickCount();
        dt = now - tick;
        tick = now;
        if (dt > 100) {
            dt = 100;
        }
        input_look(&dx, &dy);
        if (!console_open()) {
            help_now = key_down(VK_F1);
            if (help_now && !help_was) {
                g_help = g_help ? 0 : 1;
            }
            help_was = help_now;
            fly_step(cam, dx, dy, (float)dt / 1000.0f);
        }
        gfx_frame(cam, level);
    }
    gfx_set_extra(0);
    input_end();
}

static void view_row(int index)
{
    Dungeon dun;
    World *world;
    Level level;
    Camera cam;
    int glide_up;
    char line[80];
    int n;

    if (index < 0 || index >= g_nrows) {
        return;
    }
    df_copy(g_view_name, g_rows[index].name, (int)sizeof(g_view_name));
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "open ");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_rows[index].region);
    add_str(line, &n, (int)sizeof(line), " ");
    add_str(line, &n, (int)sizeof(line), g_view_name);
    log_line(line);
    if (!mem_temp_begin()) {
        log_line("temp heap failed");
        win_fail("Out of memory.");
        return;
    }
    glide_up = 0;
    world = 0;
    if (g_rows[index].outdoor) {
        n = exterior_load_at(g_rows[index].region, g_rows[index].index, &dun);
    } else {
        n = dungeon_load_at(g_rows[index].region, g_rows[index].index, &dun);
    }
    if (!n) {
        log_line(dungeon_error());
        win_fail(dungeon_error());
    } else {
        world = world_load_dungeon(&dun);
        if (!world) {
            log_line(world_error());
            win_fail(world_error());
        }
    }
    if (world) {
        float x, y, z, yaw, pitch;

        world_as_level(world, &level);
        show_list(0);
        if (!gfx_startup(win_hwnd())) {
            log_line(gfx_error());
            win_fail(gfx_error());
        } else {
            glide_up = 1;
            if (!gfx_prepare(&level)) {
                log_line(gfx_error());
                win_fail(gfx_error());
            } else {
                world_start(world, &x, &y, &z, &yaw, &pitch);
                camera_set(&cam, x, y, z, yaw, pitch);
                log_line("glide open 640x480");
                run_view(&level, &cam);
            }
        }
    }
    if (glide_up) {
        gfx_shutdown();
    }
    texture_cache_clear();
    mem_temp_end();
    show_list(1);
    input_clear_quit();
}

static int beside_exe(char *dst, int cap, const char *leaf)
{
    char exe[260];
    DWORD len;
    int slash;
    int i;
    int k;

    len = GetModuleFileNameA(0, exe, 260);
    if (len == 0 || len >= 259) {
        return 0;
    }
    exe[len] = 0;
    slash = -1;
    for (i = 0; exe[i]; i++) {
        if (exe[i] == '\\' || exe[i] == '/') {
            slash = i;
        }
    }
    if (slash < 1) {
        return 0;
    }
    k = 0;
    while (leaf[k]) {
        k++;
    }
    if (slash + 1 + k + 1 > cap) {
        return 0;
    }
    for (i = 0; i <= slash; i++) {
        dst[i] = exe[i];
    }
    for (i = 0; i < k; i++) {
        dst[slash + 1 + i] = leaf[i];
    }
    dst[slash + 1 + k] = 0;
    return 1;
}

static const char *skip_space(const char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r') {
        s++;
    }
    return s;
}

static int key_is(const char *s, const char *key, const char **rest)
{
    int i;

    i = 0;
    while (key[i]) {
        if (s[i] != key[i]) {
            return 0;
        }
        i++;
    }
    const char *after;

    s += i;
    after = s;
    s = skip_space(s);
    if (*s == '=') {
        *rest = skip_space(s + 1);
        return 1;
    }
    /* A file from before key=value still loads. */
    if (s != after) {
        *rest = s;
        return 1;
    }
    return 0;
}

static int next_number(const char **ps)
{
    const char *s;

    s = skip_space(*ps);
    if (*s == ',') {
        *ps = s + 1;
        return 1;
    }
    if (s != *ps) {
        *ps = s;
        return 1;
    }
    return 0;
}

static int read_int(const char **ps, int *out)
{
    const char *s;
    int v;
    int any;

    s = skip_space(*ps);
    v = 0;
    any = 0;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        if (v < 100000) {
            v = v * 10 + (int)(*s - '0');
        }
        s++;
    }
    if (!any) {
        return 0;
    }
    *out = v;
    *ps = s;
    return 1;
}

static void cfg_line(const char *s)
{
    const char *rest;
    int r;
    int g;
    int b;
    int v;

    s = skip_space(s);
    if (*s == 0 || *s == '#') {
        return;
    }
    if (key_is(s, "background", &rest)) {
        if (!read_int(&rest, &r) || !next_number(&rest) || !read_int(&rest, &g)
            || !next_number(&rest) || !read_int(&rest, &b)) {
            return;
        }
        if (r > 255 || g > 255 || b > 255) {
            return;
        }
        g_bg_r = r;
        g_bg_g = g;
        g_bg_b = b;
        return;
    }
    if (key_is(s, "fog", &rest)) {
        if (!read_int(&rest, &v) || v != 0) {
            g_fog = 1;
            return;
        }
        g_fog = 0;
        return;
    }
    if (key_is(s, "far", &rest)) {
        if (!read_int(&rest, &v) || v < 8) {
            return;
        }
        g_far = v;
    }
}

static void cfg_apply(void)
{
    char line[48];
    int n;

    glide_background(g_bg_r, g_bg_g, g_bg_b);
    gfx_set_fog(g_fog);
    gfx_set_far((float)g_far);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "background=");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_bg_r);
    add_str(line, &n, (int)sizeof(line), ",");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_bg_g);
    add_str(line, &n, (int)sizeof(line), ",");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_bg_b);
    log_line(line);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "fog=");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_fog);
    log_line(line);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "far=");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_far);
    log_line(line);
}

static void cfg_load(void)
{
    static const char text[] =
        "# background=R,G,B, each 0 to 255\r\n"
        "background=128,128,128\r\n"
        "# fog=1 uses the distance table, fog=0 turns it off\r\n"
        "fog=1\r\n"
        "# far is the clip distance in world units, at most 16384\r\n"
        "far=8192\r\n";
    char path[260];
    char buf[2048];
    HANDLE file;
    DWORD got;
    DWORD wrote;
    int n;
    char *p;

    g_fog = 1;
    g_far = 8192;
    g_bg_r = 128;
    g_bg_g = 128;
    g_bg_b = 128;
    if (!beside_exe(path, (int)sizeof(path), "levelfall.cfg")) {
        log_line("could not name levelfall.cfg");
        cfg_apply();
        return;
    }
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        n = 0;
        while (text[n]) {
            n++;
        }
        file = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (file == INVALID_HANDLE_VALUE
            || !WriteFile(file, text, (DWORD)n, &wrote, 0)
            || wrote != (DWORD)n) {
            log_line("could not write levelfall.cfg");
        } else {
            log_line("wrote levelfall.cfg");
        }
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
    } else {
        file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (file == INVALID_HANDLE_VALUE) {
            log_line("could not read levelfall.cfg");
        } else {
            if (ReadFile(file, buf, (DWORD)sizeof(buf) - 1, &got, 0)) {
                buf[got] = 0;
                p = buf;
                while (*p) {
                    char *row;

                    row = p;
                    while (*p && *p != '\n') {
                        p++;
                    }
                    if (*p == '\n') {
                        *p = 0;
                        p++;
                    }
                    cfg_line(row);
                }
            }
            CloseHandle(file);
        }
    }
    cfg_apply();
}

static int levelfall_main(void *instance)
{
    char root[260];
    char line[64];
    int len;

    mem_init();
    log_open();
    log_line("levelfall");
    cfg_load();
    if (!win_startup(instance, "/ARENA2", "LEVELFALL", 1)) {
        log_line("window failed");
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    vfs_root(root, (int)sizeof(root));
    log_line(root);
    if (!make_list()) {
        log_line("list failed");
        win_fail("Could not open the level list.");
        win_shutdown();
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    g_rows = (DunRef *)mem_alloc((df_u32)MAX_LEVELS * (df_u32)sizeof(DunRef));
    g_view = (int *)mem_alloc((df_u32)MAX_LEVELS * (df_u32)sizeof(int));
    if (!g_rows || !g_view) {
        log_line("out of memory");
        win_fail("Out of memory.");
        win_shutdown();
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    win_pump();
    if (!mem_temp_begin()) {
        log_line("temp heap failed");
        win_fail("Out of memory.");
        win_shutdown();
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    g_nindoor = dungeon_list(g_rows, MAX_LEVELS);
    if (g_nindoor < 0) {
        g_nindoor = 0;
    }
    if (g_nindoor > MAX_LEVELS) {
        g_nindoor = 0;
    }
    g_noutdoor = 0;
    if (g_nindoor < MAX_LEVELS) {
        g_noutdoor = exterior_list(g_rows + g_nindoor, MAX_LEVELS - g_nindoor);
    }
    mem_temp_end();
    if (g_noutdoor < 0) {
        g_noutdoor = 0;
    }
    if (g_nindoor + g_noutdoor > MAX_LEVELS) {
        log_line("too many places");
        g_nrows = 0;
    } else {
        g_nrows = g_nindoor + g_noutdoor;
    }
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "places ");
    add_u(line, &len, (int)sizeof(line), (df_u32)g_nrows);
    log_line(line);
    g_sort_col = 2;
    g_sort_desc = 0;
    g_query[0] = 0;
    g_kind_filter = 0;
    if (g_nrows > 0) {
        refill_list();
    }
    log_line("list: type to filter, Enter opens, click a column to sort, Esc clears then quits");
    log_line("view: mouse looks, WASD flies along the aim, arrows translate, Shift is faster, Esc returns");
    if (g_nrows < 1) {
        win_fail(dungeon_error()[0] ? dungeon_error() : "MAPS.BSA has no places.");
    } else {
        while (!win_closed() && wait_list()) {
            int idx;

            idx = selected_row();
            if (idx < 0) {
                continue;
            }
            view_row(idx);
        }
    }
    win_shutdown();
    log_line("quit");
    if (g_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log);
    }
    mem_shutdown();
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)levelfall_main(GetModuleHandleA(0)));
}
