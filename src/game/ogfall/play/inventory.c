#include "game/ogfall/play/inventory.h"
#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/formats/book.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/play.h"
#include "engine/core/log.h"

static const char *g_error = "";
const char *og_inventory_error(void) { return g_error; }

static int metal_roll(void)
{
    int roll, level, metal;
    level = og_character()->level - 10;
    roll = og_random_range(0, 255) + (level < 0 ? level * 4 : level * 2);
    if (roll < 0) { roll = 0; }
    if (roll > 256) { roll = 256; }
    for (metal = 0; metal < 9 && roll > og_material_share(metal); metal++) {
        roll -= og_material_share(metal);
    }
    return metal;
}

static void set_metal(OgObject *item, int metal)
{
    item->material = metal;
    item->color = metal + 16;
    item->value *= 3 * og_material_value(metal);
    item->weight = item->weight * og_material_weight(metal) / 4;
    item->max_condition = item->max_condition * og_material_condition(metal) / 4;
    item->condition = item->max_condition;
}

static int armor_image(int index, int type)
{
    /* armor_image_for_type, original ranges; helms and shields keep the template. */
    static const int low[7][3] = {{3,7,4},{8,9,9},{10,16,12},{17,21,18},{22,26,23},{27,27,27},{0,1,1}};
    static const int high[7][3] = {{3,7,6},{8,9,9},{11,16,15},{17,21,20},{22,26,25},{27,27,27},{0,1,1}};
    return index < 7 ? og_random_range(low[index][type], high[index][type]) : -1;
}

static void init_item(OgObject *item, const OgItemDef *def)
{
    df_copy(item->name,def->name,sizeof(item->name));
    item->condition = item->max_condition = def->condition;
    item->weight = def->weight;
    item->value = def->value;
    item->inventory_image = def->inventory_image ? def->inventory_image : def->dropped_image;
    item->dropped_image = def->dropped_image ? def->dropped_image : def->inventory_image;
    item->color = 18;
}

df_u32 og_item_create(df_u32 parent, int group, int index, int metal)
{
    const OgItemDef *def;
    OgObject *item;
    df_u32 id;
    int image, roll;
    def = og_item_def(group, index);
    if (!def || metal < -1 || metal > 9) { return 0; }
    id = og_object_new(OG_RECORD_ITEM, parent, index, 100);
    item = og_object_edit(id);
    if (!item) { g_error = "There is no room for another item."; return 0; }
    item->group = group;
    init_item(item, def);
    if (group == 6 || group == 12 || group == 2) {
        if (def->variants > 1) { item->inventory_image += og_random_range(0, def->variants - 1); }
        if (group == 2 && !og_character()->female) { item->inventory_image += 4 * 128; }
        item->inventory_image += og_body_shape(og_character()->race) * 128;
        if (group == 6 || group == 12) {
            if (og_random_range(0,100) >= 25) {
                item->color = og_random_range(1,10);
                if ((group == 6 && (index == 10 || index == 11)) || (group == 12 && index == 8)) {
                    item->color = og_clothing_dye(0,og_random_range(0,32767));
                } else if (group == 6 && index == 4) { item->color = og_clothing_dye(1,og_random_range(0,32767)); }
            }
            if ((group == 6 && (index == 13 || index == 14)) || (group == 12 && (index == 9 || index == 10))) {
                item->inventory_image++;
            }
        }
    }
    if (group == 3 && index != 18) { set_metal(item, metal < 0 ? metal_roll() : metal); }
    if (group == 2) {
        roll = metal < 0 ? og_random_range(1, 100) : 100;
        item->armor_type = roll < 70 ? 0 : (roll < 90 ? 1 : 2);
        if (!item->armor_type) { item->weight >>= 1; }
        if (item->armor_type == 2) { set_metal(item, metal < 0 ? metal_roll() : metal); }
        else { item->value *= item->armor_type + 1; }
        image = armor_image(index, item->armor_type);
        if (image >= 0) { item->inventory_image = (item->inventory_image & ~127) + image; }
    }
    /* item_init_from_template: a recipe item holds a recipe drawn from twenty (rand() % 20). */
    if (group == 27 && index == 4) { item->message = og_random_range(0,32767) % 20 + 1; }
    if (group == 3 && index == 18) { item->count = og_random_range(1, 20); item->condition = 0; }
    if (group == 7 && og_book_count()) {
        df_u32 seed;
        int count, pick, i;
        count = 0;
        for (i = 0; i < og_book_count(); i++) {
            if (og_book_rarity(i) <= index) { count++; }
        }
        if (!count) { og_object_delete(id); return 0; }
        pick = og_random_range(0,count-1);
        for (i = 0; i < og_book_count(); i++) {
            if (og_book_rarity(i) <= index && pick-- == 0) { break; }
        }
        item->message = og_book_id(i);
        if (og_book_title(item->message,item->name,sizeof(item->name),&seed)) {
            /* item_init_book seeds rand from the title, then restores it. */
            seed = seed * 1103515245u + 12345u;
            item->value = 300 + (int)(((seed >> 16) & 0x7fffu) % 501u);
        }
    }
    if (group == 13) { item->message = og_random_range(0,32767); }
    return id;
}

