/*
 * renderfall: pick one ARCH3D record, draw it with Glide.
 * The list is the BSA directory (numeric ids). Models are not loose files.
 * A face uses the texture archive and record stored on that face
 * (tex >> 7, tex & 0x7f). Dungeon climate remaps are not applied here;
 * those belong to a placed block, not to the model.
 * Esc on the list quits. Esc in the view returns to the list.
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
#include "engine/scene/scene.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/bsa.h"
#include "game/dagger/fnt.h"
#include "game/dagger/palette.h"
#include "game/dagger/texture.h"
#include "engine/ui/font.h"

#define MAX_TEX 256
#define PITCH_MAX 1.2f
#define FIT_RADIUS 200.0f

#define ID_LIST 1
#define ID_OPEN 2

/* SysListView32, report view. comctl32 is loaded at runtime, not imported. */
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
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVNI_SELECTED 0x0002
#define NM_DBLCLK (-3)
#define LVN_COLUMNCLICK (-108)

static HANDLE g_log = INVALID_HANDLE_VALUE;
static HWND g_list;
static HWND g_label;
static HWND g_open;
static int g_sort_mode;
static int g_open_req;

static struct Row {
    df_u32 id;
    df_u32 size;
    int index;
} *g_rows;
static int g_nrows;

static int g_stats = 1;
static df_u32 g_stat_id;
static df_u32 g_stat_size;
static int g_stat_tris;
static int g_stat_tex;
static int g_stat_miss;
static Palette g_pal;
static Image *g_images[MAX_TEX];
static df_u8 g_cutout[MAX_TEX];
static df_u16 g_key_file[MAX_TEX];
static df_u16 g_key_image[MAX_TEX];

static int g_bg_r = 48;
static int g_bg_g = 48;
static int g_bg_b = 48;
static int g_grid = 1;
static const Camera *g_frame_cam;
static float g_grid_cx, g_grid_cy, g_grid_cz, g_grid_r;

static int g_left;
static int g_right;
static int g_lx, g_ly;
static int g_dx, g_dy;
static int g_rz;

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
    name = "\\renderfall.log";
    n = slash;
    for (i = 0; name[i] && n < 259; i++) {
        path[n++] = name[i];
    }
    path[n] = 0;
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
}

static float xsqrt(float v)
{
    float r;

    if (v <= 0.0f) {
        return 0.0f;
    }
    __asm__ __volatile__("fsqrt" : "=t"(r) : "0"(v));
    return r;
}

static int mesh_bounds(const Mesh *mesh, float *cx, float *cy, float *cz, float *radius)
{
    float minx, miny, minz, maxx, maxy, maxz;
    float r2;
    int t;
    int k;

    if (!mesh || mesh->ntris < 1 || !mesh->tris) {
        return 0;
    }
    minx = maxx = mesh->tris[0].x[0];
    miny = maxy = mesh->tris[0].y[0];
    minz = maxz = mesh->tris[0].z[0];
    for (t = 0; t < mesh->ntris; t++) {
        for (k = 0; k < 3; k++) {
            float x, y, z;

            x = mesh->tris[t].x[k];
            y = mesh->tris[t].y[k];
            z = mesh->tris[t].z[k];
            if (x < minx) minx = x;
            if (y < miny) miny = y;
            if (z < minz) minz = z;
            if (x > maxx) maxx = x;
            if (y > maxy) maxy = y;
            if (z > maxz) maxz = z;
        }
    }
    *cx = (minx + maxx) * 0.5f;
    *cy = (miny + maxy) * 0.5f;
    *cz = (minz + maxz) * 0.5f;
    r2 = 0.0f;
    for (t = 0; t < mesh->ntris; t++) {
        for (k = 0; k < 3; k++) {
            float dx, dy, dz, d2;

            dx = mesh->tris[t].x[k] - *cx;
            dy = mesh->tris[t].y[k] - *cy;
            dz = mesh->tris[t].z[k] - *cz;
            d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > r2) {
                r2 = d2;
            }
        }
    }
    *radius = xsqrt(r2);
    if (*radius < 1.0f) {
        *radius = 1.0f;
    }
    return 1;
}

static void place_instance(Instance *in, Mesh *mesh,
                           float cx, float cy, float cz, float scale)
{
    int i;

    instance_set_rotation(in, 0.0f, 0.0f, 0.0f);
    for (i = 0; i < 9; i++) {
        in->m[i] *= scale;
    }
    in->x = cx * (1.0f - scale);
    in->y = cy * (1.0f - scale);
    in->z = cz * (1.0f - scale);
    in->mesh = mesh;
}

