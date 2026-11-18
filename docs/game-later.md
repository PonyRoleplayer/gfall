# GFALL: known bugs and gaps, to fix when the dagger layer is decoupled

Found while building people and law (slices 1 to 9 and the shops, guilds, potions, spells and magic items after them). Nothing here blocks play; each is either a
quirk of the original that we kept on purpose (and could switch off in a later "improved" mode), a place where GFALL leans on the dagger layer, or a shortcut taken
for now. Each line names where to look.

## Original behaviour kept on purpose (decomp quirks)

- **Potion maker keeps stale keys.** Using the ingredients up empties the cauldron's cells but not its keys, so a second Mix at once makes the same potion for nothing; a recipe loaded after a larger one
  leaves the larger one's keys in cells past its own. Cleared only by reopening the maker. `play/magic/potion.c` (`use_up_cauldron`, `recipe_load`), held by `tests/check_potion.c`.
- **Blessing's legal gift comes off the wrong region.** It is added to the region the player is in and taken back from region 0 (the record never names a region). `blessing_buy`, `tests/check_guild.c`.
- **A blessing's amount is the whole even when the stat was held at 100**, so the end takes off points that were never added. Same functions.
- **A second blessing is refused with record 454** ("not enough gold"), the original's own message for it.
- **Armorers and general stores test the faction number's bit 8** where the other shops test the person's flag, so the repair panel can fail to open. `gfall_talk_used` in `apps/gfall/people/people.c`.
- **Rank refusal 3100, "reserved for members only"** for the Thieves' and Brotherhood's strangers shows nothing at all (the original's `case 3` with no membership just breaks).
- **A Thieves' pickpocket that found nothing is not counted**; only gold counts (`steal_pocket`).
- **Each person's pocket is tried once, win or lose** (`click_pedestrian` marks the person first).
- **Magic item prices forget the base item** (`enchant_item_value`), and a parameter of 255 (to be chosen) costs a spell that does not exist, so nothing.
- **A guild's rank scan would run past its table for a skill over 94**; held at 10 (`guild_rank_for_skills`).
- **The Mages' identify seller (801) dereferences a null membership for a stranger** in the original; we say "reserved for members only".
- **The unknown brew's spell keeps whatever the spell maker's record held before** (fields the brew does not set); we use zeros (`brew_record`).
- **Daedra: the fourth day entry is a sentinel**, not a day (`daedra_offer`).

## Deviations we took (to be made faithful)

- **A promotion's message replaces the guild menu for that click**; the original shows the message and then the menu (`hall_enter`).
- **Guild healing skips record 350 when attributes are down** (it asks 403 instead); the Mages' magicka restore for a caster is not done, nor the disease/effect cleanup of the player's spell effects (`hall_heal`).
- **Room offer's offer text** uses days * 7 as the total, not the original's stale `trade_total` (`inn_days`); Heart's Day's free night is in the price but not the total.
- **A tavern with no bed marker still rents a room** (the original takes the gold and rents nothing); the beds are the inside's editor markers of record 4, read through `world_marker_nth` (a dagger accessor added for it).
- **The potion maker's name box always says "Unnamed potion"** (the original shows the recipe being loaded).
- **Teleport shows the ordinary journey popup**, not TELE00I0's yes/no (`g_teleport` in `apps/gfall/screen/maps.c`).
- **The spell shop's list is seeded with the person's own number**, the original with the high word of their record id; people's ids are ours (`tag * 31 + faction`).
- **Magic-shop stock is not flagged "not owned" (32)**, so the price scale and "unidentified" bits of those items are not there.
- **Doors keep hours now** (`entry.c`), but the building's own flag 16 and access level are not read from the blocks, a house's "by day" is the clock's 6..18 (`is_daytime` is the sky's), and a door picked is remembered by Lockpicking value in a table that is not saved.
- **A shut shop's shelf opens as a plain container** with no crime for taking from it (the original may note theft); house furniture's "private property" prompt (record 37) and its alarm roll are not built (`shelf_used`).
- **Direction words** use GFALL's compass, not `xn_math_angle_to_point`'s bands; the building distance is `longer + half the shorter`, not `xn_math_approx_dist2d` (`talk_say`, `talk_distance_to`).
- **The list popup's place (x 100..220, from y 26) and the yes/no box's size are guessed** from the other screens, not read from the original's `list_popup_*`.
- **At most four messages queue** on the message page (`g_queue`); more are dropped.
- **The dialogue’s “Copy to logbook” button** does nothing.
- **Spellbook holds 128**; a purchase past it fails silently.
- **Citizens never fight back** (as in the original, a swing kills them outright); the "people fighting back" were the guards. A citizen killed is gone with no corpse; witnesses count the victim, as the original's order has it.
- **Guild invitations are counted but never delivered** (letters are quests); a Brotherhood kill is counted for a watchman in a dungeon only, not for a pedestrian (`guild_count_crime(6, 5)`), (a picked street door now counts for the Thieves; a dungeon door's pick does not, as in the original).
- **The Daedra's weather is not passed** (0), so a prince's liking for a weather counts only when he likes any; his quest and his ally are not built.
- **No weather, `climate_update` or rest after a teleport.**

## Where GFALL still leans on the dagger layer

- The app adapters in `apps/gfall/` still translate scene entities, building
  records, travel data, quest text and original pictures into the interfaces
  used by play. Their responsibilities are listed in [architecture.md](architecture.md).
- **Macros** (`%key`, `%loc`, `%di`, `%hnt`, `%a`, `%cn`, `%dwr` by hand, `%it` by hand) are the dagger layer's, which "is differently implemented"; several records (the rumour and quest macros) are refused or left raw. A decoupled expander should read TEXT.RSC's macro table from FALL.EXE.
- **People** are the dagger layer's entities: their numbers are made up (`tag * 31 + faction`, the dagger `who`), not the original's record ids, so names, the spell shop's list and a person's talk cache differ from the original's for the same person.
- **A person's shopkeeper flag** is found by matching picture and faction among the building's people (`interior_shopkeeper`), because the usable does not carry it; two people of one faction and picture would be confused.
- **Building ids** are `(block + 1) << 8 | record`, not the original's `location << 16 | n` (`talk_building_id`); `names`'s sign seed is the RMB's first word.
- **The region-to-name-bank table** read at 0x1841E3 has only banks 0 and 1; the original's regions should reach the Nords' bank. Unchecked against the original's output (`g_region_bank`).
- **Names are not checked against the original's output**, only their shapes and seeding (`tests/check_names.c`); the same for building signs.
- **Houses for sale and their price** (model radius * 5) need the dagger layer's models; so the bank's house, a knight's house and the town's `bank_add_house_for_sale` wait.
- **The town's watch and people are the dagger layer's pool** (`people.c`): a guard is a person of archive 399 taken into GFALL's creatures (`street_enlist`), a citizen turned into a guard is a guard made near and the citizen removed (the pictures are not the citizen's), blood splashes are not drawn, and a killed guard's corpse is the dagger's picture. The pool recycles slots: guards stay held until the place closes.
- **Quest persons** are not told apart from pedestrians; no person is "a potential questor".

