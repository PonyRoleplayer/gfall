#include "game/dagger/play/ui/movie.h"
#include "game/dagger/play/ui/screen.h"
#include "game/dagger/formats/vid.h"
#include "engine/ui/pic.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/core/mem.h"

/* The classic screen a movie is drawn in, and how far the wall clock may run from the sound's before it is set to it (ticks of 60 Hz: 100 ms). */
#define MOVIE_SCREEN_W 320
#define MOVIE_SCREEN_H 200
#define MOVIE_DRIFT_TICKS 6
/* The most frames decoded in one call (a slow frame catches up, the picture is drawn once). */
#define MOVIE_CATCH_UP 8

static struct {
    int active, pic, stream, frames;
    Vid vid;
    double clock, due;          /* ticks since the start, and when the next frame is due */
    int have_frame;             /* the frame in vid.frame has not been drawn into the picture yet */
    df_u32 pushed;              /* audio samples handed to the stream */
    int rate;
    int w, mute;
} g;
/* The song that played before the first movie of a run, which comes back after the last. */
static int g_music_was, g_music_held;

/* The audio chunks as the reader finds them: the stream opens at the first, at the movie's rate. */
static void take_audio(void *ctx, const df_u8 *data, int bytes, int rate)
{
    (void)ctx;
    if (!g.stream && !g.mute && rate > 0) { g.stream = sound_stream_open(rate); g.rate = rate; if (!g.stream) { g.mute = 1; } }
    if (g.stream > 0) { sound_stream_push(g.stream, data, bytes); g.pushed += (df_u32)bytes; }
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
void movie_set_enabled(int on) { g_enabled = on; if (!on) { g_queued = 0; movie_stop(); } }
int movie_enabled(void) { return g_enabled; }

int movie_active(void) { return g.active || g_queued > 0; }
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

int movie_frame(float dt, int skip)
{
    int guard;

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
    /* Held to the sound's clock once it is playing: the samples the card has taken, at the movie's rate. */
    if (g.stream > 0 && g.rate > 0) {
        int queued;
        double played;

        queued = sound_stream_queued(g.stream);
        if (queued < 0) {
            /* The card stopped it (no card, sound off): the movie runs on its ticks, and later chunks are dropped. */
            sound_stream_close(g.stream); g.stream = 0; g.mute = 1;
        } else {
            played = ((double)g.pushed - (double)queued) * VID_TICK_HZ / (double)g.rate;
            if (played > 0.0 && (g.clock - played > MOVIE_DRIFT_TICKS || played - g.clock > MOVIE_DRIFT_TICKS)) { g.clock = played; }
        }
    }
    for (guard = 0; guard < MOVIE_CATCH_UP && g.clock >= g.due; guard++) {
        if (!vid_next(&g.vid)) { movie_stop(); return movie_active(); }
        if (g.vid.palette_changed) { pic_set_palette(g.pic, g.vid.palette); }
        pic_update(g.pic, g.vid.frame);
        g.frames++;
        g.due += g.vid.ticks;
    }
    /* Far behind (a long stall): drop the lost time instead of racing through frames. */
    if (g.clock >= g.due) { g.due = g.clock + 1.0; }
    return 1;
}

void movie_draw(void)
{
    if (!g.active || !g.pic) { return; }
    blit(g.pic, (MOVIE_SCREEN_W - g.w) / 2, 0, g.w, MOVIE_SCREEN_H);
}
