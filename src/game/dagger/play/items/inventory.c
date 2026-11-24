#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/combat/weapon.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/formats/book.h"
#include "game/dagger/play/magic/spellbook.h"
#include "game/dagger/play/play.h"
#include "engine/core/log.h"

static const char *g_error = "";
const char *inventory_error(void) { return g_error; }

static int metal_roll(void)
{
    int roll, level, metal;
    level = character_state()->level - MATERIAL_LEVEL_PIVOT;
    roll = random_range(0, MATERIAL_ROLL_MAX) + (level < 0 ? level * MATERIAL_LEVEL_LOW_MUL : level * MATERIAL_LEVEL_HIGH_MUL);
    if (roll < 0) { roll = 0; }
    if (roll > MATERIAL_ROLL_CLAMP) { roll = MATERIAL_ROLL_CLAMP; }
    for (metal = 0; metal < MATERIALS - 1 && roll > material_share(metal); metal++) {
        roll -= material_share(metal);
    }
    return metal;
}

static void set_metal(ObjectInfo *item, int metal)
{
    item->material = metal;
    item->color = metal + MATERIAL_COLOR_BASE;
    item->value *= MATERIAL_VALUE_MUL * material_value(metal);
    item->weight = item->weight * material_weight(metal) / MATERIAL_SCALE;
    item->max_condition = item->max_condition * material_condition(metal) / MATERIAL_SCALE;
    item->condition = item->max_condition;
}

static int armor_image(int index, int type)
{
    /* armor_image_for_type, original ranges; helms and shields keep the template. */
    static const int low[7][3] = {{3,7,4},{8,9,9},{10,16,12},{17,21,18},{22,26,23},{27,27,27},{0,1,1}};
    static const int high[7][3] = {{3,7,6},{8,9,9},{11,16,15},{17,21,20},{22,26,25},{27,27,27},{0,1,1}};
    return index < 7 ? random_range(low[index][type], high[index][type]) : -1;
}

static void init_item(ObjectInfo *item, const ItemDef *def)
{
    df_copy(item->name,def->name,sizeof(item->name));
    item->condition = item->max_condition = def->condition;
    item->weight = def->weight;
    item->value = def->value;
    item->inventory_image = def->inventory_image ? def->inventory_image : def->dropped_image;
    item->dropped_image = def->dropped_image ? def->dropped_image : def->inventory_image;
    item->color = ITEM_COLOR_PLAIN;
}

