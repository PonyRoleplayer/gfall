# OGFALL: enchantments in play (plan and decisions)

Written 2026-11-10 before any code, from the decomp's `docs/item-enchantments-in-play.md` (DOCUMENTER; section numbers below are its) and `docs/item-making-math.md` section 5. The item maker (`docs/ogfall-itemmaker.md`) makes and shows enchanted items; this is what they do. Classic first; where the original has a plain bug we do what it meant.

## Decisions on section 9

| # | What the original does | We do |
| --- | --- | --- |
| 9.1 | using an item latches the "ignore silence" flag for the session | **Fix.** Silence is ignored for the item's cast only. |
| 9.2 | aborting an item's readied spell refunds the last spellbook spell's cost | **Fix.** Nothing is refunded (the item's spell cost nothing up front). |
| 9.3 | Potent vs and Low damage vs never apply; the class's attack modifiers are lost too | **Fix.** Potent vs adds the attacker's level against the category, Low damage vs takes it off, and the class modifiers stay (`docs/ogfall.md` already has them). |
| 9.4 | an area strike spell is centred on the wielder (Fire Storm burns them), target 4 does nothing | **Fix.** Centred on the target; a target-4 spell bursts at the target. |
| 9.5 | the portrait's "under a spell" flag is never cleared, so poison and disease frames never show | Moot: OGFALL's portrait indicators (`og_magic_indicators`) are its own. |
| 9.6, 9.7 | a weapon branch tests the player's race for every attacker; creatures read the player's race as a creature id | **Fix.** The attacker's own race decides; the player's category is by their form (humanoid, or the beast's own). |
| 9.8 | a soul-bound item that breaks can crash | **Fix.** The summon is tested. |
| 9.9 | beast form, an unpaid item handed back, a quest taking an item clear a slot without removing its effects (+15 skill and absorption stick) | **Fix.** Every path that takes an item off removes its effects (`og_inventory_unequip` is the one path). |
| 9.10 | "first only" rules (armour, weight allowance, weight, absorption cleared by any item off) | **Keep**, except absorption: it is on while any worn item holds Absorbs spells. |
| 9.11 | a held spell survives while any enchanted item sits in its slot | **Fix.** It ends when its own item leaves. |
| 9.12 | a spell id missing from SPELLS.STD runs past the table | **Fix.** A missing spell is skipped. |
| 9.13, 9.14 | a template left behind; a magicka byte nothing sets | Moot. |
| 9.15 | level-ups, guild healing and cures unequip and re-equip every enchanted item, recasting held spells | **Not reproduced**: nothing in OGFALL takes items off for them. |
| 9.16 | several held spells wear the item once, by the last one's cost | **Fix.** Each held spell wears the item by its own cost. |
| 9.17 | tick amounts come in steps of 4 minutes; gaps over 100 minutes give nothing | **Keep** (a documented rule: resting a night does not regenerate through Regens health). |
| 9.18 | a tick that breaks an item reads the freed item's next enchantments | **Fix.** Safe iteration. |
| 9.19 | Health leech whenever used costs the wielder on every hit before the damage is known | **Keep.** |
| 9.20 | every icon 200 or over (held and strike spells) draws icon 10 | **Keep** for now (the original's look); the spell's own icon is a later change. |
| 9.21 | "Spell was absorped." | **Fix** the spelling. |

## Slices

1. **Equip and unequip** (section 2): held spells (a lasting effect with icon 200 + slot, ended with the item), Absorbs spells (a condition while any worn item has it), Enhances skill (+15 to the skill, -15 off), Regens health's stamp; and what they show: the active effect icon on the HUD (icon 10 for 200 and over, blinking by the time left), the inventory's enchanted mark (TEXTURE.380 record 5 under the item), the weapon in hand from WEAPO1##.CIF, messages.
2. **The tick** (section 3): Extra spell pts, Regens health, Vampiric at range, Repairs objects, Item deteriorates, User takes damage, Health leech (last kill), wear and breaking (section 6) with the messages.
3. **Use** (section 4): Cast when used from the inventory's use mode and the magic items list, the usable icon, Health leech on use, wear 10.
4. **Combat hooks** (section 5): Potent vs, Low damage vs, Cast when strikes, Vampiric on a hit, Health leech on a hit, armour (12/24), creatures with enchanted weapons.
5. **Carrying and people** (types 7, 11, 23, 14, 25): weight allowance, weights, reactions in talk.

## Slice 1 built (2026-11-10)

- **One path**: `og_state_equip` (state.c) removes the old item's effects, sets the slot and applies the new item's, so every way an item goes in or out (the inventory, the paper doll, the hands, a trade, a drop) applies and removes (9.9 fixed). Loading a save applies nothing (the skill, the condition and the lasting spells are in the saved state).
- **`play/enchant.c`**: Cast when held (type 1): `og_magic_hold` casts the spell on the player as a lasting effect (every effect's duration -1, icon 200 + the item's slot, level 8 as for every icon of 200 and over, no merging with an identical one), `og_magic_release` ends it with the item (9.11 fixed); Absorbs spells (9): the condition 0x200 while any worn item holds it (9.10's absorption fix); Enhances skill (10): +15 and -15. `og_item_enchantment` reads an item's enchantments whether made here or a guild's.
- **On screen**: the HUD's effect icons already draw icon 10 for 200 and over; a lasting spell no longer blinks (`permanent`). The inventory's lists mark an enchanted item with TEXTURE.380 record 5 under its picture (`paint_enchant_mark`; the equipment slots on the paper doll do not yet). An enchanted weapon that is not a bow is drawn from `WEAPO1##.CIF` (`OgWeaponView.enchanted`), the plain picture when there is none.
- **Not yet**: the wear a held spell puts on the item (its spell cost, once the wear and break rules of slice 2 exist), Regens health's stamp, and every tick effect.
- Test: `check_ogmagicitem.c`: rings made with the item maker, worn and taken off.

## Slice 2 built (2026-11-10)

- **Wear and breaking** (`og_item_damage`, `item_break` in `play/enchant.c`): the condition goes down by n; an item with no more than that breaks: condition 0, "%s is broken.", worn effects removed (it is taken off), a soul-bound enchanted item says "The soul from your broken item is released." and an enchanted item is deleted (a plain one stays in the pack, broken). Not built: the soul's creature coming back hostile (the summon; 9.8 is moot until it is).
- **The tick** (`og_enchant_minutes`, from `og_state_pass_minutes`; `effects_tick`'s amount: the minutes since the last step over four, a gap over 100 thrown away, the stamp reset by equipping Regens health): Cast when held wears 1 a step; Regens health by param (all the time, daylight, darkness), 1 in 10 wears when it did not reach the maximum; Vampiric effect at range hurts the nearest creature within 128 and heals and wears by the amount; Repairs objects mends the first damaged carried item (not arrows or magic items), 1 in 10 wears; Item deteriorates (always, daylight, temple) breaks the item when it has no more than the amount; User takes damage (daylight, temple) through `og_damage_player`; Health leech params 1 and 2 after 1440 and 10080 minutes since the last kill (`last_kill` in the state, schema 15, set by the player's kills). The held spell's cost now wears the item at equip, each spell by its own cost (9.16).
- **Extra spell pts** (`og_enchant_frame`, every world frame): +75 maximum magicka while the parameter holds (season by month, the moons by `(minutes / 1440) & 31` and the other plus 5760, a creature of the category within 1024), the maximum rebuilt from Intelligence and the class each frame while an item gives one and once more when the last comes off; the magicka grows by 5 a step for the creature parameters.
- **The world** the tick asks (`OgEnchantWorld`, `seam.c`): daylight is 6:00 to 18:00 outdoors, a temple is a building of type 14, creatures are the dungeon module's (`og_dungeon_nearest_distance`, `_category_within`, `_hurt_nearest`).
- Tests: `check_ogmagicitem.c` (wear and break, the 8-minute and 100-minute steps and the 150-minute gap, Extra spell pts on and off, an item deteriorating until gone). Not tested by hand: Vampiric, Repairs objects, User takes damage and Health leech's clock (they use the same step).

## Slice 3 built (2026-11-10)

- **Use** (`og_item_use` in `enchant.c`, from `og_possessions_use`, inv_use_item): every Cast when used of the item fires in order, each wearing the item by 10 (even when the spell then has to be aimed), and Health leech "whenever used" costs the user `(level >> 1) + 1` health through `og_damage_player` and wears the item by 2. The item's ordinary use (books, clothing, the spellbook) follows for those groups; an enchanted item with nothing else is used up in the one step.
- **The cast** (`og_magic_cast_item` in `casting.c`, cast_item_used_spell): nothing is paid, silence does not stop it and is not left off afterwards (9.1 fixed), the last spellbook spell is untouched, aborting refunds nothing (9.2 fixed). A spell on the caster goes at once with the element's hand and sound; any other is readied as a spell object of its own in the pack ("Press button to fire spell."), fired with the attack button like a spellbook spell, and the object is deleted once the spell is no longer readied. The inventory is put away when a spell is readied so it can be aimed.
- **The magic item key** (`OG_MAGIC_ITEM`, magic_items_open): a list of the carried items with a Cast when used, each by name; the pick is used. Nothing opens with none.
- Test: `check_ogmagicitem.c` (Levitate on the user and the wear of 10, a Wizard's Fire ring readied and put away, Health leech's cost and wear).
- Not built: the "usable" icon, artifacts' uses, the casting delay by Intelligence, the quest event, and the skill training an item's spell would give.
