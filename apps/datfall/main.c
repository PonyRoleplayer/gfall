/*
 * DATFALL: a Windows 98 browser of races, enemies and weapons
 * (docs/data.md). A normal window, opened maximized, with the system
 * list, buttons and a picture. comctl32 and GDI are loaded at run time.
 * Escape quits. Left and right step a frame; a timer runs the animation.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "engine/assets/palette.h"
#include "engine/assets/texture.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/palette.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/tables/classic.h"

#define ID_RACES 1
#define ID_ENEMIES 2
#define ID_WEAPONS 3
#define ID_LIST 4
#define ID_PART 5
#define ID_TIMER 1

#define LVM_FIRST 0x1000
#define LVM_SETITEMA (LVM_FIRST + 6)
#define LVM_INSERTITEMA (LVM_FIRST + 7)
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#define LVM_INSERTCOLUMNA (LVM_FIRST + 27)
#define LVM_SETCOLUMNA (LVM_FIRST + 26)
#define LVIF_TEXT 0x0001
#define LVIF_STATE 0x0008
#define LVIS_FOCUSED 0x0001
#define LVIS_SELECTED 0x0002
#define LVCF_WIDTH 0x0002
#define LVCF_TEXT 0x0004
#define LVS_REPORT 0x0001
#define LVS_SINGLESEL 0x0004
#define LVS_SHOWSELALWAYS 0x0008
#define LVN_FIRST (-100)
#define LVN_ITEMCHANGEDA (LVN_FIRST - 1)
#define CB_RESETCONTENT 0x014B
#define CB_ADDSTRING 0x0143
#define CB_SETCURSEL 0x014E
#define CB_GETCURSEL 0x0147
#define CBN_SELCHANGE 1
#define BM_SETCHECK 0x00F1

#define MODE_RACES 0
#define MODE_ENEMIES 1
#define MODE_WEAPONS 2

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

typedef struct Gdi {
    void *(WINAPI *CreateFontA)(int, int, int, int, int, unsigned, unsigned, unsigned,
                                unsigned, unsigned, unsigned, unsigned, unsigned, const char *);
    int (WINAPI *SetStretchBltMode)(HDC, int);
    int (WINAPI *StretchDIBits)(HDC, int, int, int, int, int, int, int, int,
                                const void *, const BITMAPINFO *, unsigned int, unsigned int);
    void *(WINAPI *GetStockObject)(int);
} Gdi;

static const char *g_weapon_file[] = {
    "WEAPON00.CIF", "WEAPON01.CIF", "WEAPON02.CIF", "WEAPON03.CIF",
    "WEAPON04.CIF", "WEAPON05.CIF", "WEAPON06.CIF", "WEAPON07.CIF",
    "WEAPON08.CIF", "WEAPON09.CIF", "WEAPON10.CIF", "WEAPON11.CIF",
    "WEAPO101.CIF", "WEAPO102.CIF", "WEAPO104.CIF", "WEAPO105.CIF",
    "WEAPO106.CIF", "WEAPO107.CIF", "WEAPO108.CIF"
};

static const char *g_weapon_name[] = {
    "WEAPON00", "WEAPON01", "WEAPON02", "WEAPON03",
    "WEAPON04", "WEAPON05", "WEAPON06", "WEAPON07",
    "WEAPON08", "Bow", "WEAPON10", "WEAPON11",
    "WEAPON01 swing", "WEAPON02 swing", "WEAPON04 swing", "WEAPON05 swing",
    "WEAPON06 swing", "WEAPON07 swing", "WEAPON08 swing"
};

static const char *g_race_part[] = {
    "Background", "Body", "Clothed", "Female body", "Female clothed", "Head", "Female head"
};

static Gdi g_gdi;
static HWND g_hwnd;
static HWND g_races;
static HWND g_enemies;
static HWND g_weapons;
static HWND g_list;
static HWND g_part;
static HWND g_status;
static WNDPROC g_list_proc;
static void *g_font;
static int g_mode;
static int g_sel;
static int g_part_i;
static int g_frame;
static int g_filling;
static int g_enemy_n;
static int g_enemy_id[160];
static int g_view_n;
static int g_view_rec[10];
static Palette g_pal;
static int g_pal_ok;
static CifSet g_cif;
static int g_cif_on;
static ImgPicture g_img;
static int g_img_on;
static MemHeap *g_eheap;
static Image *g_eimg;
static unsigned char *g_dib;

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
    return g_gdi.CreateFontA && g_gdi.SetStretchBltMode && g_gdi.StretchDIBits
        && g_gdi.GetStockObject;
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

static int boot_data(void)
{
    char path[260];
    DWORD n;
    int i;
    int slash;
    const df_u8 *bytes;
    df_u32 size;

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
    texture_cache_limit(2u * 1024u * 1024u);
    bytes = vfs_read("ART_PAL.COL", &size);
    g_pal_ok = palette_load(bytes, size, &g_pal);
    return 1;
}

static void drop_enemy(void)
{
    g_eimg = 0;
    if (g_eheap) {
        mem_heap_delete(g_eheap);
        g_eheap = 0;
    }
}

static void drop_pictures(void)
{
    drop_enemy();
    if (g_cif_on) {
        cif_close(&g_cif);
        g_cif_on = 0;
    }
    g_img_on = 0;
}

static const CareerRace *race_at(int index)
{
    const CareerRace *races;
    int n;

    races = classic_races(&n);
    if (index < 0 || index >= n) {
        return 0;
    }
    return &races[index];
}

static int ends_cif(const char *file)
{
    int n;

    n = 0;
    if (!file) {
        return 0;
    }
    while (file[n]) {
        n++;
    }
    return n >= 4 && file[n - 3] == 'C' && file[n - 2] == 'I' && file[n - 1] == 'F';
}

static const char *race_file(const CareerRace *race, int part)
{
    if (!race) {
        return 0;
    }
    if (part == 1) {
        return race->body_male;
    }
    if (part == 2) {
        return race->body_male_clothed;
    }
    if (part == 3) {
        return race->body_female;
    }
    if (part == 4) {
        return race->body_female_clothed;
    }
    if (part == 5) {
        return race->heads_male;
    }
    if (part == 6) {
        return race->heads_female;
    }
    return race->background;
}

static const EnemyKind *listed_kind(int index)
{
    const EnemyKind *all;
    int n;
    int id;

    if (index < 0 || index >= g_enemy_n) {
        return 0;
    }
    all = classic_enemies(&n);
    id = g_enemy_id[index];
    if (id < 0 || id >= n) {
        return 0;
    }
    return &all[id];
}

static int enemy_record(void)
{
    if (g_part_i < 0 || g_part_i >= g_view_n) {
        return 0;
    }
    return g_view_rec[g_part_i];
}

static void build_views(const EnemyKind *kind)
{
    int i;

    g_view_n = 0;
    for (i = 0; i < 5; i++) {
        g_view_rec[g_view_n++] = i;
    }
    if (kind && kind->has_idle) {
        for (i = 0; i < 5; i++) {
            g_view_rec[g_view_n++] = 15 + i;
        }
    }
}

static int frame_count(void)
{
    const EnemyKind *kind;
    int n;

    if (g_mode == MODE_ENEMIES) {
        kind = listed_kind(g_sel);
        if (!kind || !kind->male) {
            return 0;
        }
        n = texture_frame_count((int)kind->male, enemy_record());
        if (n < 1) {
            return 0;
        }
        return n;
    }
    if (g_cif_on) {
        return g_cif.n;
    }
    if (g_img_on) {
        return 1;
    }
    return 0;
}

static void load_current(void)
{
    const CareerRace *race;
    const EnemyKind *kind;
    const char *file;
    MemHeap *prev;
    int frames;

    drop_pictures();
    if (g_mode == MODE_RACES) {
        race = race_at(g_sel);
        file = race_file(race, g_part_i);
        if (!file) {
            return;
        }
        if (ends_cif(file)) {
            g_cif_on = cif_open(file, &g_cif);
            return;
        }
        g_img_on = img_load(file, &g_img);
        return;
    }
    if (g_mode == MODE_ENEMIES) {
        kind = listed_kind(g_sel);
        if (!kind || !kind->male) {
            return;
        }
        frames = texture_frame_count((int)kind->male, enemy_record());
        if (frames < 1) {
            frames = 1;
        }
        if (g_frame >= frames) {
            g_frame = frames - 1;
        }
        if (g_frame < 0) {
            g_frame = 0;
        }
        g_eheap = mem_heap_new();
        if (!g_eheap) {
            return;
        }
        prev = mem_heap_use(g_eheap);
        g_eimg = texture_load_frame((int)kind->male, enemy_record(), g_frame);
        mem_heap_use(prev);
        return;
    }
    if (g_sel < 0 || g_sel >= (int)(sizeof(g_weapon_file) / sizeof(g_weapon_file[0]))) {
        return;
    }
    g_cif_on = cif_open(g_weapon_file[g_sel], &g_cif);
}

static const df_u8 *current_pixels(int *w, int *h)
{
    const CifFrame *frame;
    int n;

    *w = 0;
    *h = 0;
    if (g_mode == MODE_ENEMIES) {
        if (!g_eimg || !g_eimg->index) {
            return 0;
        }
        *w = g_eimg->width;
        *h = g_eimg->height;
        return g_eimg->index;
    }
    if (g_mode == MODE_RACES && g_img_on) {
        *w = g_img.width;
        *h = g_img.height;
        return g_img.pixels;
    }
    if (!g_cif_on) {
        return 0;
    }
    n = g_cif.n;
    if (n < 1) {
        return 0;
    }
    if (g_frame < 0) {
        g_frame = 0;
    }
    if (g_frame >= n) {
        g_frame = n - 1;
    }
    frame = cif_frame(&g_cif, g_frame);
    if (!frame || !frame->pixels) {
        return 0;
    }
    *w = frame->w;
    *h = frame->h;
    return frame->pixels;
}

static void status_text(void)
{
    char line[160];
    int w;
    int h;
    int frames;
    const CareerRace *race;
    const EnemyKind *kind;

    line[0] = 0;
    frames = frame_count();
    current_pixels(&w, &h);
    if (g_mode == MODE_RACES) {
        race = race_at(g_sel);
        if (race) {
            lstrcpynA(line, race->name, 24);
        }
    } else if (g_mode == MODE_ENEMIES) {
        kind = listed_kind(g_sel);
        if (kind) {
            lstrcpynA(line, kind->name, 32);
        }
    } else if (g_sel >= 0 && g_sel < (int)(sizeof(g_weapon_name) / sizeof(g_weapon_name[0]))) {
        lstrcpynA(line, g_weapon_name[g_sel], 32);
        lstrcatA(line, "  ");
        lstrcatA(line, cif_metal_name(g_part_i));
    }
    if (w > 0) {
        char extra[48];

        wsprintfA(extra, "   %d x %d   frame %d / %d", w, h, frames ? g_frame + 1 : 0, frames);
        lstrcatA(line, extra);
    } else {
        lstrcatA(line, "   no picture");
    }
    if (g_mode == MODE_ENEMIES) {
        kind = listed_kind(g_sel);
        if (kind && kind->male) {
            char extra[32];

            wsprintfA(extra, "   TEXTURE.%03d", (int)kind->male);
            lstrcatA(line, extra);
        }
    }
    SetWindowTextA(g_status, line);
}

static void paint_picture(HDC dc)
{
    RECT client;
    RECT well;
    int w;
    int h;
    int stride;
    int x;
    int y;
    int dw;
    int dh;
    int dx;
    int dy;
    int s;
    int s2;
    int metal;
    const df_u8 *pix;
    BITMAPINFOHEADER info;

    if (!GetClientRect(g_hwnd, &client)) {
        return;
    }
    well.left = 8;
    well.top = 40;
    well.right = client.right - 8;
    well.bottom = client.bottom - 28;
    if (g_list) {
        RECT list;

        GetWindowRect(g_list, &list);
        MapWindowPoints(0, g_hwnd, (POINT *)&list, 2);
        well.left = list.right + 8;
    }
    if (well.right - well.left < 16 || well.bottom - well.top < 16) {
        return;
    }
    DrawEdge(dc, &well, EDGE_SUNKEN, BF_RECT);
    well.left += 2;
    well.top += 2;
    well.right -= 2;
    well.bottom -= 2;
    pix = current_pixels(&w, &h);
    if (!pix || w < 1 || h < 1 || w > 512 || h > 512) {
        DrawTextA(dc, pix ? "Picture is too large" : "No picture", -1, &well,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    stride = (w * 3 + 3) & ~3;
    if (!g_dib) {
        g_dib = (unsigned char *)mem_keep(800u * 1024u);
    }
    if (!g_dib || stride * h > 800 * 1024) {
        return;
    }
    metal = g_mode == MODE_WEAPONS ? g_part_i : -1;
    for (y = 0; y < h; y++) {
        unsigned char *row;
        int sy;

        sy = h - 1 - y;
        row = g_dib + y * stride;
        for (x = 0; x < w; x++) {
            int ix;
            int r;
            int g;
            int b;
            int checker;

            ix = pix[sy * w + x];
            if (metal >= 0) {
                ix = cif_metal(ix, metal);
            }
            checker = ix == 0;
            if (checker) {
                int on;

                on = ((x >> 3) ^ (sy >> 3)) & 1;
                r = on ? 255 : 192;
                g = r;
                b = r;
            } else if (g_pal_ok) {
                r = g_pal.r[ix];
                g = g_pal.g[ix];
                b = g_pal.b[ix];
            } else {
                r = ix;
                g = ix;
                b = ix;
            }
            row[x * 3] = (unsigned char)b;
            row[x * 3 + 1] = (unsigned char)g;
            row[x * 3 + 2] = (unsigned char)r;
        }
    }
    dw = well.right - well.left - 8;
    dh = well.bottom - well.top - 8;
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
        if (s > 6) {
            s = 6;
        }
        dw = w * s;
        dh = h * s;
    }
    dx = well.left + (well.right - well.left - dw) / 2;
    dy = well.top + (well.bottom - well.top - dh) / 2;
    memset(&info, 0, sizeof(info));
    info.biSize = sizeof(info);
    info.biWidth = w;
    info.biHeight = h;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    g_gdi.SetStretchBltMode(dc, 3);
    g_gdi.StretchDIBits(dc, dx, dy, dw, dh, 0, 0, w, h, g_dib,
                        (const BITMAPINFO *)&info, 0, SRCCOPY);
}

static void invalidate_picture(void)
{
    RECT client;
    RECT well;

    if (!g_hwnd || !GetClientRect(g_hwnd, &client)) {
        return;
    }
    well.left = 8;
    well.top = 36;
    well.right = client.right;
    well.bottom = client.bottom - 24;
    InvalidateRect(g_hwnd, &well, 1);
}

static void show_current(void)
{
    int frames;

    load_current();
    frames = frame_count();
    if (frames < 1) {
        g_frame = 0;
    } else if (g_frame >= frames) {
        g_frame = frames - 1;
    }
    status_text();
    invalidate_picture();
}

static void step_frame(int delta)
{
    int n;

    n = frame_count();
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
    if (g_mode == MODE_ENEMIES) {
        load_current();
    }
    status_text();
    invalidate_picture();
}

static void fill_combo(void)
{
    int i;
    const EnemyKind *kind;

    SendMessageA(g_part, CB_RESETCONTENT, 0, 0);
    if (g_mode == MODE_RACES) {
        for (i = 0; i < 7; i++) {
            SendMessageA(g_part, CB_ADDSTRING, 0, (LPARAM)g_race_part[i]);
        }
    } else if (g_mode == MODE_ENEMIES) {
        static const char *walk[] = { "Front", "Turn", "Side", "Away", "Back" };
        static const char *stand[] = {
            "Stand front", "Stand turn", "Stand side", "Stand away", "Stand back"
        };

        kind = listed_kind(g_sel);
        build_views(kind);
        for (i = 0; i < g_view_n; i++) {
            const char *label;

            label = g_view_rec[i] >= 15 ? stand[g_view_rec[i] - 15] : walk[g_view_rec[i]];
            SendMessageA(g_part, CB_ADDSTRING, 0, (LPARAM)label);
        }
    } else {
        for (i = 0; i < CIF_METALS; i++) {
            SendMessageA(g_part, CB_ADDSTRING, 0, (LPARAM)cif_metal_name(i));
        }
    }
    if (g_part_i < 0) {
        g_part_i = 0;
    }
    SendMessageA(g_part, CB_SETCURSEL, (WPARAM)g_part_i, 0);
}

static void select_row(int index)
{
    LvItem item;

    memset(&item, 0, sizeof(item));
    item.mask = LVIF_STATE;
    item.iItem = index;
    item.state = LVIS_SELECTED | LVIS_FOCUSED;
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageA(g_list, LVM_SETITEMA, 0, (LPARAM)&item);
}

static void fill_list(void)
{
    const CareerRace *races;
    const EnemyKind *enemies;
    int n;
    int i;
    LvItem item;
    char title[8];

    g_filling = 1;
    SendMessageA(g_list, LVM_DELETEALLITEMS, 0, 0);
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.pszText = title;
    if (g_mode == MODE_RACES) {
        races = classic_races(&n);
        for (i = 0; i < n; i++) {
            item.iItem = i;
            item.pszText = (char *)races[i].name;
            SendMessageA(g_list, LVM_INSERTITEMA, 0, (LPARAM)&item);
        }
    } else if (g_mode == MODE_ENEMIES) {
        enemies = classic_enemies(&n);
        g_enemy_n = 0;
        for (i = 0; i < n && g_enemy_n < 160; i++) {
            if (!enemies[i].male || !enemies[i].name[0]) {
                continue;
            }
            g_enemy_id[g_enemy_n] = i;
            item.iItem = g_enemy_n;
            item.pszText = (char *)enemies[i].name;
            SendMessageA(g_list, LVM_INSERTITEMA, 0, (LPARAM)&item);
            g_enemy_n++;
        }
    } else {
        n = (int)(sizeof(g_weapon_name) / sizeof(g_weapon_name[0]));
        for (i = 0; i < n; i++) {
            item.iItem = i;
            item.pszText = (char *)g_weapon_name[i];
            SendMessageA(g_list, LVM_INSERTITEMA, 0, (LPARAM)&item);
        }
    }
    g_sel = 0;
    g_frame = 0;
    g_filling = 0;
    select_row(0);
    fill_combo();
    show_current();
}

static void set_mode(int mode)
{
    g_mode = mode;
    g_part_i = 0;
    g_frame = 0;
    SendMessageA(g_races, BM_SETCHECK, mode == MODE_RACES, 0);
    SendMessageA(g_enemies, BM_SETCHECK, mode == MODE_ENEMIES, 0);
    SendMessageA(g_weapons, BM_SETCHECK, mode == MODE_WEAPONS, 0);
    fill_list();
    SetFocus(g_list);
}

static void layout(void)
{
    RECT client;
    int w;
    int h;
    int list_w;
    LvCol col;

    if (!g_hwnd || !g_list || !GetClientRect(g_hwnd, &client)) {
        return;
    }
    w = client.right;
    h = client.bottom;
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    list_w = w / 3;
    if (list_w < 160) {
        list_w = 160;
    }
    if (list_w > w - 120) {
        list_w = w / 2;
    }
    MoveWindow(g_races, 8, 8, 88, 22, 1);
    MoveWindow(g_enemies, 100, 8, 88, 22, 1);
    MoveWindow(g_weapons, 192, 8, 88, 22, 1);
    MoveWindow(g_part, 292, 8, w > 520 ? w - 300 : 200, 240, 1);
    MoveWindow(g_list, 8, 40, list_w, h - 68, 1);
    MoveWindow(g_status, 8, h - 24, w - 16, 18, 1);
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_WIDTH | LVCF_TEXT;
    col.cx = list_w - 8;
    col.pszText = "Name";
    SendMessageA(g_list, LVM_SETCOLUMNA, 0, (LPARAM)&col);
    invalidate_picture();
}

static LRESULT CALLBACK list_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN && (wp == VK_LEFT || wp == VK_RIGHT)) {
        step_frame(wp == VK_LEFT ? -1 : 1);
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
        paint_picture(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED && (int)LOWORD(wp) == ID_RACES) {
            set_mode(MODE_RACES);
        } else if (HIWORD(wp) == BN_CLICKED && (int)LOWORD(wp) == ID_ENEMIES) {
            set_mode(MODE_ENEMIES);
        } else if (HIWORD(wp) == BN_CLICKED && (int)LOWORD(wp) == ID_WEAPONS) {
            set_mode(MODE_WEAPONS);
        } else if (HIWORD(wp) == CBN_SELCHANGE && (int)LOWORD(wp) == ID_PART) {
            g_part_i = (int)SendMessageA(g_part, CB_GETCURSEL, 0, 0);
            if (g_part_i < 0) {
                g_part_i = 0;
            }
            g_frame = 0;
            if (g_mode != MODE_WEAPONS) {
                show_current();
            } else {
                status_text();
                invalidate_picture();
            }
        }
        return 0;
    case WM_NOTIFY:
        if (((NmLv *)lp)->code == (unsigned int)LVN_ITEMCHANGEDA
            && ((NmLv *)lp)->uNewState & LVIS_SELECTED && !g_filling) {
            g_sel = ((NmLv *)lp)->iItem;
            g_frame = 0;
            if (g_mode == MODE_ENEMIES) {
                g_part_i = 0;
                fill_combo();
            }
            show_current();
        }
        return 0;
    case WM_TIMER:
        step_frame(1);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            PostQuitMessage(0);
        } else if (wp == VK_LEFT) {
            step_frame(-1);
        } else if (wp == VK_RIGHT) {
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

static HWND make_button(HINSTANCE inst, const char *text, int id, int group)
{
    return CreateWindowExA(
        0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | (group ? WS_GROUP : 0),
        0, 0, 10, 10, g_hwnd, (HMENU)(UINT_PTR)id, inst, 0);
}

static int win_main(HINSTANCE inst)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    LvCol col;

    mem_init();
    if (!gdi_load() || !common_controls()) {
        MessageBoxA(0, "The window controls did not load.", "DATFALL", MB_OK | MB_ICONERROR);
        return 1;
    }
    boot_data();
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
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 540,
        0, 0, inst, 0);
    if (!hwnd) {
        return 1;
    }
    g_hwnd = hwnd;
    g_races = make_button(inst, "Races", ID_RACES, 1);
    g_enemies = make_button(inst, "Enemies", ID_ENEMIES, 0);
    g_weapons = make_button(inst, "Weapons", ID_WEAPONS, 0);
    g_part = CreateWindowExA(
        0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 10, 10, hwnd, (HMENU)ID_PART, inst, 0);
    g_list = CreateWindowExA(
        WS_EX_CLIENTEDGE, "SysListView32", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)ID_LIST, inst, 0);
    g_status = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        0, 0, 10, 10, hwnd, 0, inst, 0);
    if (!g_list || !g_part) {
        MessageBoxA(0, "The list did not open.", "DATFALL", MB_OK | MB_ICONERROR);
        return 1;
    }
    SendMessageA(g_races, WM_SETFONT, (WPARAM)g_font, 1);
    SendMessageA(g_enemies, WM_SETFONT, (WPARAM)g_font, 1);
    SendMessageA(g_weapons, WM_SETFONT, (WPARAM)g_font, 1);
    SendMessageA(g_part, WM_SETFONT, (WPARAM)g_font, 1);
    SendMessageA(g_list, WM_SETFONT, (WPARAM)g_font, 1);
    SendMessageA(g_status, WM_SETFONT, (WPARAM)g_font, 1);
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_WIDTH | LVCF_TEXT;
    col.cx = 180;
    col.pszText = "Name";
    SendMessageA(g_list, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
    g_list_proc = (WNDPROC)SetWindowLongA(g_list, GWL_WNDPROC, (LONG)list_proc);
    set_mode(MODE_RACES);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
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
    drop_pictures();
    texture_cache_clear();
    return 0;
}

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)win_main(GetModuleHandleA(0)));
}