df_u32 item_create(df_u32 parent, int group, int index, int metal)
{
    const ItemDef *def;
    ObjectInfo *item;
    df_u32 id;
    int image, roll;
    if (group == ITEM_GROUP_MAGIC_ITEM) { return magic_item_create(parent,index); }
    if (group == ITEM_GROUP_ARTIFACT) { return magic_artifact_create(parent,index); }
    def = item_def(group, index);
    if (!def || metal < -1 || metal >= MATERIALS) { return 0; }
    id = object_new(RECORD_ITEM, parent, index, 100);
    item = object_edit(id);
    if (!item) { g_error = "There is no room for another item."; return 0; }
    item->group = group;
    init_item(item, def);
    if (group == ITEM_GROUP_MENS_CLOTHING || group == ITEM_GROUP_WOMENS_CLOTHING || group == ITEM_GROUP_ARMOR) {
        if (def->variants > 1) { item->inventory_image += random_range(0, def->variants - 1); }
        if (group == ITEM_GROUP_ARMOR && !character_state()->female) { item->inventory_image += ARMOR_MALE_OFFSET * IMAGE_ARCHIVE_STRIDE; }
        item->inventory_image += body_shape(character_state()->race) * IMAGE_ARCHIVE_STRIDE;
        if (group == ITEM_GROUP_MENS_CLOTHING || group == ITEM_GROUP_WOMENS_CLOTHING) {
            if (random_range(0,PERCENT) >= DYE_KEEP_CHANCE) {
                item->color = random_range(DYE_COLOR_MIN,DYE_COLOR_MAX);
                if ((group == ITEM_GROUP_MENS_CLOTHING && (index == MENS_DYED_FIRST || index == MENS_DYED_FIRST + 1)) || (group == ITEM_GROUP_WOMENS_CLOTHING && index == WOMENS_DYED)) {
                    item->color = clothing_dye(0,random_range(0,RAND_MAX_WATCOM));
                } else if (group == ITEM_GROUP_MENS_CLOTHING && index == MENS_DYED_OTHER) { item->color = clothing_dye(1,random_range(0,RAND_MAX_WATCOM)); }
            }
            if ((group == ITEM_GROUP_MENS_CLOTHING && (index == MENS_EXTRA_STYLE || index == MENS_EXTRA_STYLE + 1)) || (group == ITEM_GROUP_WOMENS_CLOTHING && (index == WOMENS_EXTRA_STYLE || index == WOMENS_EXTRA_STYLE + 1))) {
                item->inventory_image++;
            }
        }
    }
    if (group == ITEM_GROUP_WEAPON && index != WEAPON_ARROW) { set_metal(item, metal < 0 ? metal_roll() : metal); }
    if (group == ITEM_GROUP_ARMOR) {
        roll = metal < 0 ? random_range(1, PERCENT) : PERCENT;
        item->armor_type = roll < ARMOR_LEATHER_CHANCE ? ARMOR_LEATHER : (roll < ARMOR_CHAIN_CHANCE ? ARMOR_CHAIN : ARMOR_PLATE);
        if (item->armor_type == ARMOR_LEATHER) { item->weight >>= 1; }
        if (item->armor_type == ARMOR_PLATE) { set_metal(item, metal < 0 ? metal_roll() : metal); }
        else { item->value *= item->armor_type + 1; }
        image = armor_image(index, item->armor_type);
        if (image >= 0) { item->inventory_image = (item->inventory_image & ~(IMAGE_ARCHIVE_STRIDE - 1)) + image; }
    }
    /* item_init_from_template: a recipe item holds a recipe drawn from twenty (rand() % 20). */
    if (group == ITEM_GROUP_MISC && index == MISC_RECIPE) { item->message = random_range(0,RAND_MAX_WATCOM) % RECIPES + 1; }
    if (group == ITEM_GROUP_WEAPON && index == WEAPON_ARROW) { item->count = random_range(ARROW_STACK_MIN, ARROW_STACK_ROLL_MAX); item->condition = 0; }
    if (group == ITEM_GROUP_BOOK && book_count()) {
        df_u32 seed;
        int count, pick, i;
        count = 0;
        for (i = 0; i < book_count(); i++) {
            if (book_rarity(i) <= index) { count++; }
        }
        if (!count) { object_delete(id); return 0; }
        pick = random_range(0,count-1);
        for (i = 0; i < book_count(); i++) {
            if (book_rarity(i) <= index && pick-- == 0) { break; }
        }
        item->message = book_id(i);
        if (book_title(item->message,item->name,sizeof(item->name),&seed)) {
            /* item_init_book seeds rand from the title, then restores it. */
            item->value = BOOK_VALUE_MIN + (int)(watcom_next(&seed) % BOOK_VALUE_SPREAD);
        }
    }
    if (group == ITEM_GROUP_PAINTING) { item->message = random_range(0,RAND_MAX_WATCOM); }
    return id;
}

