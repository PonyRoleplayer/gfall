#include "game/dagger/play/effects.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/ills.h"
#include "game/dagger/play/rules.h"
#include "engine/core/mem.h"
#include "engine/scene/interact.h"

/* See effects.h and docs/game.md (Magic). */

/* The effects that last, the player's and the creatures'. */
static LiveEffect g_live[EFFECTS_LIVE];
static int g_died;

/* ---- What is built. ---- */

int effects_built(int id)
{
    int group;
    int sub;

    group = id >> 8;
    sub = id & 255;
    switch (group) {
    case GROUP_DAMAGE:
    case GROUP_CONTINUOUS_DAMAGE:
    case GROUP_CURE:
        return sub <= 2 ? EFFECT_BUILT : EFFECT_NOT_BUILT;
    case GROUP_HEAL:
    case GROUP_TRANSFER:
        /* Health and fatigue; the attributes' heals and transfers wait for attributes that can be drained. */
        return sub == 8 || sub == 9 ? EFFECT_BUILT : EFFECT_NOT_BUILT;
    case GROUP_DISPEL:
        return sub == 0 ? EFFECT_BUILT : EFFECT_NOT_BUILT;
    case GROUP_REGENERATE:
    case GROUP_PARALYZE:
    case GROUP_SILENCE:
    case GROUP_ELEMENTAL_RESISTANCE:
    case GROUP_SLOWFALL:
    case GROUP_WATER_BREATHING:
    case GROUP_FREE_ACTION:
        return EFFECT_BUILT;
    case GROUP_LIGHT:
    case GROUP_LEVITATE:
    case GROUP_WATER_WALKING:
    case GROUP_JUMPING:
    case GROUP_CLIMBING:
    case GROUP_INVISIBILITY:
    case GROUP_CHAMELEON:
    case GROUP_SHADOW:
    case GROUP_SHIELD:
    case GROUP_SPELL_ABSORPTION:
    case GROUP_SPELL_REFLECTION:
    case GROUP_SPELL_RESISTANCE:
        return EFFECT_RECORDED;
    default:
        return EFFECT_NOT_BUILT;
    }
}

/* ---- The dice. ---- */

static int roll_range(int low, int high)
{
    return high < low ? low : low + combat_roll(high - low + 1);
}

/* How many of a parameter's level steps the caster has: level / per level (DFU floors it). */
static int steps(int level, int per)
{
    return per > 0 ? level / per : 0;
}

/* A magnitude: low..high plus low..high for each step of the level. */
static int roll_magnitude(const EffectSettings *e, int level)
{
    return roll_range(e->magnitude_base_low, e->magnitude_base_high)
         + roll_range(e->magnitude_plus_low, e->magnitude_plus_high) * steps(level, e->magnitude_per);
}

/* Rounds: base + plus for each step of the level, at least one step (a round is never lost to a low level). */
static int roll_rounds(const EffectSettings *e, int level)
{
    int n;

    n = steps(level, e->duration_per);
    return e->duration_base + e->duration_plus * (n < 1 ? 1 : n);
}

/* ---- The saving throw. ---- */

int effects_save_percent(int save, int roll)
{
    if (save >= 100) {
        return 0;
    }
    if (roll <= save) {
        /* Saved; but DFU prorates a roll within 20 of failing, and the share that lands falls 5% a point. */
        return save - 20 <= roll ? 100 - 5 * (save - roll) : 0;
    }
    return 100;
}

/* The effect flag (careers.h) of an element, for the race's and the class's tolerances. */
static int element_flag(int element)
{
    switch (element) {
    case ELEMENT_FIRE:
        return EFFECT_FIRE;
    case ELEMENT_COLD:
        return EFFECT_FROST;
    case ELEMENT_POISON:
        return EFFECT_POISON;
    case ELEMENT_SHOCK:
        return EFFECT_SHOCK;
    default:
        return EFFECT_MAGIC;
    }
}

int effects_player_save(const Character *c, int element, int paralysis)
{
    const CareerRace *race;
    const CareerClass *klass;
    int flags;
    int save;

    flags = element_flag(element) | (paralysis ? EFFECT_PARALYSIS : 0);
    save = 50;
    race = career_race(c->race);
    if (race) {
        if (race->immune & flags) {
            return 100;
        }
        save += (race->resist & flags) ? 30 : 0;
        save -= (race->low_tolerance & flags) ? 25 : 0;
        save -= (race->weakness & flags) ? 50 : 0;
    }
    klass = career_class(c->klass);
    if (klass) {
        save += (klass->immune & flags) ? 50 : 0;
        save -= (klass->weakness & flags) ? 50 : 0;
        save -= (klass->low_tolerance & flags) ? 25 : 0;
        save += (klass->resist & flags) ? 25 : 0;
    }
    if (save >= 100) {
        return 100;
    }
    save += rules_magic_resist(c->attribute[ATTR_WILLPOWER]);
    return save < 5 ? 5 : (save > 95 ? 95 : save);
}

