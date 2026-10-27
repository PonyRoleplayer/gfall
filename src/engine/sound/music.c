#include "engine/sound/music.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See music.h and docs/sound.md. The events are made here in plain C,
 * so hostcheck runs them; the stream calls are WINMM's, loaded at
 * runtime (engine/platform/winmm.c).
 */

/* The stream event types, high byte of the third word (MEVT_* in mmsystem.h). */
#define EV_SHORT 0x00000000u
#define EV_TEMPO 0x01000000u
#define EV_NOP 0x02000000u

/* Words in the made stream: three an event, room for the voice limit's extra note-offs. */
#define STREAM_WORDS ((MUSIC_EVENTS + MUSIC_EVENTS / 8 + 64) * 3)
/* Bytes a stream buffer, under midiStreamOut's 64 KB. */
#define CHUNK_WORDS (32768 / 4 / 3 * 3)
#define HEADERS 40

static SongEvent *g_scratch;
static SongEvent *g_sort_tmp;
static SongEvent *g_store;
static Song g_song;
static int g_have_song;
static char g_name[16];
static df_u32 *g_stream;
static int g_words;
static int g_on = 1;
static int g_volume = 80;
static int g_voices = MUSIC_VOICES_MAX;
static int g_device = -1;
static int g_stolen;
static int g_peak;
static int g_playing;

static SongEvent *keep_events(SongEvent **slot)
{
    if (!*slot) {
        *slot = (SongEvent *)mem_keep((df_u32)MUSIC_EVENTS * (df_u32)sizeof(SongEvent));
    }
    return *slot;
}

SongEvent *music_scratch(void)
{
    return keep_events(&g_scratch);
}

/* Note-offs, and note-ons at velocity 0, go before anything else at their tick. */
static int rank(df_u32 msg)
{
    df_u32 hi;

    if ((msg & 0xff000000u) == SONG_TEMPO) {
        return 1;
    }
    hi = msg & 0xf0u;
    if (hi == 0x80u || (hi == 0x90u && ((msg >> 16) & 0x7fu) == 0)) {
        return 0;
    }
    return 1;
}

static int before(const SongEvent *a, const SongEvent *b)
{
    if (a->tick != b->tick) {
        return a->tick < b->tick;
    }
    return rank(a->msg) < rank(b->msg);
}

/* The end of the run in order (none of it before the event ahead of it) that starts at lo, at most n. */
static int run_end(const SongEvent *ev, int lo, int n)
{
    int i;

    for (i = lo + 1; i < n && !before(&ev[i], &ev[i - 1]); i++) {
    }
    return i;
}

/*
 * Natural merge sort: stable, so a track's own order holds within a
 * tick. Each pass merges neighbouring runs already in order; the
 * tracks come in order, so a song is a few runs, a few passes, where
 * merging from runs of one took 14 (9 ms of a song's 15 on the 233,
 * in the frame the song changed). Stable, so the order is the same.
 */
void song_sort(Song *song)
{
    SongEvent *src;
    SongEvent *dst;
    int n;

    n = song ? song->count : 0;
    if (n < 2 || n > MUSIC_EVENTS || !keep_events(&g_sort_tmp)) {
        return;
    }
    src = song->events;
    dst = g_sort_tmp;
    for (;;) {
        SongEvent *t;
        int lo;
        int runs;

        runs = 0;
        for (lo = 0; lo < n;) {
            int mid;
            int hi;
            int i;
            int j;
            int k;

            mid = run_end(src, lo, n);
            hi = mid < n ? run_end(src, mid, n) : n;
            i = lo;
            j = mid;
            for (k = lo; k < hi; k++) {
                if (i < mid && (j >= hi || !before(&src[j], &src[i]))) {
                    dst[k] = src[i++];
                } else {
                    dst[k] = src[j++];
                }
            }
            runs++;
            lo = hi;
        }
        t = src;
        src = dst;
        dst = t;
        if (runs <= 1) {
            break;
        }
    }
    if (src != song->events) {
        int i;

        for (i = 0; i < n; i++) {
            song->events[i] = src[i];
        }
    }
}

