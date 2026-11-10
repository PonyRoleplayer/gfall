# OGFALL: people and law

Milestone 6 of [ogfall-roadmap.md](ogfall-roadmap.md), planned 2026-11-10. Classic
behaviour first, from the byte-matched decompilation at
`/home/shade/git/daggerfall-decomp`; improvements go on top of it later and are
listed in one place. Cite decomp functions in comments, never paste lifted code.
OGFALL's game folder does not include dagger headers: `apps/ogfall/seam.c`
adapts data into plain structs.

## Where the rules are

The decomp's own maths notes already worked the arithmetic out from its C, with
file and line references. Read them before the C:

| Topic | Notes (`docs/` in the decomp) | Code (`src/lifted/`) |
| --- | --- | --- |
| Shop prices, buying, selling, haggling, repairs, identification, item value | `economy-math.md` (Shared shop price .. Item value) | `inven.c`, `trade.c`, `generate.c` |
| Training, temple services, guild rank, tavern, bank, houses, ships, loans, fares | `economy-math.md` | `guilds.c`, `tavern.c`, `bank.c`, `travel.c` |
| Crime, fines, jail, legal reputation, guards | `ai-social-math.md` (Crime ... Guards) | `crime.c`, `people.c`, `steal.c` |
| Faction reputation, politics, relations | `ai-social-math.md` (Faction reputation) | `faction.c`, `talk.c` (1620..1800) |
| Dialogue disposition, tones, topics, rumours | `ai-social-math.md` (Dialogue disposition) | `talk.c`, `rumor.c`, `parse.c` |
| Pickpocketing, shop steal, lockpicking | `ai-social-math.md`, `character-skills-math.md` | `steal.c`, `click.c` |
| Court fines, carrying gold, regional prices | `economy-math.md` | `crime.c`, `inven.c` |

What the notes say is not in the tree (extern tables such as the full crime
fine table, `talk_knows_mods`, the greeting text ids) must be read out of
FALL.EXE with `tools/ogfall-data.py`, which already checks the executable's
SHA-1 against the decomp and reads tables at the addresses the decomp names.

## Slices

Each slice lands alone: its pure rules host-tested against the notes' worked
numbers, its screens driven by scripted clicks, its state in the save schema,
and a short audit section in `ogfall.md`.

1. **Standing.** (Built 2026-11-10, host-tested.) The faction records (id,
   type, parent, allies, enemies, reputation, power, social and guild group,
   region), the regions' legal reputation and price adjustment,
   `faction_change_reputation` and its propagation, and the decay. No screens.
   Everything later reads this. Guild rank from reputation waits for the guilds
   (it needs two extern tables), and building identity and `holiday_today` wait
   for trade, their first user.
2. **Trade.** (Built 2026-11-10: prices, shelves, the shop session and the buying screen over a shelf; selling and repair wait for talk, and building names for their own step.) The price chain (`trade_region_price`, `trade_base_price`,
   `trade_adjust_price`), shelves and stock, buying, selling, the offer and
   counter-offer dialogue, repair, identification, on OGFALL's inventory screen.
3. **Bank.** (Built 2026-11-10: accounts, letters, loans, sales and the screen; buying a house or a ship and the overdue effects wait for the wider world.) Regional accounts, deposits, withdrawals, loans, repayment and
   default, letters of credit, houses and ships, carrying gold.
4. **Lodging.** (Built 2026-11-10: rooms, meals, where rest is allowed; the screens wait for talk, the watch for law.) Tavern rooms and their expiry, food and drink, and where rest
   is allowed (ties into the existing rest).
5. **Law.** (Built 2026-11-10: crimes, fines, the court and its screen, sentences, reputation, the guards' rules and the arrest; making guards in a town waits for the streets' creatures.) Crimes and their witnesses, guard summoning and behaviour,
   `court_open`, fines, sentences, prison, reputation restore.
6. **Talk.** (Built 2026-11-10: who talks, the greeting, attitude and knowing, the topics, the answer and the directions' hint, and the window itself on TALK01I0.IMG with its 19 buttons, host-driven by clicks; the rumours, the quest topics, the copy to the logbook and the regional search remain.) `talk_open`, disposition, tones and the attitude roll, topics,
   directions, rumours and the logbook copy.
7. **Steal.** (Built 2026-11-10: the pocket, the trade screen's Steal button and the shelf's alarm roll as rules; the pocket and the shelf question wait for the people and the merchant panel.) Pickpocket, shop steal, container theft and the alarm roll, with
   slice 5's consequences.
8. **People.** (Built 2026-11-10: the names of people and signs over doors, a street person or one in a building met by the hud's mode, the town's buildings carrying their sign seeds, the shopkeeper's flag from the block, and the guild halls' and temples' menu with the guild memberships behind it (slice 9); training, cures, blessings, healing, the innkeeper, the knights' armour and opening hours are built; the potion and soul-gem sellers, the potion maker and the spell shop are built; magic items, the spell and item makers, the Daedra, teleporting, temple quests and the quest people remain.) Standing NPC roles, schedules and portraits, on top of the
   existing pedestrian pool, without reinterpreting it before slices 1 to 7 are
   in.

9. **Guilds.** (Built 2026-11-10: memberships, joining, rank, expulsion, invitations, the halls' and temples' gating and menu, schema 12; with training, cures, blessings and healing; potions, spells, magic items, soul gems, the Daedra, teleporting and the temples' quests remain, each a subsystem of its own.) The 22 guild kinds the original has, their messages and rank steps read out of FALL.EXE.

Order is by dependency: standing feeds trade, bank and law; law feeds steal;
talk needs standing and the quest hooks left by milestone 8.

## Open choices

- Where the faction table comes from. GFALL reads `FACTION.TXT` from ARENA2
  (`formats/factionfile.c`). The decomp parses the same file (`faction_parse_*`),
  so OGFALL can take the seam's already-parsed records, or its own parse of
  the file. The seam route is cheaper; a parse of our own removes a dagger
  dependency. Either way the in-game structure follows the decomp's record.
- Whether OGFALL's tables move to MAIN.DAT records (the engine's `recfile`) as
  they are extracted, so later changes are data mods. Not needed for slice 1.

## Acceptance

From milestone 6: obtain directions, buy and sell through the original screens,
rent a room, use a bank in two regions, and follow both a successful theft and
an arrest and trial through save and load. Nearby guards are verified on the
target.