/*
 * The share of an effect of element that lands on to, 0..100: a live resistance to the element first
 * rolls its chance to turn the whole of it away; then the saving throw.
 */
static int target_percent(const EffectTarget *to, int element, int paralysis)
{
    int save;

    if (to->player) {
        int chance;

        chance = effects_resist_chance(element);
        if (chance > 0 && 1 + combat_roll(100) <= chance) {
            return 0;
        }
        save = effects_player_save(&to->player->sheet, element, paralysis);
    } else {
        save = 50 + rules_magic_resist(combat_creature_will(to->creature));
        save = save < 5 ? 5 : (save > 95 ? 95 : save);
    }
    return effects_save_percent(save, 1 + combat_roll(100));
}

/* ---- The live list. ---- */

/* Whether l is on the player (who 0) or on that creature. */
static int on_who(const LiveEffect *l, const Entity *who)
{
    if (!who) {
        return !l->on_creature;
    }
    return l->on_creature && l->who == who && l->key.place == who->key.place && l->key.thing == who->key.thing;
}

static int same_target(const LiveEffect *l, const EffectTarget *to)
{
    return on_who(l, to->creature);
}

static int live_valid(const LiveEffect *l)
{
    if (!l->live) {
        return 0;
    }
    return !l->on_creature || (l->who && l->who->live && !l->who->dead && l->who->key.place == l->key.place
                               && l->who->key.thing == l->key.thing);
}

void effects_reset(int all)
{
    int i;

    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (all || g_live[i].on_creature) {
            g_live[i].live = 0;
        }
    }
    if (all) {
        g_died = 0;
    }
}

int effects_count(void)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (g_live[i].live) {
            n++;
        }
    }
    return n;
}

const LiveEffect *effects_get(int i)
{
    int k;

    for (k = 0; k < EFFECTS_LIVE; k++) {
        if (g_live[k].live && i-- == 0) {
            return &g_live[k];
        }
    }
    return 0;
}

/* Where an effect of this kind on this target goes: the one it renews, else a free entry, else the shortest. -1 never. */
static int live_slot(int effect, const Entity *who)
{
    int free_at;
    int worst;
    int i;

    free_at = -1;
    worst = 0;
    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (g_live[i].live && g_live[i].effect == effect && on_who(&g_live[i], who)) {
            return i;
        }
        if (!g_live[i].live && free_at < 0) {
            free_at = i;
        }
        if (g_live[i].rounds < g_live[worst].rounds) {
            worst = i;
        }
    }
    return free_at >= 0 ? free_at : worst;
}

int effects_put(const LiveEffect *e)
{
    int at;

    at = live_slot(e->effect, 0);
    if (e->on_creature || at < 0) {
        return 0;
    }
    g_live[at] = *e;
    g_live[at].live = 1;
    g_live[at].on_creature = 0;
    g_live[at].who = 0;
    return 1;
}

int effects_player_has(int group, int sub)
{
    int i;

    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (g_live[i].live && !g_live[i].on_creature && (g_live[i].effect >> 8) == group
            && (sub < 0 || (g_live[i].effect & 255) == sub)) {
            return 1;
        }
    }
    return 0;
}

int effects_resist_chance(int element)
{
    int i;

    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (g_live[i].live && !g_live[i].on_creature && (g_live[i].effect >> 8) == GROUP_ELEMENTAL_RESISTANCE
            && (g_live[i].effect & 255) == element) {
            return g_live[i].chance;
        }
    }
    return 0;
}

int effects_died(void)
{
    int d;

    d = g_died;
    g_died = 0;
    return d;
}

/* ---- What happens to a target. ---- */

/* The three vitals an effect takes and gives. */
enum {
    VITAL_HEALTH,
    VITAL_FATIGUE,
    VITAL_MAGICKA
};

/* Damage's subgroups (0 health, 1 fatigue, 2 spell points) and heal's and transfer's (8 health, 9 fatigue) as a vital. */
static int vital_of(int group, int sub)
{
    if (group == GROUP_HEAL || group == GROUP_TRANSFER) {
        return sub == 8 ? VITAL_HEALTH : VITAL_FATIGUE;
    }
    return sub;
}

