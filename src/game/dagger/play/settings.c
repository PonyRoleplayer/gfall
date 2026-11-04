#include "game/dagger/play/settings.h"
#include "game/dagger/formats/smallfont.h"
#include "engine/core/mem.h"
#include "engine/platform/input.h"

/* A handful of swatches, not a full-screen texture: 96 KB of source texels
 * serves every tab. Frames, text and the moving thumb stay independent, so
 * an idle menu or a drag performs no file reads, allocations or PCI uploads. */
static MemHeap *g_heap;
static df_u8 *g_atlas[SETTINGS_ATLASES];
static df_u8 g_rgb[768];
static df_u8 g_width[2][256];
static int g_height[2];
static SettingsTranslate g_translate;
static SettingsQuad g_quad;
static void *g_ctx;
static const int g_side[] = {128, 256, 128};
static const char *const g_names[] = {
    "Move forward", "Move backward", "Turn left", "Turn right",
    "Strafe left", "Strafe right", "Jump", "Run", "Rest", "Attack / swing",
    "Swap hands", "Character sheet", "Inventory", "Activate", "Toggle cursor",
    "Look up", "Look down", "Center view", "Free look", "Automap", "Travel map"
};
static const int g_actions[] = {
    INPUT_FORWARD, INPUT_BACK, INPUT_TURN_LEFT, INPUT_TURN_RIGHT,
    INPUT_LEFT, INPUT_RIGHT, INPUT_JUMP, INPUT_RUN, INPUT_REST, INPUT_ATTACK,
    INPUT_SWAP_HANDS, INPUT_CHARACTER, INPUT_INVENTORY, INPUT_USE, INPUT_CURSOR,
    INPUT_LOOK_UP, INPUT_LOOK_DOWN, INPUT_LOOK_CENTER, INPUT_FREELOOK, INPUT_MAP, INPUT_TRAVEL_MAP
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
typedef enum { GFX_ROW_RES, GFX_ROW_SLIDER, GFX_ROW_TOGGLE } GfxRowKind;
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

static const char *tr(const char *s)
{
    const char *translated;
    translated = g_translate ? g_translate(s) : 0;
    return translated ? translated : s;
}

void settings_translate(SettingsTranslate translate) { g_translate = translate; }

int settings_open(void)
{
    MemHeap *was;
    SmallFont font;
    int i, f, x, y;
    df_u32 seed;

    if (g_heap) return 1;
    g_heap = mem_heap_new();
    if (!g_heap) return 0;
    was = mem_heap_use(g_heap);
    for (i = 0; i < SETTINGS_ATLASES; i++) {
        g_atlas[i] = (df_u8 *)mem_alloc((df_u32)g_side[i] * g_side[i]);
        if (!g_atlas[i]) goto failed;
        mem_zero(g_atlas[i], (df_u32)g_side[i] * g_side[i]);
    }
    /* Grey ramp plus muted rust and moss ramps. The original-font glyphs
     * receive a gold bevel; none of the panel textures contain text. */
    for (i = 0; i < 256; i++) {
        g_rgb[i * 3] = g_rgb[i * 3 + 1] = g_rgb[i * 3 + 2] = (df_u8)i;
    }
    for (i = 0; i < 32; i++) {
        g_rgb[(160 + i) * 3] = (df_u8)(68 + i * 3);
        g_rgb[(160 + i) * 3 + 1] = (df_u8)(18 + i);
        g_rgb[(160 + i) * 3 + 2] = 8;
        g_rgb[(192 + i) * 3] = (df_u8)(28 + i);
        g_rgb[(192 + i) * 3 + 1] = (df_u8)(45 + i * 2);
        g_rgb[(192 + i) * 3 + 2] = 10;
    }
    g_rgb[240*3] = 250; g_rgb[240*3+1] = 224; g_rgb[240*3+2] = 38;
    g_rgb[241*3] = 255; g_rgb[241*3+1] = 250; g_rgb[241*3+2] = 113;
    g_rgb[242*3] = 135; g_rgb[242*3+1] = 107; g_rgb[242*3+2] = 8;
    /* Deterministic fine grain with a coarser mottling, generated once.
     * Three swatches occupy 64-square cells; final row holds flat colours. */
    seed = 0x67574a21u;
    for (y = 0; y < 128; y++) for (x = 0; x < 128; x++) {
        int grain;
        seed = seed * 1664525u + 1013904223u;
        grain = (int)(seed >> 29) + ((x / 5 + y / 3) % 3) * 2;
        g_atlas[0][y * 128 + x] = (df_u8)(y < 64 ? 49 + grain : x < 64 ? 164 + grain : 196 + grain);
    }
    for (x = 0; x < 64; x++) g_atlas[0][127*128+x] = (df_u8)(x == 0 ? 18 : x == 1 ? 104 : x == 2 ? 143 : x == 3 ? 32 : x == 4 ? 240 : 180);
    for (f = 0; f < 2; f++) {
        int cell, side, scale;
        /* FONT0003 is classic's compact block face. Enlarge its strokes
         * for native-resolution labels, then bevel at output pixels; this
         * keeps their edges sharp and their shadows one pixel wide. */
        if (!smallfont_load(&font, "FONT0003.FNT")) goto failed;
        scale = f ? 1 : 2;
        cell = f ? 8 : 16;
        side = g_side[f + 1];
        if (font.height * scale + 1 > cell) goto failed;
        g_height[f] = font.height * scale + 1;
        for (i = 33; i < 256; i++) {
            char word[2];
            df_u8 glyph[64];
            int width, sx, sy;
            word[0] = (char)i; word[1] = 0;
            width = smallfont_width(&font, word)-1;
            if (width < 0) width = 0;
            if (width > 7) width = 7;
            g_width[f][i] = (df_u8)(width * scale + 1);
            sx = (i % 16) * cell; sy = (i / 16) * cell;
            mem_zero(glyph, sizeof(glyph));
            smallfont_draw(&font, glyph, 8, 8, 0, 0, 240, -1, word);
            for (y = 0; y < font.height*scale; y++) for (x = 0; x < width*scale; x++) {
                int at;
                if (!glyph[(y/scale)*8+x/scale]) continue;
                at = (sy+y)*side+sx+x;
                g_atlas[f+1][at+side+1] = 18;
            }
            for (y = 0; y < font.height*scale; y++) for (x = 0; x < width*scale; x++) {
                int at;
                if (!glyph[(y/scale)*8+x/scale]) continue;
                at = (sy+y)*side+sx+x;
                g_atlas[f+1][at] = (df_u8)(!y || !glyph[((y-1)/scale)*8+x/scale] ? 241 :
                    y == font.height*scale-1 || !glyph[((y+1)/scale)*8+x/scale] ? 242 : 240);
            }
        }
        g_width[f][' '] = (df_u8)(3*scale+1);
    }
    mem_heap_use(was);
    return 1;
failed:
    mem_heap_use(was);
    settings_close();
    return 0;
}

void settings_close(void)
{
    int i;
    if (g_heap) mem_heap_delete(g_heap);
    g_heap = 0;
    for (i = 0; i < SETTINGS_ATLASES; i++) g_atlas[i] = 0;
}
const df_u8 *settings_atlas(int i, int *side)
{
    if (i < 0 || i >= SETTINGS_ATLASES) return 0;
    *side = g_side[i];
    return g_atlas[i];
}
const df_u8 *settings_palette(void) { return g_rgb; }

static void flat(int x, int y, int w, int h, int color)
{
    if (w > 0 && h > 0) g_quad(g_ctx, 0, color, 127, 1, 1, x, y, w, h);
}
static void panel(int x, int y, int w, int h, int style)
{
    /* A bounded swatch grid and six thin bevels. Small controls need one
     * patch; only long/tall panels subdivide, at most four per axis. */
    {
        int row, col, tile_w, tile_h;
        /* At most sixteen pieces per panel. Small grain stays roughly
         * square instead of stretching into streaks down a tall sidebar. */
        tile_w = w > 128 ? (w + 3) / 4 : w;
        tile_h = h > 128 ? (h + 3) / 4 : h;
        for (row = 0; row < h; row += tile_h) for (col = 0; col < w; col += tile_w)
            g_quad(g_ctx, 0, style == 2 ? 64 : 0, style ? 64 : 0, 64, 63, x+col, y+row,
                   col+tile_w > w ? w-col : tile_w, row+tile_h > h ? h-row : tile_h);
    }
    flat(x, y, w, 1, 2); flat(x, y, 1, h, 2);
    flat(x+1, y+1, w-2, 1, 1); flat(x+1, y+1, 1, h-2, 1);
    flat(x, y+h-1, w, 1, 0); flat(x+w-1, y, 1, h, 0);
}
static int width(int font, const char *s)
{
    int n;
    n = 0;
    while (*s) n += g_width[font][(unsigned char)*s++];
    return n;
}
static void text(int x, int y, int max_w, const char *s, int font)
{
    int right, cell;
    right = x + max_w;
    cell = font ? 8 : 16;
    while (*s && x < right) {
        unsigned c;
        int w;
        c = (unsigned char)*s++;
        w = g_width[font][c];
        if (c != ' ' && w) {
            int draw_w;
            draw_w = w < right-x ? w : right-x;
            g_quad(g_ctx, font+1, (c%16)*cell, (c/16)*cell, draw_w, g_height[font], x, y, draw_w, g_height[font]);
        }
        x += w;
    }
}
static void centered(int x, int y, int w, const char *s)
{
    int font, n;
    s = tr(s);
    font = width(0, s) <= w-12 ? 0 : 1;
    n = width(font, s);
    text(x + (n < w ? (w-n)/2 : 6), y, w-12, s, font);
}
static void button(int x, int y, int w, int h, const char *s, int style, int hot)
{
    panel(x, y, w, h, style);
    if (hot) {
        flat(x+2, y+2, w-4, 1, 4); flat(x+2, y+h-3, w-4, 1, 4);
    }
    centered(x, y+(h-15)/2, w, s);
}
static void paragraph(int x, int y, int w, int h, const char *s)
{
    char line[128];
    int used, pixels, last_space, i;
    s = tr(s);
    /* Bounded wrapping at spaces, with a hard split for an unusually long
     * translated word. There is never an off-panel text draw. */
    while (*s && h >= 15) {
        used = pixels = 0; last_space = -1;
        while (s[used] && s[used] != '\n' && used < 126) {
            int next;
            next = g_width[0][(unsigned char)s[used]];
            if (pixels+next > w) break;
            if (s[used] == ' ') last_space = used;
            pixels += next; used++;
        }
        if (!used) break;
        if (s[used] && s[used] != '\n' && last_space > 0) used = last_space;
        for (i = 0; i < used; i++) line[i] = s[i];
        line[used] = 0;
        text(x, y, w, line, 0);
        s += used;
        while (*s == ' ' || *s == '\n') s++;
        y += 16; h -= 16;
    }
}

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
    panel(174, y, 444, GFX_ROW_H, 0);
    text(184, y + 8, 132, tr(slider->label), 0);
    panel(TRACK_X - 5, y + 4, TRACK_W + 10, 18, 0);
    flat(TRACK_X, y + 9, TRACK_W, 8, 0);
    thumb = (value - slider->minimum) * TRACK_W / (slider->maximum - slider->minimum);
    flat(TRACK_X, y + 9, thumb, 8, 4);
    panel(TRACK_X + thumb - 4, y + 4, 8, 18, 0);
    n = 0;
    if (value >= 10000) number[n++] = (char)('0' + value / 10000 % 10);
    if (value >= 1000) number[n++] = (char)('0' + value / 1000 % 10);
    if (value >= 100) number[n++] = (char)('0' + value / 100 % 10);
    if (value >= 10) number[n++] = (char)('0' + value / 10 % 10);
    number[n++] = (char)('0' + value % 10);
    number[n] = 0;
    text(540, y + 6, 64, number, 0);
}

