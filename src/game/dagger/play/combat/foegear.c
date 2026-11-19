#include "game/dagger/play/combat/foegear.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/character/ailments.h"

#define FOE_SLOT 0x10000
/* monster_init_items' gear tiers: light, medium (a weapon 9..14), heavy. A class enemy is 0 or 1 by a coin. */
enum { FOE_TIER_LIGHT = 0, FOE_TIER_MEDIUM = 1, FOE_TIER_HEAVY = 2, FOE_TIER_CLASS = 3 };
/* monster_give_equipment (moninit.c:196..234): a weapon always (WEAPON_LIGHT_* for the light tier, WEAPON_HEAVY_* otherwise), a light-tier shield SHIELD_* half the time, else half the time a weapon 0..3 that is not 2; each armour piece at FOE_ARMOR_CHANCE by tier. */
#define FOE_WEAPON_CHANCE 100
#define WEAPON_LIGHT_FIRST 5
#define WEAPON_LIGHT_LAST 7
#define WEAPON_HEAVY_FIRST 9
#define WEAPON_HEAVY_LAST 14
#define FOE_SHIELD_FIRST ARMOR_SHIELD_FIRST
#define FOE_SHIELD_LAST 8
#define FOE_SHIELD_CHANCE 50
#define FOE_SIDEARM_LAST 3
#define FOE_SIDEARM_EXCLUDE 2
#define FOE_SIDEARM_CHANCE 50
#define SHIELD_ARMOR_STEP 5
#define FOE_ARMOR_BARE_OVER 50
#define FOE_ARMOR_BARE 60

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
    int tier, i, part, armor, equipped, chance;
    static const int gear_slot[6] = {EQUIP_HEAD,EQUIP_RIGHT_PAULDRON,EQUIP_LEFT_PAULDRON,EQUIP_CUIRASS,EQUIP_GREAVES,EQUIP_BOOTS};
    static const int armor_index[6] = {ARMOR_HELM,ARMOR_RIGHT_PAULDRON,ARMOR_LEFT_PAULDRON,ARMOR_CUIRASS,ARMOR_GREAVES,ARMOR_BOOTS};
    /* The chance of each armour piece, by tier. */
    static const int g_armor_chance[3] = {50,75,90};
    if (ENEMY_IS_CLASS(kind)) { tier = FOE_TIER_CLASS; }
    else if (kind == ENEMY_ORC || kind == ENEMY_ORC_SHAMAN) { tier = FOE_TIER_LIGHT; }
    else if (kind == ENEMY_CENTAUR || kind == ENEMY_ORC_SERGEANT) { tier = FOE_TIER_MEDIUM; }
    else if (kind == ENEMY_ORC_WARLORD) { tier = FOE_TIER_HEAVY; }
    else { return; }
    pack = object_edit(container);
    if (!pack) { return; }
    if (!(pack->flags & 2)) {
        if (tier == FOE_TIER_CLASS) { tier = random_range(0,1); }
        make(container,ITEM_GROUP_WEAPON,tier ? WEAPON_HEAVY_FIRST : WEAPON_LIGHT_FIRST,tier ? WEAPON_HEAVY_LAST : WEAPON_LIGHT_LAST,-1,FOE_WEAPON_CHANCE,EQUIP_RIGHT_HAND);
        if (tier == FOE_TIER_LIGHT) {
            make(container,ITEM_GROUP_ARMOR,FOE_SHIELD_FIRST,FOE_SHIELD_LAST,-1,FOE_SHIELD_CHANCE,EQUIP_LEFT_HAND);
            equipped = 0;
            for (i = 0; i < OBJECTS; i++) {
                item = &game_state()->object[i];
                if (item->id && item->parent == container && (item->flags & FOE_SLOT) &&
                    ((item->flags >> 8) & 31) == EQUIP_LEFT_HAND) { equipped = 1; }
            }
            if (!equipped) { make(container,ITEM_GROUP_WEAPON,0,FOE_SIDEARM_LAST,FOE_SIDEARM_EXCLUDE,FOE_SIDEARM_CHANCE,EQUIP_LEFT_HAND); }
        }
        for (i = 0; i < 6; i++) {
            make(container,ITEM_GROUP_ARMOR,armor_index[i],armor_index[i],-1,g_armor_chance[tier],gear_slot[i]);
        }
        /* The poison (only once the player is past level 1, and only on the gear made now). */
        if (character_state()->level >= POISON_WEAPON_LEVEL) {
            chance = 0;
            if (ENEMY_IS_CLASS(kind)) { chance = kind == ENEMY_ASSASSIN ? POISON_WEAPON_ASSASSIN : POISON_WEAPON_OTHER; }
            else if (kind == ENEMY_ORC || kind == ENEMY_CENTAUR || kind == ENEMY_ORC_SERGEANT) { chance = POISON_WEAPON_OTHER; }
            if (chance && random_range(1,100) < chance) {
                for (i = 0; i < OBJECTS; i++) {
                    ObjectInfo *weapon = object_edit(game_state()->object[i].id);

                    if (weapon && weapon->parent == container && (weapon->flags & FOE_SLOT) && ((weapon->flags >> 8) & 31) == EQUIP_RIGHT_HAND) {
                        weapon->flags |= WEAPON_POISONED | ((df_u32)random_range(0,POISON_WEAPON_KINDS-1) << WEAPON_POISON_SHIFT);
                        break;
                    }
                }
            }
        }
        pack->flags |= 2;
    }
    for (i = 0; i < EQUIP_SLOTS; i++) { slots[i] = 0; }
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (item->id && item->parent == container && (item->flags & FOE_SLOT) && item->condition > 0) {
            part = (item->flags >> 8) & 31;
            if (part < EQUIP_SLOTS) { slots[part] = item->id; }
        }
    }
    shield = object_at(slots[EQUIP_LEFT_HAND]);
    for (part = 0; part < BODY_PARTS; part++) {
        armor = ARMOR_BASE; item = object_at(slots[armor_slot(part)]);
        if (item && item->group == 2) {
            armor -= armor_base(item->armor_type);
            if (item->armor_type == ARMOR_PLATE) { armor -= material_hit(item->material); }
        }
        if (shield && shield->group == 2 && shield_covers(shield->index,part)) { armor -= (shield->index-(ARMOR_SHIELD_FIRST-1))*SHIELD_ARMOR_STEP; }
        fighter->armor[part] = armor > FOE_ARMOR_BARE_OVER ? FOE_ARMOR_BARE : armor < -ARMOR_LIMIT ? -ARMOR_LIMIT : armor;
    }
}

int weapon_poison(df_u32 weapon)
{
    const ObjectInfo *item = object_at(weapon);

    return item && (item->flags & WEAPON_POISONED) ? POISON_FIRST + (int)((item->flags >> WEAPON_POISON_SHIFT) & WEAPON_POISON_MASK) : 0;
}

void weapon_poison_spend(df_u32 weapon)
{
    ObjectInfo *item = object_edit(weapon);

    if (item) { item->flags &= ~((df_u32)WEAPON_POISONED | ((df_u32)WEAPON_POISON_MASK << WEAPON_POISON_SHIFT)); }
}
