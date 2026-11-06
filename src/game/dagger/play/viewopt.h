#ifndef DF_VIEWOPT_H
#define DF_VIEWOPT_H

/*
 * Stored options: full screen, head bobbing, crouch toggle, horizontal FOV, SFX/music volume.
 * Graphics exposes FOV, Gameplay exposes bobbing, Full HUD and Toggle Crouch, and Audio
 * exposes volumes. Full HUD On means full-screen off: the bottom bar shows.
 *  * Read docs/game.md (Menus) before editing.
 *
 * Full screen hides the bottom stone bar (the face frame and the icon
 * buttons). The vitals and the compass stay. It does not change the
 * view's size. Head bobbing moves
 * the camera off the body's eye while walking, by Daggerfall Unity's
 * HeadBobber PlotPath and PlotRotation. Both start off: classic's
 * interface starts visible (UESP, User Interface), and no classic source
 * found here says the bob box starts checked. DFU's own defaults.ini
 * sets HeadBobbing true; that is DFU's choice, not used here.
 *
 * Kept in gfall.cfg (config.c, keys.c): this module holds the values the game reads
 * while it runs and starts from the defaults (bar shown, no bob, Toggle Crouch off,
 * 640 x 480, High). The screen's mode and the two texture details are the player's
 * choice kept here for the file and for the next start.
 *
 * Plain C. FOV is read from/applied to camera.c's existing projection API.
 */

/* HeadBobber's timer starts here: sin is 1, the crest of a step. */
#define VIEWOPT_CREST 1.57079632679f
/* PlayerFootsteps.WalkStepInterval (2.5, matched to classic) / 2. */
#define VIEWOPT_RATE 1.25f

void viewopt_defaults(void);
/* 1 when the bottom bar is hidden. */
int viewopt_full(void);
void viewopt_set_full(int on);
/* 1 when a walk may move the eye. */
int viewopt_bob(void);
void viewopt_set_bob(int on);
/*
 * The texture details the player chose, 0 Low, 1 Medium, 2 High (the engine's GFX_DETAIL_*; High
 * the default): walls and the like, and sprites. Kept here with the other options and given to the
 * engine by the menu on Apply and by keys.c at the start; this module stays plain C.
 */
int viewopt_tex_detail(void);
void viewopt_set_tex_detail(int level);
int viewopt_sprite_detail(void);
void viewopt_set_sprite_detail(int level);
/*
 * The screen's mode the player chose, w x h (the engine's gfx_mode list; 640 x 480 the default and for older
 * files). It takes effect at the next start: apps/gfall reads it from gfall.cfg (keys_peek_mode) before the card opens.
 */
void viewopt_mode(int *w, int *h);
void viewopt_set_mode(int w, int h);
/* 1 when the crouch key flips between crouched and standing; 0 when crouched only while it is held. */
int viewopt_crouch_toggle(void);
void viewopt_set_crouch_toggle(int on);
/*
 * The frame rate counter and limiter (Gameplay tab): 1 when the counter shows; the limit in frames a second, 0 for none
 * (as fast as the card goes), else VIEWOPT_FPS_LEAST to VIEWOPT_FPS_MOST. The engine does the drawing and the pacing
 * (gfx_set_fps_counter, gfx_set_fps_limit); these are the player's choices, kept in gfall.cfg and given to it by keys.c.
 */
#define VIEWOPT_FPS_LEAST 20
#define VIEWOPT_FPS_MOST 240
int viewopt_show_fps(void);
void viewopt_set_show_fps(int on);
int viewopt_fps_limit(void);
/* A limit under VIEWOPT_FPS_LEAST and over 0 is VIEWOPT_FPS_LEAST (audio is fed from the frame); over the most is the most; 0 or less none. */
void viewopt_set_fps_limit(int fps);
/*
 * The eye's offset at this walk phase (radians), in world units.
 * right is along the body's right, up is world up, and pitch and yaw
 * are radians added to the view (HeadBobber's nod, its degrees turned).
 * Walking 0, or the setting off, writes zeros: standing still does not
 * bob. running selects the run amounts; otherwise the walk's.
 */
void viewopt_eye(float phase, int walking, int running, float *right, float *up, float *pitch, float *yaw);

#endif
