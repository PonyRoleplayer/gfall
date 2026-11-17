#include "game/dagger/play/magic/magic.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/character/vitals.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/play/items/inventory.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

static int length(const char *s) { int n; for (n = 0; s[n]; n++) { } return n; }
static int same(KeepKey a, KeepKey b) { return a.place == b.place && a.thing == b.thing; }
/* Town actors without world keys still need distinct spell targets. Their
 * scene-local addresses must never bind to creatures in the next place. */
#define MAGIC_EPHEMERAL_PLACE 0xffffffffu
#define MAGIC_TRAP_THING 0xfffffffdu

static int ephemeral_key(KeepKey key)
{
    return key.place == MAGIC_EPHEMERAL_PLACE && key.thing >= 1 && key.thing <= FOE_ACTORS;
}

KeepKey magic_key(int index)
{
    KeepKey key;
    Entity *e;

    key.place = key.thing = 0; e = dungeon_entity(index);
    if (index == -3) { key.place = MAGIC_EPHEMERAL_PLACE; key.thing = MAGIC_TRAP_THING; }
    if (e) {
        key = e->key;
        if (!key.place && !key.thing) { key.place = MAGIC_EPHEMERAL_PLACE; key.thing = (df_u32)index+1; }
    }
    return key;
}

int magic_index(KeepKey key)
{
    int i;
    if (!key.place && !key.thing) { return -1; }
    if (key.place == MAGIC_EPHEMERAL_PLACE && key.thing == MAGIC_TRAP_THING) { return -3; }
    if (ephemeral_key(key)) {
        i = (int)key.thing-1;
        return dungeon_entity(i) && same(key,magic_key(i)) ? i : TARGET_NONE;
    }
    for (i = 0; i < dungeon_count(); i++) {
        if (same(key,dungeon_entity(i)->key)) { return i; }
    }
    return TARGET_NONE;
}

void magic_forget_actor(int target)
{
    MagicState *magic;
    KeepKey key;
    int i;

    magic = magic_state(); key = magic_key(target);
    for (i = 0; i < EFFECTS; i++) {
        if (!magic->effect[i].active) { continue; }
        if (same(magic->effect[i].target,key)) { magic->effect[i].active = 0; }
        else if (same(magic->effect[i].caster,key)) { magic->effect[i].caster = magic_key(-3); }
    }
    for (i = 0; i < MAGIC_ACTORS; i++) {
        if (magic->actor[i].active && same(magic->actor[i].key,key)) { magic->actor[i].active = 0; }
    }
    for (i = 0; i < MISSILES; i++) {
        if (magic->missile[i].live && same(magic->missile[i].caster,key)) { magic->missile[i].caster = magic_key(-3); }
    }
}

void magic_forget_transients(void)
{
    MagicState *magic;
    int i;

    magic = magic_state();
    for (i = 0; i < EFFECTS; i++) {
        if (!magic->effect[i].active) { continue; }
        if (ephemeral_key(magic->effect[i].target)) { magic->effect[i].active = 0; }
        else if (ephemeral_key(magic->effect[i].caster)) { magic->effect[i].caster = magic_key(-3); }
    }
    for (i = 0; i < MAGIC_ACTORS; i++) {
        if (magic->actor[i].active && ephemeral_key(magic->actor[i].key)) { magic->actor[i].active = 0; }
    }
    for (i = 0; i < MISSILES; i++) {
        if (magic->missile[i].live && ephemeral_key(magic->missile[i].caster)) { magic->missile[i].live = 0; }
    }
}

