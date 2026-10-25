/*
 * See audio.h and docs/sound.md. DSOUND.DLL and WINMM.DLL are loaded at
 * runtime, as glide2x.dll is. DirectSound 3's calls only (Windows 95 and
 * NT 4 have no more), and its buffer description: a later one is a size
 * the older DSOUND.DLL rejects.
 */

#include "engine/sound/audio.h"
#include "engine/core/log.h"

static const char *const g_names[] = { "auto", "primary", "secondary", "waveout", "none" };

const char *audio_mode_name(int mode)
{
    if (mode < AUDIO_AUTO || mode > AUDIO_NONE) {
        mode = AUDIO_NONE;
    }
    return g_names[mode];
}

#ifdef DF_HOST
/* hostcheck has no card; the mixer is tested on its own. */
int audio_open(void *hwnd, int rate, int want)
{
    (void)hwnd;
    (void)rate;
    (void)want;
    return AUDIO_NONE;
}

void audio_close(void)
{
}

int audio_mode(void)
{
    return AUDIO_NONE;
}

int audio_rate(void)
{
    return 0;
}

void audio_update(AudioFill fill)
{
    (void)fill;
}

int audio_buffer_ms(void)
{
    return 0;
}

int audio_lead_ms(void)
{
    return 0;
}
#else

#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include "engine/core/mem.h"
#include "engine/platform/winmm.h"

/* Frames mixed at a time, into g_mix, before they are copied out. */
#define CHUNK 1024
/* The DirectSound ring we ask for: half a second. */
#define RING_SECS 0.5f
/* A written primary buffer shorter than this cannot hold a slow frame's lead. */
#define PRIMARY_MIN_SECS 0.2f
/* Wave blocks: 16 of a fortieth of a second (276 frames at 11025, rounded up to 4). */
#define BLOCKS 16
#define LEAD_MIN 0.06f
#define LEAD_PAD 0.04f

typedef HRESULT (WINAPI *DsCreate)(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN);

static int g_mode = AUDIO_NONE;
static int g_rate;
static df_i16 g_mix[CHUNK * 2];
static float g_avg = 0.05f;
static int g_lead;

static HMODULE g_dsdll;
static LPDIRECTSOUND g_ds;
static LPDIRECTSOUNDBUFFER g_primary;
static LPDIRECTSOUNDBUFFER g_buf;
static DWORD g_size;
static DWORD g_write;
static DWORD g_after;
static int g_sync;

static HWAVEOUT g_wave;
static WAVEHDR g_hdr[BLOCKS];
static int g_busy[BLOCKS];
static df_i16 *g_blocks;
static int g_block_frames;
static int g_next;

int audio_mode(void)
{
    return g_mode;
}

int audio_rate(void)
{
    return g_mode == AUDIO_NONE ? 0 : g_rate;
}

int audio_buffer_ms(void)
{
    if (g_mode == AUDIO_WAVEOUT) {
        return g_rate ? BLOCKS * g_block_frames * 1000 / g_rate : 0;
    }
    if (g_mode == AUDIO_PRIMARY || g_mode == AUDIO_SECONDARY) {
        return g_rate ? (int)(g_size / 4u) * 1000 / g_rate : 0;
    }
    return 0;
}

int audio_lead_ms(void)
{
    return g_rate ? g_lead * 1000 / g_rate : 0;
}

static void format(WAVEFORMATEX *wf, int rate)
{
    mem_zero(wf, sizeof(*wf));
    wf->wFormatTag = WAVE_FORMAT_PCM;
    wf->nChannels = 2;
    wf->nSamplesPerSec = (DWORD)rate;
    wf->wBitsPerSample = 16;
    wf->nBlockAlign = 4;
    wf->nAvgBytesPerSec = (DWORD)rate * 4u;
}

/* Frames to keep mixed ahead: a frame and a half and LEAD_PAD, within the ring. */
static int lead_frames(float secs, int ring_frames)
{
    float lead;
    int n;

    if (secs > 0.0f && secs < 0.25f) {
        g_avg += (secs - g_avg) * 0.1f;
    }
    lead = 1.5f * g_avg + LEAD_PAD;
    if (lead < LEAD_MIN) {
        lead = LEAD_MIN;
    }
    n = df_iround(lead * (float)g_rate);
    if (n > ring_frames / 2) {
        n = ring_frames / 2;
    }
    return n;
}

/*
 * Mix frames into the locked pair of regions, from their start. A Lock
 * gives two when the span wraps round the end of the ring.
 */