void item_name(const ObjectInfo *item, char *out, int capacity)
{
    const ItemDef *def;
    const char *metal;
    def = item ? item_def(item->group, item->index) : 0;
    if (item && magic_def_artifact(magic_item_def(item->id))) { df_copy(out,item->name,capacity); return; }
    metal = "";
    if (item && item->group == ITEM_GROUP_WEAPON && item->index != WEAPON_ARROW) { metal = material_name(item->material); }
    if (item && item->group == ITEM_GROUP_ARMOR) {
        metal = item->armor_type == 0 ? "Leather" : item->armor_type == 1 ? "Chain" : material_name(item->material);
    }
    log_format(out, capacity, "%s%s%s", metal, *metal ? " " : "", item && item->name[0] && !magic_item_unknown(item->id) ? item->name : def ? def->name : "Unknown item");
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

int item_tab(const ObjectInfo *item)
{
    if (!item || item->type != RECORD_ITEM) { return -1; }
    if ((item->group == ITEM_GROUP_MISC && item->index == MISC_SPELLBOOK) || (item->flags & 1)) { return 1; }
    return group_tab(item->group);
}

int inventory_rows(df_u32 parent, int tab, df_u32 *ids, int capacity)
{
    const GameState *s;
    int i, count;
    s = game_state();
    count = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *item;
        item = &s->object[i];
        if (!item->id || item->parent != parent || item->type != RECORD_ITEM ||
            (tab >= 0 && item_tab(item) != tab)) { continue; }
        if (count < capacity) { ids[count] = item->id; }
        count++;
    }
    return count;
}

int item_equipped(df_u32 id)
{
    int i;
    for (i = 0; i < EQUIP_SLOTS; i++) { if (id && game_state()->equipped[i] == id) { return i; } }
    return -1;
}

int inventory_list(df_u32 parent, int tab, df_u32 *ids, int capacity)
{
    const ObjectInfo *item;
    int i, count;
    count = 0;
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->parent != parent || item->type != RECORD_ITEM ||
            (tab >= 0 && item_tab(item) != tab)) { continue; }
        if (parent == game_state()->inventory_id && item->group != ITEM_GROUP_USELESS && item_equipped(item->id) >= 0) { continue; }
        if (count < capacity) { ids[count] = item->id; }
        count++;
    }
    return count;
}

int item_two_handed(const ObjectInfo *item)
{
    const ItemDef *def;
    def = item && item->group == ITEM_GROUP_WEAPON ? item_def(item->group,item->index) : 0;
    return def && !(def->flags & 4);
}

static void sync_weapon(void)
{
    const ObjectInfo *item;
    item = object_at(game_state()->equipped[EQUIP_RIGHT_HAND]);
    character_state()->weapon = item && item->group == ITEM_GROUP_WEAPON ? item->index : 0;
    character_state()->weapon_sound = item && item->group == ITEM_GROUP_WEAPON ? weapon_sound(item->index) : 0;
}

void inventory_unequip(df_u32 id)
{
    int slot;
    slot = item_equipped(id);
    if (slot >= 0) { state_equip(slot, 0); sync_weapon(); }
}

static int forbidden(const ObjectInfo *item)
{
    const ClassInfo *career;
    int bits;
    /* Original weapon_proficiency_bits, by weapon index. */
    career = &game_state()->career;
    if (item->group == ITEM_GROUP_WEAPON) {
        return (career->forbidden_equipment & weapon_proficiency(item->index)) ||
               (career->forbidden_materials & (1 << item->material));
    }
    if (item->group != ITEM_GROUP_ARMOR) { return 0; }
    bits = item->index >= ARMOR_SHIELD_FIRST ? 1 << (item->index + FORBID_SHIELD_SHIFT) : 1 << (item->armor_type + FORBID_ARMOR_TYPE_SHIFT);
    return (career->forbidden_equipment & bits) ||
           (item->armor_type == ARMOR_PLATE && (career->forbidden_materials & (1 << item->material)));
}

static int equip_sound(const ObjectInfo *item)
{
    if (item->group == ITEM_GROUP_WEAPON) { return weapon_sound(item->index); }
    if (item->group == ITEM_GROUP_ARMOR) {
        if (item->index == ARMOR_GAUNTLETS || (item->index >= ARMOR_HELM && item->index != ARMOR_BOOTS)) { return SOUND_EQUIP_METAL; }
        return item->armor_type+SOUND_EQUIP_ARMOR;
    }
    if (item->group == ITEM_GROUP_MENS_CLOTHING || item->group == ITEM_GROUP_WOMENS_CLOTHING) { return SOUND_EQUIP_CLOTHES; }
    return SOUND_EQUIP_OTHER;
}