int magic_target(int index, CharacterState *who, ClassInfo *career, int *base)
{
    if (index == -3) {
        int i;
        mem_zero(who,sizeof(*who)); mem_zero(career,sizeof(*career));
        who->level = character_state()->level; who->health = who->max_health = 1;
        for (i = 0; i < ATTRS; i++) { who->attribute[i] = base[i] = 50; }
        for (i = 0; i < SKILLS; i++) { who->skill[i] = 50; }
        return 1;
    }
    if (index == -1) {
        *who = *character_state(); *career = game_state()->career;
        mem_copy(base,game_state()->base_attribute,ATTRS*sizeof(int)); return 1;
    }
    return dungeon_spell_target(index,who,career,base);
}
void magic_commit(int index, const CharacterState *who, int caster)
{
    CharacterState copy;
    int maximum;
    copy = *who;
    if (copy.health < 0) { copy.health = 0; }
    if (copy.health > copy.max_health) { copy.health = copy.max_health; }
    maximum = fatigue_maximum(copy.attribute[ATTR_STR],copy.attribute[ATTR_END]);
    if (copy.fatigue < 0) { copy.fatigue = 0; }
    if (copy.fatigue > maximum) { copy.fatigue = maximum; }
    if (copy.magicka < 0) { copy.magicka = 0; }
    if (copy.magicka > copy.max_magicka) { copy.magicka = copy.max_magicka; }
    if (index == -1) {
        if (god_mode()) {
            if (copy.health < character_state()->health) { copy.health = character_state()->health; }
            if (copy.fatigue < character_state()->fatigue) { copy.fatigue = character_state()->fatigue; }
            if (copy.magicka < character_state()->magicka) { copy.magicka = character_state()->magicka; }
        }
        if (copy.health < character_state()->health) {
            damage_player(character_state()->health-copy.health,-1); copy.health = character_state()->health;
            copy.conditions = (copy.conditions & ~COND_SHIELD) | (character_state()->conditions & COND_SHIELD);
        }
        *character_state() = copy;
    } else if (index >= 0) {
        MagicActor *a;
        int i, free;
        free = -1;
        for (i = 0; i < MAGIC_ACTORS; i++) {
            a = &magic_state()->actor[i];
            if (!a->active && free < 0) { free = i; }
            if (a->active && same(a->key,magic_key(index))) { free = i; break; }
        }
        if (free >= 0) {
            a = &magic_state()->actor[free]; a->active = 1; a->key = magic_key(index);
            a->magicka = copy.magicka; a->fatigue = copy.fatigue;
        }
        dungeon_spell_commit(index,&copy,caster);
    }
}

static int divide(int value, int by) { return by ? value/by : 0; }
/* spell_cost_formula_dispatch: the formula an effect's group names, over a cost record c (four shorts) and the effect's own numbers; no skill and no target in it. */
int effect_cost_raw(int group, const df_i16 *c, const int *d, const int *ch, const int *m)
{
    int formula, cost;

    formula = effect_formula(group); cost = 0;
    if (formula == 1) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]+c[2]*ch[0]+divide(ch[1],ch[2])*c[3]; }
    else if (formula == 2) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]+c[2]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[3]; }
    else if (formula == 3) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]; }
    else if (formula == 4) { cost = c[0]*ch[0]+divide(ch[1],ch[2])*c[1]; }
    else if (formula == 5) { cost = c[0]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[1]; }
    else if (formula == 6) { cost = c[2]*((m[0]+m[1])/2)+((m[2]+m[3])/2)*divide(c[3],m[4])+c[0]*d[0]+divide(d[1]*c[1],d[2]); }
    else if (formula == 7) { cost = divide((df_i16)(c[0]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[1])*d[0],d[1]); }
    return (df_i16)cost;
}
int spell_cost(const SpellDef *s, const CharacterState *caster)
{
    const df_i16 *c;
    int k, total, skill;
    if (!s || !caster) { return 0; }
    total = 0;
    for (k = 0; k < 3; k++) {
        if (s->group[k] == 255) { continue; }
        c = effect_costs(s->group[k],s->sub[k]); if (!c) { continue; }
        skill = caster->skill[effect_school(s->group[k])];
        total += ((110-skill)*effect_cost_raw(s->group[k],c,s->duration[k],s->chance[k],s->magnitude[k]))/100;
    }
    total = (target_factor(s->target)*total) >> 1;
    return total < 5 ? 5 : total;
}
int magic_chance(int target, int group, int sub)
{
    Effect *e;
    int i;
    for (i = 0; i < EFFECTS; i++) {
        e = &magic_state()->effect[i];
        if (e->active && e->group == group && (sub < 0 || e->sub == sub) && same(e->target,magic_key(target))) { return e->chance; }
    }
    return 0;
}
int magic_resist(int index, int element, int bit, int modifier)
{
    CharacterState who;
    ClassInfo career;
    int base[8], chance, roll, race;
    if (!magic_target(index,&who,&career,base)) { return 0; }
    if ((who.conditions & resist_flag(element)) && random_range(1,100) <= magic_chance(index,GROUP_ELEMENTAL_RESISTANCE,element)) { return 0; }
    chance = 50;
    if (career.immunity & bit) { return 0; }
    if (career.weakness & bit) { return 100; }
    if (career.tolerance & bit) { chance >>= 1; }
    if (career.resistance & bit) { chance += chance >> 1; }
    chance += modifier;
    /* The form of the player counts as the race the original reads: a vampire or a beast has none of a race's gifts. */
    race = index == -1 ? curse_race() : who.race;
    if ((element == ELEMENT_COLD && race == PLAYER_RACE_NORD) || (element == ELEMENT_MAGIC && race == PLAYER_RACE_BRETON)) { chance += 30; }
    if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
    roll = random_range(1,100);
    if (roll > chance) { return 100; }
    if (chance-20 > roll) { return 0; }
    return (chance-roll)*5;
}

