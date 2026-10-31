#ifndef DATFALL_MADE_H
#define DATFALL_MADE_H

#include "view.h"

/*
 * A weapon made in a material, from the merged ITEM, WEAP and MATL
 * rows (docs/game.md, "Weapons and the pack"). The sums are the game's:
 * value × 3 × value_q; weight in whole quarter-kilograms × weight_q / 4,
 * half rounded to even; condition × condition_q / 4. Done here so the
 * app does not include the play layer.
 */

typedef struct MadeItem {
    int quarters;
    int value;
    int condition;
    int cif;
} MadeItem;

/* The weapon at this WEAP row index: its id and the ITEM name. 0 past the end. */
int made_weapon_at(const DatFile *f, int index, int *id, char *name, int namecap);
/* The material at this MATL row index. 0 past the end. */
int made_material_at(const DatFile *f, int index, int *id, char *name, int namecap);
/* WEAPON##.CIF's number, or -1. */
int made_cif(const DatFile *f, int weapon_id);
/* 0 when either record is missing. */
int made_of(const DatFile *f, int weapon_id, int material_id, MadeItem *out);

/*
 * An armour piece. The sums are item_make's, with the material's
 * value_mul (3 metal, 1 leather, 2 chain) instead of a weapon's 3.
 * item_picture: a man's archives start at 249 and a woman's at 245.
 * Breton and the other humans add body shape 2. The record is the
 * template's player_record plus the variant.
 */
int made_armour_at(const DatFile *f, int index, int *id, char *name, int namecap);
int made_armour(const DatFile *f, int armour_id, int material_id, MadeItem *out);
/* The variant range for this material, both ends included. 0 when missing. */
int made_variant(const DatFile *f, int armour_id, int material_id, int *low, int *high);
/* man is 1 for archive 249, 0 for 245. shape 2 is a human. */
int made_picture(const DatFile *f, int armour_id, int material_id, int variant, int man, int shape,
                 int *archive, int *record);

#endif
