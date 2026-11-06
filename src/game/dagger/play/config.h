#ifndef DF_CONFIG_H
#define DF_CONFIG_H

#include "engine/platform/input.h"

/*
 * gfall.cfg: every setting the player can change, as text, one key=value a
 * line, beside the program. Read docs/game.md (Menus, The settings file).
 *
 *   # a comment                  (a line that starts with #)
 *   key.forward=W                (a key by the name the bind screen shows)
 *   mouse=cursor                 (or view)
 *   resolution=640x480
 *   texture_detail=high          (low, medium, high)
 *   fog=on                       (on, off; 1, 0, yes, no, true, false)
 *   fov=75
 *
 * A line that is no key we know, or whose value is out of range, is skipped
 * and counted; the rest still load. A key left out keeps what the program
 * had. The Settings screens write the whole file on Apply, comments and all,
 * so a hand edit of a value lasts and a hand-made comment does not.
 *
 * This module is the file's text and the settings as numbers, and nothing
 * else: keys.c reads the live state into a Config and a Config back into it.
 * Plain C: hostcheck runs it.
 */

typedef struct Config {
    /* The Windows virtual key of each action (INPUT_*); 0 for none. Escape (INPUT_MENU) is never here. */
    int key[INPUT_ACTIONS];
    /* 1 the classic pointer (Cursor), 0 mouse look (View). */
    int mouse_cursor;
    int fov;
    int far_clip;
    int fog;
    int crisp_sprites;
    int vsync;
    int dungeon_lights;
    int ambient_light;
    int light_range;
    int light_shadow;
    int weather_view;
    int flora;
    int flora_density;
    int flora_sight;
    /* 0 low, 1 medium, 2 high. */
    int texture_detail;
    int sprite_detail;
    int mode_w;
    int mode_h;
    int sfx_volume;
    int music_volume;
    /* 1 the bottom bar shows. */
    int full_hud;
    int head_bob;
    int toggle_crouch;
} Config;

#define CONFIG_FILE "gfall.cfg"
/* A formatted file is shorter than this. */
#define CONFIG_MAX 6144

/* Classic's key for an action, as a Windows virtual key; 0 for none (the menu's Escape is the engine's). */
int config_default_key(int action);
/* The name an action goes by in the file ("forward"), or 0 for one that is not kept (INPUT_MENU). */
const char *config_action_name(int action);
/*
 * text, n bytes (no terminator needed), applied over c: each line it can use sets its field, anything else is
 * counted. Returns how many lines were unusable; *first_bad is the number of the first one (from 1), 0 for none.
 */
int config_parse(const char *text, int n, Config *c, int *first_bad);
/* c as the file's text into out (cap bytes, CRLF lines, never ended with a NUL byte counted): its length, or -1 when it will not fit. */
int config_format(const Config *c, char *out, int cap);
/* The screen's mode a text names, nothing else read: 1 when it names a sane one. */
int config_peek_mode(const char *text, int n, int *w, int *h);

#endif
