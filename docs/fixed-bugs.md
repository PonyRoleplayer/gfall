# Fixed bugs

Bugs found while building the game and fixed rather than kept, whether they were FALL.EXE's or ours. Each line says what was wrong, what we do now, and where it is tested. Behaviour the original has on purpose (quirks we keep) is in `game-later.md`, "Original behaviour kept on purpose". Add to this list whenever a bug is fixed.

## FALL.EXE's, not kept
| what the original does | what we do | tested in |
| --- | --- | --- |
| Quest op 29 (ask yes or no) has its question's QRC id in operand 3, but the old reading took it from the message field, so no real quest's question was asked | `QOP_ASK_MESSAGE_ARG` | `check_quest.c` cure quests |
| Disease and poison are refused when the resist roll gives 100, which means "not resisted": an immune target is always infected and a fragile one never | taken on `RESIST_FULL` | `check_magic.c`, items doc 4.4 |
| Mehrunes' Razor kills when its resist roll is not 100: the immune always die | kills on `RESIST_FULL` | enchant.c |
| Wabbajack wears by the change in most health: a loss adds condition | the size of the change | `check_magicitem.c` |
| `faction_shared_relations` returns a stale scratch word, so every alliance and enmity roll used a wrong number | the count of what two factions share | `check_standing.c` |
| `faction_tree_relation`'s 3 can never be returned (it searches a faction's own subtree for itself) | 3 is "under it" | politics.c |
| A battle can draw `rand(1, 0)` and take a power under 0 | at least 1, held to the floor 1 | politics.c |
| `region_flag_set(18)` overwrites the temple that out-powered the noble with a random god | the temple stays | politics.c |
| Region 17's "temple" is a Daedra prince | no temple there | politicsdata.h |
| The neighbours' zero padding counts as region 0, so short rows border it | a 0 counts only when region 0 lists the region | `check_standing.c` |
| War, famine and the rest read `regions[]` for factions with no region (past the array) | only nobles have a region's flags | politics.c |
| War "beginning to ongoing" sets the flags directly, with no group, so the Merchants miss those regions | the group is set | politics.c |
| News is told for 266 days: `expires` is checked only at the purge | news ends at its time | politics.c |
| The Seneschal's house keeps the previous owner's price, so its sale offers 85 % of a stale one | its list price | `check_bank.c` |
| A sold house's and ship's contents stay in the store, against texts 298 and 299 | the chests are emptied | bank.c |
| Open in use mode: the level term, clamp and lock-20 limit belong to picking only | bare roll in use mode, the rest in Steal mode (ours had them in both) | doors.c |
| A street door forced and unseen is crime 0, which the court ignores: the player is left at 1 health with the guards about | crime 1 | hooks.c |
| `entry_reset` (a town's bashed and tried flags) had no caller: flags outlived the town | cleared on entering a building of another town | `check_entry.c` |
| Hurting a castle's sleeper wakes only that one (the original's `monster_wake_all` wakes all) | all | dungeon.c |
| The mastered-skill notice (4020) is unreachable code | not built | game-later.md |
| Item flag 2048 read as "enchanted" (ours): it is the artifact flag | 1012 and 1014 for artifacts only | `check_magicitem.c` |
| The Skull of Corruption takes the first creature of the list, any team | the nearest living hostile one | enchant.c |
| An Identify price in gold for the spell; the original charges the cast cost per item | the cast cost in magicka, up front and per item | services.c |
| The beast-form guard check ran in dungeons and inside buildings | only out of doors in a town | curse.c |
| No damage was possible after a court (`D_001962B2`): never cleared by anything but a melee swing | kept as the original (it is the rule, not a bug) | `check_law.c` |

## Ours
| what was wrong | the fix | tested in |
| --- | --- | --- |
| A box left open by an old game blocked the next quest's yes-or-no | `quest_reset` clears it | `check_quest.c` |
| Brisienna's letter (op 76) came in buildings, in sleep, with creatures about, and at 06:00 sharp | outdoors in a town by day, none alive, the world running | `check_quest.c` |
| A quest's Daedra prince could be offered with no animation; the summoning's texts named nobody (`%dae`, `%dat` said blank) | the scene and the words | `check_game.c`, `check_quest.c` |
| Rumour expansion left the words' macro pointer on a dead stack frame | `words_use(0)` after saying | politics.c |
| The "alerted rat" test passed only when a stealth roll woke the rat | the rat is alerted directly | `check_combat.c` |
| A foe's knockback never hurt on a wall and never ended at speed 5 | half the speed as damage, ended at 5 | combat.h |
| A creature swung with no Speed roll | the original's roll, once per frame of the original (assumed 20 a second) | dungeon.c |
| The watch had the player's level | the player's plus 3 to 6 | enemies.h |
| A wielded weapon's poison could not be put on by the player | a drug may be put on the weapon in hand (a later addition: FALL.EXE has no way) | `check_combat.c` |
| A building's key (its chests' place key) was its record number alone, so every building of a town in another block shared the chests of the building with the same record, and a house's chests could be another house's | one key from the town's place, block and record (`town_building_key`), the same number for the bank's house, the entry's building and the things' place | `check_world.c`, `check_bank.c` |
| The bank's house was a different number from the building's place key, so the player's own house was never recognised: its chests asked leave, its bed was not free, and a sold house's chests were never emptied | `bank_owns_place`, one key | `check_bank.c` |
| The ship's cabin was never the player's: its chests asked leave and a sold ship kept its contents | the cabin's keys come from the map (`bank_set_ship_places`), a sale empties them | `check_bank.c` |
| Two tables gave a creature's corpse picture and the CFG-derived one was used, wrong for kinds 29 and 39 | `enemy_corpse` takes FALL.EXE's `monster_corpse_textures` for classic kinds | `check_dagger.c` `check_corpses` |
| The surrender flag stayed set while any creature was left in the place | it clears when no creature is alive (`law_calm`) | `check_law.c` |
| The ship's arrival stood in the street, not on the deck | lands on editor marker 8 of the ship's block | `check_dagger.c` `check_ship_marker` |
| A spell missile fired a link meant for a click (trigger 6), and sound 108 was played as itself | the load rewrite of the original: trigger 6 becomes 2, sound 108 becomes 100 (`rdb_resolve_act`) | `check_dagger.c` |
| The archer's long bow was always iron | the ordinary material roll the original makes | the new-game suites run it; the roll itself is not asserted |
| The Identify spell charged its cast at the open, so casting and closing cost magicka, and said "The spell failed to reveal its powers." which the original never says | the cast nets zero; a press charges the fee and the item; the original's two literals (`TEXT_IDENTIFY_SHORT`, `TEXT_IDENTIFY_COUNT`) | `check_magic.c` (the effect's refund); the list is the app's |
| A drug used at player level 1 was deleted while its poison was refused before it was made | the drug stays in the pack ("Nothing happens; you keep it.") | `check_magic.c` |
| Reading a file past 2 GB failed on Windows (`SetFilePointer` given no high word takes the offset as signed), and the host would not open a file over 2 GB | the high words asked for and given: files to 4 GB - 1, FAT32's largest (`docs/files.md`) | `check_engine.c` `check_fs_writes`, a 2.5 GB sparse file (host) |
| `fs_finish` closed a file unchecked, so a save that Windows 98 found would not fit only at the flush or the close was taken as written | `fs_finish` flushes and closes, both checked, and answers 0; `fs_last_error` says why (`docs/files.md`) | `check_engine.c` `check_fs_writes` |
