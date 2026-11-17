#include "game/dagger/play/character/curse.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/magic/spellbook.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/magic/magicitem.h"
#include "engine/scene/entity.h"
#include "game/dagger/play/play.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/ui/cmd.h"

/* See curse.h and the decomp's docs/curses.md. */

/* 2.2: three days from a blow to the curse, and a blow's chance, 400 of Watcom's 32768. */
#define INFECTION_MINUTES 4320u
#define INFECTION_CHANCE 400
/* 4: a vampire's 21 days. */
#define VAMPIRE_DAYS_MINUTES 30240u
#define VAMPIRE_ATTRIBUTE 20
#define LYCANTHROPE_ATTRIBUTE 40
#define SKILL_BONUS 30
#define DEFAULT_CLAN 153
/* 5.4: a vampire may go this long (16 hours) without killing. */
#define HUNGER_MINUTES 960u
/* 7.2: a change into beast form once in 20 hours. */
#define SHAPECHANGE_WAIT 1200u
/* 7.4: a lycanthrope's fortnight, and the same again for its health to reach 4. */
#define KILL_PERIOD 20160u
#define HEALTH_FLOOR 4
/* 7.1: the moons, two days in 32, four days apart. */
#define MOON_PERIOD 32
#define MOON_GAP 4
#define MOON_SOCIAL_GROUPS 5
#define BEAST_REPUTATION 100

/* TEXT.RSC 401: the message box a vampire's infection takes with. */
#define TEXT_BECOME_VAMPIRE 401

/* 4 step 14: the spells every vampire has, and the clan's (SPELLS.STD numbers). */
static const int vampire_spells[] = { 4, 90, 91 };
/* 4 step 8, 6 step 4, 4 step 9, 6 step 7: attributes and skills of each curse. */
static const int vampire_skills[] = { SKILL_JUMPING, SKILL_RUNNING, SKILL_STEALTH, SKILL_CRITICAL_STRIKE, SKILL_CLIMBING, SKILL_HAND_TO_HAND };
static const int lycanthrope_skills[] = { SKILL_SWIMMING, SKILL_CLIMBING, SKILL_JUMPING, SKILL_STEALTH, SKILL_RUNNING, SKILL_CRITICAL_STRIKE, SKILL_HAND_TO_HAND };
static const int lycanthrope_attributes[] = { ATTR_STR, ATTR_END, ATTR_AGI, ATTR_SPD };

static const CurseWorld *g_world;
static int g_busy;

void curse_set_world(const CurseWorld *world) { g_world = world; }
const CurseWorld *curse_world(void) { return g_world; }

int curse_form(void) { return curse_state()->form; }
int curse_race(void) { return curse_state()->form ? curse_state()->form : character_state()->race; }
int curse_is_vampire(void) { return curse_state()->disease == CURSE_VAMPIRISM; }
int curse_is_lycanthrope(void) { return curse_state()->disease == CURSE_WEREWOLF || curse_state()->disease == CURSE_WEREBOAR; }
int curse_beast(void) { return curse_state()->form > CURSE_FORM_VAMPIRE; }
int curse_min_metal(void) { return curse_state()->form ? 2 : 0; }
int curse_infected(void) { return curse_state()->infection_time != 0; }
int curse_suspended(int slot) { return slot >= 0 && slot < MEMBERSHIPS && (curse_state()->suspended & (1 << slot)) != 0; }

int curse_valid(const CurseState *c)
{
    int i;

    if (!c || (c->form != CURSE_FORM_NONE && (c->form < CURSE_FORM_VAMPIRE || c->form > CURSE_FORM_WEREBOAR)) || c->disease < 0 || (c->disease != 0 && (c->disease < 100 || c->disease > 102)) ||
        c->action < 0 || c->action > 1 || c->infection_kind < 0 || c->infection_kind > 2 || (c->clan != 0 && (c->clan < 150 || c->clan > 158)) ||
        c->suspended < 0 || c->suspended >= (1 << MEMBERSHIPS) || c->health_base < 0 || c->health_base > 100000) { return 0; }
    /* The form follows the curse: a vampire's is 8, a lycanthrope's human (0) or its beast (9 for the werewolf, 10 for the wereboar). */
    if (c->disease == CURSE_VAMPIRISM ? c->form != CURSE_FORM_VAMPIRE : c->disease == 0 ? c->form != CURSE_FORM_NONE : (c->form != CURSE_FORM_NONE && c->form != (c->disease == CURSE_WEREWOLF ? CURSE_FORM_WEREWOLF : CURSE_FORM_WEREBOAR))) { return 0; }
    for (i = 0; i < ATTRS; i++) { if (c->drained[i] < 0 || c->drained[i] > 100) { return 0; } }
    return 1;
}