static void gfx_toggle_row(int y, const char *label, int on, int hot)
{
    panel(174, y, 444, GFX_ROW_H, 0);
    text(184, y + 8, 220, tr(label), 0);
    button(426, y + 3, 184, 20, on ? "ON" : "OFF", 0, hot);
}

static void gfx_resolution_row(int y, int hot)
{
    panel(174, y, 444, GFX_ROW_H, 0);
    text(184, y + 8, 220, tr("RESOLUTION"), 0);
    button(426, y + 3, 184, 20, "640 x 480", 0, hot);
}

int settings_resolution_w(int index)
{
    return index == 0 ? 640 : 0;
}

int settings_resolution_h(int index)
{
    return index == 0 ? 480 : 0;
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
    panel(174,y-14,444,42,0);
    text(184,y+1,144,tr(slider->label),0);
    panel(TRACK_X-5,y-5,TRACK_W+10,20,0);
    flat(TRACK_X,y,TRACK_W,10,0);
    thumb = (value-slider->minimum)*TRACK_W/(slider->maximum-slider->minimum);
    flat(TRACK_X,y,thumb,10,4);
    panel(TRACK_X+thumb-5,y-5,10,20,0);
    n = 0;
    if (value >= 100) number[n++] = (char)('0'+value/100);
    if (value >= 10) number[n++] = (char)('0'+value/10%10);
    number[n++] = (char)('0'+value%10);
    if (slider->tab == SETTINGS_AUDIO) number[n++] = '%';
    number[n] = 0;
    text(540,y-1,64,number,0);
}
static int inside(int x, int y, int left, int top, int w, int h)
{
    return x >= left && y >= top && x < left+w && y < top+h;
}
int settings_hit(int tab, int scroll, int x, int y)
{
    int i;
    for (i = 0; i < SETTINGS_TABS; i++) if (inside(x,y,NAV_X,NAV_Y+i*40,NAV_W,NAV_H)) return SETTINGS_TAB_FILES+i;
    if (inside(x,y,500,430,122,32)) return SETTINGS_BACK;
    if (inside(x,y,18,430,136,32)) return tab == SETTINGS_FILES ? SETTINGS_EXIT : SETTINGS_DEFAULT;
    if (tab != SETTINGS_FILES && inside(x,y,170,430,136,32)) return SETTINGS_APPLY;
    if (tab == SETTINGS_FILES) {
        if (inside(x,y,190,112,190,44)) return SETTINGS_LOAD;
        if (inside(x,y,402,112,190,44)) return SETTINGS_SAVE;
    } else if (tab == SETTINGS_KEYS) {
        if (inside(x,y,400,54,208,30)) return SETTINGS_MOUSE;
        if (inside(x,y,601,ROW_Y,18,24)) return SETTINGS_UP;
        if (inside(x,y,601,ROW_Y+(SETTINGS_ROWS-1)*ROW_H,18,24)) return SETTINGS_DOWN;
        if (inside(x,y,BODY_X+4,ROW_Y,422,SETTINGS_ROWS*ROW_H)) {
            i = (y-ROW_Y)/ROW_H+scroll;
            if (i < SETTINGS_BINDINGS) return SETTINGS_BIND+i;
        }
    } else if (tab == SETTINGS_GAMEPLAY) {
        if (inside(x,y,174,98,444,42)) return SETTINGS_BOB;
        if (inside(x,y,174,154,444,42)) return SETTINGS_FULL_HUD;
    } else if (tab == SETTINGS_GRAPHICS) {
        int row, top;
        if (inside(x, y, 601, GFX_LIST_Y, 18, GFX_LIST_H)) {
            if (inside(x, y, 601, GFX_LIST_Y, 18, 24)) return SETTINGS_UP;
            if (inside(x, y, 601, GFX_LIST_Y + GFX_LIST_H - 24, 18, 24)) return SETTINGS_DOWN;
        }
        if (y < GFX_LIST_Y || y >= GFX_LIST_Y + GFX_LIST_H) return SETTINGS_NOTHING;
        row = scroll + (y - GFX_LIST_Y) / GFX_ROW_H;
        if (row < 0 || row >= GFX_ROWS) return SETTINGS_NOTHING;
        top = GFX_LIST_Y + (row - scroll) * GFX_ROW_H;
        if (g_gfx_rows[row].kind == GFX_ROW_SLIDER) {
            if (inside(x, y, TRACK_X - 8, top + 2, TRACK_W + 16, GFX_ROW_H - 4))
                return g_gfx_rows[row].slider_action;
            return SETTINGS_NOTHING;
        }
        if (inside(x, y, 174, top, 444, GFX_ROW_H)) return g_gfx_rows[row].action;
    }
    for (i = 0; i < SLIDERS; i++) {
        if (tab == g_sliders[i].tab && g_sliders[i].y != 0
            && inside(x,y,TRACK_X-8,g_sliders[i].y-10,TRACK_W+16,32)) return g_sliders[i].action;
    }
    return SETTINGS_NOTHING;
}

