#ifndef DF_SPELLS_H
#define DF_SPELLS_H

#include "engine/core/df.h"

/*
 * SPELLS.STD: classic's 89 standard spells, the ones a guild sells and a new
 * character starts with, 89 bytes each, as Daggerfall Unity's
 * DaggerfallSpellReader reads them:
 *
 *   0  three effects, each (group, subgroup) as two bytes, 255 for none
 *   6  element: 0 fire, 1 cold, 2 poison, 3 shock, 4 magic
 *   7  target: 0 the caster, 1 touch, 2 one at range, 3 an area round the
 *      caster, 4 an area at range
 *   8  the price classic asks (u16); not the spell point cost, which the
 *      effects work out (play/spells.h)
 *   10 four bytes unused
 *   14 each effect's duration: base, plus, per level (3 bytes an effect)
 *   23 each effect's chance, the same
 *   32 each effect's magnitude: base low, base high, plus low, plus high,
 *      per level (5 bytes an effect)
 *   47 the name (25 bytes, NUL padded)
 *   72 the icon (ICON00I0.IMG's number, 0 to 68)
 *   73 the spell's number: what a guild's list and a class's starting spells name
 *   74 fifteen bytes unused
 *
 * A name that starts with "!" is an ingredient's poison or potion effect, not
 * a spell to cast.
 */

#define STDSPELL_BYTES 89
#define STDSPELL_COUNT 89
#define STDSPELL_NAME 26
#define STDSPELL_EFFECTS 3

typedef struct StdSpell {
    df_u8 group[STDSPELL_EFFECTS];
    df_u8 sub[STDSPELL_EFFECTS];
    df_u8 element;
    df_u8 target;
    df_u16 price;
    df_u8 duration[STDSPELL_EFFECTS][3];
    df_u8 chance[STDSPELL_EFFECTS][3];
    df_u8 magnitude[STDSPELL_EFFECTS][5];
    char name[STDSPELL_NAME];
    df_u8 icon;
    df_u8 number;
} StdSpell;

/* The records of a whole file into out (cap of them): how many were read. */
int stdspell_read(const df_u8 *bytes, df_u32 size, StdSpell *out, int cap);

#endif
