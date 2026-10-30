# A character's numbers

`src/engine/scene/rpg.[ch]` holds a character's attributes, level, experience and vitals, and works out one blow's to-hit and damage. `src/engine/ui/rpgview.[ch]` is a screen over them. Both belong to the engine and know no game: every number comes from an `RpgRules` the game fills in. Both are plain C, and hostcheck runs them (`check_rpg`).

No game calls them yet. GFALL's `play/character.c`, `rules.c` and `combat.c` are separate and unchanged.

## The rules

Weights are in hundredths: a weight of 50 on an attribute adds half a point for each point of it.

| Rule | Means |
| --- | --- |
| `attr_name`, `nattrs` | up to 8 attributes, named by the game |
| `attr_start`, `attr_min`, `attr_max` | where each starts and the range it stays in |
| `create_points`, `level_points` | points to spend at creation, and at each level gained |
| `vital[]`, `nvitals` | up to 4 vitals. Most = `base` + `per_level` × (level − 1) + the attributes by `weight` |
| `xp_first`, `xp_growth`, `level_max` | experience from level 1 to 2; each later level takes `xp_growth` percent of the last |
| `hit_base`, `hit_min`, `hit_max` | a blow's chance, percent, before and after holding it to a range |
| `attack_weight`, `defence_weight` | attributes added to the chance for the attacker, taken off for the defender |
| `damage_weight` | attributes added to a weapon's roll |

## Creation and levelling

- `rpg_init` makes a level-1 character with the creation points to spend. An attribute may be lowered to `attr_min` to free points.
- `rpg_raise` and `rpg_lower` move a point. `rpg_commit` makes the spending final and fills the vitals. It refuses while points are left and an attribute could still take one.
- After a commit, `rpg_lower` stops at the committed value, so a level's points can be moved around until they too are committed.
- `rpg_gain_xp` carries experience over from level to level. Each level gives its points and fills the vitals. Past the last level, none is kept.

## A blow

`rpg_blow` takes a weapon's damage range, the game's bonuses, and an armour value:

- **Chance:** `hit_base` + the attacker's attributes by `attack_weight` − the defender's by `defence_weight` + the attack bonus − the defence bonus, held to `hit_min..hit_max`.
- **Damage on a hit:** a roll of the range + the attacker's attributes by `damage_weight`, × 100 / (100 + armour), at least 1. Armour 100 halves it, and 300 quarters it, so armour never makes a blow worthless.

Dice come from a seed the caller keeps (`rpg_roll`), so a test or a replay can repeat them. Bonuses and armour are the game's to supply: `pack_stat` (`docs/pack.md`) adds up what a character wears, which is how equipment changes a blow.

## The screen

`RpgView` holds a cursor over the attributes, and no keys or font:

- The game turns its input into `RPGVIEW_UP`, `DOWN`, `RAISE`, `LOWER` and `CONFIRM`, and calls `rpgview_command`. Creation and a level's points use the same list.
- The game passes its text call to `rpgview_draw`, for example `ui_string`: the heading (`Level 1  XP 0/100  Points 10`), the attributes (`>Might 11`), then the vitals (` Health 30/30`), each lower than the last.

## Not here

Classes, races, skills, backgrounds, names, pictures, saving a sheet, and what a game does when a vital reaches 0. An `RpgSheet` has no pointers, so it can be written as bytes.
