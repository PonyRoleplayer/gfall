#include "game/ogfall/tables/shopdefs.h"
#include "engine/core/df.h"
#include "game/ogfall/tables/shopdata.h"

int og_shop_groups(int variant, int building_type, int *groups)
{
    const df_u8 *row;
    int width, n;

    if (building_type < 0 || building_type >= OG_SHOP_TYPES) {
        return 0;
    }
    if (variant < 2) {
        row = g_shop_v0[building_type]; width = 3;
    } else if (variant < 4) {
        row = g_shop_v2[building_type]; width = 12;
    } else if (variant < 11) {
        row = g_shop_v4[building_type]; width = 12;
    } else if (variant < 15) {
        row = g_shop_v11[building_type]; width = 5;
    } else {
        row = g_shop_v15[building_type]; width = 11;
    }
    for (n = 0; n < width && row[n]; n++) {
        groups[n] = row[n];
    }
    return n;
}
