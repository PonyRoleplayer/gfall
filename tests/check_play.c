/*
 * The game's tests (src/game/dagger/play): classic's rules as sums, and
 * the player on foot in Privateer's Hold. Runs after check_dagger, with
 * ARENA2 mounted and MAIN.DAT merged. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>
#include <string.h>

#include "engine/core/df.h"
#include "engine/scene/walker.h"
#include "game/dagger/places/world.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/character.h"
#include "game/dagger/tables/careers.h"

/* Classic's speeds and jump, from Daggerfall Unity's formulas worked by hand. */
static void check_rules(void)
{
    check_expect(check_near(rules_walk_speed(50), 175.0f / 39.5f, 0.001f)
                     && check_near(rules_walk_speed(100), 250.0f / 39.5f, 0.001f)
                     && check_near(rules_walk_speed(10), 125.0f / 39.5f, 0.001f),
                 "walking is not (speed + 150 - drag) / 39.5");
    check_expect(check_near(rules_run_speed(50, 20), 200.0f / 39.5f * 1.45f, 0.001f)
                     && check_near(rules_run_speed(50, 100), 200.0f / 39.5f * 1.85f, 0.001f),
                 "running is not (speed + 150) / 39.5 times 1.35 and running over 200");
    check_expect(check_near(rules_jump_speed(20), 4.95f, 0.001f) && check_near(rules_gravity(), 20.0f, 0.001f)
                     && check_near(rules_jump_height(0), 4.5f * 4.5f / 40.0f, 0.001f)
                     && check_near(rules_jump_height(100), 6.75f * 6.75f / 40.0f, 0.001f),
                 "a jump does not rise as DFU's 4.5 m/s under 20 m/s^2");
}

/*
 * The character (P2): MAIN.DAT's classes from ARENA2's CLASS*.CFG, the
 * races and skills, and a new one by classic's rules, worked by hand:
 * the Warrior's base attributes 60 44 47 57 50 45 47 50, each with the
 * roll's middle 5; health 25 and 20 a level; fatigue strength and
 * endurance; spell points half its intelligence; Axe its primary, Hand
 * to Hand its major, Running its minor, Lockpicking none of them. A
 * Mage's spell points are twice its intelligence. A fall's damage.
 */
static void check_character(void)
{
    const CareerClass *warrior;
    const CareerRace *breton;
    Character c;

    warrior = career_class(CLASS_WARRIOR);
    breton = career_race(RACE_BRETON);
    check_expect(class_table()->count == 18 && race_table()->count == 8 && skill_table()->count == SKILL_COUNT,
                 "play: MAIN.DAT does not hold 18 classes, 8 races and 35 skills");
    check_expect(warrior && strcmp(warrior->name, "Warrior") == 0 && warrior->hp_per_level == 20
                     && warrior->attribute[ATTR_STRENGTH] == 60 && warrior->magicka == 0.5f
                     && warrior->primary[0] == 31,
                 "play: class 16 is not CLASS16.CFG's Warrior");
    check_expect(breton && strcmp(breton->name, "Breton") == 0 && breton->resist == EFFECT_MAGIC
                     && strcmp(career_skill(SKILL_RUNNING)->name, "Running") == 0
                     && career_skill(SKILL_RUNNING)->attribute == ATTR_SPEED && career_skill(SKILL_RUNNING)->advance == 50,
                 "play: the Breton or the Running skill is not classic's");
    check_expect(character_new(&c, RACE_BRETON, CLASS_WARRIOR, 0, "Test"), "play: no Breton warrior could be made");
    printf("play: a Breton warrior, strength %d speed %d, health %d fatigue %d spell points %d\n",
           c.attribute[ATTR_STRENGTH], c.attribute[ATTR_SPEED], c.health_max,
           c.fatigue_max / CHARACTER_FATIGUE_UNIT, c.magicka_max);
    check_expect(c.attribute[ATTR_STRENGTH] == 65 && c.attribute[ATTR_INTELLIGENCE] == 49
                     && c.attribute[ATTR_ENDURANCE] == 55 && c.attribute[ATTR_SPEED] == 52 && c.level == 1,
                 "play: the warrior's attributes are not its class's and the roll's middle");
    check_expect(c.health_max == 45 && c.health == 45 && c.fatigue_max == 120 * CHARACTER_FATIGUE_UNIT
                     && c.magicka_max == 24,
                 "play: the warrior's vitals are not 45, 120 and 24");
    check_expect(c.skill[31] == RULES_PRIMARY_SKILL && c.skill[30] == RULES_MAJOR_SKILL
                     && c.skill[SKILL_RUNNING] == RULES_MINOR_SKILL && c.skill[13] == RULES_OTHER_SKILL,
                 "play: the warrior's skills are not its class's groups");
    check_expect(character_new(&c, RACE_HIGH_ELF, CLASS_MAGE, 1, "Test") && c.magicka_max == 130 && c.health_max == 31,
                 "play: a mage's spell points are not twice its intelligence");
    check_expect(rules_max_health(3, 20) == 75 && rules_fall_damage(4.0f) == 0 && rules_fall_damage(7.0f) == 10,
                 "play: health a level or a fall's damage is not classic's");
    check_expect(!character_hurt(&c, 30) && c.health == 1 && character_hurt(&c, 5) && c.health == 0,
                 "play: hurting the character does not stop at 0");
}

