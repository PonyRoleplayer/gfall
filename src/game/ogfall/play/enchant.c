#include "game/ogfall/play/enchant.h"
#include "game/ogfall/play/itemmaker.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/state.h"

/* See enchant.h. */

/* Whether an item worn in a slot other than skip holds Absorbs spells (a worn item taking it off must not clear what another still gives). */
static int others_absorb(int skip)
{
    int slot, k, type, param;

    for (slot = 0; slot < 27; slot++) {
        if (slot == skip || !og_state()->equipped[slot]) { continue; }
        for (k = 0; og_item_enchantment(og_state()->equipped[slot], k, &type, &param); k++) { if (type == 9) { return 1; } }
    }
    return 0;
}

void og_enchant_equip(df_u32 item, int slot)
{
    int k, type, param;

    for (k = 0; og_item_enchantment(item, k, &type, &param); k++) {
        if (type == 1 && param >= 0) { og_magic_hold(param, slot); }
        else if (type == 9) { og_character()->conditions |= 0x200; }
        else if (type == 10 && param >= 0 && param < OG_SKILLS) { og_character()->skill[param] += 15; }
    }
}

void og_enchant_unequip(df_u32 item, int slot)
{
    int k, type, param;

    for (k = 0; og_item_enchantment(item, k, &type, &param); k++) {
        if (type == 1 && param >= 0) { og_magic_release(param, slot); }
        else if (type == 9 && !others_absorb(slot)) { og_character()->conditions &= ~0x200; }
        else if (type == 10 && param >= 0 && param < OG_SKILLS) { og_character()->skill[param] -= 15; }
    }
}