static int condition(int group)
{
    if (group == GROUP_PARALYZE) { return COND_PARALYZED; }
    if (group >= GROUP_INVISIBILITY && group <= GROUP_JUMPING) { return COND_GROUP(group); }
    if (group == GROUP_SHIELD) { return COND_SHIELD; }
    if (group == 39) { return COND_GROUP39; }
    if (group == 42) { return COND_GROUP42; }
    return group >= GROUP_COMPREHEND_LANGUAGES && group <= 46 ? COND_LATE_GROUP(group) : 0;
}
static void end(Effect *e)
{
    CharacterState who;
    ClassInfo career;
    int base[8], index;
    index = magic_index(e->target); e->active = 0;
    if (!magic_target(index,&who,&career,base)) { return; }
    who.conditions &= ~(e->group == GROUP_ELEMENTAL_RESISTANCE ? (int)resist_flag(e->sub) : condition(e->group));
    if (e->group == GROUP_FORTIFY && e->sub < 8) { who.attribute[e->sub] -= e->change; }
    magic_commit(index,&who,-1);
}
void magic_remove(int target, int group)
{
    int i;
    for (i = 0; i < EFFECTS; i++) {
        Effect *e;
        e = &magic_state()->effect[i];
        if (e->active && same(e->target,magic_key(target)) && (group < 0 || e->group == group)) { end(e); }
    }
}
void magic_hostile(void)
{
    int i;
    Effect *e;
    for (i = 0; i < EFFECTS; i++) {
        e = &magic_state()->effect[i];
        if (e->active && !e->target.place && !e->target.thing && e->sub == 0 &&
            (e->group == GROUP_INVISIBILITY || e->group == GROUP_CHAMELEON || e->group == GROUP_SHADOW)) { end(e); }
    }
}
int magic_sees(int kind)
{
    int conditions;
    conditions = character_state()->conditions;
    if (!(conditions & (COND_INVISIBLE | COND_CHAMELEON | COND_SHADOW)) || creature_ignores_illusion(kind)) { return 1; }
    if (conditions & COND_INVISIBLE) { return 0; }
    return random_range(1,100) <= (conditions & COND_SHADOW ? 8 : 4);
}
static Effect *extend_or_allocate(const Effect *value)
{
    Effect *e, *free;
    int i;
    free = 0;
    for (i = 0; i < EFFECTS; i++) {
        e = &magic_state()->effect[i];
        if (!e->active) { if (!free) { free = e; } continue; }
        if (value->icon < 200 && e->group == value->group && e->sub == value->sub && same(e->target,value->target)) {
            e->rounds = (e->rounds+value->rounds) & 65535; return 0;
        }
    }
    return free;
}

static void damage(CharacterState *who, int sub, int magnitude)
{
    if (sub == 0) { who->health -= magnitude; }
    else if (sub == 1) { who->fatigue -= magnitude; }
    else if (sub == 2) { who->magicka -= magnitude; }
}
static void heal(CharacterState *who, const int *base, int sub, int magnitude)
{
    if (sub < 8) { who->attribute[sub] += magnitude; if (who->attribute[sub] > base[sub]) { who->attribute[sub] = base[sub]; } }
    else if (sub == 8) { who->health += magnitude; }
    else if (sub == 9) { who->fatigue += magnitude*FATIGUE_UNIT; }
    else if (sub == 10) { who->magicka += magnitude; }
}

