#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/ailments.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/tables/magicdefs.h"
#include "game/ogfall/play/inventory.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

static int length(const char *s) { int n; for (n = 0; s[n]; n++) { } return n; }
static int same(KeepKey a, KeepKey b) { return a.place == b.place && a.thing == b.thing; }
KeepKey og_magic_key(int index)
{
    KeepKey key;
    Entity *e;
    key.place = key.thing = 0; e = og_dungeon_entity(index);
    if (index == -3) { key.place = 0xffffffffu; key.thing = 0xfffffffdu; }
    if (e) { key = e->key; }
    return key;
}
int og_magic_index(KeepKey key)
{
    int i;
    if (!key.place && !key.thing) { return -1; }
    if (key.place == 0xffffffffu && key.thing == 0xfffffffdu) { return -3; }
    for (i = 0; i < og_dungeon_count(); i++) {
        if (same(key,og_dungeon_entity(i)->key)) { return i; }
    }
    return -2;
}
int og_magic_target(int index, OgCharacter *who, OgClass *career, int *base)
{
    if (index == -3) {
        int i;
        mem_zero(who,sizeof(*who)); mem_zero(career,sizeof(*career));
        who->level = og_character()->level; who->health = who->max_health = 1;
        for (i = 0; i < OG_ATTRS; i++) { who->attribute[i] = base[i] = 50; }
        for (i = 0; i < OG_SKILLS; i++) { who->skill[i] = 50; }
        return 1;
    }
    if (index == -1) {
        *who = *og_character(); *career = og_state()->career;
        mem_copy(base,og_state()->base_attribute,OG_ATTRS*sizeof(int)); return 1;
    }
    return og_dungeon_spell_target(index,who,career,base);
}
void og_magic_commit(int index, const OgCharacter *who, int caster)
{
    OgCharacter copy;
    int maximum;
    copy = *who;
    if (copy.health < 0) { copy.health = 0; }
    if (copy.health > copy.max_health) { copy.health = copy.max_health; }
    maximum = (copy.attribute[OG_STR]+copy.attribute[OG_END])*64;
    if (copy.fatigue < 0) { copy.fatigue = 0; }
    if (copy.fatigue > maximum) { copy.fatigue = maximum; }
    if (copy.magicka < 0) { copy.magicka = 0; }
    if (copy.magicka > copy.max_magicka) { copy.magicka = copy.max_magicka; }
    if (index == -1) {
        if (og_god_mode()) {
            if (copy.health < og_character()->health) { copy.health = og_character()->health; }
            if (copy.fatigue < og_character()->fatigue) { copy.fatigue = og_character()->fatigue; }
            if (copy.magicka < og_character()->magicka) { copy.magicka = og_character()->magicka; }
        }
        if (copy.health < og_character()->health) {
            og_damage_player(og_character()->health-copy.health,-1); copy.health = og_character()->health;
            copy.conditions = (copy.conditions & ~0x400000) | (og_character()->conditions & 0x400000);
        }
        *og_character() = copy;
    } else if (index >= 0) {
        OgMagicActor *a;
        int i, free;
        free = -1;
        for (i = 0; i < OG_MAGIC_ACTORS; i++) {
            a = &og_magic_state()->actor[i];
            if (!a->active && free < 0) { free = i; }
            if (a->active && same(a->key,og_magic_key(index))) { free = i; break; }
        }
        if (free >= 0) {
            a = &og_magic_state()->actor[free]; a->active = 1; a->key = og_magic_key(index);
            a->magicka = copy.magicka; a->fatigue = copy.fatigue;
        }
        og_dungeon_spell_commit(index,&copy,caster);
    }
}

