#include "game/dagger/play/loot.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/pile.h"
#include "game/dagger/places/dunstream.h"
#include "engine/scene/keep.h"
#include "engine/scene/stream.h"

/* See loot.h. */

/* How far a body can be searched, from the eye, in units: 3.2 m, our choice, a little past a sword's reach. */
#define SEARCH_REACH 128.0f
/* A body's picture can be thin: the view need come this near its middle. */
#define SEARCH_HALF_MIN 16.0f

#define MAX_ARROWS 20

/* RandomArmorMaterial's rolls out of 100: leather up to this, chain up to the next, and plate past it. */
#define ARMOR_ROLL_LEATHER 69
#define ARMOR_ROLL_CHAIN 89

/* A gold coin's weight, the template's (item_make's whole quarter kilograms cannot hold it). */
#define GOLD_KG 0.0025f

/* ItemBuilder.materialsByModifier: how much of the roll each material takes, iron first. */
static const df_u8 g_metal_share[MATERIAL_DAEDRIC + 1] = { 64, 128, 10, 21, 13, 8, 5, 3, 2, 5 };

static df_u32 g_game_seed = 1u;

void loot_new_game(df_u32 seed)
{
    g_game_seed = seed;
}

df_u32 loot_seed(void)
{
    return g_game_seed;
}

/*
 * A stream of dice of a body's own. It is not the game's dice
 * (combat_roll): a body's contents must not depend on what else was rolled
 * before it was opened. Xorshift, three shifts a number: quick on the 233,
 * and good enough for a pile of swords.
 */
typedef struct Dice {
    df_u32 state;
} Dice;

