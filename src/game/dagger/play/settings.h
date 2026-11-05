#ifndef DF_SETTINGS_H
#define DF_SETTINGS_H

#include "engine/core/df.h"

/* Stone settings shell, in 640 x 480 top-down coordinates. Read docs/game.md
 * (Menus). Plain C: the same quads render on Glide and in hostcheck previews.
 * Artwork contains NO words. Text is emitted from strings through glyph atlases;
 * a translation provider can replace English labels without changing textures.
 * Only menu.c owns input bindings, persistence and picture handles. */
#define SETTINGS_W 640
#define SETTINGS_H 480
#define SETTINGS_ROWS 12
#define SETTINGS_BINDINGS 22
#define SETTINGS_ATLASES 3

enum { SETTINGS_FILES, SETTINGS_KEYS, SETTINGS_GRAPHICS, SETTINGS_AUDIO, SETTINGS_GAMEPLAY, SETTINGS_TABS };
enum {
    SETTINGS_NOTHING = -1,
    SETTINGS_TAB_FILES, SETTINGS_TAB_KEYS, SETTINGS_TAB_GRAPHICS,
    SETTINGS_TAB_AUDIO, SETTINGS_TAB_GAMEPLAY,
    SETTINGS_LOAD, SETTINGS_SAVE, SETTINGS_EXIT, SETTINGS_BACK,
    SETTINGS_DEFAULT, SETTINGS_APPLY, SETTINGS_MOUSE,
    SETTINGS_UP, SETTINGS_DOWN, SETTINGS_SLIDER,
    SETTINGS_SFX_SLIDER, SETTINGS_MUSIC_SLIDER, SETTINGS_BOB, SETTINGS_FULL_HUD,
    SETTINGS_CROUCH_TOGGLE,
    SETTINGS_BIND = 32
};
enum {
    SETTINGS_GFX_RESOLUTION = 64,
    SETTINGS_GFX_FAR_SLIDER,
    SETTINGS_GFX_AMBIENT_SLIDER,
    SETTINGS_GFX_LIGHTRANGE_SLIDER,
    SETTINGS_GFX_FLORADENSITY_SLIDER,
    SETTINGS_GFX_FLORASIGHT_SLIDER,
    SETTINGS_GFX_FOG,
    SETTINGS_GFX_SPRITEPOINT,
    SETTINGS_GFX_VSYNC,
    SETTINGS_GFX_LIGHTS,
    SETTINGS_GFX_LIGHTSHADOW,
    SETTINGS_GFX_WEATHERSIM,
    SETTINGS_GFX_FLORA
};
#define SETTINGS_GFX_RESOLUTIONS 1
#define SETTINGS_GFX_ROWS 10
#define SETTINGS_GFX_ITEM_COUNT 14

/* Graphics tab draft: edited in memory until Apply. menu.c owns hold/commit. */
typedef struct SettingsGfxDraft {
    int resolution;              /* Only 0 (640x480) today. */
    int fov;                     /* Horizontal degrees, 45..120. */
    int far_clip;                /* World units. */
    int fog;
    int spritepoint;
    int vsync;
    int lights;
    int ambient;                 /* 0..255. */
    int lightrange;              /* 1..8. */
    int lightshadow;
    int weathersim;
    int flora;
    int floradensity;            /* 0..100. */
    int florasight;              /* 0..16384; 0 means each place's own. */
} SettingsGfxDraft;

typedef struct SettingsView {
    int tab;
    int scroll;
    int waiting;                 /* Binding row, or -1. */
    SettingsGfxDraft gfx;
    int gfx_scroll;
    int sfx_volume;              /* Draft percentages, 0..100; zero mutes. */
    int music_volume;
    int bob;                     /* Draft head-bobbing preference. */
    int full_hud;                /* On shows the bar; inverse of stored OPTS.full. */
    int crouch_toggle;           /* Draft: the crouch key flips instead of being held. */
    int classic;
    int hover;                   /* settings_hit(), or SETTINGS_NOTHING. */
    int dirty;
    int confirm_exit;
    const char *note;
    const char *bindings[SETTINGS_BINDINGS];
} SettingsView;

/* Snapshot live engine into draft and remember that snapshot for cancel. */
void settings_gfx_draft_hold(SettingsGfxDraft *draft);
/* Write draft to the engine (Apply). */
void settings_gfx_draft_commit(const SettingsGfxDraft *draft);
/* Restore the engine to the last hold snapshot (leave tab / failed Apply). */
void settings_gfx_draft_cancel(void);

int settings_open(void);
void settings_close(void);
const df_u8 *settings_atlas(int index, int *side);
const df_u8 *settings_palette(void);

typedef const char *(*SettingsTranslate)(const char *english);
void settings_translate(SettingsTranslate translate);

typedef void (*SettingsQuad)(void *ctx, int atlas, int sx, int sy, int sw, int sh,
                             int x, int y, int w, int h);
void settings_draw(const SettingsView *view, SettingsQuad quad, void *ctx);
int settings_hit(int tab, int scroll, int x, int y);
int settings_fov_at(int x);
int settings_slider_at(int action, int x);
int settings_binding_action(int row, int classic);

int settings_resolution_w(int index);
int settings_resolution_h(int index);
const char *settings_gfx_console_var(int action);

#endif
