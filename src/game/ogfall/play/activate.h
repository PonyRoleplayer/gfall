#ifndef OGFALL_ACTIVATE_H
#define OGFALL_ACTIVATE_H

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * click_activate for a door, a lever, and a click that hits nothing.
 * In cursor mode the pick is six classic pixels down and right of the
 * pointer (the hotspot). The decomp writes that Y as a cast of an
 * address; this is the pixel. View mode's centre key, and a centre
 * activate, use the view centre.
 *
 * The decomp tests the right button as held, every frame, which would
 * start a door and then start it back the frame it finished. Slice 1
 * fires on the press when unarmed, or a stationary release with a melee
 * weapon readied, so a swing can finish. There is no spell missile,
 * so that fall-through is skipped.
 *
 * Info mode says the usable's label (interact_say) when it has one.
 * Steal picks a locked dungeon door; Grab reports its lock difficulty.
 * Unlocked doors and other Grab-mode movers use the engine's mover.
 */

/* Usable.what in this range is a treasure container; tag is its stable key.
 * The adapter supplies the original loot-table index in the low offset. */
#define OG_USE_TREASURE 0x4f4700

/* Classic pixel of the pick. at_centre ignores x and y. */
void og_activate_pixel(int cursor_mode, int cursor_active, int at_centre,
                       int classic_x, int classic_y, int *px, int *py);
/* Same projection for pointed use and spell aiming. */
void og_activate_aim(const Camera *camera, int cursor_mode, int cursor_active,
                     int at_centre, int x, int y, Camera *aim);

/*
 * Cast and, in grab mode, request the use. Returns 1 when a usable was
 * hit. The frame after this, gfx_frame steps the mover.
 */
/* A usable of this kind (a shop's shelf) is the game's to open as a shop: what the seam knows is what, the call gets its index in the scene and its tag. */
void og_activate_set_shelf(int what, int (*use)(int index, int tag));
int og_activate(const Scene *scene, const Camera *cam, int mode,
                int cursor_mode, int cursor_active, int at_centre,
                int classic_x, int classic_y);

#endif
