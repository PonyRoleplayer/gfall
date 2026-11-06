#ifndef DF_CASTFX_H
#define DF_CASTFX_H

/*
 * The casting hand: the element's own animation drawn over the view while a spell is cast
 * (FIRE00C6.CIF, FRST00C6.CIF, POIS00C6.CIF, SHOK00C6.CIF, MJIC00C6.CIF), the frames of
 * classic's weapon CIF layout but RLE compressed (cif.c reads only the weapons'), each at
 * its own place on classic's 320 x 200. Read docs/game.md (Magic) before editing.
 *
 * Each file holds six frames, the hand coming up and opening (the frames are 49 x 48 at the
 * bottom, then up to 132 x 200). It runs once, CASTFX_SECONDS from first frame to last (12 frames
 * a second; DFU's own pace is not known), and the spell goes at its fourth frame, the
 * half (castfx_release answers 1 once). One frame is on the card at a time,
 * sent when it shows as a weapon's swing is (docs/pictures.md). The file is read when the cast begins and
 * let go when it ends, so no picture or heap is kept between casts.
 *
 * Plain C: hostcheck steps it.
 */

#define CASTFX_SECONDS 0.5f
/* The release comes at this share of the frames the file holds. */
#define CASTFX_RELEASE_SHARE 0.5f

/* The cast begun in element's hand (ELEMENT_*): 0 when its file will not read (the spell then goes at once). */
int castfx_begin(int element);
/* The hand on by dt seconds. */
void castfx_step(float dt);
/* 1 once, when the release frame is reached: the spell goes now. */
int castfx_release(void);
/* 1 from begin until the last frame is done. */
int castfx_active(void);
/* The picture to draw and its place on classic's screen; 0 when nothing shows. */
int castfx_shown(int *pic, int *left, int *top, int *w, int *h);
/* Put away at once (a place left, a menu opened). */
void castfx_free(void);

/* For the tests: how many frames the file holds, the frame showing, and one frame's size and place. */
int castfx_frames(void);
int castfx_frame_index(void);

#endif
