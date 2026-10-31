/*
 * Real input.c against a deterministic Win32 surface. See docs/input.md
 * for the standalone build command: the normal hostcheck does not link
 * input.c, and this harness needs neither Windows nor the game's data.
 * Synchronous warp notifications deliberately exercise g_ignore: our own
 * SetCursorPos must not erase movement accumulated just before it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "engine/platform/input.h"
#include "engine/ui/console.h"

static int keys[256];
static int polls[256];
static int con;
static int shown;
static int warps;
static HWND capture;
static int width = 1280;
static int height = 960;

static void expect(int ok, const char *why)
{
    if (!ok) {
        fprintf(stderr, "input: %s\n", why);
        exit(1);
    }
}

unsigned int MapVirtualKeyA(unsigned int scan, unsigned int mode)
{
    (void)scan;
    (void)mode;
    return 0xc0;
}
short GetAsyncKeyState(int vk)
{
    polls[vk]++;
    return keys[vk] ? (short)0x8000 : 0;
}
int ClientToScreen(HWND hwnd, POINT *p)
{
    (void)hwnd;
    (void)p;
    return 1;
}
int SetCursorPos(int x, int y)
{
    warps++;
    input_on_mouse(x, y);
    return 1;
}
int GetClientRect(HWND hwnd, RECT *rc)
{
    (void)hwnd;
    rc->left = rc->top = 0;
    rc->right = width;
    rc->bottom = height;
    return 1;
}
int ShowCursor(int on) { shown += on ? 1 : -1; return shown; }
int ClipCursor(const RECT *rc) { (void)rc; return 1; }
HWND SetCapture(HWND hwnd) { HWND was = capture; capture = hwnd; return was; }
int ReleaseCapture(void) { capture = 0; return 1; }
void console_pump(void) {}
int console_open(void) { return con; }
int console_take_escape(void) { return 0; }

/* Every requested key, including duplicate bindings, may be polled only
   once inside a snapshot. No idle scan of 254 keys is needed to achieve it. */
static void read_frame(Input *in)
{
    int k;
    int count;

    memset(polls, 0, sizeof(polls));
    memset(in, 0xa5, sizeof(*in));
    input_read(in);
    count = 0;
    for (k = 0; k < 256; k++) {
        expect(polls[k] <= 1, "one key was sampled twice in a snapshot");
        count += polls[k];
    }
    expect(count <= INPUT_ACTIONS + 9, "a frame polled keys it does not need");
}

static void check_defaults(void)
{
    static const int defaults[] = {
        0x01, 'E', 0x10, 0x20, 'C', 'I', 'M', 'R', 0x1b,
        'W', 'S', 'A', 'D', 'H', 0x09, 0x02, 0x25, 0x27,
        0x21, 0x22, 0x24, 0x12
    };
    Input in;
    int a;

    expect(INPUT_CURSOR == 14 && INPUT_SWING == 15 && INPUT_FREELOOK == 21
           && INPUT_ACTIONS == 22, "saved action ids moved");
    for (a = 0; a < INPUT_ACTIONS; a++) {
        expect(input_binding(a) == defaults[a], "an action's default key changed");
    }
    memset(&in, 0, sizeof(in));
    expect(!in.mouse_held && !in.mouse_pressed && !in.mouse_released
           && !in.mouse_dx && !in.mouse_dy && !in.reset,
           "a zeroed old caller has nonzero mouse/reset fields");
}