/* amount points of a vital taken from the target; a creature has only health to lose. */
static void take_from(const EffectTarget *to, Player *caster, int vital, int amount)
{
    if (amount <= 0) {
        return;
    }
    if (to->player) {
        Character *c;

        c = &to->player->sheet;
        if (vital == VITAL_HEALTH) {
            if (player_hurt(to->player, amount) == 1) {
                g_died = 1;
            }
        } else if (vital == VITAL_FATIGUE) {
            c->fatigue = c->fatigue > amount * CHARACTER_FATIGUE_UNIT ? c->fatigue - amount * CHARACTER_FATIGUE_UNIT : 0;
        } else {
            c->magicka = c->magicka > amount ? c->magicka - amount : 0;
        }
    } else if (vital == VITAL_HEALTH) {
        combat_creature_hurt(&caster->sheet, to->scene, to->creature, amount, caster->body.feet, 0);
    }
}

/* amount points of a vital given back to the player, to no more than the most. */
static void give_to(Character *c, int vital, int amount)
{
    if (amount <= 0) {
        return;
    }
    if (vital == VITAL_HEALTH) {
        c->health = c->health + amount > c->health_max ? c->health_max : c->health + amount;
    } else if (vital == VITAL_FATIGUE) {
        c->fatigue = c->fatigue + amount * CHARACTER_FATIGUE_UNIT > c->fatigue_max ? c->fatigue_max : c->fatigue + amount * CHARACTER_FATIGUE_UNIT;
    } else {
        c->magicka = c->magicka + amount > c->magicka_max ? c->magicka_max : c->magicka + amount;
    }
}

/*
 * A rolled magnitude, and for an effect on another, the share of it the target's saving throw lets
 * through. The sentence a save made is said (DFU's "Save versus spell made.").
 */
static int magnitude_on(const EffectSettings *e, int level, const EffectTarget *to, int element, int self)
{
    int amount;
    int percent;

    amount = roll_magnitude(e, level);
    if (self || amount <= 0) {
        return amount;
    }
    percent = target_percent(to, element, 0);
    amount = amount * percent / 100;
    if (amount <= 0) {
        interact_say("Save versus spell made.");
    }
    return amount;
}

/* The effect of that id on the target ended, if it runs. */
static void drop_live(int effect, const EffectTarget *to)
{
    int i;

    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (g_live[i].live && g_live[i].effect == effect && same_target(&g_live[i], to)) {
            g_live[i].live = 0;
        }
    }
}

/* Every effect on the target ended: dispel magic. */
static void drop_all(const EffectTarget *to)
{
    int i;

    for (i = 0; i < EFFECTS_LIVE; i++) {
        if (g_live[i].live && same_target(&g_live[i], to)) {
            g_live[i].live = 0;
        }
    }
}

/* One round of an effect that acts each: continuous damage and regeneration, rolled afresh. */
static void run_round(LiveEffect *l, Player *player, const Scene *scene)
{
    EffectTarget to;
    int group;
    int sub;

    group = l->effect >> 8;
    sub = l->effect & 255;
    to.player = l->on_creature ? 0 : player;
    to.creature = l->on_creature ? l->who : 0;
    to.scene = scene;
    if (group == GROUP_CONTINUOUS_DAMAGE) {
        take_from(&to, player, vital_of(group, sub), magnitude_on(&l->settings, l->level, &to, l->element, l->self));
    } else if (group == GROUP_REGENERATE && !l->on_creature) {
        give_to(&player->sheet, VITAL_HEALTH, roll_magnitude(&l->settings, l->level));
    }
}

void effects_round(Player *player, const Scene *scene)
{
    int i;

    for (i = 0; i < EFFECTS_LIVE; i++) {
        LiveEffect *l;

        l = &g_live[i];
        /* Out of rounds at the tick, or its creature gone: it ends. Else it acts and counts one down (as DFU's MagicRound). */
        if (!live_valid(l) || l->rounds <= 0) {
            l->live = 0;
            continue;
        }
        run_round(l, player, scene);
        l->rounds--;
    }
}

void effects_pass(Player *player, const Scene *scene, int minutes)
{
    int i;

    /* A day's worth at most: nothing lasts longer than a spell's 60 + 60 x 20 rounds can, and continuous ones are a few. */
    for (i = 0; i < minutes && i < 1440; i++) {
        effects_round(player, scene);
    }
}

/* ---- Landing a spell. ---- */

/* The effect begun to last on the target: its first round at once, as DFU's effects have it. */
static int begin_live(const EffectSettings *e, int element, int rounds, int chance, int level, int self, const EffectTarget *to, Player *caster)
{
    LiveEffect *l;
    int at;

    at = live_slot(e->effect, to->creature);
    if (at < 0) {
        return 0;
    }
    l = &g_live[at];
    mem_zero(l, sizeof(*l));
    l->effect = e->effect;
    l->element = (df_u8)element;
    l->on_creature = to->player ? 0 : 1;
    l->live = 1;
    l->self = (df_u8)self;
    l->rounds = rounds < 1 ? 1 : rounds;
    l->level = level;
    l->settings = *e;
    l->chance = chance;
    l->who = to->creature;
    if (to->creature) {
        l->key = to->creature->key;
    }
    /* Its first round at once, which counts: a duration of 3 acts three times. */
    run_round(l, caster, to->scene);
    l->rounds--;
    return 1;
}

