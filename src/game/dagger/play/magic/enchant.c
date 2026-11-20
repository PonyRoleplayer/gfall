#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/combatdefs.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/character/progress.h"
#include "game/dagger/play/magic/spellbook.h"
#include "game/dagger/play/combat/dungeon.h"
#include "engine/core/log.h"

/* See enchant.h. */

static df_u32 g_stamp;
static int g_rebuild;

/* Whether an item worn in a slot other than skip holds Absorbs spells (a worn item taking it off must not clear what another still gives). */
static int others_absorb(int skip)
{
    int slot, k, type, param;

    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        if (slot == skip || !game_state()->equipped[slot]) { continue; }
        for (k = 0; item_enchantment(game_state()->equipped[slot], k, &type, &param); k++) { if (type == ENCHANT_ABSORBS_SPELLS) { return 1; } }
    }
    return 0;
}

void enchant_equip(df_u32 item, int slot)
{
    int k, type, param, wear;

    wear = 0;
    for (k = 0; item_enchantment(item, k, &type, &param); k++) {
        if (type == ENCHANT_CAST_WHEN_HELD && param >= 0) {
            const SpellDef *held;

            magic_hold(param, slot);
            /* Each held spell wears the item by its own cost (the original wears once by the last one's: 9.16). */
            held = spell_def(param);
            if (held) { wear += spell_cost(held, character_state()); }
        }
        else if (type == ENCHANT_REGENS_HEALTH) { g_stamp = game_state()->minutes; }
        else if (type == ENCHANT_ABSORBS_SPELLS) { character_state()->conditions |= COND_ABSORPTION; }
        else if (type == ENCHANT_ENHANCES_SKILL && param >= 0 && param < SKILLS) { character_state()->skill[param] += ENCHANT_SKILL_BONUS; }
    }
    /* Breaking takes off all the effects, so apply them all before charging wear. */
    if (wear) { item_damage(item,wear); }
}

void enchant_unequip(df_u32 item, int slot)
{
    int k, type, param;

    for (k = 0; item_enchantment(item, k, &type, &param); k++) {
        if (type == ENCHANT_CAST_WHEN_HELD && param >= 0) { magic_release(param, slot); }
        else if (type == ENCHANT_ABSORBS_SPELLS && !others_absorb(slot)) { character_state()->conditions &= ~COND_ABSORPTION; }
        else if (type == ENCHANT_ENHANCES_SKILL && param >= 0 && param < SKILLS) { character_state()->skill[param] -= ENCHANT_SKILL_BONUS; }
        else if (type == ENCHANT_EXTRA_SPELL_POINTS) { g_rebuild = 1; }
    }
}

/* ---- Wear and breaking (equip.c item_damage, inven.c item_break) ---- */

static const EnchantWorld *g_world;

void enchant_set_world(const EnchantWorld *world) { g_world = world; }

static int daylight(void) { return g_world && g_world->daylight && g_world->daylight(); }
int enchant_daylight(void) { return daylight(); }
static int temple(void) { return g_world && g_world->temple && g_world->temple(); }

static void item_break(df_u32 id)
{
    ObjectInfo *item;
    char name[ITEM_NAME_MAX], line[TEXT_LINE_MAX];
    int enchanted, k, type, param, souls[ITEM_ENCHANTMENTS], count;

    item = object_edit(id);
    if (!item) { return; }
    item->condition = 0;
    item_name(item, name, sizeof(name));
    log_format(line, sizeof(line), "%s is broken.", name);
    msg_set(line);
    enchanted = item_enchanted(id);
    inventory_unequip(id);
    if (!enchanted) { return; }
    count = 0;
    for (k = 0; item_enchantment(id, k, &type, &param); k++) {
        if (type == ENCHANT_SOUL_BOUND && param >= 0) { souls[count++] = param; }
    }
    object_delete(id);
    for (k = 0; k < count; k++) {
        msg_set("The soul from your broken item is released.");
        if (g_world && g_world->release_soul) { g_world->release_soul(souls[k]); }
    }
}

void item_damage(df_u32 id, int n)
{
    ObjectInfo *item;

    item = object_edit(id);
    if (!item || item->type != RECORD_ITEM || n <= 0 || item->condition <= 0) { return; }
    if (item->condition > n) { item->condition -= n; return; }
    item_break(id);
}