static float fit_dist(float radius)
{
    float dist;

    dist = radius * (DF_FOCAL / ((float)DF_VIEW_H * 0.5f)) * 1.35f + 16.0f;
    if (dist < radius + 8.0f) {
        dist = radius + 8.0f;
    }
    return dist;
}

static void place_camera(Camera *cam, float cx, float cy, float cz,
                         float yaw, float pitch, float dist)
{
    float sy, cyaw, sp, cp;

    sy = df_sin(yaw);
    cyaw = df_cos(yaw);
    sp = df_sin(pitch);
    cp = df_cos(pitch);
    /*
     * Look direction yaw is (sin yaw, cos yaw). The camera sits on the
     * opposite side of the bounding-box center, so the model stays in
     * the middle while yaw and pitch change.
     */
    camera_set(cam,
               cx - dist * cp * sy,
               cy + dist * sp,
               cz - dist * cp * cyaw,
               yaw,
               -pitch);
}

static void on_mouse(unsigned int msg, int x, int y)
{
    int dx;
    int dy;

    if (msg == WM_LBUTTONDOWN) {
        g_left = 1;
        g_lx = x;
        g_ly = y;
        return;
    }
    if (msg == WM_LBUTTONUP) {
        g_left = 0;
        return;
    }
    if (msg == WM_RBUTTONDOWN) {
        g_right = 1;
        g_lx = x;
        g_ly = y;
        return;
    }
    if (msg == WM_RBUTTONUP) {
        g_right = 0;
        return;
    }
    if (msg != WM_MOUSEMOVE) {
        return;
    }
    dx = x - g_lx;
    dy = y - g_ly;
    g_lx = x;
    g_ly = y;
    if (g_left) {
        g_dx += dx;
        g_dy += dy;
    }
    if (g_right) {
        g_rz += dy;
    }
}

static int find_tex(int n, df_u16 file, df_u16 image)
{
    int i;

    for (i = 0; i < n; i++) {
        if (g_key_file[i] == file && g_key_image[i] == image) {
            return i;
        }
    }
    return -1;
}

static void log_miss(int file, int image)
{
    char line[64];
    char name[16];
    int n;

    texture_filename(file, name, (int)sizeof(name));
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "missing ");
    add_str(line, &n, (int)sizeof(line), name);
    add_str(line, &n, (int)sizeof(line), " ");
    add_u(line, &n, (int)sizeof(line), (df_u32)image);
    log_line(line);
}

static int bind_textures(Mesh *mesh, int *nimages, int *missing)
{
    int t;
    int n;
    int miss;

    n = 0;
    miss = 0;
    for (t = 0; t < mesh->ntris; t++) {
        Tri *tri;
        int slot;
        Image *img;

        tri = &mesh->tris[t];
        slot = find_tex(n, tri->tex_file, tri->tex_image);
        if (slot < 0) {
            if (n >= MAX_TEX) {
                int j;

                log_line("texture cap");
                slot = 0;
                for (j = 0; j < n; j++) {
                    if (!g_images[j]) {
                        slot = j;
                        break;
                    }
                }
            } else {
                img = texture_load((int)tri->tex_file, (int)tri->tex_image);
                g_key_file[n] = tri->tex_file;
                g_key_image[n] = tri->tex_image;
                g_cutout[n] = 0;
                if (img) {
                    g_images[n] = img;
                } else {
                    g_images[n] = 0;
                    miss++;
                    log_miss((int)tri->tex_file, (int)tri->tex_image);
                }
                slot = n;
                n++;
            }
        }
        tri->slot = (df_u16)slot;
    }
    *nimages = n;
    *missing = miss;
    return n > 0;
}

static int row_before(const struct Row *a, const struct Row *b)
{
    df_u32 ka;
    df_u32 kb;
    int desc;

    if (g_sort_mode < 2) {
        ka = a->id;
        kb = b->id;
        desc = g_sort_mode == 1;
    } else {
        ka = a->size;
        kb = b->size;
        desc = g_sort_mode == 3;
    }
    if (ka != kb) {
        if (desc) {
            return ka > kb;
        }
        return ka < kb;
    }
    if (a->id != b->id) {
        return a->id < b->id;
    }
    return a->index < b->index;
}

