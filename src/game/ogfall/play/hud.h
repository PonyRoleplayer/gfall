#ifndef OGFALL_HUD_H
#define OGFALL_HUD_H

/*
 * hud_buttons from FALL.EXE at 0x185C58: eleven rectangles, hit with
 * a strict greater-than and less-than. A press remembers the index.
 * A release fires only on that same index, then sound 203.
 *
 * interaction_mode is (mode + step) & 3. Left button steps +1, right
 * steps -1. Portrait overlay keeps hud_portrait_overlay_index's order;
 * slice 1 can only show the low-health frame.
 */

#define OG_HUD_BUTTONS 11
#define OG_HUD_NONE 255
#define OG_BAR_Y 154

enum {
    OG_HUD_SHEET,
    OG_HUD_SPELL,
    OG_HUD_MODE,
    OG_HUD_PACK,
    OG_HUD_WEAPON,
    OG_HUD_MAGIC,
    OG_HUD_TRAVEL,
    OG_HUD_MAP,
    OG_HUD_REST,
    OG_HUD_OPTIONS,
    OG_HUD_STATUS
};

enum {
    OG_MODE_GRAB,
    OG_MODE_INFO,
    OG_MODE_STEAL,
    OG_MODE_TALK
};

typedef struct OgHudRect {
    int x0, y0, x1, y1;
} OgHudRect;

const OgHudRect *og_hud_rect(int index);

/* 1 when the point is strictly inside the button. The edge misses. */
int og_hud_hit(int x, int y);

void og_hud_reset(void);
int og_hud_mode(void);
void og_hud_set_mode(int mode);
int og_hud_weapon_ready(void);
/* Flips the ready flag. 1 when it is now ready. */
int og_hud_toggle_ready(void);
int og_hud_pressed(void);

/*
 * buttons is the mouse buttons down now, prev the previous frame,
 * view_mode 1 is mouse look, cursor_active is view mode's cursor.
 * Returns 1 when the click is consumed as a button. *sound is 203
 * when one fired, 0 otherwise. *which is the button, or -1.
 * A mode click writes *step +1 or -1. A weapon click toggles the flag.
 */
int og_hud_buttons(int x, int y, int buttons, int prev, int view_mode, int cursor_active,
                   int *sound, int *which, int *step);

/*
 * 1 when the mouse must not also steer: the pointer is on the bar,
 * the bar is up, and no steer key is held. A steer key still wins
 * (hud_update returns 1 only when steer_key_region is -1).
 */
int og_hud_blocks_mouse(int y, int bar_up, int steer_key);

/* (mode + step) & 3, then the status sentence. */
int og_hud_cycle(int step, char *status, int cap);
const char *og_hud_mode_name(int mode);

/* First match: low health, under a spell, poisoned, diseased. -1 for none. */
int og_hud_overlay(int health, int max_health, int under_spell, int poisoned, int diseased);

/* (yaw & 0x7ff) >> 6. yaw is the 2048-step circle. */
int og_hud_compass_frame(int yaw);

/* Vital bar height, 0..32. which is 0 health, 1 fatigue, 2 magicka. */
int og_hud_bar(int which, int health, int max_health, int fatigue, int strength, int endurance,
               int magicka, int magicka_bonus, int max_magicka);

#endif
