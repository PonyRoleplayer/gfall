# Sound

Music on the card's synthesizer, effects mixed by the CPU, and what each place sounds like. Read this before editing anything in `src/engine/sound/`, `src/engine/platform/winmm.c`, `src/game/dagger/sndfile.c`, `hmi.c` or `sounds.c`, the sound call in `draw.c`, or the sound commands in `console.c`.

The load is split: the AWE64's EMU8000 plays the music by itself, and the CPU only mixes the effects, at 11025 Hz, a few voices at a time, with MMX.

## How Daggerfall's sound works

From Daggerfall Unity's code (`SongManager`, `AmbientEffectsPlayer`, `RDBLayout`, `GameObjectHelper`, `DaggerfallAudioSource`, `SndFile`) and ZMusic's HMI reader (`midisource_hmi.cpp`, GZDoom's player). The data agrees with both.

**DAGGER.SND** is a numbered BSA of 459 records. Each record is raw unsigned 8-bit mono at 11025 Hz, with no header. A clip is named by its position in the file, as Daggerfall Unity's `SoundClips` numbers it. The record ids are not in order, and id 220 is there twice. So by-id order would shift them, and `sndfile.c` reads the directory itself rather than through `bsa.c`, which sorts by id. RDB action records name a sound by id: `sndfile_by_id`.

**MIDI.BSA** is a named BSA of 131 songs in HMI's `HMI-MIDISONG061595` format (Human Machine Interfaces' SOS library):

| Offset | Field |
|---|---|
| 0xD4 | uint16, ticks a quarter / 4. The tempo is 4,000,000 µs a quarter, so this is ticks a second: 120 in every song. |
| 0xE4 | uint16, tracks |
| 0xE8 | uint32, offset of the track offsets |
| track + 0x57 | uint32, offset of its events from the track's start |
| track + 0x99 | uint16 × 8, the devices the track is for, 0-ended |

The events are MIDI with running status, except:
- A note-on is followed by a variable-length duration and has no note-off of its own; `hmi.c` makes one.
- 0xFE begins an HMI event of its own (loop and branch marks), skipped at ZMusic's lengths.

Every Daggerfall track names General MIDI (0xA000), OPL2 (0xA002) and Gravis (0xA00A). The songs whose names end in FM are the FM arrangements.

What the songs need: at most 17 notes at once (in TAVERN, SNEAKING and the long dungeon songs), at most 11,077 events (DUNGEON.HMI), from 30 seconds to 5½ minutes long.

**When things play**, as Daggerfall Unity has it:
- **Songs.** A dungeon song inside. Outdoors: by day a playlist by the weather (sunny, cloudy, overcast and fog, rain and storm, snow), at night the night songs. A song loops until the playlist changes.
- **Weather.** Rain and storm loop the rain (389). A storm plays a thunder clip (348–350) after a burst of lightning flashes. A clear night loops crickets (6). A fair day calls a bird (437, 438) somewhere 10–20 m away every 4–35 s.
- **Dungeons.** Every 4–35 s, one of fourteen noises (63–76: drips, moans, wind, a far door, a grind, strumming, a roar, coins, a bird) somewhere 10–20 m away. Not in a castle's peaceful blocks: not done here, see the end.
- **Torches.** The flaming torch flats, `TEXTURE.210` records 0, 1, 6 and 16–20, loop burning (420), linear to 5 m at 0.7. Lanterns, chandeliers and lamp posts are quiet.
- **Animals.** `TEXTURE.201` flats call now and then within 768 units: a 101 in 32768 chance each sixteenth of a second, about once in 20 s. Horse 0–1, cow 3–4, pig 5–6, cat 7–8, dog 9–10.
- **Doors and other actions.** An RDB action plays its record's sound id when it runs. Nothing moves yet: not done.

## The layers

| Layer | File | Job |
|---|---|---|
| Data | `engine/assets/clip.h`, `song.h`, `soundscape.h`; `Scene.emitters` | A clip, a song, a place's soundscape, sounds from points. The game fills them. |
| Game | `game/dagger/sndfile.c`, `hmi.c`, `sounds.c`; `world.c`; `bind.c` | Daggerfall's files into those, its playlists and lists, an emitter per sounding flat. The `song` and `sound` loaders. |
| When | `engine/sound/ambience.c` | Once a frame: the song, the weather's loops, thunder, the odd noise, the emitters. |
| Voices | `engine/sound/sound.c` | 16 voices, flat or from a point, gains once a frame, taking the quietest. |
| Mix | `engine/sound/mixer.c` | Clips into one stereo stream, C or MMX. |
| Out | `engine/sound/audio.c` | That stream onto the card. |
| Music | `engine/sound/music.c` | A song into MIDI stream buffers, the 32-voice limit, the loop. |
| Windows | `engine/platform/winmm.c` | WINMM.DLL at runtime. DSOUND.DLL is loaded in `audio.c`. |