static void sort_rows(void)
{
    int gap;

    gap = g_nrows / 2;
    while (gap > 0) {
        int i;

        for (i = gap; i < g_nrows; i++) {
            struct Row tmp;
            int j;

            tmp = g_rows[i];
            j = i;
            while (j >= gap && row_before(&tmp, &g_rows[j - gap])) {
                g_rows[j] = g_rows[j - gap];
                j -= gap;
            }
            g_rows[j] = tmp;
        }
        gap /= 2;
    }
}

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

typedef struct RfNotify {
    HWND hwndFrom;
    unsigned int idFrom;
    unsigned int code;
    int iItem;
    int iSubItem;
} RfNotify;

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
    if (!g_list) {
        return;
    }
    if (g_sort_mode == 0) {
        set_column(0, "Id  +", 0, 0);
        set_column(1, "Bytes", 0, 0);
    } else if (g_sort_mode == 1) {
        set_column(0, "Id  -", 0, 0);
        set_column(1, "Bytes", 0, 0);
    } else if (g_sort_mode == 2) {
        set_column(0, "Id", 0, 0);
        set_column(1, "Bytes  +", 0, 0);
    } else {
        set_column(0, "Id", 0, 0);
        set_column(1, "Bytes  -", 0, 0);
    }
}

static int selected_record(void)
{
    LvItem item;
    int i;

    if (!g_list) {
        return -1;
    }
    i = (int)SendMessageA(g_list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (i < 0) {
        return -1;
    }
    mem_zero(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = i;
    if (!SendMessageA(g_list, LVM_GETITEMA, 0, (LPARAM)&item)) {
        return -1;
    }
    return (int)item.lParam;
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

static void refill_list(void)
{
    int keep;
    int i;
    int sel;

    if (!g_list || !g_rows) {
        return;
    }
    keep = selected_record();
    sort_rows();
    sort_caption();
    SendMessageA(g_list, LVM_DELETEALLITEMS, 0, 0);
    sel = 0;
    for (i = 0; i < g_nrows; i++) {
        char id[16];
        char sz[16];
        int n;
        LvItem item;

        n = 0;
        id[0] = 0;
        add_u(id, &n, (int)sizeof(id), g_rows[i].id);
        n = 0;
        sz[0] = 0;
        add_u(sz, &n, (int)sizeof(sz), g_rows[i].size);
        mem_zero(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.pszText = id;
        item.lParam = g_rows[i].index;
        if (SendMessageA(g_list, LVM_INSERTITEMA, 0, (LPARAM)&item) < 0) {
            break;
        }
        item.mask = LVIF_TEXT;
        item.iSubItem = 1;
        item.pszText = sz;
        SendMessageA(g_list, LVM_SETITEMA, 0, (LPARAM)&item);
        if (g_rows[i].index == keep) {
            sel = i;
        }
    }
    if (g_nrows > 0) {
        select_row(sel);
    }
}

static void sort_flip(int which)
{
    if (which == 0) {
        g_sort_mode = (g_sort_mode == 0) ? 1 : 0;
    } else {
        g_sort_mode = (g_sort_mode == 2) ? 3 : 2;
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
    g_label = CreateWindowExA(
        0, "STATIC", "ARCH3D     Enter or double-click opens     Esc quits",
        WS_CHILD | WS_VISIBLE,
        8, 8, rc.right - 16, 16,
        hwnd, 0, inst, 0);
    y = rc.bottom - 32;
    width = rc.right - 16;
    g_list = CreateWindowExA(
        WS_EX_CLIENTEDGE, "SysListView32", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        8, 28, width, y - 36,
        hwnd, (HMENU)ID_LIST, inst, 0);
    g_open = CreateWindowExA(
        0, "BUTTON", "&Open",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        8, y, 75, 23,
        hwnd, (HMENU)ID_OPEN, inst, 0);
    if (!g_label || !g_list || !g_open) {
        return 0;
    }
    SendMessageA(g_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    set_column(0, "Id  +", width / 2, 1);
    set_column(1, "Bytes", width - width / 2 - 4, 1);
    apply_font(g_label);
    apply_font(g_list);
    apply_font(g_open);
    return 1;
}

static int fill_list(Bsa *bsa)
{
    int n;
    int i;

    n = bsa_count(bsa);
    if (n < 1) {
        return 0;
    }
    g_rows = (struct Row *)mem_alloc((df_u32)n * (df_u32)sizeof(struct Row));
    if (!g_rows) {
        return 0;
    }
    g_nrows = 0;
    for (i = 0; i < n; i++) {
        const BsaRec *rec;

        rec = bsa_at(bsa, i);
        if (!rec) {
            break;
        }
        g_rows[g_nrows].id = rec->id;
        g_rows[g_nrows].size = rec->size;
        g_rows[g_nrows].index = i;
        g_nrows++;
    }
    g_sort_mode = 0;
    refill_list();
    return g_nrows;
}

static void show_list(int on)
{
    int cmd;

    cmd = on ? SW_SHOW : SW_HIDE;
    ShowWindow(g_label, cmd);
    ShowWindow(g_list, cmd);
    ShowWindow(g_open, cmd);
    if (on) {
        SetFocus(g_list);
    } else if (win_hwnd()) {
        SetFocus((HWND)win_hwnd());
    }
}

static void focus_step(int back)
{
    HWND order[2];
    HWND cur;
    int n;
    int i;
    int j;

    order[0] = g_list;
    order[1] = g_open;
    n = 2;
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

static void on_command(unsigned int id, unsigned int code)
{
    if (code == BN_CLICKED && id == ID_OPEN) {
        g_open_req = 1;
    }
}

static void on_notify(void *hdr)
{
    RfNotify *nm;

    nm = (RfNotify *)hdr;
    if (!nm || nm->idFrom != ID_LIST) {
        return;
    }
    if ((int)nm->code == NM_DBLCLK) {
        g_open_req = 1;
        return;
    }
    if ((int)nm->code == LVN_COLUMNCLICK) {
        sort_flip(nm->iSubItem == 0 ? 0 : 1);
    }
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
    SetFocus(g_list);
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
            win_command_hook(0);
            win_notify_hook(0);
            return 0;
        }
        esc_was = esc_now;
        enter_now = key_down(VK_RETURN);
        if (enter_now && !enter_was) {
            win_command_hook(0);
            win_notify_hook(0);
            return 1;
        }
        enter_was = enter_now;
        Sleep(10);
    }
}

static int g_help;

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

static void put_line(char store[][48], const char **lines, int *n, int cap, const char *s)
{
    int i;

    if (*n >= cap) {
        return;
    }
    i = 0;
    while (s[i] && i < 47) {
        store[*n][i] = s[i];
        i++;
    }
    store[*n][i] = 0;
    lines[*n] = store[*n];
    *n += 1;
}

static void draw_stats(void)
{
    char store[20][48];
    const char *lines[20];
    char line[48];
    int n;
    int len;
    int i;
    int shown;

    if (!g_stats) {
        return;
    }
    n = 0;
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "id ");
    add_u(line, &len, (int)sizeof(line), g_stat_id);
    put_line(store, lines, &n, 20, line);
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "triangles ");
    add_u(line, &len, (int)sizeof(line), (df_u32)g_stat_tris);
    put_line(store, lines, &n, 20, line);
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "vertices ");
    add_u(line, &len, (int)sizeof(line), (df_u32)g_stat_tris * 3u);
    put_line(store, lines, &n, 20, line);
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "bytes ");
    add_u(line, &len, (int)sizeof(line), g_stat_size);
    put_line(store, lines, &n, 20, line);
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "textures ");
    add_u(line, &len, (int)sizeof(line), (df_u32)g_stat_tex);
    add_str(line, &len, (int)sizeof(line), "  missing ");
    add_u(line, &len, (int)sizeof(line), (df_u32)g_stat_miss);
    put_line(store, lines, &n, 20, line);
    shown = g_stat_tex;
    if (shown > 8) {
        shown = 8;
    }
    for (i = 0; i < shown; i++) {
        char name[16];

        texture_filename((int)g_key_file[i], name, (int)sizeof(name));
        len = 0;
        line[0] = 0;
        add_str(line, &len, (int)sizeof(line), name);
        add_str(line, &len, (int)sizeof(line), " ");
        add_u(line, &len, (int)sizeof(line), g_key_image[i]);
        if (!g_images[i]) {
            add_str(line, &len, (int)sizeof(line), " missing");
        }
        put_line(store, lines, &n, 20, line);
    }
    if (g_stat_tex > shown) {
        put_line(store, lines, &n, 20, "...");
    }
    draw_lines(lines, n, -1, DF_VIEW_H - 48);
}

