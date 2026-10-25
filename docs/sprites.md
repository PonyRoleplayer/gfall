# Sprites

Flats: the camera-facing pictures of torches, candles, furniture, plants, people and animals. Read this before editing how `world.c` places and paints flats, `make_sheet`, `draw_flat` in `draw.c`, or the frame code in `texture.c`.

## Where a flat stands

A flat is drawn as a quad round its centre (`draw_flat`). The file gives one point for it, and that point is not the same in a dungeon and a town:

- **RDB (dungeons): the centre.** Daggerfall Unity places dungeon flats at their point as it is (`RDBLayout.AddFlat`).
- **RMB (towns): the base.** Daggerfall Unity lifts town flats by half their height (`AlignBillboardToBase` in `RMBLayout`), and `paint` does the same, outdoors only.

The data agrees. Of the hold's flats over a floor, 58 have their point at mid-height and 1 at the base; the castle's flats over a floor have it at mid-height (an 87-tall one 43 up). Daggerfall city's have it at the base, 436 of 447 (4 units up: the props' own -4). The rest hang on walls or stand on tables.

`paint` used to lift every flat by half its height, so every dungeon sprite floated half its height in the air: the castle's king and queen beside their thrones, every torch above its bracket. hostcheck now checks that a third of the hold's flats over a floor, and most of a farm's, stand with their bottom on it (`flats_on_floor`).

## Animation

Daggerfall's animated flats are TEXTURE records of several frames (`texture_frame_count`, `texture_load_frame`): flames, waterfalls, townsfolk, animals. In `TEXTURE.210`, the light flats, records 0–6, 16–21 and 24–26 are flames of 4 or 5 frames; the lanterns, chandeliers and lamp posts are one. The record's frames are either row-RLE, each frame more entries in the row table, or a table of one offset per frame, each to a frame of its own size; the loader reads both.

A record's frames go into one **sheet** (`make_sheet` in `world.c`): side by side, 2 texels apart, in the smallest power-of-two square whose grid holds them all. A narrow flame's frames fit the square its first frame was padded into anyway: the hold's torch, 16 × 94, has its four frames in the same 128 square. The picture says where they are (`Image.frames`, `frame_cols`, `frame_dx`, `frame_dy`; `width` and `height` are one frame's), and the engine shows frame k by moving the texture coordinates to its cell. Nothing is uploaded or bound per frame.

A slot per frame was tried first. Each frame is padded into its own square, so it cost the hold 36 more pictures and 440 KB, and on a 2 MB card the budget would have paid for the torches with blurrier walls. The sheets cost 190 KB, the few wide flames whose frames need the next square up.

The game says how fast (`Flat.rate`): flames 12 frames a second, everything else 5, as Daggerfall Unity runs them. Each flat starts at its own frame (`Flat.phase`, from where it stands), so the candles in a room do not move in step. The frame comes from real seconds (`g_anim_time` in `draw.c`), so a flame moves with the world clock stopped. A record whose frames do not all load, or do not fit a 256 square, stays still.

The sheet's mip levels bring the frames' cells within a texel of each other two levels down, so a flame far off may take a trace of the next frame at its edge.

## Flicker

The light a flame casts wavers: see `docs/lighting.md`. Which lights flicker comes from these pictures, not the light records.
