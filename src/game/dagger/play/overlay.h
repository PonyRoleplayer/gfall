#ifndef DF_OVERLAY_H
#define DF_OVERLAY_H

/*
 * What the play screen draws over the world: classic's compass in the
 * bottom right (COMPASS.IMG's strip scrolled under COMPBOX.IMG, as
 * Daggerfall Unity's HUDCompass draws it), the three vitals in the
 * bottom left (MAIN03I0, MAIN04I0 and MAIN05I0.IMG: health, fatigue,
 * spell points, each filled from the bottom as DFU's HUDVitals does,
 * where the files put them), a crosshair in the middle
 * (DFU's, not classic's: classic had none), and the pause line. The
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
/* 1 shows the pause line. */
void overlay_set_paused(int on);
/* 1 (default) draws the crosshair. */
void overlay_set_crosshair(int on);
/* The overlay, from gfx_set_extra. */
void overlay_draw(void);

#endif
