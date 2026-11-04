#ifndef DF_ATLAS_H
#define DF_ATLAS_H

#include "engine/core/df.h"

/*
 * Which of the map's locations the player knows of: what the travel map shows
 * and can be travelled to. Read docs/game.md (The wider world) before editing.
 *
 * A new game knows what classic's map shows from the start (worldmap.h's known
 * bit, 11199 of 15251 places). A place is learnt by standing on its pixel of the
 * world. The game's own block, kept across places; a save holds only the words
 * of bits that differ from a new game's.
 *
 * Plain C: hostcheck runs it.
 */

void atlas_new_game(void);
/* Location number k (worldmap_location_info). */
int atlas_known(int k);
void atlas_learn(int k);
/* For a save: the words that differ from a new game's, each a number and 32 bits. */
int atlas_changed_count(void);
void atlas_changed_at(int i, int *word, df_u32 *bits);
void atlas_put(int word, df_u32 bits);

#endif
