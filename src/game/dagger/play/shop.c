#include "game/dagger/play/shop.h"
#include "game/dagger/play/loot.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/tables/items.h"
#include "engine/core/mem.h"

/* See shop.h. */

/* The keep row's game bytes: the day (4), then the taken bits (8). */
#define BYTE_DAY 0
#define BYTE_TAKEN 4
/* ItemTemplates' rarity: body armour 3, shields and weapons 1. The whole table has these two for what the game has. */
#define RARITY_ARMOR 3
#define RARITY_OTHER 1

static int rarity_of(int template_id)
{
    return template_id >= ITEM_CUIRASS && template_id <= ITEM_BOOTS ? RARITY_ARMOR : RARITY_OTHER;
}

/* A small number from a bigger one, for the rolls: the same in, the same out. */
static df_u32 mix(df_u32 v)
{
    v = v * 1103515245u + 12345u;
    return v ^ (v >> 15);
}

static df_u32 read_u32(const df_u8 *b)
{
    return (df_u32)b[0] | ((df_u32)b[1] << 8) | ((df_u32)b[2] << 16) | ((df_u32)b[3] << 24);
}

static void write_u32(df_u8 *b, df_u32 v)
{
    b[0] = (df_u8)v;
    b[1] = (df_u8)(v >> 8);
    b[2] = (df_u8)(v >> 16);
    b[3] = (df_u8)(v >> 24);
}

/* Its row brought up to today: a new day takes nothing gone. */
static Keep *row_for_today(const Shop *shop, int day)
{
    Keep *row;

    row = keep_get(shop->key, KEEP_ENTITY);
    if (!row) {
        return 0;
    }
    if ((int)read_u32(row->game + BYTE_DAY) != day) {
        write_u32(row->game + BYTE_DAY, (df_u32)day);
        write_u32(row->game + BYTE_TAKEN, 0u);
        write_u32(row->game + BYTE_TAKEN + 4, 0u);
    }
    return row;
}

void shop_stock(const Shop *shop, int level, int day, Inventory *out, df_u8 *origin)
{
    const BuildingKind *kind;
    const Keep *row;
    RecTable *t;
    df_u32 seed;
    df_u32 taken[2];
    int thing;
    int i;

    inventory_clear(out);
    kind = building_kind(shop->type);
    if (!kind) {
        return;
    }
    row = row_for_today(shop, day);
    taken[0] = row ? read_u32(row->game + BYTE_TAKEN) : 0u;
    taken[1] = row ? read_u32(row->game + BYTE_TAKEN + 4) : 0u;
    seed = loot_seed() ^ shop->key.place ^ shop->key.thing ^ ((df_u32)day * 2654435761u);
    t = item_table();
    thing = 0;
    /* A general store always has a horse and a small cart (StockShopShelf): things 0 and 1, gone till tomorrow once bought. */
    if (shop->type == BUILDING_GENERAL) {
        static const int beasts[2] = { ITEM_HORSE, ITEM_CART };
        int b;

        for (b = 0; b < 2; b++) {
            Item made;

            if (item_make(&made, beasts[b], 0) && !(taken[0] & (1u << thing))) {
                int at;

                at = inventory_add(out, &made);
                if (at >= 0) {
                    origin[at] = (df_u8)thing;
                }
            }
            thing++;
        }
    }
    for (i = 0; i < t->count && thing < SHOP_STOCK_MAX; i++) {
        const ItemTemplate *it;
        int mod;
        int rarity;
        int chance;

        it = (const ItemTemplate *)(t->rows + (df_u32)i * t->schema->row_size);
        if (weapon_template(it->id) || it->id == ITEM_ARROW) {
            mod = kind->stock_weapons;
        } else if (armor_template(it->id)) {
            mod = kind->stock_armor;
        } else {
            continue;
        }
        rarity = rarity_of(it->id);
        if (!mod || rarity > shop->quality) {
            continue;
        }
        /* The chance number as StockShopShelf turns it into percent. */
        chance = mod * 5 * (21 - rarity) / 100;
        seed = mix(seed);
        if ((int)((seed >> 8) % 100u) >= chance) {
            continue;
        }
        seed = mix(seed);
        {
            Item made;


            if (loot_make_for_level(&made, it->id, level, seed)) {
                if (!(taken[thing >> 5] & (1u << (thing & 31)))) {
                    int at;

                    at = inventory_add(out, &made);
                    if (at >= 0) {
                        origin[at] = (df_u8)thing;
                    }
                }
                thing++;
            }
        }
    }
}

void shop_taken(const Shop *shop, int origin)
{
    Keep *row;
    df_u32 bits;
    int at;

    row = keep_find(shop->key);
    if (!row || origin < 0 || origin >= SHOP_STOCK_MAX) {
        return;
    }
    at = BYTE_TAKEN + (origin >> 5) * 4;
    bits = read_u32(row->game + at) | (1u << (origin & 31));
    write_u32(row->game + at, bits);
}

int shop_buys(const Shop *shop, const Item *it)
{
    int weapon;
    int armor;

    weapon = weapon_template(it->template_id) != 0 || it->template_id == ITEM_ARROW;
    armor = armor_template(it->template_id) != 0;
    switch (shop->type) {
    case BUILDING_ARMORER:
    case BUILDING_WEAPONS:
    case BUILDING_PAWN:
        return weapon || armor;
    case BUILDING_GENERAL:
        /* ItemTypesAccepted's general store: weapons, and transport. */
        return weapon || it->template_id == ITEM_HORSE || it->template_id == ITEM_CART;
    default:
        return 0;
    }
}

int shop_cost(const Shop *shop, const Item *it)
{
    int cost;

    cost = rules_cost((int)it->value, shop->quality, shop->prices) * (it->count > 0 ? it->count : 1);
    /*
     * DFU's trade window halves a line's cost on the Merchants' Festival, and a
     * weapon's on the Warriors Festival, for a shop with no guild: every shop yet.
     */
    if ((shop->holiday & HOLIDAY_SHOPS_HALF)
        || ((shop->holiday & HOLIDAY_WEAPONS_HALF) && (weapon_template(it->template_id) || it->template_id == ITEM_ARROW))) {
        cost /= 2;
    }
    return cost;
}

int shop_trade_price(const Shop *shop, int cost, const Character *c, int selling)
{
    return rules_trade_price(cost, shop->quality, c->skill[SKILL_MERCANTILE], c->attribute[ATTR_PERSONALITY], selling);
}

int shop_repair_cost(const Shop *shop, const Item *it)
{
    int cost;

    cost = rules_repair_cost((int)it->value, shop->quality, it->condition, it->condition_max, shop->prices);
    /* A guild member's (10 - rank) tenths, as DFU's ((10 - rank) << 8) / 10 x price >> 8. */
    if (shop->repair_rank > 0) {
        cost = (((10 - (shop->repair_rank - 1)) << 8) / 10 * cost) >> 8;
    }
    return cost;
}

int shop_seen_chance(const Shop *shop, const Character *c, float weight, int count)
{
    return rules_shoplift_chance(c->skill[SKILL_PICKPOCKET], shop->quality, (int)weight + count);
}