## Tests that approximate

- Several flows (a hall's cure and blessing, the innkeeper's room and food, the potion shops through a hall, the magic shop) need a building to stand in and are not driven; only their rules and the windows are.
- `tests/check_world.c` finds a person by scanning entities and calls `activate` at a made-up camera; it proves the wiring, not the aim.
- The legacy-save fixture in `tests/check_state.c` adds the wire size of each new block by hand (`stand`); a new block means a new term there.

## Fixed 2026-11-10
- A crouched player's blows and aim started 35 units under the eye, inside the floor: `chest()` in `play/combat/dungeon.c` now takes the middle of the body (test in `check_combat.c`).
- Looking down turned sprites upside down: the lean's sign in the engine's `flat_facing` (`scene.c`) was wrong; the camera-facing up is `(fx*sin, cos, fz*sin)` for the engine's pitch (tests in `check_engine.c`). An engine file, edited because no engine session was listed.

- A killed town guard leaves a ground pile (`ground_pile` with the watch's loot table) the use key opens; emptied static treasure piles and corpses of the Hold and dungeons are hidden and unusable once their container has no rows (`container_emptied` in `apps/gfall/world/world.c`).
- A citizen struck down gets a second of blood (TEXTURE.380 record 0, in the people pool) and the death sound before they go. The original kills a citizen at once and they never fight back: the counterattack is the watch summoned by a witnessed murder.

## Still open from the same report
- Crash entering a building while pursued, after being inside another: not reproduced by the host test (two enter/leave cycles with the watch active) nor under AddressSanitizer. Needs the target's log or a save near it.

## Rest and sight rays (2026-11-10)
- Resting was refused for any non-passive creature within 1024 units, asleep or not, through walls; the original refuses for any creature among those collected round the player (`creature_list`, `world_collect_object`). A sleeper now counts only when the player is in its sight; an awake creature always counts. The exact collected area is not yet read out of the decomp.
- A sleeping creature cast its sight ray (a collide gather of the whole box between it and the player) every frame, which filled the log with "collide: N triangles past the 1024" warnings; it now looks only when its 0.2 s think timer is due. Awake creatures still cast one a frame.
- The parchment (`lay_paper`, SPOP.RCI's nine 22 x 22 tiles) was blitted opaque, so its torn edge's index-0 pixels showed as black dots; they are the cutout's clear texels (GFALL's `canvas_scroll` already cuts them out) and are now skipped.
- A creature's spell followed the original's bands only loosely: a ranged spell from 256 and any "touch" spell at any distance up to there, each half second, with a cast sound and no shot (imps). Now, as `ai_creature_think`: ranged past 360 (1 in 40 a try), touch within 90, nothing between (it closes in), and never a spell the player already has on them.
- A creature's sight ray gathers the whole box between it and the player, wider than `COLLIDE_SET`; the overflow is expected there, so `collide_quiet` (engine, `collide.c`) mutes the warning for those gathers only. A very long ray can still miss a wall in the dropped triangles; the walking body's gathers still warn.
- Sight rays are now gathered and cast in 384-unit stretches (`wall_distance`, `dungeon.c`), so no stretch overflows the collision set and no wall on a long ray is dropped. Before, a ray's whole box overflowed, walls fell out of it, and far sleepers "saw" the player through rock: they woke, which blocked Rest and cost frames. Awake creatures pay up to five small gathers a frame instead of one huge one; measure on the target.
- The character sheet's points-to-place number sat about six lines too low in the attribute arrows' notch; it now takes the creation screen's offset from the arrows (x + 7, bottom 14 down; `paint_character_sheet` in `apps/gfall/screen/possessions.c`). Seen only in the user's screenshot: check it on the target.
- A street guard has no key, so no container and no gear (`foe_equipment` needs one) and the watch has no loot table: its pile was empty and never shown. At its death the pile on the ground is made and the gear made into it (`dungeon_gear_to`); test in `check_world.c`. An enlisted guard is marked hostile so the street's talk refuses it, and the offer to surrender is now made under god mode too (it is the law's, not the body's).
- Messages (`status_line`) were drawn at `ui_string(8,24)`, whose y counts up from the bottom: they landed inside the HUD bar. They are centred along the top of the view.
- Pressing E on a locked shop door asked `street_witnesses` for every person within 4096 units, a wall cast each; it now stops once both bits are set, skips a person whose bit is set, and looks 2048 out.
- A shop shut for the hour answers its door with FALL.EXE's "Store is closed. Open from %d:00 to %d:00." (the decomp shows it on click_show_building_info, for types below 14 but the tavern; GFALL shows it on the door, in modes other than Steal). The building's name on the click of its wall is still not shown.
- The shopkeeper's offer (Accept/Reject) was a flat box with words; it is now SPOP.RCI's parchment with BUTTONS.RCI's Yes and No pictures (msgbox_show_string's look). The court's tall questions keep the box.
- Buying (the shop's inventory) turns gold coins behind each thing, shelf and basket alike (TEXTURE.434 record 6, 0.08 s a frame, by the cycle counter), so a shelf reads as for sale and not as loot. The screen is repainted when the frame changes. Not measured on the 233: each repaint recomposes the 320 x 200 screen.
- The temple's donation prompt used 1000 as a TEXT.RSC record (it is `msgbox_prompt_number`'s default amount; the words are FALL.EXE's "Donate how much money"), so it showed the armour info template. It now types 1000 in first (`possessions_number_from`). The typed-number prompts (gold, name, spell name, answer, page, donation) sit on the parchment with the Yes and No pictures like the offer.
- The Status key showed only the active magic and ailments. `status_show` (pickbook_0004259C.c) is two boxes: TEXT.RSC 22 with its macros said (the town, the time `%d:%02d`, the date, the region and the law's opinion of the player from FALL.EXE's thirteen reputation names, `legal_title`), then the health status (record 18 when well). GFALL shows the first box, then the ailments, then ours: the active spells. The dagger reader does not know %tim, %dat, %crn or %ltn, so `seam_status_intro` in `apps/gfall/boot.c` says them itself.
- The street's guards were never taken into the watch: `street_convert` and `street_witnesses` skipped everyone `people_is_citizen` refuses, and that is false for a guard, so the guards already walking the street neither came for a wanted player nor counted as the witness that brings the watch (`people_check_witnesses` bit 1). Only the ones made near were. Both loops now take a citizen or a guard. In the original nothing makes more guards once those are dead (guards_summon is called by a crime, a deferred witness timer, and, each minute passed, a 5% roll while a region's legal reputation is under -10 or a punishment flag is set: crime 7, `tamriel.c` 245..254). That minute roll is not built.
- House containers (chests, cupboards, 418 props, `OTHER_CONTAINER`) were ignored by GFALL. They are stocked on first use by `shelf_stock` from the model's variant (carried in the usable's `hd` by `doors.c`), empty ones say nothing is there, the player's own house opens freely, anyone else's asks (TEXT.RSC 37) and a grab can be seen (pickpocket roll against 108, then theft and the watch), per click.c 960..985. Whether the red wardrobe of the user's report is one of the variants is not checked; the original's list is variants 3, 4, 7, 8, 32..38, 50, 51.
- The sheet's Affiliations button always said "You have no guild affiliations." It now lists each membership (`sheet_show_affiliations`: faction name and the rank title from FALL.EXE's `faction_rank_names`, 22 kinds of ten) and says TEXT.RSC 19 only when there are none. The original's tabbed columns (`\xfb\x80` in its format strings) are a colon here.

