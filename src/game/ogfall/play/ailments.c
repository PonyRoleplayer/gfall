#include "game/ogfall/play/ailments.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/tables/magicdefs.h"
#include "engine/core/mem.h"

static int same(KeepKey a, KeepKey b) { return a.place == b.place && a.thing == b.thing; }
static OgAilment *allocate(int target)
{
    int i;
    OgAilment *a;
    for (i = 0; i < OG_AILMENTS; i++) {
        a = &og_magic_state()->ailment[i];
        if (!a->active) { mem_zero(a,sizeof(*a)); a->target = og_magic_key(target); a->active = 1; return a; }
    }
    og_msg_set("Too many active ailments."); return 0;
}

static int vulnerable(int target, int bit, int no_resist)
{
    OgCharacter who;
    OgClass career;
    int base[8];
    if (!og_magic_target(target,&who,&career,base) || who.level == 1 ||
        (target == -1 && (career.immunity & bit))) { return 0; }
    /* Preserve disease.c's comparison: it rejects a result of 100, not 0.
     * This differs from the spell bundle's damage-percentage interpretation. */
    return no_resist || og_magic_resist(target,2,bit,0) != 100;
}

static int infect(int target, const df_u8 *list, int id, int no_resist)
{
    const OgDiseaseDef *def;
    OgAilment *a;
    int i, count;
    if (!vulnerable(target,64,no_resist)) { return 0; }
    if (list) {
        for (count = 0; list[count] != 255; count++) { }
        if (!count) { return 0; }
        id = list[og_random_range(0,count-1)];
    }
    def = og_disease_def(id); if (!def) { return 0; }
    a = allocate(target); if (!a) { return 0; }
    a->id = def->id; a->delay = def->minimum; a->maximum = def->maximum;
    a->left = def->days == 255 ? 255 : og_random_range(def->days,def->stage);
    for (i = 0; i < 11; i++) { a->stat[i] = def->stat[i]; }
    return 1;
}

int og_disease_infect(int target, int id, int no_resist) { return infect(target,0,id,no_resist); }

int og_poison_apply(int target, int id, int no_resist)
{
    const df_i16 *def;
    OgAilment *a;
    def = og_poison_def(id);
    if (!def || !vulnerable(target,4,no_resist)) { return 0; }
    a = allocate(target); if (!a) { return 0; }
    a->id = id; a->left = og_random_range(def[2],def[3]); a->delay = og_random_range(def[0],def[1]);
    return 1;
}

void og_ailments_cure(int target, int poison)
{
    OgCharacter who;
    OgClass career;
    OgAilment *a;
    int base[8], i, k;
    if (!og_magic_target(target,&who,&career,base)) { return; }
    for (i = 0; i < OG_AILMENTS; i++) {
        a = &og_magic_state()->ailment[i];
        if (!a->active || !same(a->target,og_magic_key(target)) || (poison ? a->id < 128 : a->id >= 100)) { continue; }
        for (k = 0; k < 8; k++) {
            who.attribute[k] += a->drained[k];
            if (who.attribute[k] > base[k]) { who.attribute[k] = base[k]; }
        }
        a->active = 0;
    }
    og_magic_commit(target,&who,-1);
}

static void drain(OgAilment *a, OgCharacter *who, int stat, int amount, int lethal)
{
    int before;
    before = who->attribute[stat]; who->attribute[stat] -= amount;
    if (who->attribute[stat] <= 0 && lethal) { who->health = 0; }
    if (who->attribute[stat] < 1) { who->attribute[stat] = 1; }
    a->stat[stat] = 1; a->drained[stat] += before-who->attribute[stat];
}