/* ---- The tick (disease_000680DD.c item_enchantment_tick) ---- */

/* enchant_spell_points_condition: 75 when the parameter's condition holds. */
static int extra_points(int param)
{
    static const int season[MONTHS] = {XSP_WINTER, XSP_SPRING, XSP_SPRING, XSP_SPRING, XSP_SUMMER, XSP_SUMMER, XSP_SUMMER, XSP_AUTUMN, XSP_AUTUMN, XSP_AUTUMN, XSP_WINTER, XSP_WINTER};
    int year, month, day, weekday, moon0, moon1;
    df_u32 now;

    now = game_state()->minutes;
    state_date(&year, &month, &day, &weekday);
    moon0 = (int)((now / MINUTES_PER_DAY) & MOON_PHASE_MASK); moon1 = (int)(((now + MOON_LOOKAHEAD_MINUTES) / MINUTES_PER_DAY) & MOON_PHASE_MASK);
    if (param >= 0 && param <= XSP_SEASON_LAST) { return season[month % MONTHS] == param ? XSP_POINTS : 0; }
    if (param == XSP_FULL_MOON) { return moon0 == MOON_FULL || moon1 == MOON_FULL ? XSP_POINTS : 0; }
    if (param == XSP_HALF_MOON) { return moon0 == MOON_HALF_WAX || moon0 == MOON_HALF_WANE || moon1 == MOON_HALF_WAX || moon1 == MOON_HALF_WANE ? XSP_POINTS : 0; }
    if (param == XSP_NEW_MOON) { return moon0 == MOON_NEW || moon1 == MOON_NEW ? XSP_POINTS : 0; }
    if (param >= XSP_NEAR_FIRST && param <= XSP_NEAR_LAST) { return g_world && g_world->category_near && g_world->category_near(param - XSP_NEAR_FIRST, XSP_NEAR_RANGE) ? XSP_POINTS : 0; }
    return 0;
}

static void repair_first(int amount)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        ObjectInfo *item;
        int depth;
        const ObjectInfo *owner;

        item = object_edit(game_state()->object[i].id);
        if (!item || item->type != RECORD_ITEM || (item->group == ITEM_GROUP_WEAPON && item->index == WEAPON_ARROW) || item_enchanted(item->id)) { continue; }
        for (owner = item, depth = 0; owner && depth < OBJECTS && owner->parent != game_state()->entity_id; depth++) { owner = object_at(owner->parent); }
        if (!owner || item->condition >= item->max_condition) { continue; }
        item->condition += amount;
        if (item->condition > item->max_condition) { item->condition = item->max_condition; }
        return;
    }
}

static void tick_one(df_u32 id, int type, int param, int amount)
{
    CharacterState *who;

    who = character_state();
    if (type == ENCHANT_CAST_WHEN_HELD) { item_damage(id, 1); }
    else if (type == ENCHANT_EXTRA_SPELL_POINTS) {
        if (game_state()->minutes % XSP_WEAR_EVERY == 0) { item_damage(id, 1); }
        if (param >= XSP_NEAR_FIRST && param <= XSP_NEAR_LAST && extra_points(param) && who->magicka < who->max_magicka) { who->magicka += amount * XSP_MAGICKA_PER_STEP; }
    } else if (type == ENCHANT_REGENS_HEALTH) {
        if (param == 1 ? !daylight() : param == 2 ? daylight() : 0) { return; }
        who->health += amount;
        if (who->health > who->max_health) { who->health = who->max_health; }
        else if (random_range(0, ENCHANT_WEAR_ONE_IN - 1) == 0) { item_damage(id, 1); }
    } else if (type == ENCHANT_VAMPIRIC && param == VAMPIRIC_TICK) {
        if (who->health < who->max_health && g_world && g_world->hurt_nearest && g_world->hurt_nearest(amount)) {
            who->health += amount;
            if (who->health > who->max_health) { who->health = who->max_health; }
            item_damage(id, amount);
        }
    } else if (type == ENCHANT_REPAIRS_OBJECTS) {
        repair_first(amount);
        if (random_range(0, ENCHANT_WEAR_ONE_IN - 1) == 0) { item_damage(id, 1); }
    } else if (type == ENCHANT_ITEM_DETERIORATES) {
        if (param == DETERIORATE_DAY ? !daylight() : param == DETERIORATE_TEMPLE ? !temple() : 0) { return; }
        item_damage(id, amount);
    } else if (type == ENCHANT_USER_TAKES_DAMAGE) {
        if (param == 0 ? daylight() : temple()) { damage_player(amount, 0); }
    } else if (type == ENCHANT_HEALTH_LEECH && (param == LEECH_DAILY || param == LEECH_WEEKLY)) {
        if (game_state()->minutes - game_state()->last_kill > (param == LEECH_DAILY ? MINUTES_PER_DAY : MINUTES_PER_WEEK)) { damage_player(amount, 0); }
    }
}

