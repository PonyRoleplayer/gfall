#include "game/dagger/play/ui/movie.h"
#include "game/dagger/play/ui/screen.h"
#include "game/dagger/formats/vid.h"
#include "game/dagger/formats/flc.h"
#include "game/dagger/formats/smallfont.h"
#include "engine/ui/pic.h"
#include "engine/sound/sound.h"
#include "engine/sound/audio.h"
#include "engine/sound/music.h"
#include "engine/core/mem.h"

/* The classic screen a movie is drawn in, and how far the wall clock may run from the sound's before it is set to it (ticks of 60 Hz: 100 ms). */
#define MOVIE_SCREEN_W 320
#define MOVIE_SCREEN_H 200
#define MOVIE_DRIFT_TICKS 6
/* The most frames shown in one call (a slow frame catches up, the picture is drawn once). */
#define MOVIE_CATCH_UP 8
/*
 * The sound comes with the frames, so a movie that decodes a frame only when it is due has no sound ahead of it: any hitch (a window of the file read from the disc, a slow frame) runs the stream dry and it
 * crackles. The reader therefore runs ahead of the clock: frames are decoded into a ring of MOVIE_RING frames while the stream holds less than a MOVIE_LEAD_DIV-th of a second of sound, and shown when
 * the clock reaches them. The clock is the wall's, set forward to the sound's when that is more than MOVIE_DRIFT_TICKS ahead; it is never set back to the sound (its movies' sound is about 8 % shorter
 * than their frames, and a clock held to it ran the picture down to a crawl, or held it still when the sound ran out: nothing decoded, so nothing pushed). The movie ends when its last frame was shown and its sound has drained.
 */
#define MOVIE_RING 4
#define MOVIE_LEAD_DIV 4
#define MOVIE_FRAME_BYTES (MOVIE_SCREEN_W * MOVIE_SCREEN_H)
#define MOVIE_PALETTE_BYTES 768

static struct {
    int active, pic, stream, frames;
    Vid vid;
    double clock, due;          /* ticks since the start, and when the next frame is due */
    int have_frame;             /* unused: kept so a frame of the ring is the one thing that waits */
    df_u32 pushed;              /* audio samples the stream took */
    int head, count, decoded;   /* the ring: the next frame to show, how many wait, and the reader at the end */
    int rate;
    int w, mute;
} g;
/* The ring of frames decoded ahead: pixels, ticks and a palette change each carries. Kept for the process (one movie at a time). */
static df_u8 g_ring[MOVIE_RING][MOVIE_FRAME_BYTES];
static df_u8 g_ring_palette[MOVIE_RING][MOVIE_PALETTE_BYTES];
static int g_ring_ticks[MOVIE_RING];
static int g_ring_changed[MOVIE_RING];
/* The song that played before the first movie of a run, which comes back after the last. */
static int g_music_was, g_music_held;

/* The audio chunks as the reader finds them: the stream opens at the first, at the movie's rate. */
static void take_audio(void *ctx, const df_u8 *data, int bytes, int rate)
{
    (void)ctx;
    if (!g.stream && !g.mute && rate > 0) { g.stream = sound_stream_open(rate); g.rate = rate; if (!g.stream) { g.mute = 1; } }
    if (g.stream > 0) { int took = sound_stream_push(g.stream, data, bytes); if (took > 0) { g.pushed += (df_u32)took; } }
}

/* Movies waiting their turn (the opening is three, a quest's one after another): their file names. A missing one is skipped. */
#define MOVIE_QUEUE 6
#define MOVIE_NAME 16
/* The most a four-digit movie number holds. */
#define MOVIE_NUMBER_MOST 9999
static char g_queue[MOVIE_QUEUE][MOVIE_NAME];
static int g_queued;

/* Movies on (the original always plays them); a host test or a player who skips them turns it off, and a queued one is then dropped. */
/* The host's tests drive the title and the world by frames: movies are off there until a test turns them on. */
#ifdef DF_HOST
static int g_enabled = 0;
#else
static int g_enabled = 1;
#endif
static void scene_end(int answer);
void movie_set_enabled(int on) { g_enabled = on; if (!on) { g_queued = 0; movie_stop(); scene_end(-1); } }
int movie_enabled(void) { return g_enabled; }