void og_item_name(const OgObject *item, char *out, int capacity)
{
    const OgItemDef *def;
    const char *metal;
    def = item ? og_item_def(item->group, item->index) : 0;
    metal = "";
    if (item && item->group == 3 && item->index != 18) { metal = og_material_name(item->material); }
    if (item && item->group == 2) {
        metal = item->armor_type == 0 ? "Leather" : item->armor_type == 1 ? "Chain" : og_material_name(item->material);
    }
    log_format(out, capacity, "%s%s%s", metal, *metal ? " " : "", item && item->name[0] ? item->name : def ? def->name : "Unknown item");
    /* A magic item's name carries the macro %it: what it is made on ("%it of Life Stealing"). */
    if (item && def) {
        char tail[64];
        int i, k;

        for (i = 0; out[i]; i++) {
            if (out[i] == '%' && out[i+1] == 'i' && out[i+2] == 't') {
                for (k = 0; out[i+3+k] && k < 63; k++) { tail[k] = out[i+3+k]; }
                tail[k] = 0;
                log_format(out+i, capacity-i, "%s%s", def->name, tail);
                break;
            }
        }
    }
}

int og_item_tab(const OgObject *item)
{
    if (!item || item->type != OG_RECORD_ITEM) { return -1; }
    if ((item->group == 27 && item->index == 0) || (item->flags & 1)) { return 1; }
    return og_group_tab(item->group);
}

int og_inventory_rows(df_u32 parent, int tab, df_u32 *ids, int capacity)
{
    const OgState *s;
    int i, count;
    s = og_state();
    count = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        const OgObject *item;
        item = &s->object[i];
        if (!item->id || item->parent != parent || item->type != OG_RECORD_ITEM ||
            (tab >= 0 && og_item_tab(item) != tab)) { continue; }
        if (count < capacity) { ids[count] = item->id; }
        count++;
    }
    return count;
}

int og_item_equipped(df_u32 id)
{
    int i;
    for (i = 0; i < 27; i++) { if (id && og_state()->equipped[i] == id) { return i; } }
    return -1;
}

int og_inventory_list(df_u32 parent, int tab, df_u32 *ids, int capacity)
{
    const OgObject *item;
    int i, count;
    count = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &og_state()->object[i];
        if (!item->id || item->parent != parent || item->type != OG_RECORD_ITEM ||
            (tab >= 0 && og_item_tab(item) != tab)) { continue; }
        if (parent == og_state()->inventory_id && item->group != 1 && og_item_equipped(item->id) >= 0) { continue; }
        if (count < capacity) { ids[count] = item->id; }
        count++;
    }
    return count;
}

int og_item_two_handed(const OgObject *item)
{
    const OgItemDef *def;
    def = item && item->group == 3 ? og_item_def(item->group,item->index) : 0;
    return def && !(def->flags & 4);
}

static void sync_weapon(void)
{
    const OgObject *item;
    item = og_object(og_state()->equipped[19]);
    og_character()->weapon = item && item->group == 3 ? item->index : 0;
    og_character()->weapon_sound = item && item->group == 3 ? og_weapon_sound(item->index) : 0;
}

void og_inventory_unequip(df_u32 id)
{
    int slot;
    slot = og_item_equipped(id);
    if (slot >= 0) { og_state_equip(slot, 0); sync_weapon(); }
}

