/*
 * levelfall: list every indoor and outdoor place and fly through one.
 * Indoors are MAPDITEM records (RDB blocks). Outdoors are MAPPITEM
 * locations (RMB blocks). The index is built once at startup.
 * Type a name to filter. Esc clears the find box, then quits.
 * Esc in the view returns to the list. The title-bar cross quits.
 * W/S and Up/Down fly along the aim. A/D strafe. Left/Right turn.
 * Page Up and Page Down pitch, staying in place.
 * The wheel changes the field of view.
 * The view keeps a title bar and a status bar. F1 opens the rest.
 * F2 shows the sound panel; F4 steps the engine's sndview range.
 * F5..F9 flip cull, occlude, covers, faces, fog. F12 is a screenshot
 * plus a bookmark in levelfall.log that levelfall.cfg can reopen.
 * The left button or E uses what the middle of the view rests on: a
 * door swings, and a passage loads where it leads (a house's inside,
 * the street again, the dungeon below, the outside above). F11 steps
 * the engine's useview range, which draws what can be used.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/texcache.h"
#include "engine/platform/input.h"
#include "engine/platform/win32.h"
#include "engine/scene/clock.h"
#include "engine/scene/interact.h"
#include "engine/scene/walker.h"
#include "engine/scene/daylight.h"
#include "engine/scene/light.h"
#include "engine/scene/scene.h"
#include "engine/scene/weather.h"
#include "engine/sound/ambience.h"
#include "engine/sound/audio.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"
#include "engine/ui/console.h"
#include "engine/ui/font.h"
#include "engine/ui/hud.h"
#include "engine/ui/shot.h"
#include "game/dagger/climate.h"
#include "game/dagger/doors.h"
#include "game/dagger/dungeon.h"
#include "game/dagger/texture.h"
#include "game/dagger/world.h"
#include "game/dagger/wild.h"
#include "game/dagger/worldmap.h"
#include "engine/scene/stream.h"

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
#define ID_STREAM 5
#define BM_GETCHECK_ 0x00F0
#define BST_CHECKED_ 1

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
/* "Stream World": open the row's map pixel as the streamed world (docs/streaming.md). */
static HWND g_stream_box;
/* While the world streams: its frame hook runs before each frame, and the panel says so. */
static void (*g_frame_hook)(Camera *cam);
static int g_streaming;
static int g_stream_px = -1;
static int g_stream_py = -1;
static char g_stream_where[40];
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
static const Camera *g_cam;
static const Dungeon *g_dun;
static const World *g_world;
static int g_region;
static int g_fps;
static int g_fps_ready;
static int g_fps_frames;
static DWORD g_fps_tick;
static int g_fog = 1;
/* Two outdoor blocks. One block is 4096. The engine caps this at DF_FAR_MAX. */
static int g_far = 8192;
static int g_bg_r = 128;
static int g_bg_g = 128;
static int g_bg_b = 128;
static char g_view_name[40];
/* The row on screen, for the F12 bookmark. */
static const DunRef *g_ref;
/* levelfall.cfg fov=. 0 leaves the camera default. */
static int g_fov_cfg;
/* levelfall.cfg time=hh:mm, minutes since midnight. -1 leaves the clock at noon. */
static int g_time_cfg = -1;
/* levelfall.cfg weather=kind: there at start and held. -1 follows the odds. */
static int g_weather_cfg = -1;
/* levelfall.cfg date=day,month,year (from 1) or season=winter, at start. -1 leaves the clock's. */
static int g_day_cfg = -1;
static int g_season_cfg = -1;
/* levelfall.cfg open=kind,region,index. kind -1 is none, 0 indoor, 1 outdoor. */
static int g_auto_kind = -1;
static int g_auto_region;
static int g_auto_index;
/* levelfall.cfg cam=x,y,z,yaw,pitch, degrees. Only for that first open. */
static int g_auto_cam;
static int g_auto_row = -1;
static float g_auto_pose[5];

/*
 * The sound panel (F2) and its cfg. soundpanel= opens it at start,
 * sndrows= is how many of the nearest sources it lists. sndview= is the
 * engine's own marker range at start, which F4 steps through. sound=,
 * sndvol=, music=, musvol= are the engine's switches, set once the card
 * is open; -1 leaves the engine's default.
 */
#define SND_ROWS_MAX 8
static int g_sound_panel = 1;
static int g_snd_rows = 5;
static int g_sndview_cfg;
static int g_sound_cfg = -1;
static int g_sndvol_cfg = -1;
static int g_music_cfg = -1;
static int g_musvol_cfg = -1;
static int g_f2_was;
static int g_f4_was;
static int g_f11_was;
/* The profile panel (P), and P's edge (docs/profile.md). */
static int g_prof_panel;
static int g_p_was;

/*
 * A passage used in the view (docs/interact.md): what it leads to
 * (DOOR_*, 0 for none yet), its tag, and the passage as it stood. The
 * view ends on the frame it is set, and view_row opens where it leads.
 */
typedef struct LfGo {
    int what;
    int tag;
    Usable door;
} LfGo;
static LfGo g_go;
/*
 * Not a passage: the season turned under a town (the date, or a fast
 * clock), and its pictures and sky are the last season's. view_row
 * builds it again where the camera stands.
 */
#define LF_RESEASON (-1)
/* The row being viewed, and 1 while it is a building's inside. */
static int g_row_now = -1;
static int g_inside;

/* F5..F9 flip these engine switches in the fly view. Same names as the console. */
#define NSWITCH 5
static const int g_switch_vk[NSWITCH] = { VK_F5, VK_F6, VK_F7, VK_F8, VK_F9 };
static int g_switch_was[NSWITCH];

/* Red frame panel: a cut visible list or a picture with no room on the card. */
#define WARN_R 224.0f
#define WARN_G 72.0f
#define WARN_B 56.0f

/*
 * The frame on the glass when F12 went down. Taken before the camera
 * moves and before the next frame resets the counts, because the shot
 * reads the front buffer, which is the previous frame.
 */
typedef struct LfSnap {
    Camera cam;
    int valid;
    int models;
    int sprites;
    int kept;
    int dropped;
    int occ_faces;
    int occ_hidden;
    int cull;
    int occlude;
    int covers;
    int faces;
    int fog;
    int far_clip;
    int fov;
    int fps;
    int hour;
    int minute;
    int weather;
    /* clock_day: the date, whose season a town is drawn in. */
    int day;
} LfSnap;

static LfSnap g_snap;

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