/* ---- Infection (2) ---- */

static void infect(int kind)
{
    CurseState *c;

    /* Both infect functions: nothing at level 1 or for a vampire or a lycanthrope in beast form (race > 7). A second infection overwrites the first and restarts the three days. */
    if (character_state()->level == 1 || curse_form() > PLAYER_RACE_ARGONIAN) { return; }
    c = curse_state();
    c->infection_time = game_state()->minutes + INFECTION_MINUTES;
    c->infection_kind = kind;
    c->dream = 1;
}

void curse_infect_vampirism(void) { infect(0); }
void curse_infect_lycanthropy(int kind) { infect(kind + 1); }

int curse_blow(int attacker_kind)
{
    if (attacker_kind != ENEMY_WEREWOLF && attacker_kind != ENEMY_WEREBOAR && attacker_kind != ENEMY_VAMPIRE && attacker_kind != ENEMY_VAMPIRE_ANCIENT) { return 0; }
    if (random_range(0, 32767) >= INFECTION_CHANCE) { return 0; }
    if (attacker_kind == ENEMY_WEREWOLF) { curse_infect_lycanthropy(0); }
    else if (attacker_kind == ENEMY_WEREBOAR) { curse_infect_lycanthropy(1); }
    else { curse_infect_vampirism(); }
    return 1;
}

void curse_clear_infection(void)
{
    CurseState *c;

    c = curse_state();
    c->infection_time = 0; c->infection_kind = 0; c->dream = 0;
}

int curse_dream(void)
{
    if (!curse_infected() || !curse_state()->dream) { return 0; }
    curse_state()->dream = 0;
    return 1;
}

/* ---- What a curse gives and takes back ---- */

/* An attribute raised by amount, its base with it, the excess over 100 taken back from both and from the record (4 step 8). */
static void bonus_attribute(int i, int amount)
{
    CurseState *c;
    int base, over;

    c = curse_state();
    base = game_state()->base_attribute[i] + amount;
    character_state()->attribute[i] += amount; c->drained[i] += amount;
    if (base > 100) {
        over = base - 100;
        base -= over; character_state()->attribute[i] -= over; c->drained[i] -= over;
    }
    state_base_attribute(i, base);
}

static void take_back_attributes(void)
{
    CurseState *c;
    int i;

    c = curse_state();
    for (i = 0; i < ATTRS; i++) {
        character_state()->attribute[i] -= c->drained[i];
        state_base_attribute(i, game_state()->base_attribute[i] - c->drained[i]);
        c->drained[i] = 0;
    }
}

void curse_skill_bonuses(int sign)
{
    const int *list;
    int n, i;

    if (curse_is_vampire()) { list = vampire_skills; n = (int)(sizeof(vampire_skills) / sizeof(vampire_skills[0])); }
    else if (curse_is_lycanthrope()) { list = lycanthrope_skills; n = (int)(sizeof(lycanthrope_skills) / sizeof(lycanthrope_skills[0])); }
    else { return; }
    for (i = 0; i < n; i++) { character_state()->skill[list[i]] += sign * SKILL_BONUS; }
}

/* A spell of the curse's into the book, marked so the cure takes it out again (disease_add_vampire_spell). */
static void give_spell(int number)
{
    df_u32 book, spell;

    book = inventory_book();
    if (!book) { book = item_create(game_state()->inventory_id, ITEM_GROUP_MISC, MISC_SPELLBOOK, 0); }
    if (!book) { return; }
    spell = spell_create(book, number);
    if (spell) { object_edit(spell)->message = CURSE_SPELL_MARK; }
}

static void take_spells(void)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        if (o->id && o->type == RECORD_SPELL && o->message == CURSE_SPELL_MARK) { object_delete(o->id); }
    }
}

static void suspend_guilds(void)
{
    int i;

    curse_state()->suspended = 0;
    for (i = 0; i < MEMBERSHIPS; i++) { if (guilds_state()->m[i].used) { curse_state()->suspended |= 1 << i; } }
}

/* The class as it was, with the curse's immunities and flags gone. */
static void restore_class(void)
{
    *career_edit() = curse_state()->saved_class;
}

/* ---- Becoming (4, 6) ---- */