static int g_dispel[EFFECTS], g_dispel_count, g_dispel_chance;
static void dispel_answer(const char *answer)
{
    Effect *e;
    Effect selected;
    int row, chance, i;
    if (!answer) { return; }
    row = 0;
    while (*answer >= '0' && *answer <= '9' && row < EFFECTS+1) { row = row*10+*answer++-'0'; }
    if (*answer || row < 1 || row > g_dispel_count) { return; }
    e = &magic_state()->effect[g_dispel[row-1]];
    if (!e->active) { return; }
    chance = g_dispel_chance+(character_state()->level-e->level)*5;
    if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
    if ((!e->caster.place && !e->caster.thing) || random_range(1,100) <= chance) {
        selected = *e;
        for (i = 0; i < EFFECTS; i++) {
            e = &magic_state()->effect[i];
            if (e->active && e->spell == selected.spell && same(e->caster,selected.caster) &&
                same(e->target,selected.target)) { end(e); }
        }
        msg_set("The spell has been dispelled.");
    } else { msg_set("The spell resisted your attempt."); }
}
static void dispel_prompt(int target, const Effect *effect)
{
    char text[2048], line[96];
    Effect *e;
    int i, j, used;
    df_copy(text,"Dispel Magic: enter the number to remove.\n",sizeof(text));
    used = length(text); g_dispel_count = 0; g_dispel_chance = effect->chance;
    for (i = 0; i < EFFECTS && used < 1850; i++) {
        e = &magic_state()->effect[i];
        if (!e->active || !same(e->target,magic_key(target))) { continue; }
        for (j = 0; j < g_dispel_count; j++) {
            const Effect *prior;
            prior = &magic_state()->effect[g_dispel[j]];
            if (prior->spell == e->spell && same(prior->caster,e->caster)) { break; }
        }
        if (j < g_dispel_count) { continue; }
        g_dispel[g_dispel_count++] = i;
        log_format(line,sizeof(line),"%d. %s\n",g_dispel_count,spell_def(e->spell)->name);
        df_copy(text+used,line,sizeof(text)-used); used = length(text);
    }
    if (g_dispel_count) { possessions_message(text,dispel_answer); }
    else { msg_set("There are no spells to dispel."); }
}

