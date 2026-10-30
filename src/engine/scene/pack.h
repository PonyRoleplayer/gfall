#ifndef ENG_PACK_H
#define ENG_PACK_H

#include "engine/core/df.h"

/*
 * What someone or something carries: a player's bag, a chest, a body.
 * Read docs/pack.md before editing. The engine knows no game's items:
 * a game hands over a catalogue of PackItem, from its own data, and
 * reads what the pack adds up to (pack_stat) in its own rules.
 *
 * A pack is stacks of catalogue items, a weight limit, money, and the
 * stacks worn in its slots. Two stacks of one item stay apart when their
 * tags differ: a tag is the game's own number for one thing's state (its
 * condition, its enchantment). A worn stack is always one thing, and
 * nothing merges into it.
 *
 * A Pack holds no pointer but its catalogue, so it can be saved as bytes;
 * pack_bind gives it the catalogue again after loading.
 *
 * Plain C, no Glide: hostcheck runs it.
 */

#define PACK_MAX 64
#define PACK_SLOTS 16
#define PACK_STATS 4
/* PackItem.slot of what cannot be worn. */
#define PACK_NO_SLOT (-1)

typedef struct PackItem {
    df_u16 id;
    char name[24];
    /* Each, in the game's unit. */
    float weight;
    /* The most in one stack, at least 1. */
    df_u16 stack_max;
    /* Where it is worn, 0..PACK_SLOTS-1, or PACK_NO_SLOT. */
    signed char slot;
    /* What wearing it adds: the game says what each means. */
    df_i16 stat[PACK_STATS];
} PackItem;

typedef struct PackStack {
    df_u16 item;
    df_u16 count;
    df_u32 tag;
} PackStack;

typedef struct Pack {
    const PackItem *items;
    int nitems;
    PackStack stack[PACK_MAX];
    int count;
    /* Each slot's stack, or -1. */
    signed char worn[PACK_SLOTS];
    /* The most it holds; 0 for no limit. */
    float max_weight;
    df_u32 money;
} Pack;

/* Empty, under this catalogue (the caller keeps it) and weight limit. */
void pack_init(Pack *p, const PackItem *items, int nitems, float max_weight);
/* The catalogue again, after loading a Pack from bytes. */
void pack_bind(Pack *p, const PackItem *items, int nitems);
/* The catalogue's row for id, or 0. */
const PackItem *pack_item(const Pack *p, int id);
/* Everything in it, by weight. */
float pack_weight(const Pack *p);
/* Up to count of item id: into its stacks of this tag, then new ones. How many fit. */
int pack_add(Pack *p, int id, int count, df_u32 tag);
/* Up to count from stack i, worn or not; the stack goes when empty. How many left. */
int pack_remove(Pack *p, int i, int count);
/* Up to count from stack i of one pack to another with the same catalogue. How many moved. */
int pack_move(Pack *from, int i, int count, Pack *to);
/* All it can from one pack to another, its money too: looting a body. How many things moved. */
int pack_move_all(Pack *from, Pack *to);
/*
 * Stack i worn in its item's slot, whatever was there coming off. From a
 * stack of several, one is split off to wear. 0 when it cannot be worn
 * or there is no room to split.
 */
int pack_equip(Pack *p, int i);
/* Off slot, staying in the pack. */
void pack_unequip(Pack *p, int slot);
/* The slot stack i is worn in, or -1. */
int pack_worn(const Pack *p, int i);
/* Stat k added up over everything worn. */
int pack_stat(const Pack *p, int k);
/* Money out: 0, and nothing taken, when there is not enough. */
int pack_pay(Pack *p, df_u32 amount);

#endif
