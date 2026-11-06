#include "game/dagger/play/settings.h"
#include "game/dagger/play/files.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/platform/input.h"

static const char *const g_names[] = {
    "Move forward", "Move backward", "Turn left", "Turn right",
    "Strafe left", "Strafe right", "Jump", "Run", "Rest", "Attack / swing",
    "Swap hands", "Character sheet", "Inventory", "Activate", "Toggle cursor",
    "Look up", "Look down", "Center view", "Free look", "Automap", "Travel map", "Transport", "Crouch"
};
static const int g_actions[] = {
    INPUT_FORWARD, INPUT_BACK, INPUT_TURN_LEFT, INPUT_TURN_RIGHT,
    INPUT_LEFT, INPUT_RIGHT, INPUT_JUMP, INPUT_RUN, INPUT_REST, INPUT_ATTACK,
    INPUT_SWAP_HANDS, INPUT_CHARACTER, INPUT_INVENTORY, INPUT_USE, INPUT_CURSOR,
    INPUT_LOOK_UP, INPUT_LOOK_DOWN, INPUT_LOOK_CENTER, INPUT_FREELOOK, INPUT_MAP, INPUT_TRAVEL_MAP, INPUT_TRANSPORT, INPUT_CROUCH
};

/* Geometry is shared by drawing and hit testing. The entire screen uses
 * actual 640x480 pixels, independent of classic's 320x200 stretched canvas. */
#define NAV_X 18
#define NAV_Y 108
#define NAV_W 136
#define NAV_H 34
#define BODY_X 170
#define BODY_W 452
#define ROW_Y 94
#define ROW_H 24
#define GFX_LIST_Y 88
#define GFX_ROW_H 26
#define GFX_LIST_H (SETTINGS_GFX_ROWS * GFX_ROW_H)
#define TRACK_X 336
#define TRACK_W 180
/* All slider geometry/ranges live here. Add a descriptor rather than
 * duplicating conversion logic for each new percentage or angle control. */
typedef struct Slider {
    int action, tab, y, minimum, maximum;
    const char *label;
} Slider;
static const Slider g_sliders[] = {
    { SETTINGS_SLIDER, SETTINGS_GRAPHICS, 0, 45, 120, "FIELD OF VIEW" },
    { SETTINGS_GFX_FAR_SLIDER, SETTINGS_GRAPHICS, 0, 16, 16384, "FAR CLIP" },
    { SETTINGS_GFX_AMBIENT_SLIDER, SETTINGS_GRAPHICS, 0, 0, 255, "AMBIENT LIGHT" },
    { SETTINGS_GFX_LIGHTRANGE_SLIDER, SETTINGS_GRAPHICS, 0, 1, 8, "LIGHT RANGE" },
    { SETTINGS_GFX_FLORADENSITY_SLIDER, SETTINGS_GRAPHICS, 0, 0, 100, "FLORA DENSITY" },
    { SETTINGS_GFX_FLORASIGHT_SLIDER, SETTINGS_GRAPHICS, 0, 0, 16384, "FLORA SIGHT" },
    { SETTINGS_SFX_SLIDER, SETTINGS_AUDIO, 112, 0, 100, "SFX VOLUME" },
    { SETTINGS_MUSIC_SLIDER, SETTINGS_AUDIO, 168, 0, 100, "MUSIC VOLUME" }
};
#define SLIDERS ((int)(sizeof(g_sliders) / sizeof(g_sliders[0])))
typedef enum { GFX_ROW_RES, GFX_ROW_SLIDER, GFX_ROW_TOGGLE, GFX_ROW_CHOICE } GfxRowKind;
typedef struct GfxRow {
    GfxRowKind kind;
    int action;
    const char *label;
    int slider_action;
} GfxRow;
static const GfxRow g_gfx_rows[] = {
    { GFX_ROW_RES, SETTINGS_GFX_RESOLUTION, "RESOLUTION", 0 },
    { GFX_ROW_SLIDER, SETTINGS_SLIDER, "FIELD OF VIEW", SETTINGS_SLIDER },
    { GFX_ROW_SLIDER, SETTINGS_GFX_FAR_SLIDER, "FAR CLIP", SETTINGS_GFX_FAR_SLIDER },
    { GFX_ROW_CHOICE, SETTINGS_GFX_TEXDETAIL, "TEXTURE DETAIL", 0 },
    { GFX_ROW_CHOICE, SETTINGS_GFX_SPRITEDETAIL, "SPRITE DETAIL", 0 },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_FOG, "DISTANCE FOG", 0 },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_SPRITEPOINT, "CRISP SPRITES", 0 },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_VSYNC, "VSYNC", 0 },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_LIGHTS, "DUNGEON LIGHTS", 0 },
    { GFX_ROW_SLIDER, SETTINGS_GFX_AMBIENT_SLIDER, "AMBIENT LIGHT", SETTINGS_GFX_AMBIENT_SLIDER },
    { GFX_ROW_SLIDER, SETTINGS_GFX_LIGHTRANGE_SLIDER, "LIGHT RANGE", SETTINGS_GFX_LIGHTRANGE_SLIDER },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_LIGHTSHADOW, "LIGHT SHADOW", 0 },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_WEATHERSIM, "WEATHER VIEW", 0 },
    { GFX_ROW_TOGGLE, SETTINGS_GFX_FLORA, "FLORA", 0 },
    { GFX_ROW_SLIDER, SETTINGS_GFX_FLORADENSITY_SLIDER, "FLORA DENSITY", SETTINGS_GFX_FLORADENSITY_SLIDER },
    { GFX_ROW_SLIDER, SETTINGS_GFX_FLORASIGHT_SLIDER, "FLORA SIGHT", SETTINGS_GFX_FLORASIGHT_SLIDER }
};
#define GFX_ROWS SETTINGS_GFX_ITEM_COUNT
static const char *const g_tab_labels[] = {
    "LOAD/SAVE", "KEYBINDINGS", "GRAPHICS", "AUDIO", "GAMEPLAY"
};
static const char *const g_titles[] = {
    "LOAD / SAVE", "KEYBINDINGS", "GRAPHICAL SETTINGS", "AUDIO SETTINGS", "GAMEPLAY SETTINGS"
};

