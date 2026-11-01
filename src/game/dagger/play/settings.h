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
#define SETTINGS_BINDINGS 20
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
    SETTINGS_BIND = 32
};

typedef struct SettingsView {
    int tab;
    int scroll;
    int waiting;                 /* Binding row, or -1. */
    int fov;                     /* Draft horizontal degrees, 45..120. */
    int sfx_volume;              /* Draft percentages, 0..100; zero mutes. */
    int music_volume;
    int bob;                     /* Draft head-bobbing preference. */
    int full_hud;                /* On shows the bar; inverse of stored OPTS.full. */
    int classic;
    int hover;                   /* settings_hit(), or SETTINGS_NOTHING. */
    int dirty;
    int confirm_exit;
    const char *note;
    const char *bindings[SETTINGS_BINDINGS];
} SettingsView;

/* Atlas 0: 128-square stone/button swatches; 1: 256-square heading font;
 * 2: 128-square compact font. Palettes match, entry zero is transparent.
 * Private heap owns these until settings_close; no allocation during draw. */
int settings_open(void);
void settings_close(void);
const df_u8 *settings_atlas(int index, int *side);
const df_u8 *settings_palette(void);

/* A provider returns translated text for an English source string, or null
 * for the English fallback. Strings use the original font's byte encoding;
 * this hook does not promise Unicode shaping or fonts the game lacks. */
typedef const char *(*SettingsTranslate)(const char *english);
void settings_translate(SettingsTranslate translate);

/* A backend draws one atlas rectangle, nearest sampled, clipping index zero
 * only for font atlases. Coordinates are integers with a top-left origin. */
typedef void (*SettingsQuad)(void *ctx, int atlas, int sx, int sy, int sw, int sh,
                             int x, int y, int w, int h);
void settings_draw(const SettingsView *view, SettingsQuad quad, void *ctx);
int settings_hit(int tab, int scroll, int x, int y);
/* The track is shared by drawing and dragging. Outside endpoints clamp. */
int settings_fov_at(int x);
/* Shared slider mapping, or -1 when action is not a slider. The same
 * descriptor supplies drawing, hit testing and drag range. */
int settings_slider_at(int action, int x);
int settings_binding_action(int row, int classic);

#endif