static void become_vampire(void)
{
    CurseState *c;
    int region, i, clan;

    c = curse_state();
    if (curse_form() > PLAYER_RACE_ARGONIAN || curse_is_lycanthrope()) { return; }
    curse_clear_infection();
    possessions_message(words_state(TEXT_BECOME_VAMPIRE), 0);
    /* 21 days pass (random encounters belong to resting here, so there are none), then the player wakes in a crypt of the region, if it has one. */
    state_pass_minutes(VAMPIRE_DAYS_MINUTES);
    region = g_world && g_world->region ? g_world->region() : 0;
    if (g_world && g_world->haunt) { g_world->haunt(region); }
    clan = g_world && g_world->clan ? g_world->clan(region) : 0;
    c->clan = clan ? clan : DEFAULT_CLAN;
    c->saved_class = game_state()->career;
    c->disease = CURSE_VAMPIRISM;
    for (i = 0; i < ATTRS; i++) { if (i != ATTR_INT) { bonus_attribute(i, VAMPIRE_ATTRIBUTE); } }
    curse_skill_bonuses(1);
    career_edit()->immunity |= EFFECT_BIT_PARALYSIS | EFFECT_BIT_DISEASE;
    career_edit()->flags |= CLASS_SUN_DAMAGE | CLASS_HOLY_DAMAGE;
    ailments_cure(-1, 0);
    suspend_guilds();
    c->form = CURSE_FORM_VAMPIRE;
    for (i = 0; i < (int)(sizeof(vampire_spells) / sizeof(vampire_spells[0])); i++) { give_spell(vampire_spells[i]); }
    switch (c->clan - 150) {
    case 0: give_spell(85); break;
    case 1: give_spell(20); give_spell(33); break;
    case 2: give_spell(64); break;
    case 3: give_spell(23); give_spell(6); break;
    case 4: give_spell(10); break;
    case 5: give_spell(50); break;
    case 6: give_spell(17); break;
    case 7:
        /* The original's 100 clamp for this clan's +20 Intelligence reads the attribute after the spell count (3: Agility), which the loop above has already held to 100: nothing is clamped (docs/curses.md 12.4). */
        character_state()->attribute[ATTR_INT] += VAMPIRE_ATTRIBUTE; c->drained[ATTR_INT] += VAMPIRE_ATTRIBUTE;
        state_base_attribute(ATTR_INT, game_state()->base_attribute[ATTR_INT] + VAMPIRE_ATTRIBUTE);
        break;
    case 8: give_spell(11); give_spell(12); give_spell(13); break;
    default: break;
    }
}

static void become_lycanthrope(int kind)
{
    CurseState *c;
    int i;

    c = curse_state();
    if (character_state()->level == 1 || curse_is_lycanthrope() || curse_form() > PLAYER_RACE_ARGONIAN) { return; }
    curse_clear_infection();
    c->saved_class = game_state()->career;
    c->disease = CURSE_WEREWOLF + kind;
    for (i = 0; i < (int)(sizeof(lycanthrope_attributes) / sizeof(lycanthrope_attributes[0])); i++) { bonus_attribute(lycanthrope_attributes[i], LYCANTHROPE_ATTRIBUTE); }
    career_edit()->immunity |= EFFECT_BIT_DISEASE;
    give_spell(SHAPECHANGE_SPELL);
    curse_skill_bonuses(1);
    c->kill_time = game_state()->minutes;
    c->health_base = character_state()->max_health;
}

/* ---- The cures (9.2) ---- */

void curse_cure_vampirism(void)
{
    CurseState *c;

    c = curse_state();
    if (c->infection_kind == 0) { curse_clear_infection(); }
    if (!curse_is_vampire()) { return; }
    take_back_attributes();
    curse_skill_bonuses(-1);
    restore_class();
    c->suspended = 0; c->form = CURSE_FORM_NONE; c->disease = 0;
    take_spells();
}

void curse_cure_lycanthropy(void)
{
    CurseState *c;

    c = curse_state();
    if (c->infection_kind != 0) { curse_clear_infection(); }
    if (!curse_is_lycanthrope()) { return; }
    /* The original keeps the beast's -100 reputation if cured in beast form (docs/curses.md 12.2): that is its bug, and the reputation comes back here. */
    if (curse_beast()) { int i; for (i = 0; i < MOON_SOCIAL_GROUPS; i++) { character_state()->reputation[i] += BEAST_REPUTATION; } }
    take_back_attributes();
    curse_skill_bonuses(-1);
    restore_class();
    take_spells();
    if (c->health_base) { character_state()->max_health = c->health_base; }
    c->form = CURSE_FORM_NONE; c->disease = 0; c->health_base = 0;
}

