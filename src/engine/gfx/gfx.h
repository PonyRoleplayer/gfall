#ifndef ENG_GFX_H
#define ENG_GFX_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"

/*
 * The graphics call the game makes. Glide, the texture cache, and the
 * clipper sit behind this. Open the device, upload one level, draw frames.
 */

int gfx_startup(void *hwnd);
void gfx_shutdown(void);
const char *gfx_error(void);

/*
 * Sends the level palette and every image, budgeted to fit the 2MB
 * card. A second call replaces what the first sent.
 */
int gfx_prepare(const Level *level);
/*
 * Mip chains, default on. A change sends the prepared level again on
 * the next gfx_frame, so the picture shows it. See docs/glide.md.
 */
void gfx_set_mipmap(int on);
int gfx_mipmap(void);
/*
 * Debug only: TMU0 holds at most kb KB, as a smaller card would. 2048 is
 * a Voodoo 1's 2 MB; 0 (default) is the card's own. A change sends the
 * level again on the next frame. See docs/memory.md.
 */
void gfx_set_texcap(int kb);
int gfx_texcap(void);
/* How far lights reach, 1..8 times their radius; cuts the decals again. */
void gfx_set_light_range(int scale);
/* 1 (default): a light does not reach through a model in its way; cuts the decals again. */
void gfx_set_light_shadow(int on);

/*
 * Between places with the card left open: hold the last frame, dimmed,
 * with text in the middle, while the next one loads (gfx_prepare may be
 * called again under it); then let its first frames fade in from black
 * over secs. Only a temp heap ends between them, never the card.
 */
void gfx_hold(const char *text);
void gfx_fade_in(float secs);
/*
 * A place before its first frame (under the hold), from cam: its song
 * read and its sounds started, and, where pictures are on the card by
 * distance (a streamed world; a place loaded whole, its entities'),
 * every loaded picture's sharper copy sent at once, as near as each
 * one's models, sprites and entities stand, so the place does not
 * start in 8 x 8 copies sharpening a frame at a time and a song read in
 * its first frame (docs/streaming.md). A hitch: call it only while the
 * screen holds, after gfx_prepare and the camera are set.
 */
void gfx_prime(const Level *level, const Camera *cam);
/*
 * The last place's load on the card side, in cycles (prof_now): its
 * pictures (texcache_upload: sent, or put on residency with their chains
 * built), its light decals (light_build), and gfx_prime. A viewer turns
 * them into milliseconds (docs/viewers.md, the load line).
 */
void gfx_load_times(df_u32 *card, df_u32 *lights, df_u32 *prime);
/* Clear, draw what the scene says is visible, overlay, swap. */
void gfx_frame(const Camera *cam, const Level *level);
/*
 * 1 while the world stands still: the console is open, or the game's
 * pause is on, and gfx_frame runs none of it (the clock, the weather,
 * the doors and their use, the walker, the flames' frames). A game's
 * own simulation (its people, its creatures) stands still too while it
 * is: it skips its step, not its loading.
 */
int gfx_paused(void);
/*
 * The game's pause, for its screens and menus: 1 stands the world still
 * as the console does, but the frame is still drawn under the screen and
 * the keys still reach input_read. 0 lets it go on. The console's own
 * pause is apart from it.
 */
void gfx_set_pause(int on);
/*
 * 1 (default): the frame takes the use key itself (the left button or
 * E: docs/interact.md). 0: it polls nothing; the game, whose left button
 * attacks, calls interact_request_use on its own use key's edge.
 */
void gfx_set_use_key(int on);

/* Drawn after the frame and before the swap. 0 clears it. */
void gfx_set_extra(void (*fn)(void));
/* Drawn after the scene and before the HUD. 0 clears it. */
void gfx_set_under(void (*fn)(void));
/*
 * Distance fog. 1 is on, 0 is off. Default is on. Safe before
 * gfx_startup; the device applies it when it opens, and a later call
 * switches the mode while the device is open.
 */
void gfx_set_fog(int on);
int gfx_fog(void);
/*
 * Far clip in world units. Default is DF_FAR (1400). A later call takes
 * effect on the next frame. Values above DF_FAR_MAX are clamped. Fog, when
 * on, goes solid at the same fraction of this distance as 1100/1400.
 */
void gfx_set_far(float far);
float gfx_far(void);

#endif
