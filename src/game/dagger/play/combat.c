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

void og_fighter_player(OgFighter *out)
{
    const OgCharacter *who;
    int i;
    who = og_character();
    mem_zero(out,sizeof(*out));
    out->race = who->race;
    out->level = who->level; out->health = who->health; out->maximum = who->max_health;
    out->flags = og_state()->career.flags;
    for (i = 0; i < OG_ATTRS; i++) { out->attribute[i] = who->attribute[i]; }
    for (i = 0; i < OG_SKILLS; i++) { out->skill[i] = who->skill[i]; }
    /* armor_value_for_part starts at 100; the sheet displays fifths. */
    for (i = 0; i < 7; i++) {
        out->armor[i] = 100-og_inventory_armor(i)*5+og_enchant_armor_modifier();
        if (out->armor[i] < -100) { out->armor[i] = -100; }
        if (out->armor[i] > 100) { out->armor[i] = 100; }
    }
}

static int race_bonus(int race, int weapon, int level)
{
    if ((race == 1 && weapon < 16) || (race == 5 && weapon >= 16)) { return level/3; }
    return race == 3 ? level/4 : 0;
}

int og_attack_chance(const OgFighter *a, const OgFighter *t, int skill, int weapon,
                     int material, int part, int swing, int critical)
{
    int chance;
    chance = skill + swing + material;
    if (a->health < a->maximum/8 && (a->flags & 4)) { chance += 5; }
    if (t->health < t->maximum/8 && (t->flags & 4)) { chance -= 5; }
    chance += (a->attribute[OG_AGI]-t->attribute[OG_AGI])/10;
    chance += (a->attribute[OG_LUC]-t->attribute[OG_LUC])/10;
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

int og_attack_damage(const OgFighter *a, const OgFighter *t, int weapon, int material,
                     int swing, int expert, int base)
{
    int damage;
    if (weapon < 0) { return base; }
    damage = base + race_bonus(a->race,weapon,a->level) + swing;
    if (expert) { damage += a->level/3+1; }
    if (t->race == 15) {
        if (material == 2) { damage *= 2; }
        /* Ordinary edged weapons halve against a skeletal warrior. */
        if (!(og_item_def(3,weapon)->flags & 16)) { damage /= 2; }
    }
    damage += og_material_hit(material)/10 + (a->attribute[OG_STR]-50)/5;
    return damage < 1 ? 0 : damage;
}

void og_item_wear(df_u32 id, int damage)
{
    OgObject *item;
    int loss;
    item = og_object_edit(id);
    if (!item || item->type != OG_RECORD_ITEM || damage <= 0) { return; }
    loss = (damage*10+50)/100;
    if (!loss && og_random_range(0,99) < 20) { loss = 1; }
    item->condition -= loss;
    if (item->condition <= 0) {
        item->condition = 0;
        og_inventory_unequip(id);
        og_msg_set("Your equipment has broken.");
    }
}

void og_player_slain(void)
{
    OgCharacter *who;
    who = og_character();
    who->health = 0;
    og_state_event(OG_EVENT_KILL,og_state()->entity_id,0,1);
    og_sound(who->female ? 258 : 243);
    og_msg_set("You have died.");
}

void og_damage_player(int damage, int part)
{
    OgCharacter *who;
    int amount;
    who = og_character();
    if (damage <= 0 || who->health <= 0) { return; }
    damage = og_magic_absorb(-1,damage); if (!damage) { return; }
    /* damage_apply: while guards are about the first blow is the offer to surrender, held back until it is answered. It is the law's, not the body's: god mode does not skip it. */
    if (og_law_blow(og_state()->region) == OG_BLOW_ASK && og_possessions_surrender()) { return; }
    if (og_god_mode()) { return; }
    who->health -= damage;
    /* Brought down among the guards: the court (or the end), not simply death. */
    if (who->health < 1 && og_guards_present() && og_possessions_arrested()) { return; }
    if (who->health < 0) { who->health = 0; }
    amount = damage < 4 ? 4 : damage > 32 ? 32 : damage;
    g_flash = (float)amount/32;
    og_state_event(who->health ? OG_EVENT_HURT : OG_EVENT_KILL,og_state()->entity_id,0,damage);
    if (og_armor_slot(part) >= 0) { og_item_wear(og_state()->equipped[og_armor_slot(part)],damage); }
    if (!who->health) {
        og_sound(who->female ? 258 : 243);
        og_msg_set("You have died.");
    }
}

void og_combat_step(float dt) { g_flash -= dt*2; if (g_flash < 0) { g_flash = 0; } }
void og_combat_reset(void) { g_flash = 0; }
float og_combat_flash(void) { return g_flash; }