static int divide(int value, int by) { return by ? value/by : 0; }
int og_spell_cost(const OgSpellDef *s, const OgCharacter *caster)
{
    const df_i16 *c;
    const int *d, *ch, *m;
    int k, cost, total, formula, skill;
    if (!s || !caster) { return 0; }
    total = 0;
    for (k = 0; k < 3; k++) {
        if (s->group[k] == 255) { continue; }
        c = og_effect_costs(s->group[k],s->sub[k]); if (!c) { continue; }
        d = s->duration[k]; ch = s->chance[k]; m = s->magnitude[k];
        formula = og_effect_formula(s->group[k]); cost = 0;
        if (formula == 1) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]+c[2]*ch[0]+divide(ch[1],ch[2])*c[3]; }
        else if (formula == 2) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]+c[2]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[3]; }
        else if (formula == 3) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]; }
        else if (formula == 4) { cost = c[0]*ch[0]+divide(ch[1],ch[2])*c[1]; }
        else if (formula == 5) { cost = c[0]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[1]; }
        else if (formula == 6) { cost = c[2]*((m[0]+m[1])/2)+((m[2]+m[3])/2)*divide(c[3],m[4])+c[0]*d[0]+divide(d[1]*c[1],d[2]); }
        else if (formula == 7) { cost = divide((df_i16)(c[0]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[1])*d[0],d[1]); }
        skill = caster->skill[og_effect_school(s->group[k])];
        total += ((110-skill)*(df_i16)cost)/100;
    }
    total = (og_target_factor(s->target)*total) >> 1;
    return total < 5 ? 5 : total;
}
int og_magic_chance(int target, int group, int sub)
{
    OgEffect *e;
    int i;
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &og_magic_state()->effect[i];
        if (e->active && e->group == group && (sub < 0 || e->sub == sub) && same(e->target,og_magic_key(target))) { return e->chance; }
    }
    return 0;
}
int og_magic_resist(int index, int element, int bit, int modifier)
{
    OgCharacter who;
    OgClass career;
    int base[8], chance, roll;
    if (!og_magic_target(index,&who,&career,base)) { return 0; }
    if ((who.conditions & og_resist_flag(element)) && og_random_range(1,100) <= og_magic_chance(index,8,element)) { return 0; }
    chance = 50;
    if (career.immunity & bit) { return 0; }
    if (career.weakness & bit) { return 100; }
    if (career.tolerance & bit) { chance >>= 1; }
    if (career.resistance & bit) { chance += chance >> 1; }
    chance += modifier;
    if ((element == 1 && who.race == 2) || (element == 4 && who.race == 0)) { chance += 30; }
    if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
    roll = og_random_range(1,100);
    if (roll > chance) { return 100; }
    if (chance-20 > roll) { return 0; }
    return (chance-roll)*5;
}

static int condition(int group)
{
    if (group == 0) { return 1; }
    if (group >= 13 && group <= 32) { return 1 << (group-11); }
    if (group == 35) { return 0x400000; }
    if (group == 39) { return 0x800000; }
    if (group == 42) { return 0x1000000; }
    return group >= 44 && group <= 46 ? 1 << (group-19) : 0;
}
static void end(OgEffect *e)
{
    OgCharacter who;
    OgClass career;
    int base[8], index;
    index = og_magic_index(e->target); e->active = 0;
    if (!og_magic_target(index,&who,&career,base)) { return; }
    who.conditions &= ~(e->group == 8 ? (int)og_resist_flag(e->sub) : condition(e->group));
    if (e->group == 9 && e->sub < 8) { who.attribute[e->sub] -= e->change; }
    og_magic_commit(index,&who,-1);
}
void og_magic_remove(int target, int group)
{
    int i;
    for (i = 0; i < OG_EFFECTS; i++) {
        OgEffect *e;
        e = &og_magic_state()->effect[i];
        if (e->active && same(e->target,og_magic_key(target)) && (group < 0 || e->group == group)) { end(e); }
    }
}
void og_magic_hostile(void)
{
    int i;
    OgEffect *e;
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &og_magic_state()->effect[i];
        if (e->active && !e->target.place && !e->target.thing && e->sub == 0 &&
            (e->group == 13 || e->group == 23 || e->group == 24)) { end(e); }
    }
}
int og_magic_sees(int kind)
{
    int conditions;
    conditions = og_character()->conditions;
    if (!(conditions & 0x3004) || og_creature_ignores_illusion(kind)) { return 1; }
    if (conditions & 4) { return 0; }
    return og_random_range(1,100) <= (conditions & 0x2000 ? 8 : 4);
}
static OgEffect *extend_or_allocate(const OgEffect *value)
{
    OgEffect *e, *free;
    int i;
    free = 0;
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &og_magic_state()->effect[i];
        if (!e->active) { if (!free) { free = e; } continue; }
        if (e->group == value->group && e->sub == value->sub && same(e->target,value->target)) {
            e->rounds = (e->rounds+value->rounds) & 65535; return 0;
        }
    }
    return free;
}

