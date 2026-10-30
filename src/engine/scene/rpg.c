#include "engine/scene/rpg.h"

/* See rpg.h. */

/* The attributes by weights in hundredths, rounded toward zero. */
static int weighted(const RpgRules *r, const RpgSheet *s, const int *weight)
{
    int sum;
    int a;

    sum = 0;
    for (a = 0; a < r->nattrs; a++) {
        sum += s->attr[a] * weight[a];
    }
    return sum / 100;
}

void rpg_init(RpgSheet *s, const RpgRules *r)
{
    int a;
    int v;

    s->level = 1;
    s->xp = 0;
    s->points = r->create_points;
    for (a = 0; a < RPG_ATTRS; a++) {
        s->attr[a] = a < r->nattrs ? r->attr_start : 0;
        s->fixed[a] = s->attr[a];
    }
    /* Creation may lower an attribute to its least, for points to spend elsewhere. */
    for (a = 0; a < r->nattrs; a++) {
        s->fixed[a] = r->attr_min;
    }
    for (v = 0; v < RPG_VITALS; v++) {
        s->vital[v] = v < r->nvitals ? rpg_vital_max(s, r, v) : 0;
    }
}

int rpg_raise(RpgSheet *s, const RpgRules *r, int a)
{
    if (a < 0 || a >= r->nattrs || s->points <= 0 || s->attr[a] >= r->attr_max) {
        return 0;
    }
    s->attr[a]++;
    s->points--;
    return 1;
}

int rpg_lower(RpgSheet *s, const RpgRules *r, int a)
{
    if (a < 0 || a >= r->nattrs || s->attr[a] <= s->fixed[a]) {
        return 0;
    }
    s->attr[a]--;
    s->points++;
    return 1;
}

int rpg_commit(RpgSheet *s, const RpgRules *r)
{
    int a;
    int v;

    if (s->points > 0) {
        for (a = 0; a < r->nattrs; a++) {
            if (s->attr[a] < r->attr_max) {
                return 0;
            }
        }
    }
    for (a = 0; a < r->nattrs; a++) {
        s->fixed[a] = s->attr[a];
    }
    for (v = 0; v < r->nvitals; v++) {
        s->vital[v] = rpg_vital_max(s, r, v);
    }
    return 1;
}

int rpg_vital_max(const RpgSheet *s, const RpgRules *r, int v)
{
    const RpgVital *rv;
    int most;

    if (v < 0 || v >= r->nvitals) {
        return 0;
    }
    rv = &r->vital[v];
    most = rv->base + rv->per_level * (s->level - 1) + weighted(r, s, rv->weight);
    return most < 0 ? 0 : most;
}

int rpg_xp_next(const RpgRules *r, int level)
{
    int need;
    int i;

    if (level < 1 || level >= r->level_max) {
        return 0;
    }
    need = r->xp_first;
    for (i = 1; i < level; i++) {
        /* Held under what the sum can reach: a long curve flattens, it does not wrap. */
        if (need > 20000000) {
            return need;
        }
        need = need * r->xp_growth / 100;
    }
    return need;
}

int rpg_gain_xp(RpgSheet *s, const RpgRules *r, int xp)
{
    int gained;
    int v;

    gained = 0;
    if (xp <= 0 || s->level >= r->level_max) {
        return 0;
    }
    s->xp += xp;
    for (;;) {
        int need;

        need = rpg_xp_next(r, s->level);
        if (need <= 0 || s->xp < need) {
            break;
        }
        s->xp -= need;
        s->level++;
        s->points += r->level_points;
        gained++;
    }
    /* Nothing is owed past the last level. */
    if (s->level >= r->level_max) {
        s->xp = 0;
    }
    if (gained) {
        for (v = 0; v < r->nvitals; v++) {
            s->vital[v] = rpg_vital_max(s, r, v);
        }
    }
    return gained;
}

int rpg_hurt(RpgSheet *s, int v, int amount)
{
    if (v < 0 || v >= RPG_VITALS) {
        return 0;
    }
    if (amount > 0) {
        s->vital[v] = amount >= s->vital[v] ? 0 : s->vital[v] - amount;
    }
    return s->vital[v] == 0;
}

void rpg_restore(RpgSheet *s, const RpgRules *r, int v, int amount)
{
    int most;

    if (v < 0 || v >= r->nvitals || amount <= 0) {
        return;
    }
    most = rpg_vital_max(s, r, v);
    s->vital[v] = amount >= most - s->vital[v] ? most : s->vital[v] + amount;
}

int rpg_roll(df_u32 *seed, int n)
{
    if (n <= 1) {
        return 0;
    }
    *seed = *seed * 1103515245u + 12345u;
    return (int)((*seed >> 16) % (df_u32)n);
}

int rpg_chance(const RpgRules *r, const RpgSheet *attacker, const RpgSheet *defender, int attack_bonus,
               int defence_bonus)
{
    int chance;

    chance = r->hit_base + weighted(r, attacker, r->attack_weight) - weighted(r, defender, r->defence_weight)
             + attack_bonus - defence_bonus;
    return chance < r->hit_min ? r->hit_min : (chance > r->hit_max ? r->hit_max : chance);
}

void rpg_blow(const RpgRules *r, const RpgSheet *attacker, const RpgSheet *defender, int lo, int hi, int attack_bonus,
              int defence_bonus, int armour, df_u32 *seed, RpgBlow *out)
{
    int damage;

    out->chance = rpg_chance(r, attacker, defender, attack_bonus, defence_bonus);
    out->hit = rpg_roll(seed, 100) < out->chance;
    out->damage = 0;
    if (!out->hit) {
        return;
    }
    damage = (hi > lo ? lo + rpg_roll(seed, hi - lo + 1) : lo) + weighted(r, attacker, r->damage_weight);
    if (armour > 0) {
        damage = damage * 100 / (100 + armour);
    }
    out->damage = damage < 1 ? 1 : damage;
}
