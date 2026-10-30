#ifndef DF_SNDFILE_H
#define DF_SNDFILE_H

#include "engine/assets/clip.h"

/*
 * DAGGER.SND: Daggerfall's sound effects, a numbered BSA of 459 records,
 * each unsigned 8-bit mono at 11025 Hz with no header. Read
 * docs/sound.md.
 *
 * A clip is named by its position in the file, as Daggerfall Unity's
 * SoundClips numbers it (the BSA's ids are not in order, and id 220 is
 * there twice, so by-id order would shift them). RDB action records
 * name a sound by its id instead: sndfile_by_id.
 *
 * Clips load on first ask and stay for the process (mem_keep), flipped
 * to signed once: a voice may play one after its place is gone.
 */

/* Positions used by the soundscape and the emitters. */
#define SND_CRICKETS 6
#define SND_DRIP_SHORT 63
#define SND_DRIP_LONG 64
#define SND_WIND_MOAN 65
#define SND_WIND_MOAN_DEEP 66
#define SND_DOOR_OPEN_FAR 67
#define SND_GRIND 68
#define SND_STRUMMING 69
#define SND_WIND_BLOW 70
#define SND_WIND_BLOW_A 71
#define SND_WIND_BLOW_B 72
#define SND_MONSTER_ROAR 73
#define SND_GOLD 74
#define SND_BIRD_FAR 75
#define SND_DOOR_CLOSE_FAR 76
#define SND_HORSE 99
#define SND_DOG 100
#define SND_CAT 101
#define SND_PIG 102
#define SND_COW 103
#define SND_LIGHTNING_SHORT 348
#define SND_LIGHTNING_THUNDER 349
#define SND_THUNDER_ROLL 350
#define SND_RAIN 389
#define SND_BURNING 420
#define SND_BIRD_1 437
#define SND_BIRD_2 438

/* Positions combat plays (DFU's SoundClips: swings, hits, parries, a body falling). */
#define SND_BODY_FALL 15
#define SND_SWING_LOW 105
#define SND_SWING_HIGH 106
#define SND_HIT_FIRST 108
#define SND_HITS 5
#define SND_SWING_MEDIUM 347
#define SND_PARRY_FIRST 428
#define SND_PARRIES 9

/* Records in the file, 0 when it does not open. */
int sndfile_count(void);
/* The clip at a position, 0 when there is none (or it is empty). */
const SoundClip *sndfile_clip(int index);
/* The position of a record id, -1 when none. */
int sndfile_by_id(df_u32 id);
/* Bytes of clips loaded so far. */
df_u32 sndfile_bytes(void);

#endif