static void damage(OgCharacter *who, int sub, int magnitude)
{
    if (sub == 0) { who->health -= magnitude; }
    else if (sub == 1) { who->fatigue -= magnitude; }
    else if (sub == 2) { who->magicka -= magnitude; }
}
static void heal(OgCharacter *who, const int *base, int sub, int magnitude)
{
    if (sub < 8) { who->attribute[sub] += magnitude; if (who->attribute[sub] > base[sub]) { who->attribute[sub] = base[sub]; } }
    else if (sub == 8) { who->health += magnitude; }
    else if (sub == 9) { who->fatigue += magnitude*64; }
    else if (sub == 10) { who->magicka += magnitude; }
}

static int g_dispel[OG_EFFECTS], g_dispel_count, g_dispel_chance;
static void dispel_answer(const char *answer)
{
    OgEffect *e;
    OgEffect selected;
    int row, chance, i;
    if (!answer) { return; }
    row = 0;
    while (*answer >= '0' && *answer <= '9' && row < OG_EFFECTS+1) { row = row*10+*answer++-'0'; }
    if (*answer || row < 1 || row > g_dispel_count) { return; }
    e = &og_magic_state()->effect[g_dispel[row-1]];
    if (!e->active) { return; }
    chance = g_dispel_chance+(og_character()->level-e->level)*5;
    if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
    if ((!e->caster.place && !e->caster.thing) || og_random_range(1,100) <= chance) {
        selected = *e;
        for (i = 0; i < OG_EFFECTS; i++) {
            e = &og_magic_state()->effect[i];
            if (e->active && e->spell == selected.spell && same(e->caster,selected.caster) &&
                same(e->target,selected.target)) { end(e); }
        }
        og_msg_set("The spell has been dispelled.");
    } else { og_msg_set("The spell resisted your attempt."); }
}
static void dispel_prompt(int target, const OgEffect *effect)
{
    char text[2048], line[96];
    OgEffect *e;
    int i, j, used;
    df_copy(text,"Dispel Magic: enter the number to remove.\n",sizeof(text));
    used = length(text); g_dispel_count = 0; g_dispel_chance = effect->chance;
    for (i = 0; i < OG_EFFECTS && used < 1850; i++) {
        e = &og_magic_state()->effect[i];
        if (!e->active || !same(e->target,og_magic_key(target))) { continue; }
        for (j = 0; j < g_dispel_count; j++) {
            const OgEffect *prior;
            prior = &og_magic_state()->effect[g_dispel[j]];
            if (prior->spell == e->spell && same(prior->caster,e->caster)) { break; }
        }
        if (j < g_dispel_count) { continue; }
        g_dispel[g_dispel_count++] = i;
        log_format(line,sizeof(line),"%d. %s\n",g_dispel_count,og_spell_def(e->spell)->name);
        df_copy(text+used,line,sizeof(text)-used); used = length(text);
    }
    if (g_dispel_count) { og_possessions_message(text,dispel_answer); }
    else { og_msg_set("There are no spells to dispel."); }
}

