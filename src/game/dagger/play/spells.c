#include "game/dagger/play/spells.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See spells.h and docs/game.md (Magic). */

void spell_standard(Spell *out, const MagicSpell *std)
{
    int k;

    mem_zero(out, sizeof(*out));
    df_copy(out->name, std->name, SPELL_NAME);
    out->icon = std->icon;
    out->element = std->element;
    out->target = std->target;
    for (k = 0; k < MAGIC_SLOTS; k++) {
        magic_settings(std, k, &out->effect[k]);
    }
}

int spell_effects(const Spell *s)
{
    int n;
    int k;

    n = 0;
    for (k = 0; k < MAGIC_SLOTS; k++) {
        if (s->effect[k].effect != SPELL_NO_EFFECT) {
            n++;
        }
    }
    return n;
}

int spell_has_effect(const Spell *s, int effect_id)
{
    int k;

    for (k = 0; k < MAGIC_SLOTS; k++) {
        if (s->effect[k].effect == effect_id) {
            return 1;
        }
    }
    return 0;
}

/* One parameter's gold: the offset, A for its start and B for each of its level steps (GetEffectComponentCosts). */
static int component_gold(const df_u16 *cost, int start, int increase, int per_level)
{
    return cost[2] + cost[0] * start + cost[1] * (per_level > 0 ? increase / per_level : 0);
}

int spell_effect_gold(const EffectSettings *e, int skill, int *points)
{
    const MagicEffect *fx;
    int gold;
    int active;

    if (points) {
        *points = 0;
    }
    if (e->effect == SPELL_NO_EFFECT) {
        return 0;
    }
    fx = magic_effect(e->effect);
    if (!fx) {
        return 0;
    }
    gold = 0;
    active = 0;
    if (fx->supports & MAGIC_DURATION) {
        active = 1;
        gold += component_gold(fx->duration_cost, e->duration_base, e->duration_plus, e->duration_per);
    }
    if (fx->supports & MAGIC_CHANCE) {
        active = 1;
        gold += component_gold(fx->chance_cost, e->chance_base, e->chance_plus, e->chance_per);
    }
    if (fx->supports & MAGIC_MAGNITUDE) {
        active = 1;
        gold += component_gold(fx->magnitude_cost, (e->magnitude_base_high + e->magnitude_base_low) / 2,
                               (e->magnitude_plus_high + e->magnitude_plus_low) / 2, e->magnitude_per);
    }
    /* Nothing to set (Teleport): classic prices it as an effect of 1, 1 and 1 at (60, 100, 160). */
    if (!active) {
        static const df_u16 flat[3] = { 60, 100, 160 };

        gold = component_gold(flat, 1, 1, 1);
    }
    if (points) {
        *points = gold * (110 - skill) / 400;
    }
    return gold;
}

int spell_target_cost(int cost, int target)
{
    switch (target) {
    case TARGET_RANGE:
        return cost * 3 / 2;
    case TARGET_AREA_CASTER:
        return cost * 2;
    case TARGET_AREA_RANGE:
        return cost * 5 / 2;
    default:
        return cost;
    }
}

int spell_cost(const Spell *s, const Character *pc, int *gold)
{
    int total_gold;
    int total_points;
    int k;

    total_gold = 0;
    total_points = 0;
    for (k = 0; k < MAGIC_SLOTS; k++) {
        const MagicEffect *fx;
        int points;
        int skill;

        fx = s->effect[k].effect == SPELL_NO_EFFECT ? 0 : magic_effect(s->effect[k].effect);
        if (!fx) {
            continue;
        }
        skill = pc && fx->school < SKILL_COUNT ? pc->skill[fx->school] : 0;
        total_gold += spell_effect_gold(&s->effect[k], skill, &points);
        total_points += points;
    }
    total_gold = spell_target_cost(total_gold, s->target);
    total_points = spell_target_cost(total_points, s->target);
    if (gold) {
        *gold = total_gold;
    }
    if (spell_effects(s) == 0) {
        return 0;
    }
    return total_points < 5 ? 5 : total_points;
}

void spell_effect_name(int effect_id, char *out, int cap)
{
    const MagicEffect *fx;

    fx = magic_effect(effect_id);
    if (!fx) {
        log_format(out, cap, "Effect %d", effect_id);
    } else if (fx->sub[0]) {
        log_format(out, cap, "%s %s", fx->group, fx->sub);
    } else {
        log_format(out, cap, "%s", fx->group);
    }
}

/* Case folded comparison for the book's order and names. */
static int fold(int c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

static int name_order(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] && b[i]; i++) {
        if (fold(a[i]) != fold(b[i])) {
            return fold(a[i]) - fold(b[i]);
        }
    }
    return fold(a[i]) - fold(b[i]);
}

int spellbook_find(const Spellbook *b, const char *name)
{
    int i;

    for (i = 0; i < b->count; i++) {
        if (name_order(b->spell[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int spellbook_add(Spellbook *b, const Spell *s)
{
    if (b->count >= SPELLBOOK_MAX || spellbook_find(b, s->name) >= 0) {
        return 0;
    }
    b->spell[b->count++] = *s;
    return 1;
}

void spellbook_remove(Spellbook *b, int i)
{
    if (i < 0 || i >= b->count) {
        return;
    }
    for (; i + 1 < b->count; i++) {
        b->spell[i] = b->spell[i + 1];
    }
    b->count--;
}

void spellbook_sort(Spellbook *b)
{
    int i;
    int j;

    /* An insertion sort: the book is short and the sort rare, and it keeps equal names (there are none) in their order. */
    for (i = 1; i < b->count; i++) {
        Spell key;

        key = b->spell[i];
        for (j = i - 1; j >= 0 && name_order(b->spell[j].name, key.name) > 0; j--) {
            b->spell[j + 1] = b->spell[j];
        }
        b->spell[j + 1] = key;
    }
}

/* DFU's StartingSpells by class (Mage, Spellsword, Battlemage, Sorcerer, Healer, Nightblade, Bard), by SPELLS.STD's numbers. */
static const df_u8 g_start[7][5] = {
    { 1, 37, 2, 97, 8 },
    { 8, 44, 37, 0, 0 },
    { 8, 2, 37, 0, 0 },
    { 1, 37, 2, 97, 8 },
    { 97, 2, 1, 37, 0 },
    { 44, 2, 0, 0, 0 },
    { 37, 0, 0, 0, 0 }
};

void spellbook_starting(Spellbook *b, const Character *pc)
{
    int set;
    int i;

    set = -1;
    if (pc->klass == CLASS_CUSTOM) {
        const CareerClass *c;

        c = career_class(CLASS_CUSTOM);
        for (i = 0; c && i < 3; i++) {
            if ((c->primary[i] >= SKILL_DESTRUCTION && c->primary[i] <= SKILL_MYSTICISM)
                || (c->major[i] >= SKILL_DESTRUCTION && c->major[i] <= SKILL_MYSTICISM)) {
                set = 1;
            }
        }
    } else if (pc->klass >= 0 && pc->klass <= 6) {
        set = pc->klass;
    }
    for (i = 0; set >= 0 && i < 5; i++) {
        const MagicSpell *std;
        Spell s;

        if (g_start[set][i] == 0) {
            break;
        }
        std = magic_standard(g_start[set][i]);
        if (std) {
            spell_standard(&s, std);
            spellbook_add(b, &s);
        }
    }
}
