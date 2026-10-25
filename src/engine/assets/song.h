#ifndef ENG_SONG_H
#define ENG_SONG_H

#include "engine/core/df.h"

/*
 * A song: every track's MIDI messages merged into one list in time
 * order. The game fills one from its own files (Daggerfall's are HMI);
 * the engine plays it (engine/sound/music.c). Read docs/sound.md.
 *
 * msg is a channel message as midiOutShortMsg takes it: status in the
 * low byte, then the first and second data bytes. SONG_TEMPO in the high
 * byte makes it a tempo change instead, microseconds a quarter note in
 * the low 24 bits. Every note-on has its own note-off (or a note-on at
 * velocity 0) later in the list.
 */

#define SONG_TEMPO 0xff000000u

typedef struct SongEvent {
    df_u32 tick;
    df_u32 msg;
} SongEvent;

typedef struct Song {
    SongEvent *events;
    int count;
    /* Ticks a quarter note, and microseconds a quarter note at the start. */
    int division;
    df_u32 tempo;
    /* Ticks from the start to where the song begins again; at least the last event's. */
    df_u32 length;
} Song;

#endif
