#ifndef GFALL_MOVIE_H
#define GFALL_MOVIE_H

/*
 * The movie player (FALL.EXE's xn_vid_play): a game's ANIM####.VID played over the whole screen. The VID reader (formats/vid.c) decodes a frame at a time under the movie's own palette;
 * a frame goes to the card as one 320 x 200 P_8 picture (pic_update sends only the tiles that changed, a palette change only the palette), and the sound goes to a streamed voice of the
 * engine's mixer as it is read. The frame clock is the wall clock, held to the sound's when there is some (the card's clock is the movie's). One movie at a time; Escape, Space, Enter
 * or a click skip it. A movie that is missing (no file on the disk and no disc) is not played and the caller goes on: nothing waits for it. Read docs/video.md.
 */

/* The movies the game plays (ANIM####.VID; FALL.EXE's intro.c, damage.c, rest.c, qcom.c): the publisher's logo, the opening's three, the funeral, the dreams (curse.h), a quest's op 50. */
#define MOVIE_LOGO 1
#define MOVIE_OPENING 0
#define MOVIE_OPENING_2 11
#define MOVIE_OPENING_3 "DAG2.VID"
#define MOVIE_DEATH 12

/* Movies on or off (on by default); off drops what is queued and refuses new ones. */
void movie_set_enabled(int on);
int movie_enabled(void);

/* The movie's number: ANIM%04d.VID. Starts it; 0 when it cannot be (no file, no picture slot). The frame calls movie_frame while movie_active. */
int movie_play(int number);
/* The same by file name (DAG2.VID). */
int movie_play_file(const char *name);
int movie_active(void);
/* Real seconds since the last call, and whether the player asked to skip: decodes the frames that are due. 1 while the movie goes on, 0 once it ended (closed). */
int movie_frame(float dt, int skip);
/* The frame, over the screen in the classic 320 x 200 frame. */
void movie_draw(void);
/* Ends the movie that plays (the next queued one follows). */
void movie_stop(void);
/* What is playing, for tests: the movie's frames shown so far. */
int movie_frames_shown(void);

#endif
