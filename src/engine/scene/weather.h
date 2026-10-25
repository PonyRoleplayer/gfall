#ifndef ENG_WEATHER_H
#define ENG_WEATHER_H

#include "engine/core/df.h"

/*
 * The weather. Read docs/weather.md before editing.
 *
 * One current kind of weather, rerolled from a table of odds every few
 * game hours, and a set of values it drives: cloud cover, how far the
 * fog lets you see, rain, snow, storm. A change never cuts: the values
 * blend from where they are to the new kind's over WEATHER_BLEND game
 * minutes. The renderer reads the values (weather_state); the game sets
 * the odds for its climate and season (weather_set_odds), or forces a
 * kind. Plain C with no Glide, so hostcheck runs it.
 *
 * The odds and the kinds' values default to Daggerfall's: the odds of
 * its woodlands in summer.
 */

enum {
    WEATHER_SUNNY,
    WEATHER_CLOUDY,
    WEATHER_OVERCAST,
    WEATHER_FOG,
    WEATHER_RAIN,
    WEATHER_THUNDER,
    WEATHER_SNOW,
    WEATHER_KINDS
};

/*
 * Which sky picture a moment wants, for Sky.pick: the game maps each to
 * its own pictures.
 */
enum {
    WEATHER_SKY_CLEAR,
    WEATHER_SKY_OVERCAST,
    WEATHER_SKY_RAIN,
    WEATHER_SKY_STORM,
    WEATHER_SKY_SNOW
};

/* What a kind of weather looks like once it has fully arrived. */
typedef struct WeatherLook {
    /* 0..1: dims the sun, flattens and greys the ambient and the fog. */
    float cloud;
    /* World units at which the fog is solid; WEATHER_CLEAR for none. */
    float fog;
    /* 0..1 of the most drops and flakes. */
    float rain;
    float snow;
    /* 0..1: lightning. */
    float storm;
    /* WEATHER_SKY_*. */
    int sky;
    /*
     * Under the simulated view (weather_set_sim): view is how far the
     * far clip reaches, haze where the fog starts as a fraction of where
     * it goes solid. A clear day is a long view with a thin haze over
     * its last stretch; a storm a short one, fogged from near the camera.
     * Not "far" and "near": windows.h defines both as empty macros.
     */
    float view;
    float haze;
} WeatherLook;

/* A fog distance that never limits the view. */
#define WEATHER_CLEAR 100000.0f
/* Game minutes a change takes. */
#define WEATHER_BLEND 30.0f
/* A kind holds this many game minutes, at random between the two. */
#define WEATHER_HOLD_MIN 240.0f
#define WEATHER_HOLD_MAX 600.0f

typedef struct Weather {
    /* The kind arrived or arriving, and the one it came from. */
    int kind;
    int from;
    /* 0 at the start of a change, 1 when it is done. */
    float blend;
    /* The values now, blended. */
    float cloud;
    float fog;
    float rain;
    float snow;
    float storm;
    /* The sky picture: the old kind's until half way, then the new. */
    int sky;
    /* The simulated view's far clip and fog start, blended. */
    float view;
    float haze;
    /* Game minutes until the next roll of the odds. */
    float next;
} Weather;

/* Advance by game minutes: blends, and rolls the odds when it is time. */
void weather_advance(float minutes);
void weather_state(Weather *out);

/*
 * Percent chance of each kind at the next roll, WEATHER_KINDS entries.
 * They need not add to 100; they are scaled. All zero means sunny.
 * When the current kind has no chance under the new odds (and is not
 * forced), a roll lands at once.
 */
void weather_set_odds(const df_u8 *odds);
const char *weather_name(int kind);
/* WEATHER_KINDS when the name is not a kind. */
int weather_by_name(const char *name);

/*
 * Change to kind now, blending over WEATHER_BLEND, and hold it: no
 * rolls until weather_force(-1) gives the odds back.
 */
void weather_force(int kind);
int weather_forced(void);
/* Arrive at kind at once, no blend: a new place, a test. */
void weather_set(int kind);

void weather_set_look(int kind, const WeatherLook *look);
void weather_look(int kind, WeatherLook *out);
/* The rolls are a fixed sequence from this seed. */
void weather_seed(df_u32 seed);

/* Switch. Off is clear weather drawn as before. Default on. */
void weather_set_on(int on);
int weather_on(void);

/*
 * The simulated view: the weather sets the far clip and the fog's start
 * and end, the far setting is only their ceiling. Off (default) keeps
 * the far setting's own clip and fog, which the weather can only bring
 * nearer. See docs/weather.md.
 */
void weather_set_sim(int on);
int weather_sim(void);

#endif