The programs still import only KERNEL32 and USER32. WINMM.DLL and DSOUND.DLL load at runtime as glide2x.dll does, and with neither, everything still runs, silent.

`draw_frame` calls `ambience_frame` after the hour's light and the lightning are known. So GFALL and LEVELFALL both sound, and neither knows. With the console open it calls `ambience_idle`, which only aims and refills, so the sound runs on under the console. GFALL and LEVELFALL open the card and the synthesizer once at startup (`sound_startup`, `music_startup`). LEVELFALL stops the song and the voices when it goes back to its list.

## Music: the card plays it

`music_play` turns the whole song into Windows MIDI stream events once (`music_render`): three words an event, `midiStreamOut`'s own format. It splits them into buffers of at most 32 KB (under `midiStreamOut`'s 64 KB limit) and queues all of them. The driver times and sends every note itself. The CPU's share while it plays is the driver's timer, nothing of ours.

`music_update`, once a frame, gives each finished buffer back to the stream behind the last. That is the loop: the buffers finish in order, so the first goes back in behind the last. A song is always at least two buffers, so one is queued while the other comes back. A stall shorter than what is queued, a place loading, does not stop the music.

How a buffer is known to be done matters on Windows 98. The first version opened the stream with no callback and polled each header's `MHDR_DONE`: the song played once, then never again. The headers of a 32-bit program are thunked to the 16-bit MMSYSTEM, and with `CALLBACK_NULL` the done flag was never seen to come back, so no buffer went back in. Now the stream has a callback (`on_done`, `CALLBACK_FUNCTION`) that does nothing but count MOM_DONE: a MIDI callback may call almost no system function, not `midiStreamOut`. It is the count's only writer, so it adds without a call. `music_update` queues that many buffers again, in order. Each start marks its buffers with a generation in `dwUser`, so the buffers a stop hands back are not counted. After queuing, `midiStreamRestart` wakes a stream that ran dry during a long stall; on one that is playing it does nothing. A buffer that does not go back in logs a warning, once, with its error.

The song is kept (`MUSIC_EVENTS`, 32768 events) so it can be made again: music turned back on, a new voice limit, or a volume change on a device with no volume of its own.

**The voice limit.** An AWE64 has 32 voices in hardware. Past them, its driver plays notes on the CPU. `music_render` keeps at most `music_voices` notes (default 32, `musvoices`) sounding: a note that would be one more ends the oldest first, and the oldest's own note-off is then dropped. The same key again on its channel ends the first. This is all decided when the events are made, so it costs nothing while playing. hostcheck renders all 131 songs: none needs more than 17, so none is cut. A preset layered from two samples takes two voices on the EMU8000, so a song near the limit can still pass 32; lower `musvoices` if `sndinfo` and the card disagree.

**The device.** `music_startup` scores every MIDI output:
1. The card's own synthesizer first, 4 (+2 with "AWE" in its name: "SB AWE64 MIDI Synth").
2. FM, 2.
3. A MIDI port, 1: a socket that may have nothing plugged in.

A synthesizer in software (`MOD_SWSYNTH`, or "wave synth", "sw synth", "software" in its name) scores 0 and is never chosen by itself: it is what the voice limit keeps the CPU out of. The MIDI mapper is never used, since it plays whatever the control panel says. `musdev` lists them and picks one. On FM (`music_kind`), the game reads the FM tracks and plays the FM playlists.

**Volume.** A device with its own volume (`MIDICAPS_VOLUME`) is turned down with `midiOutSetVolume`, at once. Without one, every controller 7 is scaled, each channel starts at General MIDI's 100 scaled, and the song starts again. The tempo is the stream's first event; each pass starts clean, since the song's last note-off and a no-op at its length close every pass.

Stopping resets the device (`midiOutReset`), so no note hangs.

## Effects: the CPU mixes them