/* Right-aligned in `width` columns. Width 0 writes the digits only. */
static void add_num(char *dst, int *n, int cap, int v, int width)
{
    char tmp[16];
    int k;
    int neg;
    unsigned int u;
    int digits;

    neg = v < 0;
    u = neg ? (unsigned int)(-v) : (unsigned int)v;
    k = 0;
    do {
        tmp[k++] = (char)('0' + (int)(u % 10u));
        u /= 10u;
    } while (u && k < (int)sizeof(tmp));
    digits = k + (neg ? 1 : 0);
    while (width > digits && *n < cap - 1) {
        dst[*n] = ' ';
        *n += 1;
        width--;
    }
    if (neg && *n < cap - 1) {
        dst[*n] = '-';
        *n += 1;
    }
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
    g_stream_box = CreateWindowExA(
        0, "BUTTON", "Stream &World",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        432, 8, 110, 20,
        hwnd, (HMENU)ID_STREAM, inst, 0);
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
    if (!g_find || !g_kind || !g_label || !g_list || !g_open || !g_stream_box) {
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
    apply_font(g_stream_box);
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
    ShowWindow(g_stream_box, cmd);
    if (on) {
        SetFocus(g_find);
    } else if (win_hwnd()) {
        SetFocus((HWND)win_hwnd());
    }
}

static void focus_step(int back)
{
    HWND order[5];
    HWND cur;
    int n;
    int i;
    int j;

    order[0] = g_find;
    order[1] = g_kind;
    order[2] = g_stream_box;
    order[3] = g_list;
    order[4] = g_open;
    n = 5;
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

static void text_right(int right, int top, const char *s)
{
    ui_string(right - ui_string_width(s), top, s);
}

static int yaw_deg(float yaw)
{
    int d;

    d = (int)(yaw * (360.0f / DF_TAU));
    d %= 360;
    if (d < 0) {
        d += 360;
    }
    return d;
}

/* Yaw 0 looks down world +Z. Positive yaw turns toward +X. */
static const char *facing_name(int deg)
{
    if (deg >= 45 && deg < 135) {
        return "+X";
    }
    if (deg >= 135 && deg < 225) {
        return "-Z";
    }
    if (deg >= 225 && deg < 315) {
        return "-X";
    }
    return "+Z";
}

static const char *climate_name(int id)
{
    if (id == CLIMATE_OCEAN) return "ocean";
    if (id == CLIMATE_DESERT) return "desert";
    if (id == CLIMATE_DESERT2) return "desert";
    if (id == CLIMATE_MOUNTAIN) return "mountain";
    if (id == CLIMATE_RAINFOREST) return "rainforest";
    if (id == CLIMATE_SWAMP) return "swamp";
    if (id == CLIMATE_SUBTROPICAL) return "subtropical";
    if (id == CLIMATE_MOUNTAIN_WOODS) return "mtn woods";
    if (id == CLIMATE_WOODLANDS) return "woodlands";
    if (id == CLIMATE_HAUNTED) return "haunted";
    return "climate";
}

static const char *family_name(int family)
{
    if (family == TEX_DESERT) return "desert";
    if (family == TEX_MOUNTAIN) return "mountain";
    if (family == TEX_WOODLAND) return "woodland";
    if (family == TEX_SWAMP) return "swamp";
    return "family";
}

/* Truncation toward zero would put a negative edge in the next tile. */
static int tile_of(float v, int tile)
{
    int i;

    i = (int)(v / (float)tile);
    if (v < 0.0f && (float)(i * tile) != v) {
        i--;
    }
    return i;
}

static const DunBlock *block_here(int *gx, int *gz)
{
    int tile;
    int i;
    const DunBlock *hit;

    if (!g_dun) {
        return 0;
    }
    tile = g_dun->outdoor ? 4096 : 2048;
    *gx = tile_of(g_cam->x, tile);
    *gz = tile_of(g_cam->z, tile);
    hit = 0;
    for (i = 0; i < g_dun->nblocks; i++) {
        if (g_dun->block[i].x == *gx && g_dun->block[i].z == *gz) {
            hit = &g_dun->block[i];
        }
    }
    return hit;
}

static void grid_span(int *w, int *h)
{
    int i;
    int minx;
    int maxx;
    int minz;
    int maxz;

    minx = 0;
    maxx = 0;
    minz = 0;
    maxz = 0;
    if (g_dun && g_dun->nblocks > 0) {
        minx = maxx = g_dun->block[0].x;
        minz = maxz = g_dun->block[0].z;
        for (i = 1; i < g_dun->nblocks; i++) {
            if (g_dun->block[i].x < minx) minx = g_dun->block[i].x;
            if (g_dun->block[i].x > maxx) maxx = g_dun->block[i].x;
            if (g_dun->block[i].z < minz) minz = g_dun->block[i].z;
            if (g_dun->block[i].z > maxz) maxz = g_dun->block[i].z;
        }
    }
    *w = maxx - minx + 1;
    *h = maxz - minz + 1;
}

static void sample_fps(void)
{
    DWORD now;
    DWORD dt;

    now = GetTickCount();
    if (g_fps_tick == 0) {
        g_fps_tick = now;
        return;
    }
    g_fps_frames++;
    dt = now - g_fps_tick;
    if (dt >= 500) {
        g_fps = (int)(((unsigned int)g_fps_frames * 1000u) / dt);
        g_fps_frames = 0;
        g_fps_tick = now;
        g_fps_ready = 1;
    }
}

static void pair_line(char *dst, int cap, const char *label, const char *value)
{
    int n;
    int col;

    n = 0;
    dst[0] = 0;
    add_str(dst, &n, cap, label);
    col = n;
    while (col < 13 && n < cap - 1) {
        dst[n++] = ' ';
        col++;
    }
    dst[n] = 0;
    add_str(dst, &n, cap, value);
}

static int line_h(void)
{
    int h;

    h = ui_line_height();
    if (h < 1) {
        h = 8;
    }
    return h;
}

static int measure_lines(const char *title, const char *aside, const char **lines, int n)
{
    int w;
    int i;
    int t;

    w = ui_string_width(title);
    if (aside && aside[0]) {
        t = ui_string_width(title) + 12 + ui_string_width(aside);
        if (t > w) {
            w = t;
        }
    }
    for (i = 0; i < n; i++) {
        if (!lines[i]) {
            continue;
        }
        t = ui_string_width(lines[i]);
        if (t > w) {
            w = t;
        }
    }
    return w + 8;
}

/* Title band, then n body lines with one pixel between them. */
static int measure_h(int n)
{
    int line;

    line = line_h();
    return (line + 4) + n * line + n + 4;
}

static void draw_panel(int x, int y, int w, int h, const char *title, const char *aside,
                       const char **lines, int n,
                       float ar, float ag, float ab)
{
    int line;
    int band;
    int i;
    int top;
    float tr;
    float tg;
    float tb;
    float br;
    float bg;
    float bb;

    line = line_h();
    band = line + 4;
    tr = ar * 0.50f;
    tg = ag * 0.50f;
    tb = ab * 0.50f;
    br = ar * 0.16f;
    bg = ag * 0.16f;
    bb = ab * 0.16f;
    glide_dim(1, 220);
    help_rect((float)x, (float)y, (float)w, (float)(h - band), br, bg, bb);
    help_rect((float)x, (float)(y + h - band), (float)w, (float)band, tr, tg, tb);
    glide_dim(0, 0);
    glide_overlay(1);
    help_rect((float)x, (float)y, (float)w, 1.0f, ar, ag, ab);
    help_rect((float)x, (float)(y + h - 1), (float)w, 1.0f, ar, ag, ab);
    help_rect((float)x, (float)y, 1.0f, (float)h, ar, ag, ab);
    help_rect((float)(x + w - 1), (float)y, 1.0f, (float)h, ar, ag, ab);
    glide_overlay(0);
    ui_string(x + 4, y + h - 2, title);
    if (aside && aside[0]) {
        text_right(x + w - 4, y + h - 2, aside);
    }
    top = y + h - band - 2;
    for (i = 0; i < n; i++) {
        if (lines[i] && lines[i][0]) {
            ui_string(x + 4, top, lines[i]);
        }
        top -= line + 1;
    }
}

/* Label, value, then the key that flips it, in three columns. */
static void switch_line(char *dst, int cap, const char *label, int on, const char *key)
{
    int n;

    pair_line(dst, cap, label, on ? "on" : "off");
    n = 0;
    while (dst[n]) {
        n++;
    }
    while (n < 18 && n < cap - 1) {
        dst[n++] = ' ';
    }
    dst[n] = 0;
    add_str(dst, &n, cap, key);
}

static void count_line(char *dst, int cap, const char *label, int v)
{
    char num[16];
    int n;

    n = 0;
    num[0] = 0;
    add_num(num, &n, (int)sizeof(num), v, 0);
    pair_line(dst, cap, label, num);
}

static void add_hm(char *dst, int *n, int cap, int hour, int minute)
{
    add_u(dst, n, cap, (df_u32)(hour / 10));
    add_u(dst, n, cap, (df_u32)(hour % 10));
    add_str(dst, n, cap, ":");
    add_u(dst, n, cap, (df_u32)(minute / 10));
    add_u(dst, n, cap, (df_u32)(minute % 10));
}

/*
 * The world clock, its timescale, and what lights an outdoor place at
 * that hour: sun or moon, and the lamps' level 0..255 once they are on.
 * Indoors nothing reads the clock (docs/daylight.md), so it says unused.
 */
static void time_line(char *dst, int cap)
{
    Daylight dl;
    int hour;
    int minute;
    int scale;
    int lamps;
    int n;

    clock_hm(&hour, &minute);
    scale = clock_scale();
    n = 0;
    dst[0] = 0;
    add_str(dst, &n, cap, "time ");
    add_hm(dst, &n, cap, hour, minute);
    if (scale > 0) {
        add_str(dst, &n, cap, "  x");
        add_num(dst, &n, cap, scale, 0);
    } else {
        add_str(dst, &n, cap, "  stopped");
    }
    if ((!g_dun || !g_dun->outdoor) && !g_streaming) {
        add_str(dst, &n, cap, "  unused");
        return;
    }
    if (!daylight_on()) {
        add_str(dst, &n, cap, "  full bright");
        return;
    }
    daylight_at(clock_minute(), &dl);
    add_str(dst, &n, cap, dl.sun ? "  sun" : "  moon");
    lamps = light_lamps();
    if (lamps > 0) {
        add_str(dst, &n, cap, "  lamps ");
        add_num(dst, &n, cap, lamps, 0);
    }
}

/* 0..1 as a whole percent. */
static int pct(float v)
{
    return df_iround(v * 100.0f);
}

/*
 * Two lines. The first is the kind, the one it is blending from and how
 * far (cloudy>rain 40%), then held when a kind is forced or the game
 * time to the next roll of the odds. The second is what the weather
 * does now: where its fog goes solid, the cloud, rain, snow and storm
 * in percent when there is any, and the simulated view when weathersim
 * is on. Single spaces keep a thunderstorm clear of the F1 panels.
 * Indoors the weather runs unseen, and with weather off it is drawn
 * clear: the second line is then empty, because none of those values
 * reach the frame.
 */
static void weather_lines(char *top, char *low, int cap)
{
    Weather w;
    int n;
    int next;

    weather_state(&w);
    n = 0;
    top[0] = 0;
    low[0] = 0;
    add_str(top, &n, cap, "weather ");
    if (w.blend < 1.0f && w.from != w.kind) {
        add_str(top, &n, cap, weather_name(w.from));
        add_str(top, &n, cap, ">");
        add_str(top, &n, cap, weather_name(w.kind));
        add_str(top, &n, cap, " ");
        add_num(top, &n, cap, pct(w.blend), 0);
        add_str(top, &n, cap, "%");
    } else {
        add_str(top, &n, cap, weather_name(w.kind));
    }
    /* A held kind does not roll, so it has no next. */
    if (weather_forced() >= 0) {
        add_str(top, &n, cap, "  held");
    } else {
        next = df_iround(w.next);
        add_str(top, &n, cap, "  next ");
        if (next >= 60) {
            add_num(top, &n, cap, next / 60, 0);
            add_str(top, &n, cap, "h");
        } else {
            add_num(top, &n, cap, next, 0);
            add_str(top, &n, cap, "m");
        }
    }
    if ((!g_dun || !g_dun->outdoor) && !g_streaming) {
        add_str(top, &n, cap, "  unused");
        return;
    }
    if (!weather_on()) {
        add_str(top, &n, cap, "  off");
        return;
    }
    n = 0;
    add_str(low, &n, cap, "fog ");
    if (w.fog >= WEATHER_CLEAR * 0.5f) {
        add_str(low, &n, cap, "clear");
    } else {
        add_num(low, &n, cap, df_iround(w.fog), 0);
    }
    if (pct(w.cloud) > 0) {
        add_str(low, &n, cap, " cloud ");
        add_num(low, &n, cap, pct(w.cloud), 0);
    }
    if (pct(w.rain) > 0) {
        add_str(low, &n, cap, " rain ");
        add_num(low, &n, cap, pct(w.rain), 0);
    }
    if (pct(w.snow) > 0) {
        add_str(low, &n, cap, " snow ");
        add_num(low, &n, cap, pct(w.snow), 0);
    }
    if (pct(w.storm) > 0) {
        add_str(low, &n, cap, " storm ");
        add_num(low, &n, cap, pct(w.storm), 0);
    }
    if (weather_sim()) {
        add_str(low, &n, cap, " view ");
        add_num(low, &n, cap, df_iround(w.view), 0);
    }
}

/* x87 fsqrt, as arch3d.c does: no libm. Only for the rows shown, not every emitter. */
static float lf_sqrt(float v)
{
    if (v <= 0.0f) {
        return 0.0f;
    }
    __asm__ __volatile__("fsqrt" : "=t"(v) : "0"(v));
    return v;
}

static const char *music_kind_name(int kind)
{
    if (kind == MUSIC_SYNTH) return "synth";
    if (kind == MUSIC_FM) return "fm";
    if (kind == MUSIC_PORT) return "port";
    return "no midi";
}

/*
 * The sndrows nearest emitters, nearest first, into best. One pass with
 * an insertion into a short list, on squared distance: no root per
 * emitter, and no array the size of the place.
 */
static int nearest_emitters(const Emitter *list, int count, int *best, float *best_d2, int want)
{
    int i;
    int n;
    int j;

    n = 0;
    for (i = 0; i < count; i++) {
        float dx;
        float dy;
        float dz;
        float d2;

        dx = list[i].x - g_cam->x;
        dy = list[i].y - g_cam->y;
        dz = list[i].z - g_cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (n == want && d2 >= best_d2[n - 1]) {
            continue;
        }
        j = n < want ? n++ : n - 1;
        while (j > 0 && best_d2[j - 1] > d2) {
            best[j] = best[j - 1];
            best_d2[j] = best_d2[j - 1];
            j--;
        }
        best[j] = i;
        best_d2[j] = d2;
    }
    return n;
}

/*
 * One source: its word, distance over reach, loop or now (now and then),
 * and play (a voice has it), near (in reach, no voice: a loop past the
 * AMBIENCE_LOOPS nearest, or a call waiting its turn) or out.
 */
static void source_line(char *dst, int cap, const Emitter *e, int index, float d2)
{
    int n;
    int d;

    n = 0;
    dst[0] = 0;
    add_str(dst, &n, cap, e->label && e->label[0] ? e->label : "?");
    while (n < 7 && n < cap - 1) {
        dst[n++] = ' ';
    }
    dst[n] = 0;
    d = df_iround(lf_sqrt(d2));
    add_num(dst, &n, cap, d, 5);
    add_str(dst, &n, cap, "/");
    add_num(dst, &n, cap, df_iround(e->reach), 0);
    add_str(dst, &n, cap, e->every > 0.0f ? "  now " : "  loop");
    if (ambience_emitter_playing(index)) {
        add_str(dst, &n, cap, "  play");
    } else if ((float)d <= e->reach) {
        add_str(dst, &n, cap, "  near");
    } else {
        add_str(dst, &n, cap, "  out");
    }
}

/*
 * F2: the sound panel, left, under place, bottom edge at top. The way
 * out and the mix, the song, the place's sources and how many are heard,
 * the engine's sndview range (F4), then the nearest sources.
 */
static void draw_sound(int x, int top)
{
    char head[6][48];
    char rows[SND_ROWS_MAX][48];
    const char *lines[6 + SND_ROWS_MAX];
    const Emitter *list;
    int best[SND_ROWS_MAX];
    float best_d2[SND_ROWS_MAX];
    int count;
    int shown;
    int heard;
    int loops;
    int stolen;
    int peak;
    int events;
    int nlines;
    int n;
    int i;
    int w;
    int h;
    const char *song;

    if (!g_sound_panel) {
        return;
    }
    n = 0;
    head[0][0] = 0;
    add_str(head[0], &n, 48, "out ");
    if (sound_ready()) {
        add_str(head[0], &n, 48, audio_mode_name(audio_mode()));
        add_str(head[0], &n, 48, " ");
        add_num(head[0], &n, 48, audio_rate(), 0);
        add_str(head[0], &n, 48, "  lead ");
        add_num(head[0], &n, 48, audio_lead_ms(), 0);
        add_str(head[0], &n, 48, "ms");
    } else {
        add_str(head[0], &n, 48, "none, silent");
    }
    n = 0;
    head[1][0] = 0;
    add_str(head[1], &n, 48, sound_on() ? "fx on " : "fx off ");
    add_num(head[1], &n, 48, sound_volume(), 0);
    add_str(head[1], &n, 48, "  voices ");
    add_num(head[1], &n, 48, sound_busy(), 0);
    add_str(head[1], &n, 48, "/");
    add_num(head[1], &n, 48, SOUND_VOICES, 0);
    n = 0;
    head[2][0] = 0;
    add_str(head[2], &n, 48, music_on() ? "music on " : "music off ");
    add_num(head[2], &n, 48, music_volume(), 0);
    add_str(head[2], &n, 48, "  ");
    song = music_name();
    add_str(head[2], &n, 48, song && song[0] ? song : "no song");
    if (song && song[0] && !music_playing()) {
        add_str(head[2], &n, 48, " stopped");
    }
    stolen = 0;
    peak = 0;
    events = 0;
    music_stats(&stolen, &peak, &events);
    n = 0;
    head[3][0] = 0;
    add_str(head[3], &n, 48, music_kind_name(music_kind()));
    add_str(head[3], &n, 48, "  notes ");
    add_num(head[3], &n, 48, peak, 0);
    add_str(head[3], &n, 48, "/");
    add_num(head[3], &n, 48, music_voices(), 0);
    add_str(head[3], &n, 48, "  cut ");
    add_num(head[3], &n, 48, stolen, 0);
    count = ambience_emitters(&list);
    heard = 0;
    loops = 0;
    ambience_stats(&heard, &loops);
    n = 0;
    head[4][0] = 0;
    add_str(head[4], &n, 48, "sources ");
    add_num(head[4], &n, 48, count, 0);
    add_str(head[4], &n, 48, "  heard ");
    add_num(head[4], &n, 48, heard, 0);
    add_str(head[4], &n, 48, "  loops ");
    add_num(head[4], &n, 48, loops, 0);
    add_str(head[4], &n, 48, "/");
    add_num(head[4], &n, 48, AMBIENCE_LOOPS, 0);
    n = 0;
    head[5][0] = 0;
    add_str(head[5], &n, 48, "sndview ");
    if (ambience_view() > 0) {
        add_num(head[5], &n, 48, ambience_view(), 0);
    } else {
        add_str(head[5], &n, 48, "off");
    }
    add_str(head[5], &n, 48, "  f4");
    for (i = 0; i < 6; i++) {
        lines[i] = head[i];
    }
    nlines = 6;
    shown = 0;
    if (list && count > 0 && g_snd_rows > 0) {
        shown = nearest_emitters(list, count, best, best_d2, g_snd_rows);
    }
    for (i = 0; i < shown; i++) {
        source_line(rows[i], 48, &list[best[i]], best[i], best_d2[i]);
        lines[nlines++] = rows[i];
    }
    /* The height follows the rows asked for, so it does not jump as sources come and go. */
    while (nlines < 6 + g_snd_rows) {
        lines[nlines++] = "";
    }
    if (count == 0 && g_snd_rows > 0) {
        lines[6] = "no sources here";
    }
    w = measure_lines("sound", "f2", lines, nlines);
    h = measure_h(nlines);
    draw_panel(x, top - h, w, h, "sound", "f2", lines, nlines, 176.0f, 120.0f, 216.0f);
}

/* Milliseconds to a tenth: "12.3". No C cast: per-frame code. */
static void add_ms(char *dst, int *n, int cap, float v)
{
    int t;

    t = df_iround(v * 10.0f);
    if (t < 0) {
        t = 0;
    }
    add_num(dst, n, cap, t / 10, 0);
    add_str(dst, n, cap, ".");
    add_num(dst, n, cap, t % 10, 0);
}

/*
 * The profile panel (P): each stage of the engine's frame, its mean and
 * its worst over the last half second, in milliseconds, from the cycle
 * counter (docs/profile.md). The title is the counter's calibrated rate.
 */
static void draw_profile(int x, int top)
{
    char rows[PROF_STAGES][40];
    const char *lines[PROF_STAGES];
    char aside[24];
    int i;
    int n;
    int w;
    int h;

    if (!g_prof_panel) {
        return;
    }
    for (i = 0; i < PROF_STAGES; i++) {
        char num[24];

        n = 0;
        num[0] = 0;
        add_ms(num, &n, (int)sizeof(num), prof_ms(i));
        add_str(num, &n, (int)sizeof(num), "  ");
        add_ms(num, &n, (int)sizeof(num), prof_peak_ms(i));
        pair_line(rows[i], 40, prof_name(i), num);
        lines[i] = rows[i];
    }
    n = 0;
    aside[0] = 0;
    if (prof_mhz() > 0) {
        add_num(aside, &n, (int)sizeof(aside), prof_mhz(), 0);
        add_str(aside, &n, (int)sizeof(aside), " mhz  p");
    } else {
        add_str(aside, &n, (int)sizeof(aside), "-- mhz  p");
    }
    w = measure_lines("ms mean worst", aside, lines, PROF_STAGES);
    h = measure_h(PROF_STAGES);
    draw_panel(x, top - h, w, h, "ms mean worst", aside, lines, PROF_STAGES,
               120.0f, 200.0f, 120.0f);
}

static void draw_overlay(void)
{
    char place_kind[40];
    char place_clim[48];
    char place_time[48];
    char place_sky[64];
    char place_air[64];
    char cam_x[48];
    char cam_y[48];
    char cam_z[48];
    char block_name[24];
    char block_cell[32];
    char block_grid[40];
    char frame_models[40];
    char frame_flats[40];
    char frame_fog[48];
    char frame_tex[48];
    char fps[16];
    char loc_v[16];
    char clim_v[32];
    char fam_v[32];
    char tri_v[16];
    char img_v[16];
    char ed_v[16];
    char files_v[16];
    char rec_row[7][40];
    char draw_row[9][32];
    char key_lines[17][48];
    const char *place_l[6];
    const char *cam_l[3];
    const char *block_l[3];
    const char *frame_l[4];
    const char *rec_l[7];
    const char *draw_l[9];
    const char *key_l[17];
    const char *key_name[17];
    const char *key_act[17];
    int n;
    int i;
    int gx;
    int gz;
    int gw;
    int gh;
    int yaw;
    int pitch;
    int models;
    int sprites;
    int kept;
    int dropped;
    int occ_faces;
    int occ_hidden;
    int tex_live;
    int tex_unfit;
    int tex_total;
    df_u32 tex_bytes;
    int warn;
    int family;
    int pw;
    int cw;
    int bw;
    int fw;
    int ph;
    int gap;
    int x;
    int side_w;
    int rw;
    int dw;
    int kw;
    int rh;
    int dh;
    int kh;
    int y;
    const DunBlock *block;

    if (!g_cam) {
        return;
    }
    sample_fps();
    yaw = yaw_deg(g_cam->yaw);
    pitch = (int)(g_cam->pitch * (360.0f / DF_TAU));
    gx = 0;
    gz = 0;
    block = block_here(&gx, &gz);
    grid_span(&gw, &gh);
    models = 0;
    sprites = 0;
    kept = 0;
    dropped = 0;
    draw_stats(&models, &sprites, &kept, &dropped);
    occ_faces = 0;
    occ_hidden = 0;
    scene_occ_stats(&occ_faces, &occ_hidden);
    tex_live = 0;
    tex_unfit = 0;
    tex_total = 0;
    tex_bytes = 0;
    texcache_stats(&tex_live, &tex_unfit, &tex_total, &tex_bytes);
    warn = models >= DRAW_VIS_MAX || sprites >= DRAW_VIS_MAX || tex_unfit > 0;

    n = 0;
    place_kind[0] = 0;
    if (g_streaming) {
        int loaded;
        int waiting;
        int filling;
        int shifts;

        stream_stats(&loaded, &waiting, &filling, &shifts);
        add_str(place_kind, &n, (int)sizeof(place_kind), "stream r");
        add_num(place_kind, &n, (int)sizeof(place_kind), stream_radius(), 0);
        add_str(place_kind, &n, (int)sizeof(place_kind), "  ");
        add_num(place_kind, &n, (int)sizeof(place_kind), loaded, 0);
        add_str(place_kind, &n, (int)sizeof(place_kind), " in  ");
        add_num(place_kind, &n, (int)sizeof(place_kind), waiting, 0);
        add_str(place_kind, &n, (int)sizeof(place_kind), " to go");
    } else if (g_dun) {
        add_str(place_kind, &n, (int)sizeof(place_kind),
                g_inside ? "inside" : (g_dun->outdoor ? "outdoor" : "indoor"));
        add_str(place_kind, &n, (int)sizeof(place_kind), "   r");
        add_num(place_kind, &n, (int)sizeof(place_kind), g_region, 0);
    }
    n = 0;
    place_clim[0] = 0;
    if (g_streaming) {
        add_str(place_clim, &n, (int)sizeof(place_clim),
                climate_name(worldmap_climate(g_stream_px, g_stream_py)));
        add_str(place_clim, &n, (int)sizeof(place_clim), "  ");
        add_str(place_clim, &n, (int)sizeof(place_clim), clock_season_name(clock_season()));
    } else if (g_dun) {
        int y, m, d;

        add_str(place_clim, &n, (int)sizeof(place_clim), climate_name(g_dun->climate));
        /* The season the place was drawn in, and the date now. */
        add_str(place_clim, &n, (int)sizeof(place_clim), "  ");
        add_str(place_clim, &n, (int)sizeof(place_clim),
                clock_season_name(g_world ? world_season(g_world) : clock_season()));
        clock_date(&y, &m, &d);
        add_str(place_clim, &n, (int)sizeof(place_clim), "  ");
        add_num(place_clim, &n, (int)sizeof(place_clim), d + 1, 0);
        add_str(place_clim, &n, (int)sizeof(place_clim), " ");
        add_str(place_clim, &n, (int)sizeof(place_clim), clock_month_name(m));
    }
    time_line(place_time, (int)sizeof(place_time));
    weather_lines(place_sky, place_air, (int)sizeof(place_sky));
    place_l[0] = g_view_name;
    place_l[1] = place_kind;
    place_l[2] = place_clim;
    place_l[3] = place_time;
    place_l[4] = place_sky;
    place_l[5] = place_air;

    n = 0;
    cam_x[0] = 0;
    add_str(cam_x, &n, (int)sizeof(cam_x), "x ");
    add_num(cam_x, &n, (int)sizeof(cam_x), (int)g_cam->x, 6);
    add_str(cam_x, &n, (int)sizeof(cam_x), "   yaw ");
    add_num(cam_x, &n, (int)sizeof(cam_x), yaw, 3);
    add_str(cam_x, &n, (int)sizeof(cam_x), " ");
    add_str(cam_x, &n, (int)sizeof(cam_x), facing_name(yaw));
    n = 0;
    cam_y[0] = 0;
    add_str(cam_y, &n, (int)sizeof(cam_y), "y ");
    add_num(cam_y, &n, (int)sizeof(cam_y), (int)g_cam->y, 6);
    add_str(cam_y, &n, (int)sizeof(cam_y), "   pitch ");
    add_num(cam_y, &n, (int)sizeof(cam_y), pitch, 4);
    n = 0;
    cam_z[0] = 0;
    add_str(cam_z, &n, (int)sizeof(cam_z), "z ");
    add_num(cam_z, &n, (int)sizeof(cam_z), (int)g_cam->z, 6);
    add_str(cam_z, &n, (int)sizeof(cam_z), "   fov ");
    add_num(cam_z, &n, (int)sizeof(cam_z), (int)camera_fov(), 3);
    cam_l[0] = cam_x;
    cam_l[1] = cam_y;
    cam_l[2] = cam_z;

    n = 0;
    block_name[0] = 0;
    if (block) {
        add_str(block_name, &n, (int)sizeof(block_name), block->name);
    } else {
        add_str(block_name, &n, (int)sizeof(block_name), "outside");
    }
    n = 0;
    block_cell[0] = 0;
    add_str(block_cell, &n, (int)sizeof(block_cell), "cell ");
    add_num(block_cell, &n, (int)sizeof(block_cell), gx, 0);
    add_str(block_cell, &n, (int)sizeof(block_cell), ",");
    add_num(block_cell, &n, (int)sizeof(block_cell), gz, 0);
    n = 0;
    block_grid[0] = 0;
    add_str(block_grid, &n, (int)sizeof(block_grid), "grid ");
    add_num(block_grid, &n, (int)sizeof(block_grid), gw, 0);
    add_str(block_grid, &n, (int)sizeof(block_grid), "x");
    add_num(block_grid, &n, (int)sizeof(block_grid), gh, 0);
    block_l[0] = block_name;
    block_l[1] = block_cell;
    block_l[2] = block_grid;

    n = 0;
    frame_models[0] = 0;
    add_str(frame_models, &n, (int)sizeof(frame_models), "models ");
    add_num(frame_models, &n, (int)sizeof(frame_models), models, 0);
    add_str(frame_models, &n, (int)sizeof(frame_models), "/");
    add_num(frame_models, &n, (int)sizeof(frame_models),
            g_world ? world_model_count(g_world) : 0, 0);
    /* The list stopped at the cap: the rest of the models were not drawn. */
    if (models >= DRAW_VIS_MAX) {
        add_str(frame_models, &n, (int)sizeof(frame_models), " CAP");
    }
    n = 0;
    frame_flats[0] = 0;
    add_str(frame_flats, &n, (int)sizeof(frame_flats), "flats ");
    add_num(frame_flats, &n, (int)sizeof(frame_flats), sprites, 0);
    add_str(frame_flats, &n, (int)sizeof(frame_flats), "/");
    add_num(frame_flats, &n, (int)sizeof(frame_flats),
            g_world ? world_flat_count(g_world) : 0, 0);
    if (sprites >= DRAW_VIS_MAX) {
        add_str(frame_flats, &n, (int)sizeof(frame_flats), " CAP");
    }
    n = 0;
    frame_fog[0] = 0;
    add_str(frame_fog, &n, (int)sizeof(frame_fog), "fog ");
    add_str(frame_fog, &n, (int)sizeof(frame_fog), gfx_fog() ? "on" : "off");
    add_str(frame_fog, &n, (int)sizeof(frame_fog), "  far ");
    add_num(frame_fog, &n, (int)sizeof(frame_fog), (int)gfx_far(), 0);
    /*
     * Pictures on the card over the level's count, and their size. An
     * unfit picture has no room, so its faces wear the last bound texture.
     */
    n = 0;
    frame_tex[0] = 0;
    add_str(frame_tex, &n, (int)sizeof(frame_tex), "tex ");
    add_num(frame_tex, &n, (int)sizeof(frame_tex), tex_live, 0);
    add_str(frame_tex, &n, (int)sizeof(frame_tex), "/");
    add_num(frame_tex, &n, (int)sizeof(frame_tex), tex_total, 0);
    add_str(frame_tex, &n, (int)sizeof(frame_tex), "  ");
    add_u(frame_tex, &n, (int)sizeof(frame_tex), tex_bytes / 1024u);
    add_str(frame_tex, &n, (int)sizeof(frame_tex), "K");
    if (tex_unfit > 0) {
        add_str(frame_tex, &n, (int)sizeof(frame_tex), "  unfit ");
        add_num(frame_tex, &n, (int)sizeof(frame_tex), tex_unfit, 0);
    }
    frame_l[0] = frame_models;
    frame_l[1] = frame_flats;
    frame_l[2] = frame_fog;
    frame_l[3] = frame_tex;
    n = 0;
    fps[0] = 0;
    if (g_fps_ready) {
        add_num(fps, &n, (int)sizeof(fps), g_fps, 0);
    } else {
        add_str(fps, &n, (int)sizeof(fps), "--");
    }
    add_str(fps, &n, (int)sizeof(fps), " fps");

    gap = 6;
    /* Place is six lines, alone in the top left. Its weather line can be empty, so its height is fixed. */
    ph = measure_h(6);
    pw = measure_lines("place", g_help ? "f1 hide" : "f1", place_l, 6);
    draw_panel(gap, DF_VIEW_H - gap - ph, pw, ph, "place",
               g_help ? "f1 hide" : "f1", place_l, 6, 204.0f, 160.0f, 64.0f);
    draw_profile(gap + pw + gap, DF_VIEW_H - gap);
    draw_sound(gap, DF_VIEW_H - gap - ph - gap);
    /* The bottom row is as tall as frame. Camera and block leave a blank last line. */
    ph = measure_h(4);
    cw = measure_lines("camera", 0, cam_l, 3);
    bw = measure_lines("block", 0, block_l, 3);
    fw = measure_lines("frame", fps, frame_l, 4);
    x = gap;
    draw_panel(x, gap, cw, ph, "camera", 0, cam_l, 3, 112.0f, 168.0f, 208.0f);
    x += cw + gap;
    draw_panel(x, gap, bw, ph, "block", 0, block_l, 3, 120.0f, 184.0f, 96.0f);
    x += bw + gap;
    if (x + fw > DF_VIEW_W - gap) {
        fw = DF_VIEW_W - gap - x;
    }
    if (fw > 8) {
        if (warn) {
            draw_panel(x, gap, fw, ph, "frame", fps, frame_l, 4, WARN_R, WARN_G, WARN_B);
        } else {
            draw_panel(x, gap, fw, ph, "frame", fps, frame_l, 4, 168.0f, 168.0f, 180.0f);
        }
    }

    if (!g_help) {
        return;
    }
    n = 0;
    loc_v[0] = 0;
    add_num(loc_v, &n, (int)sizeof(loc_v), g_dun ? g_dun->location_id : 0, 0);
    n = 0;
    clim_v[0] = 0;
    if (g_dun) {
        add_num(clim_v, &n, (int)sizeof(clim_v), g_dun->climate, 0);
        add_str(clim_v, &n, (int)sizeof(clim_v), " ");
        add_str(clim_v, &n, (int)sizeof(clim_v), climate_name(g_dun->climate));
    }
    family = g_dun ? climate_family(g_dun->climate) : -1;
    n = 0;
    fam_v[0] = 0;
    if (family >= 0) {
        add_num(fam_v, &n, (int)sizeof(fam_v), family, 0);
        add_str(fam_v, &n, (int)sizeof(fam_v), " ");
        add_str(fam_v, &n, (int)sizeof(fam_v), family_name(family));
    } else {
        add_str(fam_v, &n, (int)sizeof(fam_v), "none");
    }
    n = 0;
    tri_v[0] = 0;
    add_num(tri_v, &n, (int)sizeof(tri_v), g_world ? world_triangle_count(g_world) : 0, 0);
    n = 0;
    img_v[0] = 0;
    add_num(img_v, &n, (int)sizeof(img_v), g_world ? world_image_count(g_world) : 0, 0);
    n = 0;
    ed_v[0] = 0;
    add_num(ed_v, &n, (int)sizeof(ed_v), g_world ? world_editor_count(g_world) : 0, 0);
    n = 0;
    files_v[0] = 0;
    add_num(files_v, &n, (int)sizeof(files_v),
            g_world ? world_texture_file_count(g_world) : 0, 0);
    pair_line(rec_row[0], 40, "location", loc_v);
    pair_line(rec_row[1], 40, "climate", clim_v);
    pair_line(rec_row[2], 40, "family", fam_v);
    pair_line(rec_row[3], 40, "triangles", tri_v);
    pair_line(rec_row[4], 40, "images", img_v);
    pair_line(rec_row[5], 40, "editors", ed_v);
    pair_line(rec_row[6], 40, "tex files", files_v);
    for (i = 0; i < 7; i++) {
        rec_l[i] = rec_row[i];
    }
    /*
     * Labels are the console's words, so what is read here is what is
     * typed there. faces on drops ARCH3D planes whose normal points away.
     * occluders are faces written into the cells; occluded is the models
     * and sprites those cells dropped.
     */
    switch_line(draw_row[0], 32, "cull", scene_cull(), "f5");
    switch_line(draw_row[1], 32, "occlude", scene_occlude(), "f6");
    switch_line(draw_row[2], 32, "covers", scene_cover_draw(), "f7");
    switch_line(draw_row[3], 32, "faces", draw_faces(), "f8");
    switch_line(draw_row[4], 32, "fog", gfx_fog(), "f9");
    count_line(draw_row[5], 32, "faces kept", kept);
    count_line(draw_row[6], 32, "faces hidden", dropped);
    count_line(draw_row[7], 32, "occluders", occ_faces);
    count_line(draw_row[8], 32, "occluded", occ_hidden);
    for (i = 0; i < 9; i++) {
        draw_l[i] = draw_row[i];
    }
    key_name[0] = "mouse";
    key_act[0] = "look";
    key_name[1] = "w s up dn";
    key_act[1] = "forward";
    key_name[2] = "a d";
    key_act[2] = "strafe";
    key_name[3] = "left right";
    key_act[3] = "turn";
    key_name[4] = "pgup pgdn";
    key_act[4] = "pitch";
    key_name[5] = "wheel";
    key_act[5] = "field";
    key_name[6] = "shift";
    key_act[6] = "faster";
    key_name[7] = "f5 - f9";
    key_act[7] = "draw switches";
    key_name[8] = "esc";
    key_act[8] = "list";
    key_name[9] = "f1";
    key_act[9] = "hide";
    key_name[10] = "left of 1";
    key_act[10] = "console";
    key_name[11] = "f12";
    key_act[11] = "shot, bookmark";
    key_name[12] = "f2";
    key_act[12] = "sound panel";
    key_name[13] = "f4";
    key_act[13] = "sndview range";
    key_name[14] = "click e";
    key_act[14] = "use";
    key_name[15] = "f11";
    key_act[15] = "useview range";
    key_name[16] = "p";
    key_act[16] = "frame profile";
    for (i = 0; i < 17; i++) {
        pair_line(key_lines[i], 48, key_name[i], key_act[i]);
        key_l[i] = key_lines[i];
    }
    rw = measure_lines("record", 0, rec_l, 7);
    dw = measure_lines("draw", 0, draw_l, 9);
    kw = measure_lines("keys", 0, key_l, 17);
    side_w = rw;
    if (dw > side_w) {
        side_w = dw;
    }
    if (kw > side_w) {
        side_w = kw;
    }
    rh = measure_h(7);
    dh = measure_h(9);
    kh = measure_h(17);
    x = DF_VIEW_W - gap - side_w;
    y = DF_VIEW_H - gap - rh;
    draw_panel(x, y, side_w, rh, "record", 0, rec_l, 7, 204.0f, 160.0f, 64.0f);
    y -= gap + dh;
    draw_panel(x, y, side_w, dh, "draw", 0, draw_l, 9, 112.0f, 168.0f, 208.0f);
    y -= gap + kh;
    if (y < gap + ph) {
        y = gap + ph;
    }
    draw_panel(x, y, side_w, kh, "keys", 0, key_l, 17, 168.0f, 168.0f, 180.0f);
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

/* Flip one engine switch and say so in the console log, as the command would. */
static void flip_switch(int i)
{
    char line[24];
    const char *name;
    int on;
    int n;

    if (i == 0) {
        name = "cull";
        on = scene_cull() ? 0 : 1;
        scene_set_cull(on);
    } else if (i == 1) {
        name = "occlude";
        on = scene_occlude() ? 0 : 1;
        scene_set_occlude(on);
    } else if (i == 2) {
        name = "covers";
        on = scene_cover_draw() ? 0 : 1;
        scene_set_cover_draw(on);
    } else if (i == 3) {
        name = "faces";
        on = draw_faces() ? 0 : 1;
        draw_set_faces(on);
    } else {
        name = "fog";
        on = gfx_fog() ? 0 : 1;
        gfx_set_fog(on);
    }
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), name);
    add_str(line, &n, (int)sizeof(line), on ? " on" : " off");
    console_log(line);
}

/*
 * F4: the engine's emitter markers (sndview) off, then 512, 1024, 2048,
 * 4096 units round the camera, then off. A cfg range between the steps
 * goes on to the next one up.
 */
static void step_sndview(void)
{
    static const int steps[4] = { 512, 1024, 2048, AMBIENCE_VIEW_MAX };
    char line[24];
    int cur;
    int next;
    int i;
    int n;

    cur = ambience_view();
    next = 0;
    for (i = 0; i < 4; i++) {
        if (steps[i] > cur) {
            next = steps[i];
            break;
        }
    }
    ambience_set_view(next);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "sndview ");
    add_num(line, &n, (int)sizeof(line), ambience_view(), 0);
    console_log(line);
}

