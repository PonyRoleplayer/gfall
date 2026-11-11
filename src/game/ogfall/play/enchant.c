#include "game/ogfall/play/enchant.h"
#include "game/ogfall/play/itemmaker.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/progress.h"
#include "game/ogfall/play/spellbook.h"
#include "engine/core/log.h"

/* See enchant.h. */

static df_u32 g_stamp;
static int g_rebuild;

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
        if (type == 1 && param >= 0) {
            const OgSpellDef *held;

            og_magic_hold(param, slot);
            /* Each held spell wears the item by its own cost (the original wears once by the last one's: 9.16). */
            held = og_spell_def(param);
            if (held) { og_item_damage(item, og_spell_cost(held, og_character())); }
            if (!og_object(item)) { return; }
        }
        else if (type == 5) { g_stamp = og_state()->minutes; }
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
        else if (type == 3) { g_rebuild = 1; }
    }
}

/* ---- Wear and breaking (equip.c item_damage, inven.c item_break) ---- */

static const OgEnchantWorld *g_world;

void og_enchant_set_world(const OgEnchantWorld *world) { g_world = world; }

static int daylight(void) { return g_world && g_world->daylight && g_world->daylight(); }
static int temple(void) { return g_world && g_world->temple && g_world->temple(); }

static void item_break(df_u32 id)
{
    OgObject *item;
    char name[48], line[80];
    int enchanted, k, type, param;

    item = og_object_edit(id);
    if (!item) { return; }
    item->condition = 0;
    og_item_name(item, name, sizeof(name));
    log_format(line, sizeof(line), "%s is broken.", name);
    og_msg_set(line);
    enchanted = og_item_enchanted(id);
    og_inventory_unequip(id);
    if (!enchanted) { return; }
    for (k = 0; og_item_enchantment(id, k, &type, &param); k++) {
        if (type == 15) { og_msg_set("The soul from your broken item is released."); }
    }
    og_object_delete(id);
}

void og_item_damage(df_u32 id, int n)
{
    OgObject *item;

    item = og_object_edit(id);
    if (!item || item->type != OG_RECORD_ITEM) { return; }
    if (item->condition > n) { item->condition -= n; return; }
    item_break(id);
}

/* ---- The tick (disease_000680DD.c item_enchantment_tick) ---- */

/* enchant_spell_points_condition: 75 when the parameter's condition holds. */
static int extra_points(int param)
{
    static const int season[12] = {0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 0, 0};
    int year, month, day, weekday, moon0, moon1;
    df_u32 now;

    now = og_state()->minutes;
    og_state_date(&year, &month, &day, &weekday);
    moon0 = (int)((now / 1440u) & 31u); moon1 = (int)(((now + 5760u) / 1440u) & 31u);
    if (param >= 0 && param <= 3) { return season[month % 12] == param ? 75 : 0; }
    if (param == 4) { return moon0 == 0 || moon1 == 0 ? 75 : 0; }
    if (param == 5) { return moon0 == 8 || moon0 == 24 || moon1 == 8 || moon1 == 24 ? 75 : 0; }
    if (param == 6) { return moon0 == 16 || moon1 == 16 ? 75 : 0; }
    if (param >= 7 && param <= 10) { return g_world && g_world->category_near && g_world->category_near(param - 7, 1024) ? 75 : 0; }
    return 0;
}

static void repair_first(int amount)
{
    int i;

    for (i = 0; i < OG_OBJECTS; i++) {
        OgObject *item;
        int depth;
        const OgObject *owner;

        item = og_object_edit(og_state()->object[i].id);
        if (!item || item->type != OG_RECORD_ITEM || (item->group == 3 && item->index == 17) || og_item_enchanted(item->id)) { continue; }
        for (owner = item, depth = 0; owner && depth < OG_OBJECTS && owner->parent != og_state()->entity_id; depth++) { owner = og_object(owner->parent); }
        if (!owner || item->condition >= item->max_condition) { continue; }
        item->condition += amount;
        if (item->condition > item->max_condition) { item->condition = item->max_condition; }
        return;
    }
}

static void tick_one(df_u32 id, int type, int param, int amount)
{
    OgCharacter *who;

    who = og_character();
    if (type == 1) { og_item_damage(id, 1); }
    else if (type == 3) {
        if (og_state()->minutes % 4u == 0) { og_item_damage(id, 1); }
        if (param >= 7 && param <= 10 && extra_points(param) && who->magicka < who->max_magicka) { who->magicka += amount * 5; }
    } else if (type == 5) {
        if (param == 1 ? !daylight() : param == 2 ? daylight() : 0) { return; }
        who->health += amount;
        if (who->health > who->max_health) { who->health = who->max_health; }
        else if (og_random_range(0, 9) == 0) { og_item_damage(id, 1); }
    } else if (type == 6 && param == 0) {
        if (who->health < who->max_health && g_world && g_world->hurt_nearest && g_world->hurt_nearest(amount)) {
            who->health += amount;
            if (who->health > who->max_health) { who->health = who->max_health; }
            og_item_damage(id, amount);
        }
    } else if (type == 8) {
        repair_first(amount);
        if (og_random_range(0, 9) == 0) { og_item_damage(id, 1); }
    } else if (type == 16) {
        if (param == 1 ? !daylight() : param == 2 ? !temple() : 0) { return; }
        og_item_damage(id, amount);
    } else if (type == 17) {
        if (param == 0 ? daylight() : temple()) { og_damage_player(amount, 0); }
    } else if (type == 21 && (param == 1 || param == 2)) {
        if (og_state()->minutes - og_state()->last_kill > (param == 1 ? 1440u : 10080u)) { og_damage_player(amount, 0); }
    }
}