**Voices.** 16 (`SOUND_VOICES`). A voice plays a clip once or looping. It is either flat (a volume and a pan) or from a point, heard full within `inner`, linearly quieter to silent at `reach`. Pan comes from the listener's right (the camera's yaw). Gains are worked out once a frame (`aim`): constant power across the pan, raised by √2 so straight ahead is full on both sides, and a point within 16 units of the head is in the middle. With every voice busy, a new sound takes the quietest voice, if it would be louder. A handle carries a serial, so one kept after its voice was taken matches nothing.

**The mix.** Clips are signed 8-bit (the game flips Daggerfall's unsigned bytes once, at load: half the memory of 16-bit). A sample s is widened to s << 8 and scaled by each side's Q15 gain, (s << 8) · g >> 16, MMX's `pmulhw`. At full gain a voice is half of full scale, so two loud voices still fit, and each voice is added with saturation, `paddsw`. The MMX loop does four frames an iteration: `movd` four bytes, `punpcklbw` against zero, `punpcklwd`/`punpckhwd` to pair left and right, `pmulhw`, `paddsw`, two `movq`. It ends in `emms` and names the x87 stack as clobbered. The C loop computes exactly the same, and hostcheck holds them to it bit for bit over looping and stopping voices. A CPU without MMX (CPUID) takes the C loop; `sndmmx 0` forces it.

The card runs at 11025 Hz, the clips' own rate (`sndrate` changes it). A clip at the output's rate takes the MMX loop; any other steps through it in 16.16 fixed point, nearest sample, in C.

**Onto the card.** `audio_open` tries, cheapest first:
1. **DirectSound, primary buffer written by us** (`DSSCL_WRITEPRIMARY`). The mix goes straight into the buffer the card plays; DirectSound mixes nothing. It needs 16-bit stereo and at least 200 ms of buffer.
2. **One DirectSound buffer** at the primary's own format, so DirectSound's mixer only copies.
3. **WINMM wave blocks**, 16 of a fortieth of a second, for a card with no DirectSound driver.

DirectSound on a card with no driver of its own is emulated over the wave calls, with more latency. So `AUTO` skips an emulated DirectSound (`DSCAPS_EMULDRIVER`). The structures are DirectSound 3's (`DSBUFFERDESC1`), which every DSOUND.DLL takes. `snddrv` forces a way out.

There is no thread. Each frame `audio_update` mixes up to a lead ahead of the play cursor: a frame and a half (a running average) and 40 ms, at least 60 ms, at most half the ring. Behind that, all the way round to the play cursor, it writes silence. So a stall, a place loading, runs out into quiet, not a loop of the last moment. It times itself with the performance counter. When the time since the last refill is longer than what was mixed ahead, or the write position fell inside what the card is about to play, it starts again at the write cursor. A lost buffer is restored and cleared.

The latency of a new sound is the lead: about 150 ms at 13 frames a second.

## Each frame: `ambience_frame`

1. A new place (scene, soundscape, heap epoch or emitter count changed): every voice stops, the song is asked again.
2. **The song.** When the weather's kind or night (the street lamps' share of the hour, over a half) changes, the soundscape names a song. A different name is loaded through the game's `song` loader and played. Indoors the kind is -1.
3. **Beds.** Loops under everything, outdoors only, each fading at half its volume a second:
   - rain by the weather's rain;
   - wind by its snow, × 0.8;
   - the night's crickets by the night × how fair it is (1 − the larger of rain and snow), × 0.7.
