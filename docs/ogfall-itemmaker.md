# OGFALL: the item maker (plan and decisions)

Written 2026-11-10 before any code. The rules, tables, screen and effects are in the decomp's `docs/item-making-math.md` (written by the DOCUMENTER session from `itemmakr*.c`, `func_000562A1.c` and FALL.EXE; read it first, its section numbers are used below). Classic first: where the original has a plain bug we do what it meant, and the list says which.

## Entry
Mages Guild (faction 802) and Julianos (481), rank 5 or higher (section 1). `OG_SVC_ITEM_MAKER` is in the guild table; it needs a case beside `OG_SVC_SPELL_MAKER` in `seam.c`.

## Decisions on section 7

| # | What the original does | We do |
| --- | --- | --- |
| 7.1 | a side effect takes its exclusion row and pair from the last power added | **Fix.** Use the side effect's own pair `(e + 15, choice)` and row `D_0018597F[e]`: side effects exclude themselves and their opposites as designed. |
| 7.2 | Escape on a parameter list leaves a half-made slot | **Fix.** Escape takes the slot out again, as the spell maker's does. |
| 7.3 | a kind with every parameter taken stays in the list | **Fix.** Hidden when it has no parameter left. |
| 7.4 | "All" (param 5) reads past a five-entry exclusion row (hides Levitate) | **Fix.** "All" excludes only what a row of six says it should; no read past the row. |
| 7.5 | a soul's side effects index the row table with 15..25 | **Fix.** Index by `type - 15`; Extra weight and Feather Weight write each other's pair as a manual pick does. |
| 7.6 | a row click is off by one; the row maps go stale | **Fix.** The parameter line and the name line both remove the slot; the maps are rebuilt each frame; no overlap. |
| 7.7 | Repairs objects and Absorbs spells can be added twice | **Fix.** Each writes an exclusion pair, so once. |
| 7.8 | weapon-only effects fit any item | Keep (nothing reads them there; harmless). |
| 7.9 | soul-granted powers cost gold, not points | Keep, noted in the screen's help. |
| 7.10 | picking a second item keeps the slots | **Fix.** The first item is given back and the slots are cleared (TEXT.RSC 1655/1656 are unused in the original; we may say nothing). |
| 7.11 | renaming is immediate, 23 characters | Keep. |
| 7.12 | the soul list's "no souls" refusal is unreachable with mode 2 | Moot. |
| 7.13 | silver plate body armour is scaled twice at creation | **Keep for now**: it changes item generation (shops, loot, saves). Listed in `ogfall-later.md`. |
| 7.14 | a soul worth 1000 costs its value times its creature id | **Fix.** A soul worth 1000 costs -10 points like any other soul of its value (the table's own number). |
| 7.15 | an enchanted item's value is only its powers' cost | **Keep** (the shop's magic items already price this way; a later rule can change both). |
| 7.16 | Improves talents, Vision problems, Walking problems, Bad reactions from have no reader | Build the data; the effects stay inert until the effects stage (and these four have no effect in the original either). |
| 7.17 | "first only" effects | Keep. |
| 7.18 | Extra spell pts moon test dead assignment | Moot. |
| 7.19 | the wrong soul can be consumed | **Fix.** The soul picked is the one consumed; no limit of 63 from a shared buffer. |
| 7.20 | a broken soul-bound item can crash | **Fix.** The summon is tested. |
| 7.21 | ENCHANT with no slots pays 0 and marks the item enchanted | **Fix.** Refused with TEXT.RSC 1653 (no item) or nothing to do. |

## Stages
1. **Storage, tables, rules, tests.** A side table in the state (`custom_enchant`, keyed by item id: ten type/param pairs, a flag) with a schema bump; an item's enchant points from section 3.8 (material multiplier D_001869FE by template points) as a rule, not a stored field; `tools/ogfall-data.py` extracts every table of section 3 (names, param kinds and labels, spell lists, power and side-effect costs, exclusion rows, soul values and rows); `play/itemmaker.c`: slot state, add, param, remove, exclusion filtering, souls, points, gold, enchant, item value, the powers text (also shown in the item info box and for the shop's MAGIC.DEF items). Host-tested against section 6's worked examples.
2. **The screen** (ITEM00I0/ITEM01I0, 20 buttons, the left list, popups, name prompt) as `OG_VIEW_ITEMMAKER`, clicks in `possessions.c`, painting in `seam.c`.
3. **Souls.** The soul gems (the shop and `og_magic_soul_capture` exist), soul binding and consumption.
4. **Effects** (section 5): held, used, strikes, tick; one type at a time.

## Not decided here
Whether made items keep their enchantment table through the save as a side table (planned) or on the object (a larger schema change); the side table is smaller and leaves the object wire as it is.

## Stage 1 built (2026-11-10)

- **Tables** (`tools/ogfall-data.py` -> `tables/itemmakerdata.h`): the power and side effect names, parameter kinds and labels, the three spell lists, the side effects' costs, the power and side exclusion rows, the souls' powers and rule-outs (with the spell lists through their pointers), the materials' multiplier and each template's enchantment points. The powers' costs are `enchantdata.h` (already there for the shop's magic items) and the souls' values `souldata.h`.
- **State** (schema 14): `enchant[64]`, a row an item (by id): ten type and parameter pairs (powers 0..14, side effects 15..25; the first three powers hold the spell id) and the identified flag. A row whose item is gone is free for the next. The items bought at a guild keep their MAGIC.DEF entry in `message` (`og_magic_item_def`); the two are not yet one list.
- **Rules** (`play/itemmaker.c`, host-tested in `check_ogmagicitem.c` against section 6 of the decomp's document): capacity from the template and the material (weapons but arrows, plate armour; the second roll of 7.13 is not reproduced, the final material is used), picking (1659, 1660; another item clears the slots), the kinds offered and the parameters allowed, slots with their exclusion pairs, souls and their powers, points and gold, enchanting, the value, the powers text. The fixes of `docs/ogfall-itemmaker.md` are in: 7.1 (a side effect's own pairs), 7.2 (cancel takes the slot out), 7.3, 7.4 (no read past five parameters), 7.5, 7.7, 7.10, 7.14 (a soul worth 1000 costs -10), 7.21 (nothing chosen is nothing). 7.6 and 7.19/7.20 belong to the screen and the soul gems (stages 2 and 3).
- **Not built**: the screen, the entry from the guild, the soul list from the pack (`og_itemmaker_set_souls` is the hook), writing a made item's value into the item when the inventory opens, the powers text in the item info box, and what the enchantments do.