static void put_mix(AudioFill fill, df_u8 *p1, DWORD b1, df_u8 *p2, DWORD b2, int frames)
{
    DWORD at;

    at = 0;
    while (frames > 0) {
        const df_u8 *src;
        DWORD bytes;
        DWORD k;
        int n;

        n = frames < CHUNK ? frames : CHUNK;
        __builtin_memset(g_mix, 0, (df_u32)n * 4u);
        fill(g_mix, n);
        bytes = (DWORD)n * 4u;
        src = (const df_u8 *)g_mix;
        for (k = 0; k < bytes; ) {
            df_u8 *dst;
            DWORD room;
            DWORD take;

            if (at < b1) {
                dst = p1 + at;
                room = b1 - at;
            } else {
                dst = p2 + (at - b1);
                room = b2 - (at - b1);
            }
            take = bytes - k < room ? bytes - k : room;
            if (take == 0) {
                return;
            }
            __builtin_memcpy(dst, src + k, take);
            k += take;
            at += take;
        }
        frames -= n;
    }
}

static void zero_from(df_u8 *p1, DWORD b1, df_u8 *p2, DWORD b2, DWORD at)
{
    if (at < b1) {
        __builtin_memset(p1 + at, 0, b1 - at);
        at = b1;
    }
    if (p2 && at - b1 < b2) {
        __builtin_memset(p2 + (at - b1), 0, b2 - (at - b1));
    }
}

static void ds_release(void)
{
    if (g_buf && g_buf != g_primary) {
        IDirectSoundBuffer_Stop(g_buf);
        IDirectSoundBuffer_Release(g_buf);
    }
    if (g_primary) {
        IDirectSoundBuffer_Stop(g_primary);
        IDirectSoundBuffer_Release(g_primary);
    }
    g_buf = 0;
    g_primary = 0;
    if (g_ds) {
        IDirectSound_Release(g_ds);
    }
    g_ds = 0;
}

/* The whole ring silent, before it plays. */
static void ds_clear(void)
{
    void *p1;
    void *p2;
    DWORD b1;
    DWORD b2;

    if (IDirectSoundBuffer_Lock(g_buf, 0, g_size, &p1, &b1, &p2, &b2, 0) == DS_OK) {
        zero_from((df_u8 *)p1, b1, (df_u8 *)p2, b2, 0);
        IDirectSoundBuffer_Unlock(g_buf, p1, b1, p2, b2);
    }
}

