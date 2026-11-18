# Movies (VID)

`ANIM0000..0015.VID` and `DAG2.VID` in ARENA2 are the game's movies: 320x200 (DAG2 256x200), 8 bits under their own palette, a delta/RLE codec, 8-bit unsigned mono sound at about 11 kHz, 60 Hz frame ticks. 17 files, about 100 MB. The princes' `*.FLC` (Autodesk FLIC, 15 files) are separate and not read yet.

## The format (`src/game/dagger/formats/vid.h`)
FALL.EXE's player is `xn_vid_play` (decomp `src/engine/vid.c`, `xvid.h`). A 15-byte header (`VID\0`, version, frames, width, height, per-frame delay, flags), then chunks by type byte: 0 raw, 1 delta, 4 delta from a start row, 3 RLE (each with a word delay in ticks), 2 palette (768 bytes, six bits a channel), 7Ch audio header (delay, rate byte: rate = 1000000 / (256 - byte)), 7Dh audio data (word length, bytes). A frame is followed at once by its audio. `vid_next` decodes up to the next frame; sound goes to a callback.

Every movie decodes to its last frame in hostcheck (`check_vid`), with the original's frame counts; frames of 0, 12 and DAG2 are written to `build/vid*.ppm`. Cost on the 233: a read window of 128 KB and a 64000-byte frame, kept for the process; decoding is copies and fills.

## Where they play (decomp)
- `ANIM0012`: death. `ANIM0002` and `ANIM0004`: the lycanthropy and vampirism dreams on the first rest. `ANIM0013`: the ghost's farewell (quest op 50 of `S0000015`). Quest op 50 plays `anim%04d.vid`. The intro and Bethesda logo (`intro.c`). The princes' FLC in the summoning.

## Where they are read from
GFALL assumes a full ("huge") installation: the whole of ARENA2, movies included, on the hard disk. `fs_open_media` (engine/platform/fs.c) opens a movie from the game root first, and only when the install lacks it tries the CD-ROM drives (`X:\\DAGGER\\ARENA2`, then `X:\\ARENA2`: the original disc), by `GetDriveTypeA`. No disc and no file: the movie is skipped. Host tests give the "disc" through `GFALL_MEDIA`. The movies are the only files that fall back to the CD.

## Playing them (next)
A frame goes to the card as two P_8 tiles (`pic_update` on a 256x200 and a 64x200 picture) under the movie's palette, drawn scaled to the window; the sound is streamed to the mixer as it is read (resampled from the movie's rate), and the frame clock follows the sound when there is some. Escape skips. No Glide change is needed. Video through a REALmagic card is not planned: the files are not MPEG, and a Voodoo 1's pass-through owns the monitor in a game.