static void apply_effect(OgEffect *e, int index, int caster)
{
    OgCharacter who, giver;
    OgClass career;
    int base[8], other_base[8], group, sub, before, chance;
    if (!og_magic_target(index,&who,&career,base)) { return; }
    group = e->group; sub = e->sub;
    if (group == 0) {
        if (who.race == 4 || (index == -1 && (career.immunity & 1)) || (who.conditions & 0x8001) || og_random_range(1,100) > e->chance) { e->active = 0; return; }
    }
    if (group == 12 && index >= 0 && og_dungeon_kind(index) >= 128) {
        e->active = 0; og_msg_set("That creature has no soul you can trap."); return;
    }
    if ((group == 3 || group == 5 || group == 19 || group == 33 || group == 34) && og_random_range(1,100) > e->chance) { e->active = 0; return; }
    if (group == 8) { who.conditions |= og_resist_flag(sub); }
    else { who.conditions |= condition(group); }
    if (group == 1 || group == 4) { damage(&who,sub,e->magnitude); }
    else if (group == 5) { who.health = 0; }
    else if (group == 3) {
        if (sub < 2) { og_ailments_cure(index,sub == 1); }
        else if (sub == 2) { og_magic_remove(index,0); }
        return;
    } else if (group == 6) {
        if (sub == 0) { dispel_prompt(index,e); return; }
        else {
            int i, level, chance;
            OgCharacter victim;
            for (i = 0; i < og_dungeon_count(); i++) {
                if (!og_dispel_kind(og_dungeon_kind(i),sub-1) || !og_magic_target(i,&victim,&career,base)) { continue; }
                level = e->level; chance = e->chance+(level-victim.level)*5;
                if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
                if (og_random_range(1,100) <= chance) { victim.health = 0; og_magic_commit(i,&victim,caster); }
            }
            return;
        }
    } else if (group == 7 || group == 11) {
        before = e->magnitude; if (group == 11) { before >>= 1; }
        if (sub < 8) { who.attribute[sub] -= before; if (who.attribute[sub] < 1) { who.attribute[sub] = 1; } }
        else if (sub == 8) { who.health -= before; }
        else if (sub == 9) { who.fatigue -= before; if (who.fatigue < 1) { who.fatigue = 1; } }
        if (group == 11 && og_magic_target(caster,&giver,&career,other_base)) {
            heal(&giver,other_base,sub,before); og_magic_commit(caster,&giver,caster);
        }
    } else if (group == 9 && sub < 8) {
        before = who.attribute[sub]; who.attribute[sub] += e->magnitude;
        if (who.attribute[sub] > 100) { who.attribute[sub] = 100; }
        e->change = who.attribute[sub]-before;
    } else if (group == 10) { heal(&who,base,sub,e->magnitude); }
    else if (group == 26) { og_magic_remove(index,0); who.conditions &= ~1; }
    else if (group == 33 || group == 34) {
        if (index >= 0 && ((og_dungeon_kind(index) >= 128) == (group == 34))) { og_dungeon_pacify(index); }
    }
    else if (group == 43 && index == -1) { og_magic_recall_prompt(); }
    chance = who.health;
    og_magic_commit(index,&who,caster);
    if (index == -1 && group == 0 && chance > 0) { og_msg_set("You are paralyzed."); }
}

