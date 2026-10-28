# Weather

The engine has weather: a kind of weather at a time, rolled from odds every few game hours, blending into the next, and what it does to the light, the fog, the sky, and the air. The game sets the odds for its place and season; the engine does the rest. Read this before editing `src/engine/scene/weather.c`, the weather code in `draw.c` (`fog_apply`, `draw_precip`, `flash_tick`, `weather_light`), `scene_set_fog_far`, or `src/game/dagger/tables/forecast.c`. The hour's light and the sky are `docs/daylight.md`.

## The kinds

Seven, in Daggerfall's order from fair to foul: sunny, cloudy, overcast, fog, rain, thunder, snow. Each has a look (`WeatherLook`), the values it brings once it has fully arrived:

| Kind | Cloud | Fog solid at | Rain | Snow | Storm | Sky |
|---|---|---|---|---|---|---|
| sunny | 0 | none | 0 | 0 | 0 | clear |
| cloudy | 0.25 | none | 0 | 0 | 0 | clear |
| overcast | 0.70 | 4000 | 0 | 0 | 0 | overcast |
| fog | 0.80 | 600 | 0 | 0 | 0 | overcast |
| rain | 0.80 | 2400 | 1 | 0 | 0 | rain |
| thunder | 0.90 | 1800 | 1 | 0 | 1 | storm |
| snow | 0.70 | 1500 | 0 | 1 | 0 | snow |

A game can change any of them (`weather_set_look`). Each also has a view for the simulation (below):

| Kind | View (far clip) | Haze (fog start, of the fog's end) |
|---|---|---|
| sunny | 4096 | 0.70 |
| cloudy | 3500 | 0.60 |
| overcast | 2500 | 0.45 |
| fog | 700 | 0.10 |
| rain | 2000 | 0.25 |
| thunder | 1500 | 0.20 |
| snow | 1200 | 0.15 |

The fields are `view` and `haze`, not `far` and `near`: `windows.h` defines both as empty macros.

## How it changes

`draw_frame` advances the weather by the game minutes the clock moved (`clock_advance` returns them), so `timescale` speeds it up and `timescale 0` stops it, and indoors it goes on unseen.

Every 4 to 10 game hours, at random, the odds are rolled. A new kind never cuts in: every value blends from where it is to the new kind's over 30 game minutes, eased in and out. The fog blends as visibility (one over the distance), so it clears gradually rather than all at the end. The sky picture changes at half way. A change that begins during another starts from the values of that moment, so nothing jumps.

`weather_force` holds a kind, blending to it, until `weather_force(-1)` gives the weather back to the odds. `weather_set` arrives at once, for a new place or a test. The rolls come from a fixed sequence (`weather_seed`).

When the game sets new odds and the current kind has no chance under them, a roll lands at once: arriving in a desert does not bring the woodlands' snow along.

## The odds

The engine defaults to Daggerfall's woodlands in summer. `src/game/dagger/tables/forecast.c` has the whole table from the Daggerfall Chronicles (p. 47), as Daggerfall Unity carries it (`WeatherTable.json`): six climate groups, four seasons each. `world.c` sets the place's odds when an outdoor place loads (`forecast_set`), summer until there is a calendar. Some of it:

| Group (climates) | Season | Sunny | Cloudy | Overcast | Fog | Rain | Thunder | Snow |
|---|---|---|---|---|---|---|---|---|
| Woodlands (230..232, default) | summer | 60 | 20 | 5 | 0 | 10 | 5 | 0 |
| Woodlands | winter | 25 | 15 | 20 | 5 | 10 | 0 | 25 |
| Desert (224, 225) | summer | 85 | 15 | 0 | 0 | 0 | 0 | 0 |
| Swamp (223, 228) | fall | 15 | 15 | 15 | 20 | 20 | 15 | 0 |
| Mountains (226, 230) | winter | 18 | 20 | 25 | 2 | 0 | 0 | 35 |

Mountain woods (230) count as mountains, the rainforest (227) as jungle, subtropical (229) on its own.

## What it does

**The light.** Cloud dims the key light, the sun or the moon, by up to 60%, and pulls the ambient toward its own grey and 12% down (`weather_light`). It is the same per-face colour table as the hour's light, so it costs nothing per face.

**The fog.** The weather's fog distance, when it is nearer than the far clip's own (`df_fog_far`), is where the fog goes solid. `fog_apply` sends the card a new fog table, starting the fog at 280 units or a quarter of the distance, whichever is nearer, and tells the cull (`scene_set_fog_far`) and the clip. So a fog does not just hide the town: past 600 units nothing is culled in, transformed or drawn, and a rainy street is cheaper than a sunny one. The table moves only when the distance moves by more than 0.2%; during a change that is a 64-entry table a frame. Indoors, and with the weather off, the fog is the far clip's own, as before.

The fog's colour is the sky's horizon colour exactly (`docs/daylight.md`), so a fogged house meets the bottom of the sky with no seam. A first version greyed it by the cloud cover; the rain sky's own horizon stayed blue, and a blue band showed under the fogged houses.

**The simulated view** (`weathersim on`). Without it the weather can only bring the fog nearer than the far setting's own, and the far setting keeps the clip. With it, outdoors, the weather sets the far clip and the whole fog:

- The far clip is the kind's view, blended like the rest (as visibility), shortened by 40% at night (by the lamps' level, so it closes in at dusk), never under 256, and never past the far setting: `far` becomes the ceiling, what the machine can afford on the clearest day.
- The fog goes solid on the card's fog table entry at or before that clip (`df_fog_below`), or nearer when the kind's own fog is: everything the clip cuts off is already fully fogged, so nothing pops. The table's entries are up to 18% apart, so the fog can go solid a little before the clip; rain's 2000 goes solid at 1638.
- The fog starts at the kind's haze times its end: a clear day is clear to 70% of the view and hazy over the rest, a storm fogged from near the camera.
- The cull follows the clip and the fog as always, so the weather sets the frame's cost: a fog culls at 600, a clear day is the most expensive.