static int scene_up(void);
static int scene_pic(void);
int movie_active(void) { return g.active || g_queued > 0 || scene_up(); }
int movie_frames_shown(void) { return g.frames; }

/* Ends the movie that plays; the queue goes on (a skip) unless movie_stop_all. */
void movie_stop(void)
{
    if (!g.active) { return; }
    if (g.stream > 0) { sound_stream_close(g.stream); }
    if (g.pic) { pic_free(g.pic); }
    vid_close(&g.vid);
    if (g_music_held && g_queued == 0) { music_set_on(g_music_was); g_music_held = 0; }
    g.active = 0; g.pic = 0; g.stream = 0;
}

static int start_file(const char *name)
{
    df_u8 *grey;

    mem_zero(&g, sizeof(g));
    if (!vid_open(&g.vid, name)) { return 0; }
    /* The song is put aside for the movies (music_set_on keeps it), and the effects stopped, as the original's player owns the card. */
    if (!g_music_held) { g_music_was = music_on(); music_set_on(0); sound_stop_all(); g_music_held = 1; }
    /* The first frame decoded at once, so the picture exists with its size and palette (the movie's own). */
    if (!vid_next(&g.vid)) { vid_close(&g.vid); return 0; }
    g.w = g.vid.width;
    grey = g.vid.frame;
    g.pic = pic_load(grey, g.vid.width, g.vid.height, g.vid.palette, 0);
    if (!g.pic) { vid_close(&g.vid); return 0; }
    vid_set_audio(&g.vid, take_audio, 0);
    g.active = 1; g.have_frame = 0; g.frames = 1;
    g.head = g.count = g.decoded = 0;
    g.due = g.vid.ticks;
    return 1;
}

/* Queued; it starts at the next movie_frame, once nothing else plays (the screen may not be up when a game starts). Always 1 unless the queue is full. */
int movie_play_file(const char *name)
{
    int i;

    if (!g_enabled || g_queued >= MOVIE_QUEUE) { return 0; }
    for (i = 0; i < MOVIE_NAME - 1 && name[i]; i++) { g_queue[g_queued][i] = name[i]; }
    g_queue[g_queued][i] = 0;
    g_queued++;
    return 1;
}

int movie_play(int number)
{
    char name[MOVIE_NAME];
    int i;

    /* ANIM####.VID: four decimal digits (there is no C runtime to format with). */
    if (number < 0 || number > MOVIE_NUMBER_MOST) { return 0; }
    name[0] = 'A'; name[1] = 'N'; name[2] = 'I'; name[3] = 'M';
    for (i = 0; i < 4; i++) { name[7 - i] = (char)('0' + number % 10); number /= 10; }
    name[8] = '.'; name[9] = 'V'; name[10] = 'I'; name[11] = 'D'; name[12] = 0;
    return movie_play_file(name);
}

/* Decodes frames into the ring while there is room and the sound ahead is short (or, with no sound, while none waits). */
static void ring_fill(void)
{
    int slot, ahead;

    while (!g.decoded && g.count < MOVIE_RING) {
        if (g.stream > 0 && g.rate > 0) {
            ahead = sound_stream_queued(g.stream);
            if (ahead >= g.rate / MOVIE_LEAD_DIV) { break; }
        } else if (g.count >= 1) {
            break;
        }
        if (!vid_next(&g.vid)) { g.decoded = 1; break; }
        slot = (g.head + g.count) % MOVIE_RING;
        mem_copy(g_ring[slot], g.vid.frame, (df_u32)(g.vid.width * g.vid.height));
        g_ring_ticks[slot] = g.vid.ticks;
        g_ring_changed[slot] = g.vid.palette_changed;
        if (g.vid.palette_changed) { mem_copy(g_ring_palette[slot], g.vid.palette, MOVIE_PALETTE_BYTES); }
        g.count++;
    }
}

