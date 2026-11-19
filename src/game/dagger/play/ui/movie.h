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

/*
 * A prince's scene (FALL.EXE's flc_play_with_text): his *.FLC (formats/flc.h) loops over the whole screen with a page of text in four lines under it, in the palette's yellow with a brown
 * shadow; a click turns the page, and on the last page leaves, or, when the scene asks, waits for Y or N. It is what the summoning's "you have summoned him once" (TEXT.RSC 482) and a prince's
 * quest offer (QRC messages 1000, 1002 and 1001) are said in. It needs the movies on and the file; it returns 0 when it cannot be shown, and the caller says its words in a box instead.
 * The scene is one at a time, and it may be started from its own answer (the offer's 1000 and then its 1002). Read docs/video.md.
 */
#define MOVIE_ANSWER_LEFT 0     /* a click on the last page of a scene that asks nothing */
#define MOVIE_ANSWER_YES 1
#define MOVIE_ANSWER_NO 2
typedef void (*MovieAnswer)(int answer);
/* words: the lines of the text, '\n' between them (leading blanks of a line are dropped). done may be 0. */
int movie_scene(const char *flc, const char *words, int ask, MovieAnswer done);
int movie_scene_active(void);
/* One frame of it: real seconds since the last call, the key typed this frame (0 none) and whether a new click came. */
void movie_scene_frame(float dt, int key, int click);
/* The 320 x 200 indices the scene shows now and its palette (768 bytes), for tests; 0 when no scene is up. */
const unsigned char *movie_scene_pixels(const unsigned char **palette);

#endif