/*
 * One effect of a spell on the target. 1 when it took hold. self: the target is the caster (no saving
 * throw, no "save made").
 */
static int land_one(const EffectSettings *e, int element, const EffectTarget *to, Player *caster, int self)
{
    const MagicEffect *fx;
    int level;
    int group;
    int sub;
    int chance;
    int rounds;

    fx = magic_effect(e->effect);
    if (!fx || effects_built(e->effect) == EFFECT_NOT_BUILT) {
        return 0;
    }
    level = caster->sheet.level;
    group = e->effect >> 8;
    sub = e->effect & 255;
    chance = 0;
    if (fx->supports & MAGIC_CHANCE) {
        chance = e->chance_base + e->chance_plus * steps(level, e->chance_per);
        /* A resistance keeps its chance for the blows it meets; every other effect rolls it now. */
        if (group != GROUP_ELEMENTAL_RESISTANCE && 1 + combat_roll(100) > chance) {
            interact_say(self ? "Spell effect failed." : "Save versus spell made.");
            return 0;
        }
    }
    /* An effect with no magnitude is saved against whole. */
    if (!self && !(fx->supports & MAGIC_MAGNITUDE) && target_percent(to, element, group == GROUP_PARALYZE) == 0) {
        interact_say("Save versus spell made.");
        return 0;
    }
    rounds = (fx->supports & MAGIC_DURATION) ? roll_rounds(e, level) : 0;
    switch (group) {
    case GROUP_DAMAGE:
        take_from(to, caster, vital_of(group, sub), magnitude_on(e, level, to, element, self));
        return 1;
    case GROUP_HEAL:
        if (to->player) {
            give_to(&to->player->sheet, vital_of(group, sub), roll_magnitude(e, level));
        }
        return to->player != 0;
    case GROUP_TRANSFER: {
        int amount;

        amount = magnitude_on(e, level, to, element, self);
        take_from(to, caster, vital_of(group, sub), amount);
        give_to(&caster->sheet, vital_of(group, sub), amount);
        return 1;
    }
    case GROUP_CURE:
        if (sub == 0 && to->player) {
            ills_cure(&to->player->sheet);
        } else if (sub == 2) {
            drop_live(MAGIC_ID(GROUP_PARALYZE, 255), to);
            if (to->creature) {
                foes_freeze(to->creature, -1.0f);
            }
        }
        /* Poison has no state of its own yet: nothing to cure, and nothing wrong. */
        return 1;
    case GROUP_DISPEL:
        drop_all(to);
        return 1;
    case GROUP_PARALYZE:
        /* A free action is proof against it, and a creature is held by its own mind. */
        if (to->player && effects_player_has(GROUP_FREE_ACTION, -1)) {
            return 0;
        }
        if (to->creature) {
            foes_freeze(to->creature, (float)rounds * EFFECTS_ROUND_SECONDS);
            return foes_frozen(to->creature);
        }
        return begin_live(e, element, rounds, chance, level, self, to, caster);
    case GROUP_CONTINUOUS_DAMAGE:
        return begin_live(e, element, rounds, chance, level, self, to, caster);
    default:
        /* The rest last on the player alone (what a creature does with them is a later slice). */
        return to->player ? begin_live(e, element, rounds, chance, level, self, to, caster) : 0;
    }
}

int effects_land(const Spell *spell, const EffectTarget *to, Player *caster)
{
    int landed;
    int unbuilt;
    int self;
    int k;

    landed = 0;
    unbuilt = 0;
    self = to->player == caster;
    for (k = 0; k < MAGIC_SLOTS; k++) {
        if (spell->effect[k].effect == SPELL_NO_EFFECT) {
            continue;
        }
        if (effects_built(spell->effect[k].effect) == EFFECT_NOT_BUILT) {
            unbuilt++;
            continue;
        }
        landed += land_one(&spell->effect[k], spell->element, to, caster, self);
    }
    if (unbuilt && !landed) {
        interact_say("This magic is not worked yet.");
    }
    return landed;
}

int effects_land_self(const Spell *spell, Player *caster)
{
    EffectTarget to;

    to.player = caster;
    to.creature = 0;
    to.scene = 0;
    return effects_land(spell, &to, caster);
}
