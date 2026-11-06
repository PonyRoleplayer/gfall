#ifndef DF_COMMANDS_H
#define DF_COMMANDS_H

#include "game/dagger/play/player.h"
#include "game/dagger/play/spells.h"

/*
 * The game's console commands (docs/game.md, Console), on the engine's
 * console (cmd_add). They act on the player in play; with none, they say
 * so.
 *
 *   additem [weapon] [material]  a new weapon in the pack; bare, the
 *                                weapons and materials there are.
 *                                Iron when no material is named.
 *   items                        the pack, the one in hand marked.
 *   equip n|none                 item n in hand, or bare hands.
 *   goto name|code|region index  to a town's streets, from anywhere, as the
 *                                console closes. A whole name goes to the
 *                                largest of that name; a part with several
 *                                fits lists them with their codes. A code is
 *                                the place's location number, as LEVELFALL's
 *                                list shows it.
 *   where                        the town and region here, and its code.
 *   foes                         the creatures awake: what each is
 *                                doing and how far off.
 *   tgm [on|off]                 god mode: nothing takes the player's
 *                                health (player_hurt); tgm alone flips
 *                                it, as Bethesda's consoles do. Off with
 *                                each new game.
 *   spells                       the spellbook: each spell with its target,
 *                                element and cost in spell points.
 *   learn n|name                 a standard spell into the book, by its number
 *                                in SPELLS.STD or a part of its name (a part that
 *                                fits several lists them).
 *   forget n                     spell n of spells out of the book.
 *   ready n|last                 spell n made ready: the attack button casts it.
 *   effects                      what lasts on the player and for how many rounds.
 *   tai [on|off]                 the creatures' AI (foes.c): off, no
 *                                one wakes, moves or strikes; tai alone
 *                                flips it. Kept across games.
 */

/* Registers them once. */
void commands_setup(void);
/* The player they act on, or 0 out of play. */
void commands_player(Player *p);
/* The spellbook they act on (spells.h), kept by the game for the process. */
void commands_book(Spellbook *b);

#endif
