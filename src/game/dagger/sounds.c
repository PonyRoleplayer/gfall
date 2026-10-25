#include "game/dagger/sounds.h"
#include "game/dagger/sndfile.h"
#include "engine/scene/weather.h"
#include "engine/sound/music.h"

/*
 * See sounds.h. Distances are Daggerfall Unity's in metres times its 40
 * world units a metre.
 */

/* TEXTURE.210, the light flats; TEXTURE.201, the animals. */
#define LIGHT_FLATS 210
#define ANIMAL_FLATS 201

/* A burning torch: linear to 5 m, at 0.7 (RDBLayout.AddTorchAudioSource). */
#define TORCH_INNER 40.0f
#define TORCH_REACH 200.0f
#define TORCH_VOLUME 0.7f
/*
 * An animal: within 768 units, a 101 in 32768 chance each sixteenth of
 * a second (DaggerfallAudioSource's PlayRandomlyIfPlayerNear): about
 * once every 20 seconds.
 */
#define ANIMAL_INNER 40.0f
#define ANIMAL_REACH 768.0f
#define ANIMAL_EVERY 20.3f
/* AmbientEffectsPlayer: the next noise 4 to 35 seconds after the last. */
#define WAIT_MIN 4.0f
#define WAIT_MAX 35.0f

#define COUNT(a) (int)(sizeof(a) / sizeof((a)[0]))

typedef struct Playlist {
    int n;
    const char *const *names;
} Playlist;

/* SongManager's playlists: General MIDI, then the FM arrangements. */
static const char *const g_dungeon[] = {
    "DUNGEON.HMI", "DUNGEON5.HMI", "DUNGEON6.HMI", "DUNGEON7.HMI", "DUNGEON8.HMI",
    "DUNGEON9.HMI", "GDNGN10.HMI", "GDNGN11.HMI", "GDUNGN4.HMI", "GDUNGN9.HMI",
    "04.HMI", "05.HMI", "07.HMI", "15.HMI", "28.HMI"
};
static const char *const g_sunny[] = {
    "GDAY___D.HMI", "SWIMMING.HMI", "GSUNNY2.HMI", "SUNNYDAY.HMI", "02.HMI", "03.HMI", "22.HMI"
};
static const char *const g_cloudy[] = {
    "GDAY___D.HMI", "SWIMMING.HMI", "GSUNNY2.HMI", "SUNNYDAY.HMI", "02.HMI", "03.HMI",
    "22.HMI", "29.HMI", "12.HMI"
};
static const char *const g_overcast[] = {
    "29.HMI", "12.HMI", "13.HMI", "GPALAC.HMI", "OVERCAST.HMI"
};
static const char *const g_rain[] = { "OVERLONG.HMI", "RAINING.HMI", "08.HMI" };
static const char *const g_snow[] = { "20.HMI", "GSNOW__B.HMI", "OVERSNOW.HMI", "SNOWING.HMI" };
static const char *const g_night[] = {
    "10.HMI", "11.HMI", "GCURSE.HMI", "GEERIE.HMI", "GRUINS.HMI", "18.HMI", "21.HMI"
};

static const char *const g_dungeon_fm[] = {
    "FM_DNGN1.HMI", "FM_DNGN2.HMI", "FM_DNGN3.HMI", "FM_DNGN4.HMI", "FM_DNGN5.HMI",
    "FDNGN10.HMI", "FDNGN11.HMI", "FDUNGN4.HMI", "FDUNGN9.HMI", "04FM.HMI", "05FM.HMI",
    "07FM.HMI", "15FM.HMI"
};
static const char *const g_sunny_fm[] = {
    "FDAY___D.HMI", "FM_SWIM2.HMI", "FM_SUNNY.HMI", "02FM.HMI", "03FM.HMI", "22FM.HMI"
};
static const char *const g_cloudy_fm[] = {
    "FDAY___D.HMI", "FM_SWIM2.HMI", "FM_SUNNY.HMI", "02FM.HMI", "03FM.HMI", "22FM.HMI",
    "29FM.HMI", "12FM.HMI"
};
static const char *const g_overcast_fm[] = {
    "29FM.HMI", "12FM.HMI", "13FM.HMI", "FPALAC.HMI", "FMOVER_C.HMI"
};
static const char *const g_rain_fm[] = { "FMOVER_C.HMI", "FM_RAIN.HMI", "08FM.HMI" };
static const char *const g_snow_fm[] = { "20FM.HMI", "FSNOW__B.HMI", "FMOVER_S.HMI" };
static const char *const g_night_fm[] = {
    "11FM.HMI", "FCURSE.HMI", "FEERIE.HMI", "FRUINS.HMI", "18FM.HMI", "21FM.HMI"
};

