#include "game/ogfall/play/shelf.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/tables/shopdefs.h"

/* See shelf.h. */

int og_shelf_model(int model_id, int variant)
{
    if (model_id == 418) {
        return 1;
    }
    if (model_id != 410) {
        return 0;
    }
    /* furniture_is_container */
    return variant == 3 || variant == 4 || variant == 7 || variant == 8 || variant == 50 || variant == 51 ||
           (variant >= 32 && variant <= 38);
}

/* rand_range(0, n - 1) the original's way: the Watcom stream, through the session. */
static int pick(int n)
{
    return og_random_range(0, n - 1);
}

int og_shelf_stock(df_u32 container, int variant, int quality, int building_type, int model_index)
{
    int groups[OG_SHOP_GROUPS_MOST];
    int n, group, chance, made;

    n = og_shop_groups(variant, building_type, groups);
    if (n < 1) {
        return 0;
    }
    group = groups[pick(n)];
    chance = 100;
    made = 0;
    for (;;) {
        df_u32 id;
        int kinds;

        id = 0;
        if (group == 6 || group == 12) {
            /* Clothes for the player's sex (the original tests the character's flag bit 0). */
            group = og_character()->female ? 12 : 6;
        }
        if (group == 7) {
            /* item_make(7, (quality + 3) / 5): a book of that rarity. */
            id = og_item_create(container, 7, (quality + 3) / 5, -1);
        } else if (group != 4 && group != 5 && group != 11) {
            kinds = og_item_kinds(group);
            if (kinds > 0) {
                id = og_item_create(container, group, pick(kinds), -1);
            }
        }
        if (id) {
            og_object_edit(id)->thing_key = (df_u32)model_index + 1u;
            made++;
        }
        chance >>= 1;
        /* rand() % 100 > chance ends it. */
        if (og_random_range(0, 32767) % 100 > chance) {
            break;
        }
    }
    return made;
}
