#ifndef DF_CONTROL_H
#define DF_CONTROL_H

#include "engine/platform/input.h"
#include "engine/gfx/camera.h"
#include "engine/assets/recfile.h"

/*
 * Daggerfall's classic cursor controller, apart from Win32 and drawing.
 * Read docs/game.md (Classic dungeon controls) before editing. The original
 * manual's Cursor Based Interface supplies the behavior: held left arrow
 * movement, X to use, right gestures, and Alt look apart from the body.
 * The screen regions and turn rates here are explicit local tuning, not
 * numbers recovered from FALL.EXE. All work is bounded and allocation-free;
 * trigonometry is needed only for the view offset and an actual use ray.
 */
#define CONTROL_LOOK_SENS 0.003f
#define CONTROL_TURN_RATE (DF_TAU * 0.25f)
#define CONTROL_PITCH_RATE (DF_TAU * 0.20f)
#define CONTROL_CURSOR_SIDE 16
#define CONTROL_CURSOR_W 64
#define CONTROL_CURSOR_H 48
enum {
    CONTROL_TARGET, CONTROL_N, CONTROL_NE, CONTROL_E, CONTROL_SE,
    CONTROL_S, CONTROL_SW, CONTROL_W, CONTROL_NW
};

typedef struct ControlState {
    /* A left drag belongs to where it began: arrow, target, or HUD. Moving
     * a held HUD click into the world must neither walk nor activate it. */
    int left_route;
    int left_gate;
    int attack_gate;
    /* Alt look offsets the view from the body's yaw/pitch, so forward
     * movement keeps its heading while the player looks to one side. */
    float view_yaw;
    float view_pitch;
} ControlState;

typedef struct ControlFrame {
    float forward, right;
    /* Radians THIS frame, not a per-frame rate; keyboard/cursor rates use dt. */
    float yaw, pitch;
    int center;
    int use;
    int cursor;
    int attack_pressed, attack_held, attack_released;
    int gesture_dx, gesture_dy;
} ControlFrame;

/* Default classic, or the saved CTLS row in KEYS.DAT. Kept separately from
 * transient pointer capture so menus/Tab do not silently change preference. */
int control_classic(void);
void control_set_classic(int on);
void control_read_settings(const df_u8 *bytes, df_u32 size);
int control_put_settings(RecPut put, void *ctx);
/* New character/load: offsets centered, held buttons quarantined. */
void control_reset(ControlState *s);
/* Screen/console/handoff: preserve the view, consume old held buttons. */
void control_block(ControlState *s);
/* x,y and view_height are classic top-down pixels. A full HUD leaves 154
 * rows; panel off leaves 200. Passive cursor motion never moves the body. */
int control_region(int x, int y, int view_height);
void control_step(ControlState *s, const Input *in, int classic, int pointer,
                  int x, int y, int view_height, int sheathed, float dt, ControlFrame *out);
/* Apply the Alt offset to a body camera; does not move its eye or the body. */
void control_view(const ControlState *s, Camera *cam);
/* The same eye aimed through a logical 640x480 screen pixel. Only on use:
 * invert the renderer's projection, then reuse existing wall-tested pickers. */
void control_point(const Camera *view, int px, int py, Camera *aim);
/* One startup-built cutout sheet for eight movement arrows and the target X.
 * These are small procedural indicators, not recovered original cursor art. */
void control_cursor_sheet(df_u8 *pixels);

#endif
