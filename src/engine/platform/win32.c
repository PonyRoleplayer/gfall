/*
 * Window and the process entry. Glide is opened later, on this HWND.
 * Escape is delivered both as a key message and as a polled key.
 */

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/platform/fs.h"
#include "engine/platform/input.h"
#include "engine/platform/win32.h"

static HINSTANCE g_inst;
static HWND g_hwnd;
static int g_class;
static int g_gui;
static int g_blank;
static int g_cursor_hidden;
static char g_title[32];
static char g_class_name[32];
static void (*g_mouse)(unsigned int msg, int x, int y);
static void (*g_command)(unsigned int id, unsigned int code);
static void (*g_notify)(void *hdr);
typedef unsigned (WINAPI *gdi_color_fn)(HDC, unsigned);
static gdi_color_fn g_set_text;
static gdi_color_fn g_set_bk;
static void *g_ui_font;

/* Directory of this exe, plus the game's suffix. "/ARENA2" reproduces the old root. */
static int content_root(char *dst, int n, const char *suffix)
{
    char exe[260];
    DWORD len;
    int i;
    int slash;
    int sn;

    if (!suffix) {
        return 0;
    }
    sn = 0;
    while (suffix[sn]) {
        sn++;
    }
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
    if (slash < 1 || slash + sn + 1 >= n) {
        return 0;
    }
    for (i = 0; i < slash; i++) {
        dst[i] = exe[i];
    }
    for (i = 0; i < sn; i++) {
        dst[slash + i] = suffix[i];
    }
    dst[slash + sn] = 0;
    return 1;
}

