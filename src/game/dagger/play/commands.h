#ifndef DF_COMMANDS_H
#define DF_COMMANDS_H

#include "game/dagger/play/player.h"

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
 *   foes                         the creatures awake: what each is
 *                                doing and how far off.
 *   tgm [on|off]                 god mode: nothing takes the player's
 *                                health (player_hurt); tgm alone flips
 *                                it, as Bethesda's consoles do. Off with
 *                                each new game.
 *   tai [on|off]                 the creatures' AI (foes.c): off, no
 *                                one wakes, moves or strikes; tai alone
 *                                flips it. Kept across games.
 */

/* Registers them once. */
void commands_setup(void);
/* The player they act on, or 0 out of play. */
void commands_player(Player *p);

#endif
