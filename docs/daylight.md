# Daylight

Outdoor places have a clock and a calendar, a sky, light by the hour (the sun by day, the moon by night, street lamps and lit windows after dark), and the season's pictures. Read this before editing `src/engine/scene/clock.c`, `daylight.c`, `src/engine/gfx/sky.c`, the sky pass, the window pass or the outdoor lighting in `draw.c`, `glide_sky` or `glide_glow` in `glide.c`, `MIP_GLASS` in `mip.c` or `texture_smooth`, `src/game/dagger/skyfile.c`, `swaps.c`, or the lamps and windows in `world.c`. Dungeon lights are `docs/lighting.md`.

## The clock

`clock.c` keeps minutes since midnight, 0 to 1440, and starts at noon. `draw_frame` advances it by each frame's real time, from the performance counter, times `timescale` game seconds a real second: 12 by default, Daggerfall's. A frame that spans the console panel is not counted, and neither is one over a second (a load, a stall), so the clock stops while the console is open and the sky never jumps. `timescale 0` stops it. `time 21:30` sets it.

Indoors nothing reads the clock.

## The calendar and the seasons

The clock also keeps the date, Tamriel's as Daggerfall keeps it (Daggerfall Unity's `DaggerfallDateTime`): 12 months of 30 days, a year of 360, a week of 7 days that starts again on Sundas each month. It starts on the 1st of Midyear, 3E 405, Daggerfall Unity's start. Running past midnight turns the day; setting the hour (`time`) never moves the date. `clock_day` is the date as one number, days since the 1st of Morning Star 3E 0.

The season is the month's, on whole months, as Daggerfall Unity has it: winter Evening Star, Morning Star and Sun's Dawn; spring First Seed to Second Seed; summer Midyear to Last Seed; fall Hearthfire to Sun's Dusk. `CLOCK_FALL`..`CLOCK_WINTER` are in Daggerfall's order, the one the sky files and the weather odds use.

A town is drawn in the season of the day it loads (`World.season`):

- **Its pictures.** A model names its pictures in whatever climate it was drawn in, so before this a town showed a mix: Daggerfall, a woodland city, drew walls from the desert, mountain and woodland sets side by side. `swaps.c` is Daggerfall Unity's `ClimateSwaps`, rule for rule: the set is the archive's last two digits, the place's climate family replaces the hundreds, and in winter an outside set that has a snowy one takes it (+1: Castle 9 to 10, Farm 26 to 27, the ground 302 to 303). Inside sets (a house's) take the family and never snow; doors take the family; the desert never snows; the sets Daggerfall Unity knows to have no snowy picture keep theirs; a swapped archive without the face's record keeps the original. Daggerfall now draws 216 pictures where it drew 235, all woodland, and in winter 318 walls, 370 roofs and 303 ground. Dungeons keep their own table (`docs/dungeons.md`).
- **Its sky** (`skyfile_open`) and **its weather odds** (`forecast_set`), by the season, as before by the climate.

