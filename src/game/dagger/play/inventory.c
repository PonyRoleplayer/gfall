#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See inventory.h. */

/* A bare body part's armour value, and how much one point of armour takes off it (DFU's). */
#define ARMOR_BARE 100
#define ARMOR_POINT 5
/* An armour piece's paper doll archives: a woman's and a man's, each then moved by body shape. */
#define ARCHIVE_FEMALE 245
#define ARCHIVE_MALE 249
/* Gold's weight is its template's, a coin's; a pack's gold is a count, not an item. */
#define GOLD_KG 0.0025f

/* Whether this template comes in this material: leather and chain are armour's alone. */
static int comes_in(int template_id, int material_id)
{
    if (material_id == MATERIAL_LEATHER || material_id == MATERIAL_CHAIN) {
        return armor_template(template_id) != 0;
    }
    return 1;
}

int item_make(Item *it, int template_id, int material_id)
{
    const ItemTemplate *t;
    const Material *m;
    int quarters;
    int n;
    int q;
    int r;
    int lowest;
    int highest;

    t = item_template(template_id);
    m = material(material_id);
    if (!t || !m || !comes_in(template_id, material_id)) {
        return 0;
    }
    mem_zero(it, sizeof(*it));
    it->template_id = t->id;
    it->material = (df_u8)m->id;
    it->count = 1;
    it->value = (df_u32)t->price * m->value_mul * m->value_q;
    /*
     * DFU's CalculateWeightForMaterial: whole quarter kilograms times the
     * material's quarters, then Mathf.Round, which rounds half to even
     * (a steel longsword's 22.5 quarters are 22, 5.5 kg).
     */
    quarters = (int)(t->weight * 4.0f + 0.001f);
    n = quarters * m->weight_q;
    q = n / 4;
    r = n % 4;
    if (r > 2 || (r == 2 && (q & 1))) {
        q++;
    }
    it->weight = (float)q * 0.25f;
    it->condition_max = (df_u16)((df_u32)t->hit_points * m->condition_q / 4u);
    it->condition = it->condition_max;
    /* A new piece shows its lowest variant; loot rolls one of the rest (loot.c). */
    item_variants(it, &lowest, &highest);
    it->variant = (df_u8)lowest;
    return 1;
}

void item_variants(const Item *it, int *low, int *high)
{
    const ArmorTemplate *a;

    *low = 0;
    *high = 0;
    a = armor_template(it->template_id);
    if (!a) {
        return;
    }
    if (it->material == MATERIAL_LEATHER) {
        *low = a->leather_low;
        *high = a->leather_high;
    } else if (it->material == MATERIAL_CHAIN) {
        *low = a->chain_low;
        *high = a->chain_high;
    } else {
        *low = a->plate_low;
        *high = a->plate_high;
    }
}

/* DFU's body shapes, as an archive's distance from its sex's first: the wearer of armour's race. */
static int body_shape(int race)
{
    switch (race) {
    case RACE_ARGONIAN:
        return 0;
    case RACE_DARK_ELF:
    case RACE_HIGH_ELF:
    case RACE_WOOD_ELF:
        return 1;
    case RACE_KHAJIIT:
        return 3;
    default:
        return 2;
    }
}

int item_metal(const Item *it)
{
    if ((!weapon_template(it->template_id) && !armor_template(it->template_id)) || it->material > MATERIAL_DAEDRIC) {
        return -1;
    }
    return it->material;
}

int item_picture(const Item *it, int race, int female, int *archive, int *record)
{
    const ItemTemplate *t;

    t = item_template(it->template_id);
    if (!t) {
        return 0;
    }
    if (armor_template(it->template_id)) {
        *archive = (female ? ARCHIVE_FEMALE : ARCHIVE_MALE) + body_shape(race);
        *record = t->player_record + it->variant;
    } else if (t->player_archive) {
        *archive = t->player_archive;
        *record = t->player_record + it->variant;
    } else {
        *archive = t->world_archive;
        *record = t->world_record;
    }
    return 1;
}

void item_name(const Item *it, char *out, int cap)
{
    const ItemTemplate *t;
    const Material *m;

    t = item_template(it->template_id);
    m = material(it->material);
    if (it->template_id == ITEM_GOLD || it->template_id == ITEM_ARROW) {
        log_format(out, cap, "%s", t ? t->name : "?");
        return;
    }
    log_format(out, cap, "%s %s", m ? m->name : "?", t ? t->name : "?");
}

/* a and b alike but for case, spaces and dashes. */
static int same_name(const char *a, const char *b)
{
    for (;;) {
        char ca;
        char cb;

        while (*a == ' ' || *a == '-') {
            a++;
        }
        while (*b == ' ' || *b == '-') {
            b++;
        }
        ca = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a;
        cb = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;
        if (ca != cb) {
            return 0;
        }
        if (!ca) {
            return 1;
        }
        a++;
        b++;
    }
}

