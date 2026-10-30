#include "game/dagger/play/sfx.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/tables/careers.h"
#include "engine/sound/sound.h"

/* See sfx.h. */

/*
 * DFU's SoundClips pain clips, three a race and sex, by RACE_* - 1:
 * Breton, Redguard, Nord, Dark Elf, High Elf, Wood Elf, Khajiit,
 * Argonian. The file's order is not the races' (an Argonian man's third
 * is 42, a Khajiit woman's third 424).
 */
static const short g_pain_male[8][3] = {
    { 390, 391, 392 }, { 393, 394, 395 }, { 396, 397, 398 }, { 399, 400, 401 },
    { 402, 403, 404 }, { 405, 406, 407 }, { 408, 409, 410 }, { 411, 412, 42 }
};
static const short g_pain_female[8][3] = {
    { 43, 44, 45 }, { 46, 47, 48 }, { 49, 50, 51 }, { 52, 53, 54 },
    { 55, 56, 57 }, { 58, 59, 60 }, { 61, 62, 424 }, { 425, 426, 427 }
};

void sfx_play(int clip)
{
    const SoundClip *c;

    c = clip >= 0 ? sndfile_clip(clip) : 0;
    if (c) {
        sound_play(c, 1.0f, 0.0f, 0);
    }
}

void sfx_at(int clip, float x, float y, float z)
{
    const SoundClip *c;

    c = clip >= 0 ? sndfile_clip(clip) : 0;
    if (c) {
        sound_play_at(c, x, y, z, SFX_INNER, SFX_REACH, 1.0f, 0);
    }
}

int sfx_pick(int first, int n)
{
    return first + combat_roll(n);
}

int sfx_pain(int race, int female)
{
    if (race < RACE_BRETON || race > RACE_BRETON + 7) {
        return -1;
    }
    return female ? g_pain_female[race - RACE_BRETON][combat_roll(3)] : g_pain_male[race - RACE_BRETON][combat_roll(3)];
}