static void check_pointer(void)
{
    Input in;
    unsigned int acts;
    int before;
    int x, y;

    input_set_pointer(1);
    before = warps;
    keys[VK_LBUTTON] = keys[VK_RBUTTON] = keys[VK_LEFT] = 1;
    keys[VK_RIGHT] = keys[VK_PRIOR] = keys[VK_NEXT] = 1;
    keys[VK_HOME] = keys[VK_MENU] = 1;
    acts = INPUT_BIT(INPUT_ATTACK) | INPUT_BIT(INPUT_SWING)
         | INPUT_BIT(INPUT_TURN_LEFT) | INPUT_BIT(INPUT_TURN_RIGHT)
         | INPUT_BIT(INPUT_LOOK_UP) | INPUT_BIT(INPUT_LOOK_DOWN)
         | INPUT_BIT(INPUT_LOOK_CENTER) | INPUT_BIT(INPUT_FREELOOK);
    input_on_mouse(644, 474);
    read_frame(&in);
    expect(in.reset == 1, "pointer toggle did not mark a discontinuity");
    expect(in.held == acts && in.pressed == acts && !in.released,
           "free pointer suppressed an action or its first edge");
    expect(in.mouse_held == 3u && in.mouse_pressed == 3u && !in.mouse_released,
           "physical button press was not reported");
    expect(in.mouse_dx == 2 && in.mouse_dy == -3 && !in.look_dx && !in.look_dy,
           "pointer motion was not scaled independently of look");
    expect(warps == before, "pointer movement recentered the cursor");
    input_pointer(&x, &y);
    expect(x == 322 && y == 242, "logical pointer and raw delta scales disagree");
    read_frame(&in);
    expect(!in.reset, "a consumed pointer reset repeated");
    expect(in.held == acts && !in.pressed && !in.released
           && !in.mouse_pressed && !in.mouse_released && !in.mouse_dx && !in.mouse_dy,
           "a read repeated an edge or motion");
    input_set_pointer(1);
    read_frame(&in);
    expect(!in.reset, "setting the same pointer mode manufactured a boundary");
    memset(keys, 0, sizeof(keys));
    read_frame(&in);
    expect(!in.held && in.released == acts && in.mouse_released == 3u,
           "a physical or action release was lost");

    /* Rebinding gestures must not rebind a menu's physical left click. */
    expect(input_bind(INPUT_ATTACK, 'Z') && input_bind(INPUT_SWING, 'X'), "rebind failed");
    keys[VK_LBUTTON] = 1;
    keys['X'] = 1;
    read_frame(&in);
    expect(in.mouse_pressed == INPUT_MOUSE_LEFT && in.pressed == INPUT_BIT(INPUT_SWING),
           "a physical HUD click followed the attack binding");
    keys[VK_LBUTTON] = keys['X'] = 0;
    input_on_mouse(648, 474);
    read_frame(&in);
    expect(in.mouse_released == INPUT_MOUSE_LEFT && in.mouse_dx == 2
           && in.released == INPUT_BIT(INPUT_SWING), "button-up lost its last gesture movement");
    input_bind(INPUT_ATTACK, VK_LBUTTON);
    input_bind(INPUT_SWING, VK_RBUTTON);

    /* Fractional motion survives frame boundaries in either direction. */
    input_on_mouse(649, 474);
    read_frame(&in);
    expect(!in.mouse_dx, "half a logical pixel rounded up");
    input_on_mouse(650, 474);
    read_frame(&in);
    expect(in.mouse_dx == 1, "small movements lost their fractional carry");
    input_on_mouse(649, 474);
    read_frame(&in);
    expect(!in.mouse_dx, "negative half pixel rounded away from zero");
    input_on_mouse(648, 474);
    read_frame(&in);
    expect(in.mouse_dx == -1, "negative fractional carry was lost");
}

static void check_boundaries(void)
{
    Input in;

    keys[VK_LBUTTON] = keys[VK_RBUTTON] = 1;
    read_frame(&in);
    input_on_mouse(680, 500);
    con = 1;
    input_on_mouse(900, 800);
    input_on_wheel(120);
    read_frame(&in);
    expect(!in.held && !in.mouse_held && !in.mouse_pressed && in.mouse_released == 3u
           && in.released == (INPUT_BIT(INPUT_ATTACK) | INPUT_BIT(INPUT_SWING))
           && !in.mouse_dx && !in.mouse_dy && !in.wheel, "console leaked held input or a gesture");
    expect(in.reset == 1, "console release could be mistaken for a deliberate shot");
    read_frame(&in);
    expect(!in.released && !in.mouse_released && !in.reset,
           "suppressed read repeated a release or a consumed reset");
    memset(keys, 0, sizeof(keys));
    con = 0;
    input_on_mouse(1000, 800);
    read_frame(&in);
    expect(in.reset == 1, "leaving suppression did not mark a boundary");
    expect(!in.mouse_dx && !in.mouse_dy, "pointer jumped after the console");
    input_on_mouse(1002, 802);
    read_frame(&in);
    expect(!in.reset, "resumed motion repeated a consumed reset");
    expect(in.mouse_dx == 1 && in.mouse_dy == 1, "pointer did not resume after the console");

    input_on_mouse(1100, 850);
    input_mouse_reset(); /* WM_CAPTURECHANGED / WM_CANCELMODE, or console toggle. */
    input_on_mouse(1200, 900);
    read_frame(&in);
    expect(in.reset == 1, "capture loss did not cancel a gesture");
    expect(!in.mouse_dx && !in.mouse_dy, "capture transition leaked an old drag or a jump");
    input_on_mouse(1202, 902);
    read_frame(&in);
    expect(!in.reset, "capture reset was not consumed");
    expect(in.mouse_dx == 1 && in.mouse_dy == 1, "capture baseline never resumed");
    keys[VK_RBUTTON] = 1;
    read_frame(&in);
    input_on_mouse(1220, 920);
    input_on_focus(0);
    expect(!input_mouse_capture(), "unfocused session claims capture");
    read_frame(&in);
    expect(!in.held && !in.mouse_held && in.mouse_released == INPUT_MOUSE_RIGHT
           && !in.mouse_dx && !in.mouse_dy, "focus loss did not release/flush");
    expect(in.reset == 1, "focus release could be mistaken for a deliberate shot");
    read_frame(&in);
    expect(!in.reset && !in.mouse_released && !in.released,
           "remaining unfocused produced another reset or release");
    input_on_focus(1);
    input_on_mouse(10, 10);
    read_frame(&in);
    expect(in.reset == 1, "focus gain did not mark a boundary");
    expect(!in.mouse_dx && !in.mouse_dy && in.mouse_pressed == INPUT_MOUSE_RIGHT,
           "focus gain leaked a cursor jump or lost its fresh button edge");
    /* Capture can be lost without any button changing: the marker must
       cancel a drawn bow even though neither release mask has a bit. */
    input_mouse_reset();
    input_mouse_reset();
    read_frame(&in);
    expect(in.reset == 1 && in.mouse_held == INPUT_MOUSE_RIGHT
           && in.held == INPUT_BIT(INPUT_SWING) && !in.pressed && !in.released
           && !in.mouse_pressed && !in.mouse_released,
           "capture boundary lost its marker or changed legitimate button edges");
    read_frame(&in);
    expect(!in.reset, "coalesced reset events were replayed");
    memset(keys, 0, sizeof(keys));
    input_on_mouse(100, 100);
    input_set_pointer(0);
    read_frame(&in);
    expect(!in.mouse_dx && !in.mouse_dy && !in.look_dx && !in.look_dy,
           "pointer toggle leaked a drag or cursor warp");
}