void og_magic_soul_capture(int index)
{
    OgEffect *e;
    const OgObject *item, *owner;
    df_u32 gem, soul;
    int i, j, depth, occupied, kind;
    kind = og_dungeon_kind(index); if (kind < 0 || kind >= 43) { return; }
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &og_magic_state()->effect[i];
        if (!e->active || e->group != 12 || !same(e->target,og_magic_key(index))) { continue; }
        gem = 0;
        for (j = 0; j < OG_OBJECTS; j++) {
            item = &og_state()->object[j];
            if (!item->id || item->type != OG_RECORD_ITEM || item->group != 27 || item->index != 1) { continue; }
            occupied = 0;
            /* A soul is not an inventory item, so inspect all owned children. */
            for (depth = 0; depth < OG_OBJECTS; depth++) {
                if (og_state()->object[depth].id && og_state()->object[depth].parent == item->id) { occupied = 1; break; }
            }
            if (occupied) { continue; }
            owner = item;
            for (depth = 0; owner && depth < OG_OBJECTS; depth++) {
                if (owner->parent == og_state()->entity_id) { gem = item->id; break; }
                owner = og_object(owner->parent);
            }
        }
        if (!gem) { og_msg_set("You have no empty soul trap."); return; }
        if (og_random_range(0,100) > e->chance) { return; }
        soul = og_object_new(OG_RECORD_SOUL,gem,kind,100);
        if (soul) { og_msg_set("The soul has been trapped."); }
        return;
    }
}

void og_magic_apply(int spell, int caster, int target, int level, int no_resist)
{
    const OgSpellDef *s;
    OgCharacter from, to;
    OgClass career;
    OgEffect value, *stored;
    int base[8], k, flags, percent, rounds;
    s = og_spell_def(spell);
    if (!s || !og_magic_target(caster,&from,&career,base) || !og_magic_target(target,&to,&career,base)) { return; }
    if (level <= 0) { level = from.level; }
    if (s->icon >= 200) { level = 8; }
    if (target == -1 && og_god_mode() && caster != -1) { return; }
    if (!no_resist && s->target != 0) {
        if ((to.conditions & 0x200) || (career.absorption & 6)) {
            if (!(to.conditions & 0x200) || og_random_range(1,100) <= og_magic_chance(target,20,-1)) {
                k = og_spell_cost(s,&to);
                if (to.magicka+k <= to.max_magicka) { to.magicka += k; og_magic_commit(target,&to,caster); return; }
            }
        }
        if ((to.conditions & 0x400) && og_random_range(1,100) <= og_magic_chance(target,21,-1)) {
            target = caster; to = from;
        }
        if ((to.conditions & 0x800) && og_random_range(1,100) <= og_magic_chance(target,22,-1)) { return; }
    }
    percent = no_resist || s->target == 0 ? 100 : og_magic_resist(target,s->element,og_element_bit(s->element),-(level-to.level)*5);
    if (!percent) { og_msg_set("The spell has no effect."); return; }
    for (k = 0; k < 3; k++) {
        if (s->group[k] == 255) { continue; }
        flags = og_effect_flags(s->group[k],s->sub[k]); mem_zero(&value,sizeof(value));
        value.active = 1; value.target = og_magic_key(target); value.caster = og_magic_key(caster);
        value.group = s->group[k]; value.sub = s->sub[k]; value.spell = spell;
        value.level = level; value.icon = s->icon; value.element = s->element;
        rounds = s->duration[k][0]+s->duration[k][1]*divide(level,s->duration[k][2]);
        value.rounds = (df_u16)(rounds*percent/100);
        value.permanent = s->duration[k][0] == -1;
        value.chance = (df_u8)(s->chance[k][0]+s->chance[k][1]*divide(level,s->chance[k][2]));
        if (flags & 4) {
            value.magnitude = (df_u16)((og_random_range(s->magnitude[k][0],s->magnitude[k][1])+
                divide(level,s->magnitude[k][4])*og_random_range(s->magnitude[k][2],s->magnitude[k][3]))*percent/100);
        }
        if ((flags & 1) || value.group == 16 || value.group == 17) {
            if (value.group == 16 || value.group == 17) { value.permanent = 1; }
            stored = extend_or_allocate(&value);
            if (!stored) { continue; }
            *stored = value; apply_effect(stored,target,caster);
        } else { apply_effect(&value,target,caster); }
    }
}

