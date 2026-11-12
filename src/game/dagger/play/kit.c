#include "game/dagger/play/kit.h"
#include "game/dagger/tables/itemdefs.h"

/*
 * Weapon index, then the material byte, eighteen classes. Read from
 * FALL.EXE at D_0017BABC. The decomp adds one to the material byte
 * when it forces a metal; the ready sound does not need that metal.
 * Ready sounds come from the original weapon-index table.
 */
/* D_0017BABC is {weapon, material} for all 19 classes, custom included. */
static const unsigned char g_weapon[19][2] = {
    {3,1}, {6,0}, {6,0}, {3,1}, {11,0}, {3,0}, {3,0}, {1,1}, {6,0},
    {3,0}, {3,0}, {7,0}, {2,1}, {254,254}, {14,1}, {13,0}, {5,0}, {7,0}, {7,0}
};

static void gift(OgCharacter *who, int group, int index, int material)
{
    int n;
    n = who->gifts;
    if (n >= 32) { return; }
    who->gift[n].group = group; who->gift[n].index = index; who->gift[n].material = material;
    who->gifts++;
}

void og_kit_give(OgCharacter *who)
{
    int klass;
    int weapon;

    if (!who) { return; }
    klass = who->klass;
    if (klass < 0 || klass >= 19) { return; }
    gift(who, who->female ? 12 : 6, who->female ? 8 : 10, 0);
    gift(who, who->female ? 12 : 6, 24, 0);
    weapon = g_weapon[klass][0];
    if (weapon == 254) {
        gift(who, 3, 17, 0);
        gift(who, 3, 14, 0);
        gift(who, 3, 18, 0);
        /* Original index 18 is arrows; 17 is a long bow. */
        who->arrows += 24;
        weapon = 14;
    } else if (weapon != 255) {
        gift(who, 3, weapon, g_weapon[klass][1]);
    } else { weapon = 0; }
    gift(who, 27, 0, 0);
    who->weapon = weapon;
    who->weapon_sound = og_weapon_sound(weapon);
}
