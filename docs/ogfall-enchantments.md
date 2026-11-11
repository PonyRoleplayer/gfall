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