typedef struct Out {
    df_u32 *w;
    int left;
    df_u32 prev;
} Out;

static int emit(Out *o, df_u32 tick, df_u32 ev)
{
    if (o->left < 3) {
        o->left = -1;
        return 0;
    }
    o->w[0] = tick - o->prev;
    o->w[1] = 0;
    o->w[2] = ev;
    o->w += 3;
    o->left -= 3;
    o->prev = tick;
    return 1;
}

int music_render(const Song *song, df_u32 *out, int cap, int voices, int volume,
                 int *stolen, int *peak)
{
    /* Sounding notes, oldest first: channel in the high byte, key in the low. */
    int active[MUSIC_VOICES_MAX];
    int nactive;
    int nstolen;
    int most;
    Out o;
    int i;

    if (!song || !out || song->count < 0) {
        return 0;
    }
    if (voices < 1) {
        voices = 1;
    }
    if (voices > MUSIC_VOICES_MAX) {
        voices = MUSIC_VOICES_MAX;
    }
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 100) {
        volume = 100;
    }
    o.w = out;
    o.left = cap;
    o.prev = 0;
    nactive = 0;
    nstolen = 0;
    most = 0;
    emit(&o, 0, EV_TEMPO | (song->tempo & 0xffffffu));
    for (i = 0; i < 16; i++) {
        emit(&o, 0, EV_SHORT | (df_u32)(0xb0 | i) | (7u << 8) | ((df_u32)(100 * volume / 100) << 16));
    }
    for (i = 0; i < song->count; i++) {
        df_u32 msg;
        df_u32 tick;
        int hi;
        int ch;
        int d1;
        int d2;
        int k;

        msg = song->events[i].msg;
        tick = song->events[i].tick;
        if ((msg & 0xff000000u) == SONG_TEMPO) {
            emit(&o, tick, EV_TEMPO | (msg & 0xffffffu));
            continue;
        }
        hi = (int)(msg & 0xf0u);
        ch = (int)(msg & 0x0fu);
        d1 = (int)((msg >> 8) & 0x7fu);
        d2 = (int)((msg >> 16) & 0x7fu);
        if (hi == 0x90 && d2 > 0) {
            int key;

            key = (ch << 8) | d1;
            /* The same key again on its channel: end the first. */
            for (k = 0; k < nactive; k++) {
                if (active[k] == key) {
                    break;
                }
            }
            if (k < nactive) {
                emit(&o, tick, EV_SHORT | (df_u32)(0x80 | ch) | ((df_u32)d1 << 8));
                for (; k + 1 < nactive; k++) {
                    active[k] = active[k + 1];
                }
                nactive--;
            }
            if (nactive >= voices) {
                int old;

                old = active[0];
                emit(&o, tick, EV_SHORT | (df_u32)(0x80 | (old >> 8)) | ((df_u32)(old & 0x7f) << 8));
                for (k = 0; k + 1 < nactive; k++) {
                    active[k] = active[k + 1];
                }
                nactive--;
                nstolen++;
            }
            active[nactive++] = key;
            if (nactive > most) {
                most = nactive;
            }
            emit(&o, tick, EV_SHORT | (msg & 0xffffffu));
        } else if (hi == 0x80 || hi == 0x90) {
            int key;

            key = (ch << 8) | d1;
            for (k = 0; k < nactive; k++) {
                if (active[k] == key) {
                    break;
                }
            }
            /* A note the limit already ended has had its note-off. */
            if (k < nactive) {
                for (; k + 1 < nactive; k++) {
                    active[k] = active[k + 1];
                }
                nactive--;
                emit(&o, tick, EV_SHORT | (msg & 0xffffffu));
            }
        } else if (hi == 0xb0 && d1 == 7) {
            emit(&o, tick, EV_SHORT | (df_u32)(0xb0 | ch) | (7u << 8) | ((df_u32)(d2 * volume / 100) << 16));
        } else {
            if (hi == 0xb0 && (d1 == 120 || d1 == 123)) {
                /* All sound or all notes off on this channel. */
                int w;

                w = 0;
                for (k = 0; k < nactive; k++) {
                    if ((active[k] >> 8) != ch) {
                        active[w++] = active[k];
                    }
                }
                nactive = w;
            }
            emit(&o, tick, EV_SHORT | (msg & 0xffffffu));
        }
    }
    {
        df_u32 end;

        end = song->length > o.prev ? song->length : o.prev;
        for (i = 0; i < nactive; i++) {
            emit(&o, end, EV_SHORT | (df_u32)(0x80 | (active[i] >> 8)) | ((df_u32)(active[i] & 0x7f) << 8));
        }
        /* The rest after the last note, so the loop keeps the song's own length. */
        emit(&o, end, EV_NOP);
    }
    if (stolen) {
        *stolen = nstolen;
    }
    if (peak) {
        *peak = most;
    }
    if (o.left < 0) {
        return 0;
    }
    return (int)(o.w - out);
}