int inventory_equip(df_u32 id)
{
    const ObjectInfo *item, *hand;
    int slot, step;
    item = object_at(id);
    g_error = "";
    if (!item || item->type != RECORD_ITEM || item->parent != game_state()->inventory_id) { return 0; }
    slot = item_slot(item->group, item->index);
    if (slot < 0) { g_error = "This item cannot be equipped."; return 0; }
    if (item->condition <= 0) { g_error = "This item is broken."; return 0; }
    if (forbidden(item)) { g_error = "Your class forbids this equipment."; return 0; }
    if ((item->group == ITEM_GROUP_MENS_CLOTHING && character_state()->female) || (item->group == ITEM_GROUP_WOMENS_CLOTHING && !character_state()->female)) {
        g_error = "This clothing does not fit."; return 0;
    }
    if (item_equipped(id) >= 0) { return 1; }
    step = 0;
    if (item->group == ITEM_GROUP_GEM || item->group == ITEM_GROUP_JEWELRY) { step = 1; }
    if (item->group == ITEM_GROUP_MENS_CLOTHING || item->group == ITEM_GROUP_WOMENS_CLOTHING) { if (slot == EQUIP_CLOAK0) { step = 2; } }
    if (item->group == ITEM_GROUP_WEAPON) {
        hand = object_at(game_state()->equipped[EQUIP_RIGHT_HAND]);
        if (item_two_handed(item)) { state_equip(EQUIP_LEFT_HAND, 0); }
        else if (!item_two_handed(hand)) { step = 2; }
    }
    if (slot == EQUIP_LEFT_HAND && item_two_handed(object_at(game_state()->equipped[EQUIP_RIGHT_HAND]))) { state_equip(EQUIP_RIGHT_HAND, 0); }
    if (step && game_state()->equipped[slot] && !game_state()->equipped[slot + step]) { slot += step; }
    state_equip(slot, id);
    sync_weapon();
    if (game_state()->mode == GAME_INVENTORY) { play_sound(equip_sound(item)); }
    return 1;
}

static int belongs_to(df_u32 id, df_u32 parent)
{
    const ObjectInfo *object;
    int depth;
    for (depth = 0; id && depth < OBJECTS; depth++) {
        if (id == parent) { return 1; }
        object = object_at(id);
        if (!object) { return 0; }
        id = object->parent;
    }
    return 0;
}

int inventory_weight(df_u32 parent)
{
    const ObjectInfo *item;
    int i, weight;
    weight = 0;
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->type != RECORD_ITEM || item->group == ITEM_GROUP_TRANSPORT || !belongs_to(item->parent, parent)) { continue; }
        if (item->group == ITEM_GROUP_CURRENCY) { weight += item->value / GOLD_PER_WEIGHT; }
        else { weight += enchant_weight(item->id, item->weight) * item->count; }
    }
    if (parent == game_state()->inventory_id) { weight += character_state()->gold / GOLD_PER_WEIGHT; }
    return weight;
}
int inventory_capacity(void)
{
    int strength;
    strength = character_state()->attribute[ATTR_STR];
    return enchant_capacity(strength + (strength >> 1));
}

void weight_text(int quarters, char *out, int capacity)
{
    static const char *fraction[] = {"",".25",".50",".75"};
    if (quarters < 0) { quarters = 0; }
    log_format(out,capacity,"%d%s",quarters/4,fraction[quarters%4]);
}