## Still open after the 2026-11-10 bug pass
- Crash entering a second building while pursued by the watch: not reproduced (host test with two enter/leave cycles, AddressSanitizer). Wanted: a save or the log from just before it.
- The watch's roll is built (`law_time_pass`, `tamriel.c` 247..254): once for each pass of game time, not each minute, 4 % under -10 legal reputation and 9 % banished (the banishment's bit is never cleared, as in the original), crime 7, only while the world runs. `guards_summon(0)` reads our fresh witnesses, where the original reads a stored byte that can be stale. Still open: the post-court no-damage flag (`D_001962B2`).
- House containers: whether every piece of furniture the user finds unlootable is outside the original's variants (3, 4, 7, 8, 32..38, 50, 51) is unchecked; a container's stock uses `shelf_stock`.
- The Affiliations list uses a colon where the original has tab columns; the town's building name is not shown when a wall is clicked (`click_show_building_info`).
- Coins behind the shop's things repaint the 320 x 200 screen every 0.08 s: unmeasured on the 233.
- Shop coins, a cheaper repaint (do after measuring on the 233): the coins turn behind the items inside the one composed 320 x 200 buffer, so each 0.08 s frame recomposes the whole screen and re-publishes the surface. Instead recompose only the item cells (coin, then icon, then count) into the kept buffer and hand just those rectangles to `pic_update`, which already re-sends only the changed tiles.