static void draw_help(void)
{
    static const char *lines[] = {
        "arrows or left mouse: pan",
        "right mouse or w s: zoom",
        "r: reset",
        "g: grid",
        "f1: hide this",
        "f2: stats",
        "f3: fps",
        "esc: back to the list"
    };
    int nlines;
    int line_h;

    if (!g_help) {
        return;
    }
    nlines = (int)(sizeof(lines) / sizeof(lines[0]));
    line_h = ui_line_height() + 2;
    draw_lines(lines, nlines, 8, 12 + nlines * line_h);
}

static void draw_overlay(void)
{
    draw_stats();
    draw_help();
}

#define GRID_CLIP (1600.0f / DF_FOCAL)

static float grid_side(int plane, float x, float y, float z)
{
    switch (plane) {
    case 0:
        return z - DF_NEAR;
    case 1:
        return DF_FAR - z;
    case 2:
        return z * GRID_CLIP + x;
    case 3:
        return z * GRID_CLIP - x;
    case 4:
        return z * GRID_CLIP + y;
    default:
        return z * GRID_CLIP - y;
    }
}

static int clip_seg(float *x0, float *y0, float *z0, float *x1, float *y1, float *z1)
{
    int p;

    for (p = 0; p < 6; p++) {
        float s0;
        float s1;
        float t;
        float hx, hy, hz;

        s0 = grid_side(p, *x0, *y0, *z0);
        s1 = grid_side(p, *x1, *y1, *z1);
        if (s0 < 0.0f && s1 < 0.0f) {
            return 0;
        }
        if (s0 >= 0.0f && s1 >= 0.0f) {
            continue;
        }
        t = s0 / (s0 - s1);
        hx = *x0 + (*x1 - *x0) * t;
        hy = *y0 + (*y1 - *y0) * t;
        hz = *z0 + (*z1 - *z0) * t;
        if (s0 < 0.0f) {
            *x0 = hx;
            *y0 = hy;
            *z0 = hz;
        } else {
            *x1 = hx;
            *y1 = hy;
            *z1 = hz;
        }
    }
    return 1;
}

