#include "game/dagger/play/standing.h"
#include "engine/core/mem.h"

/* See standing.h. */

/* The static table and its tree, in file order; the links are indexes, -1 for none. */
static FactionDef g_def[FACTIONS_MAX];
static short g_parent[FACTIONS_MAX];
static short g_child[FACTIONS_MAX];
static short g_next[FACTIONS_MAX];
static short g_ally[FACTIONS_MAX][FACTION_LINKS];
static short g_enemy[FACTIONS_MAX][FACTION_LINKS];
static int g_count;
static int (*g_membership)(int bits);

int faction_total(void)
{
    return g_count;
}

const FactionDef *faction_def(int index)
{
    return index >= 0 && index < g_count ? &g_def[index] : 0;
}

int faction_index(int id)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_def[i].id == id) {
            return i;
        }
    }
    return -1;
}

int factions_define(const FactionDef *defs, int count)
{
    int i, j, last;

    if (count < 0 || count > FACTIONS_MAX) {
        return 0;
    }
    g_count = count;
    for (i = 0; i < count; i++) {
        g_def[i] = defs[i];
        g_parent[i] = g_child[i] = g_next[i] = -1;
    }
    /* The tree as the original links it: a faction's children in file order, siblings by `next`. */
    for (i = 0; i < count; i++) {
        int p;

        p = faction_index(g_def[i].parent);
        if (g_def[i].parent == 0 || p < 0) {
            continue;
        }
        g_parent[i] = (short)p;
        if (g_child[p] < 0) {
            g_child[p] = (short)i;
        } else {
            for (last = g_child[p]; g_next[last] >= 0; last = g_next[last]) {
            }
            g_next[last] = (short)i;
        }
    }
    /* The roots are siblings of one another in file order too (the original's `factions` list). */
    last = -1;
    for (i = 0; i < count; i++) {
        if (g_parent[i] < 0) {
            if (last >= 0) {
                g_next[last] = (short)i;
            }
            last = i;
        }
    }
    for (i = 0; i < count; i++) {
        for (j = 0; j < FACTION_LINKS; j++) {
            g_ally[i][j] = (short)(g_def[i].ally[j] ? faction_index(g_def[i].ally[j]) : -1);
            g_enemy[i][j] = (short)(g_def[i].enemy[j] ? faction_index(g_def[i].enemy[j]) : -1);
        }
    }
    return 1;
}

static int find_type(int first, int region, int type)
{
    int i, found;

    for (i = first; i >= 0; i = g_next[i]) {
        if (g_def[i].type == type && g_def[i].region == region) {
            return i;
        }
        if (g_child[i] >= 0) {
            found = find_type(g_child[i], region, type);
            if (found >= 0) {
                return found;
            }
        }
    }
    return -1;
}

int faction_of_type(int region, int type)
{
    int i;

    i = find_type(0, region, type);
    return i >= 0 ? i : find_type(0, -1, type);
}

void standing_set_membership(int (*find)(int bits))
{
    g_membership = find;
}

void standing_defaults(StandingState *s)
{
    int i;

    mem_zero(s, sizeof(*s));
    for (i = 0; i < g_count; i++) {
        s->rep[i] = g_def[i].rep;
        s->power[i] = g_def[i].power;
    }
    /* New-game values: classic's start is not in the C; DFU starts every region at no legal standing and normal prices. */
    for (i = 0; i < REGIONS; i++) {
        s->price[i] = PRICE_NORMAL;
    }
    s->ready = 1;
}

int standing_valid(const StandingState *s)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (s->rep[i] < -100 || s->rep[i] > 100) {
            return 0;
        }
    }
    for (i = 0; i < REGIONS; i++) {
        if (s->price[i] < 250 || s->price[i] > 4000) {
            return 0;
        }
    }
    return 1;
}

int faction_rep(int id)
{
    int i;

    i = faction_index(id);
    return i >= 0 ? standing_state()->rep[i] : 0;
}

int faction_power(int id)
{
    int i;

    i = faction_index(id);
    return i >= 0 ? standing_state()->power[i] : 0;
}

static void add_index(int i, int amount)
{
    short *rep;
    int sum;

    if (i < 0) {
        return;
    }
    rep = &standing_state()->rep[i];
    sum = *rep + amount;
    /* The original stores a short: above 100 it stops at 100, below -100 it is 65436, which is -100. */
    *rep = (short)(sum > 100 ? 100 : (sum < -100 ? -100 : sum));
}

void faction_add_reputation(int id, int amount)
{
    add_index(faction_index(id), amount);
}

/* faction_add_reputation_r: the whole subtree, the target in full and everything else at half. */
static void add_tree(int first, int target, int amount)
{
    int i;

    for (i = first; i >= 0; i = g_next[i]) {
        add_index(i, i == target ? amount : amount >> 1);
        if (g_child[i] >= 0) {
            add_tree(g_child[i], target, amount);
        }
    }
}

/* faction_propagate_reputation: up to the root (or to the Fighters' parent 108), then down through the root's descendants. */
static void propagate(int original, int amount)
{
    int f, m;

    f = original;
    if (f < 0 || f >= g_count) {
        return;
    }
    while (g_parent[f] >= 0) {
        if (g_def[f].id == 108) {
            add_index(f, amount);
            break;
        }
        f = g_parent[f];
    }
    if (g_parent[f] < 0 && g_def[f].id != 844) {
        add_index(f, amount);
    }
    if (g_def[f].id == 844) {
        add_index(f, amount);
        /* The player's order of that bit, when they belong to one, takes it too, and its tree follows. */
        m = g_membership ? g_membership(64) : 0;
        if (m && faction_index(m) >= 0) {
            f = faction_index(m);
        }
        add_index(f, amount);
    }
    add_tree(g_child[f], original, amount);
}

