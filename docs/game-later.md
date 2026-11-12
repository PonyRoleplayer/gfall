# GFALL: known bugs and gaps, to fix when the dagger layer is decoupled

Found while building people and law (slices 1 to 9 and the shops, guilds, potions, spells and magic items after them). Nothing here blocks play; each is either a
quirk of the original that we kept on purpose (and could switch off in a later "improved" mode), a place where GFALL leans on the dagger layer, or a shortcut taken
for now. Each line names where to look.

## Original behaviour kept on purpose (decomp quirks)

- **Potion maker keeps stale keys.** Using the ingredients up empties the cauldron's cells but not its keys, so a second Mix at once makes the same potion for nothing; a recipe loaded after a larger one
  leaves the larger one's keys in cells past its own. Cleared only by reopening the maker. `play/potion.c` (`use_up_cauldron`, `recipe_load`), held by `tests/check_ogpotion.c`.
- **Blessing's legal gift comes off the wrong region.** It is added to the region the player is in and taken back from region 0 (the record never names a region). `blessing_buy`, `tests/check_ogguild.c`.
- **A blessing's amount is the whole even when the stat was held at 100**, so the end takes off points that were never added. Same functions.
- **A second blessing is refused with record 454** ("not enough gold"), the original's own message for it.
- **Armorers and general stores test the faction number's bit 8** where the other shops test the person's flag, so the repair panel can fail to open. `talk_used` in `apps/gfall/seam.c`.
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
- **Teleport shows the ordinary journey popup**, not TELE00I0's yes/no (`g_teleport` in `seam.c`).
- **The spell shop's list is seeded with the person's own number**, the original with the high word of their record id; people's ids are ours (`tag * 31 + faction`).
- **Magic-shop stock is not flagged "not owned" (32)**, so the price scale and "unidentified" bits of those items are not there.
- **Doors keep hours now** (`entry.c`), but the building's own flag 16 and access level are not read from the blocks, a house's "by day" is the clock's 6..18 (`is_daytime` is the sky's), and a door picked is remembered by Lockpicking value in a table that is not saved.
- **A shut shop's shelf opens as a plain container** with no crime for taking from it (the original may note theft); house furniture's "private property" prompt (record 37) and its alarm roll are not built (`shelf_used`).
- **Direction words** use GFALL's compass, not `xn_math_angle_to_point`'s bands; the building distance is `longer + half the shorter`, not `xn_math_approx_dist2d` (`talk_say`, `talk_distance_to`).
- **The list popup's place (x 100..220, from y 26) and the yes/no box's size are guessed** from the other screens, not read from the original's `list_popup_*`.
- **At most four messages queue** on the message page (`g_queue`); more are dropped.
- **The talk list's horizontal scroll** is kept in the state but not painted; "Copy to logbook" does nothing.
- **Spellbook holds 128**; a purchase past it fails silently.
- **Citizens never fight back** (as in the original, a swing kills them outright); the "people fighting back" were the guards. A citizen killed is gone with no corpse; witnesses count the victim, as the original's order has it.
- **Guild invitations are counted but never delivered** (letters are quests); a Brotherhood kill is counted for a watchman in a dungeon only, not for a pedestrian (`guild_count_crime(6, 5)`), (a picked street door now counts for the Thieves; a dungeon door's pick does not, as in the original).
- **The Daedra's weather is not passed** (0), so a prince's liking for a weather counts only when he likes any; his quest and his ally are not built.
- **No weather, `climate_update` or rest after a teleport.**

## Where GFALL still leans on the dagger layer

- `apps/gfall/seam.c` is the only GFALL file that includes it, but the seam now does a lot: `people_in_view_at` (who is in the view), `town_buildings` and `TownBuilding` (the town's buildings; its `name_seed` came from a new
  `rmb_building_seed`), `town_block_bytes` (a block's raw bytes, copied), `travel_town_here`, `travel_building`, `travel_region`, `faction_people_of` and `Faction` (`factionfile.h`), `faces_of_flat`/`faces_read`/`faces_female`,
  `words_use`/`words_say`/`words_expand` (the macro expander, `play/words.c`) and `Words`, `img_load`, `smallfont`, `g_spell_icons` and the map hooks (`travelmap.c`, `townmap.c`, `dunmap.c`).
- **Macros** (`%key`, `%loc`, `%di`, `%hnt`, `%a`, `%cn`, `%dwr` by hand, `%it` by hand) are the dagger layer's, which "is differently implemented"; several records (the rumour and quest macros) are refused or left raw. A decoupled expander should read TEXT.RSC's macro table from FALL.EXE.
- **People** are the dagger layer's entities: their numbers are made up (`tag * 31 + faction`, the dagger `who`), not the original's record ids, so names, the spell shop's list and a person's talk cache differ from the original's for the same person.
- **A person's shopkeeper flag** is found by matching picture and faction among the building's people (`interior_shopkeeper`), because the usable does not carry it; two people of one faction and picture would be confused.
- **Building ids** are `(block + 1) << 8 | record`, not the original's `location << 16 | n` (`talk_building_id`); `names`'s sign seed is the RMB's first word.
- **The region-to-name-bank table** read at 0x1841E3 has only banks 0 and 1; the original's regions should reach the Nords' bank. Unchecked against the original's output (`g_region_bank`).
- **Names are not checked against the original's output**, only their shapes and seeding (`tests/check_ognames.c`); the same for building signs.
- **Houses for sale and their price** (model radius * 5) need the dagger layer's models; so the bank's house, a knight's house and the town's `bank_add_house_for_sale` wait.
- **The town's watch and people are the dagger layer's pool** (`people.c`): a guard is a person of archive 399 taken into GFALL's creatures (`street_enlist`), a citizen turned into a guard is a guard made near and the citizen removed (the pictures are not the citizen's), blood splashes are not drawn, and a killed guard's corpse is the dagger's picture. The pool recycles slots: guards stay held until the place closes.
- **Quest persons** are not told apart from pedestrians; no person is "a potential questor".

## Tests that approximate

- Several flows (a hall's cure and blessing, the innkeeper's room and food, the potion shops through a hall, the magic shop) need a building to stand in and are not driven; only their rules and the windows are.
- `tests/check_ogworld.c` finds a person by scanning entities and calls `activate` at a made-up camera; it proves the wiring, not the aim.
- The legacy-save fixture in `tests/check_ogstate.c` adds the wire size of each new block by hand (`stand`); a new block means a new term there.

## Fixed 2026-11-10
- A crouched player's blows and aim started 35 units under the eye, inside the floor: `chest()` in `play/dungeon.c` now takes the middle of the body (test in `check_ogcombat.c`).
- Looking down turned sprites upside down: the lean's sign in the engine's `flat_facing` (`scene.c`) was wrong; the camera-facing up is `(fx*sin, cos, fz*sin)` for the engine's pitch (tests in `check_engine.c`). An engine file, edited because no engine session was listed.

- A killed town guard leaves a ground pile (`ground_pile` with the watch's loot table) the use key opens; emptied static treasure piles and corpses of the Hold and dungeons are hidden and unusable once their container has no rows (`container_emptied` in `seam.c`).
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
- The character sheet's points-to-place number sat about six lines too low in the attribute arrows' notch; it now takes the creation screen's offset from the arrows (x + 7, bottom 14 down; `seam.c` sheet). Seen only in the user's screenshot: check it on the target.
- A street guard has no key, so no container and no gear (`foe_equipment` needs one) and the watch has no loot table: its pile was empty and never shown. At its death the pile on the ground is made and the gear made into it (`dungeon_gear_to`); test in `check_ogworld.c`. An enlisted guard is marked hostile so the street's talk refuses it, and the offer to surrender is now made under god mode too (it is the law's, not the body's).
- Messages (`status_line`) were drawn at `ui_string(8,24)`, whose y counts up from the bottom: they landed inside the HUD bar. They are centred along the top of the view.
- Pressing E on a locked shop door asked `street_witnesses` for every person within 4096 units, a wall cast each; it now stops once both bits are set, skips a person whose bit is set, and looks 2048 out.
- A shop shut for the hour answers its door with FALL.EXE's "Store is closed. Open from %d:00 to %d:00." (the decomp shows it on click_show_building_info, for types below 14 but the tavern; GFALL shows it on the door, in modes other than Steal). The building's name on the click of its wall is still not shown.
- The shopkeeper's offer (Accept/Reject) was a flat box with words; it is now SPOP.RCI's parchment with BUTTONS.RCI's Yes and No pictures (msgbox_show_string's look). The court's tall questions keep the box.
- Buying (the shop's inventory) turns gold coins behind each thing, shelf and basket alike (TEXTURE.434 record 6, 0.08 s a frame, by the cycle counter), so a shelf reads as for sale and not as loot. The screen is repainted when the frame changes. Not measured on the 233: each repaint recomposes the 320 x 200 screen.
- The temple's donation prompt used 1000 as a TEXT.RSC record (it is `msgbox_prompt_number`'s default amount; the words are FALL.EXE's "Donate how much money"), so it showed the armour info template. It now types 1000 in first (`possessions_number_from`). The typed-number prompts (gold, name, spell name, answer, page, donation) sit on the parchment with the Yes and No pictures like the offer.
- The Status key showed only the active magic and ailments. `status_show` (pickbook_0004259C.c) is two boxes: TEXT.RSC 22 with its macros said (the town, the time `%d:%02d`, the date, the region and the law's opinion of the player from FALL.EXE's thirteen reputation names, `legal_title`), then the health status (record 18 when well). GFALL shows the first box, then the ailments, then ours: the active spells. The dagger reader does not know %tim, %dat, %crn or %ltn, so `status_intro` (`seam.c`) says them itself.
- The street's guards were never taken into the watch: `street_convert` and `street_witnesses` skipped everyone `people_is_citizen` refuses, and that is false for a guard, so the guards already walking the street neither came for a wanted player nor counted as the witness that brings the watch (`people_check_witnesses` bit 1). Only the ones made near were. Both loops now take a citizen or a guard. In the original nothing makes more guards once those are dead (guards_summon is called by a crime, a deferred witness timer, and, each minute passed, a 5% roll while a region's legal reputation is under -10 or a punishment flag is set: crime 7, `tamriel.c` 245..254). That minute roll is not built.
- House containers (chests, cupboards, 418 props, `OTHER_CONTAINER`) were ignored by GFALL. They are stocked on first use by `shelf_stock` from the model's variant (carried in the usable's `hd` by `doors.c`), empty ones say nothing is there, the player's own house opens freely, anyone else's asks (TEXT.RSC 37) and a grab can be seen (pickpocket roll against 108, then theft and the watch), per click.c 960..985. Whether the red wardrobe of the user's report is one of the variants is not checked; the original's list is variants 3, 4, 7, 8, 32..38, 50, 51.
- The sheet's Affiliations button always said "You have no guild affiliations." It now lists each membership (`sheet_show_affiliations`: faction name and the rank title from FALL.EXE's `faction_rank_names`, 22 kinds of ten) and says TEXT.RSC 19 only when there are none. The original's tabbed columns (`\xfb\x80` in its format strings) are a colon here.

