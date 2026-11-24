#include "game/dagger/play/people/shelf.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/tables/shopdefs.h"
#include "game/dagger/play/combat/combat.h"

/* See shelf.h. */

/* The furniture that holds stock: model 418, and model 410 when its variant is a container (click.c:968). */
#define MODEL_SHELF 418
#define MODEL_FURNITURE 410
/* The first item is always made; then the chance halves, and rand() % 100 > chance ends it. */
#define SHELF_FIRST_CHANCE 100
/* A book's rarity is (quality + BOOK_RARITY_ADD) / BOOK_RARITY_DIV (equip.c:804). */
#define BOOK_RARITY_ADD 3
#define BOOK_RARITY_DIV 5

int shelf_model(int model_id, int variant)
{
    if (model_id == MODEL_SHELF) {
        return 1;
    }
    if (model_id != MODEL_FURNITURE) {
        return 0;
    }
    /* furniture_is_container */
    return variant == 3 || variant == 4 || variant == 7 || variant == 8 || variant == 50 || variant == 51 ||
           (variant >= 32 && variant <= 38);
    /* The original passes the variant plus MODEL_FURNITURE * 128 to this list, which stops at 51, so there no model 410 ever stocks; ours stocks its containers (docs/game-later.md). */
}

/* rand_range(0, n - 1) the original's way: the Watcom stream, through the session. */
static int pick(int n)
{
    return random_range(0, n - 1);
}

static int stock(df_u32 container, int variant, int quality, int building_type, int model_index, int shop)
{
    int groups[SHOP_GROUPS_MOST];
    int n, group, chance, made;

    n = shop_groups(variant, building_type, groups);
    if (n < 1) {
        return 0;
    }
    group = groups[pick(n)];
    chance = SHELF_FIRST_CHANCE;
    made = 0;
    for (;;) {
        df_u32 id;
        int kinds;

        id = 0;
        if (group == ITEM_GROUP_MENS_CLOTHING || group == ITEM_GROUP_WOMENS_CLOTHING) {
            /* Clothes for the player's sex (the original tests the character's flag bit 0). */
            group = character_state()->female ? ITEM_GROUP_WOMENS_CLOTHING : ITEM_GROUP_MENS_CLOTHING;
        }
        if (group == ITEM_GROUP_BOOK) {
            /* item_make(7, (quality + BOOK_RARITY_ADD) / BOOK_RARITY_DIV): a book of that rarity. */
            id = item_create(container, ITEM_GROUP_BOOK, (quality + BOOK_RARITY_ADD) / BOOK_RARITY_DIV, -1);
        } else if (group != ITEM_GROUP_MAGIC_ITEM && group != ITEM_GROUP_ARTIFACT && group != ITEM_GROUP_MAP) {
            kinds = item_kinds(group);
            if (shop && group == ITEM_GROUP_WEAPON) { kinds = WEAPON_ARROW; }
            if (kinds > 0) {
                id = item_create(container, group, pick(kinds), -1);
            }
        }
        if (id) {
            object_edit(id)->thing_key = (df_u32)model_index + 1u;
            made++;
        }
        chance >>= 1;
        /* rand() % 100 > chance ends it. */
        if (random_range(0, RAND_MAX_WATCOM) % PERCENT > chance) {
            break;
        }
    }
    return made;
}

int shelf_stock(df_u32 container, int variant, int quality, int building_type, int model_index)
{
    return stock(container, variant, quality, building_type, model_index, 0);
}

int shelf_stock_shop(df_u32 container, int variant, int quality, int building_type, int model_index)
{
    /* shelf_stock_items rolls each weapon template for these buildings,
     * regardless of the shelf model. Use that roll for ammunition. */
    static const df_u8 weapon_chance[BUILDING_TYPES] = {
        [BUILDING_ARMORER] = 20, [BUILDING_GENERAL] = 20, [BUILDING_PAWN] = 10, [BUILDING_WEAPONS] = 70
    };
    int rarity = item_rarity(ITEM_GROUP_WEAPON, WEAPON_ARROW);
    int chance = building_type >= 0 && building_type < BUILDING_TYPES ? weapon_chance[building_type] : 0;
    int made = stock(container, variant, quality, building_type, model_index, 1);

    chance = (21 - rarity) * 5 * chance / 100;
    if (chance && rarity <= quality && random_range(1, PERCENT) <= chance) {
        df_u32 id = item_create(container, ITEM_GROUP_WEAPON, WEAPON_ARROW, 0);

        if (id) {
            object_edit(id)->thing_key = (df_u32)model_index + 1u;
            made++;
        }
    }
    return made;
}

void shelf_discard(df_u32 container)
{
    df_u32 ids[OBJECTS];
    int n, i;

    if (!object_at(container)) {
        return;
    }
    n = inventory_rows(container, -1, ids, OBJECTS);
    for (i = 0; i < n && i < OBJECTS; i++) {
        object_delete(ids[i]);
    }
    object_delete(container);
}