A season that turns while a town is shown (a `date`, a `season`, or a fast `timescale`) does not change its pictures under it: they are on the card. LEVELFALL builds the town again where the camera stands, under its "Loading..." frame (`docs/viewers.md`). Nature (the trees of a block's ground, archives 500 and up) has winter pictures too; the ground's scenery is not drawn yet.

## Windows

After dark a town's windows are lit, as in Daggerfall: the glass of a window picture is palette index 255 (`MIP_GLASS`), black in `ART_PAL.COL`, as index 0 is. Which pictures are windows is Daggerfall Unity's `IsExteriorWindow` (`swap_window`): record 3 of the general sets, the gables 75 to 77, records 0 and 2 of the city specials 79 and 80, record 0 of 82 and 83, and a few named ones. 98 of the 308 such pictures have glass; the gables in many climates have none, and do not light. A house's inside is not lit (Daggerfall Unity leaves them).

197 other pictures use index 255 too, as black, so changing that palette entry would light them as well. Instead `draw_windows` (`draw.c`) draws, after the walls and the lamps' decals, every visible face whose picture is a window again, through a palette that is black but for 255 (`glide_glow`), added onto the frame, with the depth test passing on equal so the face lands on itself. Black adds nothing; the glass gets Daggerfall Unity's night window colour, 255 182 56 at 0.8, times how lit the lamps are, so the windows come on with the street lamps at dusk. An added pass is not fogged, so each model's glass fades by hand from the fog's start to its end. A mesh with no window picture is skipped by its slot keys. One palette download on and one off a frame, only while the lamps are lit.

Filtering would lose the glass: a smaller level, or a wall stretched smoothly to a power of two, maps each colour back to the nearest palette entry, and black comes back as 0. So the mips carry how much of each texel is glass in the alpha byte an 8-bit picture does not use, and a texel at least half glass is 255 (`mip.c`); `texture_smooth` keeps 255 where at least half the bilinear weight was. The nearest search never answers 255, so no other picture gains glass.

## Axes

+x is east, +y up, +z north. Yaw 0 looks north and grows to the east. The sun and the sky pictures are placed by these, so a change to the world's axes moves the sunrise.

## The sky

### The files

`SKY00.DAT` to `SKY31.DAT`, each 7758080 bytes:

| Offset | Size | What |
|---|---|---|
| 0 | 32 × 776 | Palettes, one per frame: 8 bytes of header, then 256 entries of 8-bit RGB. Entry 0 is not a key colour (frame 20 of `SKY18` has it dark blue). |
| 24832 | 32 × 16384 | Colour maps. Not read. |
| 549120 | 64 × 112640 | Pictures, 512 × 220, one byte a pixel, row 0 the top. Pictures 0..31 are record 0, frames 0..31; 32..63 are record 1. |

Record 1 is the half from north through east to south, record 0 the half from south through west to north. Frame 0 is dawn and frame 31 noon. The afternoon runs the same frames back with the halves swapped, so the glow that was east in the morning is west at dusk. That is how Daggerfall Unity reads them (`SkyFile.cs`, `DaggerfallSky.cs`), and the files agree: frame 8 of `SKY18` has its sun at column 250 of 1024, due east.

The file is the climate's sky base plus the season, from Daggerfall Unity's climate table:

| Climate | Base |
|---|---|
| Mountain (226) | 0 |
| Desert (224, 225) | 8 |
| Mountain woods, woodlands, haunted woodlands (230..232), and anything else | 16 |
| Ocean, rainforest, swamp, subtropical (223, 227..229) | 24 |

Season: fall 0, spring 1, summer 2, winter 3. The woodland four show it: `SKY19` is an overcast grey, `SKY18` a blue sky with cloud. Places load summer (`SEASON_SUMMER` in `world.c`); nothing changes the season yet.

From 06:00 to 18:00 the frame follows the hour on a curve: fast from six to eight, slow around noon, fast from four to six. Hours after dawn against frame: 0→0, 1→12, 2→20, 4→28, 6→32, 8→36, 10→44, 11→52, 12→64. Frames 32..63 are frames 31..0 with the halves swapped.

From 18:00 to 06:00 the sky is one of `NITE00I0.IMG` to `NITE03I0.IMG`: 512 × 219, no header, palette `NIGHTSKY.COL`, the same picture on both halves. Sky files 0..7 take night 3, 8..15 night 1, 16..23 night 2, 24..31 night 0. Stars are dropped on as Daggerfall Unity does: 4 in 1000 pixels of clear sky (index 17..31) become index 16, 32, 74, 105, 112 or 120, scattered again each night. The right column is a copy of its neighbour, because the picture's two ends do not meet.

`skyfile.c` reads a picture only when asked, opening the file for it: a frame is 225 KB read from two places in the file.

### How it is drawn

The game hands the engine a `Sky` (`engine/assets/sky.h`): a `pick` from the minute and the weather's sky (`docs/weather.md`) to a key, and a `read` from a key to the whole circle, 1024 × 220 pixels with column 0 north and the columns running clockwise seen from above, plus its palette. `Level.sky` is 0 indoors.

`gfx_prepare` sends the circle as four P_8 squares of 256 (`sky.c`): 64 KB each, 256 KB on TMU0, and the dome (below), 32 KB, before the light falloff and the level's pictures, so the texture budget sees the room left. Rows 220..255 repeat the horizon row. Each frame `sky_update` asks for the key; when it changes, the new picture is read, the dome is made again, and both are sent to the same places (`glide_replace`). That is one frame change every 11 game minutes or so, about once a minute at timescale 12. With no room on the card, the place draws on the viewer's background, as before.

The sky is drawn first, after the clear (`draw_sky`): 32 flat panels round the camera, each a column of 32 sky pixels, and a fan from their tops to the zenith. A row is as tall as it is wide, so the 220 rows reach 1.35 radii up the panels, about 53 degrees, which is how Daggerfall Unity scales them on the glass. The panels run from just below the horizon to the picture's top row, and clamped addressing holds the horizon row below it.

The pictures stop at 53 degrees; Daggerfall Unity never shows above them. The fan above is the dome, a 128 × 128 RGB 565 picture made from the sky each time it changes (`make_cap`). Its first row is the picture's top two rows, averaged 8 columns at a time. Each row up blurs that ring a texel wider on each side, and fades to the top rows' mean colour by row 48, which is the zenith. The first version stretched the top row up to the zenith; on the Voodoo that drew the top row's clouds as rays meeting overhead. They sit 16 units out, less with a short far clip, because depth is neither read nor written: the world draws over them. The panels go through the same clip as every polygon, so nothing reaches the card past the SST-1's ±2048.

`glide_sky(1, palette)` downloads the sky's palette, turns off depth and fog, and clamps; `glide_sky(0, 0)` downloads the level's palette again (kept from `glide_palette`) and puts the state back. Two palette downloads a frame.

The clear and the fog take the mean colour of the picture's bottom six rows (`glide_sky_color`), so a far house fades into the bottom of the sky, white at noon and black at night. The viewer's own background comes back for a place without a sky.

## The light

`daylight.c` has one row of colours for each moment of the day: ambient RGB, key RGB, and how lit the lamps are. Between rows they blend in a straight line, and the last row runs into the first across midnight.

| Time | Ambient | Key | Lamps |
|---|---|---|---|
| 00:00..04:30 | 0.18 0.20 0.32 | moon 0.20 0.23 0.34 | 1 |
| 05:30 | 0.22 0.21 0.32 | 0.04 | 1 |
| 06:00 | 0.30 0.26 0.34 | sun 0.22 0.10 0.04 | 0.7 |
| 06:45 | 0.38 0.33 0.40 | 0.95 0.55 0.30 | 0 |
| 08:00 | 0.45 0.45 0.53 | 0.95 0.80 0.62 | 0 |
| 10:00..14:00 | 0.50..0.52 0.53..0.55 0.62..0.64 | 0.95 0.92 0.84 | 0 |
| 16:00 | 0.45 0.44 0.52 | 0.95 0.78 0.58 | 0 |
| 17:15 | 0.38 0.32 0.38 | 1.00 0.55 0.28 | 0 |
| 18:00 | 0.30 0.25 0.34 | 0.25 0.10 0.05 | 0.6 |
| 18:30 | 0.22 0.21 0.32 | 0.04 | 1 |
| 19:30.. | 0.18 0.20 0.32 | moon 0.20 0.23 0.34 | 1 |

The key is the sun from 05:45 to 18:15 and the moon the rest of the night. At both switches its colour is almost nothing, so nothing jumps. The sun runs on a circle from east at six to west at eighteen through the south, tipped 30 degrees from the zenith: 60 degrees up at noon. The moon runs the same way from eighteen to six, tipped 40.

A face takes `ambient + key × max(0, n · k)`, clipped at 1, as one colour in Glide's constant colour (`glide_tint`): texture times that colour, no colour per vertex, so a lit town sends the card the same triangles as an unlit one. Per frame `draw_frame` fills a table of 33 colours by angle (`daylight_table`). Per model it turns the key into the mesh's frame (`m` transposed), so a face is one dot product with its stored normal, a `df_iround`, and a look-up. `glide_tint` sends nothing when the colour is the one already set, as on a run of ground faces. ARCH3D face normals are made unit length at load (`push_face` in `arch3d.c`) for this; the rest of the engine reads only their sign or normalises them again. A lone triangle is drawn from both sides and lit from either. A model with no faces takes the half-lit colour.

The sun at its brightest leaves a texture as painted: a face toward the sun is its texture, a face away about half of it in a cool blue. The low sun is orange on a dull ambient, so an hour after dawn the east fronts of a street burn while the street is in shade. No shadows: one building does not shade another, and the Pentium has no time for it.

Flats turn to the camera, so they share one normal, the view's horizontal backwards. They take `ambient + key × f`, where f is half plus half the sun's side of the flat. A flat with the sun behind it still gets half, and a high sun lights a standing flat less (`daylight_sprite`).

## The lamps

RMB blocks have no light records. Outdoors `world.c` makes one `Light` for each `TEXTURE.210` flat, the lamp posts, braziers and fires. It sits a quarter of the flat's height above its centre, where the flame is, with a radius of 160: at `lightrange 3` it reaches 480. In Daggerfall's 155 flats, 133 are lamp posts (image 29), 20 image 28, and 2 image 20. `Scene.outdoor` marks the place.

They use the same decals as a dungeon's lights (`docs/lighting.md`), cut at load, with three differences:

- They are drawn only while `daylight` says the lamps are lit: off by day, fading in from 17:15 to 18:30 and out from 05:30 to 06:45. `light_set_lamps` takes the level each frame.
- The decal pass is texture times the constant colour: the lamps' level times an oil flame's colour, 255 200 120, redder than the torch colour in the falloff pictures. A pool reads warm against the blue night, not white.
- It is drawn twice (`LAMP_PASSES`). One pass at most doubles a pixel, and a moonlit street starts at about a third of its texture; two take the ground under a lamp to most of its colour. The corners are transformed, clipped and projected once, and the polygon is sent twice (`g_repeat` in `draw.c`). Three passes were tried first: a little brighter, but on the Voodoo a near pool's third layer of blended fill cost more than it showed.
- A lamp fades out between 2048 and 3072 units from the camera (`LAMP_FADE_NEAR`, `LAMP_FADE_FAR`), through its tint, and is not drawn past that. In Daggerfall at far 8192, 9 of 16 lamps on the glass were past 2500, half the polygons of the lamp pass, for pools a few pixels deep in the fog.

Open ground is one flat face per patch of tiles, two triangles a tile, so a lamp's square on it took about 32 decals. When the triangles of a flat face cover the whole of the square inside the face's outline, `merge_face` in `light.c` cuts that rectangle as one decal instead; a face with a hole there (a tile left out), or one that is not flat, is still cut triangle by triangle. Daggerfall went from 9857 decals to 5880, and the far view on the Voodoo from 518 decals a frame to 219. Dungeon faces are fans, so the dungeons are unchanged.

A lamp whose flat is a moving flame (a brazier, a fire) flickers; a lantern on its post burns steady (`docs/lighting.md`).

Sprites near a lamp take the sum of the lamps on them, times the passes, on top of their daylight colour (`light_sprite_over`). A flat that gives light is drawn white.

## Switches

| Console | Default | Effect |
|---|---|---|
| `time` | 12:00 | Prints the clock, or sets it: `time 21`, `time 6:45`. |
| `timescale` | 12 | Game seconds a real second, 0..2000. 0 stops the clock. |
| `date` | 1 6 405 | Prints the date (`Sundas, 1st of Midyear, 3E 405, summer`), or sets it: `date 15 12 405` (day, month, year, from 1). |
| `season` | summer | Prints the date, or goes to the first day of a season this year: `season winter`. |
| `sky` | on | Off draws outdoor places on the viewer's background, as before. |
| `daylight` | on | Off draws outdoor places full bright, as before, with no lamps. |
| `lights` | on | Off also turns off the lamps. |

`stats` outdoors adds the hour, the switches, and the lamps' level.

## Cost

Per frame, outdoors:

- The sky: 64 polygons, most cut away by the clip, and its fill: about half the screen looking level, a little over 3 ms at the Voodoo's 45 Mpixels a second. Two palette downloads.
- The light: a 33-entry table, three dot products a model, one dot product a face, a constant colour write when the colour changes.
- At night, the decals of the lamps within 3072 units, sent twice; and the window faces again, added, with two palette downloads.

Measured on the Voodoo 1 and the 233 before the lamp changes, LEVELFALL at 640 × 480: the Daggerfall square at far 4096 ran 35 fps by day and 29 at night; a street at far 8192, 16 fps by day and 9 at night (16 with `lights off`). Sentinel at far 8192, 19 fps by day and 16 at night. After the merge, the fade and two passes, the same street at night runs 13 fps with the lamps and 14 without: the lamps went from about 50 ms to about 5.

On a key change: 225 KB read from the sky file, the dome made again, and 288 KB sent to the card, a few tens of milliseconds on the 233.

At load: the lamps' decals. Daggerfall 155 lamps, 5880 decals, 774 KB, cut in 6 ms on the host; Wayrest 110 lamps, 4705 decals, 615 KB; Sentinel 7 lamps, 235 decals.

Memory: `sky.c` keeps the circle (220 KB), one tile (64 KB) and the dome (32 KB) static, and `skyfile.c` one half picture (112 KB); none is on a heap, because the viewers' temp heap dies with each place. `sky.c` also keeps its own copy of the place's `Sky`, not a pointer into the place. 288 KB of TMU0.

## Not done

- The rain pictures: Daggerfall Unity draws the ground wet (302 + 2) while it rains; a town loads dry.
- Holidays, and Daggerfall's season turning part way through a month's last days.
- The moons are not drawn, only their light. `MOON00I0.CFA` and `MOON01I0.CFA` are probably Masser and Secunda.
- No shadows from the sun, no flicker of the lamps.
- The night picture comes in at 18:00 and goes at 06:00 in one step, as in Daggerfall Unity.
