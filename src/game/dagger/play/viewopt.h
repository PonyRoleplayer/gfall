#ifndef DF_VIEWOPT_H
#define DF_VIEWOPT_H

#include "engine/assets/recfile.h"

/*
 * Stored view preferences: full screen, head bobbing, horizontal FOV.
 * The settings shell currently exposes only FOV; older full/bob values
 * remain effective and survive writes.
 * Read docs/game.md (Menus) before editing.
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
 * Stored as one OPTS row in DATA\KEYS.DAT beside the bindings (keys.c
 * writes and reads it with them). No row means both off and FOV 75. The appended fov field uses the
 * same default for older OPTS rows; valid stored values are 45..120.
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
 * The eye's offset at this walk phase (radians), in world units.
 * right is along the body's right, up is world up, and pitch and yaw
 * are radians added to the view (HeadBobber's nod, its degrees turned).
 * Walking 0, or the setting off, writes zeros: standing still does not
 * bob. running selects the run amounts; otherwise the walk's.
 */
void viewopt_eye(float phase, int walking, int running, float *right, float *up, float *pitch, float *yaw);
/* The OPTS row in a record file, over whatever viewopt_defaults set. */
void viewopt_read(const df_u8 *bytes, df_u32 n);
/* One OPTS row into a file already begun. 1 when it wrote. */
int viewopt_write(RecPut put, void *ctx);

#endif