int settings_binding_action(int row, int classic)
{
    if (row < 0 || row >= SETTINGS_BINDINGS) return -1;
    return row == 9 && classic ? INPUT_SWING : g_actions[row];
}
int settings_slider_at(int action, int x)
{
    int i;
    for (i = 0; i < SLIDERS; i++) {
        const Slider *slider;
        slider = &g_sliders[i];
        if (slider->action != action) continue;
        if (x <= TRACK_X) return slider->minimum;
        if (x >= TRACK_X+TRACK_W) return slider->maximum;
        return slider->minimum + ((x-TRACK_X)*(slider->maximum-slider->minimum)+TRACK_W/2)/TRACK_W;
    }
    return -1;
}
int settings_fov_at(int x) { return settings_slider_at(SETTINGS_SLIDER, x); }

static int gfx_value(const SettingsView *v, int action)
{
    const SettingsGfxDraft *g;
    g = &v->gfx;
    switch (action) {
    case SETTINGS_SLIDER: return g->fov;
    case SETTINGS_GFX_FAR_SLIDER: return g->far_clip;
    case SETTINGS_GFX_AMBIENT_SLIDER: return g->ambient;
    case SETTINGS_GFX_LIGHTRANGE_SLIDER: return g->lightrange;
    case SETTINGS_GFX_FLORADENSITY_SLIDER: return g->floradensity;
    case SETTINGS_GFX_FLORASIGHT_SLIDER: return g->florasight;
    case SETTINGS_GFX_FOG: return g->fog;
    case SETTINGS_GFX_SPRITEPOINT: return g->spritepoint;
    case SETTINGS_GFX_VSYNC: return g->vsync;
    case SETTINGS_GFX_LIGHTS: return g->lights;
    case SETTINGS_GFX_LIGHTSHADOW: return g->lightshadow;
    case SETTINGS_GFX_WEATHERSIM: return g->weathersim;
    case SETTINGS_GFX_FLORA: return g->flora;
    case SETTINGS_GFX_TEXDETAIL: return g->texdetail;
    case SETTINGS_GFX_SPRITEDETAIL: return g->spritedetail;
    default: return 0;
    }
}

static const Slider *slider_desc(int action)
{
    int i;
    for (i = 0; i < SLIDERS; i++) if (g_sliders[i].action == action) return &g_sliders[i];
    return 0;
}

