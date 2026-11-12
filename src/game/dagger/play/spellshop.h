#ifndef OGFALL_SPELLSHOP_H
#define OGFALL_SPELLSHOP_H

#include "game/dagger/play/spellbook.h"

/*
 * The Mages Guild's spell shop (guild_buy_spells, spellshop_build_list, spellshop_buy: src/lifted/guilds.c 145..250, src/hand/func_0006FDBE.c). Slice 10 of people and law.
 * The shop offers some of the standard spells (SPELLS.STD's, never one named with "!" or none): for each, a cost (the spell's magicka cost, held at 95 by halving) takes
 * from a chance of the hall's quality times 5 plus 30 (at least 5), and the spell is offered when a roll of 1 to 50 is under it; the list is made again, the whole run
 * of the table, until it holds one. The rolls are on the stream seeded with the person's number, so the same person offers the same spells; the stream is put back.
 * A spell costs four times its magicka cost in gold (half on the holiday 43), through the hall's price chain.
 */
#define OG_SPELLSHOP_MOST 96

/* The definitions' ids offered by a person of a hall of this quality; how many. 0 when the player has no spellbook to put one in. */
int og_spellshop_list(unsigned person, int quality, int *ids, int cap);
/* The gold a spell asks before the chain: four times its cost, halved on holiday 43. */
int og_spellshop_price(const OgSpellDef *spell, int holiday);
/* The spell into the spellbook; 1 when it is there (the book holds 128). */
int og_spellshop_add(int id);

#endif
