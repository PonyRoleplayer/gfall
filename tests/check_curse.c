#include "check_curse.h"
#include "check_util.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/save/save.h"
#include "game/dagger/play/state.h"
#include <string.h>

static GameState g_before;
static int g_day;

static int daylight(void) { return g_day; }

/* How many spells in the book carry the curse's mark. */
static int marked_spells(void)
{
    int i, n;

    n = 0;
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        if (o->id && o->type == RECORD_SPELL && o->message == CURSE_SPELL_MARK) { n++; }
    }
    return n;
}

static void start(void)
{
    CharacterState *who;
    int i;

    state_restore(&g_before);
    god_set(0);
    career_edit()->flags = 2; career_edit()->immunity = 0;
    memset(curse_state(), 0, sizeof(*curse_state()));
    who = character_state();
    who->level = 5; who->max_health = 100; who->health = 100;
    for (i = 0; i < ATTRS; i++) { who->attribute[i] = 50; state_base_attribute(i, 50); }
    for (i = 0; i < SKILLS; i++) { who->skill[i] = 40; }
    for (i = 0; i < 5; i++) { who->reputation[i] = 0; }
}

static void infection(void)
{
    start();
    character_state()->level = 1;
    curse_infect_lycanthropy(0);
    check_expect(!curse_infected(), "curse: a level 1 character was infected");
    character_state()->level = 5;
    curse_infect_lycanthropy(0);
    check_expect(curse_infected() && curse_state()->infection_time == game_state()->minutes + 4320u && curse_state()->infection_kind == 1,
                 "curse: a werewolf's blow did not set three days and kind 1");
    curse_infect_lycanthropy(1);
    check_expect(curse_state()->infection_kind == 2 && curse_state()->infection_time == game_state()->minutes + 4320u,
                 "curse: a second infection did not overwrite the kind and restart the days");
    ailments_cure(-1, 0);
    check_expect(!curse_infected() && ailments_disease_count(-1) == 0, "curse: a cure of disease did not clear the infection");
    curse_infect_vampirism();
    check_expect(curse_infected() && ailments_disease_count(-1) == 1, "curse: a pending infection does not count as one disease for the temple");
    curse_clear_infection();
    check_expect(curse_dream() == 0, "curse: a dream was shown with no infection");
    curse_infect_vampirism();
    check_expect(curse_dream() == 1 && curse_dream() == 0, "curse: the dream was not shown exactly once");
    curse_clear_infection();
}

