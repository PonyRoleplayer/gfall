#include "engine/sound/ambience.h"
#include "engine/sound/music.h"
#include "engine/sound/sound.h"
#include "engine/assets/loader.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See ambience.h and docs/sound.md. Plain C over sound.c and music.c, so
 * hostcheck runs it with no card.
 */

/* Emitters followed; past this many in one place, the rest are silent. */
#define TRACK 2048
/* Beds: rain, wind, night. */
#define BEDS 3
/* A bed fades this much of its volume a second. */
#define BED_FADE 0.5f
/* A noise "somewhere round": this far off, heard full within ROUND_INNER. */
#define ROUND_NEAR 400.0f
#define ROUND_FAR 800.0f
#define ROUND_INNER 520.0f
#define ROUND_REACH 4160.0f

static const Scene *g_scene;
static const Soundscape *g_scape;
static df_u32 g_epoch;
static int g_nemit;
static int g_handle[TRACK];
static int g_held[AMBIENCE_LOOPS];
static int g_nheld;
static int g_heard;
static int g_bed[BEDS];
static float g_bed_gain[BEDS];
static float g_wait = 8.0f;
static float g_thunder = -1.0f;
static float g_storm;
static int g_song_kind = -1;
static int g_song_night = -1;
static float g_time;
static df_u32 g_seed = 0x2545f491u;
static int g_warned;

static float rnd(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return (float)(g_seed >> 8) * (1.0f / 16777216.0f);
}

static int pick(int n)
{
    int k;

    k = df_ifloor(rnd() * (float)n);
    return k < 0 ? 0 : (k >= n ? n - 1 : k);
}

/* A new place: everything it was playing stops, the song is asked again. */
static void enter(const Level *level)
{
    int i;

    sound_stop_all();
    g_scene = &level->scene;
    g_scape = level->sound;
    g_epoch = mem_epoch();
    g_nemit = level->scene.emitters ? level->scene.nemitters : 0;
    if (g_nemit > TRACK) {
        if (!g_warned) {
            log_print(LOG_WARN, "ambience: %d emitters, only %d heard", g_nemit, TRACK);
            g_warned = 1;
        }
        g_nemit = TRACK;
    }
    for (i = 0; i < g_nemit; i++) {
        g_handle[i] = 0;
    }
    g_nheld = 0;
    for (i = 0; i < BEDS; i++) {
        g_bed[i] = 0;
        g_bed_gain[i] = 0.0f;
    }
    g_thunder = -1.0f;
    g_song_kind = -1;
    g_song_night = -1;
    if (g_scape && g_scape->wait_max > 0.0f) {
        g_wait = g_scape->wait_min + (g_scape->wait_max - g_scape->wait_min) * rnd();
    }
}

static void song(int kind, int night)
{
    const char *name;
    Song s;

    if (kind == g_song_kind && night == g_song_night) {
        return;
    }
    g_song_kind = kind;
    g_song_night = night;
    if (!g_scape || !g_scape->song) {
        return;
    }
    name = g_scape->song(g_scape, kind, night);
    if (!name) {
        music_stop();
        return;
    }
    if (df_eq(music_name(), name)) {
        return;
    }
    mem_zero(&s, sizeof(s));
    if (!loader_call("song", name, 0, 0, &s)) {
        log_print(LOG_WARN, "ambience: no song %s", name);
        return;
    }
    music_play(&s, name);
}

/* A bed toward its target, started and stopped at the ends of its fade. */
static void bed(int k, const SoundClip *clip, float target, float secs)
{
    float g;

    if (!clip) {
        target = 0.0f;
    }
    g = g_bed_gain[k];
    if (g < target) {
        g += BED_FADE * secs;
        if (g > target) {
            g = target;
        }
    } else if (g > target) {
        g -= BED_FADE * secs;
        if (g < target) {
            g = target;
        }
    }
    g_bed_gain[k] = g;
    if (g <= 0.001f) {
        sound_stop(g_bed[k]);
        g_bed[k] = 0;
        return;
    }
    if (!sound_playing(g_bed[k])) {
        g_bed[k] = sound_play(clip, g, 0.0f, SOUND_LOOP);
        sound_seek(g_bed[k], (df_u32)df_ifloor(rnd() * (float)clip->frames));
    } else {
        sound_set_gain(g_bed[k], g);
    }
}

static void around(const SoundClip *clip, const Camera *cam)
{
    float a;
    float r;

    a = rnd() * DF_TAU;
    r = ROUND_NEAR + (ROUND_FAR - ROUND_NEAR) * rnd();
    sound_play_at(clip, cam->x + r * df_cos(a), cam->y + (rnd() - 0.5f) * 80.0f,
                  cam->z + r * df_sin(a), ROUND_INNER, ROUND_REACH, 1.0f, 0);
}

static int is_held(int i)
{
    int k;

    for (k = 0; k < g_nheld; k++) {
        if (g_held[k] == i) {
            return 1;
        }
    }
    return 0;
}

