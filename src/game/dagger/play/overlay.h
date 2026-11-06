#ifndef DF_OVERLAY_H
#define DF_OVERLAY_H

/*
 * What the play screen draws over the world: classic's compass in the
 * bottom right (COMPASS.IMG's strip scrolled under COMPBOX.IMG, as
 * Daggerfall Unity's HUDCompass draws it), the three vitals in the
 * bottom left (MAIN03I0, MAIN04I0 and MAIN05I0.IMG: health, fatigue,
 * spell points, each filled from the bottom as DFU's HUDVitals does,
 * where the files put them), a crosshair in the middle
 * (DFU's, not classic's: classic had none), drawn as a small picture of our own,
 * not a glyph of the font. The
 * world's message line is the engine's (interact_say). Read docs/game.md
 * (Controls and the HUD).
 *
 * Classic's screen is 320 x 200; the card's 640 x 480, so its pieces
 * are drawn twice as wide and 2.4 times as tall, where classic put them.
 */

/* The compass's pictures, before the first place (pic_set_room set). 0 when they do not read: no compass. */
int overlay_load(void);
void overlay_free(void);
/* Which way the player faces, for the compass: radians, 0 north, growing east. */
void overlay_set_yaw(float yaw);
/* The vitals the bars show: each now and most. */
void overlay_set_vitals(int health, int health_max, int fatigue, int fatigue_max, int magicka, int magicka_max);
/* 1 (default) draws the crosshair. */
void overlay_set_crosshair(int on);
/*
 * 1 while a use is stealing (the bar's Steal mode, or crouched): the crosshair becomes an amber ring and
 * "Stealth" stands under it, even with the crosshair hidden for the classic pointer. 0 the ordinary cross.
 */
void overlay_set_stealth(int on);
/* The overlay, from gfx_set_extra. */
void overlay_draw(void);
/*
 * Classic's bottom panel (MAIN00I0.IMG, 320 x 46, at the screen's foot; DFU's
 * HUDLarge): the character's face in it, the bars and the compass over it.
 * On by default; the console's `panel` flips it. Full screen
 * (`viewopt_full`) hides the stone, the face and the icon buttons.
 * The vitals and the bar's compass stay where the bar puts them, and
 * the crosshair stays. hudbar.h owns hit regions
 * and mode names, game.c routes the commands. This module only draws.
 */
void overlay_set_panel(int on);
int overlay_panel(void);
/* The face it shows, a race's and sex's CIF and which: read when the game starts. */
void overlay_set_face(const char *cif, int which);
/* The mode's row in MAIN01I0.IMG, from hudbar.h. A change needs no upload. */
void overlay_set_mode(int mode);

#endif