/* F11: what can be used, within 256, 512, then 1024 units, then off. */
static void step_useview(void)
{
    static const int steps[3] = { 256, 512, 1024 };
    char line[24];
    int cur;
    int next;
    int i;
    int n;

    cur = interact_view();
    next = 0;
    for (i = 0; i < 3; i++) {
        if (steps[i] > cur) {
            next = steps[i];
            break;
        }
    }
    interact_set_view(next);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "useview ");
    add_num(line, &n, (int)sizeof(line), interact_view(), 0);
    console_log(line);
}

/* Edges are tracked while the console is up too, so a key held through it does not fire. */
static void switch_keys(int act)
{
    int i;
    int down;

    for (i = 0; i < NSWITCH; i++) {
        down = key_down(g_switch_vk[i]);
        if (act && down && !g_switch_was[i]) {
            flip_switch(i);
        }
        g_switch_was[i] = down;
    }
    down = key_down(VK_F2);
    if (act && down && !g_f2_was) {
        g_sound_panel = g_sound_panel ? 0 : 1;
    }
    g_f2_was = down;
    down = key_down(VK_F4);
    if (act && down && !g_f4_was) {
        step_sndview();
    }
    g_f4_was = down;
    down = key_down(VK_F11);
    if (act && down && !g_f11_was) {
        step_useview();
    }
    g_f11_was = down;
    down = key_down('P');
    if (act && down && !g_p_was) {
        g_prof_panel = g_prof_panel ? 0 : 1;
    }
    g_p_was = down;
}

