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

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_MOUSEMOVE:
        input_on_mouse((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_SETCURSOR:
        SetCursor(0);
        return TRUE;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            input_request_quit();
        }
        return 0;
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
    MessageBoxA(0, text ? text : "GFALL failed.", "GFALL", MB_OK | MB_ICONERROR);
}

static HWND open_window(HINSTANCE inst)
{
    WNDCLASSA wc;
    HWND hwnd;

    wc.style = 0;
    wc.lpfnWndProc = wnd_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = "gfall";

    if (!RegisterClassA(&wc)) {
        win_fail("RegisterClass failed.");
        return 0;
    }
    g_class = 1;

    hwnd = CreateWindowExA(
        0,
        "gfall",
        "gfall",
        WS_POPUP | WS_VISIBLE,
        0,
        0,
        640,
        480,
        0,
        0,
        inst,
        0);
    if (!hwnd) {
        win_fail("CreateWindow failed.");
        return 0;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    return hwnd;
}

int win_startup(void *instance, const char *suffix)
{
    char root[260];

    g_inst = (HINSTANCE)instance;
    if (!content_root(root, (int)sizeof(root), suffix)) {
        win_fail("Could not find ARENA2 next to GFALL.EXE.");
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
        UnregisterClassA("gfall", g_inst);
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

extern int gfall_main(void *instance);

void WinMainCRTStartup(void)
{
    ExitProcess((UINT)gfall_main(GetModuleHandleA(0)));
}