void og_magic_minutes(df_u32 start, df_u32 count)
{
    OgEffect *e;
    OgCharacter who;
    OgClass career;
    df_u32 n;
    int i, target, caster, base[8];
    for (n = 0; n < count && og_character()->health > 0; n++) {
        for (i = 0; i < OG_EFFECTS; i++) {
            e = &og_magic_state()->effect[i];
            if (!e->active || e->permanent) { continue; }
            if (!e->rounds) { end(e); continue; }
            e->rounds--; target = og_magic_index(e->target); caster = og_magic_index(e->caster);
            if (!og_magic_target(target,&who,&career,base)) { continue; }
            if (e->group == 1) { damage(&who,e->sub,e->magnitude); }
            if (e->group == 18) { heal(&who,base,8,e->magnitude); }
            og_magic_commit(target,&who,caster);
        }
        og_ailments_minutes(start+n,1);
    }
}
void og_magic_restore_foe(int index)
{
    OgCharacter who;
    OgClass career;
    OgEffect *e;
    OgAilment *a;
    int i, k, base[8];
    if (!og_magic_target(index,&who,&career,base)) { return; }
    for (i = 0; i < OG_MAGIC_ACTORS; i++) {
        const OgMagicActor *a;
        a = &og_magic_state()->actor[i];
        if (a->active && same(a->key,og_magic_key(index))) { who.magicka = a->magicka; who.fatigue = a->fatigue; break; }
    }
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &og_magic_state()->effect[i];
        if (!e->active || !same(e->target,og_magic_key(index))) { continue; }
        who.conditions |= e->group == 8 ? (int)og_resist_flag(e->sub) : condition(e->group);
        if (e->group == 9 && e->sub < 8) { who.attribute[e->sub] += e->change; }
    }
    for (i = 0; i < OG_AILMENTS; i++) {
        a = &og_magic_state()->ailment[i];
        if (a->active && same(a->target,og_magic_key(index))) {
            for (k = 0; k < 8; k++) { who.attribute[k] -= a->drained[k]; }
        }
    }
    og_dungeon_spell_commit(index,&who,-1);
}

int og_magic_absorb(int index, int damage)
{
    OgEffect *e;
    int i, amount;
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &og_magic_state()->effect[i];
        if (!e->active || e->group != 35 || !same(e->target,og_magic_key(index))) { continue; }
        amount = e->magnitude < damage ? e->magnitude : damage;
        e->magnitude -= amount; damage -= amount;
        if (!e->magnitude) { end(e); }
        break;
    }
    return damage;
}