static void grid_line(const Camera *cam, const Basis *basis,
                      float x0, float y0, float z0,
                      float x1, float y1, float z1,
                      float cr, float cg, float cb)
{
    float vx0, vy0, vz0;
    float vx1, vy1, vz1;
    DrawVert a, dv;

    camera_view(cam, basis, x0, y0, z0, &vx0, &vy0, &vz0);
    camera_view(cam, basis, x1, y1, z1, &vx1, &vy1, &vz1);
    if (!clip_seg(&vx0, &vy0, &vz0, &vx1, &vy1, &vz1)) {
        return;
    }
    if (!camera_project(vx0, vy0, vz0, &a.x, &a.y, &a.oow, &a.ooz)) {
        return;
    }
    if (!camera_project(vx1, vy1, vz1, &dv.x, &dv.y, &dv.oow, &dv.ooz)) {
        return;
    }
    a.sow = 0.0f;
    a.tow = 0.0f;
    a.r = cr;
    a.g = cg;
    a.b = cb;
    dv.sow = 0.0f;
    dv.tow = 0.0f;
    dv.r = cr;
    dv.g = cg;
    dv.b = cb;
    glide_line(&a, &dv);
}

static float lift_ch(int c)
{
    if (c < 180) {
        return (float)(c + (255 - c) / 3);
    }
    return (float)(c / 2);
}

static void grid_plane(const Camera *cam, const Basis *basis, int axis,
                       float r, float g, float b)
{
    float step;
    float ext;
    int n;
    int i;

    step = 1.0f;
    while (g_grid_r * 2.0f / step > 12.0f && step < 4096.0f) {
        step *= 2.0f;
    }
    n = (int)(g_grid_r / step);
    if (n < 1) {
        n = 1;
    }
    if (n > 16) {
        n = 16;
    }
    ext = (float)n * step;
    for (i = -n; i <= n; i++) {
        float o;

        o = (float)i * step;
        if (axis == 0) {
            grid_line(cam, basis,
                      g_grid_cx + o, g_grid_cy, g_grid_cz - ext,
                      g_grid_cx + o, g_grid_cy, g_grid_cz + ext, r, g, b);
            grid_line(cam, basis,
                      g_grid_cx - ext, g_grid_cy, g_grid_cz + o,
                      g_grid_cx + ext, g_grid_cy, g_grid_cz + o, r, g, b);
        } else if (axis == 1) {
            grid_line(cam, basis,
                      g_grid_cx + o, g_grid_cy - ext, g_grid_cz,
                      g_grid_cx + o, g_grid_cy + ext, g_grid_cz, r, g, b);
            grid_line(cam, basis,
                      g_grid_cx - ext, g_grid_cy + o, g_grid_cz,
                      g_grid_cx + ext, g_grid_cy + o, g_grid_cz, r, g, b);
        } else {
            grid_line(cam, basis,
                      g_grid_cx, g_grid_cy + o, g_grid_cz - ext,
                      g_grid_cx, g_grid_cy + o, g_grid_cz + ext, r, g, b);
            grid_line(cam, basis,
                      g_grid_cx, g_grid_cy - ext, g_grid_cz + o,
                      g_grid_cx, g_grid_cy + ext, g_grid_cz + o, r, g, b);
        }
    }
}