static int forbidden(const OgObject *item)
{
    const OgClass *career;
    int bits;
    /* Original weapon_proficiency_bits, by weapon index. */
    career = &og_state()->career;
    if (item->group == 3) {
        return (career->forbidden_equipment & og_weapon_proficiency(item->index)) ||
               (career->forbidden_materials & (1 << item->material));
    }
    if (item->group != 2) { return 0; }
    bits = item->index >= 7 ? 1 << (item->index + 2) : 1 << (item->armor_type + 6);
    return (career->forbidden_equipment & bits) ||
           (item->armor_type == 2 && (career->forbidden_materials & (1 << item->material)));
}

static int equip_sound(const OgObject *item)
{
    if (item->group == 3) { return og_weapon_sound(item->index); }
    if (item->group == 2) {
        if (item->index == 1 || (item->index >= 5 && item->index != 6)) { return 233; }
        return item->armor_type+231;
    }
    if (item->group == 6 || item->group == 12) { return 234; }
    return 236;
}

int og_inventory_equip(df_u32 id)
{
    const OgObject *item, *hand;
    int slot, step;
    item = og_object(id);
    g_error = "";
    if (!item || item->type != OG_RECORD_ITEM || item->parent != og_state()->inventory_id) { return 0; }
    slot = og_item_slot(item->group, item->index);
    if (slot < 0) { g_error = "This item cannot be equipped."; return 0; }
    if (item->condition <= 0) { g_error = "This item is broken."; return 0; }
    if (forbidden(item)) { g_error = "Your class forbids this equipment."; return 0; }
    if ((item->group == 6 && og_character()->female) || (item->group == 12 && !og_character()->female)) {
        g_error = "This clothing does not fit."; return 0;
    }
    if (og_item_equipped(id) >= 0) { return 1; }
    step = 0;
    if (item->group == 14 || item->group == 25) { step = 1; }
    if (item->group == 6 || item->group == 12) { if (slot == 14) { step = 2; } }
    if (item->group == 3) {
        hand = og_object(og_state()->equipped[19]);
        if (og_item_two_handed(item)) { og_state_equip(21, 0); }
        else if (!og_item_two_handed(hand)) { step = 2; }
    }
    if (slot == 21 && og_item_two_handed(og_object(og_state()->equipped[19]))) { og_state_equip(19, 0); }
    if (step && og_state()->equipped[slot] && !og_state()->equipped[slot + step]) { slot += step; }
    og_state_equip(slot, id);
    sync_weapon();
    if (og_state()->mode == OG_GAME_INVENTORY) { og_sound(equip_sound(item)); }
    return 1;
}

static int belongs_to(df_u32 id, df_u32 parent)
{
    const OgObject *object;
    int depth;
    for (depth = 0; id && depth < OG_OBJECTS; depth++) {
        if (id == parent) { return 1; }
        object = og_object(id);
        if (!object) { return 0; }
        id = object->parent;
    }
    return 0;
}

int og_inventory_weight(df_u32 parent)
{
    const OgObject *item;
    int i, weight;
    weight = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &og_state()->object[i];
        if (!item->id || item->type != OG_RECORD_ITEM || !belongs_to(item->parent, parent)) { continue; }
        if (item->group == 28) { weight += item->value / 100; }
        else { weight += item->weight * item->count; }
    }
    if (parent == og_state()->inventory_id) { weight += og_character()->gold / 100; }
    return weight;
}
int og_inventory_capacity(void)
{
    int strength;
    strength = og_character()->attribute[OG_STR];
    return strength + (strength >> 1);
}

void og_weight_text(int quarters, char *out, int capacity)
{
    static const char *fraction[] = {"",".25",".50",".75"};
    if (quarters < 0) { quarters = 0; }
    log_format(out,capacity,"%d%s",quarters/4,fraction[quarters%4]);
}

