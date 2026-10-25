#ifndef DF_HMI_H
#define DF_HMI_H

#include "engine/assets/song.h"

/*
 * Daggerfall's music: MIDI.BSA, a named BSA of 131 HMI songs, Human
 * Machine Interfaces' "HMI-MIDISONG061595" format from its SOS library.
 * Read docs/sound.md.
 *
 * The layout is ZMusic's (GZDoom's player, midisource_hmi.cpp):
 *   0xD4 uint16  ticks a quarter note / 4 (the tempo is 4,000,000 us a
 *                quarter, so this is ticks a second: 120 in every song)
 *   0xE4 uint16  tracks
 *   0xE8 uint32  offset of the track offsets
 * and each track, "HMI-MIDITRACK":
 *   0x57 uint32  offset of its events, from the track's start
 *   0x99 uint16  x8, the devices the track is for, 0-ended
 * The events are MIDI with running status, except that a note-on is
 * followed by a variable-length duration and has no note-off of its own,
 * and 0xFE begins an HMI event of its own (loop marks), skipped.
 *
 * Every Daggerfall track names General MIDI (0xA000), FM (0xA002) and
 * the Gravis (0xA00A); fm picks the FM tracks. The songs whose names end
 * in FM are the FM arrangements.
 */

/* Device numbers in a track's list. */
#define HMI_DEV_GM 0xA000
#define HMI_DEV_MPU401 0xA001
#define HMI_DEV_OPL2 0xA002
#define HMI_DEV_SBAWE32 0xA008
#define HMI_DEV_OPL3 0xA009

/*
 * The song in bytes into out, its events in music_scratch(). fm 1 plays
 * the FM tracks, 0 the General MIDI ones. 0 when it is not HMI or has
 * more than MUSIC_EVENTS events.
 */
int hmi_parse(const df_u8 *bytes, df_u32 size, int fm, Song *out);
/* A song of MIDI.BSA by name (upper case, "DUNGEON5.HMI"). */
int hmi_load(const char *name, int fm, Song *out);
/* Songs in MIDI.BSA, and the name of one, for the console and the tests. */
int hmi_count(void);
const char *hmi_name(int index);

#endif
