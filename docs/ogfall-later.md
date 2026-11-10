# OGFALL: known bugs and gaps, to fix when the dagger layer is decoupled

Found while building people and law (slices 1 to 9 and the shops, guilds, potions, spells and magic items after them). Nothing here blocks play; each is either a
quirk of the original that we kept on purpose (and could switch off in a later "improved" mode), a place where OGFALL leans on the dagger layer, or a shortcut taken
for now. Each line names where to look.

## Original behaviour kept on purpose (decomp quirks)

- **Potion maker keeps stale keys.** Using the ingredients up empties the cauldron's cells but not its keys, so a second Mix at once makes the same potion for nothing; a recipe loaded after a larger one
  leaves the larger one's keys in cells past its own. Cleared only by reopening the maker. `play/potion.c` (`use_up_cauldron`, `og_recipe_load`), held by `tests/check_ogpotion.c`.
- **Blessing's legal gift comes off the wrong region.** It is added to the region the player is in and taken back from region 0 (the record never names a region). `og_blessing_buy`, `tests/check_ogguild.c`.
- **A blessing's amount is the whole even when the stat was held at 100**, so the end takes off points that were never added. Same functions.
- **A second blessing is refused with record 454** ("not enough gold"), the original's own message for it.
- **Armorers and general stores test the faction number's bit 8** where the other shops test the person's flag, so the repair panel can fail to open. `talk_used` in `apps/ogfall/seam.c`.
- **Rank refusal 3100, "reserved for members only"** for the Thieves' and Brotherhood's strangers shows nothing at all (the original's `case 3` with no membership just breaks).
- **A Thieves' pickpocket that found nothing is not counted**; only gold counts (`og_steal_pocket`).
- **Each person's pocket is tried once, win or lose** (`click_pedestrian` marks the person first).
- **Magic item prices forget the base item** (`enchant_item_value`), and a parameter of 255 (to be chosen) costs a spell that does not exist, so nothing.
- **A guild's rank scan would run past its table for a skill over 94**; held at 10 (`og_guild_rank_for_skills`).
- **The Mages' identify seller (801) dereferences a null membership for a stranger** in the original; we say "reserved for members only".
- **The unknown brew's spell keeps whatever the spell maker's record held before** (fields the brew does not set); we use zeros (`brew_record`).
- **Daedra: the fourth day entry is a sentinel**, not a day (`og_daedra_offer`).

## Deviations we took (to be made faithful)

- **A promotion's message replaces the guild menu for that click**; the original shows the message and then the menu (`hall_enter`).
- **Guild healing skips record 350 when attributes are down** (it asks 403 instead); the Mages' magicka restore for a caster is not done, nor the disease/effect cleanup of the player's spell effects (`hall_heal`).
- **Room offer's offer text** uses days * 7 as the total, not the original's stale `trade_total` (`inn_days`); Heart's Day's free night is in the price but not the total.
- **The innkeeper's bed marker is not read** (room 0), so a rented room does not send the sleeper to its bed (`inn_room_paid`).
- **The potion maker's name box always says "Unnamed potion"** (the original shows the recipe being loaded).
- **Teleport shows the ordinary journey popup**, not TELE00I0's yes/no (`g_teleport` in `seam.c`).
- **The spell shop's list is seeded with the person's own number**, the original with the high word of their record id; people's ids are ours (`tag * 31 + faction`).
- **Magic-shop stock is not flagged "not owned" (32)**, so the price scale and "unidentified" bits of those items are not there.
- **Opening hours do not gate doors** (`og_building_open` is used by shelves only); houses open by day need a daylight flag we do not pass.
- **A shut shop's shelf opens as a plain container** with no crime for taking from it (the original may note theft); house furniture's "private property" prompt (record 37) and its alarm roll are not built (`shelf_used`).
- **Direction words** use GFALL's compass, not `xn_math_angle_to_point`'s bands; the building distance is `longer + half the shorter`, not `xn_math_approx_dist2d` (`talk_say`, `talk_distance_to`).
- **The list popup's place (x 100..220, from y 26) and the yes/no box's size are guessed** from the other screens, not read from the original's `list_popup_*`.
- **At most four messages queue** on the message page (`g_queue`); more are dropped.
- **The talk list's horizontal scroll** is kept in the state but not painted; "Copy to logbook" does nothing.
- **Spellbook holds 128**; a purchase past it fails silently.
- **Guild invitations are counted but never delivered** (letters are quests); a Brotherhood kill is counted for a watchman in a dungeon only, not for a pedestrian (`guild_count_crime(6, 5)`), and a lockpicked house door (`archive.c` 269) does not count for the Thieves.
- **The Daedra's weather is not passed** (0), so a prince's liking for a weather counts only when he likes any; his quest and his ally are not built.
- **No weather, `climate_update` or rest after a teleport.**

## Where OGFALL still leans on the dagger layer

- `apps/ogfall/seam.c` is the only OGFALL file that includes it, but the seam now does a lot: `people_in_view_at` (who is in the view), `town_buildings` and `TownBuilding` (the town's buildings; its `name_seed` came from a new
  `rmb_building_seed`), `town_block_bytes` (a block's raw bytes, copied), `travel_town_here`, `travel_building`, `travel_region`, `faction_people_of` and `Faction` (`factionfile.h`), `faces_of_flat`/`faces_read`/`faces_female`,
  `words_use`/`words_say`/`words_expand` (the macro expander, `play/words.c`) and `Words`, `img_load`, `smallfont`, `g_spell_icons` and the map hooks (`travelmap.c`, `townmap.c`, `dunmap.c`).
- **Macros** (`%key`, `%loc`, `%di`, `%hnt`, `%a`, `%cn`, `%dwr` by hand, `%it` by hand) are the dagger layer's, which "is differently implemented"; several records (the rumour and quest macros) are refused or left raw. A decoupled expander should read TEXT.RSC's macro table from FALL.EXE.
- **People** are the dagger layer's entities: their numbers are made up (`tag * 31 + faction`, the dagger `who`), not the original's record ids, so names, the spell shop's list and a person's talk cache differ from the original's for the same person.
- **A person's shopkeeper flag** is found by matching picture and faction among the building's people (`interior_shopkeeper`), because the usable does not carry it; two people of one faction and picture would be confused.
- **Building ids** are `(block + 1) << 8 | record`, not the original's `location << 16 | n` (`talk_building_id`); `og_names`'s sign seed is the RMB's first word.
- **The region-to-name-bank table** read at 0x1841E3 has only banks 0 and 1; the original's regions should reach the Nords' bank. Unchecked against the original's output (`g_region_bank`).
- **Names are not checked against the original's output**, only their shapes and seeding (`tests/check_ognames.c`); the same for building signs.
- **Houses for sale and their price** (model radius * 5) need the dagger layer's models; so the bank's house, a knight's house and the town's `bank_add_house_for_sale` wait.
- **Town creatures** (guards made in towns, pedestrians fighting back) need the street people's art and AI, which are the dagger layer's (`people.c`); the law's guards are made only in dungeons.
- **Quest persons** are not told apart from pedestrians; no person is "a potential questor".

## Tests that approximate

- Several flows (a hall's cure and blessing, the innkeeper's room and food, the potion shops through a hall, the magic shop) need a building to stand in and are not driven; only their rules and the windows are.
- `tests/check_ogworld.c` finds a person by scanning entities and calls `og_activate` at a made-up camera; it proves the wiring, not the aim.
- The legacy-save fixture in `tests/check_ogstate.c` adds the wire size of each new block by hand (`stand`); a new block means a new term there.
