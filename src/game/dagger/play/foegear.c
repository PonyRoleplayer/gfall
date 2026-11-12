#include "game/dagger/play/foegear.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/itemdefs.h"

#define FOE_SLOT 0x10000

static void make(df_u32 parent, int group, int low, int high, int exclude, int chance, int slot)
{
    df_u32 id;
    int index;
    if (random_range(1,100) > chance) { return; }
    do { index = random_range(low,high); } while (index == exclude);
    id = item_create(parent,group,index,-1);
    if (id) { object_edit(id)->flags |= FOE_SLOT | (slot << 8); }
}

void foe_equipment(int kind, df_u32 container, df_u32 *slots, FighterInfo *fighter)
{
    const ObjectInfo *item, *shield;
    ObjectInfo *pack;
    int tier, i, part, armor, equipped;
    static const int gear_slot[6] = {12,13,15,18,23,26};
    static const int armor_index[6] = {5,4,3,0,2,6};
    if (kind >= 128) { tier = 3; }
    else if (kind == 7 || kind == 21) { tier = 0; }
    else if (kind == 8 || kind == 12) { tier = 1; }
    else if (kind == 24) { tier = 2; }
    else { return; }
    pack = object_edit(container);
    if (!pack) { return; }
    if (!(pack->flags & 2)) {
        if (tier == 3) { tier = random_range(0,1); }
        make(container,3,tier ? 9 : 5,tier ? 14 : 7,-1,100,19);
        if (!tier) {
            make(container,2,7,8,-1,50,21);
            equipped = 0;
            for (i = 0; i < OBJECTS; i++) {
                item = &game_state()->object[i];
                if (item->id && item->parent == container && (item->flags & FOE_SLOT) &&
                    ((item->flags >> 8) & 31) == 21) { equipped = 1; }
            }
            if (!equipped) { make(container,3,0,3,2,50,21); }
        }
        for (i = 0; i < 6; i++) {
            make(container,2,armor_index[i],armor_index[i],-1,tier == 0 ? 50 : tier == 1 ? 75 : 90,gear_slot[i]);
        }
        pack->flags |= 2;
    }
    for (i = 0; i < 27; i++) { slots[i] = 0; }
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (item->id && item->parent == container && (item->flags & FOE_SLOT) && item->condition > 0) {
            part = (item->flags >> 8) & 31;
            if (part < 27) { slots[part] = item->id; }
        }
    }
    shield = object_at(slots[21]);
    for (part = 0; part < 7; part++) {
        armor = 100; item = object_at(slots[armor_slot(part)]);
        if (item && item->group == 2) {
            armor -= armor_base(item->armor_type);
            if (item->armor_type == 2) { armor -= material_hit(item->material); }
        }
        if (shield && shield->group == 2 && shield_covers(shield->index,part)) { armor -= (shield->index-6)*5; }
        fighter->armor[part] = armor > 50 ? 60 : armor < -100 ? -100 : armor;
    }
}