void faction_change_reputation(int id, int amount)
{
    int f, i, m;

    f = faction_index(id);
    if (f < 0) {
        return;
    }
    if (g_def[f].id == 240) {
        m = g_membership ? g_membership(128) : 0;
        if (m && faction_index(m) >= 0) {
            f = faction_index(m);
        }
    }
    propagate(f, amount);
    for (i = 0; i < FACTION_LINKS; i++) {
        add_index(g_ally[f][i], amount >> 1);
    }
    for (i = 0; i < FACTION_LINKS; i++) {
        add_index(g_enemy[f][i], -(amount >> 1));
    }
}

int region_legal(int region)
{
    return region >= 0 && region < REGIONS ? standing_state()->legal[region] : 0;
}

void region_add_legal(int region, int amount)
{
    if (region >= 0 && region < REGIONS) {
        standing_state()->legal[region] = (short)(standing_state()->legal[region] + amount);
    }
}

int region_price(int region)
{
    return region >= 0 && region < REGIONS ? standing_state()->price[region] : PRICE_NORMAL;
}

void standing_decay(int court_open)
{
    StandingState *s;
    int i;

    s = standing_state();
    if (!court_open) {
        for (i = 0; i < REGIONS; i++) {
            if (s->legal[i] > 0) {
                s->legal[i]--;
            } else if (s->legal[i] < 0) {
                s->legal[i]++;
            }
        }
    }
    for (i = 0; i < g_count; i++) {
        /* A value under -100 is set to 100 first, as the original does, so it comes out at 99. */
        if (s->rep[i] > 100) {
            s->rep[i] = 100;
        } else if (s->rep[i] < -100) {
            s->rep[i] = 100;
        }
        if (s->rep[i] > 0) {
            s->rep[i]--;
        } else if (s->rep[i] < 0) {
            s->rep[i]++;
        }
    }
}

static int has_ally(int f, int other)
{
    int i;

    if (f < 0 || other < 0) {
        return 0;
    }
    for (i = 0; i < FACTION_LINKS; i++) {
        if (g_ally[f][i] == other) {
            return 1;
        }
    }
    return 0;
}

static int has_enemy(int f, int other)
{
    int i;

    if (f < 0 || other < 0) {
        return 0;
    }
    for (i = 0; i < FACTION_LINKS; i++) {
        if (g_enemy[f][i] == other) {
            return 1;
        }
    }
    return 0;
}

int faction_has_ally(int faction_id, int other_id)
{
    return has_ally(faction_index(faction_id), faction_index(other_id));
}

int faction_has_enemy(int faction_id, int other_id)
{
    return has_enemy(faction_index(faction_id), faction_index(other_id));
}

int faction_parent_id(int faction_id)
{
    int f;

    f = faction_index(faction_id);
    return f >= 0 && g_parent[f] >= 0 ? g_def[g_parent[f]].id : 0;
}

int faction_relation(int faction_id, const int *members, int count, int *score)
{
    int relation, m, i, j, f, o;

    relation = 8;
    f = faction_index(faction_id);
    if (f < 0) {
        return relation;
    }
    for (m = 0; m < count; m++) {
        o = faction_index(members[m]);
        if (o < 0) {
            continue;
        }
        if (faction_id == g_def[o].id) {
            return 0;
        }
        /* The same parent, or one of them the other's parent (the original's own grouping of && and ||). */
        if ((g_parent[o] >= 0 && g_parent[f] >= 0 && g_parent[o] == g_parent[f]) || g_parent[o] == f || g_parent[f] == o) {
            if (relation > 1) {
                relation = 1;
            }
            *score += 15;
        }
        if ((has_ally(o, f) || has_ally(f, o)) && relation > 2) {
            *score += 10;
            relation = 2;
        }
        if ((has_enemy(o, f) || has_enemy(f, o)) && relation > 3) {
            *score = 20;
            relation = 3;
        }
        for (i = 0; i < FACTION_LINKS; i++) {
            for (j = 0; j < FACTION_LINKS; j++) {
                if (g_enemy[o][i] >= 0 && g_enemy[o][i] == g_enemy[f][j] && relation > 4) {
                    relation = 4;
                    *score += 5;
                }
            }
        }
        for (i = 0; i < FACTION_LINKS; i++) {
            for (j = 0; j < FACTION_LINKS; j++) {
                if (g_ally[o][i] >= 0 && g_ally[o][i] == g_ally[f][j] && relation > 5) {
                    relation = 5;
                    *score += 5;
                }
            }
        }
        for (i = 0; i < FACTION_LINKS; i++) {
            for (j = 0; j < FACTION_LINKS; j++) {
                if (g_ally[o][i] >= 0 && has_ally(o, g_enemy[f][j]) && relation > 6) {
                    relation = 6;
                    *score -= 5;
                }
            }
        }
        for (i = 0; i < FACTION_LINKS; i++) {
            for (j = 0; j < FACTION_LINKS; j++) {
                if (g_enemy[o][i] >= 0 && has_enemy(o, g_ally[f][j]) && relation > 7) {
                    relation = 7;
                    *score -= 5;
                }
            }
        }
    }
    return relation;
}