static void apply_effect(Effect *e, int index, int caster)
{
    CharacterState who, giver;
    ClassInfo career;
    int base[8], other_base[8], group, sub, before, chance;
    if (!magic_target(index,&who,&career,base)) { return; }
    group = e->group; sub = e->sub;
    if (group == GROUP_PARALYZE) {
        if ((index == -1 ? curse_race() : who.race) == PLAYER_RACE_HIGH_ELF || (index == -1 && (career.immunity & EFFECT_BIT_PARALYSIS)) || (who.conditions & (COND_FREE_ACTION | COND_PARALYZED)) || random_range(1,100) > e->chance) { e->active = 0; return; }
    }
    if (group == GROUP_SOUL_TRAP && index >= 0 && ENEMY_IS_CLASS(dungeon_kind(index))) {
        e->active = 0; msg_set("That creature has no soul you can trap."); return;
    }
    if ((group == GROUP_CURE || group == GROUP_DISINTEGRATE || group == GROUP_SILENCE || group == GROUP_CALM || group == GROUP_CHARM) && random_range(1,100) > e->chance) { e->active = 0; return; }
    if (group == GROUP_ELEMENTAL_RESISTANCE) { who.conditions |= resist_flag(sub); }
    else { who.conditions |= condition(group); }
    if (group == GROUP_CONTINUOUS_DAMAGE || group == GROUP_DAMAGE) { damage(&who,sub,e->magnitude); }
    else if (group == GROUP_DISINTEGRATE) { who.health = 0; }
    else if (group == GROUP_CURE) {
        if (sub < 2) { ailments_cure(index,sub == 1); }
        else if (sub == 2) { magic_remove(index,0); }
        return;
    } else if (group == GROUP_DISPEL) {
        if (sub == 0) { dispel_prompt(index,e); return; }
        else {
            int i, level, chance;
            CharacterState victim;
            for (i = 0; i < dungeon_count(); i++) {
                if (!dispel_kind(dungeon_kind(i),sub-1) || !magic_target(i,&victim,&career,base)) { continue; }
                level = e->level; chance = e->chance+(level-victim.level)*5;
                if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
                if (random_range(1,100) <= chance) { victim.health = 0; magic_commit(i,&victim,caster); }
            }
            return;
        }
    } else if (group == GROUP_DRAIN || group == GROUP_TRANSFER) {
        before = e->magnitude; if (group == GROUP_TRANSFER) { before >>= 1; }
        if (sub < 8) { who.attribute[sub] -= before; if (who.attribute[sub] < 1) { who.attribute[sub] = 1; } }
        else if (sub == 8) { who.health -= before; }
        else if (sub == 9) { who.fatigue -= before; if (who.fatigue < 1) { who.fatigue = 1; } }
        if (group == GROUP_TRANSFER && magic_target(caster,&giver,&career,other_base)) {
            heal(&giver,other_base,sub,before); magic_commit(caster,&giver,caster);
        }
    } else if (group == GROUP_FORTIFY && sub < 8) {
        before = who.attribute[sub]; who.attribute[sub] += e->magnitude;
        if (who.attribute[sub] > 100) { who.attribute[sub] = 100; }
        e->change = who.attribute[sub]-before;
    } else if (group == GROUP_HEAL) { heal(&who,base,sub,e->magnitude); }
    else if (group == GROUP_FREE_ACTION) { magic_remove(index,0); who.conditions &= ~COND_PARALYZED; }
    else if (group == GROUP_CALM || group == GROUP_CHARM) {
        if (index >= 0 && (ENEMY_IS_CLASS(dungeon_kind(index)) == (group == GROUP_CHARM))) { dungeon_pacify(index); }
    }
    else if (group == GROUP_TELEPORT && index == -1) { magic_recall_prompt(); }
    chance = who.health;
    magic_commit(index,&who,caster);
    if (index == -1 && group == GROUP_PARALYZE && chance > 0) { msg_set("You are paralyzed."); }
}

static int soul_trap_empty(df_u32 id)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        if (game_state()->object[i].id && game_state()->object[i].parent == id) { return 0; }
    }
    return 1;
}

void magic_soul_capture(int index)
{
    Effect *e;
    const ObjectInfo *item, *owner;
    df_u32 gem, soul;
    int i, j, depth, kind, star_worn;

    kind = dungeon_kind(index); if (!ENEMY_IS_CLASSIC(kind)) { return; }
    gem = magic_artifact_worn(9);
    star_worn = gem != 0;
    if (star_worn) {
        if (soul_trap_empty(gem) && object_new(RECORD_SOUL,gem,kind,100)) {
            msg_set("The soul has been trapped.");
            return;
        }
    }
    for (i = 0; i < EFFECTS; i++) {
        e = &magic_state()->effect[i];
        if (!e->active || e->group != GROUP_SOUL_TRAP || !same(e->target,magic_key(index))) { continue; }
        gem = 0;
        for (j = 0; j < OBJECTS; j++) {
            item = &game_state()->object[j];
            if (!item->id || item->type != RECORD_ITEM ||
                !((item->group == ITEM_GROUP_MISC && item->index == MISC_SOUL_TRAP) || magic_artifact_power(item->id) == 9) ||
                !soul_trap_empty(item->id)) { continue; }
            owner = item;
            for (depth = 0; owner && depth < OBJECTS; depth++) {
                if (owner->parent == game_state()->entity_id) { gem = item->id; break; }
                owner = object_at(owner->parent);
            }
        }
        if (!gem) { msg_set("You have no empty soul trap."); return; }
        if (!star_worn && random_range(0,100) > e->chance) { return; }
        soul = object_new(RECORD_SOUL,gem,kind,100);
        if (soul) { msg_set("The soul has been trapped."); }
        return;
    }
}

