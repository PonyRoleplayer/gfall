#include "game/dagger/play/character/kit.h"
#include "game/dagger/tables/itemdefs.h"

/*
 * Weapon index, then the material byte, eighteen classes. Read from
 * FALL.EXE at D_0017BABC. The decomp adds one to the material byte
 * when it forces a metal; the ready sound does not need that metal.
 * Ready sounds come from the original weapon-index table.
 */
/* D_0017BABC is {weapon, material} for all 19 classes, custom included. */
static const unsigned char g_weapon[CLASSES][2] = {
    {3,1}, {6,0}, {6,0}, {3,1}, {11,0}, {3,0}, {3,0}, {1,1}, {6,0},
    {3,0}, {3,0}, {7,0}, {2,1}, {KIT_ARCHER,KIT_ARCHER}, {14,1}, {13,0}, {5,0}, {7,0}, {7,0}
};

static void gift(CharacterState *who, int group, int index, int material)
{
    int n;
    n = who->gifts;
    if (n >= KIT_GIFTS_MOST) { return; }
    who->gift[n].group = group; who->gift[n].index = index; who->gift[n].material = material;
    who->gifts++;
}

void kit_give(CharacterState *who)
{
    int klass;
    int weapon;

    if (!who) { return; }
    klass = who->klass;
    if (klass < 0 || klass >= CLASSES) { return; }
    gift(who, who->female ? ITEM_GROUP_WOMENS_CLOTHING : ITEM_GROUP_MENS_CLOTHING,
         who->female ? KIT_WOMENS_CLOTHES : KIT_MENS_CLOTHES, 0);
    gift(who, who->female ? ITEM_GROUP_WOMENS_CLOTHING : ITEM_GROUP_MENS_CLOTHING, KIT_SHOES, 0);
    weapon = g_weapon[klass][0];
    if (weapon == KIT_ARCHER) {
        gift(who, ITEM_GROUP_WEAPON, WEAPON_LONG_BOW, 0);
        gift(who, ITEM_GROUP_WEAPON, KIT_ARCHER_SWORD, 0);
        gift(who, ITEM_GROUP_WEAPON, WEAPON_ARROW, 0);
        /* Original index 18 is arrows; 17 is a long bow. */
        who->arrows += KIT_ARROWS;
        weapon = KIT_ARCHER_SWORD;
    } else if (weapon != KIT_NONE) {
        gift(who, ITEM_GROUP_WEAPON, weapon, g_weapon[klass][1]);
    } else { weapon = 0; }
    gift(who, ITEM_GROUP_MISC, MISC_SPELLBOOK, 0);
    who->weapon = weapon;
    who->weapon_sound = weapon_sound(weapon);
}
