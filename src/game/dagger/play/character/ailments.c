#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/character/vitals.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/tables/magicdefs.h"
#include "engine/core/mem.h"

static int same(KeepKey a, KeepKey b) { return a.place == b.place && a.thing == b.thing; }
static AilmentInfo *allocate(int target)
{
    int i;
    AilmentInfo *a;
    for (i = 0; i < AILMENTS; i++) {
        a = &magic_state()->ailment[i];
        if (!a->active) { mem_zero(a,sizeof(*a)); a->target = magic_key(target); a->active = 1; return a; }
    }
    msg_set("Too many active ailments."); return 0;
}

static int vulnerable(int target, int bit, int no_resist, int modifier)
{
    CharacterState who;
    ClassInfo career;
    int base[ATTRS];
    if (!magic_target(target,&who,&career,base) || who.level == 1 ||
        (target == -1 && (career.immunity & bit))) { return 0; }
    /* Preserve disease.c's comparison: it rejects a result of 100, not 0.
     * This differs from the spell bundle's damage-percentage interpretation. */
    return no_resist || magic_resist(target,RESIST_KIND_DISEASE,bit,modifier) != RESIST_FULL;
}

static int infect(int target, const df_u8 *list, int id, int no_resist)
{
    const DiseaseDef *def;
    AilmentInfo *a;
    int i, count;
    if (!vulnerable(target,EFFECT_BIT_DISEASE,no_resist,character_state()->bio_resist_disease)) { return 0; }
    if (list) {
        for (count = 0; list[count] != LIST_END; count++) { }
        if (!count) { return 0; }
        id = list[random_range(0,count-1)];
    }
    def = disease_def(id); if (!def) { return 0; }
    a = allocate(target); if (!a) { return 0; }
    a->id = def->id; a->delay = def->minimum; a->maximum = def->maximum;
    a->left = def->days == DISEASE_FOREVER ? DISEASE_FOREVER : random_range(def->days,def->stage);
    for (i = 0; i < DISEASE_STATS; i++) { a->stat[i] = def->stat[i]; }
    return 1;
}

int disease_infect(int target, int id, int no_resist) { return infect(target,0,id,no_resist); }

int poison_apply(int target, int id, int no_resist)
{
    const df_i16 *def;
    AilmentInfo *a;
    def = poison_def(id);
    if (!def || !vulnerable(target,EFFECT_BIT_POISON,no_resist,character_state()->bio_resist_poison)) { return 0; }
    a = allocate(target); if (!a) { return 0; }
    a->id = id; a->left = random_range(def[POISON_DAYS_MIN],def[POISON_DAYS_MAX]); a->delay = random_range(def[POISON_DELAY_MIN],def[POISON_DELAY_MAX]);
    return 1;
}

int ailments_disease_count(int target)
{
    AilmentInfo *a;
    int i, count;

    count = 0;
    for (i = 0; i < AILMENTS; i++) {
        a = &magic_state()->ailment[i];
        if (a->active && same(a->target,magic_key(target)) && a->id < DISEASE_CURSE_FIRST) { count++; }
    }
    /* A pending infection counts as one disease (9.1). */
    if (target == -1 && curse_infected()) { count++; }
    return count;
}

void ailments_cure(int target, int poison)
{
    CharacterState who;
    ClassInfo career;
    AilmentInfo *a;
    int base[ATTRS], i, k;
    if (!magic_target(target,&who,&career,base)) { return; }
    /* spfx_cure_disease clears a pending infection too (2.3). */
    if (target == -1 && !poison) { curse_clear_infection(); }
    for (i = 0; i < AILMENTS; i++) {
        a = &magic_state()->ailment[i];
        if (!a->active || !same(a->target,magic_key(target)) || (poison ? a->id < POISON_FIRST : a->id >= DISEASE_CURSE_FIRST)) { continue; }
        for (k = 0; k < ATTRS; k++) {
            who.attribute[k] += a->drained[k];
            if (who.attribute[k] > base[k]) { who.attribute[k] = base[k]; }
        }
        a->active = 0;
    }
    magic_commit(target,&who,-1);
}

static void drain(AilmentInfo *a, CharacterState *who, int stat, int amount, int lethal)
{
    int before;
    before = who->attribute[stat]; who->attribute[stat] -= amount;
    if (who->attribute[stat] <= 0 && lethal) { who->health = 0; }
    if (who->attribute[stat] < 1) { who->attribute[stat] = 1; }
    a->stat[stat] = 1; a->drained[stat] += before-who->attribute[stat];
}

/* One step of a poison (poison_tick, disease.c 150..306), each with its own ranges: health and magicka lost, fatigue lost in raw units (the two sleep poisons) or gained in FATIGUE_UNITs
 * (Quaesto Vil), and attributes drained (a negative drain is a gain). Those with an attribute effect are done after their first step. */
