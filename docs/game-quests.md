# GFALL: the quest machine (plan and decisions)

The original plan began on 2026-11-11 and follows the decompilation's
`docs/quest-machine.md` and `docs/quests-math.md`. The quest machine reads QBN
and QRC records from ARENA2 through the existing format readers. Section numbers
in the design notes refer to the decompilation's documents.

## Implementation layout

The public interface is `src/game/dagger/play/quests/quest.h`. The app still
supplies world and text callbacks through that interface.

- `quest.c` owns quest slots, selection, offers, startup, and teardown.
- `binding.c` resolves questors, people, and places and initializes a quest's resources.
- `resources.c` handles quest items, foes, marker placement, and building access at runtime.
- `machine.c` runs opcodes in file order, updates timers, and owns the state and operand rules.
- `events.c` dispatches talk, item, spell, and combat events through those operand rules.
- `journal.c` expands symbols and formats text, logbook entries, rumours, topics, and faces.
- `persist.c` reads and writes the existing quest save fields explicitly in wire order.

The modules share runtime storage and helper declarations through
`internal.h`. The save codec serializes fields individually, independent of the
C struct layout. Quest tests in `tests/check_quest.c` exercise dispatch,
markers, and save round trips, including rejection of truncated data.

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

## Steps (the document's section 8 on GFALL's layers)

The machine is data-driven: the quest files are read in place, never copied; nothing is hard-coded per quest. State lives in the pointer-free `GameState` (a quest table of states, timers and bindings by resource, the logbook, the global states, saved from the first step that can run).

1. **Load and run** a QBN: header, sections, arg encoding, COUNT.DAT, states as bytes, ops in order each frame and each passing minute (ops 51, 52, 53, 7, 6 first); QRC messages in the message box. Host-tested on a quest that ends at once.
2. **Start and offer**: `quest_start` (the `Y` prompt 1000/1001/1002), the quest list by guild, faction, rank and reputation, Get Quest in a hall (GFALL's guild menu already has `SVC_QUEST` and the temple quests); M0B00Y00 offered and accepted at the Fighters Guild. Long parchment offers page through the full text, with Yes and No on the last page.
3. **Text and the logbook**: the hash, quest symbol text, `%qdt` `%cn` and the macros, ops 23/24, the Log button's screen (the sheet has the button). Quest text and dialogue resolve the player's name, original race and honorific from the current character. Ending a quest releases its journal slot before the id can be reused. When an older save pairs an active quest with another quest's filename, entries backed by its recorded log writes use the active quest's text.
4. **Places and timers**: `quest_init_place`, op 19, timers 0..5 with ops 12/13, the state by hash. Building destinations are drawn from MAPPITEM's interior quest sites. The text uses the selected building's in-game name, and loading an older save repairs local building bindings stored as only a town.
5. **Foes and kills**: `quest_init_foe`, op 22 placing in dungeons or the bound building interior, event 2, op 34. The Fighters Guild spider quest exercises the named destination and the monster's placement.
   Timed spawns (op 9) gated by a building entry check also reserve their sprites when the destination loads. They use the building's entrance markers for placement, with a quest marker as a fallback. A failed placement leaves the wave available for the next timed attempt. The Fighters Guild bat infestation has seven possible waves and needs four kills. The script can send the remaining waves while the job is active.
   Ending a quest removes its creatures, corpses and their loot rows from the loaded scene and the saved world. Loading an older save clears those rows when their quest has already ended.
6. **Persons and talk**: `quest_init_person`, the live copy, event 28 from the talk window (`play/people/talkwin.c`), quest topics, rumours.
7. **Rewards**: op 4 shows the quest's completion parchment (message 1004) and awards gold, items and reputation. The Fighters Guild bat job shows its own reward text when turned in.
8. **Save and load** (schema bump): the quest tree, faces, global states; save mid-quest in a dungeon, load, finish.
9. **The rest by use**: items, the other events, topics and rumours, faces, hide/restore, child quests, building access, the S quests and the main story's global states, the Daedra and temple quests, noble quests, invitation letters.

Each step lands alone with its host test and a section in `docs/game.md`.