static Playlist playlist(int kind, int weather, int night, int fm)
{
    Playlist p;

    if (kind == SCAPE_DUNGEON) {
        p.n = fm ? COUNT(g_dungeon_fm) : COUNT(g_dungeon);
        p.names = fm ? g_dungeon_fm : g_dungeon;
        return p;
    }
    if (night) {
        p.n = fm ? COUNT(g_night_fm) : COUNT(g_night);
        p.names = fm ? g_night_fm : g_night;
        return p;
    }
    switch (weather) {
    case WEATHER_CLOUDY:
        p.n = fm ? COUNT(g_cloudy_fm) : COUNT(g_cloudy);
        p.names = fm ? g_cloudy_fm : g_cloudy;
        break;
    case WEATHER_OVERCAST:
    case WEATHER_FOG:
        p.n = fm ? COUNT(g_overcast_fm) : COUNT(g_overcast);
        p.names = fm ? g_overcast_fm : g_overcast;
        break;
    case WEATHER_RAIN:
    case WEATHER_THUNDER:
        p.n = fm ? COUNT(g_rain_fm) : COUNT(g_rain);
        p.names = fm ? g_rain_fm : g_rain;
        break;
    case WEATHER_SNOW:
        p.n = fm ? COUNT(g_snow_fm) : COUNT(g_snow);
        p.names = fm ? g_snow_fm : g_snow;
        break;
    default:
        p.n = fm ? COUNT(g_sunny_fm) : COUNT(g_sunny);
        p.names = fm ? g_sunny_fm : g_sunny;
        break;
    }
    return p;
}

/*
 * One song of the playlist for the place and the weather. Daggerfall
 * seeds it with the day (a dungeon with its own record); there is no
 * calendar yet, so the place's number stands in, stirred by the
 * weather so a change of weather can change the tune.
 */
static const char *pick_song(const Soundscape *sc, int weather, int night)
{
    Playlist p;
    df_u32 h;

    p = playlist(sc->kind, weather, night, music_kind() == MUSIC_FM);
    if (p.n <= 0) {
        return 0;
    }
    h = (df_u32)sc->index * 2654435761u;
    if (sc->kind != SCAPE_DUNGEON) {
        h += (df_u32)(weather + 1) * 40503u + (df_u32)night * 7919u;
    }
    return p.names[(h >> 8) % (df_u32)p.n];
}

static void add(const SoundClip **list, int *n, int index)
{
    const SoundClip *c;

    c = sndfile_clip(index);
    if (c && *n < SOUNDSCAPE_PICKS) {
        list[(*n)++] = c;
    }
}

void sounds_scape(Soundscape *sc, int kind, int index)
{
    int i;

    sc->song = pick_song;
    sc->rain = 0;
    sc->wind = 0;
    sc->night = 0;
    sc->nthunder = 0;
    sc->nday = 0;
    sc->naround = 0;
    sc->wait_min = WAIT_MIN;
    sc->wait_max = WAIT_MAX;
    sc->index = index;
    sc->kind = kind;
    if (kind == SCAPE_DUNGEON) {
        /* The dungeon preset's one-shots, 63 to 76: drips, moans, a door, a roar, coins, a bird. */
        for (i = SND_DRIP_SHORT; i <= SND_DOOR_CLOSE_FAR; i++) {
            add(sc->around, &sc->naround, i);
        }
        return;
    }
    sc->rain = sndfile_clip(SND_RAIN);
    sc->night = sndfile_clip(SND_CRICKETS);
    /* Daggerfall Unity's snow is silent; a gust of wind, looped, is ours. */
    sc->wind = sndfile_clip(SND_WIND_BLOW);
    add(sc->thunder, &sc->nthunder, SND_LIGHTNING_SHORT);
    add(sc->thunder, &sc->nthunder, SND_LIGHTNING_THUNDER);
    add(sc->thunder, &sc->nthunder, SND_THUNDER_ROLL);
    add(sc->day, &sc->nday, SND_BIRD_1);
    add(sc->day, &sc->nday, SND_BIRD_2);
}

int sounds_flat(int file, int image, float x, float y, float z, Emitter *out)
{
    int clip;
    const char *label;

    out->x = x;
    out->y = y;
    out->z = z;
    if (file == LIGHT_FLATS) {
        /* The flaming ones (RDBLayout.IsTorchFlat); lanterns and lamp posts are quiet. */
        switch (image) {
        case 0:
        case 1:
        case 6:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
            break;
        default:
            return 0;
        }
        out->clip = sndfile_clip(SND_BURNING);
        out->label = "fire";
        out->inner = TORCH_INNER;
        out->reach = TORCH_REACH;
        out->volume = TORCH_VOLUME;
        out->every = 0.0f;
        return out->clip != 0;
    }
    if (file == ANIMAL_FLATS) {
        switch (image) {
        case 0:
        case 1:
            clip = SND_HORSE;
            label = "horse";
            break;
        case 3:
        case 4:
            clip = SND_COW;
            label = "cow";
            break;
        case 5:
        case 6:
            clip = SND_PIG;
            label = "pig";
            break;
        case 7:
        case 8:
            clip = SND_CAT;
            label = "cat";
            break;
        case 9:
        case 10:
            clip = SND_DOG;
            label = "dog";
            break;
        default:
            return 0;
        }
        out->clip = sndfile_clip(clip);
        out->label = label;
        out->inner = ANIMAL_INNER;
        out->reach = ANIMAL_REACH;
        out->volume = 1.0f;
        out->every = ANIMAL_EVERY;
        return out->clip != 0;
    }
    return 0;
}