static void lycanthropy(void)
{
    CharacterState *who;
    int health;
    df_u32 start_minutes;

    start();
    who = character_state();
    state_base_attribute(ATTR_STR, 80); who->attribute[ATTR_STR] = 80;
    curse_infect_lycanthropy(0);
    state_pass_minutes(4320);
    check_expect(curse_infected(), "curse: the infection took before its three days were out");
    state_pass_minutes(2);
    check_expect(curse_is_lycanthrope() && curse_state()->disease == CURSE_WEREWOLF && !curse_infected() && curse_form() == 0,
                 "curse: the infection did not make a werewolf at day three");
    check_expect(who->attribute[ATTR_STR] == 100 && game_state()->base_attribute[ATTR_STR] == 100 && curse_state()->drained[ATTR_STR] == 20,
                 "curse: Strength 80 plus 40 was not held to 100 with the excess taken from the record");
    check_expect(who->attribute[ATTR_END] == 90 && who->attribute[ATTR_AGI] == 90 && who->attribute[ATTR_SPD] == 90 && who->attribute[ATTR_INT] == 50,
                 "curse: a lycanthrope's four attributes did not gain 40");
    check_expect(who->skill[17] == 70 && who->skill[30] == 70 && who->skill[0] == 40, "curse: the seven skills did not gain 30");
    check_expect(marked_spells() == 1 && (game_state()->career.immunity & 64), "curse: the Shapechange spell or the disease immunity is missing");
    /* A vampire's infection, and a second lycanthropy, are lost at day three. */
    curse_infect_vampirism();
    check_expect(curse_infected(), "curse: a lycanthrope in human form was refused an infection (it only fails at day three)");
    state_pass_minutes(4400);
    check_expect(!curse_infected() && curse_is_lycanthrope() && !curse_is_vampire(), "curse: a lycanthrope turned vampire, or kept a dead infection");
    /* The change: into the beast, once in 20 hours, back at once. */
    health = who->max_health;
    check_expect(curse_shapechange(1) && curse_form() == 9 && curse_beast() && curse_race() == 9 && who->reputation[0] == -100 && who->reputation[4] == -100 && who->health == health,
                 "curse: a change did not make the werewolf form or take 100 from each reputation");
    check_expect(curse_shapechange(0) && curse_form() == 0 && who->reputation[0] == 0 && who->reputation[3] == 0 && curse_race() == who->race,
                 "curse: changing back did not give the reputation back");
    check_expect(!curse_shapechange(0) && curse_form() == 0, "curse: a second voluntary change within 20 hours was allowed");
    state_pass_minutes(1200);
    check_expect(curse_shapechange(0) && curse_beast() && curse_shapechange(0) && !curse_beast(), "curse: a change after 20 hours was refused");
    /* Hunger: 14 days without an innocent, then the maximum falls; a kill feeds it. */
    curse_innocent_killed();
    who->max_health = 100; who->health = 100; start_minutes = game_state()->minutes;
    state_pass_minutes(20160 + 10080);
    check_expect(who->max_health == 50 && who->health == 50 && game_state()->minutes > start_minutes,
                 "curse: half way through the second fortnight the maximum health was not halved");
    curse_innocent_killed();
    check_expect(who->max_health == curse_state()->health_base && who->max_health == 100, "curse: a kill did not restore the maximum health");
    state_pass_minutes(20160 + 20161);
    check_expect(who->max_health == 4, "curse: past the second fortnight the maximum health was not 4");
    /* The cure takes everything back. */
    curse_cure_lycanthropy();
    check_expect(!curse_is_lycanthrope() && who->max_health == 100 && who->attribute[ATTR_STR] == 80 && who->attribute[ATTR_END] == 50 &&
                 who->skill[17] == 40 && marked_spells() == 0 && !(game_state()->career.immunity & 64) && game_state()->base_attribute[ATTR_STR] == 80,
                 "curse: the cure did not restore attributes, skills, class, spells and health");
}

static void moons(void)
{
    CharacterState *who;

    start();
    who = character_state();
    curse_infect_lycanthropy(1);
    state_pass_minutes(4400);
    check_expect(curse_state()->disease == CURSE_WEREBOAR, "curse: a wereboar's infection did not make a wereboar");
    /* Day 64 is a moon day (64 % 32 == 0): the next clock call forces the beast, and changing back is undone. */
    {
        df_u32 target;

        target = 64u * 1440u + 30u;
        if (game_state()->minutes < target) { state_pass_minutes(target - game_state()->minutes); }
        else { target = ((game_state()->minutes / 1440u / 32u) + 1u) * 32u * 1440u + 30u; state_pass_minutes(target - game_state()->minutes); }
    }
    check_expect(curse_form() == 10 && who->reputation[0] == -100, "curse: the full moon did not force the wereboar form");
    curse_shapechange(0);
    check_expect(curse_form() == 0, "curse: the player could not change back by hand");
    state_pass_minutes(5);
    check_expect(curse_form() == 10, "curse: on a moon day the change back was not undone at the next clock call");
    curse_cure_lycanthropy();
    check_expect(curse_form() == 0 && who->reputation[0] == 0, "curse: a cure in beast form kept the beast's reputation");
}

static void ring(void)
{
    CharacterState *who;
    df_u32 item;
    int ordinal;

    start();
    who = character_state();
    curse_infect_lycanthropy(0);
    state_pass_minutes(4400);
    /* Hircine's ring: the artifact whose power is 3. */
    item = 0;
    for (ordinal = 0; ordinal < 40 && !item; ordinal++) {
        df_u32 made;

        made = magic_artifact_create(game_state()->inventory_id, ordinal);
        if (made && magic_artifact_power(made) == 3) { item = made; }
        else if (made) { object_delete(made); }
    }
    check_expect(item != 0, "curse: no artifact of power 3 to stand for Hircine's ring");
    if (!item) { return; }
    state_equip(19, item);
    /* Worn: the moons do nothing, the hunger does nothing, and changes come as often as asked. */
    state_pass_minutes(70u * 1440u);
    check_expect(curse_form() == 0 && who->max_health == 100, "curse: Hircine's ring did not stop the moons and the hunger");
    check_expect(curse_shapechange(0) && curse_beast(), "curse: the first change was refused");
    state_equip(19, item);
    check_expect(curse_shapechange(0) && !curse_beast(), "curse: changing back was refused");
    check_expect(curse_shapechange(0) && curse_beast(), "curse: Hircine's ring did not lift the wait between changes");
    state_equip(19, item);
    curse_shapechange(0);
    state_equip(19, 0);
    curse_cure_lycanthropy();
    object_delete(item);
}