static void mouse_msg(HWND hwnd, UINT msg, LPARAM lp)
{
    int x;
    int y;

    x = (int)(short)LOWORD(lp);
    y = (int)(short)HIWORD(lp);
    if (msg == WM_MOUSEMOVE) {
        input_on_mouse(x, y);
    }
    /* Button-up is delivered before ReleaseCapture. Capture release can
       synthesize a move, and that move must not count as a drag. */
    if (g_mouse && (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP)) {
        g_mouse(msg, x, y);
    }
    if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
        SetCapture(hwnd);
    }
    if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) {
        ReleaseCapture();
    }
    if (g_mouse && msg != WM_LBUTTONUP && msg != WM_RBUTTONUP) {
        g_mouse(msg, x, y);
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        mouse_msg(hwnd, msg, lp);
        return 0;
    case WM_SETCURSOR:
        if (g_blank) {
            SetCursor(0);
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            input_request_quit();
        }
        return 0;
    case WM_COMMAND:
        if (g_command) {
            g_command((unsigned int)LOWORD(wp), (unsigned int)HIWORD(wp));
        }
        return 0;
    case WM_NOTIFY:
        if (g_notify) {
            g_notify((void *)lp);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        if (g_gui && g_set_text && g_set_bk) {
            g_set_text((HDC)wp, GetSysColor(COLOR_WINDOWTEXT));
            g_set_bk((HDC)wp, GetSysColor(COLOR_BTNFACE));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    case WM_CLOSE:
        input_request_quit();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void win_fail(const char *text)
{
    MessageBoxA(0, text ? text : "failed", g_title[0] ? g_title : "gfall",
                MB_OK | MB_ICONERROR);
}

void win_mouse_hook(void (*fn)(unsigned int msg, int x, int y))
{
    g_mouse = fn;
}

void win_command_hook(void (*fn)(unsigned int id, unsigned int code))
{
    g_command = fn;
}

void win_notify_hook(void (*fn)(void *hdr))
{
    g_notify = fn;
}

void *win_ui_font(void)
{
    typedef void *(WINAPI *create_fn)(
        int, int, int, int, int,
        unsigned, unsigned, unsigned, unsigned,
        unsigned, unsigned, unsigned, unsigned,
        const char *);
    HMODULE gdi;
    create_fn create;

    if (g_ui_font) {
        return g_ui_font;
    }
    gdi = GetModuleHandleA("gdi32.dll");
    if (!gdi) {
        gdi = LoadLibraryA("gdi32.dll");
    }
    if (!gdi) {
        return 0;
    }
    if (!g_set_text) {
        g_set_text = (gdi_color_fn)GetProcAddress(gdi, "SetTextColor");
        g_set_bk = (gdi_color_fn)GetProcAddress(gdi, "SetBkColor");
    }
    create = (create_fn)(void *)GetProcAddress(gdi, "CreateFontA");
    if (!create) {
        return 0;
    }
    /* 8pt MS Sans Serif. The System font is the bold Windows 3.1 face. */
    g_ui_font = create(-11, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 0, 0, "MS Sans Serif");
    return g_ui_font;
}

void win_blank_cursor(int on)
{
    g_blank = on ? 1 : 0;
    if (on && !g_cursor_hidden) {
        while (ShowCursor(FALSE) >= 0) {
        }
        g_cursor_hidden = 1;
        return;
    }
    if (!on && g_cursor_hidden) {
        while (ShowCursor(TRUE) < 0) {
        }
        g_cursor_hidden = 0;
    }
}

static HWND open_window(HINSTANCE inst)
{
    WNDCLASSA wc;
    HWND hwnd;
    DWORD style;

    wc.style = 0;
    wc.lpfnWndProc = wnd_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    /* Placeholder until renderfall has its own icon. */
    wc.hIcon = g_gui ? LoadIconA(0, IDI_APPLICATION) : 0;
    wc.hCursor = g_gui ? LoadCursorA(0, IDC_ARROW) : 0;
    wc.hbrBackground = g_gui ? (HBRUSH)(COLOR_BTNFACE + 1) : 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = g_class_name;

    if (!RegisterClassA(&wc)) {
        win_fail("RegisterClass failed.");
        return 0;
    }
    g_class = 1;

    if (g_gui) {
        RECT box;
        int w;
        int h;

        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
            | WS_CLIPCHILDREN | WS_VISIBLE;
        box.left = 0;
        box.top = 0;
        box.right = 640;
        box.bottom = 480;
        AdjustWindowRect(&box, style, 0);
        w = box.right - box.left;
        h = box.bottom - box.top;
        hwnd = CreateWindowExA(
            WS_EX_WINDOWEDGE,
            g_class_name,
            g_title,
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            w,
            h,
            0,
            0,
            inst,
            0);
    } else {
        style = WS_POPUP | WS_VISIBLE;
        hwnd = CreateWindowExA(
            0,
            g_class_name,
            g_title,
            style,
            0,
            0,
            640,
            480,
            0,
            0,
            inst,
            0);
    }
    if (!hwnd) {
        win_fail("CreateWindow failed.");
        return 0;
    }
    if (g_gui && wc.hIcon) {
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIcon);
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    return hwnd;
}

int win_startup(void *instance, const char *suffix, const char *title, int gui)
{
    char root[260];

    g_inst = (HINSTANCE)instance;
    g_gui = gui ? 1 : 0;
    df_copy(g_title, title ? title : "gfall", (int)sizeof(g_title));
    df_copy(g_class_name, g_title, (int)sizeof(g_class_name));
    if (!content_root(root, (int)sizeof(root), suffix)) {
        win_fail("Could not find ARENA2 next to the program.");
        return 0;
    }
    fs_set_root(root);
    g_hwnd = open_window(g_inst);
    if (!g_hwnd) {
        return 0;
    }
    win_pump();
    return 1;
}

void win_shutdown(void)
{
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = 0;
    }
    if (g_class) {
        UnregisterClassA(g_class_name, g_inst);
        g_class = 0;
    }
}

void win_pump(void)
{
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            input_request_quit();
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void *win_hwnd(void)
{
    return g_hwnd;
}