int movie_frame(float dt, int skip)
{
    int guard, queued;

    /* The next queued movie starts when none plays; one that cannot open is passed over. */
    while (!g.active && g_queued > 0) {
        int i;

        if (start_file(g_queue[0])) { skip = 0; }
        for (i = 1; i < g_queued; i++) { mem_copy(g_queue[i - 1], g_queue[i], MOVIE_NAME); }
        g_queued--;
    }
    if (!g.active) { return 0; }
    if (skip) { movie_stop(); return movie_active(); }
    g.clock += (double)dt * VID_TICK_HZ;
    ring_fill();
    /* The clock follows the sound only while there is sound to follow. */
    queued = g.stream > 0 && g.rate > 0 ? sound_stream_queued(g.stream) : 0;
    if (queued < 0) {
        /* The card stopped it (no card, sound off): the movie runs on its ticks, and later chunks are dropped. */
        sound_stream_close(g.stream); g.stream = 0; g.mute = 1; queued = 0;
    }
    if (queued > 0) {
        double played;

        /* What the speaker has played: the samples mixed, less what the card still holds (audio_lead_ms). Only forward: a movie's sound is a little shorter than its frames, so it falls behind the picture and is no clock to hold it to. */
        played = ((double)g.pushed - (double)queued) * VID_TICK_HZ / (double)g.rate - (double)audio_lead_ms() * VID_TICK_HZ / 1000.0;
        if (played - g.clock > MOVIE_DRIFT_TICKS) { g.clock = played; }
    }
    for (guard = 0; guard < MOVIE_CATCH_UP && g.count > 0 && g.clock >= g.due; guard++) {
        if (g_ring_changed[g.head]) { pic_set_palette(g.pic, g_ring_palette[g.head]); }
        pic_update(g.pic, g_ring[g.head]);
        g.frames++;
        g.due += g_ring_ticks[g.head];
        g.head = (g.head + 1) % MOVIE_RING; g.count--;
        ring_fill();
    }
    /* Far behind (a long stall): drop the lost time instead of racing through frames. */
    if (g.count > 0 && g.clock >= g.due) { g.due = g.clock + 1.0; }
    /* The end: every frame shown, and the sound played out (the last chunk is heard, not cut). */
    if (g.decoded && g.count == 0) {
        if (g.stream <= 0 || sound_stream_queued(g.stream) <= 0) { movie_stop(); return movie_active(); }
    }
    return 1;
}

void movie_draw(void)
{
    if (scene_up()) { blit(scene_pic(), 0, 0, MOVIE_SCREEN_W, MOVIE_SCREEN_H); return; }
    if (!g.active || !g.pic) { return; }
    blit(g.pic, (MOVIE_SCREEN_W - g.w) / 2, 0, g.w, MOVIE_SCREEN_H);
}

/* ---- The prince's scene ---- */

/* The text's place and colours (flc_draw_text_page): four lines from y 150, ten apart, centred on x 160, in the palette's ink and shadow. */
#define SCENE_LINES 4
#define SCENE_TOP 150
#define SCENE_LINE_H 10
#define SCENE_CENTRE 160
#define SCENE_INK 145
#define SCENE_SHADOW 156
/* The words a scene holds, its lines (a quest's message runs to a few pages), and the keys the scene answers to. */
#define SCENE_TEXT 1024
#define SCENE_ROWS 48
#define SCENE_KEY_ESCAPE 27
#define SCENE_KEY_ENTER 13
#define SCENE_KEY_SPACE ' '
#define SCENE_FONT "FONT0003.FNT"

static struct {
    int active, pic, ask, page, rows, at_end;
    Flc flc;
    SmallFont font;
    char text[SCENE_TEXT];
    char *row[SCENE_ROWS];
    MovieAnswer done;
    double wait;                /* seconds into the frame that shows */
    df_u8 shown[MOVIE_SCREEN_W * MOVIE_SCREEN_H];
} s;

static int scene_up(void) { return s.active; }
static int scene_pic(void) { return s.pic; }

/* The frame as it stands with the page's lines over it. */
static void scene_compose(void)
{
    int i, first;

    mem_copy(s.shown, s.flc.image, (df_u32)(MOVIE_SCREEN_W * MOVIE_SCREEN_H));
    first = s.page * SCENE_LINES;
    for (i = 0; i < SCENE_LINES && first + i < s.rows; i++) {
        const char *line = s.row[first + i];
        smallfont_draw(&s.font, s.shown, MOVIE_SCREEN_W, MOVIE_SCREEN_H, SCENE_CENTRE - smallfont_width(&s.font, line) / 2, SCENE_TOP + i * SCENE_LINE_H, SCENE_INK, SCENE_SHADOW, line);
    }
    if (s.pic) { if (s.flc.palette_changed) { pic_set_palette(s.pic, s.flc.palette); } pic_update(s.pic, s.shown); }
}