/*
 * The other half of row's location: with want_outdoor 0, the dungeon
 * under a town (its exterior_id is the town's number); with 1, the
 * outside above a dungeon. -1 when the list has none.
 */
static int pair_row(int row, int want_outdoor)
{
    const DunRef *r;
    int i;

    if (row < 0 || row >= g_nrows) {
        return -1;
    }
    r = &g_rows[row];
    for (i = 0; i < g_nrows; i++) {
        const DunRef *o;

        o = &g_rows[i];
        if (o->region != r->region || o->outdoor != want_outdoor) {
            continue;
        }
        if (want_outdoor ? o->location_id == r->exterior_id : o->exterior_id == r->location_id) {
            return i;
        }
    }
    return -1;
}

/*
 * The engine's handler while a place is viewed (interact_set_handler):
 * a passage ends the view, once view_row has somewhere to open; a
 * person says who they are, which is all a person does yet.
 */
static int lf_use(const Scene *scene, const Usable *u, int index)
{
    char line[64];
    int n;

    (void)scene;
    (void)index;
    if (u->kind == USE_TALK) {
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "A person, faction ");
        add_num(line, &n, (int)sizeof(line), u->what, 0);
        add_str(line, &n, (int)sizeof(line), ".");
        interact_say(line);
        return 1;
    }
    if (u->kind != USE_PASSAGE) {
        return 0;
    }
    if ((u->what == DOOR_DUNGEON && pair_row(g_row_now, 0) < 0)
        || (u->what == DOOR_EXIT && pair_row(g_row_now, 1) < 0)) {
        interact_say("It leads nowhere in the list.");
        return 1;
    }
    if (u->what == DOOR_LEAVE && !g_inside) {
        return 0;
    }
    g_go.what = u->what;
    g_go.tag = u->tag;
    g_go.door = *u;
    log_line(u->what == DOOR_ENTER ? "use: into a building"
             : u->what == DOOR_LEAVE ? "use: out to the street"
             : u->what == DOOR_DUNGEON ? "use: down into the dungeon" : "use: out of the dungeon");
    return 1;
}