static void draw_grid(void)
{
    Basis basis;
    float r, g, b;

    if (!g_grid || !g_frame_cam || g_grid_r < 1.0f) {
        return;
    }
    r = lift_ch(g_bg_r);
    g = lift_ch(g_bg_g);
    b = lift_ch(g_bg_b);
    camera_basis(g_frame_cam, &basis);
    glide_flat(1);
    grid_plane(g_frame_cam, &basis, 0, r, g, b);
    grid_plane(g_frame_cam, &basis, 1, r, g, b);
    grid_plane(g_frame_cam, &basis, 2, r, g, b);
    glide_flat(0);
}

static void run_view(const Level *level, float cx, float cy, float cz, float scaled)
{
    Camera cam;
    float yaw;
    float pitch;
    float dist;
    float base;
    int reset_was;
    int help_was;
    int grid_was;
    int stats_was;
    DWORD tick;

    base = fit_dist(scaled);
    dist = base;
    yaw = 0.0f;
    pitch = 0.2f;
    reset_was = key_down('R');
    help_was = key_down(VK_F1);
    grid_was = key_down('G');
    stats_was = key_down(VK_F2);
    g_help = 0;
    g_left = 0;
    g_right = 0;
    g_dx = 0;
    g_dy = 0;
    g_rz = 0;
    tick = GetTickCount();
    input_clear_quit();
    g_grid_cx = cx;
    g_grid_cy = cy;
    g_grid_cz = cz;
    g_grid_r = scaled;
    gfx_set_under(draw_grid);
    gfx_set_extra(draw_overlay);
    for (;;) {
        Input in;
        DWORD now;
        DWORD dt;
        int reset_now;
        int help_now;
        int grid_now;
        int stats_now;
        float step;

        win_pump();
        input_read(&in);
        if (in.quit) {
            break;
        }
        now = GetTickCount();
        dt = now - tick;
        tick = now;
        if (dt > 100) {
            dt = 100;
        }
        help_now = key_down(VK_F1);
        if (help_now && !help_was) {
            g_help = g_help ? 0 : 1;
            log_line(g_help ? "help on" : "help off");
        }
        help_was = help_now;
        grid_now = key_down('G');
        if (grid_now && !grid_was) {
            g_grid = g_grid ? 0 : 1;
            log_line(g_grid ? "grid on" : "grid off");
        }
        grid_was = grid_now;
        stats_now = key_down(VK_F2);
        if (stats_now && !stats_was) {
            g_stats = g_stats ? 0 : 1;
            log_line(g_stats ? "stats on" : "stats off");
        }
        stats_was = stats_now;
        reset_now = key_down('R');
        if (reset_now && !reset_was) {
            yaw = 0.0f;
            pitch = 0.2f;
            dist = base;
            log_line("camera reset");
        }
        reset_was = reset_now;
        yaw += (float)g_dx * 0.01f;
        pitch -= (float)g_dy * 0.01f;
        g_dx = 0;
        g_dy = 0;
        if (key_down(VK_LEFT)) {
            yaw -= (float)dt * 0.0015f;
        }
        if (key_down(VK_RIGHT)) {
            yaw += (float)dt * 0.0015f;
        }
        if (key_down(VK_UP)) {
            pitch += (float)dt * 0.0012f;
        }
        if (key_down(VK_DOWN)) {
            pitch -= (float)dt * 0.0012f;
        }
        step = dist * (float)dt * 0.001f;
        if (step < 0.05f) {
            step = 0.05f;
        }
        if (key_down('W') || key_down(VK_PRIOR)) {
            dist -= step;
        }
        if (key_down('S') || key_down(VK_NEXT)) {
            dist += step;
        }
        if (g_rz) {
            dist += (float)g_rz * (dist * 0.004f);
            g_rz = 0;
        }
        if (pitch > PITCH_MAX) {
            pitch = PITCH_MAX;
        }
        if (pitch < -PITCH_MAX) {
            pitch = -PITCH_MAX;
        }
        if (dist < scaled + 8.0f) {
            dist = scaled + 8.0f;
        }
        if (dist > 1100.0f) {
            dist = 1100.0f;
        }
        yaw = df_wrap_tau(yaw);
        place_camera(&cam, cx, cy, cz, yaw, pitch, dist);
        g_frame_cam = &cam;
        gfx_frame(&cam, level);
    }
    gfx_set_under(0);
    gfx_set_extra(0);
    g_frame_cam = 0;
}

