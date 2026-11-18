# Unnamed rule numbers in the game layer

## Status (branch `magic-numbers`, 2026-11-17)

The cleanup order at the end of this document, steps 1 to 11, is done; no behaviour changed (`make all` is green after each step, and the host tests are the proof). What the sections below say about those sites is the state before. Where each name lives now:

- Skills: `tables/careers.h` `SKILL_*` at every call site; the private copies (`talk.c`, `steal.h`, `entry.c`, `market.c`, `tables.h`, `ai.c`) are gone, and `ATTR_PER` replaces the private `ATTR_PERSONALITY`.
- Mobiles: `tables/enemies.h` `ENEMY_*` (spider, orc, centaur, nymph, orc sergeant, giant scorpion, orc shaman, orc warlord added), the bands (`ENEMY_CLASSIC_KINDS`, `ENEMY_RACE_CLASS`, `ENEMY_IS_CLASSIC`/`_CLASS`/`_CLASS_ONLY`/`_KNOWN`), the voices (`ENEMY_VOICE_*`), the archives (`ENEMY_ARCHIVE_*` and `foe_archive`), `foe_enchant_category` and `FOE_*` categories in `combatdefs.h`.
- Item groups: `tables/itemdefs.h` `ITEM_GROUP_*` (Daggerfall Unity's order), `MISC_SPELLBOOK`/`MISC_SOUL_TRAP`/`MISC_LETTER_OF_CREDIT`, `WEAPON_SHORT_BOW`/`WEAPON_LONG_BOW`/`WEAPON_ARROW` (`ARROW_GROUP` and `ARROW_INDEX` are now those). The `17` of the save upgrade and of the item maker's exclusion stays a number: it is an old arrow, not the bow.
- Player race: `tables/tables.h` `PLAYER_RACE_*` (never pass one to `career_race`), the curse forms `CURSE_FORM_*` in `curse.h`, `CLASS_*` for `ClassInfo.flags`, `EFFECT_BIT_*` for the class's resistance bits.
- Equipment: `state.h` `EQUIP_*` (the decomp's 27) and `EQUIP_SLOTS`.
- Combat: `combat.h` names every number of the to-hit, damage, wear and hurt sums.
- Vitals: `vitals.h` `RATIO_*`, `FATIGUE_UNIT`, `fatigue_maximum`, the jump and the fall; `move.h` `WALK_*` (SPD + `WALK_SPEED_ADD` is spelled once).
- Conditions: `tables/magic.h` `COND_*` (and `COND_GROUP(group)`), the effect groups by `GROUP_*` in `magic.c`; `magic.h` names the resist roll, the sight chances and the spell cost.
- Trade, guild, class maker: `trade.h`, `guild.h`, `chargen.h`.
- `curse.c`: its own spells, clans, sounds, rolls and caps.

Done 2026-11-18 (every file of `docs/gfall-constants.md` in the decomp): `kit`, `state`, `guild`, `shelf`, `market`, `names`, `standing`, `journey`, `survival`, `foegear`, `weapon`, `casting`, `spellmaker`, `spellshop`, `magicitem`, `magic`, `potion`, `itemmaker`, `describe`, `ground`, `doors`, and the quest ops (`QOP_*` in `quests/internal.h`). The differences from FALL.EXE that this found, fixed and open, are in `docs/game-later.md`.

Still open: step 12 (screen rectangles in `chargen.c`, `travelmap.c`, `controls.c`, `ui.c`, `possessions/*`, `talkwin.c`), the quest machine's other numbers (`machine.c`, `binding.c`, `journal.c`), the save bounds (`state.c`, `save.c`, `config.c`), and the person-faction tables in `guild.c`.

Read at `260e993` (`main`). The working tree also had uncommitted edits, including `play/combat/dungeon.c`, `play/magic/magic.c` and `play/combat/combat.c`. Line numbers below are that working tree. A clean `260e993` will not match every line in the dirty files.

This is a gap list, not a rename. A numeric literal that encodes a rule, a FALL.EXE constant, a record or faction id, a chance, a duration, a price, a stat, a bit, a coefficient, or a screen rectangle, written inline instead of a name. `0`, `1`, `-1`, and a loop bound that is the array's length are not gaps. A literal that is the sole initializer of a `#define` immediately above its use is the pattern to copy, recorded in the last sections, not a gap.

`src/game/dagger/play/` is 139 `.c`/`.h` files. Stripping comments and strings, they hold 7522 integer and float literals other than `0` and `1`. The heavy files are screen and data, not the dense rule files:

| literals | file |
| --- | --- |
| 727 | `possessions.c` |
| 526 | `quest.c` |
| 503 | `chargen.c` |
| 357 | `travelmap.c` |
| 348 | `dungeon.c` |
| 292 | `guild.c` |
| 261 | `magic.c` |
| 235 | `inventory.c` |
| 223 | `controls.c` |
| 209 | `itemmaker.c` |
| 177 | `ui.c` |
| 172 | `play.c` |

Denser rule files, same count: `trade.c` 67, `talk.c` 66, `combat.c` 59, `law.c` 58, `weapon.c` 50, `ailments.c` 111, `move.c` 115, `curse.c` 126. `vitals.c` is under 40: the formulas are short and the coefficients are still bare.

Most of the 7522 are coordinates, text-record ids, and loop structure. The gaps that will be misread are the ones below.

## What is already named

Declared under `src/game/dagger/`, and what `play/` does with the name.

| family | where | values | used by name in `play/` |
| --- | --- | --- | --- |
| skills | `tables/careers.h` `SKILL_*` | 0 Medical, 1 Etiquette, 2 Streetwise, 3 Jumping, 13 Lockpicking, 14 Mercantile, 15 Pickpocket, 16 Stealth, 17 Swimming, 18 Climbing, 19 Backstabbing, 20 Dodging, 21 Running, 22..27 the schools, 28 Short Blade, 30 Hand-to-Hand, 33 Archery, 34 Critical Strike | only through private copies (below). `SKILL_DODGING`, `SKILL_CRITICAL_STRIKE`, `SKILL_BACKSTABBING`, `SKILL_HAND_TO_HAND`, `SKILL_ARCHERY`, `SKILL_CLIMBING`, `SKILL_RUNNING`, `SKILL_SWIMMING`, `SKILL_MEDICAL`, and `SKILL_DESTRUCTION`..`SKILL_MYSTICISM` do not appear in `play/` |
| skills not given a `SKILL_*` | `tables/classic.c` `g_skills[]` names them; the decomp enum in `include/records.h` names them | 4 Orcish .. 12 Impish, 29 Long Blade, 31 Axe, 32 Blunt Weapon | not written as indexes in `play/`. `weapon_skill` and `foe_language` return them |
| attributes | `tables/tables.h` `ATTR_STR`..`ATTR_LUC`; `tables/careers.h` `ATTR_STRENGTH`..`ATTR_LUCK` | 0..7, same order | `play/` uses `ATTR_STR` and the short names. `talk.c` and `market.c` also `#define ATTR_PERSONALITY 5` |
| player race | `tables/tables.h` comment, `tables/tables.c` `race_name` | 0 Breton .. 7 Argonian, then 8 vampire, 9 werewolf, 10 wereboar (`curse.h`) | no enum. Stored by `chargen.c` as `pixel - 1` |
| record race | `tables/careers.h` `RACE_BRETON` = 1 .. `RACE_ARGONIAN` = 8 | DFU ids. Breton's record id is 1 | `names.c` only. Not the player-race tests |
| mobiles | `tables/enemies.h` `ENEMY_*` | rat 0, giant bat 3, werewolf 9, slaughterfish 11, wereboar 14, skeletal warrior 15, zombie 17, ghost 18, mummy 19, wraith 23, vampire 28, vampire ancient 30, dreugh 41, lamia 42, first class 128, city watch 146 | none. `places/people.c`, `places/world.c`, `places/dunstream.c` do use them |
| mobiles with a row and no `#define` | `tables/classic.c` `g_enemies[]` | spider 6, orc 7, centaur 8, nymph 10, orc sergeant 12, giant scorpion 20, orc shaman 21, orc warlord 24, and the rest of 0..42 and 128..146 | the gear and on-hit sites below write 6, 7, 8, 10, 12, 20, 21, 24 raw |
| item templates | `tables/items.h` `ITEM_*`, `MATERIAL_*`, `BODY_*` | cuirass 102, arrow template 131, gold 276, iron 0 .. daedric 9, leather 10, chain 11, body parts 0..6 | none. `play/` items are group/index through `item_def`, a different numbering |
| arrow identity | `play/people/trade.h` `ARROW_GROUP` 3, `ARROW_INDEX` 18 | weapon group, arrow index | `trade.c`, `market.c`. The other weapon sites repeat `3` and `18` |
| elements | `tables/magic.h` `ELEMENT_FIRE` 0, `ELEMENT_COLD` 1, `ELEMENT_POISON` 2, `ELEMENT_SHOCK` 3, `ELEMENT_MAGIC` 4 | SPELLS.STD order | `magic_resist` writes `1` and `4` |
| equip slots | not declared. The decomp's `include/records.h` has `EQUIP_AMULET0` .. `EQUIP_BOOTS`, 27 of them. `EQUIP_RIGHT_HAND` is 19, `EQUIP_LEFT_HAND` is 21 | | `curse.c` has file-local `HAND_SLOTS_FIRST` 19 and `HAND_SLOT_SECOND` 21. Every other hand test writes `19` and `21` |

`EQUIP_RIGHT_HAND` and `EQUIP_LEFT_HAND` are not names in this tree. An earlier scan listed them as existing and unused. They exist in the decomp header only.

## A. A name exists and the call site does not use it

89 items: 27 skill sites, 11 private copies of a name that already exists, 18 comparisons to a named mobile, 17 range tests of `128` / `146` / `43`, 12 arrow sites, 2 material sites, 2 element literals. Counts are those rows. Expanding the two curse arrays and the school range `22..27` turns the 27 skill sites into 44 skill ids.

### Skills

`damage_roll_to_hit` (`src/lifted/qkey.c`) reads `skills[SKILL_DODGING]` and `skills[SKILL_CRITICAL_STRIKE]`, then still calls `skill_add_uses(20, 1)` and `skill_add_uses(34, 1)`. The decomp names the read and leaves the use-count raw. GFALL leaves both raw. The dodge term is the attacker's Dodging, which the comment in `attack_chance` already says.

| site | literal | name in `careers.h` | expression |
| --- | --- | --- | --- |
| `ai.c:21` | 16 | `SKILL_STEALTH` | `skill_use(16)` |
| `ai.c:22` | 16 | `SKILL_STEALTH` | `(skill[16] * distance) / 1024 * 2` |
| `combat.c:55` | 20 | `SKILL_DODGING` | `a->skill[20] / 4` |
| `combat.c:56` | 34 | `SKILL_CRITICAL_STRIKE` | `critical < a->skill[34]`, then `a->skill[34] / 10` |
| `doors.c:157` | 13 | `SKILL_LOCKPICKING` | `skill[13]` |
| `doors.c:182` | 13 | `SKILL_LOCKPICKING` | `skill_use(13)` |
| `doors.c:223` | 16 | `SKILL_STEALTH` | `skill_use(16)` |
| `dungeon.c:534` | 19 | `SKILL_BACKSTABBING` | `a.skill[19]` added when the foe faces away |
| `dungeon.c:536` | 34, 19 | `SKILL_CRITICAL_STRIKE`, `SKILL_BACKSTABBING` | `skill_use(34)`; `skill_use(19)` if the backstab term was taken |
| `dungeon.c:550` | 30 | `SKILL_HAND_TO_HAND` | `rand` of `skill[30] / 10 + 1` .. `skill[30] / 5 + 1` |
| `dungeon.c:685` | 33 | `SKILL_ARCHERY` | `archer->fighter.skill[33]` passed as the to-hit skill |
| `dungeon.c:686` | 20 | `SKILL_DODGING` | `skill_use(20)` when the arrow hits the player |
| `dungeon.c:794` | 20 | `SKILL_DODGING` | `skill_use(20)` |
| `dungeon.c:798` | 30 | `SKILL_HAND_TO_HAND` | same unarmed range as `dungeon.c:550` |
| `move.c:229` | 18 | `SKILL_CLIMBING` | `skill_use(18)` once a minute while climbing |
| `move.c:230` | 18 | `SKILL_CLIMBING` | `skill[18] + (curse_race() == 6 ? 30 : 0)` |
| `move.c:245` | 18 | `SKILL_CLIMBING` | `skill_use(18)` |
| `move.c:365` | 21 | `SKILL_RUNNING` | `ground_speed(..., skill[21], ...)` |
| `move.c:366` | 17 | `SKILL_SWIMMING` | `ground_speed(..., skill[17])` |
| `move.c:396` | 3 | `SKILL_JUMPING` | `skill_use(3)`. The impulse on the same path calls `jump_magnitude` with `skill[SKILL_JUMPING]` |
| `rest.c:83` | 0 | `SKILL_MEDICAL` | `rate += skill[0]` |
| `rest.c:94` | 0 | `SKILL_MEDICAL` | `skill_use(0)` |
| `survival.c:48` | 17 | `SKILL_SWIMMING` | water fatigue uses `skill[17]` |
| `survival.c:49` | 17 | `SKILL_SWIMMING` | `skill_use(17)` |
| `curse.c:50` | 3, 21, 16, 34, 18, 30 | Jumping, Running, Stealth, Critical Strike, Climbing, Hand-to-Hand | `vampire_skills[]`. The decomp's notes list the same six numbers (`docs/character-skills-math.md`) |
| `curse.c:51` | 17, 18, 3, 16, 21, 34, 30 | Swimming, Climbing, Jumping, Stealth, Running, Critical Strike, Hand-to-Hand | `lycanthrope_skills[]` |
| `inventory.c:358` | 22..27 | `SKILL_DESTRUCTION` .. `SKILL_MYSTICISM` | a class counts as a caster when one of its twelve skills sits in that range |

`curse.c:52` already writes the lycanthrope attributes as `ATTR_STR, ATTR_END, ATTR_AGI, ATTR_SPD`. The skill arrays next to it are the gap.

Private copies. The call site uses a name, and that name is a second `#define` or a file-local `enum` of a constant `careers.h` or `tables.h` already has. `SKILL_JUMPING` in `tables.h` is the one `move.c` includes; `careers.h` has the same macro.

| copy | value | canonical |
| --- | --- | --- |
| `talk.c:11` `SKILL_ETIQUETTE` | 1 | `careers.h` |
| `talk.c:12` `SKILL_STREETWISE` | 2 | `careers.h` |
| `talk.c:13` `ATTR_PERSONALITY` | 5 | `tables.h` `ATTR_PER`, and `careers.h` `ATTR_PERSONALITY` (an enum, same ordinal) |
| `steal.h:13` `SKILL_PICKPOCKET` | 15 | `careers.h` |
| `steal.h:14` `SKILL_STEALTH` | 16 | `careers.h` |
| `entry.c:11` `SKILL_LOCKPICKING` | 13 | `careers.h` |
| `entry.c:12` `SKILL_STEALTH` | 16 | `careers.h` |
| `market.c:11` `SKILL_MERCANTILE` | 14 | `careers.h` |
| `market.c:12` `ATTR_PERSONALITY` | 5 | `ATTR_PER` |
| `tables.h:28` `SKILL_JUMPING` | 3 | `careers.h` `SKILL_JUMPING` |
| `ai.c:30` `enum { ETIQUETTE = 1 }` | 1 | `careers.h` `SKILL_ETIQUETTE`. Used at `ai.c:37` as `skill[ETIQUETTE] / 10` when `kind >= 128`. `COMPREHEND_LANGUAGES` 44 beside it is an effect group passed to `magic_chance`, not a skill |

`entry.c`, `steal.c`, `market.c`, `talk.c`, and the etiquette term in `ai_marker_pacify` already use a skill name at the call. They are not in the raw-index table.

### Mobiles

`play/` never mentions `ENEMY_*`. `places/` does. A monster's `FighterInfo.race` is the mobile id, or `43` for a class enemy (`dungeon.c:207`: `kind >= 128 ? 43 : kind`). `t->race == 15` is the skeletal warrior, not a player race. Player races stop at 10.

`damage_monster_hit_effects` (`src/lifted/damage.c`) switches on the same raw ids. The decomp has no `ENEMY_*` either. The names that do exist are in `enemies.h`.

| site | literal | name | expression |
| --- | --- | --- | --- |
| `curse.c:101` | 9, 14, 28, 30 | `ENEMY_WEREWOLF`, `ENEMY_WEREBOAR`, `ENEMY_VAMPIRE`, `ENEMY_VAMPIRE_ANCIENT` | `curse_blow` returns 0 for every other kind |
| `curse.c:103` | 9 | `ENEMY_WEREWOLF` | `curse_infect_lycanthropy(0)` |
| `curse.c:104` | 14 | `ENEMY_WEREBOAR` | `curse_infect_lycanthropy(1)` |
| `ailments.c:189` | 0 | `ENEMY_RAT` | `rand(0,100) <= 5`, then a disease |
| `ailments.c:190` | 3 | `ENEMY_GIANT_BAT` | `rand(0,100) <= 2` |
| `ailments.c:191` | 19 | `ENEMY_MUMMY` | `rand(1,100) <= 5` |
| `ailments.c:192` | 28, 30 | `ENEMY_VAMPIRE`, `ENEMY_VAMPIRE_ANCIENT` | a plague only when `curse_blow` did not take |
| `combat.c:69` | 15 | `ENEMY_SKELETAL_WARRIOR` | `t->race == 15`, then silver doubles and a non-blunt weapon halves |
| `dungeon.c:419` | 146 | `ENEMY_CITY_WATCH` | `guild_count_crime(6, 1, ...)` |
| `dungeon.c:842` | 146 | `ENEMY_CITY_WATCH` | bark id `11461` |
| `dungeon.c:1099` | 146 | `ENEMY_CITY_WATCH` | count of living watch |
| `encounter.c:139` | 146 | `ENEMY_CITY_WATCH` | archive 399, else `255 + kind` for `kind < 43` |
| `state.c:705` | 146 | `ENEMY_CITY_WATCH` | same archive test on a kept actor |
| `state.c:722` | 146 | `ENEMY_CITY_WATCH` | same test when a save is checked |

`INFECTION_CHANCE` (`curse.c`, 400 of Watcom's 32768) is already the name for the werewolf/vampire roll. The disease percents 5 and 2 are the decomp's `rand_range` bounds, left inline in both trees.

Range tests of the class band. `128` is `ENEMY_FIRST_CLASS`. `146` is `ENEMY_CITY_WATCH`. `43` is one past lamia (`ENEMY_LAMIA` is 42) and is also the sentinel `dungeon.c` stores as a class enemy's race. There is no name for 43.

| site | expression |
| --- | --- |
| `encounter.c:87` | `kind >= 128 && kind < 146` picks a class texture |
| `dungeon.c:197` | accept `kind < 43` or `kind` in `128..146` |
| `dungeon.c:206` | `kind >= 128` loads `class_at((kind - 128) % 18)` |
| `dungeon.c:207` | `fighter.race = kind >= 128 ? 43 : kind` |
| `dungeon.c:290` | `kind >= 128` or an orc/centaur id gets `foe_equipment` |
| `dungeon.c:466` | `kind >= 128 && kind < 146` copies the class |
| `dungeon.c:553`, `dungeon.c:692`, `dungeon.c:783`, `dungeon.c:817` | `kind >= 128 ? 2 : foe_category(kind)` (2 is the humanoid category) |
| `state.c:687` | the same accept test as `dungeon.c:197` |
| `state.c:708`, `state.c:724` | `kind >= 128 && kind < 146` matches the other archive |
| `magic.c:329`, `magic.c:369` | `dungeon_kind(index) >= 128` |
| `foegear.c:24` | `kind >= 128` forces gear tier 3 |
| `potion.c:121` | `kind >= 0 && kind < 43` indexes soul values |

`ailments.c:168` `id >= 128` and `magic.c:658` are poison record ids, not mobiles. `spellbook.c:96` `count >= 128` is the book limit.

### Arrows, metal, elements

`ARROW_GROUP` and `ARROW_INDEX` are the names. `trade_buy_line` and `market.c` use them. These do not:

| site | expression |
| --- | --- |
| `weapon.c:67`, `weapon.c:79` | `arrow_count` / `arrow_take`: `group == 3 && index == 18` |
| `inventory.c:86` | `group == 3 && index != 18` rolls a metal |
| `inventory.c:98` | `group == 3 && index == 18` sets a random count and condition 0 |
| `inventory.c:129` | `group == 3 && index != 18` prefixes the material name |
| `inventory.c:394`, `inventory.c:400` | taking arrows merges a stack of `group == 3 && index == 18` |
| `inventory.c:516` | `inventory_upgrade` skips metal on `index != 18` |
| `dungeon.c:625` | a foe's container, same pair |
| `state.c:361` | a new character's gift, same pair |
| `magicitem.c:172` | a generated weapon that is the arrow stack is deleted |
| `describe.c:25` | `group == 3` then `index == 18` picks text 1011, else 1001 |

`combat.c:70` `material == 2` doubles damage on race 15. That is `MATERIAL_SILVER` (`items.h`: iron 0, steel 1, silver 2). The decomp's `damage_resolve_attack` leaves `material == 2` inline and calls it silver in `docs/combat-math.md`. `magicitem.c:180` `item->material == 0` on group 2 or 3 is `MATERIAL_IRON`, the base the comment says `item_roll_material` already rolled.

`magic.c:187`: `(element == 1 && race == 2) || (element == 4 && race == 0)`, then `chance += 30`. Element 1 is `ELEMENT_COLD`. Element 4 is `ELEMENT_MAGIC`. `spfx_resist_roll` (`src/lifted/spfx.c`) writes the same test, and `docs/combat-math.md` labels them Nord and Breton. The element names exist and are not used. The race numbers are the player race, not `RACE_*` (next section).

### Do not retarget player race onto `RACE_*`

`RACE_BRETON` is 1. Player Breton is 0. `chargen.c` stores `pixel - 1`. `tables.h` says "0 is Breton". `curse_race()` returns that number, or 8, 9, 10 while cursed. Substituting `RACE_BRETON` for a `race == 0` test shifts every race by one.

| player race | who | `RACE_*` (record id) | site |
| --- | --- | --- | --- |
| 0 | Breton | `RACE_BRETON` is 1 | `magic.c:187` `element == 4 && race == 0` (magic, +30) |
| 1 | Redguard | `RACE_REDGUARD` is 2 | `combat.c:38` `race == 1 && weapon < 16` gives `level / 3` |
| 2 | Nord | `RACE_NORD` is 3 | `magic.c:187` `element == 1 && race == 2` (cold, +30) |
| 3 | Dark Elf | `RACE_DARK_ELF` is 4 | `combat.c:39` `race == 3` gives `level / 4` |
| 5 | Wood Elf | `RACE_WOOD_ELF` is 6 | `combat.c:38` `race == 5 && weapon >= 16` gives `level / 3` |
| 6 | Khajiit | `RACE_KHAJIIT` is 7 | `move.c:230` `curse_race() == 6` adds 30 to the climb chance |
| 7 | Argonian | `RACE_ARGONIAN` is 8 | `survival.c:35` and `survival.c:47` `curse_race() != 7` spends breath and the water fatigue |

`qkey.c` leaves `race == 1`, `race == 5`, `race == 3` and `weapon_index < 16` inline. Weapon indexes 16 and 17 are the bows (`weapon.c:109`; `docs/combat-math.md`). Hand-to-hand passes a negative index, so the race-1 term applies and the race-5 term does not.

`words.c:80` calls `career_race(w->pc->race)`. `career_race` looks up the record id. Breton's record id is 1, the player's race is 0, and `race_name` in `words.c` then returns "stranger". That is the two numberings meeting. A 0-based enum must not be passed to `career_race`.

High Elf is player race 4 (`magic.c` paralysis test `curse_race() == 4` on the paralyze path). `RACE_HIGH_ELF` is 5. The same off-by-one.

## B. Classic constants with no name in this tree

Grouped by the rule. Where the decomp also leaves the number inline, that is said.

### Combat to-hit and damage

`attack_chance` (`combat.c`) follows `damage_roll_to_hit` (`src/lifted/qkey.c`). Besides the skill indexes in section A:

| literal | meaning | decomp |
| --- | --- | --- |
| health `< maximum/8` and `flags & 4`, then ±5 | low-health career flag | `qkey.c`, `max_health >> 3`, `career.flags & 4`. Inline there too |
| `(AGI difference) / 10`, `(LUC difference) / 10` | attribute term | inline in `qkey.c` |
| `race < 43` then `+ 40` | a monster (race held as the mobile, class enemies stored as 43) is easier to hit | `qkey.c` `target->race < 43`. 43 has no name |
| `race == 1` / `5` / `3`, `weapon < 16`, `level/3`, `level/4` | Redguard short, Wood Elf long, Dark Elf any | inline in `qkey.c` |
| `- 50`, clamp `3..97` | the roll is then `0..100` | inline in `qkey.c` |
| `material == 2` doubles; `item_def(3, weapon)->flags & 16` is the blunt bit that does not halve | skeletal warrior | inline in `qkey.c`. The flag bit 16 has no name in `records.h` either |
| `(STR - 50) / 5` | damage | inline in `qkey.c` |
| armor starts at 100, `inventory_armor(i) * 5`, clamp `-100..100` | `armor_value_for_part` | `src/lifted/quests.c`, documented in `docs/combat-math.md`. The 100 and the 5 are inline there |

`foegear.c` repeats the armor formula for a generated foe (`armor = 100`, shield `(index - 6) * 5`, clamp against 50 and -100) and writes the part count as `7`. `BODY_PARTS` is 7. A `for` over `7` is the array length. The shield index base `6` is not a name. `gear_slot[]` is `{12, 13, 15, 18, 23, 26}`: the decomp's `EQUIP_HEAD`, `EQUIP_RIGHT_PAULDRON`, `EQUIP_LEFT_PAULDRON`, `EQUIP_CUIRASS`, `EQUIP_GREAVES`, `EQUIP_BOOTS`. The `make` calls pass slot `19` and `21` for the hands. None of those `EQUIP_*` names exist here.

Hands elsewhere, all raw `19` and `21`: `inventory.c` (the equipped right hand, a two-handed clear of the other hand, the shield in the left), `weapon.c:42` and `weapon.c:106`, `play.c` (`weapon_hand() ? 21 : 19`), `dungeon.c` (a foe's right hand). `curse.c` is the only file that named them, as `HAND_SLOTS_FIRST` and `HAND_SLOT_SECOND`.

`combat.c:112` clamps the hurt amount to `4..32` and `combat.c:122` decays `g_flash` by `dt * 2`. The clamp is `damage_player_hurt` (`src/hand/func_0002F3FA.c`): `n` is forced into `4..32`, then `screen_shake_start(n << 3)`. The `/32` and the `dt * 2` fade are not in that function. See section C.

### On-hit effects and the orcs who wear gear

`ailments_strike` matches `damage.c` except the curse kinds, which `curse_blow` takes. The kinds with no `#define`, and the gear kinds next to them:

| literal | row in `classic.c` `g_enemies[]` | site |
| --- | --- | --- |
| 6 | Spider | `ailments.c:195`, with 20, casts spell 66 unless that effect is already on |
| 10 | Nymph | `ailments.c:194` `fatigue_spend(rand(10,30) * 64)`. `damage.c` case 10 subtracts `rand_range(10,30) << 6` |
| 20 | Giant Scorpion | `ailments.c:195`, same spell 66 as the spider. `damage.c` case 6 and case 20 |
| 7, 8, 12, 21, 24 | Orc, Centaur, Orc Sergeant, Orc Shaman, Orc Warlord | `dungeon.c:290` and `foegear.c:25`..`27`. Tier 0, 0, 1, 1, 2 |

Spell 66 is inline in both trees. Unknown which SPELLS.STD row that is from this pass; the call is `magic_apply(66, ...)`.

### Walk, climb, swim, fatigue

`player` speed in `src/hand/func_00010585.c`:

```
crouched:  SPD + 50
standing:  D_00187CA9 + (SPD - 50)
running:   speed * (((Running << 8) / 200) + 320) / 256
water:     speed >>= 2; speed + speed * ((Swimming << 8) / 200) / 256
```

`move.c` names `WALK_ADD` 200 and computes `WALK_ADD + (spd - 50)`, which is SPD+150 when the dword's initial value is 200. The comment on `WALK_ADD` says the result ("walk speed is SPD + 150"). `ai.c:18` writes `ATTR_SPD + 150` for the stealth momentum test, and `dungeon.c:977` writes the same sum for a foe's chase speed. Same result, three spellings, and `WALK_ADD` is not the 150. The `/200`, `+320`, `/256` and the water `>>= 2` are inline in both trees. Water skips the swim term when `conditions & 0x100000` (`move.c:365`); that bit is unnamed.

Climb: `skill[18]` plus 30 for player race 6, then two `rand(1,100)` tests, the first against 95 (`move.c:231`). The decomp's climb note (`docs/character-skills-math.md`) records the race-6 bonus and not a named constant for 95 or 30. Fatigue while moving (`survival.c`): 11, 44 in water on a failed swim check, 88 running, 22 climbing. Those four are unnamed. Max fatigue is `(STR + END) << 6` (`tables.h` says so in a comment; `magic.c`, `rest.c` and `bar_fatigue` still write the shift).

`jump_magnitude` (`vitals.c`) is the decomp's jump: `((SPD + STR) / 2) + 50`, then `jumping * magnitude / 100`. `fall_damage` is `((v / 256) / 40) - 7`, then `(d * d) / 2`. The 50, 40, 7 and 2 are inline. Not re-read out of the decomp's jump function in this pass beyond `docs/character-skills-math.md`, which has the `+ 50` and the `/100` and does not name them.

### Trade, guild rank, class maker

`trade_adjust` (`trade.c`) is `src/lifted/inven.c` around the buy and sell returns. The decomp leaves the coefficients inline (`inven.c` `skill_factor * 192` buying, `* 179` and `* 51` selling, all over 256). So does `trade.c`: quality term `((quality - 10) * 5) + 50`, then `<< 8`, `/200`, `+128`, `/256`, and those 192 / 179 / 51 weights. `trade.h` names the functions and not the weights. `trade_base_price` divides by 1000 (the region adjust) and doubles. `trade_repair_line` is a tenth, three quarters when enchanted (`trade.h` says so). `trade_identify_line` is `(value * 25) >> 8`.

`guild_rank_for_skills` (`guild.c:137`) is `reputation / 10`, then the first step the skill reaches, capped at 9. `guild_join`'s refusal is primary `< 22` or secondary `< 4` (`guild.c:122`). `docs/character-skills-math.md` states that formula and says the step tables are data, not in the decomp's C. `g_rank_primary` / `g_rank_secondary` are that data. The 10, 22, 4 and 9 are inline.

Class-maker cost (`chargen.c` `custom_specials`, comment names `classmaker_set_advantage` and `update_advancement`):

```
cost = 65536 + (hp_per_level - 8) * 3277 * (hp_per_level < 8 ? 2 : 1)
cost = cost < 6554 ? 6554 : cost
```

`src/lifted/custom.c` does the same with the numbers inline: `(hp_per_level - 8) * 3277`, doubled when `hp_per_level < 8` (`<<= 1`), added to `advantage_cost + 65536 - disadvantage_cost`, floored at 6554. A new class starts at `hp_per_level = 8`, `advancement = 65536`, attributes 50, skill pools 6. Those are the decomp's chargen floors (`docs/character-skills-math.md`).

### Item group and weapon index

No `GROUP_*` anywhere under `src/game/dagger/`. `play/` identifies an item by `ObjectInfo.group` and `.index`. The template ids in `items.h` (`ITEM_DAGGER` 113, `ITEM_ARROW` 131, `ITEM_GOLD` 276) are a different space and are not what these `==` tests mean.

Groups this pass can attach to a use or a decomp line:

| group | why that reading | sites (not all of them) |
| --- | --- | --- |
| 2 | armour. `item_name` picks "Leather" / "Chain" / the metal for group 2 | `inventory.c`, `foegear.c`, `describe.c` |
| 3 | weapons. `ARROW_GROUP`. `weapon.c:44` refuses to switch onto a hand whose item is not group 3 | throughout `inventory.c`, `weapon.c` |
| 6 and 12 | clothing, men and women. `func_00010585.c` sets `clothing_gender_group` to 6 or 12 | `inventory.c` paper doll, `state.c` gifts |
| 27 index 0 | the spellbook. `state.c` calls `inventory_start_spells` for that pair | `inventory.c`, `bank.c`, `possessions.c` |
| 18 inside group 3 | the arrow. Already `ARROW_INDEX` | section A |

Weapon indexes 16 and 17 are the bows (`weapon.c:109`). Index 17 with `count > 1` is rewritten to 18 by `inventory_upgrade` (`inventory.c:512`). That 17 is a save migration, not the bow; see section C. `foegear.c` rolls weapon indexes `0..3`, `5..7`, `9..14` by tier. Those ranges are unnamed.

Other group numbers tested in `play/` (`0`, `1`, `4`, `5`, `7`, `8`, `9`, `11`, `13`, `14`, `23`, `24`, `25`, `28`, and the magic effect groups which reuse the word `group`) are left here as unknown rather than labeled from memory. `possessions.c` treats group 7 as a book, group 13 as a painting, group 9 index 5 as a letter, and skips group 23. Those are uses, not a header.

### Magic condition bits and the resist roll

`magic.c` `condition()` maps an effect group to a bit in `CharacterState.conditions`:

| group | bit |
| --- | --- |
| 0 | `1` |
| 13..32 | `1 << (group - 11)` |
| 35 | `0x400000` |
| 39 | `0x800000` |
| 42 | `0x1000000` |
| 44..46 | `1 << (group - 19)` |

`0x400000` is the bit `qkey.c` tests before damage comes out of `shield_points` (`character->conditions & 0x400000`). No name in either tree. `magic.c:111` and `dungeon.c:488` preserve that bit across a commit that would otherwise clear it. `0x800000` and `0x1000000` are the same function's bits for groups 39 and 42. Which effect those groups are: unknown in this pass.

`magic_resist` (`magic.c`, `spfx_resist_roll`): chance starts at 50, immunity returns 0, critical weakness returns 100, low tolerance halves, resistance adds half again, then the racial +30, clamp `5..95`. A roll above the chance returns 100 (full effect). A roll below `chance - 20` returns 0. Otherwise `(chance - roll) * 5`. All of those are inline in `spfx.c` too. The biography MR term the decomp adds on top (`D_0018DDD8`) is not in `magic_resist`.

`magic_sees`: `conditions & 0x3004`, then bit `4` hides, and bit `0x2000` makes the fail chance 8 instead of 4. The mask and the two chances are unnamed. `docs/combat-math.md` names condition `0x200` absorption, `0x400` reflection, `0x800` spell resistance, which are other bits of the same word.

Spell cost `(110 - skill) * raw / 100`, then `target_factor >> 1`, floor 5 (`spell_cost`). The 110 and the floor 5 are inline. The decomp's cost note uses the same `110 - school_skill`.

### Law, bank, and the rest of the small rule files

`law.c` writes sentence days, reputation deltas and gold thresholds inline (a day for each forty gold short, at the repay path). The formulas are the decomp's court notes in `docs/ai-social-math.md` where this pass did not re-read each one. They have no names in `law.h`.

`weapon_delay` is `(115 - speed) * 3`, at least 1. `bow_delay` is `(100 - speed) * 10 + 1000`, at least 200. Not found in the lifted C during this pass. Section C.

Quest opcodes stay numbers. `quest.c` says "op 29", "ops 54 and 36", "opcode == 2" in comments and in `if` tests, and `formats/qbn.h` does not enum them. That is deliberate: the opcode is the file's. `0x2000` on a quest person ("talk passes through") and `0x40000000` on a quest `who` are unnamed flags in `quest.c`. `QUEST_PERSON_MARK` and `QUEST_FOE_KEY` are the flags that did get names.

## C. Unexplained, or easy to misread

| site | number | what it actually is |
| --- | --- | --- |
| `move.c:19` `WALK_ADD` 200 | comment says "SPD + 150" | `200 + (spd - 50)` is SPD+150. The 200 is `D_00187CA9`'s initial value in `func_00010585.c`, not a second formula. `ai.c:18` and `dungeon.c:977` write `+ 150` and never mention `WALK_ADD`. Using `WALK_ADD` there would be SPD+200 |
| `combat.c:112`..`122` | flash `4..32`, `/32`, decay `dt*2` | `4` and `32` match `damage_player_hurt`, which then shakes by `n << 3`. The division and the `dt*2` fade are not in that function. At the cap the fade lasts half a second. Unknown whether that was chosen to match the shake |
| `dungeon.c:204` | `(thing % 4000) / 1000`, then `+ 1` | first bark is spread over about 1..5 seconds by the entity key. Later barks use `random_range(2000,6000)/1000` (`dungeon.c` voice reset). Not found as a decomp formula in this pass |
| `dungeon.c:340` `RAY_STRETCH` 384 | a named local | the comment says one box the length of a long sight ray overflows the collide gather, so the ray is walked in stretches of 384. The pattern to copy. Not a missing name |
| `inventory.c:512` | weapon index 17 with `count > 1` becomes 18 | `inventory_upgrade`. Index 17 is also a bow (`weapon.c:109`). The upgrade only fires when the count is above 1, which a bow is not. It is a save rewrite of an old arrow index, not FALL.EXE's bow |
| `magic.c:187` race `2` and `0` | look like `RACE_NORD` / `RACE_BRETON` if those enums are read as player races | they are player Nord and player Breton. `RACE_NORD` is 3 and `RACE_BRETON` is 1. The elements beside them are `ELEMENT_COLD` and `ELEMENT_MAGIC` |
| `words.c:80` | `career_race(player race)` | record ids are 1..8, player race is 0..7. Breton misses. Not a magic number at the call; it is the two numberings |
| condition `0x400000` | preserved across spell commit | the shield bit in `qkey.c`. Groups 39 and 42 (`0x800000`, `0x1000000`) have no such note. Unknown which effects |
| `0x80000000` on a keep key | `encounter.c`, `ground.c`, `state.c` | a generated thing, not a condition bit. `0x40000000` on a quest person (`quest.c`) is a different flag again. Three different high bits |

`RAY_STRETCH` and `WALK_ADD` are the named-local pattern. `WALK_ADD`'s comment describes the result and the macro is the dword, which is why it gets misread.

## Screen rectangles

Classic's screen is `CANVAS_W` 320 by `CANVAS_H` 200 (`canvas.h`). Buttons are `CANVAS_BUTTON_W` 32 by `CANVAS_BUTTON_H` 16. Files that already keep hit targets in a static table:

| file | table | rows |
| --- | --- | --- |
| `hud.c` | `g_rect[HUD_BUTTONS]`, `HudRect` | 11, comment cites FALL.EXE `hud_buttons`, strict edges |
| `controls.c` | `g_rect[42]`, `ControlRect` | 42, comment cites `controls_buttons` at `0x17B808` |
| `travelmap.c` | `static const Rect g_*` | 21 named rects, plus `g_filter_at[FILTERS]` and `g_filter_part[FILTERS]` |
| `townmap.c` | `static const Rect` | 10 |
| `dunmap.c` | `static const Rect` | 12, the same band as the town map (grid at 78,171, exit at 281,171) |
| `chargen.c` | `chargen_attr_y` `y[]` | 8 attribute rows. Skill rows are `31 + slot*10`, `80 + (slot-3)*10`, `129 + (slot-6)*10`, not a table. Answer boxes are one `static const int box[]` of 8 rectangles (`chargen.c` `answer_box`) |
| `possessions.c` | `bank_button` `box[11][4]`, plus `left[]`/`top[]`, `tabs[4][4]`, `rowtop[4]`, `button_y[4]` | hit boxes as `x0,y0,x1,y1` integers, not `Rect` |

`townmap.c` and `dunmap.c` repeat the same band (grid 78,171 27x19, arrows 21x19, exit 281,171 28x19, compass 3,172 76x17). One table would cover both; they were copied.

`possessions.c` is the largest literal file because the rest of the inventory, the court, the talk list and the shop buttons are still coordinates at the draw call. Representative bank row, already in a table: `{166,60,216,74}` and `{217,60,268,74}` are the first two of eleven. `guild.c` (292 literals), `spellmaker.c` (154), `talkwin.c` (120) and `ui.c` (177) do not have a `Rect` table. `ui.c`'s only static array is `g_side[] = {128, 256, 128}`, atlas sizes, not a screen. `spellmaker.c` has `base[]`, `step[]`, `top[]`, `offset[]` for the eleven parameter columns, which is the right shape, and still draws with loose coordinates around them.

No dump of every rectangle. A cleanup that moves a screen should add a `static const Rect` (or the `box[][4]` possessions already uses) and leave the numbers alone.

## Where a name should live

The pattern already in the tree:

- A `#define` block in the file that owns the rule, immediately above the use: `curse.c` (infection, moons, hands), `move.c` (`WALK_ADD`, `TURN_PER_SEC`, `CIRCLE`), `canvas.h` (screen and ink), `state.h` (save sizes, record types), `trade.h` (`ARROW_GROUP`, `ARROW_INDEX`).
- A `static const` table for rectangles and for a list of ids (`vampire_skills` should be the ids' names, the way `lycanthrope_attributes` already is).
- Quest opcodes stay comments. `quest.c` says "op 29". Do not enum `COUNT_OPS` of them.

| family | header | note |
| --- | --- | --- |
| `SKILL_*` | `tables/careers.h` | already there. `play/` includes it and deletes the private copies. `tables.h` `SKILL_JUMPING` goes with them |
| player race 0..10 | `tables/tables.h`, next to `race_name` | a different enum from `RACE_*`. Suggested spellings `PLAYER_RACE_BRETON` = 0 through `PLAYER_RACE_ARGONIAN` = 7, then the curse forms already described in `curse.h` |
| `RACE_*` | `tables/careers.h` | record ids only. `career_race` and `names.c` |
| `ENEMY_*` | `tables/enemies.h` | add the mobiles `play/` writes and `enemies.h` does not: spider, orc, centaur, nymph, orc sergeant, giant scorpion, orc shaman, orc warlord. `43` as the class-enemy race sentinel belongs next to `dungeon.c`'s assignment, or as `ENEMY_RACE_CLASS` beside `ENEMY_FIRST_CLASS` |
| `EQUIP_*` | `play/state.h`, the `equipped[27]` | the decomp's 27 names. `HAND_SLOTS_FIRST` / `HAND_SLOT_SECOND` become `EQUIP_RIGHT_HAND` / `EQUIP_LEFT_HAND` |
| `ARROW_GROUP`, `ARROW_INDEX` | `play/people/trade.h` | already there |
| weapon indexes 16 and 17 | `play/combat/weapon.h` | bows. Do not reuse 17 for the save-upgrade arrow |
| `MATERIAL_*` | `tables/items.h` | already there |
| `ELEMENT_*` | `tables/magic.h` | already there |
| effect-group condition bits | `play/magic/magic.h` | `0x400000` is the shield bit. Groups 39 and 42 wait until the effect is known |
| item groups | a new block in `tables/itemdefs.h` | `play/`'s group/index space, not `ITEM_*`. Only the groups a decomp line or a comment in `play/` names (2, 3, 6, 12, 27) until the others are checked |
| trade weights 192, 179, 51, and the `/200` `+128` `/256` chain | `play/people/trade.h` | the functions are already documented there |
| class-maker 65536, 3277, 6554 | `play/character/chargen.h` | `custom.c` leaves them inline too |
| guild 10, 22, 4, 9 | `play/people/guild.h` | reputation per rank, join floors, rank cap |
| walk result SPD+150 | `play/input/move.h` | not `WALK_ADD`. `WALK_ADD` stays the dword 200 |
| rectangles | `static const Rect` in the screen's `.c` | `hud.c`, `controls.c`, `travelmap.c` |

`places/` already uses `ENEMY_RAT`, `ENEMY_GHOST`, `ENEMY_WRAITH`, `ENEMY_CITY_WATCH`, `ENEMY_DRY`. The play-layer gap is not that the names were never chosen. `tables/classic.c` holds the skill and mobile names as strings in the record rows, which is why a reader can find "Dodging" and still see `skill[20]`.

Engine constants `play/` should have used, and mostly did not need to: `CANVAS_W` / `CANVAS_H` exist and the map rects still write `320`. `DF_TAU` and `2048` (`CIRCLE` in `move.c`) are the same circle; `weapon_sector` writes `2048` again. No other engine constant turned up as a rule number `play/` reinvented.

## Cleanup, in order

Behavior does not change. A rename that needs a different integer is a bug, not a cleanup. `RACE_*` is the trap. Do the call sites only after the header has the right ordinal.

1. `careers.h` skill names at the 27 sites, including `vampire_skills` and `lycanthrope_skills`. Delete the eleven private copies (`SKILL_*`, `ATTR_PERSONALITY`, and `ai.c`'s `ETIQUETTE`) and use `ATTR_PER`.
2. `ENEMY_*` at the 18 comparisons and the 17 range tests. Add `#define`s for spider, nymph, giant scorpion, and the five gear kinds before touching those lines.
3. `ARROW_GROUP` and `ARROW_INDEX` at the 12 sites.
4. `MATERIAL_SILVER`, `MATERIAL_IRON`, `ELEMENT_COLD`, `ELEMENT_MAGIC` at `combat.c:70`, `magicitem.c:180`, `magic.c:187`.
5. A 0-based player-race enum in `tables.h`. Do not pass it to `career_race`. Do not replace `race == 0` with `RACE_BRETON`.
6. `EQUIP_*` in `state.h`, from the decomp's 27. Point `HAND_SLOTS_FIRST` / `HAND_SLOT_SECOND`, `foegear.c` `gear_slot[]`, and the raw `19`/`21` hands at it.
7. Name the to-hit clamp (`3`, `97`, the `-50`) and the monster `+40` next to the skill names, in `combat.h`. The attacker's Dodging stays a comment until the name is in the expression.
8. One spelling of walk speed SPD+150 in `move.h`. Leave `WALK_ADD` as 200. `ai.c`'s `+150` uses the result, not `WALK_ADD`.
9. The shield bit `0x400000` in `magic.h`. Leave `0x800000` and `0x1000000` unnamed until groups 39 and 42 are identified.
10. Trade weights in `trade.h`, class-maker `65536` / `3277` / `6554` in `chargen.h`, guild `10` / `22` / `4` / `9` in `guild.h`. The decomp leaves the same numbers inline; the names are for the next reader.
11. Item groups 2, 3, 6, 12 and 27 in `itemdefs.h`, only those, until the other groups are checked against the decomp.
12. New screens follow `hud.c` / `controls.c` / `travelmap.c`: a `static const Rect` (or possessions' `box[][4]`). `townmap.c` and `dunmap.c` share one band today and each keep a copy. Do not retune a coordinate while naming it.