static void poison_tick(AilmentInfo *a, CharacterState *who)
{
    if (a->delay) { a->delay--; return; }
    if (!a->stage) { a->stage = 1; }
    switch (a->id-POISON_FIRST) {
    case POISON_NUX_VOMICA: who->health -= random_range(2,12); break;
    case POISON_ARSENIC: who->health -= 2; drain(a,who,ATTR_END,1,0); a->stage = POISON_STAGE_DONE; break;
    case POISON_MOONSEED: who->health -= random_range(1,10); break;
    case POISON_DROTHWEED:
        drain(a,who,ATTR_STR,random_range(5,10),0); drain(a,who,ATTR_AGI,random_range(1,5),0);
        drain(a,who,ATTR_SPD,random_range(1,5),0); a->stage = POISON_STAGE_DONE; break;
    case POISON_SOMNALIUS: who->fatigue -= random_range(10,100); break;
    case POISON_PYRRHIC_ACID: who->health -= random_range(1,30); break;
    case POISON_MAGEBANE: drain(a,who,ATTR_WIL,random_range(1,5),0); who->magicka -= random_range(5,15); a->stage = POISON_STAGE_DONE; break;
    case POISON_THYRWORT: drain(a,who,ATTR_WIL,random_range(5,20),0); drain(a,who,ATTR_PER,random_range(10,20),0); a->stage = POISON_STAGE_DONE; break;
    case POISON_INDULCET: who->fatigue -= random_range(10,100); drain(a,who,ATTR_LUC,-random_range(4,10),0); a->stage = POISON_STAGE_DONE; break;
    case POISON_SURSUM: drain(a,who,ATTR_INT,random_range(10,30),0); drain(a,who,ATTR_STR,-random_range(5,20),0); a->stage = POISON_STAGE_DONE; break;
    case POISON_QUAESTO_VIL: who->fatigue += random_range(5,10)*FATIGUE_UNIT; drain(a,who,ATTR_WIL,random_range(1,4),0); a->stage = POISON_STAGE_DONE; break;
    case POISON_AEGROTAT: who->magicka += random_range(5,10); drain(a,who,ATTR_END,random_range(1,5),0); a->stage = POISON_STAGE_DONE; break;
    }
    if (a->left) { a->left--; }
    else if (a->stage == POISON_STAGE_DONE) { a->left = AILMENT_RECOVERING; a->id = 0; }
    else { a->active = 0; }
    if (!a->target.place && !a->target.thing) { msg_set("You feel somewhat bad."); }
}

static void daily(AilmentInfo *a, CharacterState *who)
{
    int damage, i;
    if (a->id >= DISEASE_CURSE_FIRST) { return; }
    damage = random_range(a->delay,a->maximum);
    if (a->left == AILMENT_RECOVERING) { return; }
    if (a->left != DISEASE_FOREVER && --a->left == 0) { a->left = AILMENT_RECOVERING; return; }
    a->stage = 1;
    for (i = 0; i < DISEASE_STATS; i++) {
        if (!a->stat[i]) { continue; }
        if (i < ATTRS) { drain(a,who,i,damage,1); }
        else if (i == DISEASE_STAT_HEALTH) { who->health -= damage; }
        else if (i == DISEASE_STAT_FATIGUE) { who->fatigue -= damage; }
        else { who->magicka -= damage; }
    }
    if (!a->target.place && !a->target.thing) { msg_set("You feel somewhat bad."); }
}

void ailments_minutes(df_u32 start, df_u32 count)
{
    AilmentInfo *a;
    CharacterState who;
    ClassInfo career;
    int i, k, index, changed, base[ATTRS];
    df_u32 minute;
    for (minute = start+1; count--; minute++) {
        for (i = 0; i < AILMENTS; i++) {
            a = &magic_state()->ailment[i]; if (!a->active) { continue; }
            index = magic_index(a->target);
            if (!magic_target(index,&who,&career,base) || who.health <= 0) { continue; }
            if (!(minute%MINUTES_PER_DAY)) { daily(a,&who); }
            if (a->id >= POISON_FIRST) { poison_tick(a,&who); }
            else if (a->left == AILMENT_RECOVERING) {
                changed = 0;
                for (k = 0; k < ATTRS; k++) {
                    if (!a->drained[k]) { continue; }
                    if (a->drained[k] > 0 && who.attribute[k] < base[k]) { who.attribute[k]++; a->drained[k]--; changed = 1; }
                    else if (a->drained[k] < 0 && who.attribute[k] > base[k]) { who.attribute[k]--; a->drained[k]++; changed = 1; }
                }
                if (!changed) { a->active = 0; }
            }
            magic_commit(index,&who,-1);
        }
    }
}

void ailments_strike(int creature, int kind)
{
    int infection, cursed;
    infection = 0;
    /* A werewolf's, wereboar's or vampire's blow may pass on its curse; a vampire's that did not may still give a plague (damage_monster_hit_effects). */
    cursed = curse_blow(kind);
    if (kind == ENEMY_RAT && random_range(0,PERCENT) <= RAT_DISEASE_CHANCE) { infection = 1; }
    if (kind == ENEMY_GIANT_BAT && random_range(0,PERCENT) <= BAT_DISEASE_CHANCE) { infection = 1; }
    if (kind == ENEMY_MUMMY && random_range(1,PERCENT) <= MUMMY_DISEASE_CHANCE) { infection = 1; }
    if ((kind == ENEMY_VAMPIRE || kind == ENEMY_VAMPIRE_ANCIENT) && !cursed && random_range(1,PERCENT) <= VAMPIRE_DISEASE_CHANCE) { infection = 1; }
    if (infection) { infect(-1,creature_diseases(kind),0,0); }
    if (kind == ENEMY_NYMPH) { fatigue_spend(random_range(NYMPH_FATIGUE_MIN,NYMPH_FATIGUE_MAX)*FATIGUE_UNIT); }
    if ((kind == ENEMY_SPIDER || kind == ENEMY_GIANT_SCORPION) && !magic_chance(-1,0,-1)) { magic_apply(SPELL_CREATURE_PARALYSIS,creature,-1,0,0); }
}

const char *poison_name(int id)
{
    static const char *const name[] = {
        "Nux Vomica", "Arsenic", "Moonseed", "Drothweed", "Somnalius", "Pyrrhic Acid", "Magebane", "Thyrwort", "Indulcet", "Sursum", "Quaesto Vil", "Aegrotat"
    };

    if (id >= POISON_FIRST) { id -= POISON_FIRST; }
    return id >= 0 && id < (int)(sizeof(name)/sizeof(name[0])) ? name[id] : "";
}
