#include "game/dagger/play/cast.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/shots.h"
#include "game/dagger/tables/magic.h"
#include "engine/core/mem.h"
#include "engine/scene/interact.h"

/* See cast.h and docs/game.md (Magic). */

static Spell g_ready;
static int g_have_ready;
static Spell g_last;
static int g_have_last;
/* The spell each missile in the air carries, by its entry in projectile.c. */
static Spell g_flying[PROJ_MAX];
static int g_flying_live[PROJ_MAX];

void cast_new_game(void)
{
    g_have_ready = 0;
    g_have_last = 0;
    mem_zero(g_flying_live, sizeof(g_flying_live));
}

int cast_is_ready(void)
{
    return g_have_ready;
}

const Spell *cast_ready_spell(void)
{
    return g_have_ready ? &g_ready : 0;
}

const Spell *cast_last_spell(void)
{
    return g_have_last ? &g_last : 0;
}

void cast_cancel(void)
{
    g_have_ready = 0;
}

const Spell *cast_flying(int entry)
{
    return entry >= 0 && entry < PROJ_MAX && g_flying_live[entry] ? &g_flying[entry] : 0;
}

int cast_ready(const Spell *s, const Player *p)
{
    if (effects_player_has(GROUP_SILENCE, -1)) {
        interact_say("You are silenced.");
        return 0;
    }
    if (spell_effects(s) == 0) {
        return 0;
    }
    if (p->sheet.magicka < spell_cost(s, &p->sheet, 0) && !p->god) {
        interact_say("You don't have the spell points.");
        return 0;
    }
    g_ready = *s;
    g_have_ready = 1;
    return 1;
}

/* Points paid and the schools of what took hold trained, as DFU tallies a cast. */
static void pay(Player *p, const Spell *s, int landed)
{
    int k;

    if (!p->god) {
        int cost;

        cost = spell_cost(s, &p->sheet, 0);
        p->sheet.magicka = p->sheet.magicka > cost ? p->sheet.magicka - cost : 0;
    }
    for (k = 0; landed > 0 && k < MAGIC_SLOTS; k++) {
        const MagicEffect *fx;

        fx = s->effect[k].effect == SPELL_NO_EFFECT ? 0 : magic_effect(s->effect[k].effect);
        if (fx && fx->school < SKILL_COUNT) {
            character_tally(&p->sheet, fx->school, 1);
        }
    }
}

/* Every creature within the burst's radius of at, the spell landed on each. */
static int burst(const Spell *s, const float *at, Player *p, const Scene *scene)
{
    Entity *near[CAST_BURST_MAX];
    int n;
    int i;
    int landed;

    landed = 0;
    shots_flash(scene, s->element, at);
    n = combat_near(scene, at, CAST_BURST, near, CAST_BURST_MAX);
    for (i = 0; i < n; i++) {
        EffectTarget to;

        to.player = 0;
        to.creature = near[i];
        to.scene = scene;
        landed += effects_land(s, &to, p);
    }
    return landed;
}

int cast_fire(Player *p, const Scene *scene, const float *eye, const float *dir)
{
    Spell s;
    int landed;

    if (!g_have_ready) {
        return 0;
    }
    if (effects_player_has(GROUP_SILENCE, -1)) {
        interact_say("You are silenced.");
        return 0;
    }
    s = g_ready;
    if (p->sheet.magicka < spell_cost(&s, &p->sheet, 0) && !p->god) {
        interact_say("You don't have the spell points.");
        g_have_ready = 0;
        return 0;
    }
    landed = 0;
    switch (s.target) {
    case TARGET_CASTER:
        landed = effects_land_self(&s, p);
        break;
    case TARGET_TOUCH: {
        EffectTarget to;

        to.player = 0;
        to.scene = scene;
        to.creature = combat_aim_at(scene, eye, dir, CAST_TOUCH, 0);
        if (!to.creature) {
            interact_say("There is nothing in reach.");
            return 0;
        }
        landed = effects_land(&s, &to, p);
        {
            float mid[3];

            mid[0] = to.creature->x;
            mid[1] = to.creature->y + entity_height(to.creature) * 0.5f;
            mid[2] = to.creature->z;
            shots_flash(scene, s.element, mid);
        }
        break;
    }
    case TARGET_AREA_CASTER:
        landed = burst(&s, eye, p, scene);
        break;
    default: {
        ProjectileShot shot;
        int k;
        int entry;

        /* At range, one target or an area at its end: a missile from an arm's length out. */
        mem_zero(&shot, sizeof(shot));
        shot.kind = PROJ_SPELL;
        shot.from = PROJ_FROM_PLAYER;
        shot.speed = PROJ_SPELL_SPEED;
        shot.range = PROJ_SPELL_RANGE;
        for (k = 0; k < 3; k++) {
            shot.dir[k] = dir[k];
            shot.at[k] = eye[k] + dir[k] * CAST_ARM;
        }
        entry = projectile_launch(&shot);
        if (entry < 0) {
            interact_say("Too many spells are in the air.");
            return 0;
        }
        g_flying[entry] = s;
        g_flying_live[entry] = 1;
        break;
    }
    }
    pay(p, &s, s.target == TARGET_RANGE || s.target == TARGET_AREA_RANGE ? 1 : landed);
    g_last = s;
    g_have_last = 1;
    g_have_ready = 0;
    return 1;
}

int cast_arrived(const ProjectileEnd *end, Player *p, const Scene *scene)
{
    Spell s;
    int landed;

    if (end->entry < 0 || end->entry >= PROJ_MAX || !g_flying_live[end->entry]) {
        return 0;
    }
    s = g_flying[end->entry];
    g_flying_live[end->entry] = 0;
    landed = 0;
    if (s.target == TARGET_AREA_RANGE) {
        float at[3];
        int k;

        /* Burst a little short of a wall, not in it. */
        for (k = 0; k < 3; k++) {
            at[k] = end->at[k] - end->dir[k] * (end->result == PROJ_HIT_WALL ? 8.0f : 0.0f);
        }
        landed = burst(&s, at, p, scene);
    } else if (end->result == PROJ_HIT_TARGET && end->target) {
        EffectTarget to;

        to.player = 0;
        to.creature = (Entity *)end->target;
        to.scene = scene;
        landed = effects_land(&s, &to, p);
    }
    return landed;
}