static void apply_spell(const SpellDef *s, int spell, int caster, int target, int level, int no_resist)
{
    CharacterState from, to;
    ClassInfo career;
    Effect value, *stored;
    int base[8], k, flags, percent, rounds;
    if (!s || !magic_target(caster,&from,&career,base) || !magic_target(target,&to,&career,base)) { return; }
    if (level <= 0) { level = from.level; }
    if (s->icon >= 200) { level = 8; }
    if (target == -1 && god_mode() && caster != -1) { return; }
    if (!no_resist && s->target != 0) {
        if ((to.conditions & COND_ABSORPTION) || (career.absorption & 6)) {
            if (!(to.conditions & COND_ABSORPTION) || random_range(1,100) <= magic_chance(target,GROUP_SPELL_ABSORPTION,-1)) {
                k = spell_cost(s,&to);
                if (to.magicka+k <= to.max_magicka) { to.magicka += k; magic_commit(target,&to,caster); return; }
            }
        }
        if ((to.conditions & COND_REFLECTION) && random_range(1,100) <= magic_chance(target,GROUP_SPELL_REFLECTION,-1)) {
            target = caster; to = from;
        }
        if ((to.conditions & COND_RESISTANCE) && random_range(1,100) <= magic_chance(target,GROUP_SPELL_RESISTANCE,-1)) { return; }
    }
    percent = no_resist || s->target == 0 ? 100 : magic_resist(target,s->element,element_bit(s->element),-(level-to.level)*5);
    if (!percent) { msg_set("The spell has no effect."); return; }
    for (k = 0; k < 3; k++) {
        if (s->group[k] == 255) { continue; }
        flags = effect_flags(s->group[k],s->sub[k]); mem_zero(&value,sizeof(value));
        value.active = 1; value.target = magic_key(target); value.caster = magic_key(caster);
        value.group = s->group[k]; value.sub = s->sub[k]; value.spell = spell;
        value.level = level; value.icon = s->icon; value.element = s->element;
        rounds = s->duration[k][0]+s->duration[k][1]*divide(level,s->duration[k][2]);
        value.rounds = (df_u16)(rounds*percent/100);
        value.permanent = s->duration[k][0] == -1;
        value.chance = (df_u8)(s->chance[k][0]+s->chance[k][1]*divide(level,s->chance[k][2]));
        if (flags & 4) {
            value.magnitude = (df_u16)((random_range(s->magnitude[k][0],s->magnitude[k][1])+
                divide(level,s->magnitude[k][4])*random_range(s->magnitude[k][2],s->magnitude[k][3]))*percent/100);
        }
        if ((flags & 1) || value.group == GROUP_LOCK || value.group == GROUP_OPEN) {
            if (value.group == GROUP_LOCK || value.group == GROUP_OPEN) { value.permanent = 1; }
            stored = extend_or_allocate(&value);
            if (!stored) { continue; }
            *stored = value; apply_effect(stored,target,caster);
        } else { apply_effect(&value,target,caster); }
    }
}

void magic_apply(int spell, int caster, int target, int level, int no_resist)
{
    apply_spell(spell_def(spell), spell, caster, target, level, no_resist);
}

/* A spell definition given by the caller (an item's strike spell with icon 250). */
void magic_apply_def(const SpellDef *def, int spell, int caster, int target, int level, int no_resist)
{
    apply_spell(def, spell, caster, target, level, no_resist);
}

/* item_apply_equip_effects, type 1: the spell cast on the player as a lasting one (every effect's duration -1) with the icon 200 + the slot of the item that holds it. */
void magic_hold(int spell, int slot)
{
    SpellDef held;
    const SpellDef *s;
    int k;

    s = spell_def(spell);
    if (!s || slot < 0 || slot >= EQUIP_SLOTS) { return; }
    held = *s; held.icon = 200 + slot;
    for (k = 0; k < 3; k++) {
        if (held.group[k] == 255) { continue; }
        held.duration[k][0] = -1; held.duration[k][1] = 0; held.duration[k][2] = 0;
    }
    apply_spell(&held, spell, -1, -1, 0, 1);
}

