#include <stdio.h>
#include "check_ogshelf.h"
#include "check_util.h"
#include "game/dagger/play/shelf.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/shopdefs.h"

/* The groups an item belongs to, by what was stocked (OgObject.group). */
static int in_list(int group, const int *list, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (list[i] == group || (list[i] == 6 && group == 12) || (list[i] == 12 && group == 6)) {
            return 1;
        }
    }
    return 0;
}

static int count_items(df_u32 container)
{
    df_u32 ids[64];

    return og_inventory_rows(container, -1, ids, 64);
}

void check_ogshelf(void)
{
    int groups[OG_SHOP_GROUPS_MOST];
    df_u32 container, ids[64];
    int n, i, total, stocks, one_group, shelves;

    /* The tables: a general store (type 2) at variant 0 draws from 2, 6, 12; the width grows by variant. */
    n = og_shop_groups(0, 2, groups);
    check_expect(n == 3 && groups[0] == 2 && groups[1] == 6 && groups[2] == 12, "shelf: a type 2 shop's first list is not armour, men's and women's clothes");
    check_expect(og_shop_groups(2, 4, groups) == 12 && og_shop_groups(11, 4, groups) <= 5 && og_shop_groups(15, 4, groups) <= 11, "shelf: a list is the wrong width for its variant");
    check_expect(og_shop_groups(0, 21, groups) == 0 && og_shop_groups(0, -1, groups) == 0, "shelf: a type past 20 has a list");
    check_expect(og_shelf_model(418, 0) && og_shelf_model(410, 3) && !og_shelf_model(410, 5) && !og_shelf_model(100, 3),
                 "shelf: which models are shelves");

    /* Many shelves of a type 2 shop: every item is of the list's groups, one group a shelf, and more than one item sometimes. */
    og_random_seed(7);
    og_character()->female = 0;
    total = 0;
    stocks = 0;
    one_group = 1;
    shelves = 200;
    n = og_shop_groups(0, 2, groups);
    for (i = 0; i < shelves; i++) {
        int made, k, first;

        container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
        check_expect(container != 0, "shelf: no room for a shop container");
        if (!container) {
            return;
        }
        made = og_shelf_stock(container, 0, 12, 2, i);
        total += made;
        stocks += made > 1;
        k = og_inventory_rows(container, -1, ids, 64);
        check_expect(k == made, "shelf: stocked and counted items differ");
        first = k ? og_object(ids[0])->group : -1;
        for (k = 0; k < made; k++) {
            const OgObject *it;

            it = og_object(ids[k]);
            check_expect(it && in_list(it->group, groups, n), "shelf: an item is of a group the shop does not stock");
            check_expect(it && it->thing_key == (df_u32)i + 1u, "shelf: an item does not name its shelf");
            if (it && it->group != first && !((it->group == 6 || it->group == 12) && (first == 6 || first == 12))) {
                one_group = 0;
            }
        }
        for (k = 0; k < made; k++) {
            og_object_delete(ids[k]);
        }
        og_object_delete(container);
    }
    check_expect(one_group, "shelf: a shelf held more than one group");
    /* Chance halves from 100: after the first item 50% make a second, then 25%, ...: about 1.7 items a shelf. */
    check_expect(total > shelves && total < shelves * 3 && stocks > shelves / 4 && stocks < shelves * 3 / 4,
                 "shelf: the number of items a shelf is not the halving chance's");
    printf("shelf: %d shelves of a general store held %d items, %d with more than one\n", shelves, total, stocks);
    check_expect(count_items(OG_WORLD_ROOT) >= 0, "shelf: count");

    /* Clothes follow the player's sex. */
    og_character()->female = 1;
    container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    check_expect(container != 0, "shelf: no room for a shop container");
    for (i = 0; i < 12; i++) {
        og_shelf_stock(container, 0, 12, 0, i);
    }
    n = og_inventory_rows(container, -1, ids, 64);
    for (i = 0; i < n && i < 64; i++) {
        check_expect(og_object(ids[i]) && og_object(ids[i])->group == 12, "shelf: a woman's shop stocked men's clothes");
    }
    og_character()->female = 0;
}