int og_inventory_armor(int part)
{
    const OgObject *item, *shield;
    int value, slot;
    slot = og_armor_slot(part);
    if (slot < 0) { return 0; }
    item = og_object(og_state()->equipped[slot]);
    value = 0;
    if (item && item->group == 2) {
        value = og_armor_base(item->armor_type);
        if (item->armor_type == 2) { value += og_material_hit(item->material); }
    }
    shield = og_object(og_state()->equipped[21]);
    if (shield && shield->group == 2 && og_shield_covers(shield->index,part)) { value += (shield->index-6)*5; }
    if (value > 200) { value = 200; }
    return value / 5;
}

df_u32 og_inventory_book(void)
{
    int i;
    const OgObject *item;
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &og_state()->object[i];
        if (item->id && item->type == OG_RECORD_ITEM && item->group == 27 && item->index == 0 &&
            item->parent == og_state()->inventory_id) { return item->id; }
    }
    return 0;
}

void og_inventory_start_spells(df_u32 book)
{
    int klass, i, spell;
    klass = og_character()->klass;
    if (klass == 18) {
        const OgClass *career;
        int skills[12], n;
        career = &og_state()->career;
        for (i = 0; i < 3; i++) { skills[i] = career->primary[i]; skills[i+3] = career->major[i]; }
        for (i = 0; i < 6; i++) { skills[i+6] = career->minor[i]; }
        for (n = 0; n < 12 && !(skills[n] >= 22 && skills[n] <= 27); n++) { }
        if (n == 12) { return; }
        klass = 1;
    }
    for (i = 0; i < 6 && (spell = og_start_spell(klass, i)) != 255; i++) {
        df_u32 id;
        id = og_spell_create(book,spell);
        if (!id) { return; }
    }
}

int og_inventory_transfer(df_u32 id, df_u32 parent)
{
    const OgObject *item, *target;
    int taking, weight;
    item = og_object(id);
    target = og_object(parent);
    g_error = "";
    if (!item || item->type != OG_RECORD_ITEM || !target || target->type != OG_RECORD_CONTAINER) { return 0; }
    if (item->parent == parent) { return 1; }
    taking = parent == og_state()->inventory_id;
    weight = item->group == 28 ? item->value / 100 : item->weight * item->count;
    weight += og_inventory_weight(id);
    if (taking && og_inventory_weight(parent) + weight > og_inventory_capacity() * 4) {
        g_error = "You cannot carry any more."; return 0;
    }
    if (taking && item->group == 28) {
        int gold;
        gold = item->value;
        if (gold > 100000000 - og_character()->gold) { g_error = "Too much gold."; return 0; }
        if (!og_object_delete(id)) { return 0; }
        og_character()->gold += gold;
        og_state_event(OG_EVENT_GOLD, id, parent, gold);
        og_sound(204);
        return 1;
    }
    if (taking && item->group == 3 && item->index == 18) {
        int i, count;
        for (i = 0; i < OG_OBJECTS; i++) {
            OgObject *stack;
            stack = og_object_edit(og_state()->object[i].id);
            if (!stack || stack->parent != parent || stack->type != OG_RECORD_ITEM ||
                stack->group != 3 || stack->index != 18) { continue; }
            count = stack->count + item->count;
            /* inv_merge_arrows clamps the one carried stack to 199 and
             * deletes the incoming stack, including any excess arrows. */
            if (count > 199) { count = 199; }
            if (!og_object_delete(id)) { return 0; }
            stack->count = count;
            og_state_event(OG_EVENT_GIVE,stack->id,parent,count);
            return 1;
        }
    }
    og_inventory_unequip(id);
    if (!og_object_reparent(id, parent)) { return 0; }
    og_state_event(taking ? OG_EVENT_GIVE : OG_EVENT_DROP, id, parent, item->count);
    return 1;
}

static void roll_loot(df_u32 container, int index)
{
    const df_u16 *table;
    df_u32 id;
    int category, chance, group, level;
    table = og_loot_table(index);
    if (!table) { return; }
    level = og_character()->level;
    if (table[0] || table[1]) {
        id = og_item_create(container, 28, 0, 0);
        if (id) { og_object_edit(id)->value = level * og_random_range(table[0], table[1]); }
    }
    /* loot_generate uses <= (so zero has a 1% first chance), scales only
     * categories 2..5 by level, and halves the chance after each success. */
    for (category = 2; category < 15; category++) {
        chance = table[category] * (category <= 5 ? level : 1);
        if (og_random_range(0, 32767) % 100 > chance) { continue; }
        group = og_loot_group(category);
        if (group == 255) { group = og_character()->female ? 12 : 6; }
        /* MAGIC.DEF and enchantment records are a later rules slice. Never
         * substitute a mundane weapon for a magic item. */
        if (group == 4) { continue; }
        do {
            id = og_item_create(container, group, og_random_range(0, og_item_kinds(group) - 1), -1);
            if (!id) { return; }
            chance >>= 1;
        } while (og_random_range(0, 32767) % 100 <= chance);
    }
}

