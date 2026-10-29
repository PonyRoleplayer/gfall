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
 */

/* Registers them once. */
void commands_setup(void);
/* The player they act on, or 0 out of play. */
void commands_player(Player *p);

#endif