void enchant_minutes(void)
{
    df_u32 now, elapsed;
    int amount, slot, k, type, param;

    now = game_state()->minutes;
    /* A clock behind the stamp is a load of an older game: the stamp follows it, or no step would run until the clock caught up. */
    if (now < g_stamp) { g_stamp = now; }
    elapsed = now - g_stamp;
    if (elapsed > TICK_GAP_MAX) { elapsed = 0; g_stamp = now; }
    amount = (int)(elapsed >> TICK_MINUTES_SHIFT);
    if (!amount) { return; }
    g_stamp = now;
    /* effects_tick's class part (5.2): a class hurt by sunlight (flag 16) or in temples (flag 32) takes 12 a step, a vampire's 3 health a game minute. */
    if (((game_state()->career.flags & CLASS_SUN_DAMAGE) && daylight()) || ((game_state()->career.flags & CLASS_HOLY_DAMAGE) && temple())) { damage_player(amount * CLASS_SUN_DAMAGE_PER_STEP, 0); }
    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        df_u32 id;

        id = game_state()->equipped[slot];
        for (k = 0; id && object_at(id) && item_enchantment(id, k, &type, &param); k++) { tick_one(id, type, param, amount); }
    }
    if (character_state()->magicka < 0) { character_state()->magicka = 0; }
}

void enchant_frame(void)
{
    int slot, k, type, param, bonus, any;
    CharacterState *who;

    bonus = 0; any = 0;
    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        for (k = 0; game_state()->equipped[slot] && item_enchantment(game_state()->equipped[slot], k, &type, &param); k++) {
            if (type == ENCHANT_EXTRA_SPELL_POINTS) { any = 1; bonus += extra_points(param); }
        }
    }
    if (!any && !g_rebuild) { return; }
    /* The maximum is built again from Intelligence and the class, plus the bonus, each frame while an item gives one (and once more when the last is taken off). */
    who = character_state();
    who->max_magicka = magicka_state(who->attribute[ATTR_INT], who->class_flags) + bonus;
    if (who->magicka > who->max_magicka) { who->magicka = who->max_magicka; }
    g_rebuild = any;
}

int item_use(df_u32 id)
{
    int k, type, param, used;

    used = 0;
    if (item_enchantment(id,0,&type,&param) && type == ENCHANT_SPECIAL_ADVANTAGE) {
        if (param == ARTIFACT_HIRCINE_RING) {
            if (!magic_cast_item(SHAPECHANGE_SPELL)) { return 0; }
            item_damage(id,ARTIFACT_USE_WEAR);
            return 1;
        }
        if (param == ARTIFACT_SUMMON_DAEDROTH || param == ARTIFACT_SUMMON_LIKE) {
            int kind, index;
            const Entity *entity;

            /* The nearest hostile creature that is alive (the original takes the first of its list, which may be anything). */
            {
                float best = 0, dx, dz;
                int found = -1;

                for (index = 0; index < dungeon_count(); index++) {
                    entity = dungeon_entity(index);
                    if (!entity->live || entity->dead || dungeon_team(index) != TEAM_HOSTILE) { continue; }
                    dx = entity->x-move_body()->feet[0]; dz = entity->z-move_body()->feet[2];
                    if (found < 0 || dx*dx+dz*dz < best) { found = index; best = dx*dx+dz*dz; }
                }
                index = found < 0 ? dungeon_count() : found;
            }
            if (index == dungeon_count()) { msg_set("There are no creatures nearby."); return 0; }
            kind = param == ARTIFACT_SUMMON_DAEDROTH ? ENEMY_DAEDROTH : dungeon_kind(index);
            if (!g_world || !g_world->summon_ally || !g_world->summon_ally(kind)) {
                msg_set("There is no room to summon a creature.");
                return 0;
            }
            item_damage(id,ARTIFACT_SUMMON_WEAR);
            return 1;
        }
    }
    for (k = 0; object_at(id) && item_enchantment(id, k, &type, &param); k++) {
        if (type == ENCHANT_CAST_WHEN_USED && param >= 0) {
            if (magic_cast_item(param)) { used++; item_damage(id, ENCHANT_CAST_WEAR); }
        } else if (type == ENCHANT_HEALTH_LEECH && param == LEECH_WHEN_USED) {
            damage_player((character_state()->level >> LEECH_USE_LEVEL_SHIFT) + 1, 0);
            item_damage(id, LEECH_USE_WEAR);
        }
    }
    return used;
}