/* Hircine's ring is artifact 3 (item_artifact_equipped(3)). Worn, it ends the hunger and the moons and the wait between changes. */
#define HIRCINE_RING 3

static int ring_worn(void) { return magic_artifact_worn(HIRCINE_RING) != 0; }

/* ---- A lycanthrope's changes (7) ---- */

int curse_shapechange(int forced)
{
    CurseState *c;
    CharacterState *who;
    int i;

    c = curse_state(); who = character_state();
    if (!curse_is_lycanthrope()) { return 0; }
    if (!forced && c->form == CURSE_FORM_NONE && !ring_worn() &&
        game_state()->minutes - c->shape_time < SHAPECHANGE_WAIT) {
        msg_set("Can only be cast once per day...");
        return 0;
    }
    c->shape_time = game_state()->minutes;
    if (c->form == CURSE_FORM_NONE) {
        c->form = c->disease == CURSE_WEREWOLF ? CURSE_FORM_WEREWOLF : CURSE_FORM_WEREBOAR;
        for (i = 0; i < MOON_SOCIAL_GROUPS; i++) { who->reputation[i] -= BEAST_REPUTATION; }
        who->health = who->max_health;
        /* Both hands emptied: the weapons stay in the pack. */
        state_equip(EQUIP_RIGHT_HAND, 0); state_equip(EQUIP_LEFT_HAND, 0);
    } else {
        c->form = CURSE_FORM_NONE;
        for (i = 0; i < MOON_SOCIAL_GROUPS; i++) { who->reputation[i] += BEAST_REPUTATION; }
        who->health = who->max_health;
    }
    return 1;
}

void curse_innocent_killed(void)
{
    CurseState *c;

    c = curse_state();
    if (!curse_is_lycanthrope()) { return; }
    c->kill_time = game_state()->minutes;
    if (c->health_base) { character_state()->max_health = c->health_base; }
}

void curse_level_up(int health)
{
    if (curse_is_lycanthrope() && curse_state()->health_base) { curse_state()->health_base += health; }
}

int curse_hungry(void)
{
    return curse_is_vampire() && game_state()->minutes - game_state()->last_kill > HUNGER_MINUTES;
}

/* ---- A beast in the world (7.3) ---- */

/* The ambient growl: 50 in 32768 each ambient call, which the original makes about 18 times a second. */
#define GROWL_TICK 0.055f
#define GROWL_SOUND 10090
#define GROWL_CHANCE 50

void curse_frame(float dt)
{
    static int was_beast = 0;
    static float growl;
    int beast;

    beast = curse_beast();
    /* No townspeople come to a town while the player is a beast (people_max_count), and those about go. */
    if (beast != was_beast) { entity_set_population(!beast); was_beast = beast; }
    if (!beast) { growl = 0; return; }
    for (growl += dt; growl >= GROWL_TICK; growl -= GROWL_TICK) {
        if (random_range(0, 32767) < GROWL_CHANCE) { play_sound(GROWL_SOUND); }
    }
}

/* ---- Sounds (5.1, 7.3) ---- */

void curse_attack_sound(void)
{
    int form, base;

    form = curse_form();
    if (!form || random_range(0, 99) >= 20) { return; }
    /* A vampire's cry by sex (10281/10282 a woman's, 10301/10302 a man's); a werewolf's 10091/10092, a wereboar's 10141/10142. */
    base = form == CURSE_FORM_VAMPIRE ? (character_state()->female ? 10281 : 10301) : form == CURSE_FORM_WEREWOLF ? 10091 : 10141;
    play_sound(base + random_range(0, 1));
}

/* ---- The clock (2.4, 7.1, 7.4) ---- */

static void lycanthrope_tick(void)
{
    CurseState *c;
    CharacterState *who;
    df_u32 since, over;
    int new_max, day;

    c = curse_state(); who = character_state();
    if (!curse_is_lycanthrope() || ring_worn()) { return; }
    since = game_state()->minutes - c->kill_time;
    if (since > KILL_PERIOD) {
        over = since - KILL_PERIOD;
        if (over > KILL_PERIOD) { new_max = HEALTH_FLOOR; }
        else {
            new_max = (int)((((over << 8) / KILL_PERIOD) * (df_u32)who->max_health) >> 8);
            if (new_max < HEALTH_FLOOR) { new_max = HEALTH_FLOOR; }
        }
        if (who->max_health != new_max) { msg_set("You feel the need to hunt the innocent..."); }
        who->max_health = new_max;
        if (who->health > new_max) { who->health = new_max; }
    }
    if (c->form <= CURSE_FORM_VAMPIRE) {
        day = (int)(game_state()->minutes / 1440u);
        if (day % MOON_PERIOD == 0 || (day + MOON_GAP) % MOON_PERIOD == 0) {
            msg_set("You dream, once again, of the moon...");
            curse_shapechange(1);
        }
    }
}