static void gfx_slider_row(const Slider *slider, int y, int value)
{
    char number[8];
    int thumb, n;
    if (value < slider->minimum) value = slider->minimum;
    if (value > slider->maximum) value = slider->maximum;
    ui_panel(174, y, 444, GFX_ROW_H, 0);
    ui_region(slider->action, TRACK_X - 8, y + 2, TRACK_W + 16, GFX_ROW_H - 4);
    ui_text(184, y + 8, 132, ui_tr(slider->label), 0);
    ui_panel(TRACK_X - 5, y + 4, TRACK_W + 10, 18, 0);
    ui_flat(TRACK_X, y + 9, TRACK_W, 8, 0);
    thumb = (value - slider->minimum) * TRACK_W / (slider->maximum - slider->minimum);
    ui_flat(TRACK_X, y + 9, thumb, 8, 4);
    ui_panel(TRACK_X + thumb - 4, y + 4, 8, 18, 0);
    n = 0;
    if (value >= 10000) number[n++] = (char)('0' + value / 10000 % 10);
    if (value >= 1000) number[n++] = (char)('0' + value / 1000 % 10);
    if (value >= 100) number[n++] = (char)('0' + value / 100 % 10);
    if (value >= 10) number[n++] = (char)('0' + value / 10 % 10);
    number[n++] = (char)('0' + value % 10);
    number[n] = 0;
    ui_text(540, y + 6, 64, number, 0);
}

static void gfx_toggle_row(int y, const char *label, int on, int hot)
{
    ui_panel(174, y, 444, GFX_ROW_H, 0);
    ui_text(184, y + 8, 220, ui_tr(label), 0);
    ui_button(426, y + 3, 184, 20, on ? "ON" : "OFF", 0, hot);
}

/* The modes the selector lists and the one open now (settings_set_modes, settings_set_opened). */
static int g_mode_w[SETTINGS_GFX_MODES_MAX] = { 640 };
static int g_mode_h[SETTINGS_GFX_MODES_MAX] = { 480 };
static int g_modes = 1;
static int g_opened_w;
static int g_opened_h;

/* A choice among Low, Medium and High: a click goes to the next (menu.c). */
static void gfx_choice_row(int y, const char *label, int value, int hot)
{
    static const char *const names[] = { "LOW", "MEDIUM", "HIGH" };

    ui_panel(174, y, 444, GFX_ROW_H, 0);
    ui_text(184, y + 8, 220, ui_tr(label), 0);
    ui_button(426, y + 3, 184, 20, ui_tr(names[value < 0 ? 0 : (value > 2 ? 2 : value)]), 0, hot);
}

static void gfx_resolution_row(const SettingsView *v, int y, int hot)
{
    static char chosen[24];
    static char now[24];
    int w;
    int h;

    w = settings_resolution_w(v->gfx.resolution);
    h = settings_resolution_h(v->gfx.resolution);
    log_format(chosen, (int)sizeof(chosen), "%d x %d", w, h);
    ui_panel(174, y, 444, GFX_ROW_H, 0);
    ui_text(184, y + 8, 120, ui_tr("RESOLUTION"), 0);
    /* The change shows at the next start: say what is open now when it is another (a refused mode opens 640 x 480). */
    if (g_opened_w > 0 && (g_opened_w != w || g_opened_h != h)) {
        log_format(now, (int)sizeof(now), "NOW %d x %d", g_opened_w, g_opened_h);
        ui_text(300, y + 8, 120, ui_tr(now), 0);
    }
    ui_button(426, y + 3, 184, 20, chosen, 0, hot);
}

void settings_set_modes(const int *w, const int *h, int count)
{
    int i;

    g_modes = 0;
    for (i = 0; i < count && i < SETTINGS_GFX_MODES_MAX; i++) {
        g_mode_w[g_modes] = w[i];
        g_mode_h[g_modes] = h[i];
        g_modes++;
    }
    if (g_modes == 0) {
        g_mode_w[0] = 640;
        g_mode_h[0] = 480;
        g_modes = 1;
    }
}

int settings_resolutions(void)
{
    return g_modes;
}

void settings_set_opened(int w, int h)
{
    g_opened_w = w;
    g_opened_h = h;
}

int settings_resolution_w(int index)
{
    return index >= 0 && index < g_modes ? g_mode_w[index] : 0;
}

int settings_resolution_h(int index)
{
    return index >= 0 && index < g_modes ? g_mode_h[index] : 0;
}

