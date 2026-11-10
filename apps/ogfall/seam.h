#ifndef OGFALL_SEAM_H
#define OGFALL_SEAM_H

#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/scene/automap.h"

/*
 * The only file that includes Daggerfall. It loads Privateer's Hold,
 * the menu and HUD pictures, the class rows and the click sound.
 * ogfall's own sources never include src/game/dagger.
 */

int seam_boot(void);
void seam_title(void);
/* Marker 8 from world_start, then the nearest marker 6 when one exists. */
int seam_open_hold(float *x, float *y, float *z);
int seam_open_place(int restoring, int changing, float *x, float *y, float *z);
void seam_land(Camera *camera, int restoring, int changing);
void seam_world_step(Camera *camera, float seconds);
int seam_world_needs_reload(void);
void seam_world_reload(Camera *camera);
int seam_next_world(void);
const AutomapView *seam_automap_view(void);
void seam_close_hold(void);
int seam_capture_world(void);
void seam_creatures(const Camera *cam);
Level *seam_level(void);
int seam_creature_archive(int index);
/* Palette index of TAMRIEL2.IMG at a classic pixel. 0 off the map. */
int seam_map_pixel(int x, int y);
const char *seam_error(void);
/* Compose the chargen picture for this frame into OG_PIC_SCREEN. */
void seam_present(void);
/* Last composed 320 x 200 surface and its selected palette, for host QA. */
const df_u8 *seam_screen(const df_u8 **palette);
const df_u8 *seam_weapon_screen(const df_u8 **palette);

#endif