int item_find_weapon(const char *name)
{
    RecTable *t;
    int i;

    t = item_table();
    for (i = 0; i < t->count; i++) {
        const ItemTemplate *it;

        it = (const ItemTemplate *)(t->rows + (df_u32)i * t->schema->row_size);
        if (weapon_template(it->id) && same_name(it->name, name)) {
            return it->id;
        }
    }
    return -1;
}

int item_find_gear(const char *name)
{
    RecTable *t;
    int i;

    t = item_table();
    for (i = 0; i < t->count; i++) {
        const ItemTemplate *it;

        it = (const ItemTemplate *)(t->rows + (df_u32)i * t->schema->row_size);
        if ((weapon_template(it->id) || armor_template(it->id)) && same_name(it->name, name)) {
            return it->id;
        }
    }
    return -1;
}

int item_find_material(const char *name)
{
    RecTable *t;
    int i;

    t = material_table();
    for (i = 0; i < t->count; i++) {
        const Material *m;

        m = (const Material *)(t->rows + (df_u32)i * t->schema->row_size);
        if (same_name(m->name, name)) {
            return m->id;
        }
    }
    return -1;
}

void inventory_clear(Inventory *inv)
{
    int place;

    inv->count = 0;
    inv->gold = 0;
    inv->use_left = 0;
    for (place = 0; place < EQUIP_COUNT; place++) {
        inv->worn[place] = -1;
    }
}

int inventory_add(Inventory *inv, const Item *it)
{
    if (inv->count >= INVENTORY_MAX) {
        return -1;
    }
    inv->item[inv->count] = *it;
    return inv->count++;
}

void inventory_remove(Inventory *inv, int i)
{
    int place;

    if (i < 0 || i >= inv->count) {
        return;
    }
    /* The worn places follow their items down the list. */
    for (place = 0; place < EQUIP_COUNT; place++) {
        if (inv->worn[place] == i) {
            inv->worn[place] = -1;
        } else if (inv->worn[place] > i) {
            inv->worn[place]--;
        }
    }
    inv->count--;
    for (; i < inv->count; i++) {
        inv->item[i] = inv->item[i + 1];
    }
}

/* The place an armour piece is worn: a shield in the left hand, the rest by the part they cover. */
static int place_of_armor(const ArmorTemplate *a)
{
    if (a->shield) {
        return EQUIP_LEFT_HAND;
    }
    switch (a->part) {
    case BODY_HEAD:
        return EQUIP_HEAD;
    case BODY_RIGHT_ARM:
        return EQUIP_RIGHT_ARM;
    case BODY_LEFT_ARM:
        return EQUIP_LEFT_ARM;
    case BODY_CHEST:
        return EQUIP_CHEST;
    case BODY_HANDS:
        return EQUIP_GLOVES;
    case BODY_LEGS:
        return EQUIP_LEGS;
    default:
        return EQUIP_FEET;
    }
}

/* What is in hand takes both: a template that is not one-handed. */
static int is_two_handed(const Inventory *inv, int i)
{
    const ItemTemplate *t;

    if (i < 0) {
        return 0;
    }
    t = item_template(inv->item[i].template_id);
    return t && !t->one_handed;
}

int inventory_equip(Inventory *inv, int i)
{
    if (i < 0) {
        inv->worn[EQUIP_RIGHT_HAND] = -1;
        return 1;
    }
    if (i >= inv->count || !weapon_template(inv->item[i].template_id) || item_broken(&inv->item[i])) {
        return 0;
    }
    inv->worn[EQUIP_RIGHT_HAND] = i;
    if (is_two_handed(inv, i)) {
        inv->worn[EQUIP_LEFT_HAND] = -1;
    }
    return 1;
}

int inventory_wear(Inventory *inv, int i)
{
    const ArmorTemplate *a;
    int place;

    if (i < 0 || i >= inv->count) {
        return 0;
    }
    a = armor_template(inv->item[i].template_id);
    if (!a) {
        /* A one-handed weapon takes a free left hand when the right is full; anything else the right. */
        if (!is_two_handed(inv, i) && weapon_template(inv->item[i].template_id) && !item_broken(&inv->item[i])
            && inv->worn[EQUIP_RIGHT_HAND] >= 0 && inv->worn[EQUIP_LEFT_HAND] < 0
            && !is_two_handed(inv, inv->worn[EQUIP_RIGHT_HAND])) {
            inv->worn[EQUIP_LEFT_HAND] = i;
            return 1;
        }
        return inventory_equip(inv, i);
    }
    if (item_broken(&inv->item[i])) {
        return 0;
    }
    place = place_of_armor(a);
    inv->worn[place] = i;
    if (place == EQUIP_LEFT_HAND) {
        /* A shield is no weapon: the right hand is the one in use. */
        inv->use_left = 0;
        if (is_two_handed(inv, inv->worn[EQUIP_RIGHT_HAND])) {
            inv->worn[EQUIP_RIGHT_HAND] = -1;
        }
    }
    return 1;
}

