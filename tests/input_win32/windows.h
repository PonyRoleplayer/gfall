#ifndef TEST_INPUT_WINDOWS_H
#define TEST_INPUT_WINDOWS_H
/* Only input.c's Win32 surface. No fake engine behavior lives here:
   check_input.c supplies a deterministic keyboard, client and cursor. */
typedef void *HWND;
typedef struct { int x, y; } POINT;
typedef struct { int left, top, right, bottom; } RECT;
#define FALSE 0
#define TRUE 1
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_SHIFT 0x10
#define VK_MENU 0x12
#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_TAB 0x09
#define VK_LSHIFT 0xa0
#define VK_RMENU 0xa5
unsigned int MapVirtualKeyA(unsigned int scan, unsigned int mode);
short GetAsyncKeyState(int vk);
int ClientToScreen(HWND hwnd, POINT *p);
int SetCursorPos(int x, int y);
int GetClientRect(HWND hwnd, RECT *rc);
int ShowCursor(int on);
int ClipCursor(const RECT *rc);
HWND SetCapture(HWND hwnd);
int ReleaseCapture(void);
#endif
