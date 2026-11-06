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
 * card. A second call replaces what the first sent. A null level is no
 * place: a game's screen before any (its menu), with only the font, in
 * white ink, and the pictures' room on the card (docs/pictures.md).
 */
int gfx_prepare(const Level *level);
/*
 * The screen's mode, chosen before the card opens: w x h, one of
 * gfx_mode's (512 x 384 and 640 x 480; 800 x 600 on a board with 4 MB of
 * frame buffer; 320 x 240 and 400 x 300 only on a board that is neither
 * a Voodoo 1 nor a Voodoo 2, which have none below 512 x 384). Call it
 * before gfx_startup; it
 * takes effect there, so a change shows at the next start. A size not
 * in the list, or one the card refuses, opens 640 x 480 instead, which
 * gfx_mode_opened then reports. The view (df_view_w, df_view_h) is the
 * opened mode from gfx_startup on, 640 x 480 before it. The field of
 * view is kept: the focal length follows the width. 640 x 400 is also
 * taken though never listed: a probe at 70 Hz, its picture stretched up
 * and down (docs/glide.md, The screen's mode).
 */
void gfx_set_mode(int w, int h);
/*
 * The frame on the glass now, shrunk to w x h (no larger than the view)
 * into out, RGB 565, row 0 at the top: each pixel the mean of the box of
 * the view it covers (pic_box_565). For a save's snapshot: call it while
 * the play view is on the glass, before a menu covers it. It reads the
 * whole front buffer across the bus, the slow way on a Voodoo 1 (the log
 * says how long). 0 when the front buffer cannot be read.
 */
int gfx_thumbnail(int w, int h, df_u16 *out);
/* The modes to list, smallest first: final after gfx_startup, which finds the board (before it, 512 x 384 and 640 x 480). */
int gfx_mode_count(void);
int gfx_mode(int i, int *w, int *h);
/* The mode the card opened in. */
void gfx_mode_opened(int *w, int *h);
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
/*
 * Texture detail, the player's: walls (opaque pictures: models' faces,
 * the land) and sprites (cutouts: flats, creatures, people, flora), each
 * GFX_DETAIL_LOW, _MEDIUM or _HIGH (0..2, clamped; High at the start).
 * High is a picture's own size, Medium half its side, Low a quarter;
 * the smaller levels stay for distance. The sky, the font and the game's
 * screen pictures (pic.h) are never touched. A change shows on the next
 * gfx_frame: a place loaded whole is sent again, a stream's pictures are
 * made again in place (texcache_redetail); with no place it waits for
 * the next. See docs/memory.md, Detail.
 */
#define GFX_DETAIL_LOW 0
#define GFX_DETAIL_MEDIUM 1
#define GFX_DETAIL_HIGH 2
void gfx_set_tex_detail(int level);
int gfx_tex_detail(void);
void gfx_set_sprite_detail(int level);
int gfx_sprite_detail(void);
/* How far lights reach, 1..8 times their radius; cuts the decals again. */
void gfx_set_light_range(int scale);
/* 1 (default): a light does not reach through a model in its way; cuts the decals again. */
void gfx_set_light_shadow(int on);

/*
 * The frames a second in the bottom right corner, averaged over half a
 * second of swaps, drawn with the HUD off and over a game's menu too, in
 * the installed text (the engine's face before a game has one). Default
 * off; the console's `fps`. docs/glide.md, The frame's pace.
 */
void gfx_set_fps_counter(int on);
int gfx_fps_counter(void);
/*
 * At most this many frames a second, the swap waiting asleep for its
 * time; 0 (the default) is off, anything else is held to 20..240 (20:
 * the sound is refilled from the frame). With vsync on, a limit at or
 * above the refresh waits for nothing. The console's `fps_limit`.
 */
void gfx_set_fps_limit(int fps);
int gfx_fps_limit(void);

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
/*
 * Clear, draw what the scene says is visible, overlay, swap. A null
 * level is a screen frame: clear to the background, gfx_set_under then
 * gfx_set_extra (no HUD, no world, no clock), the console over it, swap.
 */
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
/*
 * Milliseconds to give a stream this frame (stream_update's budget): what
 * the last frame left before its retrace, 1 to 8 ms; fallback_ms with vsync
 * off or before the counter is calibrated. See prof_spare_ms, docs/profile.md.
 */
float gfx_stream_budget(float fallback_ms);

/*
 * The indoor automap (docs/automap.md): level's seen geometry in view's
 * rectangle, over a ground of colour back (0xRRGGBB) that also clears the
 * depth there, so the map never tests against a world drawn behind it.
 * Call it from the gfx_set_under hook; flat colour, no fog, no texture.
 * Best in a screen frame (gfx_frame(cam, 0)): the stilled world is not
 * drawn under the map at all.
 */
struct AutomapView;
void gfx_automap(const Level *level, const struct AutomapView *view, df_u32 back);
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