A clear day with no fog at all was tried: a far roof popped at the clip and the town looked pasted on the sky. The light haze is what replaces it.

`weathersim off` gives the clip and the fog back to the far setting. LEVELFALL keeps it off to inspect a place at any distance.

**The sky.** The weather picks the picture: `Sky.pick` takes the weather's sky (`WEATHER_SKY_CLEAR`, `OVERCAST`, `RAIN`, `STORM`, `SNOW`) and the game maps it to its files. Daggerfall's climate groups each have four after their seasons (`skyfile_weather_index`): +4 a dark storm sky, drawn for rain and thunder; +5 a grey one, for overcast and fog; +6 a pale snow sky; +7 a dark snow storm, not used yet. Bad weather has no stars: its frame 0, the darkest, runs all night, as in Daggerfall Unity.

As the fog closes in, the sky fades into it: drawn with constant alpha `(fog − 500) / 2100` over the clear, which is the fog colour, and not drawn at all below a few percent. In a fog that saves the sky's half screen of fill.

**Rain and snow.** `draw_precip`, after the sprites. 320 drops at most, each at a fixed place in a lattice of boxes round the camera (768 × 512 × 768 units for rain, 640 × 400 × 640 for snow), so walking through them is parallax, not a pattern stuck to the glass. They fall in real seconds, so a stopped clock still rains. The weather's rain or snow, 0..1, says how many are drawn.

- A raindrop is a streak from where it is to where it was 1/40 s before, falling 1100 units a second with a little wind: one line on the card, clipped to the six view planes before it is projected, so no coordinate reaches the card past the SST-1's ±2048.
- A snowflake is a square 1.5 to 6 pixels across, falling 70 units a second and swaying. Only a flake whose centre is on the glass is drawn, so every corner stays near it.
- Both are flat colour, half transparent, with the depth test, so they pass behind walls, and with fog. Their brightness follows the ambient and the key, so night rain is dim.

A screen of scrolling rain pictures is the usual cheap trick, but on a Voodoo 1 every layer of it is a full screen of blended fill, about 7 ms. 320 lines are a few thousand pixels and one setup each.

**Lightning.** With storm above 0.3, a stroke every 4 to 18 real seconds, more often the stormier. A stroke lasts a quarter of a second and flickers: bright, dim, bright. It lifts the ambient (so the per-face table) and whitens the fog colour, and the sky fades into that fog for the stroke, so the whole sky lights up. A first version lit only the world, and far roofs became pale shapes on a dark sky.

**Sound.** The place's soundscape hears the weather (`ambience_frame`, `docs/sound.md`): the rain loop follows the rain, a wind loop the snow, crickets the dark of a fair night, and each lightning stroke raises `g_stroke` for one frame, so a thunder clip follows it 0.3 to 2.5 seconds later. The weather's kind and the night choose the song.

## Switches

| Console | Effect |
|---|---|
| `weather` | The kind now, where it is coming from and how far, the next roll, and the values. |
| `weather rain` | Hold a kind (any of the seven), blending to it. |
| `weather auto` | Give it back to the odds. |
| `weather off` / `on` | Off draws clear weather, as before. The weather still runs. |
| `weathersim on` / `off` | The weather sets the far clip and the fog outdoors, with `far` as the ceiling. Default off. `weather` and `stats` print the view the last frame drew. |

## Cost

Per frame: a few floats blended; when the fog moves, a 64-entry table. Rain or snow: up to 320 particles, each a transform or two and one setup, about 1 to 2 ms on the 233 at full rain; a few thousand pixels of fill. Lightning: nothing. Thick weather saves more than it costs: the cull follows the fog in, and a fog skips the sky's fill. None of it is measured on the Voodoo yet.

## Not done

- No calendar or seasons in the game yet: every place is summer.
- The weather is one for the whole world, not one per climate group as in the original.
- No snow on the ground or the roofs, no wet ground: the climate's `TEXTURE` files +1 (snow) and +2 (wet) are there, but swapping them means sending the place's pictures to the card again.
- The sky changes picture in one step at half way; the rest blends.