void settings_draw(const SettingsView *v, SettingsQuad quad, void *ctx)
{
    int i, n;
    const char *title;
    if (!g_heap || !quad || v->tab < 0 || v->tab >= SETTINGS_TABS) return;
    g_quad = quad; g_ctx = ctx;
    panel(8,8,624,464,0);
    panel(14,14,612,32,0);
    title = g_titles[v->tab];
    centered(14,23,612,title);
    panel(14,52,144,370,0);
    panel(18,56,136,44,0);
    text(26,64,120,tr("RENDERER"),1);
    text(26,80,120,tr("GLIDE (VOODOO)"),1);
    for (i = 0; i < SETTINGS_TABS; i++) {
        const char *label;
        label = g_tab_labels[i];
        button(NAV_X,NAV_Y+i*40,NAV_W,NAV_H,label,0,v->hover == SETTINGS_TAB_FILES+i);
        if (v->tab == i) {
            flat(NAV_X+3,NAV_Y+i*40+3,NAV_W-6,2,5);
            flat(NAV_X+3,NAV_Y+i*40+NAV_H-5,NAV_W-6,2,5);
            flat(NAV_X+3,NAV_Y+i*40+3,2,NAV_H-6,5);
            flat(NAV_X+NAV_W-5,NAV_Y+i*40+3,2,NAV_H-6,5);
        }
    }
    panel(164,52,462,370,0);
    if (v->tab == SETTINGS_FILES) {
        text(190,76,400,tr("YOUR JOURNEY"),0);
        button(190,112,190,44,"LOAD GAME",0,v->hover == SETTINGS_LOAD);
        button(402,112,190,44,"SAVE GAME",2,v->hover == SETTINGS_SAVE);
        paragraph(190,184,400,90,"Continue an earlier adventure or save your current progress. Choose a save slot on the next screen.");
        panel(184,286,420,90,0);
        text(198,300,390,tr("SAVED GAMES"),0);
        paragraph(198,324,390,50,"Six slots are available. Saving to an occupied slot replaces that saved game.");
    } else if (v->tab == SETTINGS_KEYS) {
        text(180,64,210,tr("ACTION / KEY"),0);
        button(400,54,208,30,v->classic ? "MOUSE: CURSOR" : "MOUSE: VIEW",0,v->hover == SETTINGS_MOUSE);
        for (i = 0; i < SETTINGS_ROWS; i++) {
            n = v->scroll+i;
            if (n >= SETTINGS_BINDINGS) break;
            panel(174,ROW_Y+i*ROW_H,422,ROW_H,0);
            flat(414,ROW_Y+i*ROW_H+2,1,ROW_H-4,0);
            text(182,ROW_Y+i*ROW_H+7,224,tr(g_names[n]),0);
            text(424,ROW_Y+i*ROW_H+7,162,v->waiting == n ? tr("Press a key...") : v->bindings[n] ? tr(v->bindings[n]) : "",0);
            if (v->hover == SETTINGS_BIND+n || v->waiting == n) flat(175,ROW_Y+i*ROW_H+22,420,1,4);
        }
        panel(601,ROW_Y,18,SETTINGS_ROWS*ROW_H,0);
        button(601,ROW_Y,18,24,"^",0,v->hover == SETTINGS_UP);
        button(601,ROW_Y+(SETTINGS_ROWS-1)*ROW_H,18,24,"v",0,v->hover == SETTINGS_DOWN);
        panel(604,ROW_Y+28+v->scroll*20,12,70,0);
        if (!v->note) paragraph(180,388,428,30,"Click an action to bind it. Mouse wheel scrolls. Escape cancels a pending binding.");
    } else if (v->tab == SETTINGS_GRAPHICS) {
        int row, y, thumb;
        text(182, 68, 420, tr("DISPLAY AND RENDERING"), 0);
        for (row = v->gfx_scroll; row < GFX_ROWS && row < v->gfx_scroll + SETTINGS_GFX_ROWS; row++) {
            y = GFX_LIST_Y + (row - v->gfx_scroll) * GFX_ROW_H;
            if (g_gfx_rows[row].kind == GFX_ROW_RES)
                gfx_resolution_row(y, v->hover == SETTINGS_GFX_RESOLUTION);
            else if (g_gfx_rows[row].kind == GFX_ROW_TOGGLE)
                gfx_toggle_row(y, g_gfx_rows[row].label, gfx_value(v, g_gfx_rows[row].action),
                               v->hover == g_gfx_rows[row].action);
            else {
                const Slider *slider;
                slider = slider_desc(g_gfx_rows[row].slider_action);
                if (slider) gfx_slider_row(slider, y, gfx_value(v, slider->action));
            }
        }
        panel(601, GFX_LIST_Y, 18, GFX_LIST_H, 0);
        button(601, GFX_LIST_Y, 18, 24, "^", 0, v->hover == SETTINGS_UP);
        button(601, GFX_LIST_Y + GFX_LIST_H - 24, 18, 24, "v", 0, v->hover == SETTINGS_DOWN);
        if (GFX_ROWS > SETTINGS_GFX_ROWS) {
            thumb = v->gfx_scroll * (GFX_LIST_H - 70) / (GFX_ROWS - SETTINGS_GFX_ROWS);
            panel(604, GFX_LIST_Y + 28 + thumb, 12, 70, 0);
        }
        paragraph(180, 360, 420, 28,
                  "Every control on this tab is a draft until Apply. Resolution does not change video mode yet.");
    } else if (v->tab == SETTINGS_AUDIO) {
        text(182,68,420,tr("VOLUME"),0);
        slider_draw(slider_desc(SETTINGS_SFX_SLIDER), v->sfx_volume);
        slider_draw(slider_desc(SETTINGS_MUSIC_SLIDER), v->music_volume);
        panel(180,224,432,140,0);
        text(194,240,396,tr("SOUND AND MUSIC"),0);
        paragraph(194,266,396,90,"Adjust sound effects and music independently. Zero silences that channel. Default volume is 80 percent. Apply to keep your changes.");
    } else if (v->tab == SETTINGS_GAMEPLAY) {
        text(182,68,420,tr("GAMEPLAY"),0);
        panel(174,98,444,42,0);
        text(184,113,210,tr("HEAD BOBBING"),0);
        button(426,102,184,34,v->bob ? "ON" : "OFF",0,v->hover == SETTINGS_BOB);
        panel(174,154,444,42,0);
        text(184,169,210,tr("FULL HUD"),0);
        button(426,158,184,34,v->full_hud ? "ON" : "OFF",0,v->hover == SETTINGS_FULL_HUD);
        panel(180,224,432,144,0);
        paragraph(194,240,396,112,"Head bobbing moves the camera as you walk. Full HUD shows the bottom bar and portrait. Turn it off to hide the bar; vitals and compass remain. Defaults: bobbing off, Full HUD on.");
    }

    button(18,430,136,32,v->tab == SETTINGS_FILES ? (v->confirm_exit ? "CONFIRM EXIT" : "EXIT GAME") : "DEFAULT",1,v->hover == (v->tab == SETTINGS_FILES ? SETTINGS_EXIT : SETTINGS_DEFAULT));
    if (v->tab != SETTINGS_FILES) button(170,430,136,32,"APPLY",2,v->hover == SETTINGS_APPLY);
    button(500,430,122,32,"BACK",1,v->hover == SETTINGS_BACK);
    if (v->note) paragraph(180,382,420,36,v->note);
    else if (v->dirty) text(322,442,170,tr("Unapplied changes"),1);
}
