#ifndef GFALL_SHELF_H
#define GFALL_SHELF_H

#include "engine/core/df.h"

/*
 * What a shop's shelves hold. Slice 2 of people and law (docs/game-people.md).
 * From the decomp: shop_generate_stock (src/lifted/equip.c 755), interior_stock_shelves
 * (src/lifted/engsupp.c) and building_load_interior (maplogic.c), which clears the shop's
 * container and stocks every shelf again at each entry: nothing is kept between visits.
 */

/* A shelf is the interior model 418, or 410 when its variant is a container (furniture_is_container). */
int shelf_model(int model_id, int variant);

/*
 * Stock one shelf into container: of a shop of building_type, a shelf variant, the shop's quality,
 * and the shelf's model number (kept on each item as its owner, thing_key). One group is picked
 * from the type's list; one item is made of it, then more, the chance of another halving from
 * 100 each time (an item, 50% another, 25% a third ...). Clothing is the player's sex's, a book's
 * rarity comes from the quality, and magic items, artifacts and the soul traps wait for their
 * systems. Returns how many items were made.
 */
int shelf_stock(df_u32 container, int variant, int quality, int building_type, int model_index);

/* A shelf's container and every item still on it, gone: what a new visit does to the last one's stock. */
void shelf_discard(df_u32 container);

#endif
