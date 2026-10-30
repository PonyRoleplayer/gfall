#include "engine/scene/pack.h"

/* See pack.h. */

void pack_init(Pack *p, const PackItem *items, int nitems, float max_weight)
{
    int i;

    p->items = items;
    p->nitems = nitems;
    p->count = 0;
    p->max_weight = max_weight;
    p->money = 0;
    for (i = 0; i < PACK_SLOTS; i++) {
        p->worn[i] = -1;
    }
}

void pack_bind(Pack *p, const PackItem *items, int nitems)
{
    p->items = items;
    p->nitems = nitems;
}

const PackItem *pack_item(const Pack *p, int id)
{
    int i;

    for (i = 0; i < p->nitems; i++) {
        if (p->items[i].id == id) {
            return &p->items[i];
        }
    }
    return 0;
}

float pack_weight(const Pack *p)
{
    float kg;
    int i;

    kg = 0.0f;
    for (i = 0; i < p->count; i++) {
        const PackItem *it;

        it = pack_item(p, p->stack[i].item);
        if (it) {
            kg += it->weight * (float)p->stack[i].count;
        }
    }
    return kg;
}

int pack_worn(const Pack *p, int i)
{
    int s;

    for (s = 0; s < PACK_SLOTS; s++) {
        if (p->worn[s] == i) {
            return s;
        }
    }
    return -1;
}

int pack_add(Pack *p, int id, int count, df_u32 tag)
{
    const PackItem *it;
    int most;
    int left;
    int i;

    it = pack_item(p, id);
    if (!it || count <= 0) {
        return 0;
    }
    /* What the weight limit lets in. */
    if (p->max_weight > 0.0f && it->weight > 0.0f) {
        most = (int)((p->max_weight - pack_weight(p)) / it->weight + 0.0001f);
        if (count > most) {
            count = most;
        }
        if (count <= 0) {
            return 0;
        }
    }
    left = count;
    for (i = 0; i < p->count && left > 0; i++) {
        PackStack *s;
        int room;

        s = &p->stack[i];
        if (s->item != it->id || s->tag != tag || pack_worn(p, i) >= 0) {
            continue;
        }
        room = (int)it->stack_max - (int)s->count;
        if (room > 0) {
            room = room > left ? left : room;
            s->count = (df_u16)(s->count + room);
            left -= room;
        }
    }
    while (left > 0 && p->count < PACK_MAX) {
        PackStack *s;
        int n;

        n = it->stack_max < 1 ? 1 : (int)it->stack_max;
        n = n > left ? left : n;
        s = &p->stack[p->count++];
        s->item = it->id;
        s->count = (df_u16)n;
        s->tag = tag;
        left -= n;
    }
    return count - left;
}

int pack_remove(Pack *p, int i, int count)
{
    int s;
    int k;

    if (i < 0 || i >= p->count || count <= 0) {
        return 0;
    }
    if (count < (int)p->stack[i].count) {
        p->stack[i].count = (df_u16)(p->stack[i].count - count);
        return count;
    }
    count = p->stack[i].count;
    /* The stack goes: its slot comes empty, and the worn ones after it move down one. */
    for (s = 0; s < PACK_SLOTS; s++) {
        if (p->worn[s] == i) {
            p->worn[s] = -1;
        } else if (p->worn[s] > i) {
            p->worn[s]--;
        }
    }
    p->count--;
    for (k = i; k < p->count; k++) {
        p->stack[k] = p->stack[k + 1];
    }
    return count;
}

int pack_move(Pack *from, int i, int count, Pack *to)
{
    int moved;

    if (i < 0 || i >= from->count || from == to) {
        return 0;
    }
    if (count > (int)from->stack[i].count) {
        count = from->stack[i].count;
    }
    moved = pack_add(to, from->stack[i].item, count, from->stack[i].tag);
    return moved > 0 ? pack_remove(from, i, moved) : 0;
}

int pack_move_all(Pack *from, Pack *to)
{
    int moved;
    int i;

    if (from == to) {
        return 0;
    }
    moved = 0;
    /* From the end, so a stack that goes moves none still to come. */
    for (i = from->count - 1; i >= 0; i--) {
        moved += pack_move(from, i, from->stack[i].count, to);
    }
    to->money += from->money;
    from->money = 0;
    return moved;
}

int pack_equip(Pack *p, int i)
{
    const PackItem *it;

    if (i < 0 || i >= p->count) {
        return 0;
    }
    it = pack_item(p, p->stack[i].item);
    if (!it || it->slot < 0 || it->slot >= PACK_SLOTS) {
        return 0;
    }
    if (p->stack[i].count > 1) {
        if (p->count >= PACK_MAX) {
            return 0;
        }
        p->stack[p->count] = p->stack[i];
        p->stack[p->count].count = 1;
        p->stack[i].count--;
        i = p->count++;
    }
    p->worn[(int)it->slot] = (signed char)i;
    return 1;
}

void pack_unequip(Pack *p, int slot)
{
    if (slot >= 0 && slot < PACK_SLOTS) {
        p->worn[slot] = -1;
    }
}

int pack_stat(const Pack *p, int k)
{
    int sum;
    int s;

    sum = 0;
    if (k < 0 || k >= PACK_STATS) {
        return 0;
    }
    for (s = 0; s < PACK_SLOTS; s++) {
        if (p->worn[s] >= 0) {
            const PackItem *it;

            it = pack_item(p, p->stack[(int)p->worn[s]].item);
            if (it) {
                sum += it->stat[k];
            }
        }
    }
    return sum;
}

int pack_pay(Pack *p, df_u32 amount)
{
    if (amount > p->money) {
        return 0;
    }
    p->money -= amount;
    return 1;
}