static void vampirism(void)
{
    CharacterState *who;
    EnchantWorld world;
    int health;

    start();
    who = character_state();
    guilds_state()->m[0].used = 1; guilds_state()->m[0].kind = 142; guilds_state()->m[0].faction = 400;
    curse_infect_vampirism();
    state_pass_minutes(4400);
    check_expect(curse_is_vampire() && curse_form() == 8 && curse_race() == 8 && curse_state()->disease == CURSE_VAMPIRISM && curse_state()->clan == 153,
                 "curse: the infection did not make a vampire of the default clan");
    check_expect(who->attribute[ATTR_STR] == 70 && who->attribute[ATTR_INT] == 50 && who->attribute[ATTR_LUC] == 70 && curse_state()->drained[ATTR_INT] == 0,
                 "curse: a vampire did not gain 20 in every attribute but Intelligence");
    check_expect(who->skill[3] == 70 && who->skill[34] == 70 && who->skill[0] == 40, "curse: the six skills did not gain 30");
    check_expect((game_state()->career.flags & 0x30) == 0x30 && (game_state()->career.immunity & 0x41) == 0x41, "curse: the class flags and immunities were not set");
    check_expect(curse_suspended(0) && !guild_find_kind(142), "curse: a vampire's guild membership was not suspended");
    check_expect(marked_spells() == 5, "curse: a vampire of clan 153 should have spells 4, 90, 91, 23 and 6");
    /* Hunger: more than 960 minutes without a kill and the vampire cannot rest. */
    state_event(EVENT_KILL, 1, game_state()->entity_id, 1);
    check_expect(!curse_hungry(), "curse: a vampire who has just killed is hungry");
    state_pass_minutes(961);
    check_expect(curse_hungry(), "curse: a vampire 961 minutes from the last kill is not hungry");
    /* Sunlight: 12 a four-minute step, outdoors by day. */
    memset(&world, 0, sizeof(world)); world.daylight = daylight;
    enchant_set_world(&world);
    health = who->health; g_day = 1;
    enchant_minutes();
    state_pass_minutes(8);
    check_expect(who->health == health - 24, "curse: eight minutes in daylight did not cost 24 health");
    g_day = 0; health = who->health;
    state_pass_minutes(8);
    check_expect(who->health == health, "curse: a vampire was hurt away from daylight");
    enchant_set_world(0);
    curse_cure_vampirism();
    check_expect(!curse_is_vampire() && curse_form() == 0 && who->attribute[ATTR_STR] == 50 && who->skill[3] == 40 && marked_spells() == 0 &&
                 !(game_state()->career.flags & 0x30) && !(game_state()->career.immunity & 0x41) && guild_find_kind(142), "curse: the cure did not restore a vampire");
}

static void saving(void)
{
    start();
    curse_infect_lycanthropy(0);
    state_pass_minutes(4400);
    curse_infect_vampirism();
    curse_shapechange(1);
    check_expect(state_capture_world() && save_write(6) && save_read(6), "curse: a game holding the beast form did not save");
    check_expect(curse_beast() && curse_is_lycanthrope() && curse_infected() && curse_state()->infection_kind == 0 && character_state()->reputation[0] == -100 &&
                 curse_state()->saved_class.flags == 2, "curse: the form, the pending infection or the saved class did not survive a save");
}

void check_curse(void)
{
    const CurseWorld *booted;

    /* The game's own hooks would take a vampire to a crypt and name its clan; these tests want neither. */
    booted = curse_world(); curse_set_world(0);
    g_before = *game_state();
    infection(); lycanthropy(); moons(); ring(); vampirism(); saving();
    state_restore(&g_before); state_restore_world();
    curse_set_world(booted);
}
