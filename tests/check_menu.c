/* Exercise the shipping menu state machine with deterministic platform input.
 * These stand-ins do not test Win32 polling (tests/check_input.c does that),
 * file I/O, or Glide. Persistence's record format is checked in check_play.c.
 * No test writes the user's DATA/KEYS.DAT. */
#include "check_util.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/settings.h"
#include "game/dagger/play/keys.h"
#include "game/dagger/play/control.h"
#include "game/dagger/play/mouse.h"
#include "engine/core/mem.h"
#include "engine/gfx/camera.h"
#include "engine/ui/font.h"

static int g_bind[INPUT_ACTIONS];
static int g_px, g_py, g_key, g_writes, g_write_ok;

int input_binding(int action) { return g_bind[action]; }
int input_bind(int action, int vk)
{
    if (action < 0 || action >= INPUT_ACTIONS || !vk || vk == 0xc0 || (vk == 0x1b && action != INPUT_MENU)) return 0;
    g_bind[action] = vk;
    return 1;
}
int input_key_pressed(void) { int key; key = g_key; g_key = 0; return key; }
void input_pointer(int *x, int *y) { *x = g_px; *y = 479-g_py; }
void keys_defaults(void)
{
    int i;
    for (i = 0; i < INPUT_ACTIONS; i++) g_bind[i] = i == INPUT_MENU ? 0x1b : 'A'+i;
    mouse_set_cursor(1);
    control_set_classic(1);
}
int keys_save(void) { g_writes++; return g_write_ok; }
void ui_string(int x, int y, const char *s) { (void)x; (void)y; (void)s; }
int ui_string_width(const char *s) { int n; n=0; while (*s++) n++; return n*6; }
int ui_line_height(void) { return 8; }

static int click_at(int x, int y)
{
    Input in;
    mem_zero(&in, sizeof(in));
    g_px=x; g_py=y;
    in.mouse_pressed=INPUT_MOUSE_LEFT;
    /* A complete physical click between reads must work as well as a held
     * press. This is particularly important for the slider's first value. */
    in.mouse_released=INPUT_MOUSE_LEFT;
    return menu_frame(&in);
}
void check_menu(void)
{
    Input in;
    int old, other;
    keys_defaults();
    g_write_ok=1; g_writes=0;
    camera_set_fov(75.0f);
    menu_open(MENU_OPTIONS);
    check_expect(click_at(230,130)==MENU_LOAD, "menu: Load/Save tab load action");
    check_expect(click_at(430,130)==MENU_SAVE, "menu: Load/Save tab save action");
    click_at(30,200);
    click_at(516,112);
    check_expect(df_iround(camera_fov())==75, "menu: draft FOV changed live projection");
    click_at(200,445);
    check_expect(df_iround(camera_fov())==120 && g_writes==1, "menu: Apply did not commit FOV");
    click_at(336,112);
    click_at(550,445);
    check_expect(menu_current()==MENU_NONE && df_iround(camera_fov())==120, "menu: Back committed unapplied FOV");
    menu_open(MENU_OPTIONS);
    click_at(30,200);
    click_at(40,445); /* Default is a draft too. */
    check_expect(df_iround(camera_fov())==120, "menu: Default applied without Apply");
    g_write_ok=0;
    click_at(200,445);
    check_expect(df_iround(camera_fov())==120, "menu: failed save changed FOV");
    g_write_ok=1;
    click_at(200,445);
    check_expect(df_iround(camera_fov())==75, "menu: retry did not apply default");
    /* A drag remains owned after leaving its track, and clamps. Focus loss
     * cancels it before the stale held input can change the draft. */
    mem_zero(&in,sizeof(in));
    g_px=408; g_py=112; in.mouse_pressed=in.mouse_held=INPUT_MOUSE_LEFT;
    menu_frame(&in);
    g_px=630; in.mouse_pressed=0; menu_frame(&in);
    in.reset=1; menu_frame(&in);
    g_px=0; in.reset=0; menu_frame(&in);
    click_at(200,445);
    check_expect(df_iround(camera_fov())==120, "menu: reset did not cancel slider ownership");

    click_at(30,160);
    old=g_bind[INPUT_FORWARD]; other=g_bind[INPUT_BACK];
    click_at(450,104);
    mem_zero(&in,sizeof(in)); g_key=other; menu_frame(&in);
    check_expect(g_bind[INPUT_FORWARD]==old, "menu: binding draft applied early");
    click_at(200,445);
    check_expect(g_bind[INPUT_FORWARD]==other && g_bind[INPUT_BACK]==old, "menu: duplicate binding not swapped");
    click_at(450,104);
    g_key=0x1b; in.pressed=INPUT_BIT(INPUT_MENU); menu_frame(&in);
    check_expect(menu_current()==MENU_CONTROLS, "menu: Escape binding cancellation closed menu");
    click_at(450,104);
    mem_zero(&in,sizeof(in)); in.reset=1; g_key='Z'; menu_frame(&in);
    click_at(200,445);
    check_expect(g_bind[INPUT_FORWARD]==other, "menu: focus reset accepted stale binding");
    mem_zero(&in,sizeof(in)); in.wheel=-100; menu_frame(&in);
    click_at(450,370); g_key='Z'; in.wheel=0; menu_frame(&in);
    click_at(200,445);
    check_expect(g_bind[INPUT_MAP]=='Z', "menu: last binding inaccessible after scrolling");
    click_at(30,120);
    check_expect(click_at(40,445)==MENU_STAY, "menu: exit skipped confirmation");
    mem_zero(&in,sizeof(in)); menu_frame(&in);
    check_expect(click_at(40,445)==MENU_QUIT, "menu: idle frame lost exit confirmation");
    menu_open(MENU_NONE);
    camera_set_fov(75.0f);
}