## Still open after the 2026-11-10 bug pass
- Crash entering a second building while pursued by the watch: not reproduced (host test with two enter/leave cycles, AddressSanitizer). Wanted: a save or the log from just before it.
- The per-minute guard roll (legal reputation under -10 or a punishment flag: 5% a minute, crime 7, `tamriel.c` 245..254) is not built, so nothing makes more guards once the watch is dead short of a new crime.
- House containers: whether every piece of furniture the user finds unlootable is outside the original's variants (3, 4, 7, 8, 32..38, 50, 51) is unchecked; a container's stock uses `shelf_stock`.
- The Affiliations list uses a colon where the original has tab columns; the town's building name is not shown when a wall is clicked (`click_show_building_info`).
- Coins behind the shop's things repaint the 320 x 200 screen every 0.08 s: unmeasured on the 233.
- Shop coins, a cheaper repaint (do after measuring on the 233): the coins turn behind the items inside the one composed 320 x 200 buffer, so each 0.08 s frame recomposes the whole screen and re-publishes the surface. Instead recompose only the item cells (coin, then icon, then count) into the kept buffer and hand just those rectangles to `pic_update`, which already re-sends only the changed tiles.

## Magic numbers and folders (user's request, 2026-11-11)

- The quest system and much of GFALL carry bare numbers: opcodes in `case 52:`, flags (`0x2000` talk passes, `0xA00` hidden, `128`/`0x8000` topic hidden, `512` item used), QRC message ids 1000..1009, rumour expiries (30 and 180 days), item group/index pairs, state and section indexes, `QUEST_BLOB`. They mirror the decomp, which names almost nothing.
- Plan, as its own mechanical commit with `make all` green before and after: names taken from the decomp documents (`QOP_REPUTE_EXCEEDS`, `QPERSON_PASS_TALK`, `QRC_OFFER`), one header per domain beside its code (`quest_ops.h`, `quest_flags.h`, `item_groups.h`), constants for generated tables emitted by the same `tools/gfall-*.py` scripts, no behaviour change.
- Folders: `src/game/gfall/play/` is crowded; split into `play/quest/`, `play/people/`, `play/magic/`, `play/items/` and so on, updating `tools/check-layers`, the Makefile and AGENTS.md's layout.
- Do it after the quest machine is finished, not between its steps.