df_u32 og_loot_container(df_u32 place, df_u32 thing, float x, float y, float z, int table)
{
    const OgObject *object;
    OgObject *container;
    df_u32 id;
    int i;
    if (!place || !thing) { return 0; }
    for (i = 0; i < OG_OBJECTS; i++) {
        object = &og_state()->object[i];
        if (object->id && object->type == OG_RECORD_CONTAINER && object->parent == OG_WORLD_ROOT &&
            object->place_key == place && object->thing_key == thing) { return object->id; }
    }
    id = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, table, 1);
    container = og_object_edit(id);
    if (!container) { return 0; }
    container->place_key = place;
    container->thing_key = thing;
    container->x = x; container->y = y; container->z = z;
    if (table >= 0) {
        for (i = 0; i < 32 && !og_inventory_rows(id,-1,0,0); i++) { roll_loot(id, table); }
    }
    return id;
}

int og_inventory_valid(const OgState *state)
{
    int i, j;
    const OgObject *item;
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &state->object[i];
        if (!item->id) { continue; }
        if (item->type == OG_RECORD_SPELL) {
            const OgObject *parent;
            parent = 0;
            for (j = 0; j < OG_OBJECTS; j++) { if (state->object[j].id == item->parent) { parent = &state->object[j]; break; } }
            if (!parent || parent->type != OG_RECORD_ITEM || parent->group != 27 || parent->index != 0 ||
                !og_spell_def(item->index) || item->flags < 0 || item->flags >= OG_OBJECTS ||
                !item->name[0] || item->name[24] || item->spell_icon < 0 || item->spell_icon >= 69) { return 0; }
        }
        if (item->type != OG_RECORD_ITEM) { continue; }
        if (item->name[31] || !og_item_def(item->group, item->index) || item->count < 1 || item->count > 10000 ||
            item->material < 0 || item->material > 9 || item->weight < 0 || item->weight > 100000 ||
            item->value < 0 || item->value > 100000000 || item->max_condition < 0 || item->max_condition > 1000000 ||
            item->condition < 0 || item->condition > item->max_condition ||
            item->armor_type < 0 || item->armor_type > 2 || item->inventory_image < 0 || item->inventory_image > 65535 ||
            item->color < 0 || item->color > 31 || item->dropped_image < 0 || item->dropped_image > 65535 ||
            item->message < 0 || item->message > 99999) { return 0; }
    }
    for (i = 0; i < 27; i++) {
        if (!state->equipped[i]) { continue; }
        for (j = 0; j < OG_OBJECTS && state->object[j].id != state->equipped[i]; j++) { }
        if (j == OG_OBJECTS || state->object[j].parent != state->inventory_id) { return 0; }
        for (j = i + 1; j < 27; j++) { if (state->equipped[j] == state->equipped[i]) { return 0; } }
    }
    return 1;
}

int og_inventory_upgrade(OgState *state)
{
    int i;
    OgObject *item;
    const OgItemDef *def;
    for (i = 0; i < OG_OBJECTS; i++) {
        item = &state->object[i];
        if (!item->id || item->type != OG_RECORD_ITEM) { continue; }
        if (item->group == 3 && item->index == 17 && item->count > 1) { item->index = 18; }
        def = og_item_def(item->group, item->index);
        if (!def) { return 0; }
        init_item(item, def);
        if (item->group == 3 && item->index != 18) { set_metal(item, item->material); }
        if (item->group == 2 || item->group == 6 || item->group == 12) {
            if (item->group == 2 && !state->who.female) { item->inventory_image += 4 * 128; }
            item->inventory_image += og_body_shape(state->who.race) * 128;
        }
    }
    state->version = OG_STATE_VERSION;
    return 1;
}