const char *settings_gfx_console_var(int action)
{
    switch (action) {
    case SETTINGS_SLIDER: return "fov";
    case SETTINGS_GFX_FAR_SLIDER: return "far";
    case SETTINGS_GFX_AMBIENT_SLIDER: return "ambient";
    case SETTINGS_GFX_LIGHTRANGE_SLIDER: return "lightrange";
    case SETTINGS_GFX_FLORADENSITY_SLIDER: return "floradensity";
    case SETTINGS_GFX_FLORASIGHT_SLIDER: return "florasight";
    case SETTINGS_GFX_FOG: return "fog";
    case SETTINGS_GFX_SPRITEPOINT: return "spritepoint";
    case SETTINGS_GFX_VSYNC: return "vsync";
    case SETTINGS_GFX_LIGHTS: return "lights";
    case SETTINGS_GFX_LIGHTSHADOW: return "lightshadow";
    case SETTINGS_GFX_WEATHERSIM: return "weathersim";
    case SETTINGS_GFX_FLORA: return "flora";
    case SETTINGS_GFX_TEXDETAIL: return "texdetail";
    case SETTINGS_GFX_SPRITEDETAIL: return "spritedetail";
    default: return 0;
    }
}

/* Both audio channels reuse the FOV track and thumb. Percentage suffixes
 * and values are runtime text; dragging never regenerates an atlas. */
static void slider_draw(const Slider *slider, int value)
{
    char number[8];
    int thumb, n, y;
    y = slider->y;
    if (value < slider->minimum) value = slider->minimum;
    if (value > slider->maximum) value = slider->maximum;
    ui_panel(174,y-14,444,42,0);
    ui_region(slider->action, TRACK_X-8, y-10, TRACK_W+16, 32);
    ui_text(184,y+1,144,ui_tr(slider->label),0);
    ui_panel(TRACK_X-5,y-5,TRACK_W+10,20,0);
    ui_flat(TRACK_X,y,TRACK_W,10,0);
    thumb = (value-slider->minimum)*TRACK_W/(slider->maximum-slider->minimum);
    ui_flat(TRACK_X,y,thumb,10,4);
    ui_panel(TRACK_X+thumb-5,y-5,10,20,0);
    n = 0;
    if (value >= 100) number[n++] = (char)('0'+value/100);
    if (value >= 10) number[n++] = (char)('0'+value/10%10);
    number[n++] = (char)('0'+value%10);
    if (slider->tab == SETTINGS_AUDIO) number[n++] = '%';
    number[n] = 0;
    ui_text(540,y-1,64,number,0);
}
/* Which control is at a point: what the screen would draw, asked of its regions, with nothing drawn. */
int settings_hit(int tab, int scroll, int x, int y)
{
    SettingsView v;

    mem_zero(&v, sizeof(v));
    v.tab = tab;
    v.scroll = scroll;
    v.gfx_scroll = scroll;
    v.waiting = -1;
    v.hover = SETTINGS_NOTHING;
    settings_draw(&v, 0, 0);
    return ui_hit(x, y);
}

