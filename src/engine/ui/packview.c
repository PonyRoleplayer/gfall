#include "engine/ui/packview.h"
#include "engine/core/log.h"

/* See packview.h. */

/* The cursor inside its pack, and the rows shown round it. */
static void settle(PackView *v, int side)
{
    const Pack *p;
    int n;

    p = v->pack[side];
    n = p ? p->count : 0;
    if (v->cursor[side] >= n) {
        v->cursor[side] = n - 1;
    }
    if (v->cursor[side] < 0) {
        v->cursor[side] = 0;
    }
    if (v->top[side] > v->cursor[side]) {
        v->top[side] = v->cursor[side];
    }
    if (v->rows > 0 && v->cursor[side] >= v->top[side] + v->rows) {
        v->top[side] = v->cursor[side] - v->rows + 1;
    }
    /* A list grown shorter scrolls back: no empty rows while things sit above them. */
    if (v->rows > 0 && v->top[side] + v->rows > n) {
        v->top[side] = n > v->rows ? n - v->rows : 0;
    }
}

void packview_open(PackView *v, Pack *left, Pack *right, int rows)
{
    v->pack[0] = left;
    v->pack[1] = right;
    v->side = 0;
    v->cursor[0] = 0;
    v->cursor[1] = 0;
    v->top[0] = 0;
    v->top[1] = 0;
    v->rows = rows;
}

int packview_command(PackView *v, int command)
{
    Pack *p;
    Pack *other;
    int at;
    int changed;

    p = v->pack[v->side];
    other = v->pack[1 - v->side];
    at = v->cursor[v->side];
    changed = 0;
    if (!p) {
        return 0;
    }
    switch (command) {
    case PACKVIEW_UP:
        changed = at > 0;
        v->cursor[v->side] = at - 1;
        break;
    case PACKVIEW_DOWN:
        changed = at + 1 < p->count;
        v->cursor[v->side] = at + 1;
        break;
    case PACKVIEW_SWITCH:
        if (other) {
            v->side = 1 - v->side;
            changed = 1;
        }
        break;
    case PACKVIEW_USE: {
        int slot;

        slot = pack_worn(p, at);
        if (slot >= 0) {
            pack_unequip(p, slot);
            changed = 1;
        } else {
            changed = pack_equip(p, at);
        }
        break;
    }
    case PACKVIEW_MOVE:
        changed = other && pack_move(p, at, 1, other) > 0;
        break;
    case PACKVIEW_MOVE_ALL:
        if (other) {
            df_u32 money;

            money = p->money;
            changed = pack_move_all(p, other) > 0 || money > 0;
        }
        break;
    default:
        break;
    }
    settle(v, 0);
    settle(v, 1);
    return changed;
}

int packview_line(const PackView *v, int side, int row, char *out, int cap)
{
    const Pack *p;
    const PackItem *it;
    const PackStack *s;
    int i;

    p = v->pack[side];
    i = v->top[side] + row;
    if (!p || row < 0 || i >= p->count) {
        return 0;
    }
    s = &p->stack[i];
    it = pack_item(p, s->item);
    log_format(out, cap, "%c%c%s x%d  %.1f", (side == v->side && i == v->cursor[side]) ? '>' : ' ',
               pack_worn(p, i) >= 0 ? '*' : ' ', it ? it->name : "?", (int)s->count,
               it ? it->weight * (float)s->count : 0.0f);
    return 1;
}

void packview_heading(const PackView *v, int side, char *out, int cap)
{
    const Pack *p;

    p = v->pack[side];
    out[0] = 0;
    if (p) {
        log_format(out, cap, "Weight %.1f/%.1f  Money %u", pack_weight(p), p->max_weight, (unsigned)p->money);
    }
}

void packview_draw(const PackView *v, int x, int top, int width, int line_h, PackViewText text)
{
    int side;

    for (side = 0; side < 2; side++) {
        char line[PACKVIEW_LINE];
        int left;
        int row;

        if (!v->pack[side]) {
            continue;
        }
        left = x + side * (width / 2);
        packview_heading(v, side, line, (int)sizeof(line));
        text(left, top, line);
        /* The screen's y counts up: each line lower than the last. */
        for (row = 0; row < v->rows && packview_line(v, side, row, line, (int)sizeof(line)); row++) {
            text(left, top - (row + 1) * line_h, line);
        }
    }
}
