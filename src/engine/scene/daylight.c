#include "engine/scene/daylight.h"

/*
 * See daylight.h and docs/daylight.md. Runs once a frame (a dozen
 * interpolations and two sines), not per face.
 */

/*
 * The colours through the day, one row per moment: minute, ambient
 * r g b, key r g b, lamps. Between rows they blend in a straight line;
 * the last row runs into the first across midnight. The key is the sun
 * from SUN_UP to SUN_DOWN and the moon the rest of the night; at both
 * switches its colour is almost nothing, so nothing jumps.
 *
 * Noon is a white sun on a blue-grey ambient: a wall that faces the sun
 * is its texture as painted, one that faces away about half of it. The
 * low sun is orange on a duller ambient, so an hour after dawn the east
 * fronts of a street burn while the street is still in shade. Night is
 * a cold blue: the moon gives the faces it reaches a little more.
 */
#define ROWS 15
static const float g_rows[ROWS][8] = {
    {    0.0f, 0.18f, 0.20f, 0.32f, 0.20f, 0.23f, 0.34f, 1.0f },
    {  270.0f, 0.18f, 0.20f, 0.32f, 0.20f, 0.23f, 0.34f, 1.0f },
    {  330.0f, 0.22f, 0.21f, 0.32f, 0.04f, 0.04f, 0.06f, 1.0f },
    {  360.0f, 0.30f, 0.26f, 0.34f, 0.22f, 0.10f, 0.04f, 0.7f },
    {  405.0f, 0.38f, 0.33f, 0.40f, 0.95f, 0.55f, 0.30f, 0.0f },
    {  480.0f, 0.45f, 0.45f, 0.53f, 0.95f, 0.80f, 0.62f, 0.0f },
    {  600.0f, 0.50f, 0.53f, 0.62f, 0.95f, 0.92f, 0.84f, 0.0f },
    {  720.0f, 0.52f, 0.55f, 0.64f, 0.96f, 0.94f, 0.88f, 0.0f },
    {  840.0f, 0.50f, 0.53f, 0.62f, 0.95f, 0.92f, 0.84f, 0.0f },
    {  960.0f, 0.45f, 0.44f, 0.52f, 0.95f, 0.78f, 0.58f, 0.0f },
    { 1035.0f, 0.38f, 0.32f, 0.38f, 1.00f, 0.55f, 0.28f, 0.0f },
    { 1080.0f, 0.30f, 0.25f, 0.34f, 0.25f, 0.10f, 0.05f, 0.6f },
    { 1110.0f, 0.22f, 0.21f, 0.32f, 0.04f, 0.04f, 0.06f, 1.0f },
    { 1170.0f, 0.18f, 0.20f, 0.32f, 0.20f, 0.23f, 0.34f, 1.0f },
    { 1440.0f, 0.18f, 0.20f, 0.32f, 0.20f, 0.23f, 0.34f, 1.0f },
};

/* The sun is the key from a quarter hour before six to a quarter after eighteen. */
#define SUN_UP 345.0f
#define SUN_DOWN 1095.0f
/*
 * Each body runs along a circle from east to west through the south,
 * tipped from the zenith by this many degrees' sine and cosine: the sun
 * 30, a noon height of 60; the moon 40, lower.
 */
#define SUN_TILT_S 0.5f
#define SUN_TILT_C 0.866f
#define MOON_TILT_S 0.643f
#define MOON_TILT_C 0.766f
#define PI 3.14159265f

static int g_on = 1;

void daylight_set_on(int on)
{
    g_on = on ? 1 : 0;
}

int daylight_on(void)
{
    return g_on;
}

static float clamp01(float v)
{
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

void daylight_at(float minute, Daylight *out)
{
    const float *a;
    const float *b;
    float t;
    float ang;
    int i;

    while (minute >= 1440.0f) {
        minute -= 1440.0f;
    }
    while (minute < 0.0f) {
        minute += 1440.0f;
    }
    i = 0;
    while (i < ROWS - 2 && g_rows[i + 1][0] <= minute) {
        i++;
    }
    a = g_rows[i];
    b = g_rows[i + 1];
    t = (minute - a[0]) / (b[0] - a[0]);
    t = clamp01(t);
    out->ar = a[1] + (b[1] - a[1]) * t;
    out->ag = a[2] + (b[2] - a[2]) * t;
    out->ab = a[3] + (b[3] - a[3]) * t;
    out->kr = a[4] + (b[4] - a[4]) * t;
    out->kg = a[5] + (b[5] - a[5]) * t;
    out->kb = a[6] + (b[6] - a[6]) * t;
    out->lamps = a[7] + (b[7] - a[7]) * t;
    out->sun = minute >= SUN_UP && minute < SUN_DOWN;
    if (out->sun) {
        /* 0 at six in the east, half a turn at eighteen in the west. */
        ang = (minute - 360.0f) * (PI / 720.0f);
        out->kx = df_cos(ang);
        out->ky = df_sin(ang) * SUN_TILT_C;
        out->kz = -df_sin(ang) * SUN_TILT_S;
    } else {
        /* The moon rises at eighteen and sets at six. */
        ang = minute - 1080.0f;
        if (ang < 0.0f) {
            ang += 1440.0f;
        }
        ang *= PI / 720.0f;
        out->kx = df_cos(ang);
        out->ky = df_sin(ang) * MOON_TILT_C;
        out->kz = -df_sin(ang) * MOON_TILT_S;
    }
}

/*
 * At most 254: pure white is the tint that switches glide.c's colour
 * combine back to the plain texture, and a face that turned it on and
 * off would cost two combine changes. 254 of 255 is not visible.
 */
static df_u32 pack(float r, float g, float b)
{
    int ir;
    int ig;
    int ib;

    ir = df_iround(clamp01(r) * 254.0f);
    ig = df_iround(clamp01(g) * 254.0f);
    ib = df_iround(clamp01(b) * 254.0f);
    return ((df_u32)ir << 16) | ((df_u32)ig << 8) | (df_u32)ib;
}

void daylight_table(const Daylight *dl, df_u32 *table)
{
    int i;

    for (i = 0; i <= DAYLIGHT_STEPS; i++) {
        float f;

        f = (float)i * (1.0f / (float)DAYLIGHT_STEPS);
        table[i] = pack(dl->ar + dl->kr * f, dl->ag + dl->kg * f, dl->ab + dl->kb * f);
    }
}

df_u32 daylight_sprite(const Daylight *dl, float fx, float fz)
{
    float d;
    float f;

    /* The flat faces back along the view: its normal is -(fx, 0, fz). */
    d = -(fx * dl->kx + fz * dl->kz);
    f = clamp01(0.5f + 0.5f * d);
    /* A high sun lights a standing flat less than a low one in front of it. */
    f *= 0.55f + 0.45f * clamp01(1.0f - dl->ky);
    return pack(dl->ar + dl->kr * f, dl->ag + dl->kg * f, dl->ab + dl->kb * f);
}