void music_stats(int *stolen, int *peak, int *events)
{
    if (stolen) {
        *stolen = g_stolen;
    }
    if (peak) {
        *peak = g_peak;
    }
    if (events) {
        *events = g_words / 3;
    }
}

const char *music_name(void)
{
    return g_have_song ? g_name : "";
}

int music_playing(void)
{
    return g_playing;
}

int music_on(void)
{
    return g_on;
}

int music_volume(void)
{
    return g_volume;
}

int music_voices(void)
{
    return g_voices;
}

int music_device(void)
{
    return g_device;
}

static int device_start(void);
static void device_stop(void);
static int device_volume(int percent);

/* Make the events from the kept song and start the stream. */
static int restart(void)
{
    int volume;

    device_stop();
    g_playing = 0;
    if (!g_have_song || !g_on) {
        return 0;
    }
    if (!g_stream) {
        g_stream = (df_u32 *)mem_keep((df_u32)STREAM_WORDS * 4u);
    }
    if (!g_stream) {
        return 0;
    }
    /* A device with its own volume plays the song as written and is turned down instead. */
    volume = device_volume(g_volume) ? 100 : g_volume;
    g_words = music_render(&g_song, g_stream, STREAM_WORDS, g_voices, volume, &g_stolen, &g_peak);
    if (g_words == 0) {
        log_print(LOG_WARN, "music: %s has too many events", g_name);
        return 0;
    }
    if (!device_start()) {
        return 0;
    }
    g_playing = 1;
    return 1;
}

int music_play(const Song *song, const char *name)
{
    int i;

    if (!song || song->count <= 0 || song->count > MUSIC_EVENTS || song->division <= 0) {
        return 0;
    }
    if (!name) {
        name = "";
    }
    if (g_have_song && g_playing && df_eq(name, g_name)) {
        return 1;
    }
    if (!keep_events(&g_store)) {
        return 0;
    }
    for (i = 0; i < song->count; i++) {
        g_store[i] = song->events[i];
    }
    g_song = *song;
    g_song.events = g_store;
    g_have_song = 1;
    df_copy(g_name, name, (int)sizeof(g_name));
    return restart();
}

void music_stop(void)
{
    device_stop();
    g_playing = 0;
    g_have_song = 0;
    g_name[0] = 0;
}

void music_set_on(int on)
{
    on = on ? 1 : 0;
    if (on == g_on) {
        return;
    }
    g_on = on;
    if (g_on) {
        restart();
    } else {
        device_stop();
        g_playing = 0;
    }
}

void music_set_volume(int percent)
{
    percent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    if (percent == g_volume) {
        return;
    }
    g_volume = percent;
    /* Turned down at the device if it can; otherwise the events change: start again. */
    if (!device_volume(g_volume) && g_playing) {
        restart();
    }
}