4. **Thunder.** `flash_tick` in `draw.c` raises `g_stroke` on the frame a stroke begins. A thunder clip follows 0.3–2.5 s later (the stroke's distance), from a random side, at 0.6 + 0.4 × the storm. One at a time.
5. **The odd noise.** Every `wait_min`..`wait_max` seconds: outdoors on a fair day one of the day list, anywhere else one of the around list, from 400–800 units off in a random direction, heard full within 520.
6. **Emitters.** Each one within reach is heard. A loop is kept if it is among the six nearest (`AMBIENCE_LOOPS`); the rest stop. A loop starts where it would be by now (seconds × rate, plus its index × 7919 samples), so no two torches burn in step and one walked back to sounds as if it never stopped. A now-and-then emitter plays with a chance of secs / every each frame, once at a time.
7. `sound_update` aims the voices from the camera and refills the card; `music_update` gives finished buffers back.

## Memory

Kept for the process (`mem_keep`): the voices may play after their place's heap is gone.

| What | Size |
|---|---|
| Clips, loaded on first ask | at most 3 MB (`SND_BUDGET`); a dungeon's 15 are about 300 KB, a town's 8 about 350 KB |
| DAGGER.SND directory | 9 KB |
| MIDI.BSA directory and one song's bytes | 4 KB and 64 KB |
| Song buffers: the loader's, the sort's, the kept song | 3 × 256 KB |
| The stream | 442 KB |
| Wave blocks (wave path only) | 77 KB |

On a place's heap: its emitters, 2048 at most (`MAX_EMITTERS`), 32 bytes each.

## Switches

| Console | Effect |
|---|---|
| `sndinfo` | The way out, its rate, ring and lead, MMX or C, voices busy, emitters heard, the song and its device, events, the most notes at once and how many the limit cut. |
| `sound 0/1`, `sndvol 0..100` | Effects off and on (off stops every voice), and their volume (80). |
| `sndrate n` | The card's rate (11025); reopens it. |
| `snddrv 0..4` | 0 chooses, 1 primary, 2 one buffer, 3 wave, 4 none; reopens it. |
| `sndmmx 0/1` | The mixer's MMX loop, or the C loop. |
| `music 0/1`, `musvol 0..100` | Music off (the song is kept) and on, and its volume (80). |
| `musvoices 1..32` | The voice limit (32). |
| `musdev [n\|auto]` | The MIDI outputs, or play on one. |
| `song [name\|stop]` | The song, or play one of MIDI.BSA's by name (`song DUNGEON5.HMI`). |
| `play n` | Play DAGGER.SND clip n. |
| `sndview n` | Squares, words and rings for the emitters within n units of the camera (0..4096), the 64 nearest: see below. 0 (default) is off. Try 1024. |
| `sndlist [n]` | The place's emitters by kind ("53 fire"), then the n nearest (10, at most 64): number, word, position, distance, reach, loop or now and then, and whether it plays or is in reach. |

## Seeing the emitters

`sndview 1024` draws each emitter within 1024 units of the camera, the 64 nearest: a square at it, the game's word for it (`Emitter.label`: "fire", "horse", "cow", "pig", "cat", "dog"), and a ring as wide as its reach at its distance, where it stops being heard. A loop (a fire) is orange and a now-and-then sound (an animal) green, bright while it plays and dim while silent. So walking toward a torch shows it light up when it is among the six nearest loops, and an animal flash bright when it calls. It draws over walls, as `lightview` does, and keeps to the same limits: a square only when its centre is on the glass, a ring only when at most 480 pixels wide, so nothing reaches the SST-1's ±2048.

`sndlist` prints the same from the console, nearest first, with positions to `setpos` to. The hold has 53 fires; the Ashwing Farmstead 6 animals.

## Tests

hostcheck runs everything but the card:
- **Engine:** the mixers agree bit for bit; full scale is (s << 8) · g >> 16; the mix saturates; half rate holds each sample two frames. A sound on the right is louder on the right and moves left when the listener turns; one out of reach takes no voice; a loud one takes the quietest voice and a quiet one does not. The sort puts note-offs first at a tick; forty notes under 32 voices end the oldest eight; controller 7 is scaled; the stream opens with the tempo and ends at the song's length.
- **Dagger:** 459 clips, the rain 31346 signed samples; record id 385 at position 389. 131 songs that all read and render within 32 voices; D1.HMI is 224 s; D1FM's FM tracks read. Every playlist's songs exist. The hold has 53 sounding torches, a dungeon song, and fire beside a torch but none 5000 units off; `sndlist`'s view of them matches, and the playing torch is the one held. A farm in a thunderstorm is louder than on a sunny day.

It writes what it mixed, to listen to: `build/snd0.wav` beside a torch in the hold, `build/snd1.wav` the farm in a storm.

## On the card

Heard on the 233 with the AWE64 and a Voodoo 2 under Glide: the effects, the weather's loops and thunder, the torches and the animals, and the songs, which loop since the done callback. A good first version.

## Not done

- Doors and other RDB actions: nothing moves yet; the sound id is there for it.
- A castle's peaceful blocks still get the dungeon's noises; Daggerfall Unity skips them (a start marker with a magnitude marks the block).
- The water: Daggerfall Unity plays gentle water near a block's water level, and bubbles under it.
- Footsteps, the player's own sounds, enemies.
- A calendar: Daggerfall seeds the playlist with the day; the place's number stands in.
- Graveyards' howls and creepy birds.