static void snap_take(const Camera *cam, int valid)
{
    g_snap.cam = *cam;
    g_snap.valid = valid;
    draw_stats(&g_snap.models, &g_snap.sprites, &g_snap.kept, &g_snap.dropped);
    scene_occ_stats(&g_snap.occ_faces, &g_snap.occ_hidden);
    g_snap.cull = scene_cull();
    g_snap.occlude = scene_occlude();
    g_snap.covers = scene_cover_draw();
    g_snap.faces = draw_faces();
    g_snap.fog = gfx_fog();
    g_snap.far_clip = df_iround(gfx_far());
    g_snap.fov = df_iround(camera_fov());
    g_snap.fps = g_fps_ready ? g_fps : 0;
    clock_hm(&g_snap.hour, &g_snap.minute);
    g_snap.day = clock_day();
    {
        Weather w;

        weather_state(&w);
        g_snap.weather = w.kind;
    }
}

/*
 * Three places after the point, rounded. A C cast is fine here: this
 * runs once per F12, not per frame, and df_iround stops at 2^22.
 */
static void add_fixed(char *dst, int *n, int cap, float v)
{
    int a;
    int frac;

    a = (int)(v * 1000.0f + (v < 0.0f ? -0.5f : 0.5f));
    if (a < 0) {
        add_str(dst, n, cap, "-");
        a = -a;
    }
    add_u(dst, n, cap, (df_u32)(a / 1000));
    add_str(dst, n, cap, ".");
    frac = a % 1000;
    add_u(dst, n, cap, (df_u32)(frac / 100));
    add_u(dst, n, cap, (df_u32)((frac / 10) % 10));
    add_u(dst, n, cap, (df_u32)(frac % 10));
}