void music_set_voices(int voices)
{
    voices = voices < 1 ? 1 : (voices > MUSIC_VOICES_MAX ? MUSIC_VOICES_MAX : voices);
    if (voices == g_voices) {
        return;
    }
    g_voices = voices;
    if (g_playing) {
        restart();
    }
}

#ifdef DF_HOST
int music_startup(void)
{
    return 0;
}

void music_shutdown(void)
{
    music_stop();
}

int music_kind(void)
{
    return MUSIC_NONE;
}

const char *music_device_name(void)
{
    return "";
}

int music_devices(void)
{
    return 0;
}

const char *music_device_at(int index)
{
    (void)index;
    return "";
}

void music_set_device(int index)
{
    g_device = index;
}

void music_update(void)
{
}

static int device_start(void)
{
    return 0;
}

static void device_stop(void)
{
}

static int device_volume(int percent)
{
    (void)percent;
    return 0;
}
#else

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "engine/platform/winmm.h"

#ifndef MOD_SWSYNTH
#define MOD_SWSYNTH 7
#endif
#ifndef MOD_WAVETABLE
#define MOD_WAVETABLE 6
#endif

static HMIDISTRM g_strm;
static int g_open;
static int g_kind;
static int g_has_volume;
static char g_dev_name[MAXPNAMELEN + 1];
static MIDIHDR g_hdr[HEADERS];
static int g_nhdr;
static int g_next;
/*
 * Buffers the driver has given back (MOM_DONE, counted by the callback)
 * and buffers we have queued again. g_gen marks this start's buffers in
 * their dwUser, so the ones a stop hands back do not count.
 */
static volatile LONG g_done;
static LONG g_taken;
static volatile LONG g_gen;
static int g_warned;
static char g_at_name[MAXPNAMELEN + 1];

int music_kind(void)
{
    return g_open ? g_kind : MUSIC_NONE;
}

const char *music_device_name(void)
{
    return g_open ? g_dev_name : "";
}

int music_devices(void)
{
    const Winmm *mm;

    mm = winmm();
    return mm ? (int)mm->midiOutGetNumDevs() : 0;
}

