#ifndef DF_DUNMAP_H
#define DF_DUNMAP_H

#include "engine/scene/automap.h"

/*
 * Classic's automap indoors (AMAP00I0.IMG), a dungeon's or a building's: the
 * engine draws what the player has seen (engine/scene/automap.h, docs/automap.md);
 * this is the window round it. Read docs/game.md (The wider world, Automaps).
 *
 * The chrome is composed in memory with a clear hole where the map goes, and
 * sent as a cutout: the engine draws the map under it (gfx_automap from the
 * frame's under hook), the chrome's picture over it, and the player's arrow is
 * drawn into the picture over the hole. So the map costs no download of its own,
 * and the picture changes only when the view does.
 *
 * Views: from above, the way the player faces up, or from behind and above them
 * (the grid button). The arrows move the view, the turn buttons turn it, the
 * stairs move the slice of height a storey, the wheel zooms, the compass brings
 * it back to the player.
 *
 * Plain C, drawing nothing and reaching no card: hostcheck drives it by clicks.
 */

enum {
    DUNMAP_STAY,
    DUNMAP_LEAVE
};

/* Opened round the player: their feet and facing in the place's frame. 0 when the picture does not read. */
int dunmap_open(const float *feet, float yaw);
void dunmap_close(void);
void dunmap_compose(void);
const unsigned char *dunmap_pixels(void);
int dunmap_changed(void);
int dunmap_click(int x, int y);
void dunmap_wheel(int notches);
int dunmap_key(int vk);
/* The view the engine draws now, in its screen pixels; and the ground's colour (0xRRGGBB). */
const AutomapView *dunmap_view(void);
unsigned long dunmap_ground(void);
/* What a test reads: the slice's storeys from the player's, and the view's mode. */
int dunmap_storey(void);

#endif
