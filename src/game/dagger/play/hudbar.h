#ifndef DF_HUDBAR_H
#define DF_HUDBAR_H

#include "game/dagger/play/canvas.h"

/*
 * Classic's bottom bar: its layout and interaction modes, apart from the
 * card-facing overlay.c. Read docs/game.md (Classic HUD bar) before editing.
 * Rectangles follow Daggerfall Unity's HUDLarge (MIT) over MAIN00I0.IMG.
 * They are absolute coordinates on classic's 320 x 200 screen, top down;
 * screen_from_pointer is the only conversion at the game's input boundary.
 * This plain-C part is host-tested: a HUD click returns an intention, never
 * opens a screen, polls input, pauses the world or changes the engine.
 */
#define HUDBAR_TOP 154
#define HUDBAR_HEIGHT 46

enum {
    HUDBAR_NONE,
    HUDBAR_CHARACTER,
    HUDBAR_OPTIONS,
    HUDBAR_SPELLBOOK,
    HUDBAR_MODE,
    HUDBAR_INVENTORY,
    HUDBAR_SHEATH,
    HUDBAR_MAGIC_ITEM,
    HUDBAR_TRANSPORT,
    HUDBAR_MAP,
    HUDBAR_REST,
    HUDBAR_STATUS
};

/* Grab is zero so a new game starts in the existing general-use behavior.
 * The order cycles Grab -> Info -> Steal -> Talk -> Grab, as HUDLarge does. */
enum {
    HUDBAR_GRAB,
    HUDBAR_INFO,
    HUDBAR_STEAL,
    HUDBAR_TALK,
    HUDBAR_MODES
};

/* Button rectangle, or 0 for no such action. The caller borrows static data. */
const Rect *hudbar_rect(int action);
/* Hit the actionable buttons only; gaps and pixels outside the bar return NONE. */
int hudbar_hit(int x, int y);
/* 1 over the entire bar, including gaps: these clicks must never attack. */
int hudbar_contains(int x, int y);
const char *hudbar_mode_name(int mode);
/* Which 23-pixel row of MAIN01I0.IMG holds this mode's icon. */
int hudbar_mode_row(int mode);
/* Heading -> CMPA00I0.BSS frame, 0 north, 8 east. No divide or trig a frame. */
int hudbar_heading(float yaw);

#endif