const char *music_device_at(int index)
{
    const Winmm *mm;
    MIDIOUTCAPSA caps;

    mm = winmm();
    g_at_name[0] = 0;
    if (mm && mm->midiOutGetDevCapsA((UINT_PTR)index, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        df_copy(g_at_name, caps.szPname, (int)sizeof(g_at_name));
    }
    return g_at_name;
}

static int has(const char *s, const char *word)
{
    int i;

    for (i = 0; s[i]; i++) {
        int k;

        for (k = 0; word[k]; k++) {
            char c;

            c = s[i + k];
            if (c >= 'A' && c <= 'Z') {
                c = (char)(c - 'A' + 'a');
            }
            if (c != word[k]) {
                break;
            }
        }
        if (!word[k]) {
            return 1;
        }
    }
    return 0;
}

/*
 * How well a device plays the music. The card's own synthesizer first
 * (an AWE64's "SB AWE64 MIDI Synth"), then FM, then a MIDI port (a
 * socket on the back that may have nothing plugged in). A synthesizer
 * in software is never chosen by itself: it is what the 32-voice limit
 * keeps the CPU out of.
 */
static int score(const MIDIOUTCAPSA *caps)
{
    int s;

    if (caps->wTechnology == MOD_SWSYNTH
        || has(caps->szPname, "wave synth") || has(caps->szPname, "wavesynth")
        || has(caps->szPname, "sw synth") || has(caps->szPname, "software")
        || has(caps->szPname, "soft synth")) {
        return 0;
    }
    switch (caps->wTechnology) {
    case MOD_MIDIPORT:
        s = 1;
        break;
    case MOD_FMSYNTH:
    case MOD_SQSYNTH:
        s = 2;
        break;
    default:
        s = 4;
        break;
    }
    if (has(caps->szPname, "awe")) {
        s += 2;
    }
    return s;
}

/*
 * The driver's "buffer done", on a thread of the system's. A MIDI
 * callback may call almost nothing (not midiStreamOut), so it only
 * counts; music_update queues the buffer again. Polling MHDR_DONE with
 * no callback played each song once on Windows 98: a 32-bit header is
 * thunked to the 16-bit MMSYSTEM, and with CALLBACK_NULL its done flag
 * was never seen to come back.
 */
static void CALLBACK on_done(HMIDIOUT out, UINT msg, DWORD_PTR user, DWORD_PTR p1, DWORD_PTR p2)
{
    (void)out;
    (void)user;
    (void)p2;
    if (msg == MOM_DONE && p1 && ((MIDIHDR *)p1)->dwUser == (DWORD_PTR)g_gen) {
        /* Its only writer: a plain add, no call (a callback may make almost none). */
        g_done = g_done + 1;
    }
}

static void close_device(void)
{
    const Winmm *mm;

    mm = winmm();
    device_stop();
    if (g_open && mm) {
        mm->midiStreamClose(g_strm);
    }
    g_open = 0;
}

static int open_device(void)
{
    const Winmm *mm;
    MIDIOUTCAPSA caps;
    UINT id;
    int n;
    int i;
    int best;
    int best_score;

    close_device();
    mm = winmm();
    if (!mm) {
        return 0;
    }
    n = (int)mm->midiOutGetNumDevs();
    best = -1;
    best_score = 0;
    for (i = 0; i < n; i++) {
        int s;

        if (mm->midiOutGetDevCapsA((UINT_PTR)i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
            continue;
        }
        s = score(&caps);
        log_print(LOG_INFO, "music: device %d \"%s\" technology %d voices %d notes %d",
                  i, caps.szPname, (int)caps.wTechnology, (int)caps.wVoices, (int)caps.wNotes);
        if (g_device == i) {
            best = i;
            best_score = 100;
        } else if (s > best_score && best_score < 100) {
            best = i;
            best_score = s;
        }
    }
    if (best < 0) {
        log_text(LOG_INFO, "music: no MIDI device to play on");
        return 0;
    }
    if (mm->midiOutGetDevCapsA((UINT_PTR)best, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
        return 0;
    }
    id = (UINT)best;
    if (mm->midiStreamOpen(&g_strm, &id, 1, (DWORD_PTR)on_done, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        log_print(LOG_WARN, "music: midiStreamOpen failed on \"%s\"", caps.szPname);
        return 0;
    }
    g_open = 1;
    df_copy(g_dev_name, caps.szPname, (int)sizeof(g_dev_name));
    g_has_volume = (caps.dwSupport & MIDICAPS_VOLUME) != 0;
    if (caps.wTechnology == MOD_FMSYNTH || caps.wTechnology == MOD_SQSYNTH) {
        g_kind = MUSIC_FM;
    } else if (caps.wTechnology == MOD_MIDIPORT) {
        g_kind = MUSIC_PORT;
    } else {
        g_kind = MUSIC_SYNTH;
    }
    log_print(LOG_INFO, "music: \"%s\"%s", g_dev_name, g_has_volume ? ", its own volume" : "");
    return 1;
}

int music_startup(void)
{
    if (!open_device()) {
        return 0;
    }
    if (g_have_song) {
        restart();
    }
    return 1;
}

void music_shutdown(void)
{
    close_device();
    g_playing = 0;
}

void music_set_device(int index)
{
    g_device = index;
    if (open_device() && g_have_song) {
        restart();
    }
}

static void device_stop(void)
{
    const Winmm *mm;
    int i;

    mm = winmm();
    if (!g_open || !mm) {
        g_nhdr = 0;
        return;
    }
    /* What the stop hands back is not to be queued again. */
    g_gen = g_gen + 1;
    mm->midiStreamStop(g_strm);
    /* Every note off, every controller back: the next song starts clean. */
    mm->midiOutReset((HMIDIOUT)g_strm);
    for (i = 0; i < g_nhdr; i++) {
        mm->midiOutUnprepareHeader((HMIDIOUT)g_strm, &g_hdr[i], sizeof(MIDIHDR));
    }
    g_nhdr = 0;
}

static int device_volume(int percent)
{
    const Winmm *mm;
    DWORD v;

    mm = winmm();
    if (!g_open || !g_has_volume || !mm) {
        return 0;
    }
    v = (DWORD)(percent * 65535 / 100);
    return mm->midiOutSetVolume((HMIDIOUT)g_strm, v | (v << 16)) == MMSYSERR_NOERROR;
}

static int device_start(void)
{
    const Winmm *mm;
    MIDIPROPTIMEDIV div;
    int at;
    int per;
    int i;

    mm = winmm();
    if (!g_open || !mm) {
        return 0;
    }
    div.cbStruct = sizeof(div);
    div.dwTimeDiv = (DWORD)g_song.division;
    if (mm->midiStreamProperty(g_strm, (LPBYTE)&div, MIDIPROP_SET | MIDIPROP_TIMEDIV) != MMSYSERR_NOERROR) {
        log_text(LOG_WARN, "music: the stream took no time division");
        return 0;
    }
    /*
     * At least two buffers, so one is always queued while the other comes
     * back: a song in one buffer would wait a frame at every loop.
     */
    per = CHUNK_WORDS;
    if (g_words <= per) {
        per = (g_words / 3 + 1) / 2 * 3;
        if (per < 3) {
            per = 3;
        }
    }
    g_nhdr = 0;
    g_done = 0;
    g_taken = 0;
    for (at = 0; at < g_words && g_nhdr < HEADERS; at += per) {
        MIDIHDR *h;
        int words;

        words = g_words - at < per ? g_words - at : per;
        h = &g_hdr[g_nhdr];
        mem_zero(h, sizeof(*h));
        h->dwUser = (DWORD_PTR)g_gen;
        h->lpData = (LPSTR)(g_stream + at);
        h->dwBufferLength = (DWORD)words * 4u;
        h->dwBytesRecorded = (DWORD)words * 4u;
        if (mm->midiOutPrepareHeader((HMIDIOUT)g_strm, h, sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
            break;
        }
        g_nhdr++;
    }
    if (at < g_words) {
        log_print(LOG_WARN, "music: %s did not fit the stream's buffers", g_name);
        device_stop();
        return 0;
    }
    for (i = 0; i < g_nhdr; i++) {
        if (mm->midiStreamOut(g_strm, &g_hdr[i], sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
            log_text(LOG_WARN, "music: midiStreamOut failed");
            device_stop();
            return 0;
        }
    }
    g_next = 0;
    mm->midiStreamRestart(g_strm);
    device_volume(g_volume);
    return 1;
}

void music_update(void)
{
    const Winmm *mm;
    LONG done;
    int again;

    mm = winmm();
    if (!g_open || !g_playing || !mm || g_nhdr == 0) {
        return;
    }
    /*
     * They come back in order: each goes back in behind the last, which
     * is the loop. One read of the count; the callback may add to it.
     */
    done = g_done;
    again = 0;
    while (g_taken < done) {
        MIDIHDR *h;
        MMRESULT r;

        h = &g_hdr[g_next];
        h->dwFlags &= ~(DWORD)(MHDR_DONE | MHDR_INQUEUE);
        h->dwBytesRecorded = h->dwBufferLength;
        r = mm->midiStreamOut(g_strm, h, sizeof(MIDIHDR));
        if (r != MMSYSERR_NOERROR) {
            if (!g_warned) {
                log_print(LOG_WARN, "music: buffer %d not queued again (error %d); the loop stops",
                          g_next, (int)r);
                g_warned = 1;
            }
            break;
        }
        g_taken++;
        g_next = (g_next + 1) % g_nhdr;
        again = 1;
        if (g_next == 0) {
            log_print(LOG_DEBUG, "music: %s round again", g_name);
        }
    }
    /*
     * A stream that ran dry while a frame stalled is paused waiting; a
     * restart on one that plays does nothing.
     */
    if (again) {
        mm->midiStreamRestart(g_strm);
    }
}
#endif