static void poison_tick(OgAilment *a, OgCharacter *who)
{
    if (a->delay) { a->delay--; return; }
    if (!a->stage) { a->stage = 1; }
    switch (a->id-128) {
    case 0: who->health -= og_random_range(2,12); break;
    case 1: who->health -= 2; drain(a,who,4,1,0); a->stage = 2; break;
    case 2: who->health -= og_random_range(1,10); break;
    case 3:
        drain(a,who,0,og_random_range(5,10),0); drain(a,who,3,og_random_range(1,5),0);
        drain(a,who,6,og_random_range(1,5),0); a->stage = 2; break;
    case 4: who->fatigue -= og_random_range(10,100); break;
    case 5: who->health -= og_random_range(1,30); break;
    case 6: drain(a,who,2,og_random_range(1,5),0); who->magicka -= og_random_range(5,15); a->stage = 2; break;
    case 7: drain(a,who,2,og_random_range(5,20),0); drain(a,who,5,og_random_range(10,20),0); a->stage = 2; break;
    case 8: who->fatigue -= og_random_range(10,100); drain(a,who,7,-og_random_range(4,10),0); a->stage = 2; break;
    case 9: drain(a,who,1,og_random_range(10,30),0); drain(a,who,0,-og_random_range(5,20),0); a->stage = 2; break;
    case 10: who->fatigue += og_random_range(5,10)*64; drain(a,who,2,og_random_range(1,4),0); a->stage = 2; break;
    case 11: who->magicka += og_random_range(5,10); drain(a,who,4,og_random_range(1,5),0); a->stage = 2; break;
    }
    if (a->left) { a->left--; }
    else if (a->stage == 2) { a->left = 254; a->id = 0; }
    else { a->active = 0; }
    if (!a->target.place && !a->target.thing) { og_msg_set("You feel somewhat bad."); }
}

static void daily(OgAilment *a, OgCharacter *who)
{
    int damage, i;
    if (a->id > 99) { return; }
    damage = og_random_range(a->delay,a->maximum);
    if (a->left == 254) { return; }
    if (a->left != 255 && --a->left == 0) { a->left = 254; return; }
    a->stage = 1;
    for (i = 0; i < 11; i++) {
        if (!a->stat[i]) { continue; }
        if (i < 8) { drain(a,who,i,damage,1); }
        else if (i == 8) { who->health -= damage; }
        else if (i == 9) { who->fatigue -= damage; }
        else { who->magicka -= damage; }
    }
    if (!a->target.place && !a->target.thing) { og_msg_set("You feel somewhat bad."); }
}

void og_ailments_minutes(df_u32 start, df_u32 count)
{
    OgAilment *a;
    OgCharacter who;
    OgClass career;
    int i, k, index, changed, base[8];
    df_u32 minute;
    for (minute = start+1; count--; minute++) {
        for (i = 0; i < OG_AILMENTS; i++) {
            a = &og_magic_state()->ailment[i]; if (!a->active) { continue; }
            index = og_magic_index(a->target);
            if (!og_magic_target(index,&who,&career,base) || who.health <= 0) { continue; }
            if (!(minute%1440)) { daily(a,&who); }
            if (a->id >= 128) { poison_tick(a,&who); }
            else if (a->left == 254) {
                changed = 0;
                for (k = 0; k < 8; k++) {
                    if (!a->drained[k]) { continue; }
                    if (a->drained[k] > 0 && who.attribute[k] < base[k]) { who.attribute[k]++; a->drained[k]--; changed = 1; }
                    else if (a->drained[k] < 0 && who.attribute[k] > base[k]) { who.attribute[k]--; a->drained[k]++; changed = 1; }
                }
                if (!changed) { a->active = 0; }
            }
            og_magic_commit(index,&who,-1);
        }
    }
}

void og_ailments_strike(int creature, int kind)
{
    int infection;
    infection = 0;
    if (kind == 0 && og_random_range(0,100) <= 5) { infection = 1; }
    if (kind == 3 && og_random_range(0,100) <= 2) { infection = 1; }
    if (kind == 19 && og_random_range(1,100) <= 5) { infection = 1; }
    if ((kind == 28 || kind == 30) && og_random_range(1,100) <= 2) { infection = 1; }
    if (infection) { infect(-1,og_creature_diseases(kind),0,0); }
    if (kind == 10) { og_fatigue_spend(og_random_range(10,30)*64); }
    if ((kind == 6 || kind == 20) && !og_magic_chance(-1,0,-1)) { og_magic_apply(66,creature,-1,0,0); }
}
