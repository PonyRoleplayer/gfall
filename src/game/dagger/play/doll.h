#ifndef DF_DOLL_H
#define DF_DOLL_H

#include "engine/assets/texture.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * The paper doll: a character drawn in memory as classic draws them, in
 * classic's own pixels. Read docs/game.md (Loot and the inventory).
 *
 * From the bottom up, as Daggerfall Unity's PaperDollRenderer has them:
 * the race's background cropped to the doll, the body, the same body's
 * underclothes over it, the head, then what is worn in its draw order.
 * Every picture says where it sits on classic's screen, the character
 * sheet's doll starting at 200, 8: that corner is taken off to place it
 * here.
 *
 * A worn picture's palette index 255 is classic's mask, not a colour: it
 * cuts away what is under it, so a helm takes off the head's horns and
 * the background shows there (DOLL_MASK).
 *
 * It is composed once, when what is worn changes, never per frame, and
 * only palette indices are written: the screen it lands on goes on the
 * card as one picture. owner keeps which EQUIP_* place drew each pixel, so
 * a click on the doll finds exactly what was clicked.
 *
 * Plain C, reading pictures but drawing nothing: hostcheck runs it.
 */

#define DOLL_W 110
#define DOLL_H 184
/* An owner pixel over no worn thing. */
#define DOLL_NO_OWNER 0xff
/* The palette index in a worn thing's picture that shows the background through it. */
#define DOLL_MASK 255

/*
 * The doll of c wearing what pack says, into pixels and owner, each
 * DOLL_W x DOLL_H bytes. Pictures are read in the current heap. 0 when
 * the race or its body cannot be read.
 */
int doll_compose(df_u8 *pixels, df_u8 *owner, const Character *c, const Inventory *pack);
/*
 * A TEXTURE.### record, noting the file if this read brings it into the
 * cache. doll_forget_files lets go of those: the caller does that before
 * it deletes the heap they were read into.
 */
Image *doll_read_picture(int archive, int record);
void doll_forget_files(void);

#endif
