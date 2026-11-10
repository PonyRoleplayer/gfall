#ifndef OGFALL_SHOPDEFS_H
#define OGFALL_SHOPDEFS_H

/* Generated tables from FALL.EXE (tools/ogfall-data.py); read src/lifted/equip.c shop_generate_stock. */

#define OG_SHOP_TYPES 21
#define OG_SHOP_GROUPS_MOST 12

/*
 * The item groups a shop of building_type stocks a shelf of this variant from, in
 * the original's order, up to its first 0: 3, 12, 12, 5 or 11 entries wide by the
 * variant (0..1, 2..3, 4..10, 11..14, 15 and up). Returns how many; 0 for a type
 * with no list (21 and over, or a row that is empty).
 */
int og_shop_groups(int variant, int building_type, int *groups);

#endif
