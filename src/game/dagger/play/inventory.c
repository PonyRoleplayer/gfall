#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See inventory.h. */

int item_make(Item *it, int template_id, int material_id)
{
    const ItemTemplate *t;
    const Material *m;
    int quarters;
    int n;
    int q;
    int r;

    t = item_template(template_id);
    m = material(material_id);
    if (!t || !m) {
        return 0;
    }
    mem_zero(it, sizeof(*it));
    it->template_id = t->id;
    it->material = (df_u8)m->id;
    it->value = (df_u32)t->price * 3u * m->value_q;
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
    return 1;
}

void item_name(const Item *it, char *out, int cap)
{
    const ItemTemplate *t;
    const Material *m;

    t = item_template(it->template_id);
    m = material(it->material);
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
    inv->count = 0;
    inv->weapon = -1;
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
    if (i < 0 || i >= inv->count) {
        return;
    }
    if (inv->weapon == i) {
        inv->weapon = -1;
    } else if (inv->weapon > i) {
        inv->weapon--;
    }
    inv->count--;
    for (; i < inv->count; i++) {
        inv->item[i] = inv->item[i + 1];
    }
}

int inventory_equip(Inventory *inv, int i)
{
    if (i < 0) {
        inv->weapon = -1;
        return 1;
    }
    if (i >= inv->count || !weapon_template(inv->item[i].template_id)) {
        return 0;
    }
    inv->weapon = i;
    return 1;
}

const Item *inventory_weapon(const Inventory *inv)
{
    return inv->weapon >= 0 && inv->weapon < inv->count ? &inv->item[inv->weapon] : 0;
}

float inventory_weight(const Inventory *inv)
{
    float kg;
    int i;

    kg = 0.0f;
    for (i = 0; i < inv->count; i++) {
        kg += inv->item[i].weight;
    }
    return kg;
}
