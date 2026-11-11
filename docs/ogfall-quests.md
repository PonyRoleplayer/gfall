# OGFALL: the quest machine (plan and decisions)

Written 2026-11-11 before any code, from the decomp's `docs/quest-machine.md` (DOCUMENTER; section numbers below are its; it builds on the decomp's `docs/quests-math.md` for reward gold, timer sums and site picking). OGFALL builds its own machine from that document and the files in ARENA2 (232 QBN, 229 QRC); GFALL's `play/quest.c` is the dagger layer's and is not reused (`docs/ogfall-later.md` lists the decoupling). Classic first; where the original has a plain bug we do what it meant.

## Decisions on section 7

| # | What the original does | We do |
| --- | --- | --- |
| 7.1 | `quest_raise_event` returns nothing but `npc_talk` reads the value | Moot: our dispatch returns 1 when a quest took the event, else 0. |
| 7.2 | symbol hashes can collide, the first match wins in a fixed order | **Keep** (the order items, places, persons, foes, timers). |
| 7.3 | the Fighters Guild and temple non-member branches read a null membership | **Fix.** A non-member has rank 0 and no kind. |
| 7.4 | op 4 sets a flag before its null test; its "missing item" message reads an unset pointer | **Fix.** Tested before; the message names the item. |
| 7.5 | op 57 reads the live copy before testing | **Fix.** |
| 7.6 | a click on an unbound quest person crashes | **Fix.** Nothing happens. |
| 7.7 | with 32 quests in the logbook a new entry writes slot -1 | **Fix.** The oldest ended quest's slot is reused; with none ended the new quest runs and its entries are not written. |
| 7.8 | op 60 plays its sound on every run while its state is set | **Fix.** Once. |
| 7.9 | op 52 never clears, op 53 does: an AND stays true | **Keep.** |
| 7.10 | ops 13, 17, 37, 43, 51, 70 act on every run, only their message is once | **Keep.** |
| 7.11 | event 78 is never raised | **Keep** (only the faction-click path works). |
| 7.12 | op 19 counts only dungeons in the region's list, so a town or another region's place reveals the wrong location | **Fix.** The place bound to the symbol is revealed. |
| 7.13 | items and foes share the foe counter | **Keep.** |
| 7.14 | a place's fallback faction is overwritten by the building entry | **Fix.** The fallback holds unless the building has its own. |
| 7.15 | a timer with none of bits 0..2 never reads as expired | **Keep**, noted (no real quest is known to use it). |
| 7.16 | items are never "where is" topics | **Keep.** |
| 7.17 | a questor already in a quest makes the new one fail silently (TEXT.RSC 600) | **Keep.** |
| 7.18 | failure message 1003 is not shown | **Keep.** |
| 7.19 | the quest givers are re-rolled from the day (the same all day) | **Keep.** |
| 7.20 | op 1 and 28 on a person of type 65 match by faction | **Keep.** |
| 7.21 | the in-quest prompt answer is the arg index | **Keep.** |
| 7.22 | DAGGER.GRD hides quests whose fifth letter is X | **Keep** (the file is read if it is there). |

## Steps (the document's section 8 on OGFALL's layers)

The machine is data-driven: the quest files are read in place, never copied; nothing is hard-coded per quest. State lives in the pointer-free `OgState` (a quest table of states, timers and bindings by resource, the logbook, the global states, saved from the first step that can run).

1. **Load and run** a QBN: header, sections, arg encoding, COUNT.DAT, states as bytes, ops in order each frame and each passing minute (ops 51, 52, 53, 7, 6 first); QRC messages in the message box. Host-tested on a quest that ends at once.
2. **Start and offer**: `quest_start` (the `Y` prompt 1000/1001/1002), the quest list by guild, faction, rank and reputation, Get Quest in a hall (OGFALL's guild menu already has `OG_SVC_QUEST` and the temple quests); M0B00Y00 offered and accepted at the Fighters Guild.
3. **Text and the logbook**: the hash, quest symbol text, `%qdt` `%cn` and the macros, ops 23/24, the Log button's screen (the sheet has the button).
4. **Places and timers**: `quest_init_place`, op 19, timers 0..5 with ops 12/13, the state by hash.
5. **Foes and kills**: `quest_init_foe`, op 22 placing in a dungeon through OGFALL's own dungeon creatures, event 2, op 34.
6. **Persons and talk**: `quest_init_person`, the live copy, event 28 from the talk window (`play/talkwin.c`), quest topics, rumours.
7. **Rewards**: gold, items, op 4 with the box; M0B00Y00 plays end to end.
8. **Save and load** (schema bump): the quest tree, faces, global states; save mid-quest in a dungeon, load, finish.
9. **The rest by use**: items, the other events, topics and rumours, faces, hide/restore, child quests, building access, the S quests and the main story's global states, the Daedra and temple quests, noble quests, invitation letters.

Each step lands alone with its host test and a section in `docs/ogfall.md`.