/* n frames of 1/60 s with one input; returns how far the feet went across the ground. */
static float walk(Player *p, const Scene *scene, const Input *in, int n)
{
    float x0;
    float z0;
    float dx;
    float dz;
    int i;

    x0 = p->body.feet[0];
    z0 = p->body.feet[2];
    for (i = 0; i < n; i++) {
        player_step(p, in, scene, 1.0f / 60.0f, 1);
    }
    dx = p->body.feet[0] - x0;
    dz = p->body.feet[2] - z0;
    return df_sqrt(dx * dx + dz * dz);
}

/*
 * The player in the hold, from its start marker: it settles on the
 * floor; a quarter second's walk goes classic's walking distance, a run
 * farther by classic's ratio; both keys at once no farther than one;
 * the mouse turns it; a jump rises classic's height and lands.
 */
static void check_hold(void)
{
    World *world;
    Level level;
    Player p;
    Character sheet;
    Input in;
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float walked;
    float ran;
    float both;
    float floor_y;
    float top;
    float want;
    float speed;
    float height;
    int landed;
    int i;

    world = world_load();
    check_expect(world != 0, "play: Privateer's Hold did not load");
    if (!world) {
        return;
    }
    world_as_level(world, &level);
    world_start(world, &x, &y, &z, &yaw, &pitch);
    if (!character_new(&sheet, CHARACTER_START_RACE, CHARACTER_START_CLASS, 0, "Test")) {
        check_expect(0, "play: no starting character");
        return;
    }
    player_init(&p, &sheet);
    player_place(&p, x, y, z, yaw, pitch);
    memset(&in, 0, sizeof(in));
    walk(&p, &level.scene, &in, 60);
    check_expect(p.body.grounded, "play: the player does not stand on the hold's floor");
    floor_y = p.body.feet[1];
    body_landed(&p.body, &speed, &height);

    /* Forward along the start's facing, then back, then a run the same way. */
    in.move_f = 1;
    walked = walk(&p, &level.scene, &in, 15);
    in.move_f = -1;
    walk(&p, &level.scene, &in, 15);
    in.move_f = 1;
    in.held = INPUT_BIT(INPUT_RUN);
    ran = walk(&p, &level.scene, &in, 15);
    in.held = 0;
    in.move_f = -1;
    walk(&p, &level.scene, &in, 15);
    want = rules_walk_speed(sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 0.25f;
    printf("play: a quarter second walks %.1f (classic %.1f), runs %.1f\n", (double)walked, (double)want,
           (double)ran);
    check_expect(check_near(walked, want, want * 0.05f), "play: the walk does not go classic's distance");
    check_expect(check_near(ran / walked,
                            rules_run_speed(sheet.attribute[ATTR_SPEED], sheet.skill[SKILL_RUNNING])
                                / rules_walk_speed(sheet.attribute[ATTR_SPEED]),
                            0.05f),
                 "play: the run is not classic's ratio to the walk");
    in.move_f = 1;
    in.move_r = 1;
    both = walk(&p, &level.scene, &in, 15);
    check_expect(both <= walked * 1.02f, "play: forward and sideways at once is faster than one");
    in.move_f = -1;
    in.move_r = -1;
    walk(&p, &level.scene, &in, 15);

    /* The mouse: 100 counts right turn it 0.3 radians. */
    memset(&in, 0, sizeof(in));
    yaw = p.yaw;
    in.look_dx = 100;
    player_step(&p, &in, &level.scene, 1.0f / 60.0f, 1);
    check_expect(check_near(df_wrap_tau(p.yaw - yaw), 0.3f, 0.001f), "play: the mouse does not turn the player");

    /* A jump: up classic's height, then down on the floor with a landing. */
    memset(&in, 0, sizeof(in));
    in.pressed = INPUT_BIT(INPUT_JUMP);
    player_step(&p, &in, &level.scene, 1.0f / 60.0f, 1);
    in.pressed = 0;
    top = p.body.feet[1];
    landed = 0;
    for (i = 0; i < 120 && !landed; i++) {
        player_step(&p, &in, &level.scene, 1.0f / 60.0f, 1);
        if (p.body.feet[1] > top) {
            top = p.body.feet[1];
        }
        landed = body_landed(&p.body, &speed, &height);
    }
    want = rules_jump_height(sheet.skill[SKILL_JUMPING]) * RULES_UNITS_PER_M;
    printf("play: a jump rises %.1f (classic %.1f) and lands in %d frames (classic %.0f)\n",
           (double)(top - floor_y), (double)want, i,
           (double)(2.0f * rules_jump_speed(sheet.skill[SKILL_JUMPING]) / rules_gravity() * 60.0f));
    check_expect(check_near(top - floor_y, want, 2.0f), "play: the jump does not rise classic's height");
    check_expect(check_near((float)i, 2.0f * rules_jump_speed(sheet.skill[SKILL_JUMPING]) / rules_gravity() * 60.0f, 3.0f),
                 "play: the jump is not as long in the air as classic's");
    check_expect(landed && p.body.grounded && check_near(p.body.feet[1], floor_y, 1.0f),
                 "play: the jump does not land on the floor it left");

    /*
     * Collision off (tcl 0): two seconds' flight forward goes the whole
     * way, walls or not, level as the view is; back on, the body comes
     * out of whatever it is in and stands on a floor.
     */
    {
        float flown;
        float x0;
        float z0;

        memset(&in, 0, sizeof(in));
        in.move_f = 1;
        x0 = p.body.feet[0];
        z0 = p.body.feet[2];
        for (i = 0; i < 120; i++) {
            player_step(&p, &in, &level.scene, 1.0f / 60.0f, 0);
        }
        flown = df_sqrt((p.body.feet[0] - x0) * (p.body.feet[0] - x0) + (p.body.feet[2] - z0) * (p.body.feet[2] - z0));
        want = rules_walk_speed(sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 2.0f;
        printf("play: two seconds' flight goes %.0f (the walk's %.0f)\n", (double)flown, (double)want);
        check_expect(p.flying && check_near(flown, want, want * 0.02f) && check_near(p.body.feet[1], floor_y, 0.5f),
                     "play: flying does not go the walk's distance through everything, level");
        memset(&in, 0, sizeof(in));
        for (i = 0; i < 120; i++) {
            player_step(&p, &in, &level.scene, 1.0f / 60.0f, 1);
        }
        check_expect(!p.flying && p.body.grounded, "play: back from flying the body does not stand on a floor");
    }
}

void check_play(void)
{
    check_rules();
    check_character();
    check_hold();
}
