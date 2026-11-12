#include "game/dagger/play/combat.h"
#include "game/dagger/play/enchant.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/mem.h"

static float g_flash;

void fighter_player(FighterInfo *out)
{
    const CharacterState *who;
    int i;
    who = character_state();
    mem_zero(out,sizeof(*out));
    out->race = who->race;
    out->level = who->level; out->health = who->health; out->maximum = who->max_health;
    out->flags = game_state()->career.flags;
    for (i = 0; i < ATTRS; i++) { out->attribute[i] = who->attribute[i]; }
    for (i = 0; i < SKILLS; i++) { out->skill[i] = who->skill[i]; }
    /* armor_value_for_part starts at 100; the sheet displays fifths. */
    for (i = 0; i < 7; i++) {
        out->armor[i] = 100-inventory_armor(i)*5+enchant_armor_modifier();
        if (out->armor[i] < -100) { out->armor[i] = -100; }
        if (out->armor[i] > 100) { out->armor[i] = 100; }
    }
}

static int race_bonus(int race, int weapon, int level)
{
    if ((race == 1 && weapon < 16) || (race == 5 && weapon >= 16)) { return level/3; }
    return race == 3 ? level/4 : 0;
}

int attack_chance(const FighterInfo *a, const FighterInfo *t, int skill, int weapon,
                     int material, int part, int swing, int critical)
{
    int chance;
    chance = skill + swing + material;
    if (a->health < a->maximum/8 && (a->flags & 4)) { chance += 5; }
    if (t->health < t->maximum/8 && (t->flags & 4)) { chance -= 5; }
    chance += (a->attribute[ATTR_AGI]-t->attribute[ATTR_AGI])/10;
    chance += (a->attribute[ATTR_LUC]-t->attribute[ATTR_LUC])/10;
    chance += t->armor[part >= 0 && part < 7 ? part : 0];
    if (t->race < 43) { chance += 40; }
    chance += race_bonus(a->race,weapon,a->level);
    /* This is the ATTACKER's Dodging in the byte-matched original. */
    chance -= a->skill[20]/4;
    if (critical < a->skill[34]) { chance += a->skill[34]/10; }
    chance -= 50;
    if (chance < 3) { return 3; }
    return chance > 97 ? 97 : chance;
}

int attack_damage(const FighterInfo *a, const FighterInfo *t, int weapon, int material,
                     int swing, int expert, int base)
{
    int damage;
    if (weapon < 0) { return base; }
    damage = base + race_bonus(a->race,weapon,a->level) + swing;
    if (expert) { damage += a->level/3+1; }
    if (t->race == 15) {
        if (material == 2) { damage *= 2; }
        /* Ordinary edged weapons halve against a skeletal warrior. */
        if (!(item_def(3,weapon)->flags & 16)) { damage /= 2; }
    }
    damage += material_hit(material)/10 + (a->attribute[ATTR_STR]-50)/5;
    return damage < 1 ? 0 : damage;
}

void item_wear(df_u32 id, int damage)
{
    ObjectInfo *item;
    int loss;
    item = object_edit(id);
    if (!item || item->type != RECORD_ITEM || damage <= 0) { return; }
    loss = (damage*10+50)/100;
    if (!loss && random_range(0,99) < 20) { loss = 1; }
    item->condition -= loss;
    if (item->condition <= 0) {
        item->condition = 0;
        inventory_unequip(id);
        msg_set("Your equipment has broken.");
    }
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
    amount = damage < 4 ? 4 : damage > 32 ? 32 : damage;
    g_flash = (float)amount/32;
    state_event(who->health ? EVENT_HURT : EVENT_KILL,game_state()->entity_id,0,damage);
    if (armor_slot(part) >= 0) { item_wear(game_state()->equipped[armor_slot(part)],damage); }
    if (!who->health) {
        play_sound(who->female ? 258 : 243);
        msg_set("You have died.");
    }
}

void combat_step(float dt) { g_flash -= dt*2; if (g_flash < 0) { g_flash = 0; } }
void combat_reset(void) { g_flash = 0; }
float combat_flash(void) { return g_flash; }