static int ds_open(HWND hwnd, int rate, int primary, int any_driver)
{
    DsCreate create;
    DSCAPS caps;
    DSBUFFERDESC1 desc;
    DSBCAPS bcaps;
    WAVEFORMATEX wf;
    WAVEFORMATEX got;
    DWORD got_size;

    if (!g_dsdll) {
        g_dsdll = LoadLibraryA("DSOUND.DLL");
    }
    if (!g_dsdll) {
        log_text(LOG_INFO, "audio: no DSOUND.DLL");
        return 0;
    }
    create = (DsCreate)(void *)GetProcAddress(g_dsdll, "DirectSoundCreate");
    if (!create || create(0, &g_ds, 0) != DS_OK || !g_ds) {
        log_text(LOG_INFO, "audio: DirectSoundCreate failed");
        g_ds = 0;
        return 0;
    }
    mem_zero(&caps, sizeof(caps));
    caps.dwSize = sizeof(caps);
    if (IDirectSound_GetCaps(g_ds, &caps) == DS_OK && (caps.dwFlags & DSCAPS_EMULDRIVER) && !any_driver) {
        log_text(LOG_INFO, "audio: DirectSound is emulated on this card; the wave calls are cheaper");
        ds_release();
        return 0;
    }
    if (IDirectSound_SetCooperativeLevel(g_ds, (HWND)hwnd, primary ? DSSCL_WRITEPRIMARY : DSSCL_PRIORITY) != DS_OK) {
        log_print(LOG_INFO, "audio: no %s cooperative level", primary ? "write-primary" : "priority");
        ds_release();
        return 0;
    }
    mem_zero(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    if (IDirectSound_CreateSoundBuffer(g_ds, (LPCDSBUFFERDESC)(void *)&desc, &g_primary, 0) != DS_OK) {
        log_text(LOG_INFO, "audio: no primary buffer");
        g_primary = 0;
        ds_release();
        return 0;
    }
    format(&wf, rate);
    /* The card may keep a rate of its own: read back what it took. */
    IDirectSoundBuffer_SetFormat(g_primary, &wf);
    mem_zero(&got, sizeof(got));
    if (IDirectSoundBuffer_GetFormat(g_primary, &got, sizeof(got), &got_size) != DS_OK) {
        got = wf;
    }
    if (primary) {
        if (got.wFormatTag != WAVE_FORMAT_PCM || got.nChannels != 2 || got.wBitsPerSample != 16) {
            log_print(LOG_INFO, "audio: primary buffer is %d-bit %d channel, not 16-bit stereo",
                      (int)got.wBitsPerSample, (int)got.nChannels);
            ds_release();
            return 0;
        }
        g_rate = (int)got.nSamplesPerSec;
        g_buf = g_primary;
    } else {
        /* The secondary at the primary's rate, so DirectSound's mixer only copies. */
        if (got.wFormatTag == WAVE_FORMAT_PCM && got.nSamplesPerSec >= 8000 && got.nSamplesPerSec <= 48000) {
            rate = (int)got.nSamplesPerSec;
        }
        format(&wf, rate);
        mem_zero(&desc, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
        desc.dwBufferBytes = ((DWORD)(RING_SECS * (float)rate) & ~3u) * 4u;
        desc.lpwfxFormat = &wf;
        if (IDirectSound_CreateSoundBuffer(g_ds, (LPCDSBUFFERDESC)(void *)&desc, &g_buf, 0) != DS_OK) {
            /* DirectSound 3 knows no GETCURRENTPOSITION2. */
            desc.dwFlags = 0;
            if (IDirectSound_CreateSoundBuffer(g_ds, (LPCDSBUFFERDESC)(void *)&desc, &g_buf, 0) != DS_OK) {
                log_text(LOG_INFO, "audio: no secondary buffer");
                g_buf = 0;
                ds_release();
                return 0;
            }
        }
        g_rate = rate;
    }
    mem_zero(&bcaps, sizeof(bcaps));
    bcaps.dwSize = sizeof(bcaps);
    if (IDirectSoundBuffer_GetCaps(g_buf, &bcaps) != DS_OK || bcaps.dwBufferBytes < 1024) {
        ds_release();
        return 0;
    }
    g_size = bcaps.dwBufferBytes & ~3u;
    if (primary && (float)(g_size / 4u) < PRIMARY_MIN_SECS * (float)g_rate) {
        log_print(LOG_INFO, "audio: primary buffer only %d bytes", (int)g_size);
        ds_release();
        return 0;
    }
    ds_clear();
    if (IDirectSoundBuffer_Play(g_buf, 0, 0, DSBPLAY_LOOPING) != DS_OK) {
        ds_release();
        return 0;
    }
    g_sync = 1;
    return 1;
}

static void ds_update(AudioFill fill, float secs)
{
    DWORD play;
    DWORD wcur;
    DWORD ahead;
    DWORD safe;
    DWORD need;
    DWORD span;
    void *p1;
    void *p2;
    DWORD b1;
    DWORD b2;
    HRESULT hr;
    int ring;

    hr = IDirectSoundBuffer_GetCurrentPosition(g_buf, &play, &wcur);
    if (hr == DSERR_BUFFERLOST) {
        if (IDirectSoundBuffer_Restore(g_buf) == DS_OK) {
            ds_clear();
            IDirectSoundBuffer_Play(g_buf, 0, 0, DSBPLAY_LOOPING);
        }
        g_sync = 1;
        return;
    }
    if (hr != DS_OK) {
        return;
    }
    ring = (int)(g_size / 4u);
    g_lead = lead_frames(secs, ring);
    ahead = (g_write + g_size - play) % g_size;
    safe = (wcur + g_size - play) % g_size;
    /*
     * Start again at the write cursor: on the first refill, after a lost
     * buffer, when our writing fell inside what the card is about to
     * play, or when the time since the last refill was longer than what
     * was mixed ahead (the play cursor went past it, maybe round the ring).
     */
    if (g_sync || ahead < safe || (float)g_after < secs * (float)g_rate * 4.0f) {
        g_write = wcur;
        ahead = safe;
        g_sync = 0;
    }
    need = (DWORD)g_lead * 4u;
    if (ahead >= need) {
        g_after = ahead;
        return;
    }
    need -= ahead;
    /* Mixed, then silence up to the play cursor, round the ring. */
    span = g_size - ahead;
    if (IDirectSoundBuffer_Lock(g_buf, g_write, span, &p1, &b1, &p2, &b2, 0) != DS_OK) {
        return;
    }
    put_mix(fill, (df_u8 *)p1, b1, (df_u8 *)p2, b2, (int)(need / 4u));
    zero_from((df_u8 *)p1, b1, (df_u8 *)p2, b2, need);
    IDirectSoundBuffer_Unlock(g_buf, p1, b1, p2, b2);
    g_write = (g_write + need) % g_size;
    g_after = ahead + need;
}

static void wave_release(void)
{
    const Winmm *mm;
    int i;

    mm = winmm();
    if (!g_wave || !mm) {
        g_wave = 0;
        return;
    }
    mm->waveOutReset(g_wave);
    for (i = 0; i < BLOCKS; i++) {
        if (g_hdr[i].dwFlags & WHDR_PREPARED) {
            mm->waveOutUnprepareHeader(g_wave, &g_hdr[i], sizeof(WAVEHDR));
        }
    }
    mm->waveOutClose(g_wave);
    g_wave = 0;
}

static int wave_open(int rate)
{
    const Winmm *mm;
    WAVEFORMATEX wf;
    int i;

    mm = winmm();
    if (!mm) {
        return 0;
    }
    format(&wf, rate);
    if (mm->waveOutOpen(&g_wave, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        log_text(LOG_INFO, "audio: waveOutOpen failed");
        g_wave = 0;
        return 0;
    }
    g_rate = rate;
    g_block_frames = ((rate / 40) + 3) & ~3;
    if (!g_blocks) {
        /* Kept: the blocks outlive any place's heap. Sized for 48000. */
        g_blocks = (df_i16 *)mem_keep((df_u32)BLOCKS * (1200u + 4u) * 4u);
    }
    if (!g_blocks || g_block_frames > 1200 + 4) {
        wave_release();
        return 0;
    }
    for (i = 0; i < BLOCKS; i++) {
        mem_zero(&g_hdr[i], sizeof(WAVEHDR));
        g_hdr[i].lpData = (LPSTR)(g_blocks + i * g_block_frames * 2);
        g_hdr[i].dwBufferLength = (DWORD)g_block_frames * 4u;
        if (mm->waveOutPrepareHeader(g_wave, &g_hdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            wave_release();
            return 0;
        }
        g_busy[i] = 0;
    }
    g_next = 0;
    return 1;
}

static void wave_update(AudioFill fill, float secs)
{
    const Winmm *mm;
    int queued;
    int i;

    mm = winmm();
    if (!mm) {
        return;
    }
    g_lead = lead_frames(secs, BLOCKS * g_block_frames);
    queued = 0;
    for (i = 0; i < BLOCKS; i++) {
        if (g_busy[i] && (g_hdr[i].dwFlags & WHDR_DONE)) {
            g_busy[i] = 0;
        }
        queued += g_busy[i];
    }
    /* The blocks finish in the order written: g_next is the oldest free one. */
    while (queued * g_block_frames < g_lead && !g_busy[g_next]) {
        df_i16 *dst;

        dst = (df_i16 *)(void *)g_hdr[g_next].lpData;
        __builtin_memset(dst, 0, (df_u32)g_block_frames * 4u);
        fill(dst, g_block_frames);
        if (mm->waveOutWrite(g_wave, &g_hdr[g_next], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            break;
        }
        g_busy[g_next] = 1;
        queued++;
        g_next = (g_next + 1) % BLOCKS;
    }
}

int audio_open(void *hwnd, int rate, int want)
{
    audio_close();
    if (rate < 8000 || rate > 48000) {
        rate = 11025;
    }
    if ((want == AUDIO_AUTO || want == AUDIO_PRIMARY) && hwnd
        && ds_open((HWND)hwnd, rate, 1, want == AUDIO_PRIMARY)) {
        g_mode = AUDIO_PRIMARY;
    } else if ((want == AUDIO_AUTO || want == AUDIO_SECONDARY) && hwnd
               && ds_open((HWND)hwnd, rate, 0, want == AUDIO_SECONDARY)) {
        g_mode = AUDIO_SECONDARY;
    } else if ((want == AUDIO_AUTO || want == AUDIO_WAVEOUT) && wave_open(rate)) {
        g_mode = AUDIO_WAVEOUT;
    } else {
        g_mode = AUDIO_NONE;
    }
    if (g_mode != AUDIO_NONE) {
        log_print(LOG_INFO, "audio: %s, %d Hz 16-bit stereo, %d ms ring",
                  audio_mode_name(g_mode), g_rate, audio_buffer_ms());
    } else if (want != AUDIO_NONE) {
        log_text(LOG_WARN, "audio: no way out to the card; the sound is off");
    }
    return g_mode;
}

void audio_close(void)
{
    if (g_mode == AUDIO_PRIMARY || g_mode == AUDIO_SECONDARY) {
        ds_release();
    } else if (g_mode == AUDIO_WAVEOUT) {
        wave_release();
    }
    g_mode = AUDIO_NONE;
}

/* Seconds since the last refill, from the performance counter; 0 the first time. */
static float elapsed(void)
{
    static LARGE_INTEGER last;
    static LARGE_INTEGER freq;
    static int have;
    LARGE_INTEGER now;
    float secs;

    if (!have) {
        if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
            return 0.05f;
        }
        QueryPerformanceCounter(&last);
        have = 1;
        return 0.0f;
    }
    QueryPerformanceCounter(&now);
    secs = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
    last = now;
    return secs;
}

void audio_update(AudioFill fill)
{
    float secs;

    if (!fill) {
        return;
    }
    secs = elapsed();
    if (g_mode == AUDIO_PRIMARY || g_mode == AUDIO_SECONDARY) {
        ds_update(fill, secs);
    } else if (g_mode == AUDIO_WAVEOUT) {
        wave_update(fill, secs);
    }
}
#endif