static void check_look(void)
{
    Input in;
    int dx, dy;

    input_on_mouse(650, 476);
    keys[VK_RIGHT] = keys[VK_UP] = 1;
    read_frame(&in);
    expect(in.look_dx == 18 && in.look_dy == -12 && in.mouse_dx == 5 && in.mouse_dy == -2,
           "legacy mouse/arrow look changed or own warp erased raw motion");
    input_on_mouse(656, 472);
    input_look(&dx, &dy);
    expect(dx == 16 && dy == -8, "viewer input_look changed its mouse-only contract");
    read_frame(&in);
    expect(in.look_dx == 8 && in.look_dy == -8 && in.mouse_dx == 8 && in.mouse_dy == -4,
           "viewer input_look consumed the game's raw motion");
    memset(keys, 0, sizeof(keys));
    /* A centered mouse has no absolute baseline to reuse after capture
       moves elsewhere. Its first message restores center, without look. */
    input_mouse_reset();
    input_look(&dx, &dy);
    input_on_mouse(100, 100);
    read_frame(&in);
    expect(in.reset == 1, "viewer input_look consumed the pending reset");
    expect(!in.look_dx && !in.look_dy && !in.mouse_dx && !in.mouse_dy,
           "centered capture regain treated an OS cursor jump as motion");
    input_on_mouse(642, 482);
    read_frame(&in);
    expect(in.look_dx == 2 && in.look_dy == 2 && in.mouse_dx == 1 && in.mouse_dy == 1,
           "centered capture regain did not resume motion");
    input_on_mouse(660, 490);
    input_end();
    expect(!input_mouse_capture(), "ended session claims capture");
    input_begin((HWND)1);
    read_frame(&in);
    expect(!in.mouse_dx && !in.mouse_dy && !in.look_dx && !in.look_dy,
           "begin/end leaked motion from the last session");
    expect(in.reset == 1, "the first read after begin was not marked");
    read_frame(&in);
    expect(!in.reset, "begin's marker was not consumed");
}

int main(void)
{
    Input in;

    check_defaults();
    input_set_escape_quits(0);
    input_begin((HWND)1);
    expect(input_mouse_capture(), "begun session does not own capture");
    read_frame(&in);
    expect(in.reset == 1, "first begin did not mark the next snapshot");
    read_frame(&in);
    expect(!in.reset, "first begin's marker was not consumed");
    check_pointer();
    check_boundaries();
    check_look();
    input_end();
    read_frame(&in);
    expect(in.reset == 1, "end did not mark a discontinuity");
    read_frame(&in);
    expect(!in.reset, "end's marker was not consumed");
    puts("input snapshot ok");
    return 0;
}