static void log_model(df_u32 id, int tris, int textures, int missing, int uv)
{
    char line[80];
    int n;

    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "model ");
    add_u(line, &n, (int)sizeof(line), id);
    add_str(line, &n, (int)sizeof(line), " tris ");
    add_u(line, &n, (int)sizeof(line), (df_u32)tris);
    add_str(line, &n, (int)sizeof(line), " tex ");
    add_u(line, &n, (int)sizeof(line), (df_u32)textures);
    add_str(line, &n, (int)sizeof(line), " missing ");
    add_u(line, &n, (int)sizeof(line), (df_u32)missing);
    add_str(line, &n, (int)sizeof(line), " uvfail ");
    add_u(line, &n, (int)sizeof(line), (df_u32)uv);
    log_line(line);
}

static void view_record(Bsa *bsa, int index)
{
    const BsaRec *rec;
    df_u8 *bytes;
    Mesh *mesh;
    Instance inst;
    Level level;
    float cx, cy, cz, radius, scale, scaled;
    int nimages;
    int missing;
    int glide_up;
    int uv0;
    int i;

    rec = bsa_at(bsa, index);
    if (!rec) {
        return;
    }
    {
        char line[40];
        int n;

        n = 0;
        line[0] = 0;
        add_str(line, &n, (int)sizeof(line), "open ");
        add_u(line, &n, (int)sizeof(line), rec->id);
        log_line(line);
    }
    if (!mem_temp_begin()) {
        log_line("temp heap failed");
        win_fail("Out of memory.");
        return;
    }
    uv0 = arch3d_uv_failures();
    bytes = bsa_read(bsa, rec);
    mesh = 0;
    if (bytes) {
        mesh = arch3d_load(bytes, rec->size, rec->id);
    }
    nimages = 0;
    missing = 0;
    glide_up = 0;
    if (!mesh || mesh->ntris < 1 || !mesh_bounds(mesh, &cx, &cy, &cz, &radius)) {
        log_line(mesh ? "no triangles" : "read failed");
        win_fail(mesh ? "This record has no triangles." : "Could not read this model.");
    } else if (!bind_textures(mesh, &nimages, &missing)) {
        log_line("no textures");
        win_fail("This model has no faces.");
    } else {
        log_model(rec->id, mesh->ntris, nimages, missing,
                  arch3d_uv_failures() - uv0);
        g_stat_id = rec->id;
        g_stat_size = rec->size;
        g_stat_tris = mesh->ntris;
        g_stat_tex = nimages;
        g_stat_miss = missing;
        scale = 1.0f;
        if (radius > FIT_RADIUS) {
            scale = FIT_RADIUS / radius;
        }
        scaled = radius * scale;
        mem_zero(&inst, sizeof(inst));
        place_instance(&inst, mesh, cx, cy, cz, scale);
        mem_zero(&level, sizeof(level));
        level.images = g_images;
        level.cutout = g_cutout;
        level.nimages = nimages;
        level.palette = &g_pal;
        level.scene.models = &inst;
        level.scene.nmodels = 1;
        show_list(0);
        if (!gfx_startup(win_hwnd())) {
            log_line(gfx_error());
            win_fail(gfx_error());
        } else {
            glide_up = 1;
            if (!gfx_prepare(&level)) {
                log_line(gfx_error());
                gfx_shutdown();
                glide_up = 0;
                win_fail(gfx_error());
            } else {
                log_line("glide open 640x480");
                win_blank_cursor(1);
                win_mouse_hook(on_mouse);
                run_view(&level, cx, cy, cz, scaled);
            }
        }
    }
    win_mouse_hook(0);
    win_blank_cursor(0);
    if (glide_up) {
        gfx_shutdown();
    }
    texture_cache_clear();
    mem_temp_end();
    for (i = 0; i < MAX_TEX; i++) {
        g_images[i] = 0;
    }
    show_list(1);
    input_clear_quit();
}