/* item_remove_equip_effects, type 1: the lasting spell of that slot (and spell) ends. */
void magic_release(int spell, int slot)
{
    int i;

    for (i = 0; i < EFFECTS; i++) {
        Effect *e;

        e = &magic_state()->effect[i];
        if (e->active && e->icon == 200 + slot && e->spell == spell && !e->target.place && !e->target.thing) { end(e); }
    }
}

void magic_minutes(df_u32 start, df_u32 count)
{
    Effect *e;
    CharacterState who;
    ClassInfo career;
    df_u32 n;
    int i, target, caster, base[8];
    for (n = 0; n < count && character_state()->health > 0; n++) {
        for (i = 0; i < EFFECTS; i++) {
            e = &magic_state()->effect[i];
            if (!e->active || e->permanent) { continue; }
            if (!e->rounds) { end(e); continue; }
            e->rounds--; target = magic_index(e->target); caster = magic_index(e->caster);
            if (!magic_target(target,&who,&career,base)) { continue; }
            if (e->group == GROUP_CONTINUOUS_DAMAGE) { damage(&who,e->sub,e->magnitude); }
            if (e->group == GROUP_REGENERATE) { heal(&who,base,8,e->magnitude); }
            magic_commit(target,&who,caster);
        }
        ailments_minutes(start+n,1);
    }
}
void magic_restore_foe(int index)
{
    CharacterState who;
    ClassInfo career;
    Effect *e;
    AilmentInfo *a;
    int i, k, base[8];
    if (!magic_target(index,&who,&career,base)) { return; }
    for (i = 0; i < MAGIC_ACTORS; i++) {
        const MagicActor *a;
        a = &magic_state()->actor[i];
        if (a->active && same(a->key,magic_key(index))) { who.magicka = a->magicka; who.fatigue = a->fatigue; break; }
    }
    for (i = 0; i < EFFECTS; i++) {
        e = &magic_state()->effect[i];
        if (!e->active || !same(e->target,magic_key(index))) { continue; }
        who.conditions |= e->group == GROUP_ELEMENTAL_RESISTANCE ? (int)resist_flag(e->sub) : condition(e->group);
        if (e->group == GROUP_FORTIFY && e->sub < 8) { who.attribute[e->sub] += e->change; }
    }
    for (i = 0; i < AILMENTS; i++) {
        a = &magic_state()->ailment[i];
        if (a->active && same(a->target,magic_key(index))) {
            for (k = 0; k < 8; k++) { who.attribute[k] -= a->drained[k]; }
        }
    }
    dungeon_spell_commit(index,&who,-1);
}

int magic_absorb(int index, int damage)
{
    Effect *e;
    int i, amount;
    for (i = 0; i < EFFECTS; i++) {
        e = &magic_state()->effect[i];
        if (!e->active || e->group != GROUP_SHIELD || !same(e->target,magic_key(index))) { continue; }
        amount = e->magnitude < damage ? e->magnitude : damage;
        e->magnitude -= amount; damage -= amount;
        if (!e->magnitude) { end(e); }
        break;
    }
    return damage;
}

