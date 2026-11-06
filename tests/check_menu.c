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
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "game/dagger/play/viewopt.h"
#include "engine/ui/font.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/draw.h"
#include "engine/gfx/sky.h"
#include "engine/scene/scene.h"
#include "engine/scene/light.h"
#include "engine/scene/daylight.h"
#include "engine/scene/weather.h"

static int g_stub_fog = 1, g_stub_mipmap = 1, g_stub_vsync = 1, g_stub_wbuffer = 1;
static int g_stub_planar = 1, g_stub_faces = 1, g_stub_spritepoint = 1, g_stub_fpu24 = 1;
static int g_stub_water = 1, g_stub_sky = 1;
static float g_stub_far = 1400.0f;

float gfx_far(void) { return g_stub_far; }
void gfx_set_far(float far) { g_stub_far = far; }
int gfx_fog(void) { return g_stub_fog; }
void gfx_set_fog(int on) { g_stub_fog = on; }
int gfx_mipmap(void) { return g_stub_mipmap; }
void gfx_set_mipmap(int on) { g_stub_mipmap = on; }
void gfx_set_light_range(int scale) { (void)scale; }
void gfx_set_light_shadow(int on) { (void)on; }
static int g_stub_tex_detail = 2, g_stub_sprite_detail = 2;
static const int g_stub_modes[][2] = { { 320, 240 }, { 400, 300 }, { 512, 384 }, { 640, 480 } };
void gfx_set_mode(int w, int h) { (void)w; (void)h; }
int gfx_mode_count(void) { return 4; }
int gfx_mode(int i, int *w, int *h)
{
    if (i < 0 || i >= 4) {
        return 0;
    }
    *w = g_stub_modes[i][0];
    *h = g_stub_modes[i][1];
    return 1;
}
void gfx_mode_opened(int *w, int *h) { *w = 640; *h = 480; }
void gfx_set_tex_detail(int level) { g_stub_tex_detail = level; }
int gfx_tex_detail(void) { return g_stub_tex_detail; }
void gfx_set_sprite_detail(int level) { g_stub_sprite_detail = level; }
int gfx_sprite_detail(void) { return g_stub_sprite_detail; }
int draw_faces(void) { return g_stub_faces; }
void draw_set_faces(int on) { g_stub_faces = on; }
int draw_sprite_point(void) { return g_stub_spritepoint; }
void draw_set_sprite_point(int on) { g_stub_spritepoint = on; }
int draw_fpu_single(void) { return g_stub_fpu24; }
void draw_set_fpu_single(int on) { g_stub_fpu24 = on; }
int draw_water(void) { return g_stub_water; }
void draw_set_water(int on) { g_stub_water = on; }
int glide_vsync(void) { return g_stub_vsync; }
void glide_set_vsync(int on) { g_stub_vsync = on; }
int glide_wbuffer(void) { return g_stub_wbuffer; }
void glide_set_wbuffer(int on) { g_stub_wbuffer = on; }
int glide_planar(void) { return g_stub_planar; }
void glide_set_planar(int on) { g_stub_planar = on; }
int sky_on(void) { return g_stub_sky; }
void sky_set_on(int on) { g_stub_sky = on; }

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
    click_at(516,120);
    check_expect(df_iround(camera_fov())==75, "menu: draft FOV changed live projection");
    click_at(200,445);
    check_expect(df_iround(camera_fov())==120 && g_writes==1, "menu: Apply did not commit FOV");
    click_at(336,120);
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
    g_px=408; g_py=120; in.mouse_pressed=in.mouse_held=INPUT_MOUSE_LEFT;
    menu_frame(&in);
    g_px=630; in.mouse_pressed=0; menu_frame(&in);
    in.reset=1; menu_frame(&in);
    g_px=0; in.reset=0; menu_frame(&in);
    click_at(200,445);
    check_expect(df_iround(camera_fov())==120, "menu: reset did not cancel slider ownership");

    /* Texture and sprite detail: High at the start; a click goes Low, Medium, High; a draft until Apply; Default is High. */
    menu_open(MENU_NONE);
    menu_open(MENU_OPTIONS);
    click_at(30,200);
    check_expect(gfx_tex_detail() == 2 && gfx_sprite_detail() == 2, "menu: the details do not start at High");
    click_at(516, 116 + 26 * 2);
    click_at(516, 116 + 26 * 3);
    click_at(516, 116 + 26 * 3);
    check_expect(gfx_tex_detail() == 2 && gfx_sprite_detail() == 2, "menu: a detail draft applied early");
    click_at(200,445);
    check_expect(gfx_tex_detail() == 0 && gfx_sprite_detail() == 1 && viewopt_tex_detail() == 0 && viewopt_sprite_detail() == 1,
                 "menu: Apply did not commit Low walls and Medium sprites to the engine and the options");
    click_at(40,445);
    click_at(200,445);
    check_expect(gfx_tex_detail() == 2 && gfx_sprite_detail() == 2 && viewopt_tex_detail() == 2, "menu: Default did not return the details to High");
    click_at(550,445);
    /* The resolution: the engine's four modes, the saved 640 x 480 first; a click goes to the next (320 x 240); a draft until Apply; kept for the next start; Default is 640 x 480. */
    viewopt_set_mode(640, 480);
    menu_open(MENU_NONE);
    menu_open(MENU_OPTIONS);
    click_at(30,200);
    check_expect(settings_resolutions() == 4 && settings_resolution_w(0) == 320 && settings_resolution_h(0) == 240
                     && settings_resolution_w(3) == 640, "menu: the selector does not list the engine's modes");
    click_at(516, 100);
    {
        int mw, mh;

        viewopt_mode(&mw, &mh);
        check_expect(mw == 640 && mh == 480, "menu: a resolution draft was kept early");
        click_at(200,445);
        viewopt_mode(&mw, &mh);
        check_expect(mw == 320 && mh == 240, "menu: Apply did not keep 320 x 240 for the next start");
        click_at(40,445);
        click_at(200,445);
        viewopt_mode(&mw, &mh);
        check_expect(mw == 640 && mh == 480, "menu: Default did not return the mode to 640 x 480");
    }
    click_at(550,445);
    menu_open(MENU_OPTIONS);
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
    check_expect(g_bind[INPUT_TRANSPORT]=='Z', "menu: last binding inaccessible after scrolling");
    /* Audio defaults are independent of gameplay. Applying Gameplay must
     * not commit an audio draft left in another tab. */
    menu_open(MENU_NONE);
    sound_set_volume(80); music_set_volume(80); viewopt_set_bob(0);
    menu_open(MENU_OPTIONS);
    click_at(30,240);
    click_at(336,112); /* SFX mute. */
    click_at(516,168); /* Music full. */
    check_expect(sound_volume()==80 && music_volume()==80, "menu: audio slider bypassed Apply");
    click_at(30,280);
    click_at(500,119);
    check_expect(!viewopt_bob(), "menu: head bob draft applied early");
    click_at(200,445);
    check_expect(viewopt_bob() && sound_volume()==80 && music_volume()==80, "menu: Gameplay Apply committed audio draft");
    click_at(30,240);
    g_write_ok=0; click_at(200,445);
    check_expect(sound_volume()==80 && music_volume()==80 && viewopt_bob(), "menu: audio failure did not restore committed preferences");
    g_write_ok=1; click_at(200,445);
    check_expect(sound_volume()==0 && music_volume()==100, "menu: audio retry lost independent drafts");
    click_at(550,445);
    menu_open(MENU_OPTIONS);
    click_at(30,240); click_at(200,445);
    check_expect(sound_volume()==0 && music_volume()==100, "menu: reopening lost audio values");
    click_at(40,445);
    check_expect(sound_volume()==0 && music_volume()==100, "menu: audio Default committed early");
    click_at(200,445);
    check_expect(sound_volume()==80 && music_volume()==80 && viewopt_bob(), "menu: audio defaults changed gameplay");

    /* Drag the music thumb across the SFX row, then reset focus while
     * holding the button. Neither the other row nor later motion may act. */
    mem_zero(&in,sizeof(in)); g_px=426; g_py=168;
    in.mouse_pressed=in.mouse_held=INPUT_MOUSE_LEFT; menu_frame(&in);
    g_px=0; g_py=112; in.mouse_pressed=0; menu_frame(&in);
    in.reset=1; menu_frame(&in);
    in.reset=0; g_px=630; menu_frame(&in);
    click_at(200,445);
    check_expect(sound_volume()==80 && music_volume()==0, "menu: audio drag transferred rows or survived focus loss");
    click_at(516,112);
    mem_zero(&in,sizeof(in)); in.pressed=INPUT_BIT(INPUT_MENU); menu_frame(&in);
    check_expect(menu_current()==MENU_NONE && sound_volume()==80, "menu: Escape applied audio draft");
    menu_open(MENU_OPTIONS);
    click_at(30,280); click_at(40,445);
    check_expect(viewopt_bob(), "menu: Gameplay Default committed early");
    g_write_ok=0; click_at(200,445);
    check_expect(viewopt_bob(), "menu: failed Gameplay Apply changed bob");
    g_write_ok=1; click_at(200,445);
    check_expect(!viewopt_bob(), "menu: Gameplay default retry failed");
    click_at(500,119); click_at(550,445);
    check_expect(!viewopt_bob(), "menu: Back applied bob draft");
    /* Toggle Crouch is a draft until Apply, is kept when the write fails, and Default turns it off. */
    viewopt_set_crouch_toggle(0);
    menu_open(MENU_OPTIONS);
    click_at(30,280);
    click_at(500,231);
    check_expect(!viewopt_crouch_toggle(), "menu: crouch toggle draft applied early");
    g_write_ok=0; click_at(200,445);
    check_expect(!viewopt_crouch_toggle(), "menu: failed Apply changed the crouch toggle");
    g_write_ok=1; click_at(200,445);
    check_expect(viewopt_crouch_toggle(), "menu: Apply did not commit the crouch toggle");
    click_at(40,445); click_at(200,445);
    check_expect(!viewopt_crouch_toggle(), "menu: Gameplay Default did not turn the crouch toggle off");
    click_at(550,445);
    menu_open(MENU_OPTIONS);
    /* Full HUD is the inverse of legacy full-screen. Exercise the
     * existing saved preference without adding another persisted field. */
    viewopt_set_full(0);
    menu_open(MENU_OPTIONS);
    click_at(30,280); click_at(500,175);
    check_expect(!viewopt_full(), "menu: Full HUD draft hid the bar before Apply");
    click_at(200,445);
    check_expect(viewopt_full(), "menu: Full HUD Off did not hide the bar");
    menu_open(MENU_NONE); menu_open(MENU_OPTIONS);
    click_at(30,280); click_at(200,445);
    check_expect(viewopt_full(), "menu: reopening inverted Full HUD");
    click_at(40,445);
    check_expect(viewopt_full(), "menu: Full HUD Default applied early");
    g_write_ok=0; click_at(200,445);
    check_expect(viewopt_full(), "menu: failed write changed Full HUD");
    g_write_ok=1; click_at(200,445);
    check_expect(!viewopt_full(), "menu: Full HUD default did not show the bar");
    click_at(500,175); click_at(550,445);
    check_expect(!viewopt_full(), "menu: Back applied Full HUD draft");
    menu_open(MENU_OPTIONS);
    click_at(30,120);
    check_expect(click_at(40,445)==MENU_STAY, "menu: exit skipped confirmation");
    mem_zero(&in,sizeof(in)); menu_frame(&in);
    check_expect(click_at(40,445)==MENU_QUIT, "menu: idle frame lost exit confirmation");
    menu_open(MENU_NONE);
    camera_set_fov(75.0f);
    sound_set_volume(80); music_set_volume(80); viewopt_set_bob(0);
}