static void emitters(const Camera *cam, float secs)
{
    const Emitter *em;
    int near_i[AMBIENCE_LOOPS];
    float near_d[AMBIENCE_LOOPS];
    int nn;
    int i;
    int k;

    em = g_scene->emitters;
    nn = 0;
    g_heard = 0;
    for (i = 0; i < g_nemit; i++) {
        const Emitter *e;
        float dx;
        float dy;
        float dz;
        float d2;

        e = &em[i];
        if (!e->clip) {
            continue;
        }
        dx = e->x - cam->x;
        dy = e->y - cam->y;
        dz = e->z - cam->z;
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= e->reach * e->reach) {
            continue;
        }
        g_heard++;
        if (e->every > 0.0f) {
            /* Now and then: about once every e->every seconds within reach. */
            if (!sound_playing(g_handle[i]) && rnd() < secs / e->every) {
                g_handle[i] = sound_play_at(e->clip, e->x, e->y, e->z, e->inner, e->reach, e->volume, 0);
            }
            continue;
        }
        /* A loop: kept among the nearest few. */
        if (nn < AMBIENCE_LOOPS || d2 < near_d[nn - 1]) {
            k = nn < AMBIENCE_LOOPS ? nn++ : nn - 1;
            while (k > 0 && near_d[k - 1] > d2) {
                near_d[k] = near_d[k - 1];
                near_i[k] = near_i[k - 1];
                k--;
            }
            near_d[k] = d2;
            near_i[k] = i;
        }
    }
    /* Loops that fell out of the nearest stop; the nearest start where they are in the loop. */
    for (k = 0; k < g_nheld; k++) {
        int j;
        int keep;

        keep = 0;
        for (j = 0; j < nn; j++) {
            keep |= near_i[j] == g_held[k];
        }
        if (!keep) {
            sound_stop(g_handle[g_held[k]]);
            g_handle[g_held[k]] = 0;
        }
    }
    for (k = 0; k < nn; k++) {
        const Emitter *e;

        i = near_i[k];
        e = &em[i];
        if (!sound_playing(g_handle[i]) || !is_held(i)) {
            g_handle[i] = sound_play_at(e->clip, e->x, e->y, e->z, e->inner, e->reach, e->volume, SOUND_LOOP);
            /* Each loop at its own place in the clip, moving on with time: no two torches in step. */
            sound_seek(g_handle[i], (df_u32)df_ifloor(g_time * (float)e->clip->rate) + (df_u32)i * 7919u);
        }
        g_held[k] = i;
    }
    g_nheld = nn;
}

void ambience_frame(const Level *level, const Camera *cam, const Weather *w,
                    float night, int stroke, float secs)
{
    const Soundscape *sc;
    int outdoor;

    if (!level || !cam || !w) {
        ambience_idle();
        return;
    }
    if (&level->scene != g_scene || level->sound != g_scape || mem_epoch() != g_epoch
        || (level->scene.emitters ? level->scene.nemitters : 0) != g_nemit) {
        enter(level);
    }
    g_time += secs;
    if (g_time > 3600.0f) {
        g_time -= 3600.0f;
    }
    sc = g_scape;
    outdoor = level->scene.outdoor;
    sound_listen(cam->x, cam->y, cam->z, cam->yaw);
    song(outdoor ? w->kind : -1, outdoor && night > 0.5f);
    if (sc) {
        float fair;

        fair = 1.0f - (w->rain > w->snow ? w->rain : w->snow);
        bed(0, sc->rain, outdoor ? w->rain : 0.0f, secs);
        bed(1, sc->wind, outdoor ? w->snow * 0.8f : 0.0f, secs);
        bed(2, sc->night, outdoor ? night * fair * 0.7f : 0.0f, secs);
        /* Thunder: one at a time, after its stroke by the stroke's distance. */
        if (stroke && outdoor && sc->nthunder > 0 && g_thunder < 0.0f) {
            g_thunder = 0.3f + 2.2f * rnd();
            g_storm = w->storm;
        }
        if (g_thunder >= 0.0f) {
            g_thunder -= secs;
            if (g_thunder < 0.0f) {
                sound_play(sc->thunder[pick(sc->nthunder)], 0.6f + 0.4f * g_storm,
                           (rnd() - 0.5f) * 1.6f, 0);
            }
        }
        if (sc->wait_max > 0.0f) {
            g_wait -= secs;
            if (g_wait <= 0.0f) {
                /* Outdoors the day list on a fair day; anywhere else the other. */
                if (outdoor && night < 0.5f && fair > 0.95f) {
                    if (sc->nday > 0) {
                        around(sc->day[pick(sc->nday)], cam);
                    }
                } else if (sc->naround > 0) {
                    around(sc->around[pick(sc->naround)], cam);
                }
                g_wait = sc->wait_min + (sc->wait_max - sc->wait_min) * rnd();
            }
        }
    }
    if (g_scene->emitters && g_nemit > 0) {
        emitters(cam, secs);
    }
    sound_update();
    music_update();
}

void ambience_idle(void)
{
    sound_update();
    music_update();
}

static int g_view;

int ambience_emitters(const Emitter **list)
{
    /* The place heard last may be gone with its heap (LEVELFALL's next place). */
    if (!g_scene || !g_scene->emitters || mem_epoch() != g_epoch) {
        if (list) {
            *list = 0;
        }
        return 0;
    }
    if (list) {
        *list = g_scene->emitters;
    }
    return g_nemit;
}

int ambience_emitter_playing(int index)
{
    if (index < 0 || index >= g_nemit) {
        return 0;
    }
    return sound_playing(g_handle[index]);
}

void ambience_set_view(int units)
{
    /* A range too small to hold anything is "on": `sndview 1`. */
    if (units > 0 && units < AMBIENCE_VIEW_SMALL) {
        units = AMBIENCE_VIEW_ON;
    }
    g_view = units < 0 ? 0 : (units > AMBIENCE_VIEW_MAX ? AMBIENCE_VIEW_MAX : units);
}

int ambience_view(void)
{
    return g_view;
}

void ambience_stats(int *heard, int *loops)
{
    if (heard) {
        *heard = g_heard;
    }
    if (loops) {
        *loops = g_nheld;
    }
}