## Dropping the  /  prefixes (user's request, 2026-11-11)

- GFALL will replace GFALL, so ``, `` and `Og` prefixes (functions, macros, types, files like `gfall-*.py`, `check_og*.c`, `docs/gfall*.md`) go once GFALL's `src/game/dagger/play/` and `apps/gfall/` are retired: the prefix only exists to keep the two apart.
- Order: retire GFALL first (nothing then collides), then one mechanical rename commit (sed over `src`, `apps`, `tests`, `tools`, `docs`, Makefile), `make all` green before and after. Do it together with or just after the constants and folders pass above, so names are touched once.
- Watch for collisions with the engine and the formats layer (`` removed from `item_create` must not clash with a dagger or engine `item_create`), and for save-format identifiers: the `OGSAVE` magic and schema numbers stay.

## For later (written 2026-11-11, end of the quest-machine and entity-plan session)

- **Pacifying creatures by language and Personality** (the user's idea, confirmed in the decomp: `monster_pacify_check`, `src/hand/func_00079913.c`, called from `place_spawn_from_marker`, `mplace.c`). When a creature is placed from a marker: chance = 10 (-25 with debug bit 6 of `D_001940D6`); a humanoid (race 43 and up) adds `PER / 5 + Etiquette / 10`; any other race adds the player's skill in its language (`monster_language_skill[race]`, one use added to that skill when the id is not 0; the ids are in the decomp's `config/names.csv`: Orcish, Harpy, Giantish, Dragonish, Nymph, Daedric, Spriggan, Centaurian, Impish), plus `spell_active_chance(player, 44, 255)` and `PER / 5`. Success when `rand_range(1, 200) <= chance`: the creature gets flag 0x8000 (pacified); any damaging hit clears it (`damage_apply`). Quest op 68 sets and clears the same flag on a foe and its live copies. GFALL only pacifies by the Calm spell (`dungeon_pacify`, the mind's `passive`). To do: the roll when a creature is made (dungeon creatures, rest encounters, quest foes), cleared by a hit, op 68, and `ENT_HOSTILE` / `ENT_MOVES` set on file-made creatures so the entity shows it (the engine acts on neither flag yet).
- **Entity plan step 10**: street people (`people.c`) onto the registry, hooks in `wild.c` (a town's chunks), remove `people_in_view` (`docs/entity-plan.md`, section 5).
- **Stream GFALL's dungeons** (it loads them whole: `travel_set_dungeon_streaming(0)`): measure on the target first (the castle: 16 blocks, 1235 models, 29.8k triangles, 128 creatures, 356 pictures; the largest, Scourg Barrow: 22 blocks, 34k triangles, 424 pictures). If the load or the RAM is a problem: minds attached and dropped as creature chunks fill and go, doors/levers/treasure built per chunk, keep rows filed under chunks (the registry supports it), quest markers already numbered alike.
- **Quest machine, still open**: the MAPS site list read for exact sites (`dungeon_sites` reads it; the numbering replay is documented, quest-places-and-start.md 8), the Where-is "People" list, noble quests (need the regions' event flags), the vampirism and lycanthropy cures, `%pqn` naming the exact person, a playtest of the whole quest machine on the target.
