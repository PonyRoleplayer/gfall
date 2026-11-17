#include "game/dagger/play/combat/combat.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/mem.h"

static float g_flash;

void fighter_player(FighterInfo *out)
{
    const CharacterState *who;
    int i;
    who = character_state();
    mem_zero(out,sizeof(*out));
    /* A vampire is race 8, a beast 9 or 10, as the original reads them. */
    out->race = curse_race();
    out->level = who->level; out->health = who->health; out->maximum = who->max_health;
    out->flags = game_state()->career.flags;
    for (i = 0; i < ATTRS; i++) { out->attribute[i] = who->attribute[i]; }
    for (i = 0; i < SKILLS; i++) { out->skill[i] = who->skill[i]; }
    /* armor_value_for_part starts at 100; the sheet displays fifths. */
    for (i = 0; i < BODY_PARTS; i++) {
        out->armor[i] = ARMOR_BASE-inventory_armor(i)*ARMOR_PER_POINT+enchant_armor_modifier();
        if (out->armor[i] < -ARMOR_LIMIT) { out->armor[i] = -ARMOR_LIMIT; }
        if (out->armor[i] > ARMOR_LIMIT) { out->armor[i] = ARMOR_LIMIT; }
    }
}

static int race_bonus(int race, int weapon, int level)
{
    if ((race == PLAYER_RACE_REDGUARD && weapon < WEAPON_SHORT_BOW) || (race == PLAYER_RACE_WOOD_ELF && weapon >= WEAPON_SHORT_BOW)) { return level/RACE_BONUS_LEVEL_DIVISOR; }
    return race == PLAYER_RACE_DARK_ELF ? level/DARK_ELF_LEVEL_DIVISOR : 0;
}

int attack_chance(const FighterInfo *a, const FighterInfo *t, int skill, int weapon,
                     int material, int part, int swing, int critical)
{
    int chance;
    chance = skill + swing + material;
    if (a->health < a->maximum/LOW_HEALTH_DIVISOR && (a->flags & CLASS_ADRENALINE_RUSH)) { chance += ADRENALINE_CHANCE; }
    if (t->health < t->maximum/LOW_HEALTH_DIVISOR && (t->flags & CLASS_ADRENALINE_RUSH)) { chance -= ADRENALINE_CHANCE; }
    chance += (a->attribute[ATTR_AGI]-t->attribute[ATTR_AGI])/ATTRIBUTE_HIT_DIVISOR;
    chance += (a->attribute[ATTR_LUC]-t->attribute[ATTR_LUC])/ATTRIBUTE_HIT_DIVISOR;
    chance += t->armor[part >= 0 && part < BODY_PARTS ? part : 0];
    if (t->race < ENEMY_RACE_CLASS) { chance += MONSTER_HIT_BONUS; }
    chance += race_bonus(a->race,weapon,a->level);
    /* This is the ATTACKER's Dodging in the byte-matched original. */
    chance -= a->skill[SKILL_DODGING]/DODGE_DIVISOR;
    if (critical < a->skill[SKILL_CRITICAL_STRIKE]) { chance += a->skill[SKILL_CRITICAL_STRIKE]/CRITICAL_HIT_DIVISOR; }
    chance -= TO_HIT_OFFSET;
    if (chance < TO_HIT_MIN) { return TO_HIT_MIN; }
    return chance > TO_HIT_MAX ? TO_HIT_MAX : chance;
}

int attack_damage(const FighterInfo *a, const FighterInfo *t, int weapon, int material,
                     int swing, int expert, int base)
{
    int damage;
    if (weapon < 0) { return base; }
    damage = base + race_bonus(a->race,weapon,a->level) + swing;
    if (expert) { damage += a->level/EXPERT_LEVEL_DIVISOR+1; }
    if (t->race == ENEMY_SKELETAL_WARRIOR) {
        if (material == MATERIAL_SILVER) { damage *= 2; }
        /* Ordinary edged weapons halve against a skeletal warrior. */
        if (!(item_def(ITEM_GROUP_WEAPON,weapon)->flags & ITEM_FLAG_BLUNT)) { damage /= 2; }
    }
    damage += material_hit(material)/MATERIAL_DAMAGE_DIVISOR + (a->attribute[ATTR_STR]-STRENGTH_BASE)/STRENGTH_DAMAGE_DIVISOR;
    return damage < 1 ? 0 : damage;
}

void item_wear(df_u32 id, int damage)
{
    int loss;

    if (!object_at(id) || damage <= 0) { return; }
    loss = (damage*WEAR_PERCENT+WEAR_ROUND)/100;
    if (!loss && random_range(1,100) < WEAR_LAST_CHANCE) { loss = 1; }
    if (loss) { item_damage(id,loss); }
}

void player_slain(void)
{
    CharacterState *who;
    who = character_state();
    who->health = 0;
    state_event(EVENT_KILL,game_state()->entity_id,0,1);
    play_sound(who->female ? 258 : 243);
    msg_set("You have died.");
}

void damage_player(int damage, int part)
{
    CharacterState *who;
    int amount;
    who = character_state();
    if (damage <= 0 || who->health <= 0) { return; }
    damage = magic_absorb(-1,damage); if (!damage) { return; }
    /* damage_apply: while guards are about the first blow is the offer to surrender, held back until it is answered. It is the law's, not the body's: god mode does not skip it. */
    if (law_blow(game_state()->region) == BLOW_ASK && possessions_surrender()) { return; }
    if (god_mode()) { return; }
    who->health -= damage;
    /* Brought down among the guards: the court (or the end), not simply death. */
    if (who->health < 1 && guards_present() && possessions_arrested()) { return; }
    if (who->health < 0) { who->health = 0; }
    amount = damage < HURT_MIN ? HURT_MIN : damage > HURT_MAX ? HURT_MAX : damage;
    g_flash = (float)amount/HURT_MAX;
    state_event(who->health ? EVENT_HURT : EVENT_KILL,game_state()->entity_id,0,damage);
    if (armor_slot(part) >= 0) { item_wear(game_state()->equipped[armor_slot(part)],damage); }
    if (!who->health) {
        play_sound(who->female ? 258 : 243);
        msg_set("You have died.");
    }
}

void combat_step(float dt) { g_flash -= dt*HURT_FADE_PER_SECOND; if (g_flash < 0) { g_flash = 0; } }
void combat_reset(void) { g_flash = 0; }
float combat_flash(void) { return g_flash; }