/* Murmur3's finisher: two near keys, next door in a block, start far apart. */
static df_u32 mix(df_u32 h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

static void dice_seed(Dice *d, df_u32 seed)
{
    /* Xorshift never leaves 0. */
    d->state = mix(seed) | 1u;
}

/* 0..n-1. */
static int dice_roll(Dice *d, int n)
{
    df_u32 x;

    x = d->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    d->state = x;
    return n > 1 ? (int)(x % (df_u32)n) : 0;
}

/* low..high, both in. */
static int dice_between(Dice *d, int low, int high)
{
    return high <= low ? low : low + dice_roll(d, high - low + 1);
}

int loot_metal(int level, int roll)
{
    int value;
    int metal;

    /* RandomMaterial: past level 10 a level adds 2 to the roll, below it each takes 4 off. */
    value = level - 10;
    value = value >= 0 ? value * 2 : value * 4;
    value += roll;
    value = value < 0 ? 0 : (value > 256 ? 256 : value);
    /* Each material takes its share of the roll in turn; what the roll stops in is the one. */
    for (metal = 0; metal < MATERIAL_DAEDRIC; metal++) {
        if (value <= g_metal_share[metal]) {
            return metal;
        }
        value -= g_metal_share[metal];
    }
    return MATERIAL_DAEDRIC;
}

/* Adds an item to a body, up to what it can hold. */
static void add_thing(Inventory *out, const Item *it)
{
    if (out->count < LOOT_MAX) {
        inventory_add(out, it);
    }
}

static void add_gold(Inventory *out, int coins)
{
    Item it;

    if (coins > 0 && item_make(&it, ITEM_GOLD, MATERIAL_IRON)) {
        it.count = (df_u16)(coins > 0xffff ? 0xffff : coins);
        it.weight = GOLD_KG;
        it.value = 1u;
        add_thing(out, &it);
    }
}

/* A random weapon (ItemBuilder.CreateRandomWeapon): any of them alike, its material by the level. Arrows come in a stack. */
static void add_weapon(Inventory *out, int level, Dice *d)
{
    RecTable *t;
    const ItemTemplate *row;
    Item it;
    int kinds;
    int pick;
    int i;

    t = item_table();
    kinds = weapon_table()->count + 1;
    pick = dice_roll(d, kinds);
    for (i = 0; i < t->count; i++) {
        row = (const ItemTemplate *)(t->rows + (df_u32)i * t->schema->row_size);
        if (!weapon_template(row->id) && row->id != ITEM_ARROW) {
            continue;
        }
        if (pick-- == 0) {
            if (item_make(&it, row->id, loot_metal(level, dice_roll(d, 256)))) {
                if (row->id == ITEM_ARROW) {
                    it.count = (df_u16)dice_between(d, 1, MAX_ARROWS);
                }
                add_thing(out, &it);
            }
            return;
        }
    }
}

/* A random piece of armour (CreateRandomArmor): any of them alike, leather or chain most often, a variant of those its material has. */
static void add_armor(Inventory *out, int level, Dice *d)
{
    RecTable *t;
    const ArmorTemplate *row;
    Item it;
    int kind;
    int roll;
    int metal;
    int low;
    int high;

    t = armor_table();
    kind = dice_roll(d, t->count);
    row = (const ArmorTemplate *)(t->rows + (df_u32)kind * t->schema->row_size);
    roll = dice_between(d, 1, 100);
    if (roll <= ARMOR_ROLL_LEATHER) {
        metal = MATERIAL_LEATHER;
    } else if (roll <= ARMOR_ROLL_CHAIN) {
        metal = MATERIAL_CHAIN;
    } else {
        metal = loot_metal(level, dice_roll(d, 256));
    }
    if (item_make(&it, row->id, metal)) {
        item_variants(&it, &low, &high);
        it.variant = (df_u8)dice_between(d, low, high);
        add_thing(out, &it);
    }
}

void loot_roll(Inventory *out, const LootChance *table, int level, df_u32 seed)
{
    Dice d;
    int chance;

    inventory_clear(out);
    if (!table) {
        return;
    }
    dice_seed(&d, seed);
    add_gold(out, dice_between(&d, table->gold_min, table->gold_max) * level);
    /* Each success rolls again at half the chance (GenerateRandomLoot's order: weapons, then armour). */
    for (chance = table->weapon; chance > 0 && dice_roll(&d, 100) < chance; chance /= 2) {
        add_weapon(out, level, &d);
    }
    for (chance = table->armor; chance > 0 && dice_roll(&d, 100) < chance; chance /= 2) {
        add_armor(out, level, &d);
    }
}

/*
 * Whether the view from eye along unit dir passes through the box round
 * a body's picture, and how far along: the slab test, an axis at a time.
 */
static int ray_meets_box(const float *eye, const float *dir, const float *low, const float *high, float *t_out)
{
    float t_near;
    float t_far;
    int axis;

    t_near = 0.0f;
    t_far = SEARCH_REACH;
    for (axis = 0; axis < 3; axis++) {
        float t0;
        float t1;

        if (dir[axis] > -0.0001f && dir[axis] < 0.0001f) {
            /* Along this axis the view does not move: it is inside the slab or never will be. */
            if (eye[axis] < low[axis] || eye[axis] > high[axis]) {
                return 0;
            }
            continue;
        }
        t0 = (low[axis] - eye[axis]) / dir[axis];
        t1 = (high[axis] - eye[axis]) / dir[axis];
        if (t0 > t1) {
            float swap;

            swap = t0;
            t0 = t1;
            t1 = swap;
        }
        t_near = t0 > t_near ? t0 : t_near;
        t_far = t1 < t_far ? t1 : t_far;
        if (t_near > t_far) {
            return 0;
        }
    }
    *t_out = t_near;
    return 1;
}

/* Whether the view can come to rest on this entity: a body, a heap, or a pile put down. */
static int searchable(const Entity *e)
{
    if (!e->live) {
        return 0;
    }
    if (e->dead) {
        return dunstream_kind(e, 0) >= 0;
    }
    return dunstream_treasure(e) || dunstream_spare(e);
}

Entity *loot_find(const Scene *scene, const Camera *eye)
{
    Entity *best;
    float best_t;
    float from[3];
    float dir[3];
    Basis basis;
    int slot;

    best = 0;
    best_t = SEARCH_REACH;
    camera_basis(eye, &basis);
    from[0] = eye->x;
    from[1] = eye->y;
    from[2] = eye->z;
    /* The basis's forward is on the ground; the view is that tipped by the pitch. */
    dir[0] = basis.fx * basis.cp;
    dir[1] = basis.sp;
    dir[2] = basis.fz * basis.cp;
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            Entity *e;
            const EntityPose *pose;
            float low[3];
            float high[3];
            float half;
            float t;

            e = &ents[i];
            if (!searchable(e)) {
                continue;
            }
            pose = entity_pose(e);
            half = (float)pose->w[0] * 0.5f;
            half = half < SEARCH_HALF_MIN ? SEARCH_HALF_MIN : half;
            low[0] = e->x - half;
            low[1] = e->y;
            low[2] = e->z - half;
            high[0] = e->x + half;
            high[1] = e->y + (float)pose->h[0];
            high[2] = e->z + half;
            if (ray_meets_box(from, dir, low, high, &t) && t < best_t) {
                float middle[3];

                middle[0] = e->x;
                middle[1] = e->y + (float)pose->h[0] * 0.5f;
                middle[2] = e->z;
                if (combat_sight(scene, from, middle)) {
                    best = e;
                    best_t = t;
                }
            }
        }
    }
    return best;
}