int og_magic_valid(const OgMagicState *state)
{
    const OgEffect *e;
    const OgAilment *a;
    const OgMissile *m;
    int i, k;
    if (state->paid < 0 || state->paid > 1000000) { return 0; }
    if (state->recall.valid < 0 || state->recall.valid > 1 ||
        (state->recall.valid && (!state->recall.place ||
         !(state->recall.x >= -100000000 && state->recall.x <= 100000000) ||
         !(state->recall.y >= -100000000 && state->recall.y <= 100000000) ||
         !(state->recall.z >= -100000000 && state->recall.z <= 100000000) ||
         state->recall.yaw < 0 || state->recall.yaw >= 2048 || state->recall.pitch < -256 || state->recall.pitch > 256))) { return 0; }
    for (i = 0; i < OG_MAGIC_ACTORS; i++) {
        const OgMagicActor *a;
        a = &state->actor[i]; if (!a->active) { continue; }
        if (a->active != 1 || !a->key.place || !a->key.thing || a->magicka < 0 || a->magicka > 1000000 ||
            a->fatigue < 0 || a->fatigue > 1000000) { return 0; }
        for (k = 0; k < i; k++) {
            if (state->actor[k].active && same(a->key,state->actor[k].key)) { return 0; }
        }
    }
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &state->effect[i]; if (!e->active) { continue; }
        if (e->active != 1 || e->group < 0 || e->group >= 51 || e->sub < 0 || (e->sub >= 12 && e->sub != 255) ||
            e->rounds < 0 || e->rounds > 65535 || e->magnitude < 0 || e->magnitude > 65535 ||
            e->chance < 0 || e->chance > 255 || e->icon < 0 || e->icon > 255 || !og_spell_def(e->spell) || e->level < 1 || e->level > 1000 ||
            e->element < 0 || e->element > 4 || e->permanent < 0 || e->permanent > 1 ||
            e->change < -1000 || e->change > 1000) { return 0; }
    }
    for (i = 0; i < OG_AILMENTS; i++) {
        a = &state->ailment[i]; if (!a->active) { continue; }
        if (a->active != 1 || (!og_disease_def(a->id) && !og_poison_def(a->id)) ||
            a->delay < 0 || a->delay > 32767 || a->maximum < 0 || a->maximum > 32767 ||
            a->left < 0 || a->left > 32767 || a->stage < 0 || a->stage > 2) { return 0; }
        for (k = 0; k < 11; k++) { if (a->stat[k] < 0 || a->stat[k] > 1) { return 0; } }
        for (k = 0; k < 8; k++) { if (a->drained[k] < -100000 || a->drained[k] > 100000) { return 0; } }
    }
    for (i = 0; i < OG_MISSILES; i++) {
        m = &state->missile[i]; if (!m->live) { continue; }
        if (m->live != 1 || !og_spell_def(m->spell) || m->level < 1 || m->level > 1000 ||
            m->element < 0 || m->element > 4 || m->target < 0 || m->target > 4 ||
            !(m->x >= -100000000 && m->x <= 100000000) || !(m->y >= -100000000 && m->y <= 100000000) ||
            !(m->z >= -100000000 && m->z <= 100000000) || !(m->dx >= -1 && m->dx <= 1) ||
            !(m->dy >= -1 && m->dy <= 1) || !(m->dz >= -1 && m->dz <= 1) || m->burst < 0 || m->burst > 10000) { return 0; }
    }
    return 1;
}
int og_magic_indicators(void)
{
    int i, flags;
    flags = 0;
    for (i = 0; i < OG_EFFECTS; i++) {
        const OgEffect *e;
        e = &og_magic_state()->effect[i];
        if (e->active && !e->target.place && !e->target.thing) { flags |= 1; }
    }
    for (i = 0; i < OG_AILMENTS; i++) {
        const OgAilment *a;
        a = &og_magic_state()->ailment[i];
        if (a->active && a->stage && !a->target.place && !a->target.thing) { flags |= a->id >= 128 ? 2 : 4; }
    }
    return flags;
}
void og_magic_status(void)
{
    char text[2048], line[96];
    int i, used;
    OgEffect *e;
    OgAilment *a;
    df_copy(text,"Active magic and ailments:\n",sizeof(text)); used = length(text);
    for (i = 0; i < OG_EFFECTS && used < 1800; i++) {
        e = &og_magic_state()->effect[i];
        if (!e->active || e->target.place || e->target.thing) { continue; }
        log_format(line,sizeof(line),"%s: %d rounds\n",og_spell_effect(e->group,e->sub),e->rounds);
        df_copy(text+used,line,sizeof(text)-used); used = length(text);
    }
    for (i = 0; i < OG_AILMENTS && used < 1800; i++) {
        a = &og_magic_state()->ailment[i];
        if (!a->active || a->target.place || a->target.thing) { continue; }
        log_format(line,sizeof(line),"%s %d: %s\n",a->id >= 128 ? "Poison" : "Disease",a->id,a->stage ? "active" : "latent");
        df_copy(text+used,line,sizeof(text)-used); used = length(text);
    }
    /* status_show: TEXT.RSC 22 (the date, the time and where the player stands), then show_health_status: the ailments, or record 18 when there are none. Active spells are ours. */
    { static char intro[300]; if (og_status_intro(intro,sizeof(intro)) > 0) { og_possessions_message(intro,0); } }
    if (used == 26) { df_copy(text,og_words(18),sizeof(text)); }
    if (text[0]) { og_possessions_message(text,0); }
}
