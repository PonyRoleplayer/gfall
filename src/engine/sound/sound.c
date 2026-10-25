#include "engine/sound/sound.h"
#include "engine/sound/audio.h"
#include "engine/sound/mixer.h"

/*
 * See sound.h and docs/sound.md. A handle is the voice's slot in the low
 * five bits and a serial above, so a handle kept after its voice was
 * taken by another sound no longer matches it.
 */

typedef struct Voice {
    MixVoice mix;
    const SoundClip *clip;
    int handle;
    int positional;
    float x, y, z;
    float inner;
    float reach;
    float volume;
    float pan;
    /* The gain worked out last, 0..1, for choosing a voice to take. */
    float level;
} Voice;

static Voice g_voice[SOUND_VOICES];
static int g_serial = 1;
static int g_on = 1;
static int g_volume = 80;
static int g_rate = 11025;
static int g_mode = AUDIO_AUTO;
static int g_mix_rate = 11025;
static int g_open;
static void *g_hwnd;
static float g_lx, g_ly, g_lz;
static float g_rx = 1.0f;
static float g_rz;

static float sq_root(float v)
{
    float r;

    __asm__ __volatile__("fsqrt" : "=t"(r) : "0"(v));
    return r;
}

static void reopen(void)
{
    int mode;

    if (!g_open) {
        return;
    }
    sound_stop_all();
    mode = audio_open(g_hwnd, g_rate, g_mode);
    if (mode != AUDIO_NONE) {
        g_mix_rate = audio_rate();
    }
}

int sound_startup(void *hwnd)
{
    g_hwnd = hwnd;
    g_open = 1;
    reopen();
    return audio_mode() != AUDIO_NONE;
}

void sound_shutdown(void)
{
    sound_stop_all();
    audio_close();
    g_open = 0;
}

int sound_ready(void)
{
    return audio_mode() != AUDIO_NONE;
}

void sound_set_on(int on)
{
    g_on = on ? 1 : 0;
    if (!g_on) {
        sound_stop_all();
    }
}

int sound_on(void)
{
    return g_on;
}

void sound_set_volume(int percent)
{
    g_volume = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
}

int sound_volume(void)
{
    return g_volume;
}

void sound_set_rate(int rate)
{
    if (rate == g_rate) {
        return;
    }
    g_rate = rate;
    reopen();
}

int sound_rate(void)
{
    return sound_ready() ? audio_rate() : g_rate;
}

void sound_set_mode(int mode)
{
    if (mode < AUDIO_AUTO || mode > AUDIO_NONE || mode == g_mode) {
        return;
    }
    g_mode = mode;
    reopen();
}

int sound_mode(void)
{
    return g_mode;
}

void sound_set_mix_rate(int rate)
{
    if (rate > 0) {
        g_mix_rate = rate;
    }
}

void sound_listen(float x, float y, float z, float yaw)
{
    g_lx = x;
    g_ly = y;
    g_lz = z;
    /* Camera's right for yaw: (cos, 0, -sin). See camera_basis. */
    g_rx = df_cos(yaw);
    g_rz = -df_sin(yaw);
}

static Voice *voice_of(int handle)
{
    Voice *v;

    if (handle <= 0 || (handle & 31) >= SOUND_VOICES) {
        return 0;
    }
    v = &g_voice[handle & 31];
    if (v->handle != handle || !v->mix.on) {
        return 0;
    }
    return v;
}

static df_i16 q15(float g)
{
    if (g <= 0.0f) {
        return 0;
    }
    if (g >= 1.0f) {
        return 32767;
    }
    return (df_i16)df_iround(g * 32767.0f);
}

/*
 * The gain from distance and volume, and the two sides. Constant power
 * across the pan, raised so a sound straight ahead is at the voice's
 * full gain on both sides: cos and sin of 0..90 degrees times sqrt 2,
 * cut at 1. A point within 16 units of the head is heard from the middle.
 */
static void aim(Voice *v)
{
    float g;
    float pan;
    float a;

    g = 1.0f;
    pan = v->pan;
    if (v->positional) {
        float dx;
        float dy;
        float dz;
        float d2;
        float d;

        dx = v->x - g_lx;
        dy = v->y - g_ly;
        dz = v->z - g_lz;
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= v->reach * v->reach) {
            g = 0.0f;
            d = v->reach;
        } else {
            d = sq_root(d2);
            if (d > v->inner) {
                g = (v->reach - d) / (v->reach - v->inner);
            }
        }
        pan = (dx * g_rx + dz * g_rz) / (d + 16.0f);
    }
    if (pan < -1.0f) {
        pan = -1.0f;
    }
    if (pan > 1.0f) {
        pan = 1.0f;
    }
    g *= v->volume * (float)g_volume * 0.01f;
    v->level = g;
    a = (pan + 1.0f) * (DF_TAU / 8.0f);
    v->mix.gl = q15(g * 1.41421356f * df_cos(a));
    v->mix.gr = q15(g * 1.41421356f * df_sin(a));
}

