#include "game/dagger/play/standing.h"
#include "engine/core/mem.h"

/* See standing.h. */

/* The hierarchy is 5 deep in FACTION.TXT; a bad mod's loop stops here. */
#define WALK_MOST 16

/* Each faction's reputation, in the file's order; a social group's; the regions. */
static short *g_rep;
static short g_social[SOCIAL_COUNT];
static int g_reaction_bonus;
static RegionStanding g_region[STANDING_REGIONS];

/* Where a faction's reputation is, opening the file the first time; 0 for none. */
static short *rep_of(int faction)
{
    int i;

    if (!factionfile_open()) {
        return 0;
    }
    if (!g_rep) {
        g_rep = (short *)mem_keep((df_u32)faction_count() * (df_u32)sizeof(short));
        if (!g_rep) {
            return 0;
        }
        for (i = 0; i < faction_count(); i++) {
            g_rep[i] = faction_at(i)->rep;
        }
    }
    for (i = 0; i < faction_count(); i++) {
        if (faction_at(i)->id == faction) {
            return &g_rep[i];
        }
    }
    return 0;
}

static int clamp(int v)
{
    return v < -STANDING_MOST ? -STANDING_MOST : (v > STANDING_MOST ? STANDING_MOST : v);
}

void standing_new_game(df_u32 seed)
{
    int i;

    rep_of(0);
    for (i = 0; g_rep && i < faction_count(); i++) {
        g_rep[i] = faction_at(i)->rep;
    }
    mem_zero(g_social, sizeof(g_social));
    g_reaction_bonus = 0;
    mem_zero(g_region, sizeof(g_region));
    for (i = 0; i < STANDING_REGIONS; i++) {
        /* A small mix of the seed: each region its own of classic's random 750..1250. */
        seed = seed * 1103515245u + 12345u;
        g_region[i].prices = (short)(750 + (int)((seed >> 16) % 501u));
    }
}

int standing_rep(int faction)
{
    const short *r;

    r = rep_of(faction);
    return r ? *r : 0;
}

/* The one faction moved, nothing else. */
static void move_one(int faction, int amount)
{
    short *r;

    r = rep_of(faction);
    if (r) {
        *r = (short)clamp(*r + amount);
    }
}

/* Down the tree from root: the whole to the one changed and the root, half to the rest. */
static void spread_down(int root, int changed, int amount, int depth)
{
    int i;

    move_one(root, root == changed || depth == 0 ? amount : amount / 2);
    if (depth >= WALK_MOST) {
        return;
    }
    for (i = 0; i < faction_count(); i++) {
        if (faction_at(i)->parent == root && faction_at(i)->id != root) {
            spread_down(faction_at(i)->id, changed, amount, depth + 1);
        }
    }
}

void standing_change(int faction, int amount, int spread)
{
    const Faction *f;
    const Faction *up;
    int i;

    f = rep_of(faction) ? faction_find(faction) : 0;
    if (!f) {
        return;
    }
    if (!spread) {
        move_one(faction, amount);
        return;
    }
    for (i = 0; i < FACTION_LINKS; i++) {
        if (f->ally[i]) {
            move_one(f->ally[i], amount / 2);
        }
        if (f->enemy[i]) {
            move_one(f->enemy[i], -amount / 2);
        }
    }
    up = f;
    for (i = 0; i < WALK_MOST && up->parent; i++) {
        const Faction *next;

        next = faction_find(up->parent);
        if (!next) {
            break;
        }
        up = next;
    }
    spread_down(up->id, faction, amount, 0);
}

const Faction *standing_answers_for(int faction)
{
    const Faction *f;
    int i;

    f = factionfile_open() ? faction_find(faction) : 0;
    for (i = 0; f && i < WALK_MOST; i++) {
        if (f->type == FACTION_GROUP || f->type == FACTION_PROVINCE || f->type == FACTION_TEMPLE
            || f->type == FACTION_PEOPLE || f->type == FACTION_COURTS || f->type == FACTION_INDIVIDUAL || !f->parent) {
            return f;
        }
        f = faction_find(f->parent);
    }
    return f;
}

/* A social group as a table's index. */
static int social_index(int sgroup)
{
    return sgroup >= 0 && sgroup < SOCIAL_COUNT ? sgroup : SOCIAL_MERCHANTS;
}

int standing_social(int sgroup)
{
    return g_social[social_index(sgroup)];
}

void standing_social_change(int sgroup, int amount)
{
    int i;

    i = social_index(sgroup);
    g_social[i] = (short)clamp(g_social[i] + amount);
}

int standing_reaction_bonus(void)
{
    return g_reaction_bonus;
}

void standing_set_reaction_bonus(int bonus)
{
    g_reaction_bonus = bonus;
}

int standing_reaction(int faction)
{
    const Faction *f;
    const Faction *own;
    int r;

    own = factionfile_open() ? faction_find(faction) : 0;
    f = standing_answers_for(faction);
    r = g_reaction_bonus + (f ? standing_rep(f->id) : 0);
    /* The person's own social group, not the group over it. */
    if (own) {
        r += standing_social(own->sgroup);
    }
    return r;
}

RegionStanding *standing_region(int region)
{
    return region >= 0 && region < STANDING_REGIONS ? &g_region[region] : 0;
}

int standing_changed_count(void)
{
    int n;
    int i;

    n = 0;
    rep_of(0);
    for (i = 0; g_rep && i < faction_count(); i++) {
        n += g_rep[i] != faction_at(i)->rep;
    }
    return n;
}

void standing_changed_at(int i, int *faction, int *rep)
{
    int k;

    *faction = 0;
    *rep = 0;
    for (k = 0; g_rep && k < faction_count(); k++) {
        if (g_rep[k] != faction_at(k)->rep && i-- == 0) {
            *faction = faction_at(k)->id;
            *rep = g_rep[k];
            return;
        }
    }
}

void standing_put_rep(int faction, int rep)
{
    short *r;

    r = rep_of(faction);
    if (r) {
        *r = (short)clamp(rep);
    }
}

const short *standing_socials(void)
{
    return g_social;
}

void standing_put_socials(const short *social, int bonus)
{
    int i;

    for (i = 0; i < SOCIAL_COUNT; i++) {
        g_social[i] = social[i];
    }
    g_reaction_bonus = bonus;
}
