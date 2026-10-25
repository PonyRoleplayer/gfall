#ifndef ENG_MUSIC_H
#define ENG_MUSIC_H

#include "engine/assets/song.h"

/*
 * MIDI music on the card's own synthesizer. Read docs/sound.md before
 * editing.
 *
 * The whole song is turned into Windows MIDI stream events once, when it
 * starts, and handed to the driver in a few buffers. The driver times
 * and sends every note itself; the CPU's share is the driver's timer.
 * music_update only gives each buffer back to the stream when it is
 * done, which is the loop: the first buffer, finished first, goes back
 * in behind the last.
 *
 * At most music_voices notes sound at once (default 32, the AWE64's
 * hardware voices; past them its driver plays notes on the CPU). A note
 * that would be one more ends the oldest first. This is decided when the
 * events are made, so it costs nothing while playing.
 *
 * No device (hostcheck, a machine with no synthesizer): every call
 * works, and music_render still makes the events, for the tests.
 */

/* Most events a song may have. */
#define MUSIC_EVENTS 32768
#define MUSIC_VOICES_MAX 32

/* What the open device is, so the game can choose its FM arrangements. */
enum {
    MUSIC_NONE,
    MUSIC_SYNTH,
    MUSIC_FM,
    MUSIC_PORT
};

/* Open the device music_device names (-1: the best one). 0 when none. */
int music_startup(void);
void music_shutdown(void);
int music_kind(void);
/* The open device's name, "" for none. */
const char *music_device_name(void);
/* The machine's MIDI outputs, for the console. */
int music_devices(void);
const char *music_device_at(int index);

/*
 * A buffer of MUSIC_EVENTS for a loader to build a song in. Kept for the
 * process; music_play copies out of it, so the next load may reuse it.
 */
SongEvent *music_scratch(void);
/* Into time order; at one tick, note-offs before the rest. */
void song_sort(Song *song);

/*
 * Play song from its start, looping, as name. The song is copied: the
 * caller's may go. Playing the name already playing does nothing.
 * 0 when it could not start (no device: it is still kept, and plays
 * when music is turned on with a device).
 */
int music_play(const Song *song, const char *name);
void music_stop(void);
/* Give finished buffers back to the stream. Once a frame. */
void music_update(void);
/*
 * The song chosen, "" for none; music_playing says whether the device
 * is playing it (not with no device, or with music off).
 */
const char *music_name(void);
int music_playing(void);

/* Off stops the song and keeps it; on plays it again. Default on. */
void music_set_on(int on);
int music_on(void);
/* 0..100. Default 80. */
void music_set_volume(int percent);
int music_volume(void);
/* 1..MUSIC_VOICES_MAX notes at once. Default 32. */
void music_set_voices(int voices);
int music_voices(void);
/* -1 chooses; 0.. a device by its number. Opens it again. */
void music_set_device(int index);
int music_device(void);
/* Notes ended early by the voice limit, and the most at once, in the playing song. */
void music_stats(int *stolen, int *peak, int *events);

/*
 * The stream events for song: three words each (delta ticks, 0, event),
 * as midiStreamOut takes them. Notes past voices end the oldest; every
 * channel volume (controller 7) is scaled by volume percent, and each
 * channel starts at its General MIDI default, 100, scaled. Returns the
 * words written, or 0 when cap is too small.
 */
int music_render(const Song *song, df_u32 *out, int cap, int voices, int volume,
                 int *stolen, int *peak);

#endif