void og_enchant_minutes(void)
{
    df_u32 now, elapsed;
    int amount, slot, k, type, param;

    now = og_state()->minutes;
    elapsed = now >= g_stamp ? now - g_stamp : 0;
    if (elapsed > 100) { elapsed = 0; g_stamp = now; }
    amount = (int)(elapsed >> 2);
    if (!amount) { return; }
    g_stamp = now;
    for (slot = 0; slot < 27; slot++) {
        df_u32 id;

        id = og_state()->equipped[slot];
        for (k = 0; id && og_object(id) && og_item_enchantment(id, k, &type, &param); k++) { tick_one(id, type, param, amount); }
    }
    if (og_character()->magicka < 0) { og_character()->magicka = 0; }
}

void og_enchant_frame(void)
{
    int slot, k, type, param, bonus, any;
    OgCharacter *who;

    bonus = 0; any = 0;
    for (slot = 0; slot < 27; slot++) {
        for (k = 0; og_state()->equipped[slot] && og_item_enchantment(og_state()->equipped[slot], k, &type, &param); k++) {
            if (type == 3) { any = 1; bonus += extra_points(param); }
        }
    }
    if (!any && !g_rebuild) { return; }
    /* The maximum is built again from Intelligence and the class, plus the bonus, each frame while an item gives one (and once more when the last is taken off). */
    who = og_character();
    who->max_magicka = og_magicka(who->attribute[OG_INT], who->class_flags) + bonus;
    if (who->magicka > who->max_magicka) { who->magicka = who->max_magicka; }
    g_rebuild = any;
}

int og_item_use(df_u32 id)
{
    int k, type, param, used;

    used = 0;
    for (k = 0; og_object(id) && og_item_enchantment(id, k, &type, &param); k++) {
        if (type == 0 && param >= 0) {
            if (og_magic_cast_item(param)) { used++; og_item_damage(id, 10); }
        } else if (type == 21 && param == 0) {
            og_damage_player((og_character()->level >> 1) + 1, 0);
            og_item_damage(id, 2);
        }
    }
    return used;
}

int og_enchant_hit_bonus(df_u32 weapon, int category)
{
    int k, type, param, bonus;

    bonus = 0;
    for (k = 0; og_item_enchantment(weapon, k, &type, &param); k++) {
        if (type == 4 && param == category) { bonus += og_character()->level; }
        else if (type == 20 && param == category) { bonus -= og_character()->level; }
        else if (type == 21 && param == 0) { og_damage_player((og_character()->level >> 2) + 1, 0); }
    }
    return bonus;
}

void og_enchant_strike(df_u32 weapon, int victim, int dealt)
{
    int k, type, param, healed;
    OgCharacter *who;

    who = og_character();
    for (k = 0; og_object(weapon) && og_item_enchantment(weapon, k, &type, &param); k++) {
        if (type == 2 && param >= 0) {
            og_magic_strike(param, victim);
            og_item_damage(weapon, 10);
        } else if (type == 6 && param == 1) {
            healed = dealt / 2;
            if (healed > who->max_health - who->health) { healed = who->max_health - who->health; }
            if (healed < 0) { healed = 0; }
            who->health += healed;
            og_item_damage(weapon, healed / 4 + 1);
        }
    }
}

int og_enchant_armor_modifier(void)
{
    int slot, k, type, param;

    for (slot = 0; slot < 27; slot++) {
        for (k = 0; og_state()->equipped[slot] && og_item_enchantment(og_state()->equipped[slot], k, &type, &param); k++) {
            if (type == 12) { return -25; }
            if (type == 24) { return 25; }
        }
    }
    return 0;
}

int og_enchant_capacity(int capacity)
{
    int slot, k, type, param;

    for (slot = 0; slot < 27; slot++) {
        for (k = 0; og_state()->equipped[slot] && og_item_enchantment(og_state()->equipped[slot], k, &type, &param); k++) {
            if (type == 7) { return param == 0 ? capacity * 320 / 256 : capacity * 384 / 256; }
        }
    }
    return capacity;
}

int og_enchant_weight(df_u32 id, int weight)
{
    int k, type, param;

    for (k = 0; og_item_enchantment(id, k, &type, &param); k++) {
        if (type == 11) { return 1; }
        if (type == 23) { return weight * 4 < 20 ? 20 : weight * 4; }
    }
    return weight;
}

int og_enchant_reaction(int social)
{
    int i, k, type, param, depth, mod;
    const OgObject *item, *owner;

    mod = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &og_state()->object[i];
        if (!item->id || item->type != OG_RECORD_ITEM) { continue; }
        for (owner = item, depth = 0; owner && depth < OG_OBJECTS && owner->parent != og_state()->entity_id; depth++) { owner = og_object(owner->parent); }
        if (!owner) { continue; }
        for (k = 0; og_item_enchantment(item->id, k, &type, &param); k++) {
            if (type == 14 && (param == 5 || param == social)) { mod += 10; }
            else if (type == 25 && (param == 5 || param == social)) { mod -= 10; }
        }
    }
    return mod;
}