static void infection_tick(void)
{
    CurseState *c;
    int kind;

    c = curse_state();
    if (!c->infection_time || game_state()->minutes <= c->infection_time) { return; }
    /* The time is cleared before the curse is tried, so an infection the curse refuses is gone (3). */
    kind = c->infection_kind;
    c->infection_time = 0;
    if (kind == 0) { become_vampire(); }
    else { become_lycanthrope(kind - 1); }
}

/* disease_start_cure_quest (9.3): every 84 days a vampire has 22 in 100 and a lycanthrope 29 in 100 of being offered the cure quest; every 266 days a vampire may be offered a clan quest (P0B once the first P0A was picked). */
#define CURE_PERIOD 120960u
#define CLAN_PERIOD 383040u

void curse_schedule(df_u32 start, df_u32 minutes)
{
    CurseState *c;
    char name[9];
    df_u32 m, end;

    c = curse_state();
    if (!c->disease) { return; }
    end = start + minutes;
    for (m = (start / CURE_PERIOD + 1u) * CURE_PERIOD; m <= end; m += CURE_PERIOD) {
        if (curse_is_vampire() ? random_range(10, 100) < 30 : random_range(1, 100) < 30) { quest_begin(curse_is_vampire() ? "$CUREVAM" : "$CUREWER", 0); }
    }
    if (!curse_is_vampire()) { return; }
    for (m = (start / CLAN_PERIOD + 1u) * CLAN_PERIOD; m <= end; m += CLAN_PERIOD) {
        if (c->action) {
            if (random_range(1, 100) < 50 && quest_pick('P', 0, 'B', character_state()->level, name)) { quest_begin(name, 0); }
        } else if (random_range(1, 100) < 50 && quest_pick('P', 0, 'A', character_state()->level, name)) {
            quest_begin(name, 0); c->action = 1;
        }
    }
}

void curse_tick(void)
{
    if (g_busy || !game_state()->ready) { return; }
    g_busy = 1;
    lycanthrope_tick();
    if (!law_is_serving()) { infection_tick(); }
    /* crime_check_beast_form_in_town: a beast in a town calls the guards every half hour (they come if the street's witnesses are set). */
    if (curse_beast() && game_state()->minutes % 30u == 0) { guards_summon(0); }
    g_busy = 0;
}

/* ---- The console ---- */

static int word(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void curse_command(int argc, char **argv)
{
    CurseState *c;

    c = curse_state();
    if (!game_state()->ready) { return; }
    if (argc >= 2 && word(argv[1], "vampire")) { curse_infect_vampirism(); }
    else if (argc >= 2 && word(argv[1], "werewolf")) { curse_infect_lycanthropy(0); }
    else if (argc >= 2 && word(argv[1], "wereboar")) { curse_infect_lycanthropy(1); }
    else if (argc >= 2 && word(argv[1], "now")) { state_pass_minutes(4321); }
    else if (argc >= 2 && word(argv[1], "shape")) { curse_shapechange(0); }
    else if (argc >= 2 && word(argv[1], "cure")) { curse_cure_vampirism(); curse_cure_lycanthropy(); }
    else if (argc >= 2) { log_text(LOG_PRINT, "curse vampire | werewolf | wereboar | now | shape | cure"); return; }
    log_print(LOG_PRINT, "form %d, disease %d, clan %d, infection %s (kind %d, %d minutes left), last kill %u, hungry %d", c->form, c->disease, c->clan,
              c->infection_time ? "pending" : "none", c->infection_kind, c->infection_time > game_state()->minutes ? (int)(c->infection_time - game_state()->minutes) : 0,
              (unsigned)game_state()->last_kill, curse_hungry());
}

void curse_commands(void)
{
    cmd_add("curse", curse_command, "vampire | werewolf | wereboar | now | shape | cure", "Vampirism and lycanthropy: infect, let the three days pass, change form, cure; alone it shows the state.");
}