int inventory_armor(int part)
{
    const ObjectInfo *item, *shield;
    int value, slot;
    slot = armor_slot(part);
    if (slot < 0) { return 0; }
    item = object_at(game_state()->equipped[slot]);
    value = 0;
    if (item && item->group == ITEM_GROUP_ARMOR) {
        value = armor_base(item->armor_type);
        if (item->armor_type == ARMOR_PLATE) { value += material_hit(item->material); }
    }
    shield = object_at(game_state()->equipped[EQUIP_LEFT_HAND]);
    if (shield && shield->group == ITEM_GROUP_ARMOR && shield_covers(shield->index,part)) { value += (shield->index-SHIELD_INDEX_BASE)*SHIELD_ARMOR_PER_SIZE; }
    if (value > ARMOR_RATING_CAP) { value = ARMOR_RATING_CAP; }
    return value / ARMOR_RATING_DIV;
}

df_u32 inventory_book(void)
{
    int i;
    const ObjectInfo *item;
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (item->id && item->type == RECORD_ITEM && item->group == ITEM_GROUP_MISC && item->index == MISC_SPELLBOOK &&
            item->parent == game_state()->inventory_id) { return item->id; }
    }
    return 0;
}

void inventory_start_spells(df_u32 book)
{
    int klass, i, spell;
    klass = character_state()->klass;
    if (klass == CLASS_CUSTOM) {
        const ClassInfo *career;
        int skills[12], n;
        career = &game_state()->career;
        for (i = 0; i < 3; i++) { skills[i] = career->primary[i]; skills[i+3] = career->major[i]; }
        for (i = 0; i < 6; i++) { skills[i+6] = career->minor[i]; }
        for (n = 0; n < 12 && !(skills[n] >= SKILL_DESTRUCTION && skills[n] <= SKILL_MYSTICISM); n++) { }
        if (n == 12) { return; }
        klass = 1;
    }
    for (i = 0; i < 6 && (spell = start_spell(klass, i)) != 255; i++) {
        df_u32 id;
        id = spell_create(book,spell);
        if (!id) { return; }
    }
}

static ObjectInfo *carried_arrow_stack(df_u32 except)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *item = &game_state()->object[i];

        if (item->id && item->id != except && item->parent == game_state()->inventory_id && item->type == RECORD_ITEM &&
            item->group == ITEM_GROUP_WEAPON && item->index == WEAPON_ARROW && !(item->flags & ITEM_UNPAID)) {
            return object_edit(item->id);
        }
    }
    return 0;
}

void inventory_claim(df_u32 id)
{
    ObjectInfo *item = object_edit(id);

    if (!item || !(item->flags & ITEM_UNPAID)) { return; }
    item->flags &= ~ITEM_UNPAID;
    if (item->group == ITEM_GROUP_WEAPON && item->index == WEAPON_ARROW) {
        ObjectInfo *stack = carried_arrow_stack(id);
        int count = item->count;

        if (stack && stack->count + count <= STACK_MAX && object_delete(id)) { stack->count += count; }
    }
}

