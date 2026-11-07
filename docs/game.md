# The game (design)

What the game layer is, how it is built on the engine and Daggerfall's places, and the order it comes in. This is a design. Nothing in it is built yet except where it says so. Read `docs/architecture.md` first for the layers. Read this before adding anything to `src/game/dagger/play/` or rewriting `apps/gfall/`.

## What there is today

The engine draws, hears and walks any place. `src/game/dagger/` loads Daggerfall's places whole (`world_load_dungeon`, `world_load_inside`) or streams the whole map round the camera (`wild_open`, `wild_update`). Its doors swing, its levers run their chains, its creatures stand where their markers put them, and its townspeople walk.

LEVELFALL goes through every passage too: into a house and out, down into a dungeon and up, out of the stream and back. Its landing sums are shared with the game in `places/travel.c`.

GFALL (`apps/gfall/main.c`) is the game: classic's title and start menu, character creation (an Argonian woman only when those pictures did not open), then Privateer's Hold streamed by block, on foot, with a weapon, armour, creatures that fight back, bodies to search and saves; out of the hold into the streamed world, its towns and houses (P8); and people to talk to, shops, banks, a court, and a street killing that draws the guard (P9). The console's `goto` goes to any town, for testing until P10's map.

## The pieces

Built, in `src/game/dagger/play/`:

```
game      one frame of play, Escape's pause, the use key
player    the player in the world: a body under rig_first, walk, run, jump, fall; the eye bobs when that option is on
character the sheet: attributes, skills, vitals, armour
rules     the sums: speeds, vitals, carrying (pure functions)
combat    the player's blows and a creature's, health kept in its keep row
foes      what a creature does: wake, chase, strike
inventory items, gold, what is worn and the armour it gives
loot      what a body or a heap of treasure holds
pile      what the player drops
weapon    the weapon in hand over the view
doll      the paper doll, drawn in memory
canvas    classic's 320 x 200 composed in memory: what the next three draw with
bag       the inventory screen and its clicks
sheet     the character sheet, and a level taken on it
create    character creation, screen by screen
rest      resting and loitering
screens   whichever of those is open, on the card
save      one saved game; saves: where they are and whose; files: their screen; ui: the widgets
kit       what a new character starts with
overlay   the HUD: compass, vitals, crosshair
menu      title/start/death and the settings shell
settings  native stone panels, dynamic text, settings layout and hit targets
control   classic cursor movement, view offsets, gesture routing and pointer rays
keys      the settings, bindings among them, in gfall.cfg (config.c, keys.c)
mouse     Cursor or View (gfall.cfg: mouse=)
viewopt   full screen, head bobbing, crouch toggle, texture details, the screen's mode (values kept in gfall.cfg)
screen    classic's 320 x 200 on the card
sfx       DAGGER.SND's clips for combat
commands  the game's console commands
```

`places/travel.c` (P8) carries every move between places; LEVELFALL shares its landing sums.

The player is Daggerfall's, so it lives here and not in the engine. So do items and the inventory: the engine is simulation and world. It gives a camera, a walking body (`body`), using things (`interact`) and entities (`entity`), and `play` builds Daggerfall's player out of them. Another game would build its own in its own folder. Each is a module of one `.c` and one `.h`, as in `docs/architecture.md`. What can be plain C is, and is host-tested in `tests/check_play.c`. What draws or reaches the keys (`PLAY_DRAW_SRC`) is checked by running GFALL; the menu controller also runs in hostcheck with deterministic input stand-ins.

`travel` is the one piece that is a place's business more than the game's: LEVELFALL needs it too. It goes in `src/game/dagger/places/travel.c`, not in `play/`, so that both GFALL and LEVELFALL use one copy. Its job is to load what a passage leads to and say where the player lands. It finds the other side from the location index (`dungeon.h`), not from a viewer's rows.

## A frame of play

GFALL's loop, once the game is running:

1. `win_pump`, then read the keys (`input_read`, plus the game keys; see the engine requests).
2. `game_frame(dt)` does nothing more while `gfx_paused()` (the console is open) or a screen is up. Otherwise:
   1. `body`: the walker moves the camera (`walker_step` runs inside `gfx_frame` today), with run, jump and swim on top.
   2. The use key and the attack. The use key goes through `interact_set_handler` and `travel` (passages) or `foes` (talk, later). The attack goes through `foes`.
   3. `foes` steps the creatures that are awake, under a time budget.
   4. The place's own steps: `world_creatures_step` (reading their pictures), or `wild_update` in the stream.
   5. The clock (the engine's `clock_advance`). Resting and waiting run it faster.
3. `gfx_frame(cam, level)`, with the HUD drawn by `gfx_set_extra(hud_draw)`.
4. `travel` changes place between frames, under `gfx_hold`, then calls `gfx_prepare` and `gfx_prime`, and fades in (`gfx_fade_in`).

**Built (P1, `play/game.c`, `apps/gfall/main.c`).** GFALL's loop is `win_pump`, then `input_read`, then `game_frame(cam, in, dt)`, then `gfx_frame`. dt is the performance counter's, at most a tenth of a second. `game_frame`, bracketed as the profile's `play` stage:
1. Before any place, the title or the start menu takes the frame (Menus, below). In play, Escape opens the options over the stilled world.
2. Unless paused (the game's, or the console's: `gfx_paused`): the use key, the player's step, its landing taken, the camera on its eye (`rig_first`), and the creatures read near it (2 ms).
3. The compass is turned.

A passage says where it will lead, and a person that talk comes later, through the game's use handler (`interact_set_handler`). The pictures' room on the card is 296 KB (`GAME_PICTURE_KB`: 256, and the saved games' snapshot), set before the first place.


## The wider world (P10, first slice built: the travel map)

Classic's travel map and its fast journeys, on Daggerfall Unity's numbers and classic's own pictures.

### What the map knows
`play/atlas.c`: a bit for each of the map's 15251 places. A new game knows what classic's map shows from the start: bit 30 of each place's MAPTABLE word, DFU's Discovered, which is 11199 of them (`worldmap_location_info`). A place is learnt when the player stands on its pixel of the world, or arrives there. The game's own block; a save's `DISC` chunk holds only the words that differ from a new game's.

### The map (`play/travelmap.c`)
- **The world:** `TRAV0I00.IMG`. The province under the pointer is named from `TRAV0I01.IMG`, whose index less 128 is the region; a click opens its map.
- **A province:** `FMAP0I##.IMG`, the panel at (0, 12). DFU's table gives each map's world pixel at the panel's top left. Regions 0, 1 and 16 are two or four screens, with arrows between them.
  - Betony's is drawn four times larger. Its corner, (82, 244), is fitted to the island: DFU's comes with hover offsets in another form. Every known place of every one-screen province is tested to land on its panel.
  - Known places are one pixel each, coloured by type (DFU's indices). The four filters hide dungeons, temples, homes and towns.
  - A cross marks the player's pixel on a province map. The world picture has no place for that pixel, so the player's province blinks there (half a second on, half off, palette index 244). "I'm at" brings back the player's province.
  - Find, on the world or a province's bar, takes a typed name and looks among the known places for every one with it in its name, those beginning with it first, eight at most. None: TEXT.RSC 13. One: its province opens on the screen that has it, a ring round its dot flashes for 1.5 s (DFU's identify flash; a click stops it), then the travel question. Several: a scroll lists them, a name a line, and a click on one goes there the same way.
- **A click on a place** asks (TEXT.RSC 31, `%tcn` the place). Yes opens the journey's popup (`TRAV0I04.IMG`): cautious or reckless, foot or ship, inns or camping, the gold, the trip's cost and its days. Begin counts the days down a twentieth of a second each, then goes. Too little gold says so (454).
- **Palettes:** the maps are under `FMAP_PAL.COL`, the popup, the scroll and the font under `ART_PAL.COL`. One screen is one picture under one palette, so what is drawn in the art palette is mapped to the map's nearest colour where it lies: the popup's rectangle whole, and in the scroll only the pixels it changed, so its burnt edges keep the map under them.

### Automaps
- **A town's** (built, `play/townmap.c`): classic's flat map. Each RMB block file carries its own 64 x 64 automap at +2251, a byte a cell: 0 for nothing, a building's type + 1, other values for its props. `town_automap` lays the town's blocks out north up, row 0 of a block its north edge.
  - **Colours:** under `AMAP00I0.IMG` with `TOWN00I0.IMG`'s caption, the kinds take the caption's own swatches (guilds and temples 101, shops 240, taverns 199, under ART_PAL.COL). Houses are ours (222): the caption has no swatch for them.
  - **View modes:** the grid button cycles DFU's three. Original shows buildings; Extra adds a ship and the special buildings; All adds the props.
  - **Zoom and pan:** the town is fitted at the largest zoom that shows it whole: half size for Daggerfall's 8 x 8 blocks, up to four screen pixels a map pixel for a hamlet. The arrows, the arrow keys and the wheel pan and zoom (half, whole, 2, 3, 4), and the compass or Home centres the player.
  - **The pointer** names the building kind under it, by `BLDG`. A left-button drag moves the map with the pointer, and a held arrow button repeats. The player's arrow is the indoor map's, `marker.c`, north up.
  - **Not yet:** rotating, and buildings' own names.
- **The keys, DFU's:** M is the automap (`INPUT_MAP`): indoors what has been seen, in a town's streets its map, in the wilds, where there is nothing to map, the travel map. V is the travel map (`INPUT_TRAVEL_MAP`), from anywhere outdoors and from the town map. Both have their slots on the controls screen, and V is in `keys.c`'s defaults, so a reset brings it back.
- **A dungeon's and a building's** (built, `play/dunmap.c` over the engine's automap, `docs/automap.md`):
  - **Marking:** the engine marks the models seen or passed near, rows by place. Travel names the place as it opens (`automap_set_place`: the keep table's own key, a dungeon's or a building's) and none in the world. The rows go in a save's `AMAP` chunk, and a new game clears them.
  - **Drawing:** the chrome (AMAP00I0.IMG, AMAP01I0.IMG's grid in 3D) is composed with a clear hole where the map goes and sent as a cutout. From the frame's under hook, `gfx_automap` draws the map in that hole over a dark ground of ours, and the chrome's picture goes over it. The player's arrow is drawn into the picture where `automap_to_screen` puts their feet. A screen frame draws no world under it.
  - **Views:** from above, the player's facing up, starting at 0.15 pixels a unit. The grid button gives the 3D view: behind and well above the player looking steeply down (700 back, 1100 up), so the slice's cut ceilings let it see into the rooms. The wheel zooms both.
  - **Controls:** the arrows pan along the view; the turn buttons turn it a sixteenth; the stairs or Page Up and Down move the slice a storey; the compass or Home brings it back to the player; M, Escape or Exit leave.
  - **The mouse, ours beside classic's buttons** (the user found classic's map hard to move round):
    - a left-button drag on the map moves it with the pointer, and a right-button drag turns it;
    - a button held over the arrows or the turns acts again after 0.3 s, then every 0.05 s;
    - in 3D a drag pans by the camera's own distance across its field of view, the ground's slant counted, so the map keeps up with the pointer.
  - **The player's arrow** is a picture of its own (`marker.c`): an arrowhead with a notch and a dark outline, 32 texels a side, drawn 18 screen pixels across, so the card shows it at four times the map picture's detail. Drawn into the 320 x 200 picture it was a blob. It is composed again only when its step of 32 a turn changes. Its way is where a point ahead of the player falls on the screen, so it is right in both views.
  - **The slice:** from 64 under the player's feet to 160 over them. A storey is 384, ours: classic and DFU move a camera, not a fixed slice.
  - **Not yet:** classic's door and exit marks and notes (the engine's to come), and a building's own colours.

### Riding (built: the horse, the cart and its wagon)
`play/ride.c`, as DFU's TransportManager and PlayerSpeedChanger.
- **Owning one:** a general store always has a horse (ITEM 94, 1000 gold) and a small cart (93, 150) on its shelves. They are things 0 and 1 of its stock, gone till the next day once bought, and it buys them back. Owned, not carried: neither weighs anything in the pack (`inventory_weight`).
- **Mounting:** the transport key (T, DFU's; `INPUT_TRANSPORT`, rebindable in Keybindings) or the HUD's transport button mounts the horse if the player owns one, else the cart, and dismounts. Only outdoors. Going into a building or a dungeon gets down: `arrive` in `game.c`, as TransportManager does on entering.
- **Speed:** a horse goes at (Speed + 375) / 39.5 m/s and a cart at (Speed + 250) / 39.5, against the walk's (Speed + 150 - drag) / 39.5. Riding cannot run, and does not train Running.
- **The picture:** `MRED00I0.CFA` for the horse, `MRED01I0.CFA` for the cart: 106 x 105, four frames, read with `cfa_load` (`formats/cif.c`, the CIF's own RLE) into pictures the first time a mount is ridden.
  - It is drawn at the screen's foot, over the bar when the bar shows: 105 of classic's rows tall and DFU's 0.8 a pixel across.
  - The frame moves every 0.125 s while the player moves on the ground (`ride_frame`, driven from `game.c` by how far the feet went), the first while they stand. The weapon is not shown while riding.
- **The sounds** (`ride_sound`, TransportManager's): while moving, a loop of the horse's hooves, `HorseClop` (97) at half speed or less and half as loud, `HorseClop2` (298) faster, or the cart's `HorseAndCart` (104); a whinny (99) every 2 to 40 seconds while mounted, the first at once. A screen, a menu or getting down stops the loop (`ride_quiet`). DFU raises the pitch running; the engine's voices have no pitch and a rider cannot run.
- **The travel map's "Foot/horse"** goes on the horse when one is owned, else the cart (`ride_journey`): the land at half the minutes on foot by horse, three quarters by cart.
- **The wagon** (`play/wagon.c`): with a cart, 750 kg of things beside the pack (DFU's WagonKgLimit), the game's own block, in a save's `WAGN` chunk.
  - **Reach:** the inventory's Wagon button turns its right list into the wagon and back. It works anywhere outdoors or in a building, but in a dungeon only within 5 m of its way out, where the cart waits (`wagon_in_reach` in `game.c`, DFU's rule).
  - **Moving things:** Remove on a pack item puts it in, if it fits. A click on a wagon item takes it back, if the player can carry it. Gold dropped goes in as a stack of coins.
  - **Refused:** a horse or a cart never goes in. Without a cart it says there is no wagon, and out of reach that it is too far.
  - **In a shop** (DFU's trade window): the trade window's Wagon button, with the same rules, makes the left list the wagon's while selling or repairing, so things are sold or mended straight out of it; what is laid on the counter and not sold goes back where it came from. Buying with it down, what is paid for goes into the wagon, by its 750 kg.
- **The cart's picture** in a list is the wagon's container icon, `INVE16I0.CIF`'s fourth frame (DFU's ItemHelper: the cart's template picture is wrong). That file's frames are bones, a body, the ground, the wagon, shelves, a chest, a merchant's hands, a bin, a sparkle, a backpack, a heap of skulls.
- **Not yet:** a ship to stand on (DFU's ship scenes); the ship itself is bought at a bank (Banks).

### A journey (`play/journey.c`, DFU's TravelTimeCalculator)
- **The path:** a line of map pixels, a step along the longer axis.
- **Land:** each pixel is `((102 x transport) >> 8) x (512 - ease) >> 8` minutes. Transport is foot 256, horse 128, cart 192. The ease comes from the climate's terrain row: 240, 220, 200, 200, 230, 250.
- **Sea:** 51 minutes a pixel by ship, 255 without one.
- **Camping and recklessness:** camping out is 300/256 of each pixel; reckless halves the whole.
- **Gold:** an inn is 5 a night of land plus the first. A ship's fare is 25 x (sea pixels / 24 + 1).
- **Arriving:** the clock moves on by the journey. A cautious traveller arrives whole and in daylight: before 07:10 waits for it, after 17:00 waits for the next morning's.
  - The minutes on the road are not tired one by one afterwards (`g_last_minute`).
  - The player lands at the location's entrance: the edge facing the way they came, a tenth of a block outside, then the nearest walkable cell, looking in (`stand_outside`, DFU's `DirectionFromStartMarker`). With no origin, the south edge. A dungeon's outside (labyrinth, keep, ruin, graveyard, coven) is landed on by its way down when that door is loaded (`travel_go_to_place`); otherwise the same edge.
- **When it opens:** outdoors, by the map key or the HUD's map button (in a town, from its automap: Automaps, above). In a dungeon or a building it says the automap is not built yet.

Tested on the host (`check_wider_world`):
- the arrow's picture: an arrow tip up at step 0, outlined, a quarter turn pointing right, the steps of a turn;
- the indoor automap: the hold's place named on arrival and none after leaving, the hold revealed, the view's rectangle, the player on the map from above and from behind, polygons drawn in both, the stairs, the wheel, the compass, M and Exit, the chrome's hole whole and nothing clear outside it, the arrow on the map pointing up from above, a drag there and back, a right drag turning, which buttons repeat, what was seen through a save, writing `build/dunmap0.ppm` and the polygons' outlines `build/dunmap-top.pgm`, `dunmap-3d.pgm`;
- the town map: Daggerfall's 8 x 8 blocks fitted at half size, its kinds coloured and its modes, an armorer named under the pointer, the up arrow and zoom by clicks, the wheel, V and Exit, writing `build/townmap0..1.ppm`;
- the wagon by clicks: none without a cart, too far out of reach, a sword in by Remove and out by a click, the cart refused, its 750 kg, gold and things through a save, writing `build/wagon0.ppm`;
- riding: a general store's horse and cart, a horse weighing nothing in the pack, no mounting without one or indoors, classic's speeds, a quarter second on the hold's floor at 425 / 175 of a walk's with the run key doing nothing, the journey by horse, `MRED00I0.CFA`'s and `MRED01I0.CFA`'s four frames, the frame's turns;
- the sums by hand: woodland on foot, recklessly, camping and on a horse, the sea by ship and swimming, a diagonal's steps, and arrivals reckless, before light and after 17:00;
- the map's 11199 known places, one learnt and kept through a save;
- every province's places on its map;
- by clicks: the world, Daggerfall's province picked by its mask, Daggerfall's dot named and chosen, yes, reckless, on foot, the countdown, and the journey chosen at the sums. Sentinel to Daggerfall comes to 17 days and 45 gold.
- `build/travel0..3.ppm` are for a look.

**Not yet:**
- a ship to stand on;
- the indoor automap's door and exit marks and notes;
- talk's marking places on the travel map (a building is marked on the town map);
- clicking a neighbouring province from a province's map;
- the guilds' faster travel;
- regional holidays.

The journey's numbers are in `journey.c`, not MAIN.DAT, as P9's talk tables are.

## People (built: P9)

Everything a person does for the player, on Daggerfall Unity's numbers, with classic's own pictures and TEXT.RSC's words, read at run time and never copied.

### What the world thinks
`play/standing.c` is the game's own block, kept across places, reset at a new game before creation (its answers move it):
- **Each faction's reputation**, from `FACTION.TXT` (`formats/factionfile.c`, 366 factions, a tree by indentation). A change spreads as DFU's `PersistentFactionData`: half to the allies, half off the enemies, and down from the root of its tree, the whole to the one changed and the root, half to the rest. All within -100..100.
- **Each social group's reputation**, and the lasting bonus to every reaction (`r`, `rf` and `RR` in the background answers, `biog.c`).
- **Each region's law and bank:** legal reputation, banished, the account, a loan, and what things cost there (rolled 750..1250 per 1000 at a new game; classic's monthly drift is not built).

A person's reaction is their answering faction's reputation (`standing_answers_for`) plus their own social group's plus the bonus.

### Who is spoken to
The use key's order: a body or heap first; then a townsperson the view rests on (`people_in_view`, within `TALK_REACH`, the region's People); then the engine's ray. A flat person is `USE_TALK`, its faction in `what` and its flat in `tag`.
- A merchant (social group 1) in a bank or a shop offers a panel (`service.c`): Talk and Banking, Talk and Sell, or a smith's Repair, Talk and Sell.
- Anyone else talks. A face is `FLATS.CFG`'s for their flat, in `TFAC00I0.RCI` (`formats/faces.c`); a flat it does not name gets record 410, the fallback DFU uses too.

### Talk
`play/talk.c`, on `TALK01I0.IMG` at DFU's rectangles. `TALK02I0.IMG` and `TALK03I0.IMG` have no headers, and their bands run top to bottom: Location, People, Things, Work; and Tell me about, Where is.
- **Where is** lists the kinds of building in this town (`places/town.c`, read from every block once and kept until another town is asked about). The answer gives the nearest one's way by the compass. Told the way, gladly or grudgingly, by one who knows (`rules_talk_knows`), the building is marked on the town map: a ring round its cell in the talk's ink, its name with "marked" on the map's line, until another is pointed out (`townmap_mark`; DFU's talk discovers it). The conversation adds "(It is marked on your map.)". The mark is not saved.
- **Tell me about** has the news: one piece a person, from those whose macros the game can say (`words_say_plain`).
- **Work** is a question with no list; no one has work yet (P11).
- **The answer:** `rules_talk_value` against a roll, DFU's: Personality / 5, +5 for a place, the reaction, the tone's liking by social group with its skill check. It picks the record from TalkManager's tables. Rolls are seeded by who is asked, so a person does not change their answer. Under -20 a person says only "no response".
- **Ours, not DFU's:** adding the reaction into the value (DFU's summary gives Personality and tone; a person who hates you must answer worse), and using the table's first row for where a place is.
- **`play/words.c`** fills a record's macros. A macro it does not know comes out empty, so a record meant for a person checks for that in the tests (no `%` left in what is said).

### Names (built: people's)
MAIN.DAT's `NAME` (`tables/names.c`), cooked on the first run from ARENA2's NAMEGEN.DAT (`formats/namegen.c`): 785 pieces in eleven banks. A person in talk is named over the window instead of their faction: their region's people's bank (FALL.EXE's region races, DFU's `MapsFile.RegionRaces`: Breton or Redguard; kept in `names.c`, not yet in MAIN.DAT), their sex from FLATS.CFG's gender line (2 a woman, `faces_female`; else the seed's), and their own seed, so they keep one name (DFU's NameHelper patterns: a first name of two pieces and a surname of two, a Nord's "...sen", a Redguard's one name of three or four). An Argonian's bank is classic's Imperial one. The names' own dice are not the game's (`combat_roll`).

### Shops
`play/shop.c` and the inventory screen's trade modes (`bag_open_trade`).
- **Shelves are models** (`DaggerfallInterior`'s furniture numbers past 41000), `USE_OTHER`/`OTHER_SHELF` usables in any building; the game asks whether it is a shop. A house's chests and cupboards are the same kind of model, `OTHER_CONTAINER` (Steal and theft).
- **Clothing stores are shops** (`BLDG`'s `stock_clothing`: 50 a clothing store, 10 a general store; `building_is_shop` counts it). A shop's clothes are the player's sex's templates, each rolled twice (DFU's two clothing groups both turn into the player's), no rarer than the quality, at `chance x 5 x (21 - rarity) / 100`, each in a look the roll picks. A clothing store buys clothing; so do a general store and a pawn shop (DFU's accepted groups).
- **Coins turn behind what is for sale.** While buying, every thing in the shelf's list and the basket has DFU's coins animation behind it (TEXTURE.434 record 6, eight frames, one each 0.08 s: `bag_step`), the sign that this is a purchase. It makes the screen change, so composes it and sends it to the card every 0.08 s while buying: unmeasured on the 233, and the one place the inventory is not still.
- **Stock holds no list.** Today's comes from the game's seed, the shop's key and the day: each template no rarer than the quality, by `BLDG`'s chance number, `chance x 5 x (21 - rarity) / 100`. Its keep row holds the day and a bit for each thing taken, so a town of shops costs no memory, and a new day stocks anew. Only weapons and armour are stocked: nothing else exists yet.
- **Buying** moves things from the shelves on the right to the basket on the left, then Buy pays `rules_trade_price` of the sums' cost. **Selling** lays things from the pack on the counter on the right, which gives back what is not sold when the screen closes. **Repair** is the same, at a tenth of the value costed; it is instant here, where classic takes days.
- **Steal:** seen by `rules_shoplift_chance`, the basket left behind and the court next. Unseen, it is taken free.
- **Hours** are `BLDG`'s. A shut street door says it is locked. Holding the run key while using it picks the lock (`rules_lock_chance`, strength half the quality), as a retained shortcut; P9-b's Steal mode also selects that lockpicking path (the remaining theft paths are P12-a). A failed lock will not be tried again until the skill has grown. Shelves in a shop broken into refuse Buy but allow Steal.

### Holidays
`HOLI` (`commerce.c`) holds the sixteen holidays every city keeps, by row id `month x 30 + day` with the day from 1 (UESP's Holidays; regional ones are not in yet). `holiday_on(month, day)` takes the clock's zero-based day. On the first frame of a day that is one, the game says "Today is <name>." Flags say what a holiday does; built so far:
- **Sun's Rest** (Sun's Height 20) shuts every shop all day, its street door locked whatever the hour (DFU's `BuildingIsUnlocked`) and its shelves closed, as out of hours.
- **The Merchants' Festival** (Sun's Height 10) halves every price in a shop; **the Warriors Festival** (Frostfall 20) halves a weapon's (`shop_cost`, `Shop.holiday`).
- The temple cure (First and Second Planting), half-price temple services and free rooms and drink are flagged for when temples and taverns sell them.

### Banks
`play/bank.c`, on `BANK00I0.IMG`, with sums usable without the screen. Each region has its own account. A letter of credit is an item worth what it says, a hundredth more to write, at least 100. A loan is 100 to 50000 a level, a tenth more to repay, due in a year of 360 days; carried gold pays first. A loan past its day is a default, a crime in its region, looked at once a game day (`bank_check_loans`). Houses say none are for sale. **Ships** (DaggerfallBankManager): Buy Ship asks 1 for a small ship (100,000) or 2 for a large galley (200,000), paid from the gold carried first, then the region's account; one at a time. Sell Ship asks 1 to sell it back for 85% into the account. Owning one, the travel map's voyages pay no fare (`journey_own_ship`). The ship is the game's one, in the save's `GAME` row. DFU's ship to stand on, its deck and cabin, is not built.

### The law
`play/court.c`. `court_commit` takes the crime's loss off the legal reputation and half that off the region's People (`court_note` does that alone, for a crime the watch answers: a failed pickpocket). Shoplifting and a defaulted loan still open the court at once. A citizen killed in the street does not: the watch comes, below, and the court for that killing is not built. The trial is DFU's `DaggerfallCourtWindow`: the sentence from `CRIM`'s sums, split into pieces of 40 gold or three days; what cannot be paid is served; a low standing can mean banishment. Guilty halves it. Not guilty argues by Etiquette or lies by Streetwise, against the standing and the skill, within 5..95; losing moves the fine by the court's mood. Prison ticks a day each 0.3 s on `PRIS00I0.IMG`. Then four hours pass, the player is whole, a sentence served gives back half the loss less one, and they stand in the street (`travel_out_to_street`). Banishment is marked on the region and not yet enforced.

### Tested on the host
`check_standing`, `check_people_sums`, `check_words`, `check_talk`, `check_shop_and_court`, `check_bank`, `check_people_in_town`, `check_citizen` (the sure blow, every guard hostile, wanted after the people are dropped and built again, a hostile guard not a talk target, the blood splash, the guard's corpse and one thing taken off it), `check_standing_saved`: the tree, a spread by hand, every sum by hand, no macro left in any record of talk, Daggerfall's 647 buildings, a way asked and the news by clicks, one who hates saying nothing; a shop's stock by day, buying, selling and mending by clicks at the sums, a thief seen; guilty's half, prison's days, the reputation back; gold, letters and loans; the bank and the merchant's panels by clicks; an armorer in Daggerfall entered with its shelves and its person; all of it through a save file. `build/talk0..1.ppm`, `shop0..1.ppm`, `court0..2.ppm`, `bank0.ppm`, `service0.ppm` are for a look.

### A killing in the street
`play/watch.c`, from the swing in `play/combat.c`. A street citizen's health is 1 (`PEOPLE_CITIZEN_HP`). The blow is `combat_blow`: the to-hit roll is made at 100, so it cannot miss (`combat_roll(100)` is 0..99), and the damage is the weapon's ordinary damage. That 1 health is what the damage takes. There is no separate "dead at once" flag. A guard, and every creature, keeps the 3..97 roll and its own health.

Trusted for the chase: Daggerfall Unity's `PlayerEntity.SpawnCityGuards` and `MakeNPCGuardsIntoEnemiesIfGuardsSpawned`, and the note that an active crime clears when the player leaves the location or fast-travels, not when they step into a building. UESP's crime and reputation pages agree that a citizen dies in one blow and that the guards then pursue. The blood and the corpse are DFU's `EnemyBlood` and `EnemyDeath`, and they match ENEM: TEXTURE.380 record 0 is the splash (`blood` 0 on every human and the city watch), record 1 is the city watch's corpse.

The killing sets **wanted** on a keep row the game owns (`watch_wanted`). It is not a bit on a person, because entering a building drops the people and builds them again outside. The building does not clear it. Leaving the town does (the wild, or a dungeon: `travel_town_here` is false). A new game's `keep_clear` does. This slice does not open a court, a fine, or a jail.

Who comes: every living guard already in the loaded population. DFU's immediate pass is the guards within 77.5 m, and once any city watch is out it turns every guard still loaded; the pool here is that loaded population, so they all turn. Each is marked hostile to the player (`people_set_hostile`) and woken with `foes_summon` as ENEM 146 (archive 399, bark DAGGER.SND 456). The mind gives up after 600 classic ticks unseen, three times a creature's, and while wanted is still set a guard who gave up is put back on the player. A guard who walks into the pool later is turned too. If the street had none, 2 to 5 are spawned, not one. Three lines, each `HALT`, are queued at the kill and spoken one a second, each with that bark. `tai` stands the minds; the hostile flag stays.

Hostile blocks talk. `people_in_view` takes the nearest person along the ray; when that one is a hostile guard, the use key does not open a conversation (nothing behind them is spoken to instead). A guard who is not hostile is still someone to talk to, the same as any other person in the street.

The killing blow shows one blood splash: TEXTURE.380 record 0, a billboard at the body's middle, 10 frames a second, for a second, then gone. The same picture a dungeon creature's splash uses. The citizen leaves no corpse.

A killed guard stays. The pose is ENEM 146's corpse, TEXTURE.380 record 1 (`ENTITY_DEAD`), on the ground under them. The street motor does not take that body while the player is within `PEOPLE_RECYCLE` of it, including after the chase; it goes only once it is far and out of sight. The use key searches it (`loot_find`). The city watch's loot letter is empty, so there is no table roll; the body holds a class enemy's kit (`loot_class_kit`, DFU's starting equipment): a one-handed or heavy weapon, sometimes a shield or a second blade, and the helm, pauldrons, cuirass, greaves and boots as that roll gives. Taking one is the inventory's existing take, remembered on the corpse's keep row.

**Not yet:**
- running on the 233;
- the court and the bounty for that killing, and banishment enforced;
- Steal mode on enemies, locks' alert chance, the keyboard scheme, containers (P12-a, slices 2 to 5; slice 1 picks a citizen's pocket);
- buildings' own names (the list and the map show their kinds; DFU's GenerateBuildingName builds them from TEXT.RSC);
- talk's People and Things lists, Tell me about's organisations, guild greetings, the logbook;
- taverns' rooms and drinks, guilds' services, houses, shelves in a house or a library;
- classic's monthly price drift, and repairs taking time.

## Classic HUD bar (built: P9-b, host-tested; target acceptance pending)

`play/hudbar.c` owns the plain-C layout, mode names and heading selection; `overlay.c` owns pictures, and `game.c` routes buttons through the same screen-opening functions as the keys. The rectangles and icon order follow [Daggerfall Unity's HUDLarge](https://github.com/Interkarma/daggerfall-unity/blob/master/Assets/Scripts/Game/UserInterface/HUDLarge.cs). The full bar is on by default (`panel` still flips it).

- **Art and portrait:** MAIN00I0.IMG, MCOL00I0.CIF's blue backing, the selected race/sex/face, live vitals, MAIN01I0.IMG's mode sheet and CMPA00I0.BSS's compact compass. The head is refreshed by `game_enter` for a new or loaded character; the picture module owns its pixels across every place change.
- **Pointer:** Classic Cursor mode starts with a free pointer. Tab (`INPUT_CURSOR`, rebound in the controls screen's cursor slot and kept in gfall.cfg) switches between the pointer and continuous mouse look without pausing. View mode starts with mouse look; Tab frees its HUD pointer. Character, inventory, options and rest open their existing screens; a hostile creature still forbids rest. The compass reports the date and time. The weapon button sheathes or draws the weapon. Screens and menus temporarily own the pointer; closing one restores the player's cursor choice.
- **Modes:** the button cycles Grab, Info, Steal, Talk. Grab keeps the general use key; Info reports the aimed usable or building without activating it; Talk only addresses people. Steal enables the existing lockpicking path and never falls through to ordinary dialogue; its missing theft and pickpocket paths are P12-a. The spellbook, magic-item, transport and map buttons give a short response until their systems arrive in P10/P12-b.
- **Input handoff:** pointer clicks, including clicks in empty bar pixels, never attack. The attack gate holds through the final release of a click after leaving a screen or the pointer. Entering the pointer or sheathing cancels a drawn bow or pending swing without a hit, keeping its cached idle picture. A pointer handoff discards the old look delta already sampled that frame. A new or loaded character starts in Grab with the weapon drawn; travel preserves the live mode and sheath choice.

**The 233's cost:** pictures are read once at startup (the head once per character/load), not while drawing. The four mode icons share one sheet and cycling changes only UVs. The BSS's 32 uncompressed 48 x 40 frames stay in about 60 KB of RAM; only the current frame is on the card, replaced when the 11.25-degree heading changes. This bounded update is in `overlay_set_yaw`, outside `overlay_draw`; no whole-panel recomposition or upload runs per frame. The unchanged bar just draws its pieces. The tests measure 36 KB for all persistent HUD pictures on a keyed card and 42 KB with 1555 cutouts, including the old strip/box and the pointer. Both leave room inside GAME_PICTURE_KB's 256 KB for the existing worst-case weapon and controls-screen allowances.

**Tests:** `check_hudbar` checks button edges and seams, empty bar pixels, compass wrap, icon rows, the real art, a truncated BSS refused without publishing a partial result, and the fake-card budget in both formats. `check_weapon` also cancels a drawn bow and a pending melee hit and checks the click-release gate. `build/hud0.ppm` and `hud1.ppm` show the selected head, Grab/north/full vitals and Steal/east/half vitals. These are native art previews, not target screenshots. Windows pointer routing and the complete bar still need a playtest on the 233; P9-c supplies the implemented classic cursor movement and world-pointing scheme below, also awaiting target acceptance.

## Quests (P11: the files read, the list in MAIN.DAT, the machine built)

Classic's quests are a pair of files each in ARENA2, read in place and never copied, as TEXT.RSC is: the `.QRC`, TEXT.RSC's own format, the words by message number (`textrsc_read_in`); and the `.QBN`, the logic (`formats/qbn.c`, from UESP's Quest hacking guide and Quest pseudo-codes). Daggerfall Unity runs its own text versions of them, which are classic's content, so they are not used: the binary is.

### What is in MAIN.DAT, and what is not
- **`QUES`** (`tables/quests.c`) is the list of what can be offered: Daggerfall Unity's QuestList-Classic, 187 quests by file name, with who offers each (a guild group from FACTION.TXT's ggroup, or a social group), to whom (members, others, prospects, a temple's god), the least it asks (a rank or level under 10, a reputation from 10) and its flags (once only, adult). 13 of them are the Special Edition's and not in every ARENA2: a quest whose files are not there is not offered.
- **The quests themselves are not cooked.** They are the game's text and scripts, and stay in ARENA2 like the blocks and the pictures. A mod adds a quest by its two files and a `QUES` row.

### The QBN
- A 60-byte header: at 16 ten record counts, at 36 eleven offsets. Sections: 0 items (19 bytes a record), 3 people (20), 4 places (24), 6 timers (33), 7 foes (14), 8 the logic (87), 9 states (8), 10 the symbols' names (27, in seven files). 1, 2 and 5 are never used. Every one of the 232 quests in ARENA2 parses with every section inside the file.
- A logic record is an opcode, flags, a count of operands (1..5; eight of classic's files carry bad counts, held to 5), five 15-byte operands (a negation, a constant or a reference to a section's record, a value), and a QRC message.
- Each resource carries its symbol's hash, `qbn_hash` (UESP's Text Variable Hashes: twice the hash plus each character): "reward" is 0x1B14. The QRC's text names resources by symbol, `_name_` the thing itself (a person's name, a gold amount, a timer's minutes), `__name_` and `___name_` where it is (its town, its dungeon or house), `=name_` a timer's days, `==name_` a person's faction; `%` macros are TEXT.RSC's (`words.c`).

### The survey: what the 174 offered quests here use
Every quest uses 6 (if a state, end), 12 (start a timer), 23 (a log entry) and nearly every one 4 (the reward). Then 52 (AND of states, 138 quests), 28 (the player meets a person, 116), 51 (a message when a state is set, 105), 7 (a state clears others, 101), 19 (a place onto the map, 93), 22 (a foe at a place, 87), 1 (an item given to a person, 87), 2 (a kill count, 85), 9 (foes spawning, 72), 61 (a reputation change, 70), 26 (an item to a person or the player, 66), 21 (a foe hurt, 64), 34 (a random state, 61), 54 (an item made ordinary, 61), 43 (the player at a place, 53), 81 (50), 30 (a person at a place, 37). Those twenty run most guild quests; the rest come as quests need them.

### The machine (built: `play/quest.c`)
- **Starting** a quest from its `QUES` row (`quest_start`): its QBN read into a heap of its own; the giver is the person who offered it (their building and faction), known by the symbol "qgiver" or "questgiver"; another person gets a name from a seed; a place is bound by its record: a building of this town (general 1, by building type, -1 any but houses), a building of another town of the region (general 2), or a dungeon of the region (fine 1) of the kind the file asks for (its type: a crypt, a mine...; -1 any; with none of that kind found in forty tries the first dungeon found will do), noted both by its map location (for the map and journeys) and the dungeon under it (as travel names it inside). A place named outright (a storyline's, 4 offered quests) is not done: such a quest does not start. Gold is DFU's Item.CreateGold: 150..200 × (guild rank + 1, else level / 2 + 1, to 10), by the region's prices and the guild's power. As many as `QUEST_SLOTS` (32) at once, each its own heap and save row; Get Quest never limits by guild.
- **The logic** runs record by record until nothing changes (`run`), each time something happens: a record with no state in its first operand runs once at the start; a trigger (28 meet a person, 1 an item given to them, 2 a kill count) sets its state when its event comes and says its message; AND (52) and OR (53) set their states continuously (a 0xFFFFFFFE operand is the filler, true for AND, false for OR); an action runs when its state comes true, and again only after it has been false, so a state cleared (7) re-arms it.
- **Done:** 1, 2, 4 (the reward: gold paid), 6 (the end), 7, 8 (start a quest by number), 21 (a foe hurt, `quest_hurt` from combat), 29 (a question: classic's message box with Yes and No, `quest_take_prompt` and `quest_answer`; its words are the fourth operand), 43 (the player at a place, `quest_arrived` as a place is entered), 10 and 11 (topics), 77, 78, 79 (always-on conditions), 81 (a person anywhere), 12, 13 (timers: random, fixed, or by the way to their place and back with a day to spare, 2 × the journey's minutes + a day; when one runs out, the state of its symbol is set), 19 (the place onto the travel map, `atlas_learn`), 22 (a foe put at a place), 23, 24 (the log), 26, 76 (an item to the player, held by the quest), 28, 30 (a person put at a place), 34 (a random state), 35 (cycle), 46, 48 (hide, reveal), 51 (a message), 52, 53, 61 (a faction's reputation), 65 (the legal one). The rest do nothing yet.
- **Quest people stand in their buildings:** entering a building, `world.c` asks the game's hook (`world_set_people_hook`, `quest_people_hook` in `game.c`) for its people; each quest person put there (30) stands beside the building's first person as their faction's flat for their sex (else the region's people's), marked `QUEST_PERSON_TAG(slot, person)` under `FLAT_PERSON`. Using them is `quest_met_person`; with nothing from the quest just then, they talk as one of the region's people.
- **A quest person's name** is `person_name` (their region's people's bank, their sex, the quest's seed and their place in its file): the one its messages use, and the one over the talk window when they are spoken to (`quest_person_name`).
- **Quest foes stand in their dungeon:** opening a dungeon, `dunstream.c` asks the game's hook (`dunstream_set_foes_hook`, `quest_foes_at`) for its quests' creatures; they take the dungeon's last draws, deepest in its blocks' order, awake. The same draws each time, the dead included, so the stream's keys keep a killed one dead when its block comes back.
- **Quest items:** an item handed to the player (26, 76) goes in the pack as a Parcel (`ITEM_QUEST` 999, ours, the letter's picture), tagged in `Item.variant` by its quest and item (`QUEST_ITEM_TAG`); given to a person (1) it must be carried, and is taken; made ordinary (54) it is untagged and stays; the rest go with the quest when it ends. A reward that is no gold is armour or a weapon (categories 2, 3) in a material by level, else gold to its worth.
- **Once-only quests:** a `QUES` row with `QUEST_ONCE` (flag 1: 22 offered quests and every main one) is marked done when accepted, finished or not, and never offered or started again (DFU's `oneTimeQuestsAccepted`); the marks are a bitset of `QUES` rows, saved as a `QDON` row, and a new game clears them.
- **The main quest** (UESP's Main Quest; DFU's StartGameBehaviour and its StartQuest action): 35 `QUES` rows of group `QUEST_GROUP_MAIN` hold the main quest's S000000x, the master S0000999, the tutorial and the cures; no guild offers them. A new game starts `_BRISIEN` (Brisienna in Privateer's Hold), which starts S0000999 and S0000977 (op 8, a quest by its number, file `S%07d`). S0000999 watches the player: op 77 (a level reached, `LevelCompleted`), 78 (a person of a faction is available) and 79 (reputation with a faction at least, `WhenReputeWith`) set their state when true and leave it set, and an AND of them starts each main quest by op 8 as the player's level and standing allow (level 3 to 8 and the Emperor's, kings' and queens' favour). Each runs through the same machine. The tutorial (`_TUTOR__`) is not started yet.
- **Places named outright** (a storyline's, by an object id the game does not read) are bound to nowhere (region -1): the quest runs, and nothing it does to that place shows. A person with no place (op 81) is put in a building of another town of the region, in the place spared after the file's own.
- **Events from play:** speaking to the giver in their building (`quest_met`, from `person_used`) or to a quest person (`quest_met_person`); a creature killed in a dungeon (`quest_killed`, from `combat.c`, by its kind and the dungeon); each game minute (`quest_tick`, for timers).
- **Messages** (`quest_take_message`): shown whole in classic's message box (`play/msgbox.c`: words on a scroll sized to them, any click or key closes it), one at a time, and in the console's log. A record's QRC message, one of its variants, its symbols filled (`_x_` the thing: a person's name, gold's amount, a place's kind or a dungeon's name, a foe's kind, a timer's minutes; `__x_`/`___x_` its town or dungeon; `=x_` a timer's days; `==x_` a person's faction), `%qdt` the day it began ("Day 11 of Sun's Dawn, 3E 405"), then TEXT.RSC's macros. - **Get Quest** at a guild: a quest of the guild's group, for members or not (a temple's own god's letter for its members), whose least (a rank under 10, a reputation from 10) the player meets, not running, its files in ARENA2; one at random, offered (`quest_offer`: bound, so its offer can name its gold and places, not running) with the person as giver. Its offer (1000) is asked in the message box with Yes and No (Enter, Y; Escape, N): Yes runs it (`quest_accept`) and says its acceptance (1002); No drops it and says its refusal (1001).
- **Talk topics** (ops 10 and 11, `quest_topics`): a person, a place or a thing a quest has added while its state is true and no remove record has taken away is listed under "Tell me about" (after the news), by the person's name, the dungeon's name or the building's kind; asking says the QRC messages the resource names for it (its two "tell" messages, or for a thing the one it is given with). "Where is" for a quest's place and the marks it puts on the map wait. None of the 174 offered quests opens one at its start, so only the call is tested.
- **The logbook** (`play/logbook.c`, the sheet's Log button at DFU's rectangle): a quest a page on a scroll: its file name and place among the quests, its log lines as the quest wrote them (symbols filled), and the days its soonest timer has left. A click on the right half, Right or Page Down turns to the next, the left half, Left or Page Up to the one before; Escape or a click on its bottom edge closes it.
- **Saved** as `QUST` rows, a quest a row (its states, fired records, timers, gold, bound places, where its people and foes are, kills, log); the logic is read again from its QBN.
- **The console:** `quests` lists what is running and its log; `quest M0B00Y00` starts one here, the player its own giver.
- **Not yet:** repeating spawns (9), foes that find the player on the road (they need creatures made in the player's presence, an engine matter); named storyline places' own locations; "Where is" for a quest's place, and the marks it puts on the map; the tutorial (`_TUTOR__`) at a new game; the Thieves Guild's and the Dark Brotherhood's invitations; the cures' start; and the opcodes the offered quests use least (31, 37, 39, 46's kin, 55, 56, 57, 68 to 73).

### The plan
1. **The machine:** built, above.
2. **The world:** a quest person standing in their building, a foe or an item at a dungeon's quest marker, a place put on the travel map (the talk gap from P10), the town map's mark.
3. **The ways in:** Get Quest at a guild (`GUILD_SERVICE_QUESTS`) and talk's work, choosing from `QUES` by group, membership and what the player has.
4. **The logbook:** the sheet's Log button, the active quests and their deadlines.
5. Later: the main quest, the tutorial, the cures, the Thieves Guild's and the Dark Brotherhood's invitations.

## Steal and theft (P12-a: built and host-tested; first playtest 2026-11-05 fixed below)

**The first playtest (2026-11-05)** found: a seen theft from a container went straight to the court where DFU brings the guards (`court_note` and `watch_alarm` now, as a pickpocket's failure does); "HALT" and no guard after a failed lock or pocket, because a busy street fills all `PEOPLE_MAX` slots and `people_spawn_guard` then found no slot (a citizen out of sight is now sent away to make room, `room_for_guard`) and because guards, when they did come, came 4000 to 5500 units out of sight (now a ring of `PEOPLE_GUARD_NEAR` to `PEOPLE_GUARD_FAR`, 700 to 1600, out of sight if a cell allows; and when every loaded guard is farther than 1800 from the crime, 2 to 5 are made near, `WATCH_CLOSE`); a stutter on E, whose one found cause is that the first guard needed called `while (load_one())`, which reads every picture of every outfit of the street at once, even when it then found no slot (it loads the guard set only now), and the pickpocket asked the street twice (`people_in_view_at` once). A stutter that is not that one has not been found: the console's `prof` shows the `play` stage when E is pressed. And two things asked for: the player could not tell whether a use would steal (the bar's mode icon shows Steal while crouched, the middle of the screen says "Stealing" and its cross becomes an amber ring, `overlay_set_stealth`), and the font's "+" crosshair looked wrong (a small dot with four ticks and a dark edge, built at startup, `overlay.c`'s `cross_sheet`; the "+" stays only as the fallback when the picture does not load). A Steal use with Info or Talk chosen on the bar is that mode's, so crouched in Info shows no Stealing. The label was first "Stealth", which promised the skill; it says what the use will do. **The red cross** (`probe_frame` in `game.c`, `overlay_set_target`): the cross or the ring turns red when the middle of the view rests on a target. Stealing: a townsperson or a class enemy in pocket's reach not yet tried, a locked door of a dungeon or a house, a shut street door, a closed shop's shelf, a house's container. With a weapon drawn and not stealing: a creature in weapon's reach with its back to the player, a backstab (triple damage on a roll of the skill, `combat_blow`), asleep or awake. Not drawn with the classic pointer (no cross). The questions are throttled, since a passive ray is dear on the 233 (87 microseconds a person ray on the host, about 40 times that there): the creature is asked every frame (0.2 microsecond), and the person and the lock take turns every 0.12 s, so the reading is under a quarter second old.

**Decided with the user (2026-11-04):** the player's height becomes variable in the engine and the game builds its crouch on it (asked of the ENGINE implementer); a failed pickpocket brings the guards (done through the watch below); houses have no containers, so they are ours to build (slice 5).

**Built, slice 4's crouch part:** crouching (`player_crouch` in `player.c`, over the engine's `body_set_size` and `body_can_stand`; DFU's sizes: half the height, 36 units, the eye at 34.2, half the walking speed, no running and no jump while down, never on a horse). The key is `INPUT_CROUCH`, default Ctrl (C is the character sheet), on the Keybindings screen. Held by default; the Gameplay checkbox "Toggle Crouch" makes a press flip it. Under a low ceiling the player stays down and rises when the head has room. E stays the one use key, as in a first-person game: used while crouched it is Steal (`steal_intent` in `game.c`, the same as the bar's Steal mode): a street citizen's pocket is picked, a shut door's lock, and a person inside a building is not spoken to. The bar's modes keep their meaning, Info and Talk first. Crouching helps Stealth through speed, not by itself (see Creatures awake, Stealth). Tests: `check_crouch`, `check_menu`'s Toggle Crouch.

**Built, slice 5 (house containers):** a house's chests, cupboards and drawers are the furniture models DFU names (`houseContainerObjectGroupIndices`: ids past 41000 numbered 3, 4, 7, 8, 27, 32 to 35, 37, 38, 50, 51, and group 418; a shelf id is a shelf first), `OTHER_CONTAINER` usables of `doors.c` in any building's inside, their keep key made as a mover's (`tag`). Daggerfall's type 17 and 18 houses have four and three. Used, `container_used` in `game.c`: an empty one says "You find nothing"; else TEXT.RSC's record 37 (DFU's PrivatePropertyId, "This looks like private property. Do you still want to look through it?") asks Yes or No in the message box, and Yes opens the loot screen on it. It is searched as a body is: `loot_house_begin` makes the game's one `Entity` stand for it, its things rolled from the keep row's level and the key's seed out of one table of ours, 'A' (a little gold, rarely a weapon or a plate; DFU's `StockHouseContainer` was not found), the same each look, what was taken kept in its row (never restocked). **Taking is theft** as in DFU's `AttemptPrivatePropertyTheft`: when the screen closes the things taken this visit are counted and weighed, and `shop_seen_chance` (the shoplifting sum at the building's quality) says whether it was seen: seen, `CRIME_THEFT` is noted and the guards come (`court_note`, `watch_alarm`: DFU's Theft and SpawnCityGuards, as a pickpocket's failure does; only a shop counter's theft opens the court at once); unseen, a use of Pickpocket. Not here: a house of the player's own (DFU: no question, no theft), since nothing can be bought; the engine's shop shelves and a library's books are unchanged. Tests: `check_house_container` (the same each look, a thing taken counted and gone), `check_house_interiors` (Daggerfall's houses have containers).

**Built, slice 3 (locks):** a locked door of a dungeon or a house (a mover, `Mover.lock` a level) is picked in Steal (`door_picked` in `game.c`, `theft_pick_inside`): chance `Lockpicking + 5 x (level - lock)` within 5..95 (`rules_inside_lock_chance`), a use of Lockpicking each try, won or lost (UESP), tries without limit (DFU's inside doors have none); a lock of 20 or more says "held by magic" and costs nothing. Picked, `Mover.lock` is set to 0 and the use goes on to open it: the engine's keep rows note a lock other than its file's, so it stays picked across saves and blocks (`docs/interact.md`). A street door that failed (`door_lets_in`, the exterior sum) is heard by the watch `33 x (1 - Stealth%)` times in 100 (`rules_lock_alert_chance`, UESP's classic rule; DFU's own code has no such roll): an attempted breaking and entering (`CRIME_ATTEMPTED_BREAKING_IN`, `court_note`) and the watch (`watch_alarm`). UESP says only if you were not seen, but we have no witness model: it is a roll. The shelves of a closed shop were already stealable. Tests: `check_theft`'s lock sums, clamps, alert, the use counted, a magic lock.

**Built, slice 2 (enemies, and looking):** `combat_aim` (the nearest living creature a ray meets within reach, no wall between; an arrow in flight aims with it too, `combat_aim_at`). In Steal (the bar's mode or crouched), `creature_robbed` in `game.c`: a creature within half of talk's reach, a **class enemy** (DFU: only a human is robbed, a monster is "nothing to steal from") has its pocket picked by the same sums as a citizen's with `+ 5 x (player level - their level)`; a class enemy is made at the player's level, so today that term is 0 and only a future creature of another level moves it. A failure wakes them and brings them (`foes_alert`) and notes no crime (`theft_pickpocket` with a region under 0), as DFU. One try each, by their key hashed; DFU also turns the enemies near them hostile, which we do not. Looking: with no steal in mind, E on a living creature within 512 units (`LOOK_REACH`, ours) says "You see a Rat."; the bar's Info mode does the same as far as the creature can be seen, before it tries a door or a heap. Tests: `check_theft` (the level term, no crime for an enemy), `check_combat`'s aim at a creature of the hold and away from it.

**Built, slice 1:** Steal mode on a street citizen. `rules_pickpocket_chance` and `rules_pickpocket_gold`, `play/theft.c` (the attempt, the once-a-person memory, the lines), `court_note` (a crime's cost to the reputation with no court), `watch_alarm` (the watch's wanted, HALTs and guards for a crime that is no killing; `watch_on_kill` calls it) and `CRIME_PICKPOCKETING`. In `game.c` the use key in the bar's Steal mode (a click on the pointer, E) tries `street_person_robbed`: a person within half of talk's reach is robbed; one farther is "too far away"; no one lets the ray go on to doors and shelves as before. A person is known by `people_in_view`'s number; the tried set (64) is dropped with every place entered (`game_enter`), as DFU's flag goes with the person; a slot recycled to a new walker inside one visit can inherit "tried", a known small leak. The run key held no longer means anything for people, only for locks, so a hurried E never robs anyone. Tests: `check_theft`. Guards come as they do for a killing: every loaded guard hostile, or 2..5 spawned (`people_spawn_guard`); there is no arrest to a court yet (that is the killing's gap too), the guards fight.

Complete the theft left out of P9: pickpocketing, the other places a lock or a shelf is robbed, what a failure costs, and a way to do it from the keyboard. P9 already has a shop's Steal button and shoplifting, a street door's lockpicking (the Steal mode or the run key held), `CRIME_THEFT` and the court.

### What classic does (UESP; Daggerfall Unity's PlayerActivate and FormulaHelper as the cross-check)
Steal is one of four interaction modes (Steal, Grab, Info, Talk; F1 to F4 in classic, one button on the bar). What the use does in Steal mode:
- **A street citizen** (not one inside a building): a pickpocket attempt within reach (DFU: 128 units). Chance in 100 is the live Pickpocket skill, clamped 5..95. Every attempt tallies Pickpocket. Success: a 67% chance of 1 to 6 gold, else a line saying nothing of value was found. Failure: the crime is Pickpocketing and the guards come (DFU's `SpawnCityGuards`).
- **A human enemy** (a class enemy, not a monster): the same, with `+ 5 x (player level - their level)` before the clamp. Failure turns them hostile (and the ones near them).
- **A shut building's street door:** a lockpick attempt, built (`door_lets_in`); the lock is half the building's quality, chance `skill - 5 x lock`. Missing: a failed pick's chance of alerting the guards (UESP: `33 x (1 - stealth%)`).
- **A dungeon door** that is not magically locked: a pick, chance `skill + 5 x (level - lock)`; a lock of 20 or more cannot be picked.
- **A closed shop's shelves:** the stealing screen, chance of being seen `100 - Pickpocket + quality + weight and count`, clamped 5..95. Built.
- **A house's container:** an owned property's loot is taken without a prompt; any other asks "private property, enter?" first (DFU). Waits for houses to have containers.
DFU lets each person be tried once while loaded (its code says classic allows endless tries; not confirmed, so one attempt, which also stops the mode being a tap of gold).

### One intent, two ways to say it
Everything above runs when the player means to steal: `g_stealing` in `game.c` (today the Steal mode or the run key held). The plan keeps that one flag and gives it two schemes, so the rules are written once:
- **Point and click** (the Cursor scheme, the bar on; "low reflexes"): the mode button picks Steal, and a click on a citizen, an enemy, a door or a shelf is the attempt, as classic. Info does the looking. Nothing new for the player to learn.
- **Keyboard** (View scheme, bar off): the use key E is the one verb, and what it does depends on how the player stands:
  - standing, E on a creature or a citizen says what is seen (Info); on a door, a body or a heap it does what it does now;
  - crouched, E is Steal: on a citizen or a human enemy a pickpocket, on a shut door a lockpick, on a closed shop's shelf the stealing screen;
  - an attempt is only ever made on E, never for walking up crouched, since an accident would be a crime.
  Crouching also keeps Steal's flag for the bar's own mode, so a player who uses both is never in two states.

### What each needs
- **Crouch is an engine matter.** `docs/collision.md` says there is none: a body has one height and eye. It needs a lower height and eye the game can set, standing again only where the head has room (the walker's ceiling test), and the game owns the speed (`player.c`) and the key (`INPUT_CROUCH`, hold or toggle, a bind slot and an option). Asked of the ENGINE implementer (2026-11-04). Whether crouching also helps Stealth (as DFU's sneaking) is to check, not assumed.
- **Pickpocketing** a citizen: `people_in_view` already gives who and the sex; a small set of the citizens tried this visit (forgotten when the place is, as DFU's flag is) needs no save. Failure is a Pickpocketing crime (CRIM row 12; `commerce.h` names only `CRIME_THEFT` yet) and the guards hostile through `watch.c`; a town's guards already spawn from its people pool (`people_spawn_guard`), so that is what slice 1 uses; creating creatures where the pool is not (the quests' road ambush, the wild) is the engine matter the quests wait on.
- **Human enemies** need an aim at a creature (`combat_ray_box` and `combat_reaches` exist), their level from `combat_foe`, and one try each by their keep key; failure wakes them hostile through `foes.c`.
- **Info on a creature or a citizen:** a line with the kind's name, as Info says a usable's; DFU and classic say little more.
- **Dungeon doors** need the usable's lock strength from the engine's use ray; a dungeon's locks have a level the RDB gives.

### Slices, each landing alone, tested and documented
1. **Pickpocket a citizen (built)**, from the bar's Steal mode and the point-and-click scheme: `rules` (the chance, the gold roll), `theft.c` (the tried set, the attempt), the crime and the guards, Pickpocket's tally. Tests: the chances by hand at the clamps, gold 1..6 at 67%, one try a citizen, a failure's crime to the court, through a save.
2. **A human enemy (built):** the level term, gold or hostility. Tests: the 45 and -20 of DFU's own example are `check_theft`'s sums.
3. **Locks and shelves (built):** the alert chance on a failed pick, the dungeon door's formula and the 20 limit.
4. **The keyboard scheme:** the crouch key, hold or toggle, and Steal on E are built (above); Info on E for creatures is built with slice 2.
5. **Containers in houses (built):** the furniture was already in the interiors; the question, the loot and the theft are above.
The run key held while using a lock stays until crouching can replace it (open question).

**Done when:** Steal can be chosen from the bar or done crouched on E, successful theft and pickpocketing transfer the intended gold and items, failed or seen attempts follow the law, and the chances, the memory and the saves are tested.

## Magic (P12-b: investigated and planned 2026-11-06)

Sources: Daggerfall Unity (FormulaHelper's cost and saving-throw sums, EntityEffect's rolls, EntityEffectManager's cast flow, the spellbook, spell maker and effect windows, DaggerfallMissile), UESP's SPELLS.STD indices and Spell Maker pages. DFU's effect files were read, not copied; every number below is also a row of MAIN.DAT, so a mod can change it.

### What classic does

- **A spell** is up to three effects, one target, one element, a name and an icon. **Targets:** the caster, touch, a single target at range, an area around the caster, an area at range. **Elements:** magic, fire, cold, poison, shock. Each effect is a group and a subgroup (Damage / Health), has up to three parameters it supports, and belongs to one of six schools (Alteration, Destruction, Illusion, Mysticism, Restoration, Thaumaturgy) whose skill lowers its cost.
- **The parameters:** duration (a base and a plus per so many levels), chance (the same), magnitude (a base range and a plus range per so many levels). A roll at the caster's level: duration `base + plus x max(1, level / perLevel)` rounds; chance `base + plus x (level / perLevel)` percent; magnitude `rand(baseMin..baseMax) + rand(plusMin..plusMax) x (level / perLevel)`. **A magic round is one game minute**, so duration comes out of the clock, and rest and travel pass rounds.
- **The cost:** every effect has costs per parameter, (offset gold, A, B): `gold = offset + A x start + B x (increase / perLevel)`, the magnitude's start and increase being the middles of its two ranges. The effect's gold is the sum, its spell points `gold x (110 - skill) / 400` (the school's skill), and a spell's cost is the sum of its effects' times the target's multiplier (1, 1, 1.5, 2, 2.5), at least **5**. An effect with no parameter (Teleport) costs as 1/1/1 of (60, 100, 160). The Spell Maker charges the gold, no skill in it.
- **The effects** (the classic group and subgroup, which SPELLS.STD stores, are in `tables/magic.c`): damage, continuous damage and drain of health, fatigue, spell points and the attributes; transfer; heal, fortify, regenerate; cure disease, poison, paralysis; paralyze, silence, disintegrate, charm, soul trap; dispel daedra, magic, undead; shield, the five resistances, spell resistance, reflection, absorption; chameleon, invisibility, shadow, light; levitate, slowfall, water breathing, water walking, jumping, climbing, free action; lock, open, teleport, create item; detect enemy, magic, treasure; identify; comprehend languages.
- **Saving throw** (DFU, already in `ills.c` for diseases): 50, +30 resistant, +25 tolerant, -25 or -50 low or critical, plus Willpower / 10 (a spell's `MagicResist`), clamped 5..95; a roll at or under it saves, and within 20 of failing the effect is cut by 5 for each point missed. An effect that was given a resistance (the spell on the caster) is tried first by its chance.
- **Casting:** a spell is chosen in the book and is then ready ("Press button to fire spell"); the spell points are taken when it is cast, a caster-only spell at once, any other with the casting hand (the element's picture, `FIRE00C6`, `FRST00C6`, `MJIC00C6`, `POIS00C6`, `SHOK00C6`), whose release frame sends the missile. A touch spell needs a target within 3 m. A missile flies at 25 m/s, 8 s at most, 0.45 m wide; an area spell bursts at its end within 4 m. No cost check but spell points; silence forbids casting.
- **The book:** SPBK00I0.IMG, 259 x 164: the list, delete, up, sort and down, the spell's icon, target, element and its effects in three panels; ICON00I0.IMG is 69 icons 16 x 16 in rows of 20, MASK04I0.IMG the targets (24 wide) and elements (16 wide). **The maker** is INFO01I0.IMG and the effect editor MASK05I0.IMG (spinners: duration 1..60, 1..60, 1..20; chance 1..100, 1..100, 1..20; magnitude 1..100 four times, 1..20), reached at a Mages Guild's "Make Spells". "Buy Spells" (a Mages Guild, a temple of Kynareth) is the book's screen in buy mode (SPBK01I0.IMG) over the standard spells the guild sells.
- **Starting spells** by class (SPELLS.STD's `index`): Mage and Sorcerer 1, 37, 2, 97, 8; Spellsword 8, 44, 37; Battlemage 8, 2, 37; Healer 97, 2, 1, 37; Nightblade 44, 2; Bard 37; a custom class with any magic skill primary or major the Spellsword's.

### The files (read in place)

`SPELLS.STD`: 89 records of 89 bytes: 3 x (effect type, subtype) signed bytes, element, target, cost (u16), 4 unused, 3 x duration (base, plus, per level), 3 x chance, 3 x magnitude (base low, base high, plus low, plus high, per level), the name (25 bytes at 47), the icon at 72, the spell's index at 73, 15 unused.

### The plan, in slices that each land alone, tested and documented

1. **The projectile (built, host-tested: the bow's arrow is its first user).** `play/projectile.c`: a point that flies along a direction at a speed, gravity optional (arrows fall a little), stepped each frame against the walls (`combat_ray`), the creatures of the stream (the same box test `aim_creature` uses), the people of a street and the player; it ends on a hit, at its range, or on a wall, and says what it hit. Pure C over callbacks, so hostcheck drives it on a fake world. The picture is a billboard entity from the engine's spawn pool (`spawn.h`), an arrow's or an element's, with its burst on impact. Arrows stop landing at once: they fly (docs/game.md, Combat's "an arrow lands at once" gap).
2. **The data (built, host-tested).** `formats/spells.c` reads SPELLS.STD, `tables/magic.c` is `EFCT` (90 effects: group, subgroup, school, which parameters, costs, targets, elements; classic's code through DFU) and `SPEL` (the 89 standard spells), cooked into MAIN.DAT as the other tables are (MAIN.DAT's version 17; SPELLS.STD is a source of the stale check). `play/spells.c`: a spell, its cost in gold and spell points, and the book (no two of a name, sorted, removal, the starting sets by class), tested by hand: Shock is 256 gold, 70 points at Destruction 0, 38 at 50, 6 at 100; Wizard's Fire 324 gold and 88 points; Fireball 330 gold; Fenrik's Door Jam 288 gold and 79 points; Recall 320 gold as an effect of nothing to set; no spell under 5 points. **SPELLS.STD's own price (bytes 8 and 9) is not the cost** (Shock's 225 is not 256, Levitate's 50 is not 220): it is classic's shop price, kept as `price`. Three facts the file corrects in DFU: Jumping is effect 32, the dispel effects reach others as well as the caster (Holy Touch, Null Magicka are touch spells, Holy Word an area), and Buoyancy is a cold spell of water walking.
3. **Effects in play (built, host-tested): `play/effects.c`.** What a spell does when it lands, by DFU's AssignBundle: each effect rolls its chance (a miss says "Spell effect failed." on the caster's own spell, "Save versus spell made." on another's), an effect on another with no magnitude (paralysis, silence) is saved against whole, the duration is `base + plus x max(1, level / per level)` rounds, the magnitude `low..high + (low..high) x (level / per level)` cut on another by the saving throw's share. The first round is at once and counts (three rounds of continuous damage act three times); an effect of one round is in force until the next minute turns. **The saving throw** (`effects_player_save`): 50, the race's and the class's resistance to the element (+30, +25), low tolerance and weakness (-25, -50), Willpower / 10, held to 5..95, 100 for an immunity (nothing lands); a roll at or over the save lets all through, at or under within 20 lets 5% less a point nearer, further under none (`effects_save_percent`); a creature's is 50 and its Willpower / 10; a live elemental resistance first rolls its chance to turn the whole effect away. **Built:** damage to health, fatigue (raw 64ths, a magnitude is whole points) and spell points, heal (health, fatigue), transfer (health, fatigue), continuous damage and regeneration, paralysis (a creature's mind is held still, `foes_freeze`; the player's is a live effect), silence, the five elemental resistances, free action (proof against paralysis), slowfall, water breathing, the three cures (disease by `ills_cure`; poison has no state yet; paralysis frees), dispel magic. **Recorded only** (kept while they last, nothing reads them yet): light, levitate, water walking, jumping, climbing, invisibility, chameleon, shadow, shield, spell absorption, reflection and resistance. **Not built** (a cast says "This magic is not worked yet."): drains, fortifies, transfers and heals of attributes, the dispels of undead and daedra, disintegrate, soul trap, charm, calm, lock, open, teleport, detect, identify, create item, comprehend languages. A creature a spell kills lies down as a blow's kill does (`combat_creature_hurt`, which shares `settle` with a blow). Tests: the share by hand, every vital, the level's steps, rounds counted, a pass of minutes, dispel, resistance against fire and not cold, free action against a hundred paralyses, a killing spell said once, and a creature of the hold held, freed and killed.

4. **Casting (built, host-tested; none of it run on the target).** `play/cast.c`: a spell is made ready (from the book, or the cast key, `Q`, which readies the last one cast and puts a ready one away), and **the attack button casts it** while one is ready: the weapon is not drawn, the name stands under the cross (`overlay_set_ready`). Silence forbids ("You are silenced."), too few spell points forbid ("You don't have the spell points."); a touch spell with nothing within 3 m is not cast and costs nothing. The cost is paid when it goes and the school of each effect that took hold is trained. By target: the caster at once; touch at once on the creature the view rests on within 3 m; at range a missile (`projectile.c`: 25 m/s, 8 s, no gravity, started 0.9 m out) that lands on the creature it meets, a wall or the end of its range landing nothing; an area round the caster, every living creature within 4 m with no wall between, at once; an area at range, a missile that bursts where it ends (a creature, a wall, its range). A burst never reaches the player. `shots.c` steps the missiles with the arrows and hands each spell's end to `cast_arrived`. In `game.c`: the minute's rounds (`effects_round`, and `catch_up_minutes` for the time a screen, a journey or a training takes), slowfall (no landing damage), water breathing (no breath spent), paralysis (the controls zeroed, the body still falls), a killing spell (`effects_died`). The console: `spells`, `learn n|name`, `forget n`, `ready n|last`, `effects`. The book is `g_book`, filled by `spellbook_starting` at a new game and saved (`SPBK` rows, one per spell, flat arrays; `MGIC` rows, one per lasting effect on the player, in `save.c`, through `save_use_book` so no caller has to carry the book; creatures' effects are not saved). **Seen:** the casting hand (`play/castfx.c`): the element's file (`FIRE00C6.CIF`, `FRST00C6.CIF`, `POIS00C6.CIF`, `SHOK00C6.CIF`, `MJIC00C6.CIF`, six RLE frames each, 49 x 48 at the bottom growing to 132 x 200; `cif.c` reads only the weapons', so the hand has its own small reader) plays in 0.5 s over the view and the spell goes at its fourth frame; a spell on the caster goes at once with no hand, as DFU's instant cast. The missile is a billboard of the engine's spawn pool (TEXTURE.375 fire, 376 cold, 377 poison, 378 shock, 379 magic; record 0 in flight at 5 frames a second, record 1 as a 0.6 s burst at 15 where it lands; `shots.c`), made resident once a stream with `wres_acquire`; where no stream is open the spell flies unseen. An arrow has no picture. The hand's pace and release are this slice's guess (DFU's are not read). Tests: ready (points, silence), the caster, touch in reach and out, at range (the points paid at the cast, the creature not struck at once, then struck on arrival), an area at range, an area round the caster, each killing a creature of the hold, and the book and two lasting effects through a file.
5. **The spellbook screen (built, host-tested).** `play/spellbook.c` on SPBK00I0.IMG (259 x 164 at (23, 18)), as `logbook.c` is built (a `Composer`, composed when it changes, `screens.c`'s OPEN_SPELLBOOK): the list (13 rows of 10), a click chooses and a second click on the chosen spell readies it (the screen closes with `SCREENS_SPELL` and `game.c` calls `cast_ready`), Enter readies, the arrows, the wheel and Page Up and Down scroll, the Up and Down buttons move the chosen spell in the book, Sort puts the book in name order and keeps the same spell chosen, Delete asks once more as the shell's Exit Game does (another control cancels it), Exit or Escape closes. The right side: the chosen spell's name, its icon (ICON00I0.IMG: 69 of 16 x 16, a headerless 320 wide, 20 to a row), target and element (MASK04I0.IMG: targets 24 wide then elements 16 wide, five rows each), cost in spell points for this character, and each effect's name and parameters. Opened by the HUD bar's Spellbook button, `B` (`INPUT_SPELLBOOK`) and the character sheet's Spellbook button (69, 151). **Buy mode** (`spellbook_open_buy`, SPBK01I0.IMG): a Mages Guild's or temple's Buy Spells person (anyone may buy; the building's quality bargains) lists the standard spells but SPELLS.STD's "!" ingredients, by name; the price is four times the spell's cost in spell points for this character, bargained by `rules_trade_price` with the player's Mercantile and Personality (DFU's presented cost); the Delete button is Buy, a second click pays; a spell the book has is refused and the gold kept (DFU sells duplicates, a book holds one of a name here). The Witches' Festival's half price is not applied yet. Tests drive it by clicks and write `build/spellbook0..1.ppm`.
6. **The spell maker (built, host-tested).** `play/maker.c` on INFO01I0.IMG and, for an effect's parameters, MASK05I0.IMG, with DFU's rectangles (the slots, Add Effect, Buy, New, Exit, the target and element columns, the icon arrows, the name line, the spinners: duration 1..60, 1..60, 1..20, chance 1..100, 1..100, 1..20, magnitude 1..100 four times and 1..20). Add Effect lists the groups on a scroll (the effects built or recorded; the rest wait until they are), then the subgroups when there are several, then opens the editor with DFU's defaults (every parameter 1); a spinner works only for a parameter the effect supports, and a magnitude's low and high drag each other. The targets that light are those every effect allows together (an effect that would leave none is refused: "That cannot be joined with the other effects."), the elements those any allows with magic always; the costs shown are `spell_cost`'s, the gold with no skill in it. The name is typed after a click on its line (Enter ends it); Buy needs an effect, the gold and a name, refuses a name the book has, charges the gold and inscribes the spell; New clears; Delete or Backspace on the main screen takes the slot last touched out. At a Mages Guild the Make Spells person opens it for a member of any rank (DFU's MagesGuild: IsMember). Tests drive it by clicks and keys and write `build/maker0..3.ppm`. **Not yet:** the effect descriptions of TEXT.RSC 1500 and on; the buy mode's Yes and No box and the Witches' Festival's half price. The Keybindings screen lists Crouch (it was left out of its 22 rows), Spellbook and Recast spell.
7. **Creatures that cast (built, host-tested).** DFU's lists (`ESPL`, a MAIN.DAT table, `tables/magic.c`, `magic_foe_spells`): an imp, a ghost, an orc shaman, a wraith, the daedra (frost, fire, daedroth, seducer, lord), a vampire and an ancient vampire, a lich and an ancient lich each their own spells; a **class enemy that casts magic** (the Mage, Sorcerer, Battlemage, Spellsword, Healer, Nightblade and Bard, `casts_magic` in ENEM) the list of its level / 3 (frost daedra's, daedroth's, orc shaman's, ancient vampire's, daedra lord's, lich's, ancient lich's, the last from level 18), at the player's level. A creature's mind (`foes.c`) picks its usable spells the first time it thinks of casting (those with an effect built; ranged and touch ones apart; a spell on the caster alone it would only waste), keeps spell points (10 a level and 100, a cast costs what it would at skill 80 in each school as DFU sets them) and casts while it has any. **When:** a touch or area-round spell in place of a blow, the player in reach and nearly ahead and its timer out; a ranged one from 6 to 51.2 m, the player seen and nearly ahead, one tick in 40 (EnemyMotor's rates); its cast pose (the attack's rate, five frames) and the spell leaving on the fourth; struck mid-cast it flinches and the spell is lost. **What:** `cast_foe`: a touch lands at once, an area round bursts at it if the player is within 4 m and in sight, a ranged one or an area at range flies (`PROJ_FROM_FOE`, tested against a box about the player) and lands on the player or bursts where it ends; the effects land through `effects_land_from_foe`, the creature's level standing for the caster's, the player's saving throw and elemental resistances as for any spell. A spell that kills says so (`effects_died`, read after the creatures' frame). A creature has no effects of its own on it beyond paralysis and continuous damage, so a spell on its own caster does nothing and is not chosen. Tests: the lists by kind and level, and a lich of the hold made to know the player 3 m off that cast at them and wounded them. **Citizens and guards as targets:** an arrow and a spell reach the street's people as a blow does (`combat_aim_person_at` beside `combat_aim_at`: the nearer of the two is what a missile meets, a touch spell strikes, and a burst finds every one within 4 m with no wall between). An arrow strikes by `combat_blow` (a citizen's 1 health falls to any that lands); a spell's damage is whole (`combat_person_hurt`: a citizen falls to any, a guard has the watch's health, the saving throw 50 and a creature's Willpower rule applied as for a creature). A death by either is a killing in the street: the watch is told (`watch_on_kill`, the guards come, wanted is set) and a lycanthrope's innocent is killed, from the player's eye and facing, as a sword's kill does. A wound that does not kill tells no one, as a sword's does not. **Not yet:** creatures casting at each other or at townspeople, healing themselves, a creature's spell absorption or reflection, the spell sounds.
8. **Light (not built; a possible improvement).** Nothing a spell does gives light: its missile and burst are billboards, and the Light effect is recorded and shown but lights nothing. DFU's missile carries a light of its element's colour that lights the way as it flies, and a lit player (Light, Mage Light) lights what is near. The engine's lights are fixed at a place's load or a stream's chunk (`light.c`); a light that moves each frame is its own engine request, with a cost the 233's frame must be measured for.

## Places and travel

`places/travel.c` (built: P8) is a small state machine of three kinds of place: **world** (outside, the whole map, `wild.c`), **inside** (one building, loaded whole, where it stands), **dungeon** (streamed by block, `dunstream.c`). Every move between them goes through one passage, kept as a `Usable` (`doors.h`: `DOOR_ENTER`, `DOOR_LEAVE`, `DOOR_DUNGEON`, `DOOR_EXIT`).

A move happens between two frames:
1. **The use key meets a passage.** `on_use` calls `travel_use`, which finds where it leads while the place is still open: the town's numbers from the door's world point (`wild_location_at`), the dungeon under a town or the outside over a dungeon from the location's id (`dungeon_under`, `dungeon_outside`, one region's records). A way that leads nowhere says so and nothing happens.
2. **`game_frame` returns `GAME_TRAVEL`.** `enter_place` in `apps/gfall/main.c` holds the frame under "Loading...", then `travel_leave`, sets the far clip for the place (`WORLD_FAR` 8192 outdoors, `DF_FAR` elsewhere), `travel_open`, `gfx_prepare`, `travel_land`, `game_arrive`, `gfx_prime`, the fade-in. A new game or a save takes the same path with `game_enter`, which makes the player first.

Rules a later edit keeps:
- A place's data lives in its own temp heap (`docs/memory.md`), `travel_open`'s to begin and `travel_leave`'s to end. What the player carries across (the sheet, the items, the quests later) is kept with `mem_keep` or in the game's own block, never in a place's heap. Pictures on the screen have `pic.c`'s own heap.
- **The way back is a world point:** the step outside the door taken, three body radii out, facing away from it, kept in `TravelWhere` as doubles. Up from a dungeon or out of a house opens the stream there (`wild_open_at`), so going out lands at the same door. A hold first met from within has none: its outside's pixel opens, the camera at the town's middle, and the nearest way down is the landing.
- **Entering a dungeon from outside lands by its way out** (`DOOR_EXIT`), not at the start marker: the hold's start is the cave at its bottom. A body stood at each of a sample of the map's dungeons' ways out finds footing (`check_every_way_out`, every 197th).
- **A house that does not read puts the player back on its step** ("It will not open."), not out of the game.
- **A stream's origin move** (past `STREAM_REBASE` chunks from it) shifts the scene and the camera. What the game keeps in the scene's frame is its own to move: `travel_update` hands back the shift, and `game_frame` gives it to `player_shift` and `foes_shift`.
- **The budget:** a world's chunks are heavier than a dungeon's blocks, so `game_frame` gives the stream what is spare before the next retrace, as LEVELFALL does (1 to 8 ms; 3 ms when vsync or the calibration is not there).
- The season turning or the world's meshes filling up reloads under "Loading..." from the program's loop (`travel_must_reload`).

The game starts in Privateer's Hold at its enter marker, the cave by the campfire at the bottom of the place, as Daggerfall does (`docs/dungeons.md`).

Tested on the host (`check_travel`): a new game in the hold; out to the step by its way down; standing and walking on the land; no blow and nothing to search there; down by that door and landing by the way out; a door opened in the hold kept across the world; up at the same door; a save in the world read back at the same point with the same way back; an origin move at 17 chunks leaving the player where they stand in the world; a house in Daggerfall and out at its door, with combat, creatures and loot run harmlessly in a place with no stream.

**Not yet:** run on the 233, where the budget and the load times are to be measured. Saved towns' doors and people are not keyed (see below). Treasure and dropped piles only exist in a dungeon streamed by block, so nothing can be dropped outdoors or in a house. The sea can be walked on.

### What the player changed (built: the keys)
Every door, lever and creature of a place loaded whole is keyed for the engine's state table (`keep.c`, `docs/interact.md`, State that outlives a load). A key is two words:
- **place:** the location's id in the low 24 bits; a building's record and one in the next 6; the kind in the top 2 (1 a dungeon, 2 a town, 3 a building's inside). A place without an id keeps nothing.
- **thing:** the block's index << 24, and the record's offset in the block's file: an action record's for a mover, a marker's for a creature. An RMB model has no offset, so its thing is 0x800000 and its index.

`world.c` puts back what was kept at the end of every load (`world_keep_restore`); `world_keep_save` writes it before a place is left. A creature kept dead is never read or drawn (`FOE_DEAD`). The engine's `live` means drawn, so a creature not yet read is saved as alive.

Tested in the hold: all 105 things keyed, no key twice, a door and a lever left open come back open after a reload, a door shut again loses its row. GFALL leaves places since P8, and a house's and a dungeon's rows go with them. The streamed world's towns are not keyed yet (their chunks).

### A dungeon streamed by block (built: the source)
`places/dunstream.c` is the chunk source of a dungeon streamed by block (`docs/streaming.md`, Dungeons): each RDB block a chunk of 2048, by its place in the layout, the start block the origin.
- **Each block's chunk** holds what a whole load holds of it, by the same rules:
  - its models, painted by the dungeon's picture table (`swap_dungeon`, now shared with `world.c`);
  - its flats at their centres, its lights flickering by their flames, its sounds;
  - its doors, levers and teleports (`doors_build`, keyed);
  - its creatures, on their floors, keyed;
  - its water.
- **Pictures and meshes** go through the session's registry (`wres.c`), with a dungeon mesh variant (`wres_meshes_dungeon`, `wres_paint_by`).
- **Creatures are drawn at open, in the blocks' order.** A random marker's slot 0 and a class's gender take classic's rand, so the order decides who stands where. At open the source reads every block's markers alone (`rdb_markers`: the block's bytes, no models) and draws each creature as a whole load would; a block coming in places its creatures from that table.
- **Kept state** is the stream's to save and put back. A creature kept dead is hidden again in `changed` (live is "drawn").

**Tested against the hold loaded whole:** the ring round its start holds all of it (365 models, 130 flats, 71 lights, 63 movers, 42 creatures, as the whole load). Each creature is the whole load's, keyed and standing alike. A door opened, its blocks dropped 8 blocks off and come back, is open.

**GFALL plays every dungeon through it** (`travel.c`, `enter_place` in `apps/gfall/main.c`): `dunstream_open` under "Loading...", `gfx_prepare` of the streamed level, the player at its start, `dunstream_fill` (the ring and its lights cut, shadows and all), `gfx_prime`, the fade-in. Each frame `game_frame` steps the stream (`travel_update`), and the blocks' creatures come with them. Tested on the host: the player stands on the streamed hold's floor and walks classic's distance. On the 233 it plays at a good frame rate. Its first run there found the body going through walls, ceilings and floors in places (a wall's end after a slide, a jump's rise, the legs in the air, a ramp, a step with no headroom); the engine's collision fixes (`c98b0c3`, `docs/collision.md`) cured them, confirmed on the 233. The `load:` time and the 3 ms budget are still to be measured there.

**Not yet:**
- LEVELFALL's indoor rows;
- a block's meshes parsed in steps (a block is read in one step today).

## The player

### The body
Daggerfall is first person: the player is the camera. The physics is not the camera's, though. It is an engine **body** (`body.h`): 72 units tall, a 24-unit step, gravity and stairs, which the game steps each frame with the motion it wants. A first-person rig then puts the camera at the body's eye. A third-person game would instead draw an entity at the body and put the camera behind it with a third-person rig; the engine allows both, and this game uses the first.

Entities have no physics of their own. A creature that only stands, or walks a flat town cell, needs none. A creature that chases the player down a stair gets a body of its own size, and the game copies the body's place to the creature's entity each frame.

With head bobbing on, a walk on the ground moves the camera off that eye (`viewopt_eye`, `player_camera`). Standing, swimming, flying and a jump do not. The wave is Daggerfall Unity's HeadBobber: along the body's right by cos(phase) × 0.045 m walking or 0.09 m running, and up by |sin(phase)| × 0.062 m walking or 0.11 m running, plus that file's nod. Phase advances by the horizontal speed in metres a second times 1.25, and rests at π/2. The setting off leaves the eye on the body.

The game adds, as rules on top of the player's body:
- **Run**: speed from Speed and the Running skill.
- **Jump**: height from the Jumping skill.
- **Fall damage**.

**Built (P7, `play/player.c`).**
- **Swimming** (PlayerSpeedChanger.GetSwimSpeed): when `scene_water_body` says the eye is under, there is no gravity, and the player goes along the view at a walk's quarter and Swimming / 200 of it more; jump rises. Each game minute under is a use of Swimming. A place with water is not played yet: it is tested with water laid over the hold's floor.
- **Breath** (DaggerfallEntity.MaxBreath, PlayerEntity): half of Endurance, a point each 19 classic ticks, an Argonian keeping it half the time. None left drowns, at once. It is full again at each dive.
- **Fatigue** falls each game minute, in its 64ths (PlayerEntity): 11 about, 88 running, 44 swimming on a failed roll of the skill, which an Argonian does not make; 11 for a jump. Only rest gives it back. The minutes are counted by `game.c` from the game's own clock; time a screen passes (rest, prison, the travel map's countdown) is skipped, as that screen already gave back or took what those hours do (walking them again once emptied fatigue after a night's rest, and killed with a creature near). `tgm` stops fatigue falling and breath running out too.
- **Exhaustion** (PlayerEntity_OnExhausted): at none, with a creature awake or in the water, dead. Otherwise the player drops: an hour passes, an hour's rest comes back, and Medical is used.

**Built (P1, `play/player.c`, `play/rules.c`).**
- **The numbers are classic's, as Daggerfall Unity's PlayerSpeedChanger and AcrobatMotor have them:**
  - walking (Speed + 150 − drag) / 39.5 m/s;
  - running (Speed + 150) / 39.5 × (1.35 + Running / 200);
  - a jump's height, 4.5 m/s × (1 + Jumping / 200) rising under 20 m/s².
- **Until P2 the character is a level-1 one:** Speed 50, Running 20, Jumping 20 (`PLAYER_*`). So a walk is 4.43 m/s, 177 units a second.
- **Forward and sideways at once go no faster than one** (DFU's limitDiagonalSpeed).
- **The player falls at classic's 20 m/s²** (`Body.gravity`, DFU's AcrobatMotor), not the engine's default 9.8. So a jump is classic's in height and in time in the air.
- **The landing is taken but does nothing yet:** fall damage comes with the vitals (P2). Swimming comes when a place with water is played.

### The sheet
Daggerfall's character, per UESP (Character Creation, Attributes, Skills, Leveling):
- the eight attributes, 0 to 100: Strength, Intelligence, Willpower, Agility, Endurance, Personality, Speed, Luck;
- the 35 skills, with the class's 3 primary, 3 major and 6 minor ones;
- race, class, level and experience;
- the three vitals (health, fatigue, spell points) and their maximums;
- reputation, then factions and legal standing later.

The derived values (the vitals' maximums, carrying weight, the bonuses) are functions in `rules`, each citing its source. The sheet's layout is ours.

Skill advance by use and the level-up rule come from UESP's Leveling page. A pre-made character comes first, and the creation screens come later.

**Built (P2, `play/character.c`, `play/rules.c`, `tables/careers.c`).**
- **Where it comes from:** classic builds a character from its **class**, not its race.
  - MAIN.DAT's `CLAS` table is cooked from ARENA2's own CLASS00 to CLASS17.CFG (`formats/classcfg.c`): base attributes, the 3 primary, 3 major and 6 minor skills, hit points a level, spell points an Intelligence point (bits 10 to 12 of its abilities: 3, 2, 1.75, 1.5, 1 or ½), resistances.
  - `RACE` holds DFU's RaceTemplate: names, the paper doll's pictures, what each race resists. Bretons resist magic, Nords frost, and High Elves are immune to paralysis; the other racial gifts come with magic.
  - `SKIL` holds the 35 skills, each with its governing attribute and how hard it is to advance.
- **The rolls (DFU's StatsRollout and SkillsRollout):**
  - an attribute is its class's base and a roll of 0 to 10;
  - a primary skill is 28, a major 18, a minor 13, each and a roll of 0 to 3; any other skill is 3 to 6.
  - Creation rolls them (Creation, below). Skipped, the game takes the rolls' middles (`RULES_*`), and starts an **Argonian woman, a warrior** (`CHARACTER_START_*`): strength 65, speed 52, health 45, fatigue 120, 24 spell points. A class sets these numbers, so a Breton warrior has the same ones, and the tests' sums worked by hand stay a Breton's.
- **The vitals (FormulaHelper, DaggerfallEntity):**
  - health is 25, plus the class's hit points a level, plus a roll each later level;
  - fatigue is strength and endurance, kept in 64ths;
  - spell points are intelligence times the class's multiplier, rounded down.
- **Movement:** the walk, run and jump now come from the sheet's Speed, Running and Jumping.
- **Collision and `tcl`:**
  - The game hands the engine its player's body (`walker_set_game`): the engine's own walker is never stepped in a game, and creatures, other bodies and `stuck` take the player's body.
  - The console's `tcl` is the game's collision switch (`walker_on`), on by default. `tcl 0` flies through everything, Oblivion-style: along the view, pitch and all, Q up and E down, at the walk's and run's speeds, with no gravity. Back to `tcl 1`, the body comes out of whatever it is in and falls from there.
  - Before this, `tcl 1` stood the engine's walker body on the player's, and the two shoved each other: sideways, through walls.
- **Teleports** are the game's to take: each frame, before the step, `interact_teleport` puts the body where the last frame's teleport sends it, at its eye, facing its way.
- **A fall** past 5 m costs 5 health a metre (PlayerHealth); "That hurt." A fall to 0 health says "You have died." and shows the death screen (Saves, below).

### Creation (built: P6)

`play/create.c`, in the order of Daggerfall Unity's creation windows, at their rectangles. Each is classic's own picture, composed in memory like the inventory.

| Step | Picture | What it decides |
| --- | --- | --- |
| Province | TMAP00I0.IMG, read under ART_PAL; a click looks up TAMRIEL2.IMG, whose every pixel is a race | the race, its TEXT.RSC words to confirm (`textrsc.c`), and DAGGER.SND's clip for that race (`CareerRace.clip` is the record id, `sndfile_by_id`) |
| Sex | a scroll, with BUTTONS.RCI's Female and Male, and the race's two bodies drawn at two fifths of the screen (`canvas_scale`) | a woman or a man; a click on a figure is that button |
| Method | BUTN01I0.IMG over the map | the list, or the questions |
| Class | PICK00I0.IMG, nine rows and its arrows | one of the 18, or Custom last; its words (2100 and on) to confirm |
| Questions | CHGN00I0.IMG, under its own palette | ten of TEXT.RSC record 9000's forty, none twice, answered by A, B, C or a click; each answer goes warrior, rogue or mage by DFU's table, and CLASSES.DAT's table (from byte 18, threes of counts, four to an entry) says what class the counts make: ten warrior a Knight, ten mage a Mage. The picture's stars are palette indices 192, 160 and 128 (warrior, rogue, mage). They start at blue 8 and each answer of that way adds 24 (`create_constellation_blue`), which is how classic lights a leaning |
| Custom | CUST00I0.IMG, CUST08I0.IMG's dagger | a name; twelve skills from a list, none twice; hit points 4 to 30; attributes from 50 moved between 10 and 75, the points given up placed elsewhere. Hit points over 8 add to its hardness, under 8 take off twice that; advancement is 0.3 + 2.7 × (hardness + 12) / 52. Spell points ½ an Intelligence point, an iron longsword, and the background questions of the listed class sharing most skills |
| Background | BIOG00I0.IMG | twelve questions from `BIOG##T0.TXT` (`biog.c`): skills, gold, weapons and armour. An answer that moves a social group says so, in TEXT.RSC 35's words: "Merchants: higher", "Commoners: lower" |
| Name | CHAR00I0.IMG | typed; each word begins with a capital |
| Face | CHAR01I0.IMG | one of the race's ten, and it shows on the paper doll |
| Attributes | CHAR02I0.IMG, CHAR02I1.IMG's spinner | class base + 0..10, then 6..14 points to place; Reroll; the derived numbers shown beside classic's words. Shift (the run key) on the spinner places that attribute's whole remaining pool |
| Skills | CHAR03I0.IMG, CHAR03I1.IMG's spinner | primary 28, major 18, minor 13, each + 0..3, then 6 points a group to place. Shift on a group's spinner places that group's whole pool in the chosen skill |
| Reflexes | CHAR05I0.IMG, CHAR05I1.IMG's slices | very high to very low |
| Summary | CHAR04I0.IMG | looked over; Restart or OK |

- **Leaving.** Escape on the initial race map cancels creation and returns to the start menu. It does not make a character and it does not enter a place. Escape during the questions answers the rest at random, as classic's skip does. The default Argonian woman is only when creation's pictures did not open.
- **The kit** (`play/kit.c`, DFU's AssignStartingGear): the class's weapon in hand, and 100 gold. A Warrior's is an iron broadsword, so the default character no longer starts with a longsword. An Archer also has a steel battle axe and 24 arrows.
- **Scrolls and buttons** are classic's on every screen (`canvas_scroll`, `canvas_button`): SPOP.RCI's nine 22 × 22 pieces, as DFU's parchment style, and BUTTONS.RCI's 32 × 16 buttons. The class questions stand on a dark band of their picture's own colours: classic's SCRL00I0.GFX and SCRL01I0.GFX are animated, and not read.
- **Ours, not classic's:** the place of the derived numbers by eye; the dagger's steps between its ends; a custom class's skill list by number, not alphabetical.
- **A custom class** is kept apart from the table (`career_set_custom`, `CLASS_CUSTOM`), and a save of one writes it as a `CLAS` chunk.

### Levelling (built: P6)

`character.c`, by PlayerEntity.RaiseSkills, CheckForLevelUp and FormulaHelper; `play/sheet.c` for the screen.

- **Uses** (`character_tally`): a swing that reaches something, its weapon's skill (Hand-to-Hand bare) and Critical Strike once a swing; a blow at a creature's back, Backstabbing; a creature's blow come at the player, Dodging, hit or not; four classic updates of running, Running; a jump from the ground, Jumping.
- **When skills are looked at:** at the end of a rest of an hour or more, not loitering (`rest_take_slept`), as classic does; never while playing (it was every frame until the playtest of 2026-11-07). The ones that rose are listed centred at the top of the view, "Medical skill has increased", drifting up and fading over 4 s (`draw_raised` in `game.c`).
- **A skill rises** at most every 360 game minutes, when its uses reach value × the skill's advancement × the class's × 1.04 ^ level × 0.4, down, plus 1. Reflexes count uses an eighth more or less a step from average. Past 95 only until a primary skill reaches 100. The class's raw word reads as 16.16 fixed point (a Mage's 1.04): inferred from the values, since DFU's conversion was not read.
- **A level** comes when (sum now − sum at creation + 28) / 15 passes it. The sum is the three primary skills, the two highest major and the highest minor. So the first needs only 2 points, each after 15.
- **Taking it.** The character sheet (INFO00I0.IMG) opens at once. The level rises, health gains half the class's hit points plus a roll of the other half, with endurance's modifier, at least 1, and 4 to 6 points go on the attributes through the spinner. It cannot close until they are placed.
- **The sheet** also opens on the character key: who they are, the doll, the skill lists on a scroll, Inventory and Exit.
- **Reflexes** also set a creature's melee timer, 450 a step past average.

## Controls and the HUD

GFALL starts in classic Cursor mode on a Win98 keyboard and mouse. View is mouse look and the left button attacks, held to swing; Cursor leaves the pointer where it is.
- move the pointer freely; hold the left button in an arrow region to move/turn;
- left-click the central X to use, or right-click anywhere in the world while sheathed;
- right-button drag swings while armed; Shift-right-click activates instead;
- WASD also walks/strafes; Left/Right turn, Page Up/Down look, Home centers, Alt moves the view independently;
- E activates the center; Shift runs, Space jumps;
- C, I, M and R open character, inventory, map and rest; H swaps hands;
- Tab switches the free pointer and continuous mouse look; Escape opens the menu.

The Controls screen's **Mouse: Cursor / Mouse: View** button selects the saved scheme. View mode starts with continuous mouse look and left-button attacks. Existing WASD movement defaults and old binding ids are retained; this is not a wholesale replacement with the original DOS keyboard layout. All available actions can be rebound in classic's slots.

The engine's `Input` supplies actions (`INPUT_*`: held, pressed, released), physical left/right button edges, and raw logical mouse deltas. The game owns their interpretation (`control.c`). Escape is its menu (`input_set_escape_quits(0)`), and it owns use (`gfx_set_use_key(0)`). Both keyboard and pointed use queue an explicit wall-tested ray (`interact_request_use_from`).

The HUD is drawn at the card's 640 × 480. Classic's 320 × 200 pieces are drawn twice as wide and 2.4 times as tall, where classic put them, with the engine's pictures (`pic.h`, `docs/pictures.md`).

**Built (P1, `play/overlay.c`):**
- **The compass:** COMPASS.IMG's 322 × 14 strip scrolled under COMPBOX.IMG in the bottom right, as DFU's HUDCompass draws it. It scrolls 258 pixels a turn, with yaw 0 north and growing east. The pictures are read by `formats/img.c` under ART_PAL.COL.
- **A crosshair:** a "+" in the middle. It is DFU's, not classic's: classic had none. `overlay_set_crosshair` turns it off.
- **The message line:** the engine's `interact_say`.
- **The pause line:** shown while paused.

- **The vitals (P2):** MAIN03I0, MAIN04I0 and MAIN05I0.IMG (health, fatigue, spell points, 4 × 32 each), filled from the bottom to their share as DFU's HUDVitals does, where their files put them (x 49, 57 and 65, y 161).

- **The bottom panel** (P7's initial art, completed in P9-b): MAIN00I0.IMG, 320 × 46 at the screen's foot, with the selected head, blue backing, bars, four interaction-mode icons and compact compass. On by default, `panel` flips it. Cursor mode already has its pointer; Tab frees it in View mode; see Classic HUD bar above for routing, resource costs and the remaining target acceptance. With the panel off, the original compass strip remains. Full screen, the options checkbox, hides the stone bar, the face and the icon buttons. The three vital bars sit at the left, ten pixels in and ten up from the corner, four wide with four between them (`hudbar_full_vital`, DFU's HUDVitals). The compass is COMPBOX.IMG's N/S/E/W strip at the picture's own place, bottom right, not the bar's compact compass. Clicks on the hidden buttons fall through to the world. The crosshair and the weapon stay, and the view is not resized. P9-c below supplies classic dungeon controls without continuous mouse look.

## Classic dungeon controls without mouse look (P9-c)

**Implemented and host-tested; full target acceptance pending.** `play/control.c`
is a plain-C controller. `game.c` routes its intent to the existing player,
weapon, interaction and HUD paths; menus/screens always use physical left-button
clicks, independently of rebound attack actions.

The behavior follows the original manual's **Cursor Based Interface** (printed
pp. 34–36), [Bethesda's public manual](https://downloads.ctfassets.net/rporu91m20dc/64TCuMQANyymi2m88GoYQG/a7bf45a208dca3d23fef1fc8825b0c0e/manual_daggerfall_pc_en-us.pdf).
Slot locations follow [DFU ControlsWindow](https://github.com/Interkarma/daggerfall-unity/blob/master/Assets/Scripts/Game/UserInterfaceWindows/DaggerfallControlsWindow.cs).
These are behavior references; the numerical regions/rates below are local
implementation choices, not constants recovered from FALL.EXE.

- **Cursor movement:** the visible world has a 3 × 3 region grid: central X and
  eight surrounding arrows. Horizontal cuts are x=106/214 on classic's 320-wide
  screen; vertical cuts are one/two thirds of the visible height (154 with the
  HUD, 200 without). Hold left on an arrow to move forward/back and turn left/right.
  Speed increases linearly toward the edge, to the normal character walking/run
  speed. Maximum turn is 90 degrees/s, keyboard pitch 72 degrees/s. Rates use
  seconds, with no fixed per-frame keyboard increment. Keyboard strafing remains
  available. Cursor speed scales movement input, never gravity's timestep.
- **Activation:** left-click X, sheathed right-click anywhere in the world, or
  Shift-right-click while armed uses the selected pixel. E still uses the center.
  The ray is the inverse of the renderer's current 640 × 480 projection and FOV,
  including pitch; the HUD covers the bottom of that view rather than changing
  its projection. Loot, doors/levers and people use that aim. Street-person picking
  now checks height, pitch and the nearest model wall too (`docs/entities.md`).
- **Weapons:** Cursor uses a separate `INPUT_SWING` action (default right button),
  View the existing `INPUT_ATTACK` (left). The same tested weapon gestures and
  bow draw/release code consume physical mouse motion without rotating Cursor's
  view. A world gesture keeps ownership when dragged over the HUD. A right press
  starting on the HUD cannot attack; left HUD clicks use the existing buttons.
- **Independent look:** hold Alt and move the mouse to offset the view while
  movement keeps its heading; release preserves that offset. Home or Alt-left
  centers it and levels pitch. Sensitivity is 0.003 rad/logical pixel; pitch is
  clamped to the camera's limit. `GAME.view_yaw/view_pitch` preserve the offset
  in saves; older saves take zero. Switching scheme, or loading a classic save with View selected, folds the
  offset into the player's heading, preserving the visible direction.
- **Ownership:** a left drag belongs to its starting arrow/X/HUD region class.
  Dragging a HUD or X press into the world never starts movement. Menus, screens,
  console, Tab and arrivals block old held buttons until release; pending weapon
  hits/drawn bows are canceled. `Input.reset` identifies focus/capture and other
  discontinuities, so synthetic release bits cannot loose a bow or become a swing. Screen clicks remain left-button clicks even if
  either attack action is rebound. The world runs under the free pointer.
- **Settings:** Controls' Mouse button selects Cursor (default) or View, saved
  as `mouse=cursor` or `mouse=view` in gfall.cfg; without the line it is Cursor. Existing movement bindings stay valid.
  New slots cover turning, pitch, center and Alt; the Swing slot shows the
  selected scheme's binding. Tab is a temporary pointer switch, not a preference
  change. Classic Cursor hides the modern crosshair.

**Cost:** no per-frame allocation, table lookup or passive cursor raycast. The
nine 16 × 16 arrow/X indicators share a startup-built 64 × 48 cutout sheet:
3 KB keyed / 6 KB 1555 on the card, selected by UVs. These are procedural
indicators under ART_PAL.COL, not recovered original cursor artwork. The Controls
button's opaque label tile adds 64 bytes. `player_step_motion` normalizes only
movement vectors longer than one; ordinary cardinal walking needs no sqrt.
The full persistent UI plus worst weapon/Controls reserve still fits 256 KB
in either card format (`check_hudbar`). Target cycle costs remain unmeasured.

**Tests:** `check_controls` covers passive motion, all regions, edge speed,
X/right activation, gestures, Alt/Home, menu gates, settings, turning at
30/60/120 Hz, ray reprojection at FOV 45/75/120 and an off-center door behind
an actual wall mesh. The hold test checks half-speed movement without slowing
a jump; the save test retains Alt offsets; the Daggerfall town test picks a
person and refuses a ray above their head. The ENGINE Engineer's focused
`tests/check_input.c` exercises the real input snapshot with Win32 stand-ins
(`docs/input.md`), including capture/focus flushing and independent physical clicks.

**Target acceptance still required:** navigate and fight through Privateer's
Hold, use doors/levers and all HUD modes, save/resume, change/rebind schemes,
Alt look and Tab, and check capture/focus transitions on Windows 98/the 233.
Tune region boundaries/rates from that playtest; no target run is claimed here.

## Menus

The title (`TITL00I0.IMG`), start (`PICK03I0.IMG`) and death
(`DIE_00I0.IMG`) screens retain classic's artwork and coordinates. Escape in
play opens the stone settings shell (`play/settings.c`, input in `menu.c`),
over the paused world. Its layout is **640 x 480**, independent of the
classic screens' stretched 320 x 200 canvas. It does not change video mode.

### The widgets (`play/ui.c`)

Every native screen is built of one set of pieces, `ui.h`: `ui_panel` (stone, rust or moss), `ui_button`, `ui_text`, `ui_centered`, `ui_paragraph`, `ui_flat`, and `UiList`, a column of rows with its arrows and thumb at its side. The atlases (stone swatches and FONT0003.FNT enlarged and in a compact face, 96 KB of texels) are built once at `ui_open`; there are no words in the art, so a label is a runtime string (and `ui_translate` lets a provider replace the English). **Drawing and hit-testing share one geometry**: a control draws itself and says where it answers (`ui_region`, an id and a rectangle); `ui_hit` asks which is under a point from what the last draw said, a later region over an earlier one winning. A frame that only needs the hit draws to no target (`ui_begin(0, 0)`), which makes the same regions with no quad, so the settings tests click exactly where a player would. A new screen is a draw function over these and a handler of its ids. `settings.c`, `files.c` use it now; classic's own canvas screens (the inventory, sheet, rest and so on) stay as classic drew them, and move to it only if they are ever redone.

### The saved games' list (`play/files.c`)

On the right a list: each character a row, their saves under them, newest first, each with the real time it was saved. On the left the snapshot of the save selected, with where it was left and when. Underneath: **LOAD** (also a double click on a save, 450 ms), **SAVE** (only from the game: a new save of the character playing, which the list then selects) and **DELETE**, a single save or a whole character with every save they have (asked once more, as Exit Game is). The wheel and the arrows scroll, Up and Down move the selection, Enter loads, Delete deletes. Plain C over `ui.h` and `saves.h`: hostcheck drives it by clicks and keys and rasterizes it.

### The five sections

1. **Saved games** (the first tab, `play/files.c`): the list below. Exit Game
   requires a second click; idle frames keep that confirmation. Another
   control cancels it. Back or Escape returns to play.
2. **Keybindings**: twelve visible rows, scrolled by mouse wheel or the two
   arrow buttons. All twenty implemented binding rows are reachable. Clicking
   a row captures the next key or mouse button; Escape cancels capture without
   closing the shell. A conflicting key swaps with the selected action's old
   key. The menu/console keys remain reserved by `input_bind`. Mouse: Cursor /
   View switches the draft scheme; the Attack / swing row edits INPUT_SWING
   for Cursor, INPUT_ATTACK for View, preserving the other scheme's binding.
3. **Graphics**: player-facing controls only — **resolution** (the engine's `gfx_mode` list, smallest
   first, and only what the board takes: a Voodoo 1 or 2 has nothing below 512 x 384 (the user's 86Box refused 320 x 240 and
   400 x 300), so it lists 512 x 384 and 640 x 480, and 800 x 600 with 4 MB of frame buffer; 320 x 240 and 400 x 300 are
   listed only for another board type;
   a click goes to the next; kept as `resolution=` in gfall.cfg, 640 x 480 when it says none; it shows at
   the next start, since Glide opens with the mode: `apps/gfall/main.c` reads it with `keys_peek_mode` before
   `gfx_startup` and passes it to `gfx_set_mode`; a mode the card refuses opens 640 x 480, and the row then
   says `NOW 640 x 480` beside the choice), horizontal field of view (45..120,
   default 75), far clip, **texture detail** and **sprite detail** (Low, Medium or High each, High the
   default: the engine's `texdetail` and `spritedetail`, a quarter, half or the full side of a picture, walls and
   the like apart from the sprites the player meets; the weapon, the HUD and every screen are untouched;
   a click goes to the next; `docs/memory.md`, Detail), distance fog, crisp sprites, vsync, dungeon lights,
   ambient light, light range, light shadow, weather view, flora, flora density
   and flora sight. Each maps to the matching engine console variable (`fov`,
   `far`, `fog`, `spritepoint`, `vsync`, `lights`, `ambient`, `lightrange`,
   `lightshadow`, `weathersim`, `flora`, `floradensity`, `florasight`). Developer
   switches such as culling, occlusion, back faces, mip chains, W-buffer,
   planar polygons, FPU 24-bit, sky, sun and moon, and water stay on the console
   only. Opening the tab snapshots the live engine into a graphics draft;
   edits stay in that draft until **Apply** writes them in one step (every
   setting here is written to gfall.cfg on Apply, the two details `texture_detail` and `sprite_detail`
   among them, given to the engine by `keys_load` before the first place; an Apply that changes one sends every picture
   again, so it is a load-sized pause, and a change is passed on only when it differs). Leaving the tab, Back, Escape, or a failed
   Apply restores the engine to the snapshot from when the tab was first opened
   in that session; tab changes keep the draft. A resolution shows at the next start.
   The shell is drawn in 640 x 480 coordinates scaled to the view (`menu.c`), so at 512 x 384 its text is
   four fifths size: not checked for legibility yet. Classic's screens scale through `screen.c` by w / 320 and
   h / 200 (a picture's width is taken from its two edges, so a factor that is not whole, 1.25 or 1.6,
   leaves no seam), the map markers (`dunmap.c`, `townmap.c`) by the same factors, the pointer through
   `df_view_w()` and `df_view_h()`. 320 x 200 and 640 x 400, which need a separate vertical focal length,
   and switching live wait on the engine.
4. **Audio**: independent SFX and music volume sliders, 0..100 percent,
   each defaulting to 80. Zero mutes that channel without toggling the
   console's separate sound/music enable flags. These use the existing
   `sound_set_volume` and `music_set_volume` APIs. Changes take effect only
   on Apply: MIDI devices without hardware volume may restart the song when
   its volume changes, so dragging must not call the music setter each frame.
5. **Gameplay**: Head Bobbing On/Off, default off. The entire option row is
   clickable. Apply updates the existing `viewopt_bob` preference; the walk/run
   camera wave itself is unchanged. Turning it off produces a steady eye.
   **Full HUD** On/Off, default on, shows or hides the bottom HUD bar and
   portrait. Vitals and compass remain when it is off. It is `full_hud` in gfall.cfg
   (the program's `viewopt_full` is its inverse, the bar hidden). It shares Gameplay's drafts,
   Apply, Default, cancellation and failed-write rollback.
   **Toggle Crouch** On/Off, default off: off, the crouch key crouches the player only while it is held;
   on, a press crouches and the next press stands (`viewopt_crouch_toggle`, drafted, applied and rolled
   back with the other two).
   **Show FPS** On/Off, default off, and **FPS Limit** (Off, 30, 60, 72 or 100 a click; the file takes
   20 to 240): the game's half of the engine's frame counter and frame limiter
   (`viewopt_show_fps`, `viewopt_fps_limit`; a number outside 20..240 is brought into it, 0 is none).
   Drafted, applied and rolled back like the rest. Apply, a rollback and the start-up read
   (`keys.c`) hand both to the engine (`gfx_set_fps_counter`, `gfx_set_fps_limit`; with vsync on, a
   limit of 60, 72 or 100 changes nothing, the retrace paces it), so a change shows at once.

FOV and both volume sliders share descriptors for their tab, action, range,
label and vertical position. Drawing, hit testing and pointer conversion use
those descriptors. A drag retains the action of the slider where it began:
crossing another row does not transfer ownership. No additional textures or
per-frame uploads are needed for Audio or Gameplay.

Default changes only the displayed section's draft. **Apply** commits that
section and writes `gfall.cfg` (every setting, all tabs' included); a write failure restores its previous live
settings, reports the failure, and retains the draft for another attempt.
Tab changes preserve drafts. Back, Escape, or leaving for a save-slot screen
abandons any unapplied drafts. A successful Apply remains effective when the
shell closes. There is no automatic write merely from closing it.

Settings are in gfall.cfg (The settings file, below). Out-of-range angles or percentages in it are skipped, and
zero volume is a real mute value. These are user preferences, not part of a
character's save. Full-screen is exposed inversely as Full HUD in Gameplay,
alongside head bobbing and Toggle Crouch.

### Artwork, text and ownership

`settings.c/.h` is plain C. It emits atlas rectangles through a callback;
`menu.c` submits them via `pic_draw_part`, and hostcheck rasterizes the exact
same rectangles for visual review. One coordinate system drives drawing,
hit targets and the slider endpoints. Menu clicks are physical left-button
edges, independent of attack bindings. Classic's ARROW.RAW remains the pointer.

**No words are baked into panel art.** The three atlases are a 128-square
stone/rust/moss swatch sheet, a 256-square enlarged block-font sheet and a
128-square compact-font sheet. Both fonts are built from FONT0003.FNT in the
user's ARENA2 at open, with gold ink and a one-pixel shadow. Labels, binding
names, notes and numeric values are runtime strings. `settings_translate`
accepts a translation provider with English fallback; paragraphs wrap, button
labels choose a smaller face when necessary, and text is clipped to its panel.
This is translation-ready rendering, not a language selector or a Unicode
font system: the existing font's byte encoding and glyph coverage still apply.

The source texels total **96 KB**, three square textures with their own shared
palette (opaque stone and keyed fonts). The Voodoo's existing 256 KB picture
room is unchanged. These textures are loaded only while the shell is open and
freed before any classic save-slot screen is opened. Their private heap dies
with them. The renderer uses bounded swatch pieces and glyph quads; an idle
frame, hover, binding edit or slider drag does **no texture upload, disk read
or allocation**. No full-screen image is rebuilt on a slider tick.

### The settings file (gfall.cfg)

Every setting the player can change lives in **`gfall.cfg`** beside the exe, as text, one `key=value` a line (`play/config.c`, plain C and host-tested; `keys.c` is the bridge to the live state). It is read at the start and written, whole, by every Apply of the Settings screens; a missing one is written at the first start from the defaults. No older format is read: this is a reimplementation, and nothing is ported from earlier files. A hand edit of a value lasts; a hand-made comment does not, since the file is written afresh with its own notes. A line that is no key we know or whose value is out of range is skipped, counted, and its line number logged once (`keys: gfall.cfg: 2 lines were not used, the first line 14`); the others still load, and a key left out keeps what the program had.

- **The screen:** `resolution=640x480` (512x384, 640x480, and 800x600 on 4 MB, on a Voodoo 1 or 2; it shows at the next start, read before the card opens by `keys_peek_mode`), `fov` (45..120), `far_clip` (16..16384), `texture_detail` and `sprite_detail` (`low`, `medium`, `high`), `fog`, `crisp_sprites`, `vsync`, `dungeon_lights`, `ambient_light` (0..255), `light_range` (1..8), `light_shadow`, `weather_view`, `flora`, `flora_density` (0..100), `flora_sight` (0..16384). The graphics ones were console variables that reset at every start; they are kept now.
- **Sound:** `sfx_volume`, `music_volume` (0..100).
- **Play:** `mouse` (`cursor` or `view`), `full_hud`, `head_bob`, `toggle_crouch`, `show_fps` (on or off), `fps_limit` (0 for none, else 20 to 240).
- **Keys:** `key.<action>=<name>`, the name the Keybindings screen shows ("Space", "Ctrl", "Left Button", "Num +", "="; a plain number is a virtual key too). The actions: attack, use, run, jump, character, inventory, map, rest, forward, back, left, right, swap_hands, cursor, swing, turn_left, turn_right, look_up, look_down, look_center, freelook, travel_map, transport, crouch. Escape is the menu and is not in it.
- `on` and `off` may be written 1 and 0, yes and no, true and false; case and spaces about `=` do not matter; a `#` at a line's start, or after a space following a value, begins a comment.
- **What gets logged** (the console's log, or `logfile on`): `keys: gfall.cfg read`, `keys: gfall.cfg asks for 512x384` before the card opens, `gfall: the screen is 512x384` or, when the card opened another, `gfall: gfall.cfg asks for 640x400, the card opened 640x480`, and `keys: gfall.cfg written, the screen's mode 320x240` on every Apply. The Graphics row says `NOW 640 x 480` beside a choice the card did not open.
- Not in it: saved games (`saves\<character>\S####.SAV`), MAIN.DAT and its mods, and the engine's console variables that are developer switches (they stay on the console, `autoexec.cfg`).

### Adding another option

This is a small game-specific settings UI, not a general widget framework.
The reusable parts are the panel/button drawing, glyph atlases, text wrapping,
translation hook and rendering callback. Layout and input dispatch are still
explicit C; there is no widget registry, automatic layout or option schema
that generates controls. Adding a toggle or another slider does not require
new panel artwork or engine rendering code.

To add an option backed by an existing game or engine API:

1. Add its draft value to `SettingsView` and its action to the settings action
   enum. Define its geometry beside the existing geometry in `settings.c`,
   and use the same coordinates in `settings_draw` and `settings_hit`.
2. Draw the control and runtime label, using `tr` for localizable text. For a
   slider, share its range/position conversion between drawing and dragging,
   using the existing slider descriptors. Keep text within its panel and preserve hit sizes.
3. In `menu.c`, snapshot the committed value on open, handle input into the
   draft, include it in dirty/default handling, and commit only on Apply.
   Restore the previous live value if writing fails. Back/Escape discards
   the remaining draft; focus reset must cancel any pointer/key capture.
4. If persistent, append a named field to the appropriate record schema
   (`viewopt.c` for view preferences). Give old files a default and validate
   the stored range. Do not renumber existing actions or record identities.
5. Add interaction and old-file/round-trip tests, update the host preview,
   and run hostcheck and the GFALL PE check. Verify the control on the target.

The amount of work is mainly the option's behavior and persistence. A new
rendering feature that the engine does not already expose is separate work
for the ENGINE Engineer. The shell exposes engine graphics console variables
on the Graphics tab; the reusable styling does not imply other UI exists.

### Verification

`make hostcheck` includes the real menu controller with deterministic input
and persistence stand-ins (`tests/check_menu.c`): dispatch, draft/apply/cancel,
failed-write retry, focus-loss drag cancellation, binding conflict swaps,
Escape capture cancellation, last-row scrolling and two-click Exit. The real
settings file's text is tested separately (`check_config`: a round trip, a hand-written file, the lines it cannot use). Audio and
Gameplay tests cover independent drafts, section-specific Apply/Default,
failed-write rollback/retry, drag ownership across rows, and focus cancellation.
`tests/check_play.c` also checks atlas bounds, output bounds and hit rectangles,
and writes `build/settings-{files,keys,graphics,audio,gameplay,translated}.ppm`; the translated
preview replaces headings without changing artwork. Win32 polling itself
remains covered by `tests/check_input.c`. Real Voodoo input and appearance
still require target playtesting; host previews are not hardware captures.

## Creatures and combat

The creatures already stand in a dungeon as entities (`docs/entities.md`): each has a kind (`world_entity_kind`) and a standing and a walking pose. The game adds:
- **Friend or foe** from the kind (monsters and most class people hostile in a dungeon).
- **A motor**: asleep until seen or heard, then turn to the player, walk to them (walking cells in a town, straight with the collider in a dungeon), and attack within reach. One step a frame for the near ones, under a budget.
- **The attack and the hit**: to-hit and damage per UESP's Combat page, as Daggerfall Unity's formulas have it (read for what they do). The weapon's swing is a picture drawn over the view.
- **Hurt and death**: the hurt and death records (5 to 14, 20 to 29; `docs/entities.md`, Not yet), and a body left on the floor with its loot.
- **The player's death**: classic's death screen, then the last save (Saves, below). A new game, and a load, close that world with `travel_drop`: the keep is not written back, so doors the dead character opened and enemies they killed do not come with the next life. The line "You have died." is cleared with the other death state (`forget_last_game`, `travel_drop`).

Moving an entity's position and pose from game code, and the walker colliding with entities, are engine requests.

### Weapons and the pack (built: P4's first slice)

- **The records.** `ITEM`, `WEAP` and `MATL` (`tables/items.c`, `docs/data.md`) carry classic's 19 weapon templates, what the 18 swung ones add (damage, skill, CIF), and the 10 materials, as Daggerfall Unity's ItemTemplates, FormulaHelper and ItemBuilder have them.
- **An item** (`play/inventory.c`) is a template in a material, by ItemBuilder's SetItemPropertiesByMaterial: value × 3 × `value_q`; weight in whole quarter kilograms × `weight_q` / 4, rounded half to even (Unity's `Mathf.Round`: a steel longsword's 22.5 quarters are 22, 5.5 kg), back to kilograms; condition × `condition_q` / 4.
- **The pack** is the player's (`Player.pack`): up to 64 items and what is worn. The start gives classic's kit (Creation, above).
- **Going places** (`play/commands.c`, `travel_go_to_town`), for testing until P10's map: `goto Sentinel` (a whole name, any case, the largest of that name), `goto 50026` (a place's code, its location number, unique over the map's 15251 outdoor places, the Code column of LEVELFALL's list) or `goto 17 1231` (region and index) goes to the town's streets from anywhere, once the console closes, the world standing still until then. It lands on the walkable cell nearest the town's middle, which can be inside a building. A part of a name that fits several lists them with their codes. `where` says the town, the region and its code.
- **Console** (`play/commands.c`), in play only: `additem [weapon] [material]` (bare, the weapons and materials there are; the material first or last, iron when none; any case, spaces and dashes ignored: `additem dai katana ebony`), `items` (the pack, `*` the one in hand, with weight, value and condition), `equip n|none`, `tai` (the creatures' AI on or off, below), and `tgm` (god mode: nothing takes the player's health, falls or blows, all through `player_hurt`; alone it flips, as Bethesda's consoles do; off with each new game).
- **Armour and gold** came with P5: Loot and the inventory, below.

### The weapon on the screen (built: P4's second slice)

- **The file** (`play/weapon.c`): the hand's WEAP `cif`, `WEAPON##.CIF`; bare hands `WEAPON10.CIF`, the fist (`WEAPON11.CIF` is the werecreature's claws). `weapon_hold` follows the pack every frame and reads the file again only when the weapon or its material changes. The file lives in the CIF set's own heap and goes with it (`cif_open`).
- **Where it stands:** every frame where classic draws it on its 320 × 200 screen, from the file (`CifFrame.x`, `y`: an IMG's own header, a swing's bytes 6 and 8). Every frame's box sits on the screen's bottom. Drawn under the compass and the bars.
- **Cropped at load:** a swing's frames are all stored at the whole swing's box, and one frame fills about 30% of it on average. Each frame goes on the card cut to the rectangle of its drawn pixels (`cif_bounds`: index 0 is clear), drawn at its place moved by the crop's corner, so the same pixels land on the same screen pixels. Worked out from the file each time it is read, for any `WEAPON##.CIF` a mod puts in ARENA2; nothing is kept per weapon. `check_weapon` checks every frame's crop loses no pixel and is tight.
- **The mouse swings** (`weapon_gesture`, DFU's WeaponManager). With the attack button down, in View the mouse's motion chooses a blade's swing, and the view keeps turning: classic stands it still, and a tester took that for a bug. Cursor reports no motion, so that click swings at random. `weapon_gate` keeps the click that closes a screen from swinging, held or on its release.
- **A bow** has no armed picture: nothing shows at rest. It needs an arrow: the press draws, the drawn frame (3) holds while the button is down, with the view free to aim, and the release looses, an arrow spent. The arrow flies (`shots.c` over `projectile.c`, below): 40 m/s along the view from the eye, no gravity as in classic, 4096 units at most, ended by the first wall (`combat_ray`) or living creature it enters (`combat_aim_at`). The release costs the fatigue and trains Archery and Critical Strike whether it strikes or not (`combat_loosed`); the arrival strikes by `combat_blow` with the bow in hand then, or the one it left if that hand holds another kind by then (`combat_missile_strike`). Eight arrows at most are in the air. It has no picture yet (DFU's is a 3D arrow); a place left, or a new game, ends them. People of a street are not struck by an arrow yet.
- **The material** recolours the picture by classic's tables (`cif_metal`; Loot and the inventory, Colours).
- **The card:** the armed picture is kept; the swing's frame showing is sent when it shows and let go when the next one does (a frame every fifth of a second). A cutout goes as 8-bit P_8 with the chroma key on index 0 (the engine's `cc5ec07`, `docs/pictures.md`), or as 16-bit 1555 on a glide2x.dll without the key. Cropped and keyed, the largest frame is 39 KB, the bow drawn back (`WEAPON09.CIF`, 204 × 179); 65 KB whole, 78 KB cropped as 1555. The largest melee frame is the staff's, 271 × 125. With the HUD, the armed picture (under 13 KB) and the controls screen (66 KB) that is at most about 140 KB of `GAME_PICTURE_KB`'s 256 on a keyed card, about 195 KB without the key. `check_weapon` measures both cards and fails past 48 KB keyed or 96 KB at 1555.
- **Not the whole swing at once:** the largest swing's frames together are 196 KB even keyed (the bow's 7), so a swing stays one frame on the card at a time.

### Striking creatures (built: P4's third slice)

`play/combat.c`, by Daggerfall Unity's FormulaHelper (CalculateAttackDamage, CalculateSuccessfulHit, CalculateWeaponAttackDamage, CalculateHandToHandAttackDamage, RollEnemyClassMaxHealth, DamageModifier), EnemyEntity, WeaponManager and FPSWeapon, read for what they do.

- **When:** once a swing, on its hit frame: frame 2 of a melee swing, 5 of the bow's (`weapon_hit`). The swing costs 11 sixty-fourths of fatigue then, struck or not.
- **What:** classic's (FALL.EXE): every live creature of the stream whose feet are under 91 units from the player's (2.25 m), within 35.156 degrees (200 of 2048) of the player's facing on the ground (any way nearer than 11 units), that no wall hides: a ray from the eye to its middle on the scene's collision (`combat_reaches`, `collide_use`, `collide_ray`, as `rig_third`'s). Each is struck by its own blow. The view's pitch does not matter. The swing's sound plays only when none was struck.
- **The creature as the sums see it** (`combat_foe`): a monster's level and armour value × 5 from ENEM; a class enemy at the player's level and armour 60 (DFU's cap for worn armour, as it wears nothing yet); Dodging 5 a level and 30, at most 100; its attributes from ENEM (`docs/data.md`).
- **To hit:** the weapon's skill (Hand-to-Hand bare), the swing's (down −10, down-left −5, left and right 0, down-right +5, up +10), the material's modifier × 10, a Dark Elf's level / 4, a Redguard's level / 3 with anything but a bow or a Wood Elf's with one, the level for a class expert with the weapon, the Backstabbing skill when it faces away, its armour, (Luck − its Luck) / 10, (Agility − its Agility) / 10, less its Dodging / 4, Critical Strike / 10 on a roll under that skill, +40 against a creature, −50; held to 3..97, rolled on 100. A Breton warrior at a rat with an iron longsword swung left: 31%. A street citizen is the same blow with the roll made at 100 and with 1 health (A killing in the street, above).
- **Damage:** the weapon's range, the swing's (down +4, down-left +2, down-right −2, up −4), the race's, level / 3 + 1 for a class expert, the level more or less for a class's bonus or phobia against the creature's group (`enemy_group`), a skeletal warrior halving a blunt weapon's and doubling silver's, floor((Strength − 50) / 5), the material's modifier; below 1 is 0. Bare: Hand-to-Hand / 10 + 1 to / 5 + 1, the swing's, the strength's. A backstab triples it on a roll under the skill.
- **A metal too poor** for the creature (ENEM's `min_metal`: a ghost needs silver) does nothing: "The material of the weapon you are using is ineffective." (classic's line). A backstab says "Successful backstab!"
- **Health** is rolled the first time a creature is struck (a monster between ENEM's least and most; a class enemy 10 and a roll of 1..its class's hit points for each level) and kept in its keep row's game bytes (health in two bytes, then 1 once rolled), so it stays wounded or dead as its block goes and comes back.
- **Struck and living:** it flinches, its hurt pose for half a second. **Killed:** it lies as its body, on the floor under it (a flyer falls), passed through (radius 0); with no body's picture it goes. A body come back with its block is laid again (`combat_frame`).
- **Sounds** (`play/sfx.c`, DAGGER.SND by DFU's SoundClips): a hit, Hit1..5 (108..112) at the creature; a blow that takes nothing, a parry (Parry1..9, 428..436) from a creature that parries (ENEM's `parries`) struck by a weapon, else the swing; a swing that reaches no creature, the swing: SwingMediumPitch (347) with a weapon, SwingHighPitch (106) bare (DFU's FPSWeapon default and its bare-hands fallback). A creature killed: BodyFall (15) where it lies, our choice; DFU's death sound was not found.
- **Blood** (`dunstream_blood`): a blow that takes health shows the kind's splash, `ENEM.blood`'s record of TEXTURE.380, at its middle for 0.4 s, on one of its block's 2 spare effects. They have no key, and nothing of them outlives the moment.
- **Knock-back** (`combat_knock_speed`, `foes_knock`, WeaponManager and EnemyMotor). From the damage and the kind's weight, 15 at least, 40 at most; it slides at up to 25, losing 5 a classic tick, and acts again at 5. The source's summary gave the formula's outline: the inner term of ours is our reading of it, so the shape is classic's, the exact figures unchecked.
- **Class enemies** wear and carry a kit (`loot_class_kit`, AssignEnemyStartingEquipment), worked out from their key: its armour goes by the part a blow comes at, capped at 60; its weapon sets its blow's damage; its body holds it all. Only two of DFU's variants are used, as EnemyEntity rolls.
- **Not yet:** creatures of a place loaded whole; a class enemy's poisoned weapon.

### Creatures awake (built: P4's fourth slice)
**Rays go in pieces** (`ray_by_pieces`, `RAY_PIECE` 256). One gather round a long sight ray held whole rooms, passed `COLLIDE_SET`, and left walls out, so creatures saw through them and could not lose the player. No gather a ray makes may come near the limit.

**Noticing needs an open way.** Classic's stealth roll goes through anything. Here the way must be open or through one door, and the distance counts height: a tester found a whole level waking. From the hold's start, 42 of 42 creatures are walled off. A mind only sees in front of it, as a sleeper does. A flyer's climb is swept (`fly_level`), so it stops at ceilings and floors. `foes_hunting` is what stops a rest and tires as a fight: nobody hunts with the AI off.


`play/foes.c`, by Daggerfall Unity's EnemySenses, EnemyMotor and EnemyAttack, read for what they do.

- **Asleep** where its marker stood until it sees the player: within 4096 units (classic's sight), anywhere not behind it (180 degrees), no wall between its eye (two thirds up) and the player's (`combat_sight`). Two sleepers look a classic tick (about 18 a second, `FOES_TICK`), round the stream's creatures. A passive one (its marker's action byte, `dunstream_passive`) wakes only when struck. Struck, any creature wakes (`foes_alert`).
- **Awake**, it has a mind (24 at most): a body a quarter of its picture's width across (8 to 24 units), its height tall, under classic's gravity (a flyer, none), solid to the player and the others (`Entity.radius`). Waking, it barks (ENEM's `bark_sound`).
- **Each classic tick** it looks again every sixth (DFU's reacquiring), turns at most 20 degrees toward where it last saw the player, counts down 200 ticks unseen and then gives up (asleep where it stands), and may start a swing: the player seen, within 2.25 m, within 22.5 degrees of its facing, its melee timer run out, and a roll on its Speed (at least 8) under Speed / 8 + 6, as classic's AI has it (about one tick in four at Speed 50; see Checked against classic). It plays its attack sound.
- **Each frame** it walks along its facing at (Speed + 150) / 39.5 m/s (`rules_walk_speed`) toward where it last saw the player, stopping short of reach (0.85 of 2.25 m) while it sees them; a flyer rises or sinks to the player's feet at the same speed. A move sound every 2 to 5 s while it walks (our interval). A flinch stands it still, and cancels a swing begun.
- **Its swing** is its attack pose (records 5 to 9) at 10 frames a second, held by the game frame by frame; the blow lands on its third frame (index 2, our choice: DFU takes the frame from each kind's animation) if the player is still within 2.25 m and 0.25 m **feet to feet, height included**, within 35.156 degrees of its facing on the ground (or within 0.25 m), and no wall has come between (ours). Reach once went by the ground alone, and a flyer stopped changing height once near across the ground: a Giant Bat 269 units over the cave's floor killed a motionless player from there. A flyer now keeps coming down while it sees the player, and `check_blows_in_reach` holds this. Rolled by `combat_foe_blow`: a monster's three attacks each rolled to hit and summed, a class enemy's bare hands. The melee timer: 1500 to 3000, less 50 a player level past 10, reflexes average, over 980: seconds.
- **The player hit:** Hit1..5 flat and the race's and sex's pain clip (DFU's, three each); missed, the high swing at the creature. Health through `player_hurt`, so `tgm` keeps the player whole. A blow that kills, or a fall: "You have died." and the death screen (`die` in `game.c`; Saves, below).
- **The console's `tai`** switches them off: no one wakes, moves or strikes, and a swing begun stops. Kept across games.
- **Far creatures move less often** (`foes_due`). A creature within `FOES_NEAR` (12.8 m) of the player moves every frame; past it every second frame, and past `FOES_FAR` (25.6 m) every fourth, staggered by its mind's number. The time it did not move is kept and spent by its next move, so it covers the same ground. Senses and blows are unaffected: a blow only lands within 2.25 m, and senses keep classic's ticks.
- **On the 233:** a near mind steps a body (a collision gather and sweep) every frame and looks along a ray every sixth tick; not measured there yet.
- **Hearing** (EnemySenses). One that knows of the player, not seeing them, hears them within 25 m, through a door but not a wall, and keeps after them; it cannot swing on hearing alone.
- **Stealth.** A sleeper within 1024 units may notice the player through anything: once a game minute, a roll against 2 × (distance × Stealth / 1024), so nearness and a poor skill wake it. The roll is worked out from the creature's key and the minute, so nothing is stored. It is one use of Stealth a minute. **Sneaking** (DFU's `IsMovingLessThanHalfSpeed`, which counts a crouch's half a walk): a player moving at no more than half a walk, crouched or standing still (`player_sneaking`, measured from where the feet actually went each step, `Player.speed_xz`), is rolled against only every other game minute (`foes_rolls`), and the skill is used only on a minute that rolls, so crouching halves how often a sleeper behind or beside the player gets its chance; walking or running does not. Sight is unchanged: a sleeper before the player, in the 180 degrees classic gives it, with a clear way, wakes at once. DFU's rule that a creature which has already met the player finds them again within 1024 unless they sneak is what **hearing** is here (an awake creature within 1000 with an open way keeps the player, sneaking or not, as DFU's hearing does). The console's `foes` lists the creatures awake, then the sleepers within sight's range nearest first (12), each with its distance, whether the player is before or behind it, and what it makes of them: sees them, too far, no open way, or the chance a minute it rolls of noticing them at their Stealth, and whether that is every other minute.
- **The search.** Reaching where the player was last known, and they not there, it walks on along the way they were going, a metre further each time, up to 11, until its give-up count runs out.
- **Doors.** One that `opens_doors` opens a closed, unlocked door its sight met, from 2 m. Bashing is DFU's Enhanced Combat AI, not classic's: left out.
- **Not yet:** ranged attacks and spells; the creature's hurt voice. Classic has no wandering.

### Checked against classic (FALL.EXE, decompiled)

`daggerfall/decompiled/` is not authoritative (AGENTS.md), and nothing of it is copied: it was read, by its constants, to check DFU's numbers where they were a guess or a paraphrase. What it showed:

- **The swing roll:** the AI's decision routine starts a melee swing when the melee timer is 0 and a roll modulo Speed is *under* Speed / 8 + 6 (about one tick in four at Speed 50). `foes.c` had it the other way round (three in four); fixed.
- **Confirmed as built:** the melee timer (1500 to 3000, less 50 a player level past 10, 450 a step of reflexes past 2); reach 90 units against the player (60 against another creature); the give-up count, 200; a blow lands within 91 units, at any angle within 11, else within 200 of 2048 (35.156 degrees) of the attacker's facing; a creature's death plays sound id 17 (DAGGER.SND's position 15, BodyFall) at it, never at the player; a blow that takes health plays id 378 + 0..4 (positions 108..112, Hit1..5). Classic names clips by record id; `sndfile_by_id` maps them.
- **Classic's messages,** from FALL.EXE's strings: "The material of the weapon you are using is ineffective." and "Successful backstab!", now shown.
- **The player's strike** goes through the same routine: classic strikes every creature within 90 units and 35 degrees of the player's facing (on the ground, not the view's pitch), each counted, and plays the swing only when it counted none. `combat.c` took only the nearest near the line of the view, as DFU's box does; it now strikes as classic does.
- **Not found:** the frame of a creature's attack its blow lands on (still the third, our choice), and how often a walking creature plays its move sound (still 2 to 5 s, ours).

## Loot and the inventory (built: P5)

**Inventory is the game's, not the engine's.** The engine is simulation and world: it holds, moves and draws what stands in a place. What an item is, what is carried and what a body holds are the game's, in `play/`. The engine's generic `scene/pack.c` is not used: there weight belongs to a kind of item and wearing is 16 slots summed into 4 stats, where here weight and value belong to one item, by its material, and armour counts per body part.

### Items and armour

- **The records.** `ITEM` holds 31 templates: the armour (102..112), the weapons (113..131) and gold (276). `ARMR` says what an armour template adds. `MATL` gains leather and chain, which only armour comes in, what each material's armour gives, and a second value multiplier.
- **A piece's numbers** (ItemBuilder.ApplyArmorMaterial): plate as a weapon's material; leather half the weight, the template's price; chain the template's weight, twice the price.
- **Armour** (DaggerfallEntity.UpdateEquippedArmorValues). Each of the seven body parts starts at 100, lower being better. A piece takes 5 × its material's armour (iron 7, steel 9, daedric 21, leather 3, chain 6) off its part. A shield takes 5 × its own (1..4, whatever it is made of) off each part it covers.
- **Armour changes combat.** A creature's blow comes at one part by classic's odds (of 20: head 2, each arm 3, chest 4, hands 4, legs 3, feet 1), and that part's value goes into its chance to hit. A rat's 83% at a bare warrior is 48% through iron.
- **Colours** (`cif_metal`, DFU's ImageProcessing.GetMetalColorTable, classic's own tables). Weapons and armour are drawn in the sixteen palette indices from 0x70, which are steel as they stand; each other metal swaps those sixteen by its table, on the doll, in the lists and in the swing alike. Leather and chain keep their picture's colours (ItemHelper.GetArmorDyeColor gives them none). A picture's pixels outside those sixteen stay as drawn: a sword's hilt is always gold, and two of the six helms (variants 0 and 3) are gold all over, whatever they are made of, so `additem helm daedric`, which takes the first variant, shows a gold helm. Clothing's dyes, the sixteen from 0x60, come with clothing. `make hostcheck` writes every metal side by side to `build/dye0.ppm`.
- **Wearing.** A one-handed weapon goes in the first free hand. The swap key (H) changes which is in use, unless a shield holds the left (WeaponManager). A two-handed weapon takes both, and a shield takes it out of the hand. The console's `equip` always uses the right.
- **Carrying:** Strength × 1.5 kg (UESP); past it a thing cannot be taken. A pack's gold weighs 400 coins a kilogram. Classic's weight does not slow the walk (DFU's PlayerSpeedChanger takes no weight).

- **Wear** (`item_wear_down`, FormulaHelper). A blow that takes health wears the weapon, and a creature's blow what covers the part it lands on, a shield before armour: (10 × damage + 50) / 100, and with none a one in five chance of 1. Worn out it comes off, says so, and cannot go back on.
- **What a class allows** (`character_refuses`, DFCareer's bits): forbidden weapon groups, leather, chain and plate, each shield, and metals. The inventory and `equip` refuse them. A class's forbidding does not stop a thing being carried.

**Clothing (built, host-tested, 2026-11-06).** DFU's 76 templates (men's 141 to 181, women's 182 to 216) are rows of `ITEM` with a rarity and a count of looks (variants) each, which weapons and armour leave at 0 (their rarity and looks the code knows). A piece has no material: its weight, price and condition are its template's, its name is the template's alone, `item_make_clothing(template, look)` makes it and `item_make` of a clothing template does the same. **Where it is worn** (DFU's ItemEquipTable, `item_clothing_place`): the chest (shirts, tunics, robes, dresses, straps and the like), the legs (pants, breeches, skirts, loincloths, tights, wraps, the Khajiit suit), the feet (shoes, boots, sandals: the same place armour's boots take, one or the other), and a cloak in the second of two cloak places, the first when the second is full, the second again when both are. They are four new `EQUIP_` places (chest clothes, legs clothes, two cloaks) saved with the rest. **Its picture** is the template's archive (TEXTURE.239 for men, 235 for women) moved by the wearer's body shape (Argonian 0, Elves 1, humans 2, Khajiit 3) and its record by the look; the doll lays it in its draw order over the underclothes (a cloak 90, a shirt 40, a skirt 35, pants 10, shoes 20). The inventory's third tab, Clothing & Misc, lists it; worn things stay off the list. **A new character starts in** a short shirt and casual pants (a woman's closed short shirt and casual pants), the pants' look rolled. **Loot** adds clothing after weapons and armour, a table's clothing chance halved at each success, the player's sex's (`loot_set_sex`). **Not yet:** dyes (DFU gives every shirt and the like a colour: `ChangeDye` over palette ranges; ours are drawn in the file's own colour), clothing's wear (it keeps a condition nothing lowers), jewellery and everything else in the other tabs, class enemies wearing what they carry.

### What can be searched

`play/loot.c`, by LootTables.GenerateRandomLoot and FormulaHelper's RandomMaterial and RandomArmorMaterial.

- **The use key** searches what the view rests on within 3.2 m (ours), no wall between (`loot_find`): a dead creature, a heap of treasure, a dropped pile. Otherwise it does what it always did. With nothing to find it says "You find nothing."
- **Treasure** (`dunstream.c`, DFU's RDBLayout). A block's random treasure markers (archive 199, record 19) and its fixed flats of TEXTURE.216 stand as entities, keyed like its creatures, not as flats, so they can be searched and can go. A random marker's heap lies on the floor under it, its picture one of DFU's 20 (`randomTreasureIconIndices`), chosen by the marker's offset so it is the same each time it is seen. It fills from the table the dungeon's type names (`LDUN`, `dungeon_loot`): a human stronghold's is N. Emptied, it goes for good (`Entity.dead`, kept); a body stays where it fell. The hold has 8. A place loaded whole still draws its treasure flats as flats, and nothing searches them: GFALL only plays the stream.
- **The rolls,** under the table the creature's kind names (`ENEM.loot`, `LOOT`) or the dungeon's type does: gold between the table's two figures, times the player's level; then weapons, then armour, each rolling again at half the chance after a success. A weapon or piece is any of its kind alike; armour is leather 69%, chain 20%, else plate; plate and weapons take their material from a roll of 0..255 moved by the level.
- **A body or a heap holds no list.** Its things are worked out when it is opened, from the game's seed (`loot_new_game`) and its own key, on its own dice, so nothing else rolled changes them. A dungeon of them costs no memory until one is searched. Its keep row holds the rest in its game bytes (`COMBAT_KEEP_*`): the player's level when it died or was first opened, and a bit for each thing taken. So what was taken stays gone as its block goes and comes back, and a save only has to keep the seed and the rows. The price: they can only be emptied, not given things.
- **Not yet:** ingredients, books, potions, maps; the other columns of `LOOT` are read in but not rolled.

### Dropped piles

`play/pile.c`. Dropping is classic's: Remove moves a thing to the right list, and when the screen closes those things land as a pile at the player's feet (DFU's CreateDroppedLootContainer). Near a pile already there (48 units, ours) they join it. Searched again, a pile takes things and gives them back, and with nothing in it, it goes.

- **Where it stands.** A pile is one of its block's `DUNSTREAM_SPARES` (4) spare things: entities the stream builds with every block, not drawn until used. Being the block's entities, the stream moves them with the origin and keeps their place when the block goes and comes back, so the game needs no list of positions and the engine no new call. A spare's keep row says it is used (`DUNSTREAM_SPARE_USED`), which `changed` in `dunstream.c` reads to draw it again. A block's fifth pile joins its nearest.
- **What is in it** is one pool of 256 items in `pile.c`, each marked with its pile's key, so a block gone from memory loses nothing and a save only writes the pool. Past 256 the screen says there is no room. With no block loaded round the player the things go back in the pack: nothing dropped is ever lost.
- **The card:** each block holds its 4 spare pictures whether used or not, a few KB of its picture residency.

### The inventory screen
**The wheel** scrolls the list under the pointer, the pack's when it is over neither (`bag_wheel`). In the talk window it scrolls the topics or the conversation (`talk_wheel`).


`play/bag.c` is what the screen shows and what a click does, plain C and host-tested. `play/doll.c` draws the paper doll. `play/screens.c` puts the result on the card, as it does the character sheet and creation, and all three draw through `play/canvas.c`. The inventory key (I) opens it, the use key on a body opens it with the body's things in the right list, and Escape, the inventory key or Exit closes it.

- **Everything is classic's picture at classic's place,** by DFU's DaggerfallInventoryWindow rectangles: INVE00I0.IMG the screen, INVE01I0.IMG the chosen tab and button in gold, INVE06I0 and INVE07I0 a list's green and red arrows, ITEM00I0 the stone behind the small panel, FONT0003.FNT the labels (`formats/smallfont.c`) in classic's ink, index 146.
- **The doll** (`doll_compose`, DFU's PaperDollRenderer): the race's SCBG background cropped at 8, 7, the body, its underclothes, the head, then what is worn by the template's draw order. Each picture's own header places it, less classic's doll corner 200, 8. Armour's archive is 245 for a woman and 249 for a man, plus the body shape (Argonian 0, elf 1, human 2, Khajiit 3). A worn picture's index 255 is a mask: it shows the background again, which is how a helm takes off the horns under it. In a list it is only clear.
- **The lists** show four rows of 50 × 38. A thing worn is on the doll, not in the list. Clicks go by the mode: Equip wears, Remove drops, Info shows a box. A click on the right list takes the thing, a click on the doll takes it off, found by a byte per doll pixel saying which worn place drew it.
- **The Gold button** asks how much, and that much lies in the right list as a thing, not more than is carried (DFU's). A body or a heap refuses it.
- **Ours, not classic's:** the "Carrying", "Dropped", "Body", "Treasure" and "Pile" words over the lists. Wagon and Use say why they do nothing yet.
- **The info box is classic's** (`play/iteminfo.c`, DFU's `GetItemInfo`): TEXT.RSC 1000 armour (1014 a helm or shield, with no material), 1001 a weapon, 1011 an arrow, 1007 a letter of credit, 1003 everything else, their `%mat %arm %wep %it %mod %wdm %qua %kg %wth` filled in (a stack's weight and worth are of the lot; a thing with no condition has no Condition line). The condition's words (new, almost new, slightly used, used, worn, battered, useless, by a fifth) are ours.
- **Saves keep the weather's kind** (`SaveState.weather`, WEATHER_*): `game_enter` gives it back with `weather_set` once the place is loaded, so a load finds the sky it left. The engine's roll sequence and its timer are not kept: the next change comes by the odds from there.

**What it costs the 233.** The whole screen is composed in memory into one 320 × 200 picture, only when something on it changes: a click, or the pointer coming to rest on another thing. An open frame sends nothing to the card and draws that one picture's 29 quads, however full the lists are. It takes the 66560 bytes of `GAME_PICTURE_KB` the controls screen already counts on, not one more. A fitted item picture is kept in one of 8 places, so a list scrolled back reads nothing. While it is open `gfx_frame` gets no level (`game_world_shows`, the controls screen too): no world is drawn only to be covered. All this is by the design's sums and host tests: **none of it has run on the 233 yet.**

**Memory:** two heaps of its own while open, one for its 64 KB pictures and buffers, one for what it only reads to copy, let go past 512 KB. Both go at close, and any TEXTURE file read into them leaves the cache with them (`texture_forget`).

## Afflictions (built: diseases, lycanthropy, vampirism; host-tested, not run on the target)
`play/ills.c`: what ails the player beyond their wounds, the game's one state, saved as rows (`SICK` a disease each, `CURS` the rest). Daggerfall Unity's DiseaseEffect, LycanthropyInfection, VampirismInfection, LycanthropyEffect, VampirismEffect and PassiveSpecialsEffect, as sums over the sheet. Its gifts and drains go into the sheet's own attributes and skills, held to 1..100, and what was really added or taken is kept, so it can be given back exactly.

### Diseases
- **The seventeen** are MAIN.DAT's `DISE` (`tables/diseases.c`), classic's numbers: each attribute's, health's, fatigue's and spell points' weight, the day's damage range, the days of symptoms (most last until cured; Caliron's Curse, Blood Rot and Wizard Fever 3..18 days).
- **Caught** from a creature's blow that hurt and did not kill (`ills_struck`, from `foes.c`): a rat 5% and a giant bat 2% (plague, stomach rot or brain fever; DFU takes DF Chronicles' list for the rat), a zombie 2% and a mummy 5% (one of eleven), a vampire's 2% plague. Never at level 1. The saving throw is DFU's: 50, the race's resistance +30, low tolerance -25, weakness -50 (immunity: none caught), the class's immunity +50, resistance +25, low tolerance -25, weakness -50, then a tenth of the Willpower, held to 5..95; a roll within 20 under the save still passes it on (DFU's proration). Nothing is said when it is caught.
- **Each day** after the one it was caught on, one roll of its damage times each weight, off the attributes, health (it can kill: "Your disease has killed you."), fatigue in its raw 64ths, and spell points. "You feel somewhat bad." Days skipped by rest or travel each count. Passing, it gives back what it took ("You feel better.").
- **Cured** at a temple: anyone in a temple offers a panel with Cure Disease (`SERVICE_PANEL_CURE`, the general panel). 250 gold a disease and an infection, by the temple's quality and the region's prices (`rules_cost`), then bargained; free on South Wind's Prayer and the two Plantings, half on North Wind's Prayer and the Mid Year Celebration. Classic asks first; this pays at once and says the price.

### The curses
- **Infection**: a werewolf's or a wereboar's blow 0.6% its lycanthropy, a vampire's 0.6% vampirism (`specialInfectionChance`). A dream the next day; past three days the curse. Not two at once; none while cursed.
- **Becoming one** cures every disease, and none can be caught after. A lycanthrope gains +40 Strength, Agility, Endurance and Speed; a vampire +20 to every attribute but Intelligence (an Anthotis that too). Both +30 to Running, Stealth, Critical Strike, Climbing, Hand-to-Hand and Jumping, a lycanthrope Swimming too. A vampire's clan is the `vam:` of the region it was caught in (FACTION.TXT, read into `Faction.vampire`), the Lyrezi when none. The HUD's face becomes `VAMP00I0.CIF`'s; the sheet calls the race Vampire, or Werewolf and Wereboar in the beast's shape.
- **A vampire's turning** is DFU's fake death: two weeks pass and they wake at dusk, whole, "Death is not eternal". DFU carries them to a cemetery of the region; here they wake where they fell (a gap).
- **A lycanthrope** changes shape once a day into the beast (the console's `morph` until magic gives the spell, P12-b) and back at any time; every full moon by force ("You dream of the moon."). The moons are DFU's lunar phases: Masser full on (day of year + year × 360 + 3) mod 32 = 0, Secunda on (… − 1). Each change heals them whole, as classic. In the beast: both hands empty and bare hands fight, no pack ("You cannot use your inventory while shapechanged."), no talk ("You get no response."), no trade. A month without killing an innocent (a street citizen or a guard, `combat_strike`) and their health is held under a limit falling a point an hour of want, to 4; "You need to hunt the innocent." each half hour of game time.
- **A vampire** burns 12 health each fourth game minute outdoors from 6 to 18 and inside a temple; any blow that lands feeds them; unfed for a day they cannot rest ("You must feed before you can rest."); by day they cannot set out on the travel map.
- **Lifted**: `ills_lift_curse` takes back the gifts. Only the console's `cure` does it until P11's quests.

The console: `ills` (what the player has), `catch <disease>|werewolf|wereboar|vampire` (a disease at once; an infection already dreamt, to turn at the next day), `cure`, `morph`.

Not yet: the cemetery; the beast's own weapon picture, head (`WERE00I0.IMG`, `WERE01I0.IMG`) and paper doll (`WOLF00I0.IMG`, `BOAR00I0.IMG`); its sounds; citizens fleeing the beast and the crimes it does not count; silver to hit the player; the dream videos (`ANIM0002.VID`, `ANIM0004.VID`); the cure quests and vampire quests (P11); Hircine's Ring; a vampire's spells and a lycanthrope's (P12-b); poisons; a class's disease biography modifier.

## The first real-hardware playtest (2026-11-07)

On the P55C 233 with a Voodoo 2. The engine's half (picture state batching, `pic_update`, `swapahead`, `prof log`, Alt-Tab) went to the ENGINE implementer. The game's half, fixed and host-tested unless said:
- **Escape** on the start menu, the root of every screen, leaves the game.
- **Settings at 512x384 and 800x600:** a glyph is drawn at its own pixel size, placed at the layout point of the run's advance (`ui_scale`, a glyph quad's size in native pixels); panels scale by their edges and a 1-pixel bevel stays a pixel. Widths come back in layout pixels, so fitting and centring hold. Applying a new resolution says "Restart the game to change the resolution."
- **Mouse look** from Cursor mode puts the interaction mode back to Grab: there is no pointer to click the mode button with.
- **Title music** plays from the first frame: sound and music start before the title opens (`apps/gfall/main.c`).
- **Popups over the world:** rest, the spellbook, message boxes, the bank, a merchant's panel and the logbook are composed on index 0 and drawn as cutouts over the paused world and the HUD (`screens_over_world`), as classic. Resting, the bars are seen filling. Index 0 inside their art is clear too.
- **Skills after a rest** (Levelling, above).
- **A horse or a cart in a shop's basket** weighs nothing against the load (`item_carried_kg`), as in the pack: they could not be bought before.
- **The bank's loan** is due on a date ("15 Morning Star, 3E 406"), not the game's day count.
- **The numeric keypad** types digits in every number box (`ui_digit`: the bank, rest, the gold box, a merchant's days, the travel map's search).
- **A new game's main quest** says only the hold's opening. S0000999, the main quest's master, has a task per step of the story: "when <the story's person> is available and repute and level are met, start the quest", and beside it "when available but the level is not met, say 1032..1036" ("come back when you are better"). Its "npc available" condition (opcode 78, DFU's `WhenNpcIsAvailable`) is true only as the player clicks that named person (their individual faction) while no running quest has a person of that faction; clicking the same one twice running is no new click (`quest_met`, `g_clicked_faction`). We had it true whenever the faction existed, so every refusal fired with the hold's opening. The source of the rule: Daggerfall Unity's `Quests/S0000999.txt` and `Questing/Actions/WhenNpcIsAvailable.cs` (whose own comment says when a named person counts as available is not clear). The tutorial is not started (it never was).
- **Screens sent by tiles:** a composed screen under the same palette and cutout goes to the card by `pic_update`, only its changed tiles; a new palette or cutout is a new picture (`send_screen` in `screens.c`). A scroll step or a coin's frame no longer sends the whole 320 x 200.
- **Settings text after panels:** `menu.c` holds the glyph quads and draws them after every panel (`settings_text_flush`), so the engine's batched picture state switches between opaque and keyed once a frame, not once a word. Not measured on the 233 yet.
- **The logbook's head** names who gave the quest (the giver's faction, the guild group, or "Main quest"), not its file.

Open from the playtest, game side: swimming (slow, the surface flickers in and out, no climbing out at an edge) and water creatures that do not swim up or down; the shop's coins gone on a second opening (not reproduced on the host: the steps are wanted); the spellbook item, and its description text out of line; a gold sound on buying and selling; Privateer's Hold's song against classic's; arrow and spell sounds; creatures woken by a rest standing outside the map. And the frame rate: `prof log` numbers are wanted from the 233.

## Known gaps

What is built but not whole, and the phase each goes in. Each section above says the detail.

**The first playtest** (2026-10-30, in the hold) found: sight through walls, creatures that never lost the player, flyers in walls, no rest with the AI off, a swing from Goodbye's click, the view frozen in a swing, two weapons in one hand on the doll, no wheel, a platform not ridden and culled from below, sounds not shown or muffled. All are fixed and host-tested, none yet tried again there. One report stands open: two bats that seemed to load two images. The pictures are shared by archive and record, so if a bat drew doubled on the screen it is another bug.

**Not yet run on the 233** (host-tested only, some of it since played in that test): striking and creature AI, the inventory screen, character creation, the sheet and level-ups, saving, loading, the death screen and rest, a frame with no world under a full screen, bodies, treasure and piles, the controls screen and the options' Exit. Not yet measured there: the `play` stage, the 3 ms streaming budget, the load time.

| Where | Gap | Phase |
| --- | --- | --- |
| Combat | an arrow has no picture in flight | later |
| Creatures | ranged attacks and spells; a hurt voice | P12-b |
| Creatures | the frame a blow lands on and how often a move sounds are our guesses, not found in classic | - |
| Saves | creatures awake come back asleep; the weather keeps its kind, not the engine's roll sequence or its timer | later |
| Rest | an encounter wakes a sleeper, not a new creature; a class with no spell point regeneration | later, P12-b |
| Items | jewellery, ingredients, books, potions, maps, and the screen's other tabs (clothing is built) | P12-b, later |
| Items | clothing's dyes, clothing's wear | later |
| Inventory | the Use button | P12-b |
| Inventory | two weapons: the doll shows the one in use, as the pictures have one weapon hand; classic's own way not found | later |
| Inventory | the info box's condition words (new .. useless) are ours: classic's thresholds were not found | later |
| Loot | the other columns of `LOOT` wait for their items | P12-b, later |
| Loot | a body or a heap takes nothing put on it (a choice, see What can be searched) | - |
| Places | treasure and piles only in a dungeon streamed by block; a town's doors not keyed; the sea can be walked on | later |
| World | a ship to stand on; the indoor automap's marks and notes, the town map's rotation and names; the travel map's neighbouring provinces; places on the travel map marked by talk; a bed and kept things in a rented room (a building is marked on the town map); guild travel; regional holidays; the afflictions' cemetery, beast pictures and sounds (Afflictions) | P10 |
| Player | the completed classic HUD bar needs target acceptance; the map button opens the travel map outdoors, the automap and magic buttons await their systems | P9-b, P10, P12-b |
| Player | target acceptance/tuning of the implemented classic cursor controls | P9-c |
| People | guards that arrest rather than fight; a house of one's own | P12-a, later |
| Creation | a custom class's special advantages, disadvantages and reputations; the Help button | P12-b, later |
| Creation | the class questions' animated scroll and characters | later |
| Creation | the answers' history and lasting modifiers (`#`, `RP`, `TH`...) are read past | later |
| Creation | Save Roll and Load Roll; the race's voice on choosing | later |
| Character | the Spellbook, History and Affiliations buttons | P12-b, later |
| Character | (starting clothes and spellbook are built) | |
| Engine | `scene/pack.c`, `ui/packview.c`, `scene/rpg.c`, `ui/rpgview.c` are used by no game; the user's line is that the engine is simulation and world | to decide |

## Rest and time (built: P7)

`play/rest.c`, by DFU's DaggerfallRestWindow and FormulaHelper. The rest key (R) opens REST00I0.IMG over a black screen, as classic does.

- **Three ways.** For a while (hours typed, 99 at most), until fully healed, and loitering. REST01I0.IMG counts hours past, and REST02I0.IMG hours remaining, each with its Stop.
- **Time** passes ten game minutes a step: a step every 0.075 s of real time resting, every 0.125 s loitering. The clock is set directly and turns the day itself, so a console that stopped the clock's scale cannot hang a rest.
- **Each whole hour of rest** gives back health (endurance's modifier + (Medical + 60) × its most / 1000, at least 1), fatigue and spell points (an eighth of their most, at least 1), and is a use of Medical. Loitering gives back nothing.
- **A creature awake** refuses a rest ("You cannot rest with enemies nearby."), and stops one each hour. Skills that have earned it rise when the rest ends, and a level due opens the sheet.
- **An encounter.** Each hour one chance in 36 (ours: DFU's dungeon roll, how often it rolls not read) of the nearest sleeper within 2048 units waking and coming, which ends the rest (`foes_rouse_nearest`). DFU conjures a creature; there is no way to here yet.
- **Where** (DFU's CanRest; UESP's Rest: resting in a town is vagrancy), decided by `game.c` when the screen opens (`rest_set_rule`): in a city, hamlet or village (not a farm or a lone tavern), resting in the streets is vagrancy. The screen closes, TEXT.RSC 17 is said, and the crime goes before the court (`CRIME_VAGRANCY`; DFU sends guards to arrest). Inside a town's building, only a room rented there allows it ("You have not rented a room here."), and a rest stops when the room runs out ("Your rented room has expired."); no encounter comes in a rented room. Loitering is allowed everywhere. A dungeon, the wild, a farm and a lone tavern are free. A guild hall whose guild gives its members rest (the Fighters) is free to them; a house or ship to sleep in is not built.
- **Not yet:** a class with no spell point regeneration; resting in a house or a ship.

### Guilds (built: joining, ranks, training, members' benefits)
`play/guild.c` over MAIN.DAT's `GILD` (`tables/guilds.c`), DFU's Guild, FightersGuild, MagesGuild, Temple, KnightlyOrder and DaggerfallGuildServicePopupWindow.
- **Which guild:** the building's faction (FACTION.TXT): the Fighters Guild (41) or the Mages Guild (40) themselves, a temple's order by its god, its parent (the Akatosh Chantry, 92, under Akatosh, 26), a knightly order by its own (368..417). One of each group: one temple, one order. The Thieves Guild and the Dark Brotherhood invite by quest (P11) and are not in yet.
- **The panel:** anyone in a guild hall or a temple opens GILD00I0.IMG (Join Guild, Talk, their service) or, for a member, GILD01I0.IMG. Their service is their own faction's (DFU's GuildNpcServices: 849 Fighters' training, 850 their repairs, 813 a temple's cure, 240 a temple's quests...), written in capitals on the third row. A temple whose god has no row still cures.
- **Joining and ranks:** a reputation with the guild of 0, 10 .. 90 for ranks 0..9, and its skills: one at the rank's high mark and another at its low (22 and 4 for rank 0, 87 and 37 for rank 9). Join Guild says TEXT.RSC's welcome, or why not (a bad name, too little skill). Classic shows the eligible text and asks first; this joins at once. On each visit, 28 days after the rank last changed, it is looked at again: up ("You are now Swordsman."), down, or out when no rank is earned (TEXT.RSC 668). The titles are DFU's Internal_Strings' (Apprentice .. Master; Novice .. Patriarch; Aspirant .. Paladin).
- **Training:** a trainer's service lists the guild's training skills with their values on a scroll. A temple trains anyone, the others their members. 100 gold a level to a member, 400 to another; once in 12 game hours; not past 50. Three hours pass, fatigue goes by DFU's 11 × 180 64ths, and the skill gets 10 to 20 uses times its advancement (TallySkill), so it rises at the next rest.
- **Given:** the Fighters' hall to rest in (`guild_can_rest_in`, not a tavern, which the data marks theirs too) and their repairs at (10 - rank) tenths (`Shop.repair_rank`); Arkay's cure at (10 - rank) tenths; a temple's members healed whole on coming in from its healing rank (Arkay and Stendarr 0, Akatosh, Kynareth, Mara and Zenithar 1, Dibella and Julianos 2); a knightly order's free tavern rooms from rank 4, or in its own region.
- **Saved** as `MEMB` rows (the guild, the rank, the day it changed) and the last training's minute in `GAME`.
- **Not yet:** quests (P11); donations, identifying, spells, potions, magic items, teleport, summoning (P12-b); an order's armour and house; Akatosh's quicker travel, Kynareth's breath, Stendarr's saving from death; the Mages' library; the sheet's Affiliations button; a demotion's own words.

### Taverns (built: rooms, food and drink)
`play/lodging.c`, with the tavern keeper's panel (`SERVICE_PANEL_TAVERN`, TVRN00I0.IMG: Room, Talk, Food & Drinks, Goodbye), DaggerfallTavernWindow's rules and UESP's Taverns.
- **A room:** Room asks how many days (one to start, typed, Enter). 7 gold a day, Heart's Day not counted when the days take it in (FormulaHelper.CalculateRoomCost), then bargained by the tavern's quality, Mercantile and Personality (`rules_trade_price`). Renting again adds days to a room held there; 350 days at most. Classic asks before paying; this pays at once and says the price. The rooms are rows in the save (`ROOM`), by the building's town, block and record, and the game minute each runs out. A knightly order's member has the room free (from rank 4, or in the order's own region); DFU's own bed in the room, and things left in it kept, are not built.
- **Food and drink:** eleven things at DFU's prices (ale, beer, bread, broth 1; mead, cheese, gruel, pie 2; wine, fowl, stew 3). Each gives back twice its price in health, once in four game hours ("You are not hungry."). Free on the New Life Festival, half (at least 1) on Harvest's End (`HOLIDAY_FOOD_HALF`), as DFU found classic does; the halved price heals half. The last meal's minute is in the save's `GAME` row.


The travel map comes later, with `worldmap.c`'s pixels and the location index: pick a place and land in the stream there, time passing as the trip.

## Saves (built: P7; remade 2026-11-06: a list of characters)

`play/save.c` writes and reads one save, `play/saves.c` knows where they are. A save is a record file (`docs/data.md`) as KEYS.DAT is: `dfrec dump` reads it. **Nothing older is read**: GFALL's own files are not ported between versions (the user's rule: this is a reimplementation), so a change of format is a change of the version in the head, and an older file is simply not listed. Daggerfall's own files in `ARENA2` are a different matter, read in place as ever.

**Where:** `saves\<character>\S0001.SAV`, `S0002.SAV`, beside the program, numbered, never named by date. The folder is the character's name made safe for a file system (`saves_safe_name`): letters, digits, spaces and hyphens kept, anything else an underscore, trailing spaces dropped, Windows' device names (CON, NUL, COM1...) given an underscore at the end, and " (2)", " (3)" when another character has the name (compared without case). **Who a save is for is inside it**, never in the folder's name: `META` (the character's id, name, level, the real time saved, where the game was, in words) and `THMB` (the snapshot) are its first two chunks, so the list reads only the head of every file. Two Annas are two characters, since their ids differ (`Character.id`, rolled when a game starts, kept in every save). A file written to `.TMP` and moved over (`fs_move`) so a cut-off write never replaces a good save; a stray temporary or a file that is no save is not listed.

**The snapshot:** `gfx_thumbnail` reads the front buffer at 160 x 120, RGB 565, in `menu_over_play` before the menu covers the view (Escape in play). It is kept in memory and goes into the next save. The picture on the screen takes 38 KB of the card's picture room (`GAME_PICTURE_KB`, now 296).

**Where the game was**, in words, made by `place_words` in `game.c`: a town and its province (inside a building, "Inside, in" first), a dungeon and its province, Privateer's Hold, or "The wilds, near" a province.

| Chunk | Holds |
| --- | --- |
| `DISC` | the places the map knows that a new game's does not |
| `WAGN` | what is in the cart's wagon |
| `SICK` | a row per disease had: its days left, the day last felt, what it took from each attribute (`play/ills.h`) |
| `QDON` | the once-only quests accepted and the main quests started: one row, a bitset of `QUES` rows (`play/quest.h`) |
| `QUST` | a row per quest running: its QUES row, states, fired records, timers, gold, bound places, its people's and foes' places, kills, log (`play/quest.h`) |
| `MEMB` | a row per guild belonged to: its faction, the rank, the day the rank last changed (`play/guild.h`) |
| `ROOM` | a row per tavern room held: its building, by town, block and record, and when it runs out (`play/lodging.h`) |
| `CURS` | one row: an infection, a curse, its clan and shape, its times, and its gifts to attributes and skills |
| `GAME` | which place and the way back out of its door, the feet in that place's frame with the stream's origin that frame has, the movement facing and independent Alt view offsets (P9-c), the clock's minute and day, the day the game began, the seed, the gold, what is worn |
| `CHAR` | the sheet, whole: skill uses and the level's count among it |
| `PACK` | what is carried, material, variant and condition among it |
| `KEEP` | every door, lever, creature, heap and pile not as its file has it (`keep.c`) |
| `PILE` | what lies dropped (`pile.c`) |
| `FACT` | each faction whose reputation has moved |
| `SOCL` | the social groups' reputations and the lasting bonus |
| `REGN` | each region's law, bank and prices |

- **What is not written:** a body's and a heap's things, which come again from the seed and their keys (What can be searched). So a save stays small however much of the hold was searched.
- **Saving** first flushes the blocks loaded now into the keep table (the engine's `stream_keep_now`): a door opened where the player stands has otherwise only been saved in memory on its block's own arrays.
- **Loading** (`load_game` in `game.c`) reads the file, replaces the keep table and the pool, and returns `GAME_START`, the same path as a new game. `main.c` closes the place and opens it again, each place in a temp heap of its own as LEVELFALL's (`leave_place`), and `game_enter` puts the player at the save's place. Without its own heap every load leaked the stream's arrays.
- **A new game** forgets the last one's keep table and piles, and takes a new seed (`forget_last_game`).
- **The menus:** one screen, the Files tab of the settings shell (below, The list): the title's Load opens the shell on it (nothing to save), Escape in play opens the shell on it (with SAVE). Hostcheck uses a folder of its own (`saves_use_root`, `save_use_file`), so it never touches a player's.
- **Death** shows classic's DIE_00I0.IMG until a key or a click, then the shell's Files tab (Back, or no saves at all, goes to the start menu).
- **A save knows its place** (P8): GAME holds the kind of place, its region and index, a house's block and record, and the way back out of its door, as whole chunks and what is left because a record file has no doubles. The feet are in the place's frame with the stream's origin that frame has, so a stream opening on another origin puts them back in the same spot; a save from before P8 reads as the hold with origin 0, which is what its feet already were.
- **Creatures awake are not saved.** They come back asleep, where they stood, wounded as they were.

## Later

Talk (the people's lists, `USE_TALK`), factions, quests (QBN/QRC per UESP), spells and spell effects, shops and banks, the law, horses and carts. Each gets its own design here when its phase comes.

**At the end of the prototype (the user's call, 2026-11-04): P10's leftovers.** Not to start until the prototype plays from start to quests; each is small and lands on its own.
- buildings' own names, on signs, in talk's Where is list and on the town map (DFU's GenerateBuildingName, from TEXT.RSC's pieces);
- clicking a neighbouring province from a province's travel map;
- the indoor automap's door and exit marks, and notes;
- the town map's rotation;
- the region races into MAIN.DAT (`names.c` keeps FALL.EXE's table in code);
- a vampire's turning waking in a cemetery of the region, not where they fell;
- the beast's own head (`WERE00I0.IMG`, `WERE01I0.IMG`), paper doll (`WOLF00I0.IMG`, `BOAR00I0.IMG`), weapon and sounds; townspeople fleeing it;
- a bed of one's own in a rented room, and things left there kept;
- regional holidays;
- a ship to stand on (DFU's ship scenes);
- Yes/No before paying for a cure, a room, a guild's welcome, as classic asks.

**Texture detail for walls and for sprites (asked 2026-11-05, the ENGINE implementer building the engine side).** Two settings, Low / Medium / High each: one for the world's wall, floor and terrain textures, one for the sprites the player meets (creatures, townspeople, billboards, pickups). The weapon, the HUD and every screen picture stay as they are. The user wants walls lowered to keep sprites crisp on the Voodoo 1's 2 MB (a sprite sheet too large for 256 now loses half its texels in `texture_sheet`, and pictures coming onto the card stutter). Their ladder was 100% / 75% / 50% and then 100% / 50% / 25%; 75% is not a power of two, so the engine says what the card allows. The game adds the two rows to the Graphics tab (draft until Apply, kept in OPTS) when the engine's cvars are on main.

## Budgets on the 233

- **CPU**: the game's own work (rules, foes, HUD) aims at 2 ms or less a frame, so it costs little out of the 60 Hz frame the engine and the stream need. The frame profile (`docs/profile.md`) gets a `play` stage, and the number is set from the 233's log, not guessed.
- **RAM**: the sheet, the items and the save's change lists stay under 64 KB, kept for the process.
- **The card**: the HUD's pictures and the weapon's swing come out of the same 2 MB. They are counted in the budget the way sprites are (`docs/memory.md`).

## What the engine must give

Requests for the engine engineer, each in its smallest form:

1. **Game keys in the input snapshot**: attack (left button, held), use, run, jump, and the screen keys. `Input` today has movement, look and quit.
2. **The walker: run, jump, and a fall's speed at landing**, so the game can scale speed, start a jump, and take fall damage.
3. **The walker against entities**: the body stops at a creature or a person, and they stop at it.
4. **Moving an entity from game code**: set its position, facing and pose (walk, stand, attack, hurt, die) for a creature's motor.
5. **A 2D picture draw** for the screens and the weapon: a picture on the card drawn at a screen rectangle, under or over the HUD's text.
6. **A pause that is not the console**, for the game's menus: the world stands still (`gfx_paused`) while a screen is up.
7. **A `play` stage in the frame profile.**
8. **A body apart from the camera**: the walker split into bodies the game steps (as many as wanted, each its own size) and camera rigs that place a camera from a body (first person at the eye; third person behind it, pulled in from walls). The viewers' walker stays as it is, built on one body. The use ray takes an origin and a direction, not only a camera.
9. **The calendar out of the engine**: the engine keeps minutes and days, and the game installs Tamriel's months and seasons. Built: `clock_set_calendar` (`docs/daylight.md`).

## Data
Daggerfall's tables are records in MAIN.DAT, cooked on the first run, with mods over it in a load order (`docs/data.md`). Built so far: enemy kinds, encounter tables, weather odds and looks, the calendar. Each table below comes with the system that reads it:

| Tables | Holds | From |
| --- | --- | --- |
| `LOCS`, `WMAP`, `CLIM` | the index of 4232 dungeons and 15251 towns; the heights, climates and locations' pixels; each climate's numbers | MAPS.BSA, WOODS.WLD, CLIMATE.PAK (they cut the 233's start-up) |
| `ITEM`, `MAGI`, `WEAP`, `MATL`, `ARMR`, `LOOT`, `LDUN` | items, artifacts, weapons, materials, armour, loot, each dungeon type's loot table | the exe (via DFU), MAGIC.DEF; pictures stay in ARENA2 |
| `CMON`, `CWEK`, `CALE` | the calendar (built) | the exe; the engine takes it whole (`clock_set_calendar`, request 9) |
| `RACE`, `CLAS`, `SKIL`, `SPEL`, `EFCT`, `ENCH`, `POTN`, `BIOG` | races, classes, skills, spells, effects, enchantments, potions, background | CLASS*.CFG, SPELLS.STD, BIOG*.TXT, the exe |
| `EFCT`, `SPEL` (built: the effects and SPELLS.STD's 89 spells), `BLDG`, `CRIM`, `HOLI`, `DISE`, `GILD`, `NAME` (built) and `FACT`, `PEOP`, `POLI`, `LAW`, `PRIC`, `FLAT`, `MUSI`, `SWAP`, `CURS` | factions, names, townspeople, politics, guilds, law, prices, holidays, flats, music, swaps, diseases, curses | FACTION.TXT, NAMEGEN.DAT, POLITIC.PAK, FLATS.CFG, the exe |
| `QUES` (built), `BOOK`, `TEXT` | the quests offered (their files stay in ARENA2), indexes of the books and TEXT.RSC | BOOKS\, TEXT.RSC, QBN/QRC (their text and scripts stay in ARENA2) |

Blocks, models, pictures, sounds and songs are never cooked: they stay in ARENA2.

## Phases
The first goal is a **prototype**: from classic's start menu, Privateer's Hold played from start to finish, out to the first town and back after a save (P1 to P8). Then the rest of classic (P9 to P12-b), with P9-b completing the HUD, P9-c classic dungeon controls without mouse look, P12-a the theft mechanics, and P10 bringing the travel map before P12-b magic. Then a mode of our own (P13). Each phase:
- is measured on the 233 with the profile's `play` stage;
- adds its rules' tests to `tests/check_play.c`;
- adds its numbers to MAIN.DAT;
- gets a view in LEVELFALL.

| Phase | What | Who | Done when |
| --- | --- | --- | --- |
| P0 | Foundation: the layers, MAIN.DAT and mods, dfrec; the engine's keys, bodies and rigs, pause, poses, calendar, `DATA\` | all | built |
| **P1** | **On foot** (built, runs on the target). GFALL rewritten as the game's single play state: the player a body under `rig_first`, walk, run, jump and fall, mouse look; the use key on doors and levers; HUD crosshair, compass and message line; Escape pauses | GAME | the hold walked from marker 10 to its exit on the 233 |
| P2 | A character (built; its bars run on the target): a fixed level-1 one, classic's attributes, skills and vitals; vitals bars on the HUD; `RACE`, `CLAS`, `SKIL` | GAME, ENGINE's pictures | derived values match UESP's worked figures |
| P3 | The start menu (built; the title, the start menu and the start into the hold run on the target, the options and controls not tried there yet): classic's main menu (PICK03I0.IMG: load, start, exit) before the hold, the title before it, Escape's menu in play (go on, exit) | GAME, ENGINE's pictures | GFALL opens on the menu, starts the hold, and exits from both |
| P4 | Combat (built; the swing runs on the target, striking and creatures awake not tried there yet; the player's death is back to the start until saves; a creature's state is the console's `foes`, as LEVELFALL does not link the game): the weapon's swing from WEAPON##.CIF (the shown swing's frames only on the card), classic's to-hit and damage; creatures wake, chase on bodies of their size, attack, flinch, die; the player's death; `WEAP`, `MATL` | GAME | the hold can be cleared, or the player killed, from records |
| P5 | Loot and inventory (built, host-tested, not run on the target; the other tabs' items not yet): bodies, treasure, dropped piles, gold, pick up, equip weapon and armour, classic's inventory screen and paper doll, classic's metal colours; `ITEM`, `ARMR`, `LOOT`, `LDUN` | GAME, ENGINE's pictures | equipment changes combat |
| P6 | Character creation and levelling (built, host-tested, not run on the target): classic's screens, class questions and custom classes among them, background questions, skill gains, level-ups, the character sheet | GAME | a character made by the screens plays the hold and levels |
| P7 | Prototype polish (built, host-tested, not run on the target; the 233 profile pass not done): loading holds, one save slot in `DATA\`, the menu's load, rest and wait, a 233 profile pass | GAME | from the menu through the hold and back after a save |
| P8 | Out of the hold (built, host-tested, not run on the target): `places/travel.c` lifted from LEVELFALL and shared; the hold's outside in the stream, the nearest town, its houses | GAME and LEVELFALL | from the menu to town and back after a save, in one session |
| P9 | People and assault (built, host-tested, not run on the target; people's names built in P10; buildings' names and the court for a street killing not yet): talk from TEXT.RSC, shops, banks, factions, reputation, law; a street citizen at 1 health, a blow at 100 that the weapon's damage kills, blood at the body, every loaded guard hostile and still wanted after a building, talk refused while hostile, and a killed guard's corpse to loot; `BLDG`, `CRIM` | GAME | a shop bought from, a bank used, a way asked in town, a citizen killed and the guards on them |
| P9-b | Classic HUD bar (built, host-tested; pointer and target acceptance pending): selected character's head and blue backing, live vitals, clickable buttons, four mode icons and compact compass; Tab frees the pointer, the weapon button sheathes | GAME, ENGINE's input and pictures | the selected character's head and vitals shown correctly, available controls usable by pointer, across saves and place changes |
| P9-c | Classic dungeon controls without mouse look (implemented, host-tested): free pointer, cursor/keyboard movement and turning, pointed use, right-button gestures, Alt look and saved scheme; target acceptance pending | GAME, ENGINE's input | the hold navigated and fought through, doors and HUD used, and a save resumed without continuous mouse look |
| P10 | The wider world (first slice built, host-tested, not run on the target: the travel map, its journeys and the places known): travel map before magic, horse and cart, holidays (built), diseases, lycanthropy and vampirism (built) | GAME | a journey from the map to a town and a dungeon, and back after a save |
| P11 | Quests: the QBN interpreter, guild quests | GAME | |
| P12-a | Steal and theft (slice 1 built, host-tested: a citizen's pocket; the rest planned): classic Steal mode, pickpocketing, deferred theft paths, skill checks, detection and law consequences; build on P9's shoplifting and lockpicking | GAME | theft and pickpocketing can succeed or be detected, with the right items and legal consequences kept and tested |
| P12-b | Magic: spells, effects, spellbook, casting, potions, enchantments | GAME | Built and host-tested, not run on the target: the projectile (arrows fly), the effects and standard spells as tables, costs, the book and its saves, effects by the minute, casting (the hand, missiles, bursts), the spellbook screen with Buy Spells, the spell maker. Left: creatures that cast, the effects not built (attributes, light, levitate, invisibility, teleport, lock and open, detect, charm and the rest), potions, enchanting, magic items |
| P13 | Our own mode: a ruleset switch over classic's rules, a mode `.DAT` over MAIN.DAT; classic stays the default and the reference | GAME | |

Each phase updates this page (what is built, what is not) and AGENTS.md's layout list.