int magic_valid(const MagicState *state)
{
    const Effect *e;
    const AilmentInfo *a;
    const MissileInfo *m;
    int i, k;
    if (state->paid < 0 || state->paid > 1000000) { return 0; }
    if (state->recall.valid < 0 || state->recall.valid > 1 ||
        (state->recall.valid && (!state->recall.place ||
         !(state->recall.x >= -100000000 && state->recall.x <= 100000000) ||
         !(state->recall.y >= -100000000 && state->recall.y <= 100000000) ||
         !(state->recall.z >= -100000000 && state->recall.z <= 100000000) ||
         state->recall.yaw < 0 || state->recall.yaw >= 2048 || state->recall.pitch < -256 || state->recall.pitch > 256))) { return 0; }
    for (i = 0; i < MAGIC_ACTORS; i++) {
        const MagicActor *a;
        a = &state->actor[i]; if (!a->active) { continue; }
        if (a->active != 1 || !a->key.place || !a->key.thing || a->magicka < 0 || a->magicka > 1000000 ||
            a->fatigue < 0 || a->fatigue > 1000000) { return 0; }
        for (k = 0; k < i; k++) {
            if (state->actor[k].active && same(a->key,state->actor[k].key)) { return 0; }
        }
    }
    for (i = 0; i < EFFECTS; i++) {
        e = &state->effect[i]; if (!e->active) { continue; }
        if (e->active != 1 || e->group < 0 || e->group >= 51 || e->sub < 0 || (e->sub >= 12 && e->sub != 255) ||
            e->rounds < 0 || e->rounds > 65535 || e->magnitude < 0 || e->magnitude > 65535 ||
            e->chance < 0 || e->chance > 255 || e->icon < 0 || e->icon > 255 || !spell_def(e->spell) || e->level < 1 || e->level > 1000 ||
            e->element < 0 || e->element > 4 || e->permanent < 0 || e->permanent > 1 ||
            e->change < -1000 || e->change > 1000) { return 0; }
    }
    for (i = 0; i < AILMENTS; i++) {
        a = &state->ailment[i]; if (!a->active) { continue; }
        if (a->active != 1 || (!disease_def(a->id) && !poison_def(a->id)) ||
            a->delay < 0 || a->delay > 32767 || a->maximum < 0 || a->maximum > 32767 ||
            a->left < 0 || a->left > 32767 || a->stage < 0 || a->stage > 2) { return 0; }
        for (k = 0; k < 11; k++) { if (a->stat[k] < 0 || a->stat[k] > 1) { return 0; } }
        for (k = 0; k < 8; k++) { if (a->drained[k] < -100000 || a->drained[k] > 100000) { return 0; } }
    }
    for (i = 0; i < MISSILES; i++) {
        m = &state->missile[i]; if (!m->live) { continue; }
        if (m->live != 1 || !spell_def(m->spell) || m->level < 1 || m->level > 1000 ||
            m->element < 0 || m->element > 4 || m->target < 0 || m->target > 4 ||
            !(m->x >= -100000000 && m->x <= 100000000) || !(m->y >= -100000000 && m->y <= 100000000) ||
            !(m->z >= -100000000 && m->z <= 100000000) || !(m->dx >= -1 && m->dx <= 1) ||
            !(m->dy >= -1 && m->dy <= 1) || !(m->dz >= -1 && m->dz <= 1) || m->burst < 0 || m->burst > 10000) { return 0; }
    }
    return 1;
}
int magic_indicators(void)
{
    int i, flags;
    flags = 0;
    for (i = 0; i < EFFECTS; i++) {
        const Effect *e;
        e = &magic_state()->effect[i];
        if (e->active && !e->target.place && !e->target.thing) { flags |= 1; }
    }
    for (i = 0; i < AILMENTS; i++) {
        const AilmentInfo *a;
        a = &magic_state()->ailment[i];
        if (a->active && a->stage && !a->target.place && !a->target.thing) { flags |= a->id >= 128 ? 2 : 4; }
    }
    return flags;
}
void magic_status(void)
{
    char text[2048], line[96];
    int i, used;
    Effect *e;
    AilmentInfo *a;
    df_copy(text,"Active magic and ailments:\n",sizeof(text)); used = length(text);
    for (i = 0; i < EFFECTS && used < 1800; i++) {
        e = &magic_state()->effect[i];
        if (!e->active || e->target.place || e->target.thing) { continue; }
        log_format(line,sizeof(line),"%s: %d rounds\n",spell_effect(e->group,e->sub),e->rounds);
        df_copy(text+used,line,sizeof(text)-used); used = length(text);
    }
    for (i = 0; i < AILMENTS && used < 1800; i++) {
        a = &magic_state()->ailment[i];
        if (!a->active || a->target.place || a->target.thing) { continue; }
        log_format(line,sizeof(line),"%s %d: %s\n",a->id >= 128 ? "Poison" : "Disease",a->id,a->stage ? "active" : "latent");
        df_copy(text+used,line,sizeof(text)-used); used = length(text);
    }
    /* status_show: TEXT.RSC 22 (the date, the time and where the player stands), then show_health_status: the ailments, or record 18 when there are none. Active spells are ours. */
    { static char intro[300]; if (status_intro(intro,sizeof(intro)) > 0) { possessions_message(intro,0); } }
    if (used == 26) { df_copy(text,words_state(18),sizeof(text)); }
    if (text[0]) { possessions_message(text,0); }
}