int inventory_transfer(df_u32 id, df_u32 parent)
{
    const ObjectInfo *item, *target;
    int taking, weight;
    item = object_at(id);
    target = object_at(parent);
    g_error = "";
    if (!item || item->type != RECORD_ITEM || !target || target->type != RECORD_CONTAINER) { return 0; }
    if (item->parent == parent) { return 1; }
    taking = parent == game_state()->inventory_id;
    if (taking && item->group != ITEM_GROUP_TRANSPORT) {
        weight = item->group == ITEM_GROUP_CURRENCY ? item->value / GOLD_PER_WEIGHT : item->weight * item->count;
        weight += inventory_weight(id);
        if (inventory_weight(parent) + weight > inventory_capacity() * WEIGHT_UNITS_PER_KG) {
            g_error = "You cannot carry any more."; return 0;
        }
    }
    if (taking && item->group == ITEM_GROUP_CURRENCY) {
        int gold;
        gold = item->value;
        if (gold > GOLD_MAX - character_state()->gold) { g_error = "Too much gold."; return 0; }
        if (!object_delete(id)) { return 0; }
        character_state()->gold += gold;
        state_event(EVENT_GOLD, id, parent, gold);
        play_sound(SOUND_GOLD);
        return 1;
    }
    if (taking && item->group == ITEM_GROUP_WEAPON && item->index == WEAPON_ARROW) {
        int count = item->count, space = STACK_MAX - arrow_count();
        ObjectInfo *stack;

        /* The shop's stack stays separate so it can be priced and returned intact. */
        if (space <= 0 || ((item->flags & ITEM_UNPAID) && count > space)) {
            g_error = "You cannot carry any more arrows."; return 0;
        }
        stack = item->flags & ITEM_UNPAID ? 0 : carried_arrow_stack(id);
        if (stack) {
            if (count > space) {
                stack->count += space;
                object_edit(id)->count = count - space;
                state_event(EVENT_GIVE,stack->id,parent,stack->count);
                g_error = "You cannot carry any more arrows.";
                return 1;
            }
            if (!object_delete(id)) { return 0; }
            stack->count += count;
            state_event(EVENT_GIVE,stack->id,parent,stack->count);
            return 1;
        }
    }
    inventory_unequip(id);
    if (!object_reparent(id, parent)) { return 0; }
    state_event(taking ? EVENT_GIVE : EVENT_DROP, id, parent, item->count);
    return 1;
}

static void roll_loot(df_u32 container, int index)
{
    const df_u16 *table;
    df_u32 id;
    int category, chance, group, level;
    table = item_loot_list(index);
    if (!table) { return; }
    /* A place's piles are rolled at the level the player had when they came into it (docs/gfall-place-state.md). */
    level = place_roll_level(object_at(container) ? object_at(container)->place_key : 0u);
    if (table[0] || table[1]) {
        id = item_create(container, ITEM_GROUP_CURRENCY, 0, 0);
        if (id) { object_edit(id)->value = level * random_range(table[0], table[1]); }
    }
    /* loot_generate uses <= (so zero has a 1% first chance), scales only
     * categories 2..5 by level, and halves the chance after each success. */
    for (category = LOOT_FIRST_CATEGORY; category < LOOT_CATEGORIES; category++) {
        chance = table[category] * (category <= LOOT_LEVEL_LAST ? level : 1);
        if (random_range(0, RAND_MAX_WATCOM) % PERCENT > chance) { continue; }
        group = loot_group(category);
        if (group == LOOT_CLOTHING) { group = character_state()->female ? ITEM_GROUP_WOMENS_CLOTHING : ITEM_GROUP_MENS_CLOTHING; }
        /* MAGIC.DEF and enchantment records are a later rules slice. Never
         * substitute a mundane weapon for a magic item. */
        if (group == ITEM_GROUP_MAGIC_ITEM) { continue; }
        do {
            id = item_create(container, group, random_range(0, item_kinds(group) - 1), -1);
            if (!id) { return; }
            chance >>= 1;
        } while (random_range(0, RAND_MAX_WATCOM) % PERCENT <= chance);
    }
}

df_u32 loot_container_find(df_u32 place, df_u32 thing)
{
    const ObjectInfo *object;
    int i;

    if (!place || !thing) { return 0; }
    for (i = 0; i < OBJECTS; i++) {
        object = &game_state()->object[i];
        if (object->id && object->type == RECORD_CONTAINER && object->parent == WORLD_ROOT &&
            object->place_key == place && object->thing_key == thing) { return object->id; }
    }
    return 0;
}