static void add_flag(char *dst, int *n, int cap, const char *name, int v)
{
    add_str(dst, n, cap, name);
    add_str(dst, n, cap, "=");
    add_num(dst, n, cap, v, 0);
}

/*
 * F12 went down: write the place and the pose as levelfall.cfg lines,
 * so pasting open= and cam= into the cfg reopens this exact view. The
 * comment lines are the switches and counts of that frame.
 */
static void bookmark(void)
{
    char line[256];
    int n;
    int live;
    int unfit;
    int total;

    if (!g_snap.valid || !g_ref) {
        return;
    }
    /* open= names a row of the list; a building's inside is not one. */
    if (g_inside) {
        log_line("bookmark: not inside a building");
        return;
    }
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "bookmark ");
    add_str(line, &n, (int)sizeof(line), shot_status());
    log_line(line);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), g_ref->outdoor ? "open=outdoor," : "open=indoor,");
    add_num(line, &n, (int)sizeof(line), g_ref->region, 0);
    add_str(line, &n, (int)sizeof(line), ",");
    add_num(line, &n, (int)sizeof(line), g_ref->index, 0);
    log_line(line);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "cam=");
    add_fixed(line, &n, (int)sizeof(line), g_snap.cam.x);
    add_str(line, &n, (int)sizeof(line), ",");
    add_fixed(line, &n, (int)sizeof(line), g_snap.cam.y);
    add_str(line, &n, (int)sizeof(line), ",");
    add_fixed(line, &n, (int)sizeof(line), g_snap.cam.z);
    add_str(line, &n, (int)sizeof(line), ",");
    add_fixed(line, &n, (int)sizeof(line), g_snap.cam.yaw * (360.0f / DF_TAU));
    add_str(line, &n, (int)sizeof(line), ",");
    add_fixed(line, &n, (int)sizeof(line), g_snap.cam.pitch * (360.0f / DF_TAU));
    log_line(line);
    n = 0;
    line[0] = 0;
    add_flag(line, &n, (int)sizeof(line), "fov", g_snap.fov);
    log_line(line);
    /* The hour lights an outdoor place, so a lighting bug needs it to reopen. */
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "time=");
    add_hm(line, &n, (int)sizeof(line), g_snap.hour, g_snap.minute);
    log_line(line);
    /* The date's season picks a town's pictures and sky. */
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "date=");
    add_num(line, &n, (int)sizeof(line), g_snap.day % CLOCK_MONTH_DAYS + 1, 0);
    add_str(line, &n, (int)sizeof(line), ",");
    add_num(line, &n, (int)sizeof(line), (g_snap.day % CLOCK_YEAR_DAYS) / CLOCK_MONTH_DAYS + 1, 0);
    add_str(line, &n, (int)sizeof(line), ",");
    add_num(line, &n, (int)sizeof(line), g_snap.day / CLOCK_YEAR_DAYS, 0);
    log_line(line);
    /* The kind arrived or arriving. Reopened, it is there at once and held. */
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "weather=");
    add_str(line, &n, (int)sizeof(line), weather_name(g_snap.weather));
    log_line(line);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "# ");
    add_str(line, &n, (int)sizeof(line), g_view_name);
    add_str(line, &n, (int)sizeof(line), "  ");
    add_flag(line, &n, (int)sizeof(line), "cull", g_snap.cull);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "occlude", g_snap.occlude);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "covers", g_snap.covers);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "faces", g_snap.faces);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "fog", g_snap.fog);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "far", g_snap.far_clip);
    log_line(line);
    live = 0;
    unfit = 0;
    total = 0;
    texcache_stats(&live, &unfit, &total, 0);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "# ");
    add_flag(line, &n, (int)sizeof(line), "models", g_snap.models);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "sprites", g_snap.sprites);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "kept", g_snap.kept);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "hidden", g_snap.dropped);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "occluders", g_snap.occ_faces);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "occluded", g_snap.occ_hidden);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "tex", live);
    add_str(line, &n, (int)sizeof(line), "/");
    add_num(line, &n, (int)sizeof(line), total, 0);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "unfit", unfit);
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "fps", g_snap.fps);
    log_line(line);
    /* What the place sounded like: read now, not snapped, since sound runs on its own clock. */
    {
        int heard;
        int loops;

        heard = 0;
        loops = 0;
        ambience_stats(&heard, &loops);
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "# song=");
        add_str(line, &n, (int)sizeof(line), music_name()[0] ? music_name() : "none");
        add_str(line, &n, (int)sizeof(line), " ");
        add_flag(line, &n, (int)sizeof(line), "voices", sound_busy());
        add_str(line, &n, (int)sizeof(line), " ");
        add_flag(line, &n, (int)sizeof(line), "sources", ambience_emitters(0));
        add_str(line, &n, (int)sizeof(line), " ");
        add_flag(line, &n, (int)sizeof(line), "heard", heard);
        add_str(line, &n, (int)sizeof(line), " ");
        add_flag(line, &n, (int)sizeof(line), "loops", loops);
        add_str(line, &n, (int)sizeof(line), " ");
        add_flag(line, &n, (int)sizeof(line), "sndview", ambience_view());
        log_line(line);
    }
    /* Where the frame's time went, the last half second's means in ms (docs/profile.md). */
    {
        int i;

        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "# prof ");
        add_num(line, &n, (int)sizeof(line), prof_mhz(), 0);
        add_str(line, &n, (int)sizeof(line), "mhz");
        for (i = 0; i < PROF_STAGES; i++) {
            add_str(line, &n, (int)sizeof(line), " ");
            add_str(line, &n, (int)sizeof(line), prof_name(i));
            add_str(line, &n, (int)sizeof(line), "=");
            add_ms(line, &n, (int)sizeof(line), prof_ms(i));
        }
        log_line(line);
    }
}

static void run_view(const Level *level, Camera *cam)
{
    int help_was;
    int f12_was;
    int i;
    int frames;
    DWORD tick;

    help_was = key_down(VK_F1);
    f12_was = key_down(VK_F12);
    for (i = 0; i < NSWITCH; i++) {
        g_switch_was[i] = key_down(g_switch_vk[i]);
    }
    g_f2_was = key_down(VK_F2);
    g_f4_was = key_down(VK_F4);
    g_f11_was = key_down(VK_F11);
    g_p_was = key_down('P');
    g_go.what = 0;
    interact_set_handler(lf_use);
    frames = 0;
    g_snap.valid = 0;
    g_fps_tick = 0;
    g_fps_frames = 0;
    g_fps_ready = 0;
    g_cam = cam;
    tick = GetTickCount();
    input_clear_quit();
    input_begin(win_hwnd());
    hud_set(0);
    console_set_camera(cam);
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
        /* Before the camera moves: this pose and these counts are the glass. */
        snap_take(cam, frames > 0);
        if (!console_open()) {
            help_now = key_down(VK_F1);
            if (help_now && !help_was) {
                g_help = g_help ? 0 : 1;
            }
            help_was = help_now;
            fly_step(cam, dx, dy, (float)dt / 1000.0f);
        }
        switch_keys(!console_open());
        /* The streamed world loads round the camera before the frame reads the scene. */
        if (g_frame_hook) {
            g_frame_hook(cam);
        }
        gfx_frame(cam, level);
        frames++;
        /* A passage was used in that frame: view_row opens where it leads. */
        if (g_go.what) {
            break;
        }
        if (g_world && world_place(g_world) == PLACE_TOWN && world_season(g_world) != clock_season()) {
            log_line("the season turned: the town again in its pictures");
            g_go.what = LF_RESEASON;
            break;
        }
        /*
         * shot_pump saw F12 inside that frame and wrote the file, so
         * shot_status already names it. Poll right after for the same edge.
         */
        {
            int f12_now;

            f12_now = key_down(VK_F12);
            if (f12_now && !f12_was) {
                bookmark();
            }
            f12_was = f12_now;
        }
    }
    gfx_set_extra(0);
    interact_set_handler(0);
    hud_set(1);
    g_cam = 0;
    console_set_camera(0);
    input_end();
}

/*
 * Where the camera lands in a place a passage opened (docs/viewers.md):
 * the place's own start; outside the street door it went in by; inside,
 * by the way out nearest that door; above a dungeon, by its way down;
 * below, on the start facing away from the nearest way out, as
 * Daggerfall Unity turns the player on coming down.
 */
enum {
    LAND_START,
    LAND_STREET,
    LAND_IN,
    LAND_DOWN,
    LAND_BELOW,
    LAND_STAY
};
/* LAND_STAY: where the camera stood when the season turned. */
static Camera g_stay;
/* Out of a door three body radii, into a house a radius and 0.4 m: Daggerfall Unity's. */
#define LAND_OUT (3.0f * WALK_RADIUS)
#define LAND_INTO (WALK_RADIUS + 16.0f)

static void land_at(const World *world, int land, const Usable *street,
                    float *x, float *y, float *z, float *yaw, float *pitch)
{
    const Usable *u;
    int k;

    world_start(world, x, y, z, yaw, pitch);
    u = 0;
    if (land == LAND_STREET) {
        u = street;
    } else if (land == LAND_IN) {
        k = world_passage(world, DOOR_LEAVE, street->cx, street->cy, street->cz);
        u = k >= 0 ? world_usable(world, k) : 0;
    } else if (land == LAND_DOWN) {
        k = world_passage(world, DOOR_DUNGEON, *x, *y, *z);
        u = k >= 0 ? world_usable(world, k) : 0;
    }
    if (land == LAND_BELOW) {
        k = world_passage(world, DOOR_EXIT, *x, *y, *z);
        if (k >= 0) {
            u = world_usable(world, k);
            *yaw = df_wrap_tau(df_atan2(u->nx, u->nz));
            *pitch = 0.0f;
        }
        return;
    }
    if (u) {
        doors_landing(u, land == LAND_IN ? LAND_INTO : LAND_OUT, WALK_EYE, x, y, z, yaw);
        *pitch = 0.0f;
    }
}

/* A new place through a passage comes up from black over this long. */
#define LAND_FADE 0.6f

