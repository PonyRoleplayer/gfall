#include "game/dagger/play/wagon.h"
#include "game/dagger/tables/items.h"

/* See wagon.h. */

static Inventory g_wagon;
static int g_reachable;

Inventory *wagon_contents(void)
{
    return &g_wagon;
}

void wagon_new_game(void)
{
    inventory_clear(&g_wagon);
    g_reachable = 0;
}

void wagon_set_reachable(int reachable)
{
    g_reachable = reachable;
}

int wagon_reachable(void)
{
    return g_reachable;
}

int wagon_fits(const Item *it)
{
    return inventory_weight(&g_wagon) + it->weight * (float)(it->count > 0 ? it->count : 1) <= WAGON_KG;
}

int wagon_refuses(const Item *it)
{
    return it->template_id == ITEM_HORSE || it->template_id == ITEM_CART;
}