df_u32 loot_container(df_u32 place, df_u32 thing, float x, float y, float z, int table)
{
    ObjectInfo *container;
    df_u32 id;
    int i;

    if (!place || !thing) { return 0; }
    id = loot_container_find(place,thing);
    if (id) { return id; }
    id = object_new(RECORD_CONTAINER, WORLD_ROOT, table, 1);
    container = object_edit(id);
    if (!container) { return 0; }
    container->place_key = place;
    container->thing_key = thing;
    container->message = (int)place_epoch();
    container->x = x; container->y = y; container->z = z;
    if (table >= 0) {
        /* The pile is the same however often it is opened afresh: its rolls come from its own seed, not from the stream the frame loop spends (docs/gfall-place-state.md, 1). */
        seed_stream_begin(thing_seed(place, thing));
        for (i = 0; i < LOOT_FILL_TRIES && !inventory_rows(id,-1,0,0); i++) { roll_loot(id, table); }
        seed_stream_end();
    }
    return id;
}

int inventory_valid(const GameState *state)
{
    int i, j;
    const ObjectInfo *item;
    for (i = 0; i < OBJECTS; i++) {
        item = &state->object[i];
        if (!item->id) { continue; }
        if (item->type == RECORD_SPELL) {
            const ObjectInfo *parent;
            parent = 0;
            for (j = 0; j < OBJECTS; j++) { if (state->object[j].id == item->parent) { parent = &state->object[j]; break; } }
            if (!parent || parent->type != RECORD_ITEM || parent->group != ITEM_GROUP_MISC || parent->index != MISC_SPELLBOOK ||
                !spell_def(item->index) || item->flags < 0 || item->flags >= OBJECTS ||
                !item->name[0] || item->name[24] || item->spell_icon < 0 || item->spell_icon >= 69) { return 0; }
        }
        if (item->type != RECORD_ITEM) { continue; }
        if (item->name[31] || !item_def(item->group, item->index) || item->count < 1 || item->count > 10000 ||
            item->material < 0 || item->material > 9 || item->weight < 0 || item->weight > 100000 ||
            item->value < 0 || item->value > 100000000 || item->max_condition < 0 || item->max_condition > 1000000 ||
            item->condition < 0 || item->condition > item->max_condition ||
            item->armor_type < 0 || item->armor_type > 2 || item->inventory_image < 0 || item->inventory_image > 65535 ||
            item->color < 0 || item->color > 31 || item->dropped_image < 0 || item->dropped_image > 65535 ||
            item->message < 0 || item->message > 99999) { return 0; }
    }
    for (i = 0; i < EQUIP_SLOTS; i++) {
        if (!state->equipped[i]) { continue; }
        for (j = 0; j < OBJECTS && state->object[j].id != state->equipped[i]; j++) { }
        if (j == OBJECTS || state->object[j].parent != state->inventory_id) { return 0; }
        for (j = i + 1; j < EQUIP_SLOTS; j++) { if (state->equipped[j] == state->equipped[i]) { return 0; } }
    }
    return 1;
}

int inventory_upgrade(GameState *state)
{
    int i;
    ObjectInfo *item;
    const ItemDef *def;
    for (i = 0; i < OBJECTS; i++) {
        item = &state->object[i];
        if (!item->id || item->type != RECORD_ITEM) { continue; }
        if (item->group == ITEM_GROUP_WEAPON && item->index == 17 && item->count > 1) { item->index = 18; }
        def = item_def(item->group, item->index);
        if (!def) { return 0; }
        init_item(item, def);
        if (item->group == ITEM_GROUP_WEAPON && item->index != WEAPON_ARROW) { set_metal(item, item->material); }
        if (item->group == ITEM_GROUP_ARMOR || item->group == ITEM_GROUP_MENS_CLOTHING || item->group == ITEM_GROUP_WOMENS_CLOTHING) {
            if (item->group == ITEM_GROUP_ARMOR && !state->who.female) { item->inventory_image += ARMOR_MALE_OFFSET * IMAGE_ARCHIVE_STRIDE; }
            item->inventory_image += body_shape(state->who.race) * IMAGE_ARCHIVE_STRIDE;
        }
    }
    state->version = STATE_VERSION;
    return 1;
}