void settings_draw(const SettingsView *v, UiQuad quad, void *ctx)
{
    int i, n;
    const char *title;
    if (v->tab < 0 || v->tab >= SETTINGS_TABS) return;
    ui_begin(quad, ctx);
    ui_panel(8,8,624,464,0);
    ui_panel(14,14,612,32,0);
    title = g_titles[v->tab];
    ui_centered(14,23,612,title);
    ui_panel(14,52,144,370,0);
    ui_panel(18,56,136,44,0);
    ui_text(26,64,120,ui_tr("RENDERER"),1);
    ui_text(26,80,120,ui_tr("GLIDE (VOODOO)"),1);
    for (i = 0; i < SETTINGS_TABS; i++) {
        const char *label;
        label = g_tab_labels[i];
        ui_button(NAV_X,NAV_Y+i*40,NAV_W,NAV_H,label,0,v->hover == SETTINGS_TAB_FILES+i);
        ui_region(SETTINGS_TAB_FILES+i,NAV_X,NAV_Y+i*40,NAV_W,NAV_H);
        if (v->tab == i) {
            ui_flat(NAV_X+3,NAV_Y+i*40+3,NAV_W-6,2,5);
            ui_flat(NAV_X+3,NAV_Y+i*40+NAV_H-5,NAV_W-6,2,5);
            ui_flat(NAV_X+3,NAV_Y+i*40+3,2,NAV_H-6,5);
            ui_flat(NAV_X+NAV_W-5,NAV_Y+i*40+3,2,NAV_H-6,5);
        }
    }
    ui_panel(164,52,462,370,0);
    if (v->tab == SETTINGS_FILES) {
        /* The saved games' screen is files.c's, made of the same widgets. */
        files_draw(v->hover);
    } else if (v->tab == SETTINGS_KEYS) {
        ui_text(180,64,210,ui_tr("ACTION / KEY"),0);
        ui_button(400,54,208,30,v->classic ? "MOUSE: CURSOR" : "MOUSE: VIEW",0,v->hover == SETTINGS_MOUSE);
        ui_region(SETTINGS_MOUSE,400,54,208,30);
        for (i = 0; i < SETTINGS_ROWS; i++) {
            n = v->scroll+i;
            if (n >= SETTINGS_BINDINGS) break;
            ui_panel(174,ROW_Y+i*ROW_H,422,ROW_H,0);
            ui_region(SETTINGS_BIND+n,174,ROW_Y+i*ROW_H,422,ROW_H);
            ui_flat(414,ROW_Y+i*ROW_H+2,1,ROW_H-4,0);
            ui_text(182,ROW_Y+i*ROW_H+7,224,ui_tr(g_names[n]),0);
            ui_text(424,ROW_Y+i*ROW_H+7,162,v->waiting == n ? ui_tr("Press a key...") : v->bindings[n] ? ui_tr(v->bindings[n]) : "",0);
            if (v->hover == SETTINGS_BIND+n || v->waiting == n) ui_flat(175,ROW_Y+i*ROW_H+22,420,1,4);
        }
        ui_panel(601,ROW_Y,18,SETTINGS_ROWS*ROW_H,0);
        ui_button(601,ROW_Y,18,24,"^",0,v->hover == SETTINGS_UP);
        ui_button(601,ROW_Y+(SETTINGS_ROWS-1)*ROW_H,18,24,"v",0,v->hover == SETTINGS_DOWN);
        ui_region(SETTINGS_UP,601,ROW_Y,18,24);
        ui_region(SETTINGS_DOWN,601,ROW_Y+(SETTINGS_ROWS-1)*ROW_H,18,24);
        ui_panel(604,ROW_Y+28+v->scroll*20,12,70,0);
        if (!v->note) ui_paragraph(180,388,428,30,"Click an action to bind it. Mouse wheel scrolls. Escape cancels a pending binding.");
    } else if (v->tab == SETTINGS_GRAPHICS) {
        int row, y, thumb;
        ui_text(182, 68, 420, ui_tr("DISPLAY AND RENDERING"), 0);
        for (row = v->gfx_scroll; row < GFX_ROWS && row < v->gfx_scroll + SETTINGS_GFX_ROWS; row++) {
            y = GFX_LIST_Y + (row - v->gfx_scroll) * GFX_ROW_H;
            if (g_gfx_rows[row].kind == GFX_ROW_RES)
                gfx_resolution_row(v, y, v->hover == SETTINGS_GFX_RESOLUTION);
            else if (g_gfx_rows[row].kind == GFX_ROW_CHOICE)
                gfx_choice_row(y, g_gfx_rows[row].label, gfx_value(v, g_gfx_rows[row].action),
                               v->hover == g_gfx_rows[row].action);
            else if (g_gfx_rows[row].kind == GFX_ROW_TOGGLE)
                gfx_toggle_row(y, g_gfx_rows[row].label, gfx_value(v, g_gfx_rows[row].action),
                               v->hover == g_gfx_rows[row].action);
            else {
                const Slider *slider;
                slider = slider_desc(g_gfx_rows[row].slider_action);
                if (slider) gfx_slider_row(slider, y, gfx_value(v, slider->action));
            }
            /* A slider answers only on its track (its own region); every other row answers across. */
            if (g_gfx_rows[row].kind != GFX_ROW_SLIDER) ui_region(g_gfx_rows[row].action, 174, y, 444, GFX_ROW_H);
        }
        ui_panel(601, GFX_LIST_Y, 18, GFX_LIST_H, 0);
        ui_button(601, GFX_LIST_Y, 18, 24, "^", 0, v->hover == SETTINGS_UP);
        ui_button(601, GFX_LIST_Y + GFX_LIST_H - 24, 18, 24, "v", 0, v->hover == SETTINGS_DOWN);
        ui_region(SETTINGS_UP, 601, GFX_LIST_Y, 18, 24);
        ui_region(SETTINGS_DOWN, 601, GFX_LIST_Y + GFX_LIST_H - 24, 18, 24);
        if (GFX_ROWS > SETTINGS_GFX_ROWS) {
            thumb = v->gfx_scroll * (GFX_LIST_H - 70) / (GFX_ROWS - SETTINGS_GFX_ROWS);
            ui_panel(604, GFX_LIST_Y + 28 + thumb, 12, 70, 0);
        }
        ui_paragraph(180, 360, 420, 28,
                  "Every control on this tab is a draft until Apply. Texture and sprite detail (Low a quarter size, Medium half, High full) send the pictures again on Apply. A resolution shows at the next start.");
    } else if (v->tab == SETTINGS_AUDIO) {
        ui_text(182,68,420,ui_tr("VOLUME"),0);
        slider_draw(slider_desc(SETTINGS_SFX_SLIDER), v->sfx_volume);
        slider_draw(slider_desc(SETTINGS_MUSIC_SLIDER), v->music_volume);
        ui_panel(180,224,432,140,0);
        ui_text(194,240,396,ui_tr("SOUND AND MUSIC"),0);
        ui_paragraph(194,266,396,90,"Adjust sound effects and music independently. Zero silences that channel. Default volume is 80 percent. Apply to keep your changes.");
    } else if (v->tab == SETTINGS_GAMEPLAY) {
        ui_text(182,68,420,ui_tr("GAMEPLAY"),0);
        ui_panel(174,98,444,42,0);
        ui_text(184,113,210,ui_tr("HEAD BOBBING"),0);
        ui_button(426,102,184,34,v->bob ? "ON" : "OFF",0,v->hover == SETTINGS_BOB);
        ui_region(SETTINGS_BOB,174,98,444,42);
        ui_panel(174,154,444,42,0);
        ui_text(184,169,210,ui_tr("FULL HUD"),0);
        ui_button(426,158,184,34,v->full_hud ? "ON" : "OFF",0,v->hover == SETTINGS_FULL_HUD);
        ui_region(SETTINGS_FULL_HUD,174,154,444,42);
        ui_panel(174,210,444,42,0);
        ui_text(184,225,210,ui_tr("TOGGLE CROUCH"),0);
        ui_button(426,214,184,34,v->crouch_toggle ? "ON" : "OFF",0,v->hover == SETTINGS_CROUCH_TOGGLE);
        ui_region(SETTINGS_CROUCH_TOGGLE,174,210,444,42);
        ui_panel(180,266,432,104,0);
        ui_paragraph(194,280,396,84,"Head bobbing moves the camera as you walk. Full HUD shows the bottom bar and portrait; turn it off to hide the bar (vitals and compass remain). Toggle Crouch makes the crouch key flip between crouched and standing; off, you crouch only while holding it. Defaults: bobbing off, Full HUD on, Toggle Crouch off.");
    }

    ui_button(18,430,136,32,v->tab == SETTINGS_FILES ? (v->confirm_exit ? "CONFIRM EXIT" : "EXIT GAME") : "DEFAULT",1,v->hover == (v->tab == SETTINGS_FILES ? SETTINGS_EXIT : SETTINGS_DEFAULT));
    if (v->tab != SETTINGS_FILES) ui_button(170,430,136,32,"APPLY",2,v->hover == SETTINGS_APPLY);
    ui_button(500,430,122,32,"BACK",1,v->hover == SETTINGS_BACK);
    ui_region(v->tab == SETTINGS_FILES ? SETTINGS_EXIT : SETTINGS_DEFAULT,18,430,136,32);
    if (v->tab != SETTINGS_FILES) ui_region(SETTINGS_APPLY,170,430,136,32);
    ui_region(SETTINGS_BACK,500,430,122,32);
    /* The saved games' screen keeps its own note where the buttons are. */
    if (v->tab != SETTINGS_FILES) {
        if (v->note) ui_paragraph(180,382,420,36,v->note);
        else if (v->dirty) ui_text(322,442,170,ui_tr("Unapplied changes"),1);
    }
}
