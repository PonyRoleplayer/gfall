#ifndef GFALL_SHOPDEFS_H
#define GFALL_SHOPDEFS_H

/* Generated tables from FALL.EXE (tools/gfall-data.py); read src/lifted/equip.c shop_generate_stock. */

#define SHOP_TYPES 21
#define SHOP_GROUPS_MOST 12

/*
 * The item groups a shop of building_type stocks a shelf of this variant from, in
 * the original's order, up to its first 0: 3, 12, 12, 5 or 11 entries wide by the
 * variant (0..1, 2..3, 4..10, 11..14, 15 and up). Returns how many; 0 for a type
 * with no list (21 and over, or a row that is empty).
 */
int shop_groups(int variant, int building_type, int *groups);

#endif