## Magic numbers and folders (user's request, 2026-11-11)

- The quest system and much of GFALL carry bare numbers: opcodes in `case 52:`, flags (`0x2000` talk passes, `0xA00` hidden, `128`/`0x8000` topic hidden, `512` item used), QRC message ids 1000..1009, rumour expiries (30 and 180 days), item group/index pairs, state and section indexes, `QUEST_BLOB`. They mirror the decomp, which names almost nothing.
- Plan, as its own mechanical commit with `make all` green before and after: names taken from the decomp documents (`QOP_REPUTE_EXCEEDS`, `QPERSON_PASS_TALK`, `QRC_OFFER`), one header per domain beside its code (`quest_ops.h`, `quest_flags.h`, `item_groups.h`), constants for generated tables emitted by the same `tools/gfall-*.py` scripts, no behaviour change.
- The play files are now grouped by subsystem. Building record IDs and classic canvas sizes already have shared headers. The quest opcode and flag pass is still open.
- Do it after the quest machine is finished, not between its steps.

## Dropping the  /  prefixes (user's request, 2026-11-11)

- GFALL will replace GFALL, so ``, `` and `Og` prefixes (functions, macros, types, files like `gfall-*.py`, `check_og*.c`, `docs/gfall*.md`) go once GFALL's `src/game/dagger/play/` and `apps/gfall/` are retired: the prefix only exists to keep the two apart.
- Order: retire GFALL first (nothing then collides), then one mechanical rename commit (sed over `src`, `apps`, `tests`, `tools`, `docs`, Makefile), `make all` green before and after. Do it together with or just after the constants and folders pass above, so names are touched once.
- Watch for collisions with the engine and the formats layer (`` removed from `item_create` must not clash with a dagger or engine `item_create`), and for save-format identifiers: the `OGSAVE` magic and schema numbers stay.