/* A free voice, or the quietest if it is quieter than level. */
static Voice *take(float level)
{
    Voice *quiet;
    int i;

    quiet = 0;
    for (i = 0; i < SOUND_VOICES; i++) {
        if (!g_voice[i].mix.on) {
            return &g_voice[i];
        }
        if (!quiet || g_voice[i].level < quiet->level) {
            quiet = &g_voice[i];
        }
    }
    if (quiet && quiet->level <= level) {
        return quiet;
    }
    return 0;
}

static int start(Voice *v, const SoundClip *clip, int flags)
{
    int slot;

    slot = (int)(v - g_voice);
    v->clip = clip;
    v->mix.data = clip->data;
    v->mix.frames = clip->frames;
    v->mix.pos = 0;
    v->mix.frac = 0;
    v->mix.step = g_mix_rate > 0 ? (df_u32)(((df_u32)clip->rate << 16) / (df_u32)g_mix_rate) : MIX_ONE;
    v->mix.loop = (flags & SOUND_LOOP) ? 1 : 0;
    v->mix.on = 1;
    g_serial = (g_serial + 1) & 0x3ffffff;
    if (g_serial == 0) {
        g_serial = 1;
    }
    v->handle = (g_serial << 5) | slot;
    aim(v);
    return v->handle;
}

int sound_play(const SoundClip *clip, float volume, float pan, int flags)
{
    Voice probe;
    Voice *v;

    if (!g_on || !clip || !clip->data || clip->frames == 0) {
        return 0;
    }
    probe.positional = 0;
    probe.pan = pan;
    probe.volume = volume;
    aim(&probe);
    v = take(probe.level);
    if (!v) {
        return 0;
    }
    v->positional = 0;
    v->pan = pan;
    v->volume = volume;
    return start(v, clip, flags);
}

int sound_play_at(const SoundClip *clip, float x, float y, float z,
                  float inner, float reach, float volume, int flags)
{
    Voice probe;
    Voice *v;

    if (!g_on || !clip || !clip->data || clip->frames == 0) {
        return 0;
    }
    if (reach <= inner) {
        reach = inner + 1.0f;
    }
    probe.positional = 1;
    probe.x = x;
    probe.y = y;
    probe.z = z;
    probe.inner = inner;
    probe.reach = reach;
    probe.volume = volume;
    probe.pan = 0.0f;
    aim(&probe);
    if (probe.level <= 0.0f && !(flags & SOUND_LOOP)) {
        return 0;
    }
    v = take(probe.level);
    if (!v) {
        return 0;
    }
    v->positional = 1;
    v->x = x;
    v->y = y;
    v->z = z;
    v->inner = inner;
    v->reach = reach;
    v->volume = volume;
    v->pan = 0.0f;
    return start(v, clip, flags);
}

void sound_stop(int handle)
{
    Voice *v;

    v = voice_of(handle);
    if (v) {
        v->mix.on = 0;
        v->handle = 0;
    }
}

int sound_playing(int handle)
{
    return voice_of(handle) != 0;
}

void sound_set_gain(int handle, float volume)
{
    Voice *v;

    v = voice_of(handle);
    if (v) {
        v->volume = volume;
        aim(v);
    }
}

void sound_seek(int handle, df_u32 frame)
{
    Voice *v;

    v = voice_of(handle);
    if (v && v->mix.frames > 0) {
        v->mix.pos = frame % v->mix.frames;
        v->mix.frac = 0;
    }
}

void sound_stop_all(void)
{
    int i;

    for (i = 0; i < SOUND_VOICES; i++) {
        g_voice[i].mix.on = 0;
        g_voice[i].handle = 0;
    }
}

int sound_busy(void)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < SOUND_VOICES; i++) {
        n += g_voice[i].mix.on;
    }
    return n;
}

void sound_mix(df_i16 *dst, int frames)
{
    int i;

    for (i = 0; i < SOUND_VOICES; i++) {
        Voice *v;

        v = &g_voice[i];
        if (!v->mix.on) {
            continue;
        }
        mix_voice(&v->mix, dst, frames);
        if (!v->mix.on) {
            v->handle = 0;
        }
    }
}

void sound_update(void)
{
    int i;

    for (i = 0; i < SOUND_VOICES; i++) {
        if (g_voice[i].mix.on) {
            aim(&g_voice[i]);
        }
    }
    audio_update(sound_mix);
}