/* Ends it and tells who asked (answer -1: no one, the movies were turned off). The callback may start the next scene. */
static void scene_end(int answer)
{
    MovieAnswer done;

    if (!s.active) { return; }
    done = s.done;
    if (s.pic) { pic_free(s.pic); }
    flc_close(&s.flc);
    s.active = 0; s.pic = 0; s.done = 0;
    if (done && answer >= 0) { done(answer); }
}

int movie_scene_active(void) { return s.active; }

int movie_scene(const char *flc, const char *words, int ask, MovieAnswer done)
{
    int i, n;

    if (!g_enabled || s.active || !flc || !words) { return 0; }
    mem_zero(&s, sizeof(s));
    if (!flc_open(&s.flc, flc)) { return 0; }
    if (s.flc.width != MOVIE_SCREEN_W || s.flc.height != MOVIE_SCREEN_H || !flc_next(&s.flc)) { flc_close(&s.flc); return 0; }
    if (!smallfont_load(&s.font, SCENE_FONT)) { flc_close(&s.flc); return 0; }
    /* The lines: a break ends one, a line's leading blanks are dropped (the file's continuation lines begin with one), a blank line is nothing. */
    n = 0;
    for (i = 0; words[i] && n < SCENE_TEXT - 1; i++) { s.text[n++] = words[i]; }
    s.text[n] = 0;
    for (i = 0; i < n && s.rows < SCENE_ROWS; ) {
        while (s.text[i] == ' ') { i++; }
        if (s.text[i] && s.text[i] != '\n') { s.row[s.rows++] = s.text + i; }
        while (s.text[i] && s.text[i] != '\n') { i++; }
        if (s.text[i]) { s.text[i++] = 0; }
    }
    s.pic = pic_load(s.flc.image, MOVIE_SCREEN_W, MOVIE_SCREEN_H, s.flc.palette, 0);
    if (!s.pic) { flc_close(&s.flc); return 0; }
    s.ask = ask; s.done = done; s.active = 1; s.wait = 0.0;
    scene_compose();
    return 1;
}

/* A page on: the next one, or whether this was the last. */
static int scene_turn(void)
{
    if ((s.page + 1) * SCENE_LINES < s.rows) { s.page++; scene_compose(); return 1; }
    return 0;
}

void movie_scene_frame(float dt, int key, int click)
{
    if (!s.active) { return; }
    {
        int last = (s.page + 1) * SCENE_LINES >= s.rows;

        /* Y and N answer a question on its last page; Escape is a no there (the original has none), and any other leaving click is a click. */
        if (s.ask && last) {
            if (key == 'y' || key == 'Y') { scene_end(MOVIE_ANSWER_YES); return; }
            if (key == 'n' || key == 'N' || key == SCENE_KEY_ESCAPE) { scene_end(MOVIE_ANSWER_NO); return; }
        }
        if (click || key == SCENE_KEY_ENTER || key == SCENE_KEY_SPACE || (!s.ask && key == SCENE_KEY_ESCAPE)) {
            if (scene_turn()) { return; }
            if (!s.ask) { scene_end(MOVIE_ANSWER_LEFT); return; }
        }
    }
    /* The animation: a frame every flc_frame_ms; the file's last frame is a delta back to the first, so the loop goes on at the second. */
    s.wait += (double)dt;
    if (s.wait * 1000.0 >= (double)flc_frame_ms(&s.flc)) {
        s.wait -= (double)flc_frame_ms(&s.flc) / 1000.0;
        if (s.wait > 1.0) { s.wait = 0.0; }
        if (!flc_next(&s.flc)) { flc_rewind(&s.flc); flc_next(&s.flc); }
        scene_compose();
    }
}

const unsigned char *movie_scene_pixels(const unsigned char **palette)
{
    if (!s.active) { return 0; }
    if (palette) { *palette = s.flc.palette; }
    return s.shown;
}