int enchant_hit_bonus_for(df_u32 weapon, int category, int attacker, int level)
{
    int k, type, param, bonus;

    bonus = 0;
    for (k = 0; item_enchantment(weapon, k, &type, &param); k++) {
        if (type == ENCHANT_POTENT_VS && param == category) { bonus += level; }
        else if (type == ENCHANT_LOW_DAMAGE_VS && param == category) { bonus -= level; }
        else if (type == ENCHANT_HEALTH_LEECH && param == LEECH_WHEN_USED) {
            if (attacker < 0) { damage_player((level >> LEECH_HIT_LEVEL_SHIFT) + 1, -1); }
            else { dungeon_enchant_damage(attacker,(level >> LEECH_HIT_LEVEL_SHIFT) + 1,attacker); }
        }
    }
    return bonus;
}

int enchant_hit_bonus(df_u32 weapon, int category)
{
    return enchant_hit_bonus_for(weapon,category,-1,character_state()->level);
}

void enchant_strike_for(df_u32 weapon, int attacker, int victim, int level, int dealt)
{
    int k, type, param, healed;
    CharacterState *who;

    who = character_state();
    for (k = 0; object_at(weapon) && item_enchantment(weapon, k, &type, &param); k++) {
        if (type == ENCHANT_CAST_WHEN_STRIKES && param >= 0) {
            if (attacker < 0) { magic_strike(param,victim); }
            else { magic_creature_item_strike(param,attacker,victim,level); }
            item_damage(weapon, ENCHANT_CAST_WEAR);
        } else if (type == ENCHANT_SPECIAL_ADVANTAGE && param == ARTIFACT_MEHRUNES_RAZOR && victim >= 0 && dealt > 0) {
            int health = dungeon_health(victim);

            if (health > 0 && magic_resist(victim,ELEMENT_MAGIC,RAZOR_CLASS_BIT,0) == RESIST_FULL) {
                dungeon_enchant_damage(victim,health,attacker < 0 ? TARGET_PLAYER : attacker);
                item_damage(weapon, health / RAZOR_WEAR_DIV + 1);
            }
        } else if (type == ENCHANT_SPECIAL_ADVANTAGE && param == ARTIFACT_WABBAJACK && victim >= 0 && g_world && g_world->transform && !dungeon_transformed(victim)) {
            int change = g_world->transform(victim,g_wabbajack_kinds[random_range(0,WABBAJACK_KINDS-1)]);

            if (change >= 0) { item_damage(weapon, change); }
        } else if (type == ENCHANT_SPECIAL_ADVANTAGE && param == ARTIFACT_MACE_MOLAG_BAL && victim >= 0) {
            CharacterState target;
            ClassInfo career;
            int base[ATTRS], drain;

            drain = random_range(1,MACE_DRAIN_MOST);
            if (dungeon_spell_target(victim,&target,&career,base)) {
                if (target.magicka > MACE_FLOOR) { target.magicka -= drain; if (target.magicka < 0) { target.magicka = 0; } }
                else if (target.attribute[ATTR_STR] > MACE_FLOOR) { target.attribute[ATTR_STR] -= drain; }
                dungeon_spell_commit(victim,&target,attacker);
            }
            item_damage(weapon, MACE_WEAR);
        } else if (type == ENCHANT_VAMPIRIC && param == VAMPIRIC_STRIKE) {
            if (attacker >= 0) { healed = dungeon_enchant_heal(attacker,dealt/VAMPIRIC_HEAL_DIV); }
            else {
                healed = dealt / VAMPIRIC_HEAL_DIV;
                if (healed > who->max_health - who->health) { healed = who->max_health - who->health; }
                if (healed < 0 || who->health <= 0) { healed = 0; }
                who->health += healed;
            }
            item_damage(weapon, healed / VAMPIRIC_WEAR_DIV + 1);
        }
    }
}