/*
 * View list row index, then wherever its passages lead, until Esc. Each
 * place is its own temp heap; the card stays open from one to the next,
 * holding the last frame under "Loading..." while the next one loads
 * (gfx_hold), so a door is not the list and back. A building's inside
 * keeps the town's record (dun), which the street is opened from again.
 */
static void view_row(int index)
{
    Dungeon dun;
    World *world;
    Level level;
    Camera cam;
    Usable street;
    LfGo go;
    int glide_up;
    int row;
    int block;
    int record;
    int land;
    char line[80];
    int n;

    if (index < 0 || index >= g_nrows) {
        return;
    }
    row = index;
    block = -1;
    record = -1;
    land = LAND_START;
    glide_up = 0;
    mem_zero(&street, sizeof(street));
    for (;;) {
        df_copy(g_view_name, g_rows[row].name, (int)sizeof(g_view_name));
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "open ");
        add_u(line, &n, (int)sizeof(line), (df_u32)g_rows[row].region);
        add_str(line, &n, (int)sizeof(line), " ");
        add_str(line, &n, (int)sizeof(line), g_view_name);
        if (block >= 0) {
            add_str(line, &n, (int)sizeof(line), ", building ");
            add_num(line, &n, (int)sizeof(line), record, 0);
            add_str(line, &n, (int)sizeof(line), " of block ");
            add_num(line, &n, (int)sizeof(line), block, 0);
        }
        log_line(line);
        if (!mem_temp_begin()) {
            log_line("temp heap failed");
            if (glide_up) {
                gfx_shutdown();
                glide_up = 0;
            }
            win_fail("Out of memory.");
            break;
        }
        world = 0;
        go.what = 0;
        n = 1;
        /* Inside, dun is still the town's record from the street. */
        if (block < 0) {
            if (g_rows[row].outdoor) {
                n = exterior_load_at(g_rows[row].region, g_rows[row].index, &dun);
            } else {
                n = dungeon_load_at(g_rows[row].region, g_rows[row].index, &dun);
            }
        }
        if (n) {
            world = block >= 0 ? world_load_inside(&dun, block, record) : world_load_dungeon(&dun);
        }
        if (!world) {
            /* The card holds the screen: give it back before Windows says why. */
            if (glide_up) {
                gfx_shutdown();
                glide_up = 0;
            }
            log_line(n ? world_error() : dungeon_error());
            win_fail(n ? world_error() : dungeon_error());
        } else {
            float x, y, z, yaw, pitch;

            world_as_level(world, &level);
            if (!glide_up) {
                show_list(0);
                if (!gfx_startup(win_hwnd())) {
                    log_line(gfx_error());
                    win_fail(gfx_error());
                } else {
                    glide_up = 1;
                    log_line("glide open 640x480");
                }
            }
            if (glide_up && !gfx_prepare(&level)) {
                gfx_shutdown();
                glide_up = 0;
                log_line(gfx_error());
                win_fail(gfx_error());
            } else if (glide_up) {
                land_at(world, land, &street, &x, &y, &z, &yaw, &pitch);
                if (land == LAND_STAY) {
                    x = g_stay.x;
                    y = g_stay.y;
                    z = g_stay.z;
                    yaw = g_stay.yaw;
                    pitch = g_stay.pitch;
                }
                /* cam= from the cfg replaces the start marker, once, for open= only. */
                if (land == LAND_START && g_auto_cam && row == g_auto_row) {
                    x = g_auto_pose[0];
                    y = g_auto_pose[1];
                    z = g_auto_pose[2];
                    yaw = g_auto_pose[3] * (DF_TAU / 360.0f);
                    pitch = g_auto_pose[4] * (DF_TAU / 360.0f);
                    g_auto_cam = 0;
                    log_line("cam from levelfall.cfg");
                }
                camera_set(&cam, x, y, z, yaw, pitch);
                if (land != LAND_START) {
                    gfx_fade_in(LAND_FADE);
                }
                g_region = g_rows[row].region;
                g_dun = &dun;
                g_world = world;
                g_ref = &g_rows[row];
                g_row_now = row;
                g_inside = block >= 0;
                run_view(&level, &cam);
                go = g_go;
                g_go.what = 0;
                /* The list is quiet, and the next place has its own song. */
                music_stop();
                sound_stop_all();
                g_ref = 0;
                g_dun = 0;
                g_world = 0;
                g_row_now = -1;
                g_inside = 0;
            }
        }
        /* Through a passage the card stays open, holding this frame while the next place loads. */
        if (go.what && glide_up) {
            gfx_hold("Loading...");
        }
        texture_cache_clear();
        mem_temp_end();
        /* Where the passage leads, if one was used; Esc and a failed load go back to the list. */
        if (go.what == LF_RESEASON) {
            g_stay = cam;
            land = LAND_STAY;
        } else if (go.what == DOOR_ENTER) {
            street = go.door;
            block = go.tag >> 8;
            record = go.tag & 0xff;
            land = LAND_IN;
        } else if (go.what == DOOR_LEAVE) {
            block = -1;
            land = LAND_STREET;
        } else if (go.what == DOOR_DUNGEON || go.what == DOOR_EXIT) {
            row = pair_row(row, go.what == DOOR_EXIT);
            block = -1;
            land = go.what == DOOR_EXIT ? LAND_DOWN : LAND_BELOW;
        } else {
            break;
        }
        if (row < 0) {
            break;
        }
    }
    if (glide_up) {
        gfx_shutdown();
    }
    show_list(1);
    input_clear_quit();
}

/* The stream's share of a frame on the 233, to start with (docs/streaming.md, docs/profile.md). */
#define STREAM_BUDGET_MS 4.0f

/* The radius of chunks the far clip needs: 4096 a ring, at least one. */
static int stream_radius_for_far(void)
{
    int r;

    r = (df_iround(gfx_far()) + STREAM_CHUNK - 1) / STREAM_CHUNK;
    return r < 1 ? 1 : (r > STREAM_RADIUS_MAX ? STREAM_RADIUS_MAX : r);
}

/* Each frame of the stream: its radius from the far clip, its loading, and where the camera is. */
static void stream_frame(Camera *cam)
{
    int px;
    int py;

    stream_set_radius(stream_radius_for_far());
    wild_update(cam, STREAM_BUDGET_MS);
    wild_here(cam, &px, &py);
    if (px != g_stream_px || py != g_stream_py) {
        int region;
        int index;
        int i;
        int n;

        g_stream_px = px;
        g_stream_py = py;
        n = 0;
        g_stream_where[0] = 0;
        /* A pixel's location by name: once when the pixel changes, a pass over the list. */
        if (worldmap_location(px, py, &region, &index)) {
            for (i = 0; i < g_nrows; i++) {
                if (g_rows[i].outdoor && g_rows[i].region == region && g_rows[i].index == index) {
                    add_str(g_stream_where, &n, (int)sizeof(g_stream_where), g_rows[i].name);
                    break;
                }
            }
        }
        if (!g_stream_where[0]) {
            add_str(g_stream_where, &n, (int)sizeof(g_stream_where), "wilderness");
        }
        n = 0;
        g_view_name[0] = 0;
        add_str(g_view_name, &n, (int)sizeof(g_view_name), "world ");
        add_num(g_view_name, &n, (int)sizeof(g_view_name), px, 0);
        add_str(g_view_name, &n, (int)sizeof(g_view_name), ",");
        add_num(g_view_name, &n, (int)sizeof(g_view_name), py, 0);
        add_str(g_view_name, &n, (int)sizeof(g_view_name), "  ");
        add_str(g_view_name, &n, (int)sizeof(g_view_name), g_stream_where);
    }
}

/*
 * "Stream World": the row's map pixel as the streamed world, from its
 * middle (an indoor row, its outside's), until Esc. Phase 1: the
 * wilderness, flat, no buildings (docs/streaming.md). One temp heap and
 * one Glide open for the whole walk.
 */
static void view_stream(int index)
{
    Level level;
    Camera cam;
    float x, y, z;
    int row;
    int px;
    int py;
    int glide_up;

    row = index;
    if (row >= 0 && row < g_nrows && !g_rows[row].outdoor) {
        row = pair_row(row, 1);
    }
    if (row < 0 || row >= g_nrows) {
        win_fail("Stream World: this place has no outside on the map.");
        return;
    }
    if (!mem_temp_begin()) {
        win_fail("Out of memory.");
        return;
    }
    glide_up = 0;
    if (!worldmap_open() || !worldmap_pixel_of(g_rows[row].region, g_rows[row].index, &px, &py)) {
        log_line("stream: the world map did not read, or the place is not on it");
        win_fail("Stream World: the world map did not read.");
    } else if (!wild_open(px, py, stream_radius_for_far(), &level, &x, &y, &z)) {
        log_line("stream: the wilderness did not open");
        win_fail("Stream World: the wilderness did not open.");
    } else {
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
                char line[64];
                int n;

                n = 0;
                line[0] = 0;
                add_str(line, &n, (int)sizeof(line), "stream from pixel ");
                add_num(line, &n, (int)sizeof(line), px, 0);
                add_str(line, &n, (int)sizeof(line), ",");
                add_num(line, &n, (int)sizeof(line), py, 0);
                log_line(line);
                camera_set(&cam, x, y, z, 0.0f, 0.0f);
                g_streaming = 1;
                g_stream_px = -1;
                g_stream_py = -1;
                g_frame_hook = stream_frame;
                stream_frame(&cam);
                g_region = g_rows[row].region;
                run_view(&level, &cam);
                g_frame_hook = 0;
                g_streaming = 0;
                music_stop();
                sound_stop_all();
            }
        }
        wild_close();
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

/* A whole number lo..hi into dst. Anything else leaves dst as it was. */
static void cfg_int(const char *rest, int lo, int hi, int *dst)
{
    int v;

    if (!read_int(&rest, &v) || v < lo || v > hi) {
        return;
    }
    *dst = v;
}

/* An optional minus, digits, and an optional point with more digits. */
static int read_fixed(const char **ps, float *out)
{
    const char *s;
    float v;
    float place;
    int neg;
    int any;

    s = skip_space(*ps);
    neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    v = 0.0f;
    any = 0;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        if (v < 10000000.0f) {
            v = v * 10.0f + (float)(*s - '0');
        }
        s++;
    }
    if (*s == '.') {
        s++;
        place = 0.1f;
        while (*s >= '0' && *s <= '9') {
            any = 1;
            v += place * (float)(*s - '0');
            place *= 0.1f;
            s++;
        }
    }
    if (!any) {
        return 0;
    }
    *out = neg ? -v : v;
    *ps = s;
    return 1;
}