## For later (written 2026-11-11, end of the quest-machine and entity-plan session)

- **Entity plan step 10**: street people (`people.c`) onto the registry, hooks in `wild.c` (a town's chunks), remove `people_in_view` (`docs/entity-plan.md`, section 5).
- **Stream GFALL's dungeons** (it loads them whole: `travel_set_dungeon_streaming(0)`): measure on the target first (the castle: 16 blocks, 1235 models, 29.8k triangles, 128 creatures, 356 pictures; the largest, Scourg Barrow: 22 blocks, 34k triangles, 424 pictures). If the load or the RAM is a problem: minds attached and dropped as creature chunks fill and go, doors/levers/treasure built per chunk, keep rows filed under chunks (the registry supports it), quest markers already numbered alike.
- **Quest machine, still open**: the MAPS site list read for exact sites (`dungeon_sites` reads it; the numbering replay is documented, quest-places-and-start.md 8), the Where-is "People" list, noble quests (need the regions' event flags), the vampirism and lycanthropy cures, `%pqn` naming the exact person, a playtest of the whole quest machine on the target.

## Nothing asked every frame or minute reads a file or walks the world's lists
Ask once when the place opens and keep the answer. `dungeon_outside` keeps its last answers (`docs/dungeons.md`); `exterior_find_number` finds a fixed place's outside with one open of MAPS.BSA and no list copies (the first frame of a new game opened it 72 times); `town_here` in `play/quests/quest.c` keeps the answer for the place the player stands in. `quest_here` still calls `travel_capture` each question, cheap now that the outside is kept. The profile's `PROF_PLAY` stage shows the game's own frame: look there first.

## Fixed 2026-11-13 (after the VM playtest)
- **Spell points go at the cast, not at the choice.** `ready` in `play/magic/casting.c` only checks the cost for a spell with a target; `pay_cast` takes the points, trains the schools and notes `EVENT_SPELL` when the attack button lets the spell go (`cast_paid`). A spell on the caster is cast at once and pays then. Putting a readied spell away costs nothing. A recast pays the last cast's price (`magic_state()->paid`); `last` is set at the cast, not at the choice. An item's spell pays nothing, as before.
- **The status line is a feed** (`play/ui/msg.c`): up to five entries, the newest on top and each older one a row lower, fading over the last two seconds of seven (`ui_string_alpha`), a sixth dropping the oldest, a line equal to the newest renewing it. `msg_tick` runs in `play_frame` on real seconds. Test: `check_feed` in `tests/check_ui.c`.
- **The message parchment fits its lines to its width** (`notice_lines` measures with the app's font through `possessions_set_measure`; 250 pixels). Quest records arrive broken into lines of about 45 letters with a space starting each continuation; `join_lines` in `play/quests/quest.c` joins them so a parchment line is not a full row plus a short stub.
- **A flyer killed in the air falls.** Flying bodies have no gravity, and a dead foe was never stepped again, so a bat or imp killed aloft hung there (user's screenshot, VM). `corpse_fall` in `play/combat/dungeon.c` gives the body `WALK_GRAVITY` and steps it until it is grounded; a flyer already on the floor is not stepped. Test: `flyer_corpse` in `tests/check_combat.c`.
- **Quest op 84 plays its sound** (`play/quests/quest.c`): S0000977 is the ghost of Lysandus screaming in Daggerfall at night (state 3 = in Daggerfall and not between 05:00 and 19:00; clip 386 every 5th minute at 25%). The console's `quests` lists each running quest, how many of its states are set and its running timers (`quest_commands`), to see why a quest waits. Not yet tested: op 84 has no host test.

## Vampirism and lycanthropy: what is left (2026-11-14)
See docs/game.md, Vampirism and lycanthropy, Not built. The form lives in `CurseState`, not in `CharacterState.race`: a decoupled game that reads `race > 7` must read `curse_form()`.

## Long-campaign save limits (2026-11-16)

The schema 18 serializer can write at most about 288 KiB per slot with the
current record limits. Its wire buffer is 512 KiB. Both are GFALL limits.

- **World state.** `state_capture_world` in `play/state.c` accepts 512 keep
  records and refuses to save at 513. Host probes verified both cases. Dungeon
  creatures can create keep records during initialization, so visiting dungeons
  can consume the budget without combat.
- **Automap.** `world_capture` in `play/travel/travel.c` keeps the first 8,192 records.
  Exploration beyond that cutoff can disappear on reload.
- **Quests.** `quest_pack` has a 24 KiB budget. If it overflows, `save_write`
  refuses the new save before replacing the existing slot.

Deferred work. Store world changes by place and omit records for creatures
still in their default state. Keep the active place's records in memory and
read other places on demand so a long campaign can grow on disk within the
64 MB target's RAM budget. Preserve existing saves and test a campaign that
crosses today's world and automap limits, revisits changed dungeons, and saves
and reloads in a fresh process. Raising the wire buffer alone leaves the
record-count limits in place.

## From the constants pass (2026-11-18, docs/gfall-constants.md in the decomp)
Fixed: a creature hurt gives up after 60 ticks, not 200; the second blood picture is the undead's; an arrow on a wall fires trigger 5; Repairs Objects skips arrows; a swimmer over 250 does not sink while levitating or water-walking.
Open, with the decomp's value: a class enemy's armour is its creature row ×5 (20..40), ours 60; the city watch's level is the player's + rand(3,6); a creature rolls `rand() % SPD < SPD/8 + 6` before a swing; a knocked creature that hits a wall takes speed/2 and the knockback stops at 5; a flag-2 creature gives up a target under a score of 8; the encounter table rerolls a Daedra Seducer, a Nymph or entry 42 when DAGGER.GRD exists; the material roll lacks +60 for an enchanted item and -(14 - quality) * 2 in a shop; a recipe is `rand() % 20` in the count, not 1..20 in `message`; silver armour rolls its material twice; `magic_resist` lacks the biography modifiers (resist magic, disease, poison); Potent vs and Low damage vs sum every entry (the original counts them only when all ten slots are full: kept as a fix); the original reads the attacker's race for hit effects even for the player (docs/curses.md 11).

## Differences from FALL.EXE found by the constants document (2026-11-18)
The full list, with the decomp's value and file:line, is the closing summary of `docs/gfall-constants.md` in the decomp.
**Fixed (2026-11-18):** the bow waits N - 200 ms (`BOW_RELOAD_FLOOR`); the Open spell adds (level - lock) x 5, clamps 5..95, rolls 0..100 and cannot open a lock of 20; a knight pays no inns or ship fare and a trip that cannot be paid takes all the gold and goes; a bed in the Fighters Guild hall is for its members, not the knights; temple 142's fourth service (456, daedra, rank 7); a Mages Guild member whose class cannot regain magicka has it filled and is not healed; a stranger's blessing is 4 days and 8 points; a knight's armour gift is one material lower (rank 1 rolls the usual material); Kynareth's members hold 3 more breath a rank; rapid healing is flag 1 by day, 2 in the dark, 4 always; icon-250 strike spells merge with the same effect; a creature casts at range only under 2048; every creature's armour is its row x 5 (`FOE_ARMOR_SCALE`; it was the raw row, 4..8, so creatures were far easier to hit) and a class enemy's the row of its class creature (4..8, read from FALL.EXE); the courier of op 76 (Brisienna's letter) comes only in a town's streets, by day (a vampire's by night), while the world runs, one roll a game minute.
**Open:**
- **combat/**: the watch's level +rand(3,6); the SPD swing roll (`rand() % SPD < SPD/8 + 6`, per AI step: our steps are per frame, so it needs the original's step first); knockback wall damage and the stop at 5; giving up a target under a score of 8; poisoned weapons from player level 2 (an assassin 59%, others 4%); the table seed is the location number (check `place_key`).
- **magic/**: the resist roll's biography modifiers (magic +4, disease +0, poison +12); the area spell's knockback (radius x 30); the spell shop's stock is seeded with the place's half of the seller's id (ours: the person's number, so each seller has a list of their own).
- **items/**: the info texts 1005, 1006, 1008, 1012..1015, 1073 and the recipe's.
- **travel/**: the link trigger's `& 15` (a lock-bit trigger never fires in classic; the scan stops at the first mismatching link and skips a running chain); trigger 6 becomes 2 and param 108 becomes 100 at load; an owned ship (none in ours) sails at 51 and pays no fare.
- **people/**: `player_reaction_mod(100)` for groups past 4 and the biography's `reputation_mod`; coven summoning; classic never stocks model-410 furniture (ours does, `shelf_model`).
- **character/**: the biography's FT (no drain one call in four); "enemies about" is `creature_count`, not a radius; the mastered-skill notice (text 4020, sound 34); a level-up waits for `levelup_check` (no creatures, no menu, not a beast form); the archer's bow takes an ordinary material roll; a second new game in one run starts 3 days earlier; the courier of op 76 also wants no creature near (the quests are not told of creatures).
- **input/**: the slowfall cap, the pitch limit and the turn rate are untraced.

## Overdue loans (2026-11-18)
Built from `world-economy-loans.md` (`bank_loans_due`, `bank_collectors`). FALL.EXE reads account 0 only (its loops never advance the pointer), so a late loan does nothing outside the Alik'r Desert and there sends 62 groups of collectors; GFALL takes the intent instead: the late account's own region loses 5 legal reputation once as the due minute is passed (the original needs a pass that ends exactly on it), and while the loan of the region the player stands in is overdue a check every 1400..1700 minutes, never in a dungeon, sends one group of 2..5 hidden hostile class creatures (Spellsword, Warrior, Battlemage, Assassin, the player's level). No text, no loot, the loan untouched; the "defaulted" flag and texts 288, 297 and 360 are never used, as in the original. The next check is not saved (after a load it is a day off).

## Access, picks and bashing (2026-11-18)
From `world-economy-access.md`. Built: the crime roll after every inner-door pick attempt, even a refused one (`pick_crime` in `doors.c`: a witness, or 1..300 under 100 less Stealth, crime 1 and the watch); a building's street door struck (`gfall_building_bash`: 9 % it gives, flag 16 is kept for the visit in `entry_set_bashed` and the player goes in if the building lets them, a bashed shop stays shut outside its hours; else, over Stealth and no guard about, crime 0 and the watch: the court ignores crime 0). Open: the access grant on entry (5 until closing, 255 for the own house, none for a tavern) and the interior trespass from the RMB section 3's restriction bytes (`rmb.c` skips section 3; houses and taverns need a decision, the faithful reading makes them trespass), dungeon action 99, the Lock effect's roll on closing a door, and clearing the bashed flags when the town reloads (`entry_reset` has no caller).

## Houses and ships (2026-11-18)
From `world-economy-property.md`. Built: the bank's Buy House lists the houses for sale of the town (type 1 buildings, at most 20) as rows of "Price : %d gold", each priced at five times its largest model's radius (`town_house_price`, the int at +12 of the ARCH3D record; unverified against the original's prices); a house is paid from the account balance (454) and one at a time (286); Buy Ship lists the two ships (100000 and 200000), only in a port (`exterior_is_port`: bit 16 of the byte at 0x2F of the exterior data, the 39 ports; the original's bank sells wherever it is not 0, which also counts eight small towns with a low value) and one at a time (284); an owned ship makes every sea pixel cost 51 and no fare. Open: the list is a plain list without the original's turning 3D preview; the saved region and town names (`%hrg`, `%htwn`) and the house's marking on the town map when leaving it; the knights' Seneschal's free house (462; `bank_gift_house` exists, and its sale offers 85 % of a stale price as in the original); the house's contents kept and restored (`house_container`), its furniture opening with no question, "Your house." as its name; boarding the ship from the transport menu (region 31, location 1 or 2, "Your Ship"); the sold house's contents staying as the original leaves them.

## Daedra summoning (world-economy-daedra.md)
Built: a first success starts the prince's quest (`<code>0C0<letter>Y00`, Malacath's with `x`; Sheogorath replacing Malacath keeps the stale letter and starts nothing, as the original); a repeat success is the prince's angry servant (26/25/27/29/31), hostile, put beside the player by the frame (`daedra_servant_take`); the covens (419..432) draw the day's prince from the day number (never Hircine, Glenmoril always Hircine, no day refusal, no enemy test); the weather byte carries the storm mark (a thunderstorm is rain 4 + 128, so the rain princes lose the +30).
Open: the coven menu screen (DAED00I0.IMG: TALK, SUMMON, QUEST by level) is not built, the covens' summon is reached only through the guild service; the prince's FLC is not played with text 482 and texts 482/484 do not name the prince; ChildGard (DAGGER.GRD) is not read; the draw's seed leak is not kept.

## Curses: cure adventures, the crypt and the dreams (curses-cures-and-dreams.md)
Tested end to end (`check_quest.c`, `check_cures`): `$CUREWER` (the letter, the priest's choice, the alchemist's exactly 1000 gold, the local, the child, cure, -10 and -30 legal, the Glenmoril road with no cure) and `$CUREVAM` (the letter, vamp1 with and without the letter shown, the bracelet to vamp2, the bloodfather's death cures and costs 20 reputation, timer 0 costs 15). Found by the tests: op 29 (ask yes or no) had its QRC id in operand 3, not in the message field, so no real quest's question was ever asked (fixed: `QOP_ASK_MESSAGE_ARG`), and a box left open by an old game blocked the next quest's (`quest_reset` clears it).
The crypt: the region's n-th dungeon of any type, n below the number of crypts (the original's bug, kept), entered on its start marker (`travel_go_into_dungeon`). Open: the move to the kind-9 marker nearest the start marker, and the knights' spawns during the hunt are not asserted.
The dreams: `curse_dream` returns the movie (`DREAM_MOVIE_LYCANTHROPY` 2, `DREAM_MOVIE_VAMPIRISM` 4) once, after the closing line and before the skills check; `rest_set_dream` is the hook for the movie player, which is not built yet (frames, audio, Escape: docs/video.md).