void enchant_strike(df_u32 weapon, int victim, int dealt)
{
    enchant_strike_for(weapon,-1,victim,character_state()->level,dealt);
}

/* Namira reflects a creature's actual blow without reducing the incoming hit. */
void enchant_reflect(int attacker, int category, int dealt)
{
    int slot, type, param, wear;
    df_u32 item;

    if (dealt <= 0 || category < 0 || category > FOE_HUMANOID || dungeon_kind(attacker) == ENEMY_SPRIGGAN) { return; }
    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        item = game_state()->equipped[slot];
        if (!item_enchantment(item,0,&type,&param) || type != ENCHANT_SPECIAL_ADVANTAGE || param != ARTIFACT_NAMIRA_RING) { continue; }
        wear = category == FOE_UNDEAD ? dealt*2 : category == FOE_DAEDRA ? dealt/2 : dealt;
        dungeon_enchant_damage(attacker,dealt,TARGET_PLAYER);
        item_damage(item,wear);
        return;
    }
}

int enchant_armor_modifier(void)
{
    int slot, k, type, param;

    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        for (k = 0; game_state()->equipped[slot] && item_enchantment(game_state()->equipped[slot], k, &type, &param); k++) {
            if (type == ENCHANT_STRENGTHENS_ARMOR) { return -ENCHANT_ARMOR_MODIFIER; }
            if (type == ENCHANT_WEAKENS_ARMOR) { return ENCHANT_ARMOR_MODIFIER; }
        }
    }
    return 0;
}

int enchant_capacity(int capacity)
{
    int slot, k, type, param;

    for (slot = 0; slot < EQUIP_SLOTS; slot++) {
        for (k = 0; game_state()->equipped[slot] && item_enchantment(game_state()->equipped[slot], k, &type, &param); k++) {
            if (type == ENCHANT_INCREASED_WEIGHT_ALLOWANCE) { return param == 0 ? capacity * CARRY_BONUS_SMALL / 256 : capacity * CARRY_BONUS_LARGE / 256; }
        }
    }
    return capacity;
}

int enchant_weight(df_u32 id, int weight)
{
    int k, type, param;

    for (k = 0; item_enchantment(id, k, &type, &param); k++) {
        if (type == ENCHANT_FEATHER_WEIGHT) { return FEATHER_WEIGHT; }
        if (type == ENCHANT_EXTRA_WEIGHT) { return weight * EXTRA_WEIGHT_MULTIPLIER < EXTRA_WEIGHT_MIN ? EXTRA_WEIGHT_MIN : weight * EXTRA_WEIGHT_MULTIPLIER; }
    }
    return weight;
}

int enchant_reaction(int social)
{
    int i, k, type, param, depth, mod;
    const ObjectInfo *item, *owner;

    mod = 0;
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->type != RECORD_ITEM) { continue; }
        for (owner = item, depth = 0; owner && depth < OBJECTS && owner->parent != game_state()->entity_id; depth++) { owner = object_at(owner->parent); }
        if (!owner) { continue; }
        for (k = 0; item_enchantment(item->id, k, &type, &param); k++) {
            if (type == ENCHANT_GOOD_REP_WITH && (param == SOCIAL_GROUP_ALL || param == social)) { mod += ENCHANT_REACTION; }
            else if (type == ENCHANT_BAD_REP_WITH && (param == SOCIAL_GROUP_ALL || param == social)) { mod -= ENCHANT_REACTION; }
        }
    }
    return mod;
}