static int load_palette(void)
{
    df_u8 *bytes;
    df_u32 size;

    bytes = vfs_read("ART_PAL.COL", &size);
    if (!palette_load(bytes, size, &g_pal)) {
        log_line("ART_PAL.COL failed");
        return 0;
    }
    log_line("palette ART_PAL.COL");
    return 1;
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
    if (s[i] != 0 && s[i] != ' ' && s[i] != '\t' && s[i] != '\r') {
        return 0;
    }
    *rest = s + i;
    return 1;
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
        if (!read_int(&rest, &r) || !read_int(&rest, &g) || !read_int(&rest, &b)) {
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
    if (key_is(s, "grid", &rest)) {
        if (!read_int(&rest, &v) || v != 0) {
            g_grid = 1;
            return;
        }
        g_grid = 0;
    }
}

static void cfg_log(void)
{
    char line[64];
    int n;

    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "background ");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_bg_r);
    add_str(line, &n, (int)sizeof(line), " ");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_bg_g);
    add_str(line, &n, (int)sizeof(line), " ");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_bg_b);
    log_line(line);
    n = 0;
    line[0] = 0;
    add_str(line, &n, (int)sizeof(line), "grid ");
    add_u(line, &n, (int)sizeof(line), (df_u32)g_grid);
    log_line(line);
}

static void cfg_load(void)
{
    static const char text[] =
        "# background R G B, each 0 to 255\r\n"
        "background 48 48 48\r\n"
        "# grid 1 draws scale planes, 0 hides them\r\n"
        "grid 1\r\n";
    char path[260];
    char buf[2048];
    HANDLE file;
    DWORD got;
    DWORD wrote;
    int n;
    char *p;

    g_bg_r = 48;
    g_bg_g = 48;
    g_bg_b = 48;
    g_grid = 1;
    if (!beside_exe(path, (int)sizeof(path), "renderfall.cfg")) {
        log_line("could not name renderfall.cfg");
        glide_background(g_bg_r, g_bg_g, g_bg_b);
        cfg_log();
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
            log_line("could not write renderfall.cfg");
        } else {
            log_line("wrote renderfall.cfg");
        }
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
    } else {
        file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (file == INVALID_HANDLE_VALUE) {
            log_line("could not read renderfall.cfg");
        } else {
            if (ReadFile(file, buf, (DWORD)sizeof(buf) - 1, &got, 0)) {
                buf[got] = 0;
                p = buf;
                while (*p) {
                    char *line;

                    line = p;
                    while (*p && *p != '\n') {
                        p++;
                    }
                    if (*p == '\n') {
                        *p = 0;
                        p++;
                    }
                    cfg_line(line);
                }
            }
            CloseHandle(file);
        }
    }
    glide_background(g_bg_r, g_bg_g, g_bg_b);
    cfg_log();
}

static void load_font(void)
{
    if (!fnt_install()) {
        log_line(fnt_error());
        log_line("text uses the built-in font");
        return;
    }
    if (fnt_generated()) {
        log_line("font0000.bmp written from FONT0000.FNT");
        return;
    }
    log_line("FONT0000.FNT");
}

static int renderfall_main(void *instance)
{
    Bsa *bsa;
    char root[260];
    int n;
    char line[64];
    int len;

    mem_init();
    log_open();
    log_line("renderfall");
    cfg_load();
    if (!win_startup(instance, "/ARENA2", "RENDERFALL", 1)) {
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
        win_fail("Could not open the model list.");
        win_shutdown();
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    load_font();
    if (!load_palette()) {
        win_fail("Could not read ART_PAL.COL.");
        win_shutdown();
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    bsa = bsa_open("ARCH3D.BSA");
    if (!bsa) {
        log_line("ARCH3D.BSA failed");
        win_fail("Could not read ARCH3D.BSA.");
        win_shutdown();
        if (g_log != INVALID_HANDLE_VALUE) {
            CloseHandle(g_log);
        }
        mem_shutdown();
        return 1;
    }
    n = fill_list(bsa);
    len = 0;
    line[0] = 0;
    add_str(line, &len, (int)sizeof(line), "records ");
    add_u(line, &len, (int)sizeof(line), (df_u32)n);
    log_line(line);
    log_line("list: Enter or double-click opens, click a column to sort, Esc quits");
    log_line("view: arrows or left mouse pan, W/S zoom, R resets, G grid, F1 help, F2 stats, F3 fps, Esc returns");
    if (n < 1) {
        win_fail("ARCH3D.BSA has no records.");
    } else {
        while (wait_list()) {
            int idx;

            idx = selected_record();
            if (idx < 0) {
                continue;
            }
            view_record(bsa, idx);
        }
    }
    bsa_close(bsa);
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
    ExitProcess((UINT)renderfall_main(GetModuleHandleA(0)));
}