void inventory_take_off(Inventory *inv, int place)
{
    if (place >= 0 && place < EQUIP_COUNT) {
        inv->worn[place] = -1;
    }
}

int inventory_worn_at(const Inventory *inv, int i)
{
    int place;

    for (place = 0; place < EQUIP_COUNT; place++) {
        if (inv->worn[place] == i) {
            return place;
        }
    }
    return -1;
}

int inventory_swap_hands(Inventory *inv)
{
    int left;

    left = inv->worn[EQUIP_LEFT_HAND];
    if (left >= 0 && armor_template(inv->item[left].template_id)) {
        return 0;
    }
    inv->use_left = !inv->use_left;
    return 1;
}

int inventory_hand_place(const Inventory *inv)
{
    return inv->use_left ? EQUIP_LEFT_HAND : EQUIP_RIGHT_HAND;
}

const Item *inventory_weapon(const Inventory *inv)
{
    int i;

    i = inv->worn[inventory_hand_place(inv)];
    return i >= 0 && i < inv->count ? &inv->item[i] : 0;
}

Item *inventory_hand(Inventory *inv)
{
    int i;

    i = inv->worn[inventory_hand_place(inv)];
    return i >= 0 && i < inv->count ? &inv->item[i] : 0;
}

int item_wear_down(Item *it, int damage, int roll)
{
    int loss;

    loss = (10 * damage + 50) / 100;
    if (loss < 1 && roll < 20) {
        loss = 1;
    }
    it->condition = (df_u16)(loss >= it->condition ? 0 : it->condition - loss);
    return item_broken(it);
}

int item_broken(const Item *it)
{
    return it->condition_max > 0 && it->condition < 1;
}

int inventory_armor_at_part(const Inventory *inv, int part)
{
    static const df_u8 place_of_part[BODY_PARTS] = {
        EQUIP_HEAD, EQUIP_RIGHT_ARM, EQUIP_LEFT_ARM, EQUIP_CHEST, EQUIP_GLOVES, EQUIP_LEGS, EQUIP_FEET
    };
    int shield;

    /* A shield comes first, over the part it covers (FormulaHelper's DamageEquipment). */
    shield = inv->worn[EQUIP_LEFT_HAND];
    if (shield >= 0) {
        const ArmorTemplate *a;

        a = armor_template(inv->item[shield].template_id);
        if (a && a->shield && (a->shield_parts & (1 << part))) {
            return EQUIP_LEFT_HAND;
        }
    }
    return inv->worn[place_of_part[part]] >= 0 ? place_of_part[part] : -1;
}

int inventory_arrows(const Inventory *inv)
{
    int i;

    for (i = 0; i < inv->count; i++) {
        if (inv->item[i].template_id == ITEM_ARROW && inv->item[i].count > 0) {
            return i;
        }
    }
    return -1;
}

int inventory_spend_arrow(Inventory *inv)
{
    int i;

    i = inventory_arrows(inv);
    if (i < 0) {
        return 0;
    }
    inv->item[i].count--;
    if (!inv->item[i].count) {
        inventory_remove(inv, i);
    }
    return 1;
}

float inventory_weight(const Inventory *inv)
{
    float kg;
    int i;

    kg = (float)inv->gold * GOLD_KG;
    for (i = 0; i < inv->count; i++) {
        kg += inv->item[i].weight * (float)inv->item[i].count;
    }
    return kg;
}

void inventory_armor(const Inventory *inv, int *out)
{
    int part;
    int place;

    for (part = 0; part < BODY_PARTS; part++) {
        out[part] = ARMOR_BARE;
    }
    for (place = 0; place < EQUIP_COUNT; place++) {
        const ArmorTemplate *a;
        const Item *it;

        if (inv->worn[place] < 0) {
            continue;
        }
        it = &inv->item[inv->worn[place]];
        a = armor_template(it->template_id);
        if (!a) {
            continue;
        }
        if (a->shield) {
            /* A shield's armour is its own, whatever it is made of. */
            for (part = 0; part < BODY_PARTS; part++) {
                if (a->shield_parts & (1 << part)) {
                    out[part] -= a->shield * ARMOR_POINT;
                }
            }
        } else {
            const Material *m;

            m = material(it->material);
            out[a->part] -= (m ? m->armor : 0) * ARMOR_POINT;
        }
    }
}