/* A thing's seed: the game's and its own key. */
static df_u32 thing_seed(const Entity *e)
{
    return mix(g_game_seed ^ e->key.place) ^ e->key.thing;
}

/* The bits of the things already taken off it. */
static unsigned taken_bits(const Keep *row)
{
    return (unsigned)row->game[COMBAT_KEEP_TAKEN] | ((unsigned)row->game[COMBAT_KEEP_TAKEN + 1] << 8);
}

/* The table e is filled from: its kind's for a body, the dungeon's for a heap. */
static const LootChance *table_of(const Entity *e)
{
    const EnemyKind *kind;

    if (dunstream_treasure(e)) {
        return dungeon_loot(dunstream_dungeon_type());
    }
    kind = enemy_kind(dunstream_kind(e, 0));
    return kind ? loot_chance(kind->loot[0]) : 0;
}

int loot_open(Entity *e, int level, Inventory *out, df_u8 *origin)
{
    Inventory rolled;
    Keep *row;
    unsigned taken;
    int i;

    if (dunstream_spare(e)) {
        pile_read(e, out);
        return out->count;
    }
    inventory_clear(out);
    row = keep_get(e->key, KEEP_ENTITY);
    if (!row) {
        return 0;
    }
    /* A heap has no death to take the level from: the first look at it sets it. */
    if (!row->game[COMBAT_KEEP_LEVEL]) {
        row->game[COMBAT_KEEP_LEVEL] = (df_u8)(level > 255 ? 255 : level);
    }
    loot_roll(&rolled, table_of(e), row->game[COMBAT_KEEP_LEVEL], thing_seed(e));
    taken = taken_bits(row);
    for (i = 0; i < rolled.count; i++) {
        if (!(taken & (1u << i))) {
            origin[out->count] = (df_u8)i;
            inventory_add(out, &rolled.item[i]);
        }
    }
    return out->count;
}

void loot_taken(const Entity *e, int origin)
{
    Keep *row;
    unsigned taken;

    if (dunstream_spare(e)) {
        return;
    }
    row = keep_find(e->key);
    if (!row || origin < 0 || origin >= LOOT_MAX) {
        return;
    }
    taken = taken_bits(row) | (1u << origin);
    row->game[COMBAT_KEEP_TAKEN] = (df_u8)(taken & 0xffu);
    row->game[COMBAT_KEEP_TAKEN + 1] = (df_u8)(taken >> 8);
}

int loot_takes_things(const Entity *e)
{
    return dunstream_spare(e);
}

const char *loot_label(const Entity *e)
{
    if (dunstream_spare(e)) {
        return "Pile";
    }
    return dunstream_treasure(e) ? "Treasure" : "Body";
}

void loot_closed(Entity *e, const Inventory *left)
{
    if (dunstream_spare(e)) {
        pile_write(e, left);
    } else if (!left->count && dunstream_treasure(e)) {
        /* Emptied, it goes, and stays gone: dead is what the stream's kept state puts back. */
        e->dead = 1;
        e->live = 0;
    }
}
