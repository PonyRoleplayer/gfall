#include "game/dagger/play/pile.h"
#include "game/dagger/places/dunstream.h"
#include "engine/scene/keep.h"
#include "engine/scene/stream.h"

/* See pile.h. */

/* Dropping this near a pile joins it: about the width of the heap itself. */
#define JOIN_DISTANCE 48.0f

typedef struct PileItem {
    KeepKey pile;
    Item item;
} PileItem;

static PileItem g_pool[PILE_ITEMS];
static int g_used;

void pile_new_game(void)
{
    g_used = 0;
}

static int same_key(KeepKey a, KeepKey b)
{
    return a.place == b.place && a.thing == b.thing;
}

/* How many items of the pool belong to piles other than e. */
static int items_elsewhere(const Entity *e)
{
    int n;
    int i;

    n = 0;
    for (i = 0; i < g_used; i++) {
        n += !e || !same_key(g_pool[i].pile, e->key);
    }
    return n;
}

int pile_room(const Entity *e, int count)
{
    return items_elsewhere(e) + count <= PILE_ITEMS;
}

void pile_read(const Entity *e, Inventory *out)
{
    int i;

    inventory_clear(out);
    for (i = 0; i < g_used; i++) {
        if (same_key(g_pool[i].pile, e->key)) {
            inventory_add(out, &g_pool[i].item);
        }
    }
}

/* Takes pile e's items out of the pool, closing the gaps. */
static void clear_items(const Entity *e)
{
    int kept;
    int i;

    kept = 0;
    for (i = 0; i < g_used; i++) {
        if (!same_key(g_pool[i].pile, e->key)) {
            g_pool[kept++] = g_pool[i];
        }
    }
    g_used = kept;
}

/* Adds things to pile e's items, as many as the pool holds. */
static void add_items(const Entity *e, const Inventory *things)
{
    int i;

    for (i = 0; i < things->count && g_used < PILE_ITEMS; i++) {
        g_pool[g_used].pile = e->key;
        g_pool[g_used].item = things->item[i];
        g_used++;
    }
}

/* A spare thing used or not: drawn, and its keep row says so, so it comes back drawn with its block. */
static void set_used(Entity *e, int used)
{
    Keep *row;

    e->live = (df_u8)used;
    row = keep_get(e->key, KEEP_ENTITY);
    if (row) {
        row->game[DUNSTREAM_SPARE_USED] = (df_u8)used;
    }
}

void pile_write(Entity *e, const Inventory *things)
{
    clear_items(e);
    add_items(e, things);
    if (!things->count) {
        set_used(e, 0);
    }
}

/* The ground between a spare thing and feet. */
static float ground_distance(const Entity *e, const float *feet)
{
    float dx;
    float dz;

    dx = e->x - feet[0];
    dz = e->z - feet[2];
    return df_sqrt(dx * dx + dz * dz);
}

/*
 * The nearest spare thing in use and the nearest free, over the loaded
 * blocks. A free one stands at its block's middle or where it last lay, so
 * the nearest is in the block the player is in, or near enough.
 */
static void nearest_spares(const float *feet, Entity **used, Entity **free_one)
{
    float used_at;
    float free_at;
    int slot;

    *used = 0;
    *free_one = 0;
    used_at = 0.0f;
    free_at = 0.0f;
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            float distance;

            if (!dunstream_spare(&ents[i])) {
                continue;
            }
            distance = ground_distance(&ents[i], feet);
            if (ents[i].live) {
                if (!*used || distance < used_at) {
                    *used = &ents[i];
                    used_at = distance;
                }
            } else if (!*free_one || distance < free_at) {
                *free_one = &ents[i];
                free_at = distance;
            }
        }
    }
}

int pile_drop(const Inventory *things, const float *feet)
{
    Entity *used;
    Entity *free_one;
    Entity *pile;

    nearest_spares(feet, &used, &free_one);
    if (used && (!free_one || ground_distance(used, feet) < JOIN_DISTANCE)) {
        pile = used;
    } else if (free_one) {
        pile = free_one;
        pile->x = feet[0];
        pile->y = feet[1];
        pile->z = feet[2];
        set_used(pile, 1);
    } else {
        return 0;
    }
    add_items(pile, things);
    return 1;
}