/* A word followed by the end, a comma, or a space. */
static int read_word(const char **ps, const char *word)
{
    const char *s;
    int i;

    s = skip_space(*ps);
    for (i = 0; word[i]; i++) {
        if (fold_ch((unsigned char)s[i]) != word[i]) {
            return 0;
        }
    }
    if (s[i] && s[i] != ',' && s[i] != ' ' && s[i] != '\t' && s[i] != '\r') {
        return 0;
    }
    *ps = s + i;
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
        return;
    }
    if (key_is(s, "fov", &rest)) {
        if (!read_int(&rest, &v) || v < 1) {
            return;
        }
        g_fov_cfg = v;
        return;
    }
    if (key_is(s, "time", &rest)) {
        int m;

        if (!read_int(&rest, &v) || v > 23) {
            return;
        }
        /* time=21 is the hour alone. time=21:30 adds the minute. */
        m = 0;
        if (*rest == ':') {
            rest++;
            if (!read_int(&rest, &m) || m > 59) {
                return;
            }
        }
        g_time_cfg = v * 60 + m;
        return;
    }
    if (key_is(s, "soundpanel", &rest)) {
        cfg_int(rest, 0, 1, &g_sound_panel);
        return;
    }
    if (key_is(s, "profpanel", &rest)) {
        cfg_int(rest, 0, 1, &g_prof_panel);
        return;
    }
    if (key_is(s, "sndrows", &rest)) {
        cfg_int(rest, 0, SND_ROWS_MAX, &g_snd_rows);
        return;
    }
    if (key_is(s, "sndview", &rest)) {
        cfg_int(rest, 0, AMBIENCE_VIEW_MAX, &g_sndview_cfg);
        return;
    }
    if (key_is(s, "sound", &rest)) {
        cfg_int(rest, 0, 1, &g_sound_cfg);
        return;
    }
    if (key_is(s, "sndvol", &rest)) {
        cfg_int(rest, 0, 100, &g_sndvol_cfg);
        return;
    }
    if (key_is(s, "music", &rest)) {
        cfg_int(rest, 0, 1, &g_music_cfg);
        return;
    }
    if (key_is(s, "musvol", &rest)) {
        cfg_int(rest, 0, 100, &g_musvol_cfg);
        return;
    }
    if (key_is(s, "date", &rest)) {
        int d;
        int m;
        int y;

        if (!read_int(&rest, &d) || !next_number(&rest) || !read_int(&rest, &m)
            || !next_number(&rest) || !read_int(&rest, &y)
            || d < 1 || d > CLOCK_MONTH_DAYS || m < 1 || m > CLOCK_MONTHS || y > 9999) {
            return;
        }
        g_day_cfg = y * CLOCK_YEAR_DAYS + (m - 1) * CLOCK_MONTH_DAYS + (d - 1);
        return;
    }
    if (key_is(s, "season", &rest)) {
        for (v = 0; v < 4; v++) {
            const char *name;
            int i;

            name = clock_season_name(v);
            for (i = 0; name[i] && fold_ch((unsigned char)rest[i]) == name[i]; i++) {
            }
            if (!name[i]) {
                g_season_cfg = v;
                break;
            }
        }
        return;
    }
    if (key_is(s, "weather", &rest)) {
        char word[16];
        int i;

        /* The value is one word; weather_by_name wants it on its own. */
        for (i = 0; i < 15 && rest[i] && rest[i] != ' ' && rest[i] != '\t'
                    && rest[i] != '\r' && rest[i] != '#'; i++) {
            word[i] = (char)fold_ch((unsigned char)rest[i]);
        }
        word[i] = 0;
        v = weather_by_name(word);
        if (v >= 0 && v < WEATHER_KINDS) {
            g_weather_cfg = v;
        }
        return;
    }
    if (key_is(s, "open", &rest)) {
        int kind;

        if (read_word(&rest, "indoor")) {
            kind = 0;
        } else if (read_word(&rest, "outdoor")) {
            kind = 1;
        } else {
            return;
        }
        if (!next_number(&rest) || !read_int(&rest, &r)
            || !next_number(&rest) || !read_int(&rest, &v)) {
            return;
        }
        g_auto_kind = kind;
        g_auto_region = r;
        g_auto_index = v;
        return;
    }
    if (key_is(s, "cam", &rest)) {
        float pose[5];
        int i;

        for (i = 0; i < 5; i++) {
            if (i > 0 && !next_number(&rest)) {
                return;
            }
            if (!read_fixed(&rest, &pose[i])) {
                return;
            }
        }
        for (i = 0; i < 5; i++) {
            g_auto_pose[i] = pose[i];
        }
        g_auto_cam = 1;
    }
}

/*
 * The engine's sound switches from the cfg, once the card and the
 * synthesizer are open, so a volume reaches the device that plays it.
 */
static void sound_cfg_apply(void)
{
    char line[64];
    int n;

    if (g_sound_cfg >= 0) {
        sound_set_on(g_sound_cfg);
    }
    if (g_sndvol_cfg >= 0) {
        sound_set_volume(g_sndvol_cfg);
    }
    if (g_music_cfg >= 0) {
        music_set_on(g_music_cfg);
    }
    if (g_musvol_cfg >= 0) {
        music_set_volume(g_musvol_cfg);
    }
    n = 0;
    line[0] = 0;
    add_flag(line, &n, (int)sizeof(line), "sound", sound_on());
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "sndvol", sound_volume());
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "music", music_on());
    add_str(line, &n, (int)sizeof(line), " ");
    add_flag(line, &n, (int)sizeof(line), "musvol", music_volume());
    log_line(line);
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
    if (g_fov_cfg > 0) {
        camera_set_fov((float)g_fov_cfg);
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "fov=");
        add_u(line, &n, (int)sizeof(line), (df_u32)camera_fov());
        log_line(line);
    }
    if (g_day_cfg >= 0) {
        clock_set_day(g_day_cfg);
        log_line("date= from levelfall.cfg");
    } else if (g_season_cfg >= 0) {
        clock_set_season(g_season_cfg);
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "season=");
        add_str(line, &n, (int)sizeof(line), clock_season_name(g_season_cfg));
        log_line(line);
    }
    if (g_time_cfg >= 0) {
        clock_set_minute((float)g_time_cfg);
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "time=");
        add_hm(line, &n, (int)sizeof(line), g_time_cfg / 60, g_time_cfg % 60);
        log_line(line);
    }
    ambience_set_view(g_sndview_cfg);
    /* Set, so it is there with no blend, then forced, so no roll replaces it. */
    if (g_weather_cfg >= 0) {
        weather_set(g_weather_cfg);
        weather_force(g_weather_cfg);
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "weather=");
        add_str(line, &n, (int)sizeof(line), weather_name(g_weather_cfg));
        log_line(line);
    }
    if (g_auto_kind >= 0) {
        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), g_auto_kind ? "open=outdoor," : "open=indoor,");
        add_u(line, &n, (int)sizeof(line), (df_u32)g_auto_region);
        add_str(line, &n, (int)sizeof(line), ",");
        add_u(line, &n, (int)sizeof(line), (df_u32)g_auto_index);
        log_line(line);
    }
}

static void cfg_load(void)
{
    static const char text[] =
        "# background=R,G,B, each 0 to 255\r\n"
        "background=128,128,128\r\n"
        "# fog=1 uses the distance table, fog=0 turns it off\r\n"
        "fog=1\r\n"
        "# far is the clip distance in world units, at most 16384\r\n"
        "far=8192\r\n"
        "# fov is the horizontal field of view in degrees, 45 to 120\r\n"
        "fov=75\r\n"
        "# open=outdoor,17,1231 opens that place at start: indoor or outdoor,\r\n"
        "# the region, the index. cam=x,y,z,yaw,pitch (degrees) puts the\r\n"
        "# camera there. time=21:30 sets the world clock at start (it lights\r\n"
        "# outdoor places). weather=rain starts in that weather and holds it:\r\n"
        "# sunny cloudy overcast fog rain thunder snow. date=1,6,405 (day,\r\n"
        "# month, year) or season=winter sets the calendar at start: a town\r\n"
        "# is drawn in its season. F12 in the view logs these lines to\r\n"
        "# levelfall.log.\r\n"
        "# profpanel=1 shows the frame profile at start (P toggles it).\r\n"
        "# soundpanel=1 shows the sound panel at start (F2 toggles it).\r\n"
        "# sndrows is how many of the nearest sound sources it lists, 0 to 8.\r\n"
        "soundpanel=1\r\n"
        "sndrows=5\r\n"
        "# sndview draws the sources within that many units, 0 (off) to\r\n"
        "# 4096; F4 steps it. sound and music are 1 or 0, sndvol and\r\n"
        "# musvol 0 to 100. Left out, the engine's own defaults stay.\r\n"
        "sndview=0\r\n";
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
    g_fov_cfg = 0;
    g_time_cfg = -1;
    g_weather_cfg = -1;
    g_day_cfg = -1;
    g_season_cfg = -1;
    g_auto_kind = -1;
    g_auto_cam = 0;
    g_sound_panel = 1;
    g_prof_panel = 0;
    g_snd_rows = 5;
    g_sndview_cfg = 0;
    g_sound_cfg = -1;
    g_sndvol_cfg = -1;
    g_music_cfg = -1;
    g_musvol_cfg = -1;
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

/*
 * open= from levelfall.cfg: select that row in the list and fly it, so a
 * bookmark reopens without a search. Esc from the view lands on the list.
 */
static void open_auto(void)
{
    int i;
    int row;

    row = -1;
    for (i = 0; i < g_nrows; i++) {
        if (g_rows[i].outdoor == g_auto_kind && g_rows[i].region == g_auto_region
            && g_rows[i].index == g_auto_index) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        log_line("open=: no such place");
        g_auto_cam = 0;
        return;
    }
    for (i = 0; i < g_nview; i++) {
        if (g_view[i] == row) {
            select_row(i);
            break;
        }
    }
    g_auto_row = row;
    view_row(row);
    g_auto_cam = 0;
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
    log_line("view: mouse looks, WASD flies along the aim, Left/Right turn, Shift is faster, Esc returns");
    if (g_nrows < 1) {
        win_fail(dungeon_error()[0] ? dungeon_error() : "MAPS.BSA has no places.");
    } else {
        /* Once for the program: each place plays its own sound in it (docs/sound.md). */
        sound_startup(win_hwnd());
        music_startup();
        sound_cfg_apply();
        if (g_auto_kind >= 0) {
            open_auto();
        }
        while (!win_closed() && wait_list()) {
            int idx;

            idx = selected_row();
            if (idx < 0) {
                continue;
            }
            if (SendMessageA(g_stream_box, BM_GETCHECK_, 0, 0) == BST_CHECKED_) {
                view_stream(idx);
            } else {
                view_row(idx);
            }
        }
        music_shutdown();
        sound_shutdown();
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
