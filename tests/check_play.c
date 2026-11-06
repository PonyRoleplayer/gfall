/*
 * The game's tests (src/game/dagger/play): classic's rules as sums, and
 * the player on foot in Privateer's Hold. Runs after check_dagger, with
 * ARENA2 mounted and MAIN.DAT merged. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>
#include <string.h>

#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/scene/walker.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/places/wild.h"
#include "engine/scene/stream.h"
#include "game/dagger/places/town.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/words.h"
#include "game/dagger/play/talk.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/hudbar.h"
#include "game/dagger/play/control.h"
#include "game/dagger/play/service.h"
#include "game/dagger/play/shop.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/court.h"
#include "game/dagger/play/atlas.h"
#include "game/dagger/play/journey.h"
#include "game/dagger/play/travelmap.h"
#include "game/dagger/play/townmap.h"
#include "game/dagger/play/dunmap.h"
#include "game/dagger/play/marker.h"
#include "game/dagger/play/ride.h"
#include "engine/assets/vfs.h"
#include "game/dagger/play/logbook.h"
#include "game/dagger/play/msgbox.h"
#include "game/dagger/play/quest.h"
#include "game/dagger/tables/quests.h"
#include "game/dagger/formats/qbn.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/tables/names.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/play/guild.h"
#include "game/dagger/play/lodging.h"
#include "game/dagger/play/ills.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/formats/hmi.h"
#include "game/dagger/tables/sounds.h"
#include "engine/assets/song.h"
#include "game/dagger/play/wagon.h"
#include "engine/scene/automap.h"
#include "game/dagger/formats/climate.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/formats/dungeon.h"
#include "engine/scene/stream.h"
#include "engine/scene/collide.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/character.h"
#include "game/dagger/play/commands.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/loot.h"
#include "game/dagger/play/pile.h"
#include "game/dagger/play/bag.h"
#include "game/dagger/play/create.h"
#include "game/dagger/play/kit.h"
#include "game/dagger/play/sheet.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/save.h"
#include "engine/scene/clock.h"
#include "engine/platform/fs.h"
#include "game/dagger/formats/biog.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/play/doll.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/play/mouse.h"
#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/settings.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/assets/recfile.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/watch.h"
#include "game/dagger/play/theft.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/saves.h"
#include "game/dagger/play/files.h"
#include "game/dagger/places/people.h"
#include "game/dagger/places/wres.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/texture.h"
#include "engine/ui/pic.h"
#include "game/dagger/tables/items.h"
#include "engine/ui/cmd.h"
#include "engine/core/log.h"
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

/* Classic control intent, independent of Win32/Glide. The expectations use
 * manual behavior and worked distances/angles, rather than copying region
 * implementation: passive movement, edge speed, activation ownership,
 * Alt view versus body heading, and exactly the pixel selected by a use. */
static void check_controls(void)
{
    ControlState s;
    ControlFrame frame;
    Input in;
    Camera view, aim;
    Basis b;
    df_u8 pixels[CONTROL_CURSOR_W * CONTROL_CURSOR_H];
    float turn, slow;
    int i, j, count;

    check_expect(control_region(160, 77, 154) == CONTROL_TARGET && control_region(160, 0, 154) == CONTROL_N
                     && control_region(319, 0, 154) == CONTROL_NE && control_region(319, 77, 154) == CONTROL_E
                     && control_region(319, 153, 154) == CONTROL_SE && control_region(160, 153, 154) == CONTROL_S
                     && control_region(0, 153, 154) == CONTROL_SW && control_region(0, 77, 154) == CONTROL_W
                     && control_region(0, 0, 154) == CONTROL_NW && control_region(160, 154, 154) == -1
                     && control_region(320, 100, 200) == -1 && control_region(160, 199, 200) == CONTROL_S,
                 "controls: arrows, center or HUD region is wrong");
    control_reset(&s);
    memset(&in, 0, sizeof(in));
    in.mouse_dx = 100;
    in.mouse_dy = -100;
    control_step(&s, &in, 1, 1, 319, 0, 154, 0, 1.0f / 60.0f, &frame);
    check_expect(!frame.use && frame.forward == 0.0f && frame.yaw == 0.0f && frame.pitch == 0.0f,
                 "controls: passive cursor motion moved the body or view");
    in.mouse_held = in.mouse_pressed = INPUT_MOUSE_LEFT;
    control_step(&s, &in, 1, 1, 160, 25, 154, 0, 1.0f / 60.0f, &frame);
    slow = frame.forward;
    check_expect(slow > 0.4f && slow < 0.6f && !frame.use && frame.yaw == 0.0f,
                 "controls: held north arrow did not walk at a fraction of full speed");
    in.mouse_pressed = 0;
    control_step(&s, &in, 1, 1, 160, 0, 154, 0, 1.0f / 60.0f, &frame);
    check_expect(frame.forward == 1.0f && frame.forward > slow, "controls: screen edge is not full speed");
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 1.0f / 60.0f, &frame);
    check_expect(frame.forward == 0.0f && !frame.use, "controls: dragging an arrow to X activated something");
    in.mouse_held = 0;
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    in.mouse_pressed = in.mouse_held = INPUT_MOUSE_LEFT;
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(frame.use && frame.forward == 0.0f, "controls: X did not activate once on press");
    in.mouse_pressed = 0;
    control_step(&s, &in, 1, 1, 0, 0, 154, 0, 0.01f, &frame);
    check_expect(!frame.use && frame.forward == 0.0f && frame.yaw == 0.0f,
                 "controls: dragging an X click turned into movement");
    /* The game blocks a HUD/menu click before it reaches the controller. */
    control_block(&s);
    control_step(&s, &in, 1, 1, 0, 0, 154, 0, 0.01f, &frame);
    check_expect(frame.forward == 0.0f && frame.yaw == 0.0f, "controls: held HUD click walked after handoff");

    memset(&in, 0, sizeof(in));
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    in.held = in.pressed = INPUT_BIT(INPUT_SWING);
    in.mouse_dx = 40;
    in.mouse_dy = -15;
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(frame.attack_pressed && frame.attack_held && frame.gesture_dx == 40 && frame.gesture_dy == -15
                     && frame.yaw == 0.0f && frame.pitch == 0.0f && !frame.use,
                 "controls: a right gesture moved the camera or lost its motion");
    in.pressed = 0;
    control_step(&s, &in, 1, 1, 160, 170, 154, 0, 0.01f, &frame);
    check_expect(frame.attack_held, "controls: a world gesture was released by crossing the HUD");
    in.held = 0;
    in.released = INPUT_BIT(INPUT_SWING);
    control_step(&s, &in, 1, 1, 160, 170, 154, 0, 0.01f, &frame);
    check_expect(frame.attack_released, "controls: a world gesture lost its release over the HUD");
    in.released = 0;
    in.held = in.pressed = INPUT_BIT(INPUT_SWING);
    control_step(&s, &in, 1, 1, 319, 0, 154, 1, 0.01f, &frame);
    check_expect(frame.use && !frame.attack_pressed, "controls: sheathed right click did not activate in an arrow region");
    in.held = in.pressed = 0;
    control_step(&s, &in, 1, 1, 319, 0, 154, 0, 0.01f, &frame);
    in.held = INPUT_BIT(INPUT_SWING) | INPUT_BIT(INPUT_RUN);
    in.pressed = INPUT_BIT(INPUT_SWING);
    control_step(&s, &in, 1, 1, 319, 0, 154, 0, 0.01f, &frame);
    check_expect(frame.use && !frame.attack_held, "controls: Shift-right click swung instead of activating");
    control_block(&s);
    in.held = INPUT_BIT(INPUT_SWING);
    in.pressed = 0;
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(!frame.attack_held && !frame.use, "controls: held attack survived a menu");
    in.held = 0;
    in.released = INPUT_BIT(INPUT_SWING);
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(!frame.attack_released, "controls: old menu button release became a swing");

    in.reset = 1;
    in.released = INPUT_BIT(INPUT_SWING);
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(!frame.attack_released && !frame.attack_held && !frame.use,
                 "controls: focus/capture discontinuity became a bow release or click swing");
    in.reset = 0;
    in.released = 0;
    in.held = INPUT_BIT(INPUT_SWING);
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(!frame.attack_held, "controls: held pre-discontinuity button resumed a gesture");

    memset(&in, 0, sizeof(in));
    in.held = INPUT_BIT(INPUT_FREELOOK);
    in.move_f = 1;
    in.mouse_dx = 100;
    in.mouse_dy = -50;
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(frame.forward == 1.0f && frame.yaw == 0.0f && frame.pitch == 0.0f
                     && check_near(s.view_yaw, 0.3f, 0.0001f) && check_near(s.view_pitch, 0.15f, 0.0001f),
                 "controls: Alt look changed the movement heading");
    camera_set(&view, 0, 0, 0, 1.0f, 0.1f);
    control_view(&s, &view);
    check_expect(check_near(view.yaw, 1.3f, 0.0001f) && check_near(view.pitch, 0.25f, 0.0001f),
                 "controls: Alt offset did not reach the camera");
    in.mouse_pressed = INPUT_MOUSE_LEFT;
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(frame.center && s.view_yaw == 0 && s.view_pitch == 0 && !frame.use,
                 "controls: Alt-left did not center without activating");
    memset(&in, 0, sizeof(in));
    in.pressed = INPUT_BIT(INPUT_LOOK_CENTER);
    control_step(&s, &in, 1, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(frame.center, "controls: Home did not center");
    /* One second of arrow turning at 30, 60 and 120 Hz gives the same angle.
     * The legacy look_dx arrow increment is deliberately ignored by play. */
    for (j = 0; j < 3; j++) {
        count = 30 << j;
        memset(&in, 0, sizeof(in));
        in.held = INPUT_BIT(INPUT_TURN_RIGHT);
        in.look_dx = 12;
        turn = 0;
        for (i = 0; i < count; i++) {
            control_step(&s, &in, 1, 1, 160, 77, 154, 0, 1.0f / (float)count, &frame);
            turn += frame.yaw;
        }
        check_expect(check_near(turn, DF_TAU * 0.25f, 0.0001f), "controls: keyboard turning depends on frame rate");
    }
    memset(&in, 0, sizeof(in));
    in.mouse_dx = 100;
    in.mouse_dy = -50;
    control_step(&s, &in, 0, 0, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(check_near(frame.yaw, 0.3f, 0.0001f) && check_near(frame.pitch, 0.15f, 0.0001f),
                 "controls: View mouse look no longer works");
    in.mouse_held = in.mouse_pressed = INPUT_MOUSE_LEFT;
    in.pressed = in.held = INPUT_BIT(INPUT_ATTACK);
    control_step(&s, &in, 0, 1, 160, 77, 154, 0, 0.01f, &frame);
    check_expect(frame.use && !frame.attack_held && frame.yaw == 0, "controls: View free pointer attacked or looked");

    /* Reproject an actual ray at multiple fields of view, yaw and pitch.
     * Screen coordinates are the renderer's full 640x480, even with the HUD. */
    for (j = 0; j < 3; j++) {
        camera_set_fov(j == 0 ? 45.0f : (j == 1 ? 75.0f : 120.0f));
        camera_set(&view, 12, 30, -70, 1.1f, 0.35f);
        for (i = 0; i < 3; i++) {
            float vx, vy, vz, sx, sy, w, z;
            int px, py;

            px = i == 0 ? 100 : (i == 1 ? 320 : 550);
            py = i == 0 ? 180 : (i == 1 ? 240 : 440);
            control_point(&view, px, py, &aim);
            camera_basis(&aim, &b);
            {
                Basis original;

                camera_basis(&view, &original);
                camera_view(&view, &original, view.x + b.fx * b.cp * 100,
                            view.y + b.sp * 100, view.z + b.fz * b.cp * 100, &vx, &vy, &vz);
            }
            check_expect(camera_project(vx, vy, vz, &sx, &sy, &w, &z)
                             && check_near(sx, (float)px, 0.01f) && check_near(sy, (float)py, 0.01f),
                         "controls: pointed use did not follow its screen pixel");
        }
    }
    camera_set_fov(75);
    {
        Scene scene;
        Usable door;
        UseHit hit;

        memset(&scene, 0, sizeof(scene));
        memset(&door, 0, sizeof(door));
        door.model = door.sprite = -1;
        door.kind = USE_OTHER;
        door.cx = 40;
        door.cz = 100;
        door.hw = door.hh = door.hd = 4;
        door.nx = 0;
        door.nz = 1;
        door.reach = 200;
        scene.usables = &door;
        scene.nusables = 1;
        camera_set(&view, 0, 0, 0, 0, 0);
        check_expect(!interact_aim(&scene, &view, &hit), "controls: off-center door unexpectedly met the center ray");
        control_point(&view, 320 + df_iround(camera_focal() * 0.4f), 240, &aim);
        check_expect(interact_aim(&scene, &aim, &hit) && hit.usable == 0,
                     "controls: pointer ray did not select an off-center door");
        {
            Mesh mesh;
            Tri tri;
            Instance wall;
            Bound bound;

            memset(&mesh, 0, sizeof(mesh));
            memset(&tri, 0, sizeof(tri));
            memset(&wall, 0, sizeof(wall));
            tri.x[0] = -100; tri.y[0] = -100; tri.z[0] = 50;
            tri.x[1] = 100; tri.y[1] = -100; tri.z[1] = 50;
            tri.x[2] = 0; tri.y[2] = 100; tri.z[2] = 50;
            mesh.tris = &tri;
            mesh.ntris = 1;
            wall.mesh = &mesh;
            wall.m[0] = wall.m[4] = wall.m[8] = 1;
            bound.cx = bound.cy = 0;
            bound.cz = 50;
            bound.ex = bound.ey = 100;
            bound.ez = 1;
            scene.models = &wall;
            scene.nmodels = 1;
            scene.bounds = &bound;
            check_expect(!interact_aim(&scene, &aim, &hit) && hit.model == 0,
                         "controls: pointed use reached a door through a wall");
        }
    }
    control_cursor_sheet(pixels);
    for (j = 0; j < 12; j++) {
        count = 0;
        for (i = 0; i < 256; i++) {
            count += pixels[(j / 4 * 16 + i / 16) * CONTROL_CURSOR_W + j % 4 * 16 + i % 16] != 0;
        }
        check_expect(j < 9 ? count > 15 : count == 0, "controls: cursor sheet has missing or spilled indicators");
    }
    control_set_classic(1);
    printf("controls: cursor movement, X/right use, gestures, Alt/Home, handoffs, settings, 30/60/120 Hz and pointed rays\n");
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
    if (!character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test")) {
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

    /* A cursor fraction scales requested ground motion, not elapsed time.
     * Use copies so classic's established full-speed checks below still
     * start at the marker. Half input covers half a walk while the same
     * jump velocity and gravity advance in both bodies. */
    {
        Player half, still;
        float dx, dz;

        half = still = p;
        for (i = 0; i < 15; i++) {
            player_step_motion(&half, &in, &level.scene, 1.0f / 60.0f, 1, 0.5f, 0.0f);
        }
        dx = half.body.feet[0] - p.body.feet[0];
        dz = half.body.feet[2] - p.body.feet[2];
        want = rules_walk_speed(sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 0.125f;
        check_expect(check_near(df_sqrt(dx * dx + dz * dz), want, 1.0f), "controls: half cursor speed did not cover half a walk");
        half = p;
        body_jump(&half.body, 100.0f);
        body_jump(&still.body, 100.0f);
        player_step_motion(&half, &in, &level.scene, 1.0f / 60.0f, 1, 0.5f, 0.0f);
        player_step_motion(&still, &in, &level.scene, 1.0f / 60.0f, 1, 0.0f, 0.0f);
        check_expect(check_near(half.body.vy, still.body.vy, 0.001f)
                         && check_near(half.body.feet[1], still.body.feet[1], 0.001f),
                     "controls: cursor speed changed gravity or jump time");
    }

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

/*
 * State that outlives a load (keep.c through world.c): in the hold,
 * every door and creature keyed; a door opened and a lever pulled, the
 * place saved and loaded again, come back so; one door shut again loses
 * its row; the others are as the file has them.
 */
static void check_keep(void)
{
    World *world;
    const Mover *m;
    int door;
    int lever;
    int keyed;
    int i;

    keep_clear();
    world = world_load();
    if (!world) {
        check_expect(0, "keep: the hold did not load");
        return;
    }
    door = -1;
    lever = -1;
    keyed = 0;
    for (i = 0; i < world_mover_count(world); i++) {
        m = world_mover(world, i);
        keyed += m->key.place != 0 || m->key.thing != 0;
        if (m->teleport || m->lock) {
            continue;
        }
        if (m->door && door < 0) {
            door = i;
        } else if (!m->door && lever < 0) {
            lever = i;
        }
    }
    for (i = 0; i < world_entity_count(world); i++) {
        keyed += world_entity(world, i)->key.thing != 0;
    }
    check_expect(keyed == world_mover_count(world) + world_entity_count(world),
                 "keep: not every door, lever and creature of the hold is keyed");
    {
        KeepKey keys[256];
        int n;
        int j;
        int twice;

        n = 0;
        for (i = 0; i < world_mover_count(world) && n < 256; i++) {
            keys[n++] = world_mover(world, i)->key;
        }
        for (i = 0; i < world_entity_count(world) && n < 256; i++) {
            keys[n++] = world_entity(world, i)->key;
        }
        twice = 0;
        for (i = 0; i < n; i++) {
            for (j = i + 1; j < n; j++) {
                twice += keys[i].place == keys[j].place && keys[i].thing == keys[j].thing;
            }
        }
        check_expect(twice == 0, "keep: two of the hold's things share a key");
    }
    check_expect(door >= 0 && lever >= 0, "keep: the hold has no unlocked door and lever to try");
    if (door < 0 || lever < 0) {
        return;
    }
    mover_set_end((Mover *)world_mover(world, door), 1);
    mover_set_end((Mover *)world_mover(world, lever), 1);
    world_keep_save(world);
    check_expect(keep_count() == 2, "keep: leaving the hold kept more or less than its open door and pulled lever");
    world = world_load();
    check_expect(world && mover_at_end(world_mover(world, door)) && mover_at_end(world_mover(world, lever))
                     && !mover_at_end(world_mover(world, door == 0 ? 1 : 0)),
                 "keep: the door and lever did not come back as left, or another moved");
    if (world) {
        mover_set_end((Mover *)world_mover(world, door), 0);
        world_keep_save(world);
        check_expect(keep_count() == 1, "keep: a door shut again keeps its row");
    }
    printf("keep: the hold's %d things keyed; a door and a lever kept across a reload\n", keyed);
    keep_clear();
}

/*
 * The hold streamed by block (dunstream.c, docs/streaming.md Dungeons):
 * its ring round the start holds every block of it, as many models,
 * flats, lights, doors and creatures as the hold loaded whole; the start
 * is the whole load's; a door opened, the camera walked far off (its
 * blocks dropped) and back, comes back open.
 */
static void check_dunstream(void)
{
    Dungeon hold;
    World *whole;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    float wx, wy, wz, wyaw, wpitch;
    int models, sprites, lights, movers, ents, water;
    int door;
    KeepKey key;
    int fixed_treasure;
    int i;

    keep_clear();
    whole = world_load();
    if (!dungeon_load(&hold) || !whole) {
        check_expect(0, "dunstream: the hold did not load");
        return;
    }
    world_start(whole, &wx, &wy, &wz, &wyaw, &wpitch);
    /* Every creature of the whole load read, so its live ones can be counted against the stream's. */
    world_creatures_step(whole, 0, 0.0f, 0.0f);
    {
        int live;

        live = 0;
        for (i = 0; i < world_entity_count(whole); i++) {
            live += world_entity(whole, i)->live;
        }
        printf("dunstream: the whole hold's creatures: %d, %d of them drawn\n", world_entity_count(whole), live);
    }
    if (!dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "dunstream: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    models = 0;
    sprites = 0;
    lights = 0;
    movers = 0;
    ents = 0;
    water = 0;
    for (i = 0; i < level.scene.nmodels; i++) {
        models += level.scene.models[i].mesh != 0 && level.scene.models[i].mesh->ntris > 0;
    }
    for (i = 0; i < level.scene.nsprites; i++) {
        sprites += level.scene.sprites[i].w > 0.0f;
    }
    for (i = 0; i < level.scene.nlights; i++) {
        lights += level.scene.lights[i].radius > 0.0f;
    }
    fixed_treasure = 0;
    door = -1;
    for (i = 0; i < level.scene.nmovers; i++) {
        if (level.scene.movers[i].inst) {
            movers++;
            if (door < 0 && level.scene.movers[i].door && !level.scene.movers[i].lock
                && !level.scene.movers[i].teleport) {
                door = i;
            }
        }
    }
    /* Creatures alone: the stream's entities also hold the block's treasure and spare things. */
    for (i = 0; i < level.scene.nentities; i++) {
        ents += level.scene.entities[i].live && dunstream_kind(&level.scene.entities[i], 0) >= 0;
    }
    /* The whole load draws treasure flats as flats; the stream makes them entities. */
    for (i = 0; i < world_flat_count(whole); i++) {
        fixed_treasure += world_flat(whole, i)->file == RDB_TREASURE_ARCHIVE;
    }
    for (i = 0; i < level.scene.nwater; i++) {
        water += level.scene.water[i].x1 > level.scene.water[i].x0;
    }
    printf("dunstream: the hold's ring: %d models, %d flats, %d lights, %d movers, %d creatures (whole: %d, %d, %d, %d, %d)\n",
           models, sprites, lights, movers, ents, world_model_count(whole), world_flat_count(whole),
           world_light_count(whole), world_mover_count(whole), world_entity_count(whole));
    check_expect(check_near(x, wx, 0.5f) && check_near(z, wz, 0.5f), "dunstream: the stream does not start where the hold does");
    check_expect(models == world_model_count(whole) && lights == world_light_count(whole)
                     && movers == world_mover_count(whole) && ents == world_entity_count(whole) && water == 0,
                 "dunstream: the hold's ring does not hold what the hold loaded whole holds");
    check_expect(sprites + fixed_treasure == world_flat_count(whole), "dunstream: the hold's ring has another count of flats");
    /* Each creature the same one: its key, and where it stands (the hold's start block is the stream's origin). */
    {
        int same;
        int w;

        same = 0;
        for (w = 0; w < world_entity_count(whole); w++) {
            const Entity *a;

            a = world_entity(whole, w);
            for (i = 0; i < level.scene.nentities; i++) {
                const Entity *b;

                b = &level.scene.entities[i];
                if (b->live && b->key.place == a->key.place && b->key.thing == a->key.thing) {
                    same += check_near(a->x, b->x, 0.5f) && check_near(a->y, b->y, 0.5f)
                            && check_near(a->z, b->z, 0.5f);
                    break;
                }
            }
        }
        check_expect(same == world_entity_count(whole),
                     "dunstream: a creature of the stream is not the whole hold's, keyed and standing alike");
    }
    /* The player in the streamed hold: on its floor, and a quarter second's walk goes classic's distance. */
    {
        Character sheet;
        Player p;
        Input in;
        float walked;
        float want;

        if (character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test")) {
            player_init(&p, &sheet);
            player_place(&p, x, y, z, yaw, pitch);
            memset(&in, 0, sizeof(in));
            walk(&p, &level.scene, &in, 60);
            /* The cave: the body settles on the floor at the marker, not thrown elsewhere by the stuck search. */
            check_expect(p.body.grounded && check_near(p.body.feet[0], 510.0f, 1.0f) && check_near(p.body.feet[2], 1672.0f, 1.0f)
                             && p.body.feet[1] < 25.0f,
                         "dunstream: the player does not settle at the hold's enter marker");
            /* The start is at the bottom of the hold, far from the way out. */
            for (i = 0; i < level.scene.nusables; i++) {
                const Usable *u;

                u = &level.scene.usables[i];
                if (u->kind == USE_PASSAGE) {
                    float dx;
                    float dy;
                    float dz;

                    dx = u->cx - p.body.feet[0];
                    dy = u->cy - p.body.feet[1];
                    dz = u->cz - p.body.feet[2];
                    check_expect(dx * dx + dy * dy + dz * dz > 1000.0f * 1000.0f, "dunstream: the hold starts beside its way out");
                }
            }
            in.move_f = 1;
            walked = walk(&p, &level.scene, &in, 15);
            want = rules_walk_speed(sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 0.25f;
            check_expect(p.body.grounded && check_near(walked, want, want * 0.05f),
                         "dunstream: the player does not stand and walk in the streamed hold");
        }
    }
    /* A door opened, the camera taken 8 blocks east (the hold dropped) and back: still open. */
    if (door >= 0) {
        key = level.scene.movers[door].key;
        mover_set_end(&level.scene.movers[door], 1);
        camera_set(&cam, x + 8.0f * 2048.0f, y, z, yaw, pitch);
        for (i = 0; i < 200; i++) {
            dunstream_update(&cam, 50.0f);
        }
        check_expect(keep_find(key) != 0, "dunstream: a door opened in a dropped block is not kept");
        camera_set(&cam, x, y, z, yaw, pitch);
        dunstream_fill(&cam);
        for (i = 0; i < level.scene.nmovers; i++) {
            if (level.scene.movers[i].key.place == key.place && level.scene.movers[i].key.thing == key.thing) {
                break;
            }
        }
        check_expect(i < level.scene.nmovers && mover_at_end(&level.scene.movers[i]),
                     "dunstream: a door opened comes back shut after its block came back");
    } else {
        check_expect(0, "dunstream: no unlocked door in the hold's stream");
    }
    dunstream_close();
    keep_clear();
}

/*
 * Weapons from MAIN.DAT's ITEM, WEAP and MATL by DFU's material rules,
 * worked by hand; the pack and the hand; the console's commands.
 */
static void check_items(void)
{
    static Player p;
    Inventory inv;
    Item it;
    char name[48];

    check_expect(item_table()->count == 35 && weapon_table()->count == 18 && armor_table()->count == 11
                     && material_table()->count == MATERIAL_COUNT && loot_table()->count == 21,
                 "items: MAIN.DAT lacks classic's 34 items (a cart, a horse, 11 of armour, 18 swung, arrows, a letter of credit, gold) and a quest's parcel, 12 materials or 21 loot tables");
    check_expect(item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON) && check_near(it.weight, 4.5f, 0.001f)
                     && it.value == 45 && it.condition_max == 800 && it.condition == 800,
                 "items: an iron longsword is not 4.5 kg, 45 gold, 800");
    /* 18 quarters times 5 is 22.5 quarters: half to even, 22, 5.5 kg. */
    check_expect(item_make(&it, ITEM_LONGSWORD, MATERIAL_STEEL) && check_near(it.weight, 5.5f, 0.001f)
                     && it.value == 90 && it.condition_max == 1200,
                 "items: a steel longsword is not 5.5 kg, 90 gold, 1200");
    check_expect(item_make(&it, ITEM_DAGGER, MATERIAL_DAEDRIC) && check_near(it.weight, 0.5f, 0.001f)
                     && it.value == 1536 && it.condition_max == 400,
                 "items: a daedric dagger is not 0.5 kg, 1536 gold, 400");
    item_name(&it, name, (int)sizeof(name));
    check_expect(strcmp(name, "Daedric Dagger") == 0, "items: the daedric dagger's name");
    check_expect(!item_make(&it, 9999, MATERIAL_IRON) && !item_make(&it, ITEM_DAGGER, 99),
                 "items: an unknown template or material made an item");
    check_expect(item_find_weapon("dai katana") == 123 && item_find_weapon("LONGSWORD") == ITEM_LONGSWORD
                     && item_find_weapon("potato") < 0 && item_find_material("daedric") == MATERIAL_DAEDRIC,
                 "items: weapons or materials are not found by name");
    check_expect(material(MATERIAL_IRON)->modifier == -1 && material(MATERIAL_DAEDRIC)->modifier == 6
                     && weapon_template(ITEM_LONGSWORD)->damage_min == 2 && weapon_template(ITEM_LONGSWORD)->damage_max == 16,
                 "items: the longsword's damage or the materials' modifiers are not classic's");

    inventory_clear(&inv);
    item_make(&it, ITEM_DAGGER, MATERIAL_IRON);
    inventory_add(&inv, &it);
    item_make(&it, ITEM_LONGSWORD, MATERIAL_STEEL);
    inventory_add(&inv, &it);
    check_expect(inventory_equip(&inv, 1) && inventory_weapon(&inv) == &inv.item[1] && !inventory_equip(&inv, 5),
                 "items: the hand does not take a weapon from the pack");
    inventory_remove(&inv, 0);
    check_expect(inv.worn[EQUIP_RIGHT_HAND] == 0 && inventory_weapon(&inv)->template_id == ITEM_LONGSWORD,
                 "items: the hand loses its weapon when another leaves the pack");
    inventory_remove(&inv, 0);
    check_expect(inv.count == 0 && !inventory_weapon(&inv), "items: the hand keeps a weapon that left the pack");

    /* A Warrior: a player of nothing is class 0, a Mage, who may not hold a long blade. */
    character_new(&p.sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    inventory_clear(&p.pack);
    commands_setup();
    commands_player(&p);
    cmd_run("additem dwarven claymore; additem long sword; additem Dai-Katana Ebony; equip 1");
    check_expect(p.pack.count == 3 && p.pack.item[0].material == MATERIAL_DWARVEN && p.pack.item[1].material == MATERIAL_IRON
                     && p.pack.item[2].template_id == 123 && p.pack.item[2].material == MATERIAL_EBONY && p.pack.worn[EQUIP_RIGHT_HAND] == 1,
                 "items: additem and equip did not fill the pack and the hand");
    check_expect(check_near(inventory_weight(&p.pack), 5.5f + 4.5f + 1.75f, 0.001f), "items: the pack's weight");
    cmd_run("additem potato; equip 7; equip none");
    check_expect(p.pack.count == 3 && p.pack.worn[EQUIP_RIGHT_HAND] == -1, "items: a bad additem or equip changed the pack");
    /* tgm: alone it flips; on, a blow takes nothing; off, it does. */
    p.sheet.health = 10;
    p.god = 0;
    cmd_run("tgm");
    check_expect(p.god == 1 && player_hurt(&p, 50) == -1 && p.sheet.health == 10, "items: tgm on, the player was hurt");
    cmd_run("tgm");
    check_expect(p.god == 0 && player_hurt(&p, 3) == 0 && p.sheet.health == 7, "items: tgm off, a blow took nothing");
    cmd_run("tgm on; tgm off");
    check_expect(p.god == 0, "items: tgm on and off do not set it");
    commands_player(0);
    cmd_run("additem dagger");
    check_expect(p.pack.count == 3, "items: additem out of play added to a pack");
    printf("items: %d weapons swung, %d materials; additem, items, equip\n", weapon_table()->count, material_table()->count);
}

/* A card that takes every tile, for the swing frames' bytes: a tile is its side squared, twice when it goes wide (1555). */
static df_u32 wc_bytes(int side, int wide)
{
    return (df_u32)side * (df_u32)side * (wide ? 2u : 1u);
}

static int wc_upload(const df_u8 *texels, int side, int wide, df_u32 addr, df_u32 *handle)
{
    (void)texels;
    (void)side;
    (void)wide;
    *handle = addr + 1;
    return 1;
}

static void wc_release(df_u32 handle)
{
    (void)handle;
}

static void wc_begin(int how, int alpha)
{
    (void)how;
    (void)alpha;
}

static void wc_palette(int id, const df_u32 *table)
{
    (void)id;
    (void)table;
}

static void wc_quad(df_u32 handle, float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1)
{
    (void)handle;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)s0;
    (void)t0;
    (void)s1;
    (void)t1;
}

static void wc_end(void)
{
}

/* 1 when every drawn pixel of f lies in its bounds and each edge of them touches one: the crop loses nothing and is tight. */
static int bounds_exact(const CifFrame *f)
{
    int x;
    int y;
    int w;
    int h;
    int edges;
    int row;
    int col;

    if (!cif_bounds(f, &x, &y, &w, &h)) {
        for (row = 0; row < f->w * f->h; row++) {
            if (f->pixels[row]) {
                return 0;
            }
        }
        return 1;
    }
    edges = 0;
    for (row = 0; row < f->h; row++) {
        for (col = 0; col < f->w; col++) {
            if (!f->pixels[row * f->w + col]) {
                continue;
            }
            if (col < x || col >= x + w || row < y || row >= y + h) {
                return 0;
            }
            edges |= (col == x) | (col == x + w - 1) << 1 | (row == y) << 2 | (row == y + h - 1) << 3;
        }
    }
    return edges == 15;
}

/* A frame cropped to its bounds, as weapon.c sends it. */
static int load_cropped(const CifFrame *f, const df_u8 *rgb)
{
    static df_u8 buf[320 * 200];
    int x;
    int y;
    int w;
    int h;
    int row;

    if (!cif_bounds(f, &x, &y, &w, &h)) {
        return 0;
    }
    for (row = 0; row < h; row++) {
        memcpy(buf + row * w, f->pixels + (y + row) * f->w + x, (size_t)w);
    }
    return pic_load(buf, w, h, rgb, 1);
}

/*
 * The largest swing frame of WEAPON00..10.CIF as a cutout on the card,
 * cropped as weapon.c sends it (*whole: as stored, uncropped), and the
 * largest whole swing's cropped frames together (*swing): one frame
 * shows at a time beside the armed picture and the HUD, inside the
 * game's GAME_PICTURE_KB (docs/game.md, Combat). chroma 1: the card has
 * the key and a cutout goes as P_8; 0: as 1555, twice the bytes.
 * *exact: every frame's crop loses no pixel and is tight.
 */
static df_u32 largest_swing_frame(int chroma, df_u32 *whole, df_u32 *swing, int *exact)
{
    static const PicOps keyed = { wc_bytes, wc_upload, wc_release, wc_begin, wc_palette, wc_quad, wc_end, 1 };
    static const PicOps wide = { wc_bytes, wc_upload, wc_release, wc_begin, wc_palette, wc_quad, wc_end, 0 };
    static df_u8 rgb[768];
    df_u32 most;
    int n;

    most = 0;
    *whole = 0;
    *swing = 0;
    *exact = 1;
    pic_begin(0, 1024u * 1024u, chroma ? &keyed : &wide);
    for (n = 0; n <= 10; n++) {
        char name[16];
        CifSet set;
        int i;

        sprintf(name, "WEAPON%02d.CIF", n);
        if (!cif_open(name, &set)) {
            continue;
        }
        df_u32 sum;
        int group;

        sum = 0;
        group = -1;
        for (i = 0; i < set.n; i++) {
            int id;
            int pics;
            int on;
            df_u32 used;
            df_u32 room;

            if (set.frame[i].group != group) {
                sum = 0;
                group = set.frame[i].group;
            }

            id = pic_load(set.frame[i].pixels, set.frame[i].w, set.frame[i].h, rgb, 1);
            pic_stats(&pics, &on, &used, &room);
            if (used > *whole) {
                *whole = used;
            }
            pic_free(id);
            id = load_cropped(&set.frame[i], rgb);
            pic_stats(&pics, &on, &used, &room);
            if (used > most) {
                most = used;
            }
            pic_free(id);
            /* Group 0 is the armed picture, not a swing. */
            sum += used;
            if (group > 0 && sum > *swing) {
                *swing = sum;
            }
            if (!bounds_exact(&set.frame[i])) {
                *exact = 0;
            }
        }
        cif_close(&set);
    }
    pic_end();
    return most;
}

/*
 * The weapon in hand: its file's armed picture where classic puts it, a
 * click's random swing on the screen's bottom, its frames at classic's
 * rate and back to rest; bare hands; a bow at rest shows nothing.
 */
static void check_weapon(void)
{
    Item it;
    int pic;
    int left;
    int top;
    int w;
    int h;
    int seen;
    int swing;
    int ok;
    int i;
    float each;
    df_u32 most;
    df_u32 whole;
    df_u32 swing_bytes;
    int exact;

    each = rules_swing_frame_time(50);
    check_expect(check_near(each, 195.0f / 980.0f, 0.0001f), "weapon: a swing's frame is not 3 (115 - Speed) / 980 s");
    item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON);
    weapon_hold(&it);
    /* WEAPON04.CIF's armed picture is 60 x 86 at 260, 114; it draws cropped inside that. */
    check_expect(weapon_shown(&pic, &left, &top, &w, &h) && left >= 260 && top >= 114 && left + w <= 320 && top + h <= 200
                     && w > 30 && h > 40,
                 "weapon: the longsword at rest is not inside WEAPON04.CIF's armed picture at 260, 114");
    seen = 0;
    ok = 1;
    for (i = 0; i < 40; i++) {
        int f;

        weapon_step(0.013f * (float)(i % 7), 50);
        swing = weapon_attack();
        if (swing < 1 || swing > 6 || !weapon_swinging() || weapon_attack()) {
            ok = 0;
            break;
        }
        seen |= 1 << swing;
        /* Five frames, each on classic's screen, then rest. */
        for (f = 0; f < 5; f++) {
            if (!weapon_shown(&pic, &left, &top, &w, &h) || top < 0 || top + h > 200 || left < 0 || left + w > 320) {
                ok = 0;
            }
            weapon_step(each, 50);
        }
        if (weapon_swinging()) {
            ok = 0;
        }
    }
    check_expect(ok, "weapon: a swing does not run five frames on classic's screen and stop");
    check_expect((seen & 0x7E) == 0x7E, "weapon: forty clicks did not show all six swings");
    check_expect(weapon_shown(&pic, &left, &top, &w, &h) && top >= 114 && left >= 260, "weapon: the armed picture does not come back");
    weapon_hold(0);
    check_expect(weapon_shown(&pic, &left, &top, &w, &h) && left >= 214 && top >= 147 && left + w <= 214 + 63 && top + h <= 200,
                 "weapon: bare hands are not WEAPON10.CIF's fist (63 x 53 at 214, 147)");
    item_make(&it, 129, MATERIAL_IRON);
    weapon_hold(&it);
    check_expect(!weapon_shown(&pic, &left, &top, &w, &h) && weapon_attack() == 1 && weapon_shown(&pic, &left, &top, &w, &h),
                 "weapon: a bow shows at rest, or does not shoot");
    /* Tab during a drawn bow cancels it. Opening the HUD is not releasing
     * a shot; the same invariant protects the sheath button. */
    weapon_gesture(0, 1, 0, 0, 0, 0.0f);
    weapon_step(each * 3.0f, 50);
    weapon_cancel();
    weapon_step(each * 12.0f, 50);
    check_expect(!weapon_swinging() && !weapon_hit() && !weapon_shown(&pic, &left, &top, &w, &h),
                 "hud: cancelling a drawn bow fired or left its frame visible");
    item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON);
    weapon_hold(&it);
    {
        int idle;

        weapon_shown(&idle, &left, &top, &w, &h);
        weapon_attack();
        weapon_step(each * 2.0f, 50);
        weapon_cancel();
        check_expect(!weapon_hit() && !weapon_swinging() && weapon_shown(&pic, &left, &top, &w, &h) && pic == idle,
                     "hud: cancelling a swing lost the cached idle picture or left a hit due");
        check_expect(weapon_gate(1, 1) && weapon_gate(0, 1) && weapon_gate(0, 0) && !weapon_gate(0, 0),
                     "hud: a held click or its release escaped the pointer-to-attack gate");
    }
    weapon_free();
    /* A card with the chroma key: cutouts as P_8. */
    most = largest_swing_frame(1, &whole, &swing_bytes, &exact);
    check_expect(exact, "weapon: a frame's crop loses a pixel or is not tight");
    check_expect(most > 0 && most <= 48u * 1024u, "weapon: a cropped swing frame takes more than 48 KB of a keyed card");
    printf("weapon: swings of 5 frames at %.3f s, all six seen, fists, a bow; on a keyed card the largest frame %u bytes"
           " cropped (%u whole), the largest swing's frames %u together\n",
           each, (unsigned)most, (unsigned)whole, (unsigned)swing_bytes);
    /* A card without it: cutouts as 1555, twice. */
    most = largest_swing_frame(0, &whole, &swing_bytes, &exact);
    check_expect(most > 0 && most <= 96u * 1024u, "weapon: a cropped swing frame takes more than 96 KB of a 1555 card");
    printf("weapon: on a card without the key the largest frame %u bytes cropped (%u whole)\n", (unsigned)most,
           (unsigned)whole);
}

/*
 * Combat: the creatures' attributes from their CFGs, classic's sums
 * worked by hand, and in the streamed hold a creature struck from the
 * eye until it dies, a body after its block is dropped and comes back.
 */
static void check_combat(void)
{
    const EnemyKind *rat;
    const EnemyKind *ghost;
    Character pc;
    Foe f;
    Blow b;
    Item sword;
    Item silver;
    int lo;
    int hi;
    int hits;
    int i;

    rat = enemy_kind(ENEMY_RAT);
    ghost = enemy_kind(ENEMY_GHOST);
    check_expect(rat && rat->attribute[ATTR_STRENGTH] == 40 && rat->attribute[ATTR_AGILITY] == 80
                     && rat->attribute[ATTR_LUCK] == 50 && enemy_kind(ENEMY_LAMIA)->attribute[ATTR_AGILITY] == 85,
                 "combat: a creature's attributes are not its ENEMY###.CFG's");
    check_expect(enemy_kind(ENEMY_FIRST_CLASS)->attribute[ATTR_INTELLIGENCE] == career_class(0)->attribute[ATTR_INTELLIGENCE],
                 "combat: a class enemy's attributes are not its CLASS##.CFG's");
    check_expect(combat_strength_bonus(50) == 0 && combat_strength_bonus(75) == 5 && combat_strength_bonus(42) == -2
                     && combat_strength_bonus(54) == 0,
                 "combat: the strength bonus is not floor((STR - 50) / 5)");
    combat_foe(&f, rat, 1, 0);
    check_expect(f.level == 1 && f.armor == 30 && f.dodging == 35, "combat: the rat is not level 1, armour 30, Dodging 35");
    combat_foe(&f, enemy_kind(ENEMY_FIRST_CLASS), 7, 0);
    check_expect(f.level == 7 && f.armor == 60 && f.dodging == 65, "combat: a class enemy is not at the player's level, bare");
    for (i = 0; i < 200; i++) {
        int h;

        h = combat_roll_health(&f);
        if (h < 17 || h > 10 + 7 * career_class(0)->hp_per_level) {
            break;
        }
    }
    check_expect(i == 200, "combat: a class enemy's health is not 10 and a roll a level");

    if (!character_new(&pc, RACE_BRETON, CLASS_WARRIOR, 0, "Test")) {
        check_expect(0, "combat: no starting character");
        return;
    }
    item_make(&sword, ITEM_LONGSWORD, MATERIAL_IRON);
    /* No critical strike, so the chance is the sum alone. */
    pc.skill[SKILL_CRITICAL_STRIKE] = 0;
    combat_foe(&f, rat, 1, 0);
    combat_blow(&pc, &sword, 3, &f, &b);
    printf("combat: a Breton warrior's long blade %d, luck %d, agility %d, strength %d; at the rat with an iron longsword "
           "swung left, %d%% to hit\n",
           pc.skill[29], pc.attribute[ATTR_LUCK], pc.attribute[ATTR_AGILITY], pc.attribute[ATTR_STRENGTH], b.chance);
    /*
     * By hand: long blade 30, swung left 0, iron -10, the rat's armour 30,
     * luck (55 - 50) / 10 = 0, agility (62 - 80) / 10 = -1 (cut toward 0,
     * as C# does), its Dodging 35 / 4 = 8, against a creature 40, less 50.
     */
    check_expect(b.chance == 31, "combat: a Breton warrior's chance at the rat is not 31");
    /* Damage: the longsword's 2..16, the strength bonus, iron's -1, swung left (0); never below 0. */
    lo = 1000;
    hi = -1;
    hits = 0;
    combat_seed(7);
    for (i = 0; i < 2000; i++) {
        combat_blow(&pc, &sword, 3, &f, &b);
        if (b.hit) {
            hits++;
            lo = b.damage < lo ? b.damage : lo;
            hi = b.damage > hi ? b.damage : hi;
        }
    }
    {
        int bonus;

        bonus = combat_strength_bonus(pc.attribute[ATTR_STRENGTH]) - 1;
        check_expect(lo == (2 + bonus < 1 ? 0 : 2 + bonus) && hi == 16 + bonus,
                     "combat: an iron longsword's damage is not 2..16 with strength and iron's -1");
        check_expect(check_near((float)hits / 2000.0f, (float)b.chance / 100.0f, 0.04f),
                     "combat: blows hit at another rate than their chance");
    }
    /* A ghost takes silver or better: iron does nothing, silver hurts. */
    combat_foe(&f, ghost, 1, 0);
    combat_blow(&pc, &sword, 3, &f, &b);
    check_expect(b.ineffective && !b.hit && !b.damage, "combat: iron hurts a ghost");
    item_make(&silver, ITEM_LONGSWORD, MATERIAL_SILVER);
    combat_blow(&pc, &silver, 3, &f, &b);
    check_expect(!b.ineffective, "combat: silver does not reach a ghost");
    /* Bare hands: Hand-to-Hand / 10 + 1 to / 5 + 1, with strength. */
    combat_foe(&f, rat, 1, 0);
    lo = 1000;
    hi = -1;
    for (i = 0; i < 2000; i++) {
        combat_blow(&pc, 0, 3, &f, &b);
        if (b.hit) {
            lo = b.damage < lo ? b.damage : lo;
            hi = b.damage > hi ? b.damage : hi;
        }
    }
    {
        int h2h;
        int str;
        int want_lo;

        h2h = pc.skill[SKILL_HAND_TO_HAND];
        str = combat_strength_bonus(pc.attribute[ATTR_STRENGTH]);
        want_lo = h2h / 10 + 1 + str;
        check_expect(lo == (want_lo < 0 ? 0 : want_lo) && hi == h2h / 5 + 1 + str,
                     "combat: bare hands' damage is not Hand-to-Hand's range with strength");
    }

    /* In the streamed hold: a creature struck from the eye until it dies, kept dead. */
    {
        Dungeon hold;
        Level level;
        Camera cam;
        float x, y, z, yaw, pitch;
        Entity *target;
        KeepKey key;
        const Keep *row;
        float feet[3];
        float eye[3];
        int struck;
        int flinched;
        int s;

        keep_clear();
        combat_reset();
        if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
            check_expect(0, "combat: the hold's stream did not open");
            return;
        }
        camera_set(&cam, x, y, z, yaw, pitch);
        dunstream_fill(&cam);
        target = 0;
        struck = 0;
        /* A creature from any side in turn, a metre and a quarter off: the first a strike reaches. */
        for (s = 0; s < stream_slots() && !target; s++) {
            Entity *ents;
            int first;
            int n;

            ents = stream_slot_entities(s, &first, &n);
            for (i = 0; ents && i < n && !target; i++) {
                int side;

                if (!ents[i].live || ents[i].dead || dunstream_kind(&ents[i], 0) < 0) {
                    continue;
                }
                for (side = 0; side < 4 && !target; side++) {
                    float a;

                    a = (float)side * (DF_TAU / 4.0f);
                    feet[0] = ents[i].x - df_sin(a) * 50.0f;
                    feet[1] = ents[i].y;
                    feet[2] = ents[i].z - df_cos(a) * 50.0f;
                    eye[0] = feet[0];
                    eye[1] = feet[1] + WALK_EYE;
                    eye[2] = feet[2];
                    if (combat_reaches(&level.scene, feet, eye, a, &ents[i])) {
                        /* Classic's arc: 40 degrees off it is out, 30 off it is in. */
                        check_expect(!combat_reaches(&level.scene, feet, eye, a + 40.0f * DF_TAU / 360.0f, &ents[i])
                                         && combat_reaches(&level.scene, feet, eye, a - 30.0f * DF_TAU / 360.0f, &ents[i]),
                                     "combat: the strike's arc is not classic's 35 degrees");
                        /* Aimed at: the view that rests on it finds it, and the opposite way does not. */
                        {
                            float to[3];
                            float away[3];
                            float len;

                            to[0] = ents[i].x - eye[0];
                            to[1] = ents[i].y + entity_height(&ents[i]) * 0.5f - eye[1];
                            to[2] = ents[i].z - eye[2];
                            len = df_sqrt(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
                            to[0] /= len;
                            to[1] /= len;
                            to[2] /= len;
                            away[0] = -to[0];
                            away[1] = to[1];
                            away[2] = -to[2];
                            check_expect(combat_aim(&level.scene, eye, to, 400.0f) == &ents[i], "combat: the view on a creature does not aim at it");
                            check_expect(combat_aim(&level.scene, eye, away, 400.0f) != &ents[i], "combat: the view away from a creature aims at it");
                        }
                        pc.fatigue = pc.fatigue_max;
                        combat_strike(&pc, &sword, 3, &level.scene, feet, eye, a);
                        target = &ents[i];
                        key = ents[i].key;
                        yaw = a;
                    }
                }
            }
        }
        check_expect(target != 0, "combat: no creature of the hold could be struck from a metre and a quarter");
        if (!target) {
            dunstream_close();
            return;
        }
        check_expect(pc.fatigue == pc.fatigue_max - COMBAT_SWING_FATIGUE, "combat: a swing does not tire by 11");
        flinched = 0;
        combat_seed(11);
        for (i = 0; i < 400 && !target->dead; i++) {
            combat_strike(&pc, &sword, 3, &level.scene, feet, eye, yaw);
            if (!target->dead && target->act == ENTITY_HURT) {
                flinched = 1;
                combat_frame(1.0f);
                check_expect(target->act == 0, "combat: a flinch does not run out");
            }
            struck++;
        }
        row = keep_find(key);
        printf("combat: %s struck %d times in the hold till it died\n", enemy_kind(dunstream_kind(target, 0))->name, struck);
        check_expect(target->dead && row && row->game[2] == 1 && row->game[0] == 0 && row->game[1] == 0,
                     "combat: the creature does not die, or its health is not kept at 0");
        check_expect(!target->pose[ENTITY_DEAD].w[0] || (target->act == ENTITY_DEAD && target->hold),
                     "combat: a dead creature does not lie as its body");
        check_expect(flinched || !target->pose[ENTITY_HURT].w[0], "combat: a creature struck and living did not flinch");
        /* Its block dropped and back: the body still a body. */
        camera_set(&cam, eye[0] + 8.0f * 2048.0f, eye[1], eye[2], yaw, 0.0f);
        for (i = 0; i < 200; i++) {
            dunstream_update(&cam, 50.0f);
        }
        camera_set(&cam, eye[0], eye[1], eye[2], yaw, 0.0f);
        dunstream_fill(&cam);
        combat_frame(0.0f);
        target = 0;
        for (s = 0; s < stream_slots() && !target; s++) {
            Entity *ents;
            int first;
            int n;

            ents = stream_slot_entities(s, &first, &n);
            for (i = 0; ents && i < n; i++) {
                if (ents[i].key.place == key.place && ents[i].key.thing == key.thing) {
                    target = &ents[i];
                    break;
                }
            }
        }
        check_expect(target && target->dead && target->radius == 0.0f
                         && (target->pose[ENTITY_DEAD].w[0] ? target->act == ENTITY_DEAD : !target->live),
                     "combat: a dead creature comes back alive with its block");
        dunstream_close();
        keep_clear();
    }
}

/*
 * Creatures awake: a rat's blow at the player worked by hand; in the
 * streamed hold a creature that sees the player wakes, comes, faces
 * them and hurts them; tgm keeps the player whole; tai stands it still.
 */
static void check_foes(void)
{
    Character pc;
    Foe f;
    Blow b;
    int lo;
    int hi;
    int i;

    if (!character_new(&pc, RACE_BRETON, CLASS_WARRIOR, 0, "Test")) {
        check_expect(0, "foes: no starting character");
        return;
    }
    combat_foe(&f, enemy_kind(ENEMY_RAT), 1, 0);
    combat_foe_blow(&f, &pc, &b);
    printf("foes: a rat at a Breton warrior (dodging %d, luck %d, agility %d): %d%% to hit\n", pc.skill[SKILL_DODGING],
           pc.attribute[ATTR_LUCK], pc.attribute[ATTR_AGILITY], b.chance);
    /*
     * By hand: its skill 35, the player's bare armour 100, luck (50 - 55) / 10
     * = 0, agility (80 - 62) / 10 = 1, the player's Dodging 15 / 4 = 3 off,
     * less 50: 83, or 86 when its critical strike (35 / 10) comes up.
     */
    check_expect(b.chance == 83 || b.chance == 86, "foes: a rat's chance at a Breton warrior is not 83 (86 critical)");
    lo = 1000;
    hi = -1;
    combat_seed(5);
    for (i = 0; i < 2000; i++) {
        combat_foe_blow(&f, &pc, &b);
        if (b.hit) {
            lo = b.damage < lo ? b.damage : lo;
            hi = b.damage > hi ? b.damage : hi;
        }
    }
    check_expect(lo == 1 && hi == 4, "foes: a rat's blow is not its 1..4");
    /* A class enemy at level 1 bare-handed: Hand-to-Hand 35, so 4..8. */
    combat_foe(&f, enemy_kind(ENEMY_FIRST_CLASS), 1, 0);
    lo = 1000;
    hi = -1;
    for (i = 0; i < 2000; i++) {
        combat_foe_blow(&f, &pc, &b);
        if (b.hit) {
            lo = b.damage < lo ? b.damage : lo;
            hi = b.damage > hi ? b.damage : hi;
        }
    }
    check_expect(lo == 4 && hi == 8, "foes: a class enemy's bare blow is not 4..8 at level 1");

    {
        Dungeon hold;
        Level level;
        Camera cam;
        float x, y, z, yaw, pitch;
        Player p;
        Input in;
        Entity *foe;
        float start;
        float near;
        float ex;
        float ez;
        int hurt;
        int s;

        keep_clear();
        combat_reset();
        foes_reset();
        foes_set_on(1);
        if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
            check_expect(0, "foes: the hold's stream did not open");
            return;
        }
        camera_set(&cam, x, y, z, yaw, pitch);
        dunstream_fill(&cam);
        player_init(&p, &pc);
        memset(&in, 0, sizeof(in));
        /* A walking creature, not passive, the player 4 m before it where it can see them. */
        foe = 0;
        for (s = 0; s < stream_slots() && !foe; s++) {
            Entity *ents;
            int first;
            int n;

            ents = stream_slot_entities(s, &first, &n);
            for (i = 0; ents && i < n && !foe; i++) {
                Entity *e;
                int kind;
                float eye[3];
                float peye[3];

                e = &ents[i];
                kind = dunstream_kind(e, 0);
                if (!e->live || e->dead || kind < 0 || dunstream_passive(e) || enemy_flies(kind)
                    || enemy_spectral(kind) || !e->pose[ENTITY_ATTACK].w[0]) {
                    continue;
                }
                player_place(&p, e->x + df_sin(e->yaw) * 160.0f, e->y + p.body.eye, e->z + df_cos(e->yaw) * 160.0f,
                             df_wrap_tau(e->yaw + DF_TAU * 0.5f), 0.0f);
                walk(&p, &level.scene, &in, 30);
                if (!p.body.grounded || !check_near(p.body.feet[1], e->y, 8.0f)) {
                    continue;
                }
                eye[0] = e->x;
                eye[1] = e->y + entity_height(e) * (2.0f / 3.0f);
                eye[2] = e->z;
                body_eye(&p.body, &peye[0], &peye[1], &peye[2]);
                if (combat_sight(&level.scene, eye, peye)) {
                    foe = e;
                }
            }
        }
        check_expect(foe != 0, "foes: no creature of the hold stands where it can see the player");
        if (!foe) {
            dunstream_close();
            return;
        }
        ex = p.body.feet[0] - foe->x;
        ez = p.body.feet[2] - foe->z;
        start = df_sqrt(ex * ex + ez * ez);
        near = start;
        hurt = 0;
        combat_seed(3);
        p.sheet.health = p.sheet.health_max;
        for (i = 0; i < 30 * 20 && !hurt; i++) {
            player_step(&p, &in, &level.scene, 1.0f / 30.0f, 1);
            if (foes_frame(&p, &level.scene, 1.0f / 30.0f)) {
                p.sheet.health = p.sheet.health_max;
            }
            ex = p.body.feet[0] - foe->x;
            ez = p.body.feet[2] - foe->z;
            near = df_sqrt(ex * ex + ez * ez) < near ? df_sqrt(ex * ex + ez * ez) : near;
            hurt = p.sheet.health < p.sheet.health_max;
        }
        printf("foes: %s woke %.0f units off, came to %.0f, struck the player after %.1f s (%d awake)\n",
               enemy_kind(dunstream_kind(foe, 0))->name, start, near, (float)i / 30.0f, foes_awake());
        check_expect(foes_awake() >= 1, "foes: a creature that sees the player does not wake");
        check_expect(near < start - 20.0f && near <= FOES_MELEE + 4.0f, "foes: an awake creature does not come to reach");
        check_expect(hurt, "foes: a creature in reach for 20 s never hurt the player");
        /* tgm: ten seconds more, nothing taken. */
        p.god = 1;
        p.sheet.health = p.sheet.health_max;
        for (i = 0; i < 30 * 10; i++) {
            player_step(&p, &in, &level.scene, 1.0f / 30.0f, 1);
            foes_frame(&p, &level.scene, 1.0f / 30.0f);
        }
        check_expect(p.sheet.health == p.sheet.health_max, "foes: tgm on, a creature hurt the player");
        p.god = 0;
        /* tai off: it stands where it is, for all the player walks away. */
        foes_set_on(0);
        ex = foe->x;
        ez = foe->z;
        in.move_f = -1;
        for (i = 0; i < 30 * 3; i++) {
            player_step(&p, &in, &level.scene, 1.0f / 30.0f, 1);
            foes_frame(&p, &level.scene, 1.0f / 30.0f);
        }
        check_expect(foe->x == ex && foe->z == ez && !foe->moving, "foes: tai off, a creature still moves");
        foes_set_on(1);
        foes_reset();
        dunstream_close();
        keep_clear();
    }
}

/*
 * Armour: what a piece is made of, where it goes, the two-handed rules,
 * and what it does to a creature's blow, all worked by hand.
 */
static void check_armor(void)
{
    Character pc;
    Inventory inv;
    Item it;
    Foe f;
    Blow b;
    int armor[BODY_PARTS];
    int count[BODY_PARTS];
    int archive;
    int record;
    int i;

    /* 12.5 kg is 50 quarters; leather halves them, 25, 6.25 kg. Its price and condition are the template's. */
    check_expect(item_make(&it, ITEM_CUIRASS, MATERIAL_LEATHER) && check_near(it.weight, 6.25f, 0.001f)
                     && it.value == 100 && it.condition_max == 4096,
                 "armor: a leather cuirass is not 6.25 kg, 100 gold, 4096");
    check_expect(item_make(&it, ITEM_CUIRASS, MATERIAL_CHAIN) && check_near(it.weight, 12.5f, 0.001f) && it.value == 200,
                 "armor: a chain cuirass is not 12.5 kg, 200 gold");
    check_expect(item_make(&it, ITEM_CUIRASS, MATERIAL_DAEDRIC) && it.value == 153600 && it.condition_max == 32768,
                 "armor: a daedric cuirass is not 153600 gold, 32768");
    check_expect(!item_make(&it, ITEM_LONGSWORD, MATERIAL_LEATHER) && !item_make(&it, ITEM_DAGGER, MATERIAL_CHAIN),
                 "armor: a sword was made of leather or chain");
    item_make(&it, ITEM_CUIRASS, MATERIAL_IRON);
    check_expect(it.variant == 1, "armor: a new iron cuirass does not show its first plate variant");

    /* A woman's Argonian armour is archive 245; a man's Breton 249 and the human shape's 2. */
    check_expect(item_picture(&it, RACE_ARGONIAN, 1, &archive, &record) && archive == 245 && record == 4,
                 "armor: an Argonian woman's iron cuirass is not TEXTURE.245, record 4");
    check_expect(item_picture(&it, RACE_BRETON, 0, &archive, &record) && archive == 251 && record == 4,
                 "armor: a Breton man's iron cuirass is not TEXTURE.251, record 4");

    /* Iron on the chest, 7 x 5 off; a tower shield 4 x 5 off the head, left arm, hands and legs. */
    inventory_clear(&inv);
    item_make(&it, ITEM_CUIRASS, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    item_make(&it, ITEM_TOWER_SHIELD, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    inventory_armor(&inv, armor);
    check_expect(armor[BODY_CHEST] == 65 && armor[BODY_HEAD] == 80 && armor[BODY_LEFT_ARM] == 80 && armor[BODY_HANDS] == 80
                     && armor[BODY_LEGS] == 80 && armor[BODY_RIGHT_ARM] == 100 && armor[BODY_FEET] == 100,
                 "armor: an iron cuirass and a tower shield do not give classic's values");

    /* Hands: a claymore takes the shield off; a shield takes the claymore out of the hand. */
    item_make(&it, 122, MATERIAL_IRON);
    i = inventory_add(&inv, &it);
    check_expect(inventory_wear(&inv, i) && inv.worn[EQUIP_LEFT_HAND] == -1 && inv.worn[EQUIP_RIGHT_HAND] == i,
                 "armor: a two-handed weapon left the shield on");
    check_expect(inventory_wear(&inv, 1) && inv.worn[EQUIP_LEFT_HAND] == 1 && inv.worn[EQUIP_RIGHT_HAND] == -1,
                 "armor: a shield left a two-handed weapon in hand");
    /* The worn places follow their items down the list. */
    inventory_remove(&inv, 0);
    check_expect(inv.worn[EQUIP_CHEST] == -1 && inv.worn[EQUIP_LEFT_HAND] == 0,
                 "armor: taking an item out left what is worn pointing at the wrong one");

    /* The same rat, 35 better armour everywhere: 83 less 35 is 48 (51 on its critical). */
    character_new(&pc, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    for (i = 0; i < BODY_PARTS; i++) {
        pc.armor[i] = 65;
        count[i] = 0;
    }
    combat_foe(&f, enemy_kind(ENEMY_RAT), 1, 0);
    combat_seed(11);
    for (i = 0; i < 20000; i++) {
        combat_foe_blow(&f, &pc, &b);
        check_expect(b.chance == 48 || b.chance == 51, "armor: a rat's chance through iron is not 48 (51 critical)");
        count[b.part]++;
    }
    /* Twenty parts of the body, by DFU's table: 2, 3, 3, 4, 4, 3, 1 of them; within a percent. */
    check_expect(count[BODY_HEAD] > 1800 && count[BODY_HEAD] < 2200 && count[BODY_CHEST] > 3800 && count[BODY_CHEST] < 4200
                     && count[BODY_FEET] > 800 && count[BODY_FEET] < 1200,
                 "armor: blows do not fall on the body's parts by classic's odds");
    printf("armor: iron and a tower shield, chest 65, head 80; a rat at 48%% through iron\n");
}

/* How often a mind moves: every frame near, less often far, and never two far ones on the same frame by accident. */
static void check_foes_due(void)
{
    int frame;
    int near_moves;
    int middle_moves;
    int far_moves;
    int far_on_frame_0;

    near_moves = 0;
    middle_moves = 0;
    far_moves = 0;
    for (frame = 0; frame < 8; frame++) {
        near_moves += foes_due(100.0f, frame, 3);
        middle_moves += foes_due(FOES_NEAR + 1.0f, frame, 3);
        far_moves += foes_due(FOES_FAR + 1.0f, frame, 3);
    }
    check_expect(near_moves == 8 && middle_moves == 4 && far_moves == 2,
                 "foes: a mind does not move every frame near, every 2nd past FOES_NEAR, every 4th past FOES_FAR");
    far_on_frame_0 = foes_due(FOES_FAR + 1.0f, 0, 0) + foes_due(FOES_FAR + 1.0f, 0, 1)
                     + foes_due(FOES_FAR + 1.0f, 0, 2) + foes_due(FOES_FAR + 1.0f, 0, 3);
    check_expect(far_on_frame_0 == 1, "foes: four far minds are not spread over four frames");
}

/* The default character, what the rolls do and what a body holds, by hand and in the hold. */
static void check_loot(void)
{
    Inventory one;
    Inventory two;
    const LootChance *t;
    int i;
    int gold;

    check_expect(CHARACTER_START_RACE == RACE_ARGONIAN && CHARACTER_START_FEMALE == 1,
                 "loot: the game does not start as an Argonian woman");

    /* Level 1 takes 36 off the roll: 0 stays iron, and 255 is 219: past iron 155, steel 27, silver 17, into elven. */
    check_expect(loot_metal(1, 0) == MATERIAL_IRON && loot_metal(1, 255) == MATERIAL_ELVEN,
                 "loot: level 1's materials are not classic's");
    /* Level 30 adds 40 to the roll: 255 clamps to 256, every share taken, daedric. */
    check_expect(loot_metal(30, 255) == MATERIAL_DAEDRIC, "loot: level 30's best material is not daedric");

    /* Table T: gold 20..80 a level, and always at least one weapon and one piece of armour. */
    t = loot_chance('T');
    check_expect(t && t->weapon == 100 && t->armor == 100 && loot_table()->count == 21, "loot: table T is not classic's");
    if (!t) {
        return;
    }
    for (i = 1; i < 200; i++) {
        loot_roll(&one, t, 3, (df_u32)i);
        loot_roll(&two, t, 3, (df_u32)i);
        check_expect(one.count == two.count && one.count >= 3 && one.count <= LOOT_MAX,
                     "loot: a seed did not roll the same things, or too few");
        check_expect(one.item[0].template_id == ITEM_GOLD, "loot: gold does not come first");
        gold = one.item[0].count;
        check_expect(gold >= 60 && gold <= 240, "loot: table T's gold at level 3 is not 60..240");
    }
    /* Table I has no gold, weapons or armour: nothing rolls. */
    loot_roll(&one, loot_chance('I'), 5, 1u);
    check_expect(one.count == 0, "loot: table I rolled something");

    /* A body in the hold: searched by the view, the same each time, what is taken stays gone. */
    {
        Dungeon hold;
        Level level;
        Camera cam;
        float x, y, z, yaw, pitch;
        Entity *body;
        Keep *row;
        df_u8 origin[INVENTORY_MAX];
        int s;

        keep_clear();
        combat_reset();
        if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
            check_expect(0, "loot: the hold's stream did not open");
            return;
        }
        camera_set(&cam, x, y, z, yaw, pitch);
        dunstream_fill(&cam);
        loot_new_game(77u);
        body = 0;
        for (s = 0; s < stream_slots() && !body; s++) {
            Entity *ents;
            int first;
            int n;

            ents = stream_slot_entities(s, &first, &n);
            for (i = 0; ents && i < n && !body; i++) {
                const EnemyKind *kind;

                kind = ents[i].live ? enemy_kind(dunstream_kind(&ents[i], 0)) : 0;
                if (kind && loot_chance(kind->loot[0]) && ents[i].pose[ENTITY_DEAD].w[0]) {
                    body = &ents[i];
                }
            }
        }
        check_expect(body != 0, "loot: the hold has no creature with a loot table");
        if (body) {
            float eye_y;
            int found;
            int things;

            /* Killed by hand, at level 20: gold is sure wherever the table has any. */
            body->dead = 1;
            row = keep_get(body->key, KEEP_ENTITY);
            row->game[COMBAT_KEEP_LEVEL] = 20;
            /* Standing over it, looking down at it. */
            eye_y = body->y + 60.0f;
            camera_set(&cam, body->x, eye_y, body->z - 40.0f, 0.0f, 0.0f);
            /* Pitch straight at the body's middle is found; looking at the sky, it is not. */
            cam.pitch = -0.9f;
            check_expect(loot_find(&level.scene, &cam) == body, "loot: looking down at a body did not find it");
            cam.pitch = 1.2f;
            check_expect(loot_find(&level.scene, &cam) != body, "loot: looking away still found the body");
            found = loot_open(body, 20, &one, origin);
            loot_open(body, 20, &two, origin);
            check_expect(found == two.count, "loot: a body changed what it holds between two looks");
            /* Take the first thing, if it has one: it must stay gone, the rest the same. */
            things = found;
            if (things > 0) {
                loot_taken(body, origin[0]);
                loot_open(body, 20, &two, origin);
                check_expect(two.count == things - 1 && (things == 1 || two.item[0].template_id == one.item[1].template_id),
                             "loot: a thing taken off a body came back");
            }
            printf("loot: a body in the hold (table %s) held %d things at level 20\n",
                   enemy_kind(dunstream_kind(body, 0))->loot, found);
        }
        dunstream_close();
        keep_clear();
    }
    printf("loot: seeds repeat; materials by level; bodies searched by the view\n");
}

/*
 * Every metal side by side, for a person to look at against classic's:
 * build/dye0.ppm, a row a metal, of a cuirass, a helm, greaves and a longsword.
 * Also says how much of each picture is in the ramp the tables swap.
 */
static void write_dye_sheet(void)
{
    static const int records[4][2] = { { 245, 4 }, { 245, 29 }, { 245, 12 }, { 234, 12 } };
    enum { CELL_W = 100, CELL_H = 80, SHEET_W = 4 * CELL_W, SHEET_H = CIF_METALS * CELL_H };
    static df_u8 sheet[SHEET_W * SHEET_H];
    df_u8 rgb[768];
    FILE *f;
    int metal;
    int kind;
    int i;

    if (!img_art_palette(rgb)) {
        return;
    }
    memset(sheet, 0, sizeof(sheet));
    for (kind = 0; kind < 4; kind++) {
        const Image *picture;
        int in_ramp;
        int drawn;

        picture = texture_load(records[kind][0], records[kind][1]);
        if (!picture) {
            check_expect(0, "dye: an armour or weapon picture did not read");
            continue;
        }
        in_ramp = 0;
        drawn = 0;
        for (i = 0; i < picture->width * picture->height; i++) {
            int index;

            index = picture->index[i];
            if (index && index != DOLL_MASK) {
                drawn++;
                in_ramp += index >= CIF_RAMP && index < CIF_RAMP + 16;
            }
        }
        printf("dye: TEXTURE.%d record %d, %d of %d pixels in the metal ramp\n", records[kind][0], records[kind][1], in_ramp, drawn);
        for (metal = 0; metal < CIF_METALS; metal++) {
            int row;

            for (row = 0; row < picture->height && row < CELL_H; row++) {
                int col;

                for (col = 0; col < picture->width && col < CELL_W; col++) {
                    int index;

                    index = picture->index[row * picture->width + col];
                    if (index && index != DOLL_MASK) {
                        sheet[(metal * CELL_H + row) * SHEET_W + kind * CELL_W + col] = (df_u8)cif_metal(index, metal);
                    }
                }
            }
        }
    }
    f = fopen("build/dye0.ppm", "wb");
    if (!f) {
        check_expect(0, "dye: the sheet did not write");
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", SHEET_W, SHEET_H);
    for (i = 0; i < SHEET_W * SHEET_H; i++) {
        fwrite(rgb + sheet[i] * 3, 1, 3, f);
    }
    fclose(f);
}

/* The colours by classic's tables: steel as drawn, the rest swapped, nothing else touched. */
static void check_dye(void)
{
    check_expect(cif_metal(0x75, MATERIAL_STEEL) == 0x75, "dye: steel is not the pictures as drawn");
    check_expect(cif_metal(0x70, MATERIAL_IRON) == 0x77 && cif_metal(0x7F, MATERIAL_IRON) == 0x5F,
                 "dye: iron's ends are not classic's");
    check_expect(cif_metal(0x7D, MATERIAL_DWARVEN) == 0x9D, "dye: dwarven is not classic's");
    check_expect(cif_metal(0x70, MATERIAL_DAEDRIC) == 0xEF && cif_metal(0x7F, MATERIAL_DAEDRIC) == 0xFE,
                 "dye: daedric's ends are not classic's");
    check_expect(cif_metal(0x6F, MATERIAL_DAEDRIC) == 0x6F && cif_metal(0x80, MATERIAL_DAEDRIC) == 0x80,
                 "dye: a colour outside the ramp was swapped");
    {
        Item it;

        item_make(&it, ITEM_CUIRASS, MATERIAL_LEATHER);
        check_expect(item_metal(&it) == -1, "dye: leather was given a metal");
        item_make(&it, ITEM_CUIRASS, MATERIAL_CHAIN);
        check_expect(item_metal(&it) == -1, "dye: chain was given a metal");
        item_make(&it, ITEM_CUIRASS, MATERIAL_EBONY);
        check_expect(item_metal(&it) == MATERIAL_EBONY, "dye: ebony armour is not ebony");
    }
    write_dye_sheet();
}

/* The first entity of the stream a test asks for: a heap of treasure when treasure is 1, else a spare thing that is free. */
static Entity *find_in_stream(int treasure)
{
    int s;

    for (s = 0; s < stream_slots(); s++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(s, &first, &n);
        for (i = 0; ents && i < n; i++) {
            if (treasure ? (ents[i].live && dunstream_treasure(&ents[i])) : (!ents[i].live && dunstream_spare(&ents[i]))) {
                return &ents[i];
            }
        }
    }
    return 0;
}

/* The entity with this key, as it stands after its block came back. */
static Entity *entity_by_key(KeepKey key)
{
    int s;

    for (s = 0; s < stream_slots(); s++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(s, &first, &n);
        for (i = 0; ents && i < n; i++) {
            if (ents[i].key.place == key.place && ents[i].key.thing == key.thing) {
                return &ents[i];
            }
        }
    }
    return 0;
}

/* 1 when the view finds e, looking down at it from 40 units off on one of its four sides. */
static int found_from_a_side(const Scene *scene, const Entity *e)
{
    int side;

    for (side = 0; side < 4; side++) {
        Camera cam;
        float yaw;

        yaw = (float)side * (DF_TAU / 4.0f);
        camera_set(&cam, e->x - df_sin(yaw) * 40.0f, e->y + 60.0f, e->z - df_cos(yaw) * 40.0f, yaw, -0.9f);
        if (loot_find(scene, &cam) == e) {
            return 1;
        }
    }
    return 0;
}

/* Takes the camera eight blocks off, so the hold drops, and brings it back, so its blocks fill again. */
static void away_and_back(Camera *cam, float x, float y, float z, float yaw, float pitch)
{
    int i;

    camera_set(cam, x + 8.0f * 2048.0f, y, z, yaw, pitch);
    for (i = 0; i < 200; i++) {
        dunstream_update(cam, 50.0f);
    }
    camera_set(cam, x, y, z, yaw, pitch);
    dunstream_fill(cam);
}

/* Heaps of treasure and dropped piles standing in the streamed hold, through what the screen calls. */
static void check_piles(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    Inventory on_it;
    df_u8 origin[INVENTORY_MAX];
    Entity *pile;
    Entity *heap;
    KeepKey key;
    Item it;
    Inventory things;
    float feet[3];
    int heaps;
    int s;

    keep_clear();
    combat_reset();
    pile_new_game();
    if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "piles: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    loot_new_game(5u);

    check_expect(dunstream_dungeon_type() == hold.dungeon_type && dungeon_loot(hold.dungeon_type) != 0,
                 "piles: the hold's type names no loot table");
    /* DFU's keys: a human stronghold, the hold, fills from table N; a dragon's den from S. */
    check_expect(dungeon_loot(2) == loot_chance('N') && dungeon_loot(14) == loot_chance('S'),
                 "piles: dungeon types do not name classic's loot tables");

    /* Treasure: every heap on its own key, and none of it taken for a creature. */
    heaps = 0;
    for (s = 0; s < level.scene.nentities; s++) {
        const Entity *e;

        e = &level.scene.entities[s];
        if (e->live && dunstream_treasure(e)) {
            heaps++;
            check_expect(dunstream_kind(e, 0) < 0, "piles: a heap of treasure was taken for a creature");
            check_expect(e->pose[ENTITY_WALK].w[0] > 0 && e->radius == 0.0f, "piles: a heap has no picture, or stops the player");
        }
    }
    printf("piles: the hold has %d heaps of treasure\n", heaps);
    check_expect(heaps > 0, "piles: the hold has no treasure");

    heap = find_in_stream(1);
    if (heap) {
        int first_look;

        /* The same heap holds the same things at each look, whatever level looks second. */
        first_look = loot_open(heap, 20, &on_it, origin);
        check_expect(loot_open(heap, 1, &on_it, origin) == first_look, "piles: a heap changed between two looks");
        /* Looking down at it from a little way off, from whichever side no wall stands, the view finds it. */
        check_expect(found_from_a_side(&level.scene, heap), "piles: looking down at a heap from every side found nothing");
        /* Emptied, it goes, and it stays gone when its block comes back. */
        key = heap->key;
        inventory_clear(&on_it);
        loot_closed(heap, &on_it);
        check_expect(heap->dead && !heap->live, "piles: a heap emptied still stands");
        away_and_back(&cam, x, y, z, yaw, pitch);
        heap = entity_by_key(key);
        check_expect(heap && heap->dead && !heap->live, "piles: a heap emptied came back with its block");
    }

    /* A dropped pile: put down where the player stands, drawn, holding what was dropped. */
    pile = find_in_stream(0);
    check_expect(pile != 0, "piles: the hold's blocks have no spare things");
    inventory_clear(&things);
    item_make(&it, ITEM_LONGSWORD, MATERIAL_STEEL);
    inventory_add(&things, &it);
    /* Where a player really stands: the start marker is not on the floor, so let a body fall to it. */
    {
        Character sheet;
        Player p;
        Input in;

        character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
        player_init(&p, &sheet);
        player_place(&p, x, y, z, yaw, pitch);
        memset(&in, 0, sizeof(in));
        walk(&p, &level.scene, &in, 60);
        check_expect(p.body.grounded, "piles: the player did not come to stand on the hold's floor");
        feet[0] = p.body.feet[0];
        feet[1] = p.body.feet[1];
        feet[2] = p.body.feet[2];
    }
    check_expect(pile_drop(&things, feet), "piles: dropping found nowhere to put it");
    pile = 0;
    for (s = 0; s < level.scene.nentities; s++) {
        Entity *e;

        e = (Entity *)&level.scene.entities[s];
        if (e->live && dunstream_spare(e)) {
            pile = e;
        }
    }
    check_expect(pile && check_near(pile->x, feet[0], 0.5f) && check_near(pile->z, feet[2], 0.5f),
                 "piles: the pile is not at the player's feet");
    if (pile) {
        check_expect(found_from_a_side(&level.scene, pile), "piles: looking down at the pile from every side found nothing");
        check_expect(loot_takes_things(pile) && strcmp(loot_label(pile), "Pile") == 0, "piles: a pile does not take things");
        /* Dropping again on the same spot joins it. */
        check_expect(pile_drop(&things, feet), "piles: a second drop found nowhere to put it");
        check_expect(loot_open(pile, 1, &on_it, origin) == 2, "piles: a second drop did not join the pile");

        /* It stays, where it was and with what is in it, when its block goes and comes back. */
        key = pile->key;
        away_and_back(&cam, x, y, z, yaw, pitch);
        pile = entity_by_key(key);
        check_expect(pile && pile->live && check_near(pile->x, feet[0], 0.5f) && check_near(pile->z, feet[2], 0.5f),
                     "piles: a pile did not come back with its block, where it was");
        if (pile) {
            check_expect(loot_open(pile, 1, &on_it, origin) == 2 && on_it.item[0].template_id == ITEM_LONGSWORD,
                         "piles: a pile came back without its things");
            /* Emptied, it goes, its spare thing free again, and it does not come back. */
            inventory_clear(&on_it);
            loot_closed(pile, &on_it);
            check_expect(!pile->live, "piles: a pile emptied still stands");
            away_and_back(&cam, x, y, z, yaw, pitch);
            pile = entity_by_key(key);
            check_expect(pile && !pile->live, "piles: a pile emptied came back with its block");
        }
    }
    /* The pool's limit: one more than it holds is refused. */
    check_expect(pile_room(0, PILE_ITEMS) && !pile_room(0, PILE_ITEMS + 1), "piles: the pool's room is not PILE_ITEMS");
    dunstream_close();
    keep_clear();
    printf("piles: heaps and dropped piles stand, are searched, go when emptied, and keep across their block\n");
}

/* A composed 320 x 200 picture under ART_PAL.COL to a file, for a person to look at against classic's. */
static void write_picture(const df_u8 *pixels, const char *path)
{
    df_u8 rgb[768];
    FILE *f;
    int i;

    f = img_art_palette(rgb) ? fopen(path, "wb") : 0;
    if (!f) {
        check_expect(0, "screens: a picture did not write");
        return;
    }
    fprintf(f, "P6\n320 200\n255\n");
    for (i = 0; i < 320 * 200; i++) {
        fwrite(rgb + pixels[i] * 3, 1, 3, f);
    }
    fclose(f);
}

/* P9-b: classic's real art, exact button seams, compass wrap and the 256 KB
 * UI room. This does not assert a copy of overlay_draw's implementation:
 * it checks the file shapes and input boundaries on which that draw relies,
 * and writes two native-size panels for inspection against the reference.
 */
static void check_hudbar(void)
{
    static const PicOps keyed = { wc_bytes, wc_upload, wc_release, wc_begin, wc_palette, wc_quad, wc_end, 1 };
    static const PicOps wide = { wc_bytes, wc_upload, wc_release, wc_begin, wc_palette, wc_quad, wc_end, 0 };
    static const char *const art[] = {
        "MAIN00I0.IMG", "MAIN01I0.IMG", "MAIN03I0.IMG", "MAIN04I0.IMG", "MAIN05I0.IMG",
        "COMPASS.IMG", "COMPBOX.IMG", "ARROW.RAW"
    };
    ImgAnimation compass;
    ImgAnimation bad;
    ImgPicture images[8];
    ImgPicture frame;
    CifSet colors;
    CifSet heads;
    const CareerRace *race;
    const CifFrame *color;
    const CifFrame *face;
    Composer c;
    df_u8 rgb[768];
    df_u8 tiny[18];
    df_u8 cursor_sheet[CONTROL_CURSOR_W * CONTROL_CURSOR_H];
    static const df_u8 label_color = 222;
    int ids[16];
    int i;
    int pass;
    int n;
    int pics;
    int tiles;
    df_u32 used;
    df_u32 room;

    check_expect(hudbar_hit(7, 162) == HUDBAR_CHARACTER && hudbar_hit(39, 191) == HUDBAR_CHARACTER
                 && hudbar_hit(40, 191) == HUDBAR_NONE, "hud: the portrait's clickable edges");
    check_expect(hudbar_hit(131, 154) == HUDBAR_MODE && hudbar_hit(177, 176) == HUDBAR_MODE
                 && hudbar_hit(131, 177) == HUDBAR_TRANSPORT && hudbar_hit(178, 176) == HUDBAR_INVENTORY,
                 "hud: mode and neighbor buttons overlap or miss their seam");
    check_expect(hudbar_hit(71, 199) == HUDBAR_OPTIONS && hudbar_hit(83, 154) == HUDBAR_NONE
                 && hudbar_hit(319, 199) == HUDBAR_NONE && hudbar_hit(319, 153) == HUDBAR_NONE
                 && hudbar_contains(319, 199) && !hudbar_contains(320, 199) && !hudbar_contains(-1, 154),
                 "hud: blank bar pixels must consume clicks without issuing a command");
    check_expect(hudbar_hit(226, 155) == HUDBAR_SHEATH && hudbar_hit(226, 178) == HUDBAR_REST
                 && hudbar_hit(179, 178) == HUDBAR_MAP && hudbar_hit(276, 157) == HUDBAR_STATUS,
                 "hud: the lower row or compass buttons");
    check_expect(hudbar_heading(0.0f) == 0 && hudbar_heading(DF_TAU * 0.25f) == 8
                 && hudbar_heading(DF_TAU * 0.5f) == 16 && hudbar_heading(-DF_TAU * 0.25f) == 24
                 && hudbar_heading(DF_TAU) == 0, "hud: compass directions or wrap");
    check_expect(hudbar_mode_row(HUDBAR_GRAB) == 2 && hudbar_mode_row(HUDBAR_INFO) == 3
                 && hudbar_mode_row(HUDBAR_STEAL) == 0 && hudbar_mode_row(HUDBAR_TALK) == 1,
                 "hud: MAIN01I0's four mode rows");

    /* A wholly absent final byte is not a valid animation. Decode must leave
     * the old result intact, so a bad read cannot publish half an asset. */
    memset(tiny, 0, sizeof(tiny));
    tiny[4] = tiny[6] = tiny[8] = 2;
    bad.frames = 73;
    check_expect(!img_bss_decode(tiny, 17, &bad) && bad.frames == 73 && img_bss_decode(tiny, 18, &bad),
                 "hud: BSS accepted a missing final byte or changed a refused result");
    tiny[8] = 255;
    tiny[9] = 127;
    check_expect(!img_bss_decode(tiny, 18, &bad), "hud: BSS accepted frames past the file");

    if (!img_load_bss("CMPA00I0.BSS", &compass) || !img_art_palette(rgb)) {
        check_expect(0, "hud: the real compass or palette did not read");
        return;
    }
    check_expect(compass.frames == 32 && compass.width == 48 && compass.height == 40
                 && img_bss_frame(&compass, 31, &frame)
                 && frame.pixels == compass.pixels + 31 * 48 * 40
                 && !img_bss_frame(&compass, -1, &frame) && !img_bss_frame(&compass, 32, &frame),
                 "hud: CMPA00I0's layout or frame bounds");
    for (i = 0; i < 8; i++) {
        if (!(i == 7 ? img_load_raw(art[i], 10, 10, &images[i]) : img_load(art[i], &images[i]))) {
            check_expect(0, "hud: a panel picture did not read");
            return;
        }
    }
    check_expect(images[0].width == 320 && images[0].height == HUDBAR_HEIGHT
                 && images[1].width == 47 && images[1].height == 92, "hud: the panel or mode sheet's shape");
    race = career_race(RACE_BRETON);
    if (!race || !cif_open("MCOL00I0.CIF", &colors)) {
        check_expect(0, "hud: the blue portrait backing did not read");
        return;
    }
    if (!cif_open(race->heads_male, &heads)) {
        check_expect(0, "hud: the character's head did not read");
        cif_close(&colors);
        return;
    }
    color = cif_frame(&colors, 0);
    face = cif_frame(&heads, 0);
    check_expect(color && color->w == 66 && color->h == 36 && face, "hud: the portrait's blue backing or selected face");
    if (!color || !face) {
        cif_close(&heads);
        cif_close(&colors);
        return;
    }

    /* Native-size previews: Grab/north/full vitals, then Steal/east/half.
     * The target scales these same pieces; no generated art enters ARENA2. */
    if (composer_open(&c)) {
        canvas_use(&c);
        for (pass = 0; pass < 2; pass++) {
            const Rect *r;
            int x, y;

            memset(c.screen, 0, CANVAS_BYTES);
            canvas_copy_part(images[0].pixels, 320, 0, 0, 320, 46, 0, HUDBAR_TOP);
            canvas_copy_part(color->pixels, color->w, 0, 0, 66, 36, 5, 159);
            /* The HUD intentionally scales the selected face into classic's
             * 33 x 30 aperture, regardless of the source head's dimensions. */
            for (y = 0; y < 30; y++) {
                for (x = 0; x < 33; x++) {
                    int index;

                    index = face->pixels[(y * face->h / 30) * face->w + x * face->w / 33];
                    if (index) {
                        c.screen[(162 + y) * 320 + 7 + x] = (df_u8)index;
                    }
                }
            }
            r = hudbar_rect(HUDBAR_MODE);
            canvas_copy_part(images[1].pixels, 47, 0, hudbar_mode_row(pass ? HUDBAR_STEAL : HUDBAR_GRAB) * 23,
                             47, 23, r->x, r->y);
            img_bss_frame(&compass, pass ? 8 : 0, &frame);
            r = hudbar_rect(HUDBAR_STATUS);
            for (y = 0; y < r->h; y++) {
                for (x = 0; x < r->w; x++) {
                    int index;

                    index = frame.pixels[(y * frame.height / r->h) * frame.width + x * frame.width / r->w];
                    if (index) {
                        c.screen[(r->y + y) * 320 + r->x + x] = (df_u8)index;
                    }
                }
            }
            for (i = 2; i < 5; i++) {
                int rows;

                rows = pass ? images[i].height / 2 : images[i].height;
                canvas_copy_part(images[i].pixels, images[i].width, 0, images[i].height - rows,
                                 images[i].width, rows, images[i].x, images[i].y + images[i].height - rows);
            }
            write_picture(c.screen, pass ? "build/hud1.ppm" : "build/hud0.ppm");
        }
        composer_close(&c);
    } else {
        check_expect(0, "hud: the preview composer did not open");
    }

    /* Keep every persistent HUD piece, including the old overlay's strip,
     * on a fake 256 KB room in both card formats. Reserve the existing
     * worst swing, idle weapon and controls screen on top, simultaneously. */
    control_cursor_sheet(cursor_sheet);
    for (pass = 0; pass < 2; pass++) {
        pic_begin(0, 256u * 1024u, pass ? &wide : &keyed);
        n = 0;
        for (i = 0; i < 8; i++) {
            ids[n++] = pic_load(images[i].pixels, images[i].width, images[i].height, rgb, i >= 6);
        }
        ids[n++] = pic_load(color->pixels, color->w, color->h, rgb, 0);
        ids[n++] = pic_load(face->pixels, face->w, face->h, rgb, 1);
        ids[n++] = pic_load(frame.pixels, frame.width, frame.height, rgb, 1);
        ids[n++] = pic_load(cursor_sheet, CONTROL_CURSOR_W, CONTROL_CURSOR_H, rgb, 1);
        ids[n++] = pic_load(&label_color, 1, 1, rgb, 0);
        pic_stats(&pics, &tiles, &used, &room);
        check_expect(used + 66560u + (pass ? 96u + 26u : 48u + 13u) * 1024u <= room,
                     "hud: HUD, controls and the largest weapon frame exceed the 256 KB UI room");
        printf("hud: %s card, %u HUD bytes, %u bytes left for screens and weapon\n",
               pass ? "1555" : "keyed", (unsigned)used, (unsigned)(room - used));
        for (i = 0; i < n; i++) {
            check_expect(ids[i] != 0, "hud: an asset did not acquire a picture slot");
            pic_free(ids[i]);
        }
        pic_end();
    }
    cif_close(&heads);
    cif_close(&colors);
    {
        int x;
        int y;
        int w;
        int h;

        hudbar_full_vital(0, &x, &y, &w, &h);
        check_expect(x == 10 && y == 158 && w == 4 && h == 32, "hud: health is not at the left of a full screen");
        hudbar_full_vital(1, &x, &y, &w, &h);
        check_expect(x == 18 && y == 158, "hud: fatigue is not beside health");
        hudbar_full_vital(2, &x, &y, &w, &h);
        check_expect(x == 26 && y == 158, "hud: magicka is not beside fatigue");
    }
}

/*
 * The inventory screen, driven by clicks at classic's own coordinates as
 * the player would: the cell of a list's row, a button, a point on the doll.
 */
#define BAG_LOCAL_CELL_X 190
#define BAG_REMOTE_CELL_X 288
#define BAG_ROW_Y 60
#define BAG_BUTTON_X 240
#define BAG_INFO_Y 42
#define BAG_EQUIP_Y 64
#define BAG_REMOVE_Y 86
#define BAG_USE_Y 108
#define BAG_EXIT_X 240
#define BAG_EXIT_Y 190
#define BAG_DOLL_HELM_X 103
#define BAG_DOLL_HELM_Y 30
static void check_bag(void)
{
    Player p;
    Character sheet;
    Item it;
    const df_u8 *grey;
    const df_u8 *gold;
    ImgPicture pic;
    int cached_before;
    int i;
    int differs;
    int x;
    int y;

    if (!character_new(&sheet, CHARACTER_START_RACE, CHARACTER_START_CLASS, CHARACTER_START_FEMALE, "Test")) {
        check_expect(0, "bag: no starting character");
        return;
    }
    mem_zero(&p, sizeof(p));
    player_init(&p, &sheet);
    item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON);
    inventory_equip(&p.pack, inventory_add(&p.pack, &it));
    item_make(&it, ITEM_CUIRASS, MATERIAL_STEEL);
    inventory_add(&p.pack, &it);
    item_make(&it, ITEM_HELM, MATERIAL_ELVEN);
    inventory_add(&p.pack, &it);
    item_make(&it, ITEM_DAGGER, MATERIAL_DAEDRIC);
    inventory_add(&p.pack, &it);
    p.pack.gold = 123;

    cached_before = texture_cached(245);
    check_expect(bag_open(&p, 0), "bag: the screen did not open");
    bag_compose();

    /* The chosen tab and button are cut from the gold picture, all else is the grey one's. */
    grey = img_load("INVE00I0.IMG", &pic) ? pic.pixels : 0;
    gold = img_load("INVE01I0.IMG", &pic) ? pic.pixels : 0;
    check_expect(grey && gold, "bag: its pictures did not read");
    if (grey && gold) {
        const df_u8 *screen;

        screen = bag_pixels();
        /* The Equip button's corner, and the Wagon button's, which is not chosen. */
        check_expect(screen[58 * 320 + 226] == gold[58 * 320 + 226] && screen[14 * 320 + 226] == grey[14 * 320 + 226],
                     "bag: the chosen button is not gold and the others grey");
        /* The Argonian woman stands on the doll: it is not only the background. */
        differs = 0;
        for (y = 50; y < 150; y++) {
            for (x = 80; x < 130; x++) {
                differs += screen[y * 320 + x] != grey[y * 320 + x];
            }
        }
        check_expect(differs > 2000, "bag: the doll does not show the character");
    }

    /* Equip mode's default: a click on the pack's first row wears the cuirass. The chest's armour follows. */
    check_expect(bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y) == BAG_STAY && inventory_worn_at(&p.pack, 1) == EQUIP_CHEST,
                 "bag: a click in equip mode did not wear the cuirass");
    check_expect(p.sheet.armor[BODY_CHEST] == 100 - 9 * 5, "bag: wearing steel did not change the chest's armour");
    /* The helm next, now first in the list: worn things leave it. */
    bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    check_expect(inventory_worn_at(&p.pack, 2) == EQUIP_HEAD && p.sheet.armor[BODY_HEAD] == 100 - 11 * 5,
                 "bag: the helm did not go on");
    bag_compose();
    write_picture(bag_pixels(), "build/bag0.ppm");

    /* A click on the helm, on the doll over the head, takes it off: found by the doll's record of who drew each pixel. */
    bag_click(BAG_DOLL_HELM_X, BAG_DOLL_HELM_Y);
    check_expect(p.pack.worn[EQUIP_HEAD] == -1 && p.sheet.armor[BODY_HEAD] == 100,
                 "bag: a click on the worn helm did not take it off");
    bag_compose();

    /* Remove mode: a thing goes to the dropped list, and any click on it there comes back. */
    bag_click(BAG_BUTTON_X, BAG_REMOVE_Y);
    i = p.pack.count;
    bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    check_expect(p.pack.count == i - 1, "bag: remove did not drop the thing");
    bag_compose();
    bag_click(BAG_REMOTE_CELL_X, BAG_ROW_Y);
    check_expect(p.pack.count == i, "bag: a dropped thing did not come back");
    /* Too much to carry: a click takes nothing and says why. */
    bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    p.sheet.attribute[ATTR_STRENGTH] = 1;
    bag_compose();
    bag_click(BAG_REMOTE_CELL_X, BAG_ROW_Y);
    check_expect(p.pack.count == i - 1 && bag_note() != 0, "bag: more than the player can carry was taken");
    p.sheet.attribute[ATTR_STRENGTH] = 65;
    bag_click(BAG_REMOTE_CELL_X, BAG_ROW_Y);

    /* The Gold button: a sum typed goes to the right list, and comes back to the purse. */
    {
        df_u32 purse;

        purse = p.pack.gold;
        bag_click(BAG_BUTTON_X, 130);
        check_expect(bag_asking(), "bag: the Gold button did not ask");
        bag_key('2');
        bag_key('0');
        bag_key(0x0D);
        check_expect(!bag_asking() && p.pack.gold == purse - 20, "bag: dropping gold did not take it from the purse");
        bag_compose();
        bag_click(BAG_REMOTE_CELL_X, BAG_ROW_Y);
        check_expect(p.pack.gold == purse, "bag: dropped gold did not come back");
        /* More than there is: nothing goes. */
        bag_click(BAG_BUTTON_X, 130);
        bag_key('9');
        bag_key('9');
        bag_key('9');
        bag_key(0x0D);
        check_expect(p.pack.gold == purse && bag_note() != 0, "bag: more gold than there is was dropped");
        bag_click(BAG_BUTTON_X, BAG_EQUIP_Y);
    }

    /* Info mode's box: it shows, and the next click only puts it away. */
    bag_click(BAG_BUTTON_X, BAG_INFO_Y);
    i = p.pack.count;
    bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    bag_compose();
    write_picture(bag_pixels(), "build/bag1.ppm");
    bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    check_expect(p.pack.count == i, "bag: the info box's click did something else");

    /* A frame with no change composes nothing. */
    bag_compose();
    check_expect(!bag_changed(), "bag: a screen unchanged asked to compose");
    bag_point(0, 199);
    bag_changed();
    bag_point(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    check_expect(bag_changed(), "bag: the pointer over an item did not change the panel");
    bag_point(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    check_expect(!bag_changed(), "bag: a pointer held still changed the screen");

    check_expect(bag_click(BAG_EXIT_X, BAG_EXIT_Y) == BAG_LEAVE, "bag: Exit did not leave");
    bag_close();
    /* Nothing it read stays cached pointing into its freed heap. */
    check_expect(texture_cached(245) == cached_before, "bag: a file stayed cached after its heap went");
    printf("bag: an Argonian woman in steel and elven; wear, take off, drop, take back, info; build/bag0.ppm, bag1.ppm\n");
}

/* Skills rising and levels coming, worked by hand. */
static void check_levelling(void)
{
    Character c;
    int start;
    int i;

    /* A value of 30, a skill 5 hard, a class 1.0, level 1: 30 x 5 x 1.04 x 0.4 = 62.4, down, plus 1. */
    check_expect(rules_skill_uses_needed(30, 5, 1.0f, 1) == 63, "levels: a skill's uses needed are not classic's");
    /* 15 points past the start and 2 to begin with: (gain + 28) / 15. */
    check_expect(rules_level_from_sums(100, 100) == 1 && rules_level_from_sums(100, 102) == 2 && rules_level_from_sums(100, 117) == 3,
                 "levels: the level from the sums is not classic's");
    /* A warrior's 20 hit points, half and a roll of 3, endurance 60 adding 1. */
    check_expect(rules_level_health(20, 3, 60) == 14 && rules_level_health(2, 0, 0) == 1, "levels: a level's health is not classic's");
    check_expect(rules_reflex_uses(2) == 1.0f && rules_reflex_uses(0) == 1.25f && rules_reflex_uses(4) == 0.75f,
                 "levels: reflexes do not move uses an eighth a step");
    check_expect(check_near(rules_class_advance(0x10A00u), 1.04f, 0.01f), "levels: a Mage's advancement is not 1.04");

    check_expect(character_new(&c, RACE_ARGONIAN, CLASS_WARRIOR, 1, "Test"), "levels: no character");
    start = character_level_sum(&c);
    check_expect(start == c.level_sum_start && !c.level_due, "levels: a new character's count does not start here");
    /* Three primary at 30, two of three major at 20, the highest minor at 15. */
    check_expect(start == 30 * 3 + 20 * 2 + 15, "levels: the level sum is not three primary, two major and a minor");

    /* Uses count, but nothing moves until six game hours have passed. */
    character_tally(&c, 29, 100000);
    check_expect(!character_raise_skills(&c, 359.0f) && c.skill[29] == 30, "levels: a skill rose before six hours");
    check_expect(character_raise_skills(&c, 360.0f) && c.skill[29] == 31 && c.skill_uses[29] == 0,
                 "levels: a skill with its uses did not rise at six hours");
    check_expect(!c.level_due, "levels: one point brought a level");
    /* A second point in a primary skill: the first level-up needs only 2. */
    character_tally(&c, 29, 100000);
    character_raise_skills(&c, 720.0f);
    check_expect(c.skill[29] == 32 && c.level_due, "levels: two points did not bring level 2");
    i = c.health_max;
    character_level_up(&c, 0);
    check_expect(c.level == 2 && !c.level_due && c.health_max > i, "levels: the level did not come, or its health");
    /* Past 95 nothing rises unless no primary skill has mastered yet; a mastered one holds the rest at 95. */
    c.skill[29] = 100;
    c.skill[0] = 95;
    character_tally(&c, 0, 100000);
    character_raise_skills(&c, 1080.0f);
    check_expect(c.skill[0] == 95, "levels: a skill passed 95 after a primary mastered");
    printf("levels: uses, six hours, the sum, a level's health, by hand\n");
}

/* What each class starts with, and TEXT.RSC and BIOG files as they read. */
static void check_kit_and_words(void)
{
    static Biog biog;
    Inventory pack;
    char words[2048];
    int k;

    kit_give(CLASS_WARRIOR, &pack);
    check_expect(pack.gold == 100 && pack.count == 1 && inventory_weapon(&pack)->template_id == 118
                     && inventory_weapon(&pack)->material == MATERIAL_IRON,
                 "kit: a warrior does not start with an iron broadsword and 100 gold");
    kit_give(13, &pack);
    check_expect(pack.count == 3 && inventory_weapon(&pack)->template_id == 130 && pack.item[2].template_id == ITEM_ARROW
                     && pack.item[2].count == 24,
                 "kit: an archer does not start with a long bow, a battle axe and 24 arrows");
    for (k = 0; k < 18; k++) {
        kit_give(k, &pack);
        check_expect(inventory_weapon(&pack) != 0, "kit: a class starts with nothing in hand");
    }

    check_expect(textrsc_read(career_race(RACE_ARGONIAN)->description, words, (int)sizeof(words)) > 100,
                 "words: the Argonian's description did not read");
    check_expect(textrsc_read(2100 + CLASS_WARRIOR, words, (int)sizeof(words)) > 0, "words: a class's description did not read");
    check_expect(textrsc_read(65000, words, (int)sizeof(words)) < 0, "words: a record that is not there read");

    /* Every class has its twelve questions, each with answers. */
    for (k = 0; k < 18; k++) {
        int q;

        check_expect(biog_load(k, &biog) && biog.questions == BIOG_QUESTIONS, "biog: a class does not have twelve questions");
        for (q = 0; q < biog.questions; q++) {
            check_expect(biog.question[q].answers >= 2 && biog.question[q].line[0][0], "biog: a question has no words or too few answers");
        }
    }
    printf("kit and words: 18 classes' kits, TEXT.RSC, 18 BIOG files of twelve questions\n");
}

/* Creation, driven as a player would: clicks on classic's places, keys for the name. */
#define CREATE_YES_X 120
#define CREATE_NO_X 190
#define CREATE_BOX_Y 160
/* BUTN01I0.IMG's two ways, and PICK00I0.IMG's next arrow and its rows. */
#define CREATE_METHOD_X 150
#define CREATE_METHOD_LIST_Y 90
#define CREATE_METHOD_ASK_Y 145
#define CREATE_LIST_NEXT_X 243
#define CREATE_LIST_NEXT_Y 148
#define CREATE_LIST_X 100
#define CREATE_LIST_ROW_Y(row) (63 + (row) * 8 + 2)
#define CREATE_OK_X 280
#define CREATE_OK_Y 180
static void check_create(void)
{
    Character sheet;
    Inventory pack;
    int x;
    int y;
    int found;
    int i;

    check_expect(create_open(), "create: it did not open");
    create_compose();
    write_picture(create_pixels(), "build/create0.ppm");

    /* Anywhere over the sea is no race. */
    create_click(2, 2);
    check_expect(create_step() == CREATE_RACE, "create: a click off the land chose a race");
    /* Find Black Marsh by the file's own mask: a click on a pixel of value 8. */
    found = 0;
    {
        ImgPicture mask;

        check_expect(img_load("TAMRIEL2.IMG", &mask), "create: no TAMRIEL2.IMG");
        for (y = 0; y < 200 && !found; y++) {
            for (x = 0; x < 320 && !found; x++) {
                if (mask.pixels[y * 320 + x] == RACE_ARGONIAN) {
                    create_click(x, y);
                    found = 1;
                }
            }
        }
    }
    check_expect(found && create_step() == CREATE_RACE_CONFIRM, "create: a click on Black Marsh asked nothing");
    create_compose();
    write_picture(create_pixels(), "build/create1.ppm");
    /* No goes back, Yes goes on. */
    create_click(CREATE_NO_X, CREATE_BOX_Y);
    check_expect(create_step() == CREATE_RACE, "create: No did not go back to the map");
    create_click(x - 1, y - 1);
    create_click(CREATE_YES_X, CREATE_BOX_Y);
    check_expect(create_step() == CREATE_SEX, "create: Yes did not go on");
    create_click(CREATE_YES_X, CREATE_BOX_Y);
    check_expect(create_step() == CREATE_METHOD, "create: choosing a woman did not go on");
    create_compose();
    write_picture(create_pixels(), "build/create11.ppm");
    create_click(CREATE_METHOD_X, CREATE_METHOD_LIST_Y);
    check_expect(create_step() == CREATE_CLASS, "create: choosing the list did not bring it");
    create_compose();
    write_picture(create_pixels(), "build/create2.ppm");
    /* The Warrior, class 16, down the list: eight steps, then the ninth row. */
    for (i = 0; i < 8; i++) {
        create_click(CREATE_LIST_NEXT_X, CREATE_LIST_NEXT_Y);
    }
    create_click(CREATE_LIST_X, CREATE_LIST_ROW_Y(8));
    check_expect(create_step() == CREATE_CLASS_CONFIRM, "create: a click on the list asked nothing");
    create_click(CREATE_YES_X, CREATE_BOX_Y);
    check_expect(create_step() == CREATE_QUESTIONS, "create: a class did not bring its questions");
    create_compose();
    write_picture(create_pixels(), "build/create3.ppm");
    /* Twelve answers, the first each time. */
    for (i = 0; i < BIOG_QUESTIONS; i++) {
        create_click(40, 80);
    }
    check_expect(create_step() == CREATE_NAME, "create: twelve answers did not reach the name");
    /* No name, no going on. A capital begins each word; backspace takes one off. */
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_NAME && create_note() != 0, "create: an empty name went on");
    create_key('S');
    create_key('E');
    create_key('E');
    create_key('X');
    create_key(0x08);
    create_key(0x20);
    create_key('J');
    create_key('A');
    create_compose();
    write_picture(create_pixels(), "build/create4.ppm");
    create_key(0x0D);
    check_expect(create_step() == CREATE_FACE, "create: Enter did not take the name");
    /* The next face, and on. */
    create_click(300, 73);
    create_compose();
    write_picture(create_pixels(), "build/create5.ppm");
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_ATTRIBUTES, "create: the face's OK did not go on");
    create_compose();
    write_picture(create_pixels(), "build/create6.ppm");
    /* Points must all be placed: OK refuses, then the spinner places them on Strength. */
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_ATTRIBUTES && create_note() != 0, "create: OK went on with points left");
    create_click(25, 30);
    create_click(50, 24);
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_ATTRIBUTES && create_note() != 0,
                 "create: one point did not leave points to place");
    create_click_bulk(50, 24, 1);
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_SKILLS, "create: shift did not place the rest of the points");
    create_compose();
    write_picture(create_pixels(), "build/create7.ppm");
    /* Six points a group: Shift on the spinner places the group's pool. */
    create_click_bulk(235, 33, 1);
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_SKILLS && create_note() != 0, "create: OK went on with the major and minor points left");
    create_click(100, 83);
    create_click_bulk(235, 82, 1);
    create_click(100, 132);
    create_click_bulk(235, 131, 1);
    create_compose();
    write_picture(create_pixels(), "build/create8.ppm");
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_REFLEXES, "create: every skill point placed did not go on");
    /* Very low, the bottom line. */
    create_click(150, 148 + 40);
    create_compose();
    write_picture(create_pixels(), "build/create9.ppm");
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_SUMMARY, "create: reflexes did not go on");
    create_compose();
    write_picture(create_pixels(), "build/create10.ppm");
    check_expect(create_click(CREATE_OK_X, CREATE_OK_Y) == CREATE_DONE, "create: the summary's OK did not finish");
    check_expect(create_result(&sheet, &pack), "create: no character came of it");
    check_expect(sheet.race == RACE_ARGONIAN && sheet.female && sheet.klass == CLASS_WARRIOR && sheet.level == 1,
                 "create: the character is not who was chosen");
    check_expect(strcmp(sheet.name, "See Ja") == 0, "create: the name is not as typed");
    check_expect(sheet.face == 1 && sheet.reflexes == 4, "create: the face or the reflexes were not kept");
    check_expect(inventory_weapon(&pack) != 0 && pack.gold <= 2000u, "create: no kit, or a mad amount of gold");
    check_expect(sheet.health == sheet.health_max && sheet.level_sum_start == character_level_sum(&sheet),
                 "create: the character is not whole, or their level count did not start");
    create_close();

    /* The ten questions: CLASSES.DAT's table says ten warrior answers make a Knight. */
    check_expect(create_open(), "create: it did not open for the questions");
    check_expect(create_class_of_answers(10, 0, 0) == 17, "create: ten warrior answers are not a Knight");
    i = create_class_of_answers(0, 0, 10);
    printf("create: ten mage answers make class %d\n", i);
    check_expect(i >= 0 && i < 18, "create: ten mage answers come to no class");
    create_click(2, 2);
    {
        ImgPicture mask;

        check_expect(img_load("TAMRIEL2.IMG", &mask), "create: no TAMRIEL2.IMG");
        found = 0;
        for (y = 0; y < 200 && !found; y++) {
            for (x = 0; x < 320 && !found; x++) {
                if (mask.pixels[y * 320 + x] == RACE_NORD) {
                    create_click(x, y);
                    found = 1;
                }
            }
        }
    }
    create_click(CREATE_YES_X, CREATE_BOX_Y);
    create_click(CREATE_NO_X, CREATE_BOX_Y);
    create_click(CREATE_METHOD_X, CREATE_METHOD_ASK_Y);
    check_expect(create_step() == CREATE_GENERATE, "create: choosing the questions did not ask one");
    create_compose();
    {
        df_u8 rgb[768];
        ImgPicture art;
        FILE *f;

        create_palette(rgb);
        /* Under its own palette, not ART_PAL.COL. */
        f = fopen("build/create12.ppm", "wb");
        if (f) {
            fprintf(f, "P6\n320 200\n255\n");
            for (i = 0; i < 320 * 200; i++) {
                fwrite(rgb + create_pixels()[i] * 3, 1, 3, f);
            }
            fclose(f);
        }
        check_expect(img_load("CHGN00I0.IMG", &art) && art.palette && rgb[3] == (df_u8)(art.palette[3] * 4), "create: the questions' screen is not under its own palette");
    }
    for (i = 0; i < 10; i++) {
        create_key('A' + (i % 3));
    }
    check_expect(create_step() == CREATE_CLASS_CONFIRM, "create: ten answers did not come to a class");
    create_close();

    /* A class of one's own: last on the list. It will not finish until it is whole. */
    check_expect(create_open(), "create: it did not open for a custom class");
    {
        ImgPicture mask;

        img_load("TAMRIEL2.IMG", &mask);
        found = 0;
        for (y = 0; y < 200 && !found; y++) {
            for (x = 0; x < 320 && !found; x++) {
                if (mask.pixels[y * 320 + x] == RACE_KHAJIIT) {
                    create_click(x, y);
                    found = 1;
                }
            }
        }
    }
    create_click(CREATE_YES_X, CREATE_BOX_Y);
    create_click(CREATE_NO_X, CREATE_BOX_Y);
    create_click(CREATE_METHOD_X, CREATE_METHOD_LIST_Y);
    for (i = 0; i < 10; i++) {
        create_click(CREATE_LIST_NEXT_X, CREATE_LIST_NEXT_Y);
    }
    create_click(CREATE_LIST_X, CREATE_LIST_ROW_Y(8));
    check_expect(create_step() == CREATE_CUSTOM, "create: Custom on the list did not begin a class");
    create_click(280, 180);
    check_expect(create_step() == CREATE_CUSTOM && create_note() != 0, "create: a class with no name finished");
    create_key('T');
    create_key('O');
    create_key('Y');
    create_click(280, 180);
    check_expect(create_step() == CREATE_CUSTOM && create_note() != 0, "create: a class with no skills finished");
    /* Twelve skills, each from the top of the list: primary, major, minor lines. */
    {
        static const df_u8 lines[12] = { 32, 42, 52, 81, 91, 101, 130, 140, 150, 160, 170, 180 };

        for (i = 0; i < 12; i++) {
            create_click(100, lines[i] + 1);
            check_expect(create_step() == CREATE_CUSTOM_PICK, "create: a skill's line did not bring the list");
            create_click(CREATE_LIST_X, CREATE_LIST_ROW_Y(0));
        }
    }
    /* Hit points up by four, the attributes moved about. */
    for (i = 0; i < 4; i++) {
        create_click(255, 50);
    }
    create_click(25, 30);
    create_click(50, 37);
    create_click(25, 52);
    create_click(50, 46);
    create_compose();
    write_picture(create_pixels(), "build/create13.ppm");
    create_click(280, 180);
    check_expect(create_step() == CREATE_QUESTIONS || create_step() == CREATE_NAME, "create: a whole class did not go on");
    {
        const CareerClass *mine;

        mine = career_class(CLASS_CUSTOM);
        check_expect(mine && strcmp(mine->name, "Toy") == 0 && mine->hp_per_level == 12, "create: the class is not as made");
        /* Four hit points over the middle: hardness 4, 0.3 + 2.7 x 16 / 52 is 1.13. */
        check_expect(mine && check_near(rules_class_advance(mine->advancement), 1.13f, 0.01f), "create: a class's advancement is not by its hardness");
        check_expect(mine && mine->attribute[ATTR_STRENGTH] == 49 && mine->attribute[ATTR_INTELLIGENCE] == 51, "create: the attributes were not moved");
    }
    create_close();

    /*
     * Escape on the race map cancels. The step stays the map, nobody is made,
     * and the game's answer is the start menu (screens turns CANCELLED into
     * CLOSED; that file is not on the host). A page after the map does not
     * take the same way out.
     */
    check_expect(create_open() && create_key(0x1B) == CREATE_CANCELLED, "create: Escape on the map did not cancel");
    check_expect(create_step() == CREATE_RACE, "create: Escape left the race map");
    check_expect(!create_result(&sheet, &pack), "create: Escape on the map made a character");
    create_close();
    check_expect(create_open(), "create: it did not open again after Escape");
    {
        ImgPicture mask;

        found = 0;
        if (img_load("TAMRIEL2.IMG", &mask)) {
            for (y = 0; y < 200 && !found; y++) {
                for (x = 0; x < 320 && !found; x++) {
                    if (mask.pixels[y * 320 + x] == RACE_BRETON) {
                        create_click(x, y);
                        found = 1;
                    }
                }
            }
        }
    }
    check_expect(found && create_step() == CREATE_RACE_CONFIRM, "create: a race was not waiting to confirm");
    check_expect(create_key(0x1B) == CREATE_STAY && create_step() == CREATE_RACE_CONFIRM,
                 "create: Escape on the confirm left it or started a game");
    create_close();
    /* Escape before any race is selected cancels. It creates no character
     * and leaves no kit for the caller to turn into a new game. */
    check_expect(create_open() && create_key(0x1B) == CREATE_CANCELLED, "create: Escape on the map did not cancel");
    check_expect(!create_result(&sheet, &pack), "create: canceled creation produced a character");
    create_close();
    check_expect(create_default(&sheet, &pack) && sheet.race == RACE_ARGONIAN && sheet.female && pack.gold == 100,
                 "create: the default is not an Argonian woman with her kit");
    printf("create: map to summary by clicks and keys; build/create0..10.ppm\n");
}

/* The character sheet, and a level taken on it. */
static void check_sheet(void)
{
    Player p;
    Character sheet;
    Inventory pack;
    int before;
    int points;
    int i;

    if (!create_default(&sheet, &pack)) {
        check_expect(0, "sheet: no default character");
        return;
    }
    memset(&p, 0, sizeof(p));
    player_init(&p, &sheet);
    p.pack = pack;
    player_wear_changed(&p);

    /* No level due: it opens with nothing to place, and Exit leaves. */
    check_expect(sheet_open(&p) && sheet_points() == 0, "sheet: it opened with points and no level due");
    sheet_compose();
    write_picture(sheet_pixels(), "build/sheet0.ppm");
    /* A list opens, and the next click only puts it away. */
    sheet_click(50, 110);
    sheet_compose();
    write_picture(sheet_pixels(), "build/sheet1.ppm");
    sheet_click(70, 190);
    check_expect(sheet_click(70, 190) == SHEET_LEAVE, "sheet: Exit did not leave");
    sheet_close();

    /* A level due: taken as it opens, and it cannot be left until its points are placed. */
    p.sheet.level_due = 1;
    before = p.sheet.health_max;
    check_expect(sheet_open(&p) && p.sheet.level == 2 && p.sheet.health_max > before && !p.sheet.level_due,
                 "sheet: the level was not taken as it opened");
    points = sheet_points();
    check_expect(points >= RULES_LEVEL_POOL_LOW && points < RULES_LEVEL_POOL_LOW + RULES_LEVEL_POOL_ROLLS,
                 "sheet: a level's points are not 4 to 6");
    sheet_compose();
    write_picture(sheet_pixels(), "build/sheet2.ppm");
    check_expect(sheet_click(70, 190) == SHEET_STAY && sheet_note() != 0, "sheet: Exit left with points to place");
    /* The spinner by Strength: one on, one off, then every point. A point cannot come off below where it began. */
    before = p.sheet.attribute[ATTR_STRENGTH];
    sheet_click(180, 8);
    check_expect(p.sheet.attribute[ATTR_STRENGTH] == before + 1 && sheet_points() == points - 1, "sheet: the spinner put no point on");
    sheet_click(180, 22);
    check_expect(p.sheet.attribute[ATTR_STRENGTH] == before && sheet_points() == points, "sheet: the spinner took no point off");
    sheet_click(180, 22);
    check_expect(p.sheet.attribute[ATTR_STRENGTH] == before, "sheet: a point came off below where it began");
    for (i = 0; i < points; i++) {
        sheet_click(180, 8);
    }
    check_expect(sheet_points() == 0 && p.sheet.fatigue_max == (p.sheet.attribute[ATTR_STRENGTH] + p.sheet.attribute[ATTR_ENDURANCE]) * CHARACTER_FATIGUE_UNIT,
                 "sheet: fatigue did not follow Strength");
    check_expect(sheet_click(70, 190) == SHEET_LEAVE, "sheet: Exit did not leave with every point placed");
    sheet_close();
    printf("sheet: the sheet, a list, a level taken and its points placed; build/sheet0..2.ppm\n");
}

/* What a class allows, proficiency, groups, wear, the mouse's swings and arrows, by hand. */
static void check_class_and_wear(void)
{
    static const signed char motions[][3] = {
        /* dx, dy, the swing: mouse down is +dy. */
        { 40, 0, 4 }, { 0, -40, 6 }, { 30, -30, 6 }, { -30, -30, 6 }, { -40, 0, 3 }, { -30, 30, 2 }, { 0, 40, 1 }, { 30, 30, 5 }
    };
    Character c;
    Foe f;
    Blow b;
    Item it;
    Inventory inv;
    int i;

    /* The Mage's word: long blades, axes and missiles forbidden; chain, plate and all but the buckler. */
    character_new(&c, RACE_BRETON, CLASS_MAGE, 0, "Test");
    item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON);
    check_expect(character_refuses(&c, &it) != 0, "class: a Mage may wield a longsword");
    item_make(&it, ITEM_DAGGER, MATERIAL_IRON);
    check_expect(character_refuses(&c, &it) == 0, "class: a Mage may not wield a dagger");
    item_make(&it, ITEM_CUIRASS, MATERIAL_IRON);
    check_expect(character_refuses(&c, &it) != 0, "class: a Mage may wear plate");
    item_make(&it, ITEM_CUIRASS, MATERIAL_CHAIN);
    check_expect(character_refuses(&c, &it) != 0, "class: a Mage may wear chain");
    item_make(&it, ITEM_CUIRASS, MATERIAL_LEATHER);
    check_expect(character_refuses(&c, &it) == 0, "class: a Mage may not wear leather");
    item_make(&it, ITEM_BUCKLER, MATERIAL_IRON);
    check_expect(character_refuses(&c, &it) == 0, "class: a Mage may not carry a buckler");
    item_make(&it, ITEM_TOWER_SHIELD, MATERIAL_IRON);
    check_expect(character_refuses(&c, &it) != 0, "class: a Mage may carry a tower shield");

    /* The Archer's word: expert with missiles, plate and every shield forbidden. */
    character_new(&c, RACE_BRETON, 13, 0, "Test");
    check_expect(character_expert(&c, SKILL_ARCHERY) && !character_expert(&c, SKILL_SHORT_BLADE), "class: an Archer is not expert with a bow alone");
    /* A Warrior has none of it. */
    character_new(&c, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    check_expect(!character_expert(&c, SKILL_ARCHERY), "class: a Warrior is expert with a bow");

    /* An expert gains the level to hit and level / 3 + 1 damage: level 6, 6 and 3 on the Archer's sum. */
    character_new(&c, RACE_BRETON, 13, 0, "Test");
    c.level = 6;
    c.skill[SKILL_CRITICAL_STRIKE] = 0;
    combat_foe(&f, enemy_kind(ENEMY_RAT), 6, 0);
    item_make(&it, 130, MATERIAL_STEEL);
    combat_blow(&c, &it, 3, &f, &b);
    i = b.chance;
    c.klass = CLASS_WARRIOR;
    c.skill[SKILL_ARCHERY] = career_class(13) ? c.skill[SKILL_ARCHERY] : 0;
    combat_blow(&c, &it, 3, &f, &b);
    check_expect(i == b.chance + 6 || i == 97, "class: an expert's chance is not the level more");

    /* Groups: undead, daedra, humanoid, animals, and a class enemy in none. */
    check_expect(enemy_group(ENEMY_RAT) == ENEMY_GROUP_ANIMALS && enemy_group(ENEMY_SKELETAL_WARRIOR) == ENEMY_GROUP_UNDEAD
                     && enemy_group(25) == ENEMY_GROUP_DAEDRA && enemy_group(7) == ENEMY_GROUP_HUMANOID
                     && enemy_group(ENEMY_FIRST_CLASS) == ENEMY_GROUP_NONE,
                 "class: a creature is in the wrong group");

    /* Wear: 15 damage takes (150 + 50) / 100 = 2; 4 takes none but a roll under 20 takes 1. */
    item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON);
    i = it.condition;
    item_wear_down(&it, 15, 99);
    check_expect(it.condition == i - 2, "wear: 15 damage did not take 2");
    i = it.condition;
    item_wear_down(&it, 4, 99);
    check_expect(it.condition == i, "wear: 4 damage on a high roll took some");
    item_wear_down(&it, 4, 0);
    check_expect(it.condition == i - 1, "wear: 4 damage on a low roll took none");
    /* Worn right out it is broken, and cannot be worn or wielded. */
    it.condition = 1;
    check_expect(item_wear_down(&it, 15, 99) && item_broken(&it), "wear: it did not break");
    inventory_clear(&inv);
    check_expect(!inventory_equip(&inv, inventory_add(&inv, &it)), "wear: a broken sword went in hand");

    /* What takes a blow: the shield over the armour on a part it covers. */
    inventory_clear(&inv);
    item_make(&it, ITEM_CUIRASS, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    item_make(&it, ITEM_BUCKLER, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    check_expect(inventory_armor_at_part(&inv, BODY_CHEST) == EQUIP_CHEST && inventory_armor_at_part(&inv, BODY_HANDS) == EQUIP_LEFT_HAND
                     && inventory_armor_at_part(&inv, BODY_HEAD) == -1,
                 "wear: the blow is not taken by what covers the part");

    /* The mouse's swings, by DFU's segments. */
    for (i = 0; i < (int)(sizeof(motions) / sizeof(motions[0])); i++) {
        check_expect(weapon_swing_of_motion(motions[i][0], motions[i][1]) == motions[i][2], "swing: a motion gave the wrong swing");
    }
    /* A gesture: the button down and the mouse moved right swings right, the look's distance reached. */
    weapon_hold(0);
    weapon_gesture(1, 1, 0, 0, 0, 0.016f);
    check_expect(!weapon_swinging(), "swing: the press alone swung");
    check_expect(weapon_gesture(0, 1, 0, WEAPON_GESTURE, 0, 0.016f) == 4 && weapon_swing() == 4, "swing: a move right did not swing right");
    while (weapon_swinging()) {
        weapon_step(0.1f, 50);
    }
    weapon_hit();
    /* A click with no motion still swings, at random. */
    weapon_gesture(1, 1, 0, 0, 0, 0.016f);
    check_expect(weapon_gesture(0, 0, 1, 0, 0, 0.016f) >= 1 && weapon_swinging(), "swing: a click did not swing");
    while (weapon_swinging()) {
        weapon_step(0.1f, 50);
    }
    weapon_hit();
    weapon_free();

    /* Arrows: spent one at a time, the stack gone with the last. */
    inventory_clear(&inv);
    check_expect(inventory_arrows(&inv) < 0 && !inventory_spend_arrow(&inv), "arrows: an empty pack shot one");
    item_make(&it, ITEM_ARROW, MATERIAL_IRON);
    it.count = 2;
    inventory_add(&inv, &it);
    check_expect(inventory_spend_arrow(&inv) && inv.item[0].count == 1 && inventory_spend_arrow(&inv) && inv.count == 0,
                 "arrows: they were not spent to nothing");
    printf("class and wear: limits, expert, groups, condition, swings by the mouse, arrows\n");
}

/* The senses and the rest of what creatures do beyond sight, by hand and in the hold. */
static void check_creatures_more(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    Inventory one;
    Inventory two;
    int armor[BODY_PARTS];
    Entity *foe;
    Entity *classed;
    Entity *blood;
    int s;
    int i;

    /* Stealth: 500 units off, Stealth 30: 500 x 30 >> 10 is 14, doubled. Nearer, or less skilled, less. */
    check_expect(foes_stealth_chance(500.0f, 30) == 28 && foes_stealth_chance(0.0f, 30) == 0 && foes_stealth_chance(1024.0f, 100) == 200,
                 "senses: the stealth chance is not classic's");
    /* Sneaking, DFU's IsMovingLessThanHalfSpeed: standing still or crouched at half a walk is rolled against every other minute; walking or running, every one. */
    {
        Player sp;
        Character ssheet;
        float half;

        character_new(&ssheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
        player_init(&sp, &ssheet);
        half = rules_walk_speed(sp.sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 0.5f;
        sp.speed_xz = 0.0f;
        check_expect(player_sneaking(&sp) && foes_rolls(&sp, 10) && !foes_rolls(&sp, 11), "sneaking: standing still is not rolled every other minute");
        sp.speed_xz = half;
        check_expect(player_sneaking(&sp), "sneaking: a crouch's half a walk is not sneaking");
        sp.speed_xz = half * 2.0f;
        check_expect(!player_sneaking(&sp) && foes_rolls(&sp, 10) && foes_rolls(&sp, 11), "sneaking: a walk is rolled every minute");
        sp.speed_xz = half * 4.0f;
        check_expect(!player_sneaking(&sp), "sneaking: a run is sneaking");
    }
    /* Knock-back: at least 15; heavier knocks less far than light, for the same blow. */
    check_expect(combat_knock_speed(0, 100) == 0 && combat_knock_speed(1, 1000) == 15, "knock: the least is not 15");
    check_expect(combat_knock_speed(20, 40) >= combat_knock_speed(20, 400), "knock: a heavy creature goes further than a light one");

    keep_clear();
    combat_reset();
    foes_reset();
    if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "creatures: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    loot_new_game(99u);

    foe = 0;
    classed = 0;
    for (s = 0; s < stream_slots(); s++) {
        Entity *ents;
        int first;
        int n;

        ents = stream_slot_entities(s, &first, &n);
        for (i = 0; ents && i < n; i++) {
            int kind;

            kind = dunstream_kind(&ents[i], 0);
            if (ents[i].live && kind >= ENEMY_FIRST_CLASS && !classed) {
                classed = &ents[i];
            }
            if (ents[i].live && kind >= 0 && kind < ENEMY_FIRST_CLASS && !foe) {
                foe = &ents[i];
            }
        }
    }
    check_expect(foe && classed, "creatures: the hold lacks a monster or a class enemy");

    /* The listing: sleepers near the start, each with what it makes of the player. */
    {
        Player lp;
        Character lsheet;
        df_u32 before;
        int added;
        int lines;
        int k;

        character_new(&lsheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
        player_init(&lp, &lsheet);
        player_place(&lp, x, y, z, yaw, pitch);
        before = log_serial();
        foes_report(&lp);
        /* The log is newest first, and a ring: the lines added are the first few. */
        added = (int)(log_serial() - before);
        lines = 0;
        for (k = 0; k < added && k < log_count(); k++) {
            const char *line;

            line = log_get(k);
            lines += line && strstr(line, "asleep") != 0;
        }
        printf("foes: the listing added %d lines, %d of sleepers\n", added, lines);
        check_expect(added >= 2 && lines >= 1, "foes: the listing shows no sleeper near the start");
    }

    /* A class enemy's kit: the same each time, a weapon in hand, its armour capped, its level's materials. */
    if (classed) {
        loot_class_kit(classed, 5, &one, armor);
        loot_class_kit(classed, 5, &two, 0);
        check_expect(one.count == two.count && one.count >= 1 && inventory_weapon(&one) != 0, "creatures: a class enemy's kit is not its own, or has no weapon");
        for (i = 0; i < BODY_PARTS; i++) {
            check_expect(armor[i] <= CLASS_ARMOR_CAP, "creatures: a class enemy's armour is past its cap");
        }
        /* Its body holds what it wore. */
        {
            Inventory on_body;
            df_u8 origin[INVENTORY_MAX];
            Keep *row;
            int worn_found;
            int k;

            classed->dead = 1;
            row = keep_get(classed->key, KEEP_ENTITY);
            row->game[COMBAT_KEEP_LEVEL] = 5;
            loot_open(classed, 5, &on_body, origin);
            worn_found = 0;
            for (k = 0; k < on_body.count; k++) {
                worn_found += on_body.item[k].template_id == one.item[0].template_id && on_body.item[k].material == one.item[0].material;
            }
            check_expect(worn_found > 0, "creatures: a class enemy's body lacks what it wore");
            classed->dead = 0;
        }
    }

    /* Blood: shown on a block's spare effect, its picture read, nothing kept. */
    if (foe) {
        const EnemyKind *k;

        k = enemy_kind(dunstream_kind(foe, 0));
        blood = dunstream_blood(foe->x, foe->y + 30.0f, foe->z, k->blood);
        check_expect(blood && blood->live && !blood->key.place && !blood->key.thing && blood->pose[ENTITY_WALK].w[0] > 0,
                     "creatures: no blood showed");
        check_expect(blood && dunstream_kind(blood, 0) < 0, "creatures: blood was taken for a creature");
        if (blood) {
            blood->live = 0;
        }

        /* A knock-back moves it away from the blow, and it dies away. */
        foes_alert(foe);
        {
            Character sheet;
            Player p;
            float was[3];
            float from[3];
            float d0;
            float d1;

            character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
            player_init(&p, &sheet);
            /* Far off and out of its sight: only the knock moves it. */
            player_place(&p, foe->x + 3000.0f, foe->y + 3000.0f, foe->z + 3000.0f, 0.0f, 0.0f);
            was[0] = foe->x;
            was[1] = foe->y;
            was[2] = foe->z;
            from[0] = foe->x - 20.0f;
            from[1] = foe->y;
            from[2] = foe->z;
            foes_knock(foe, from, 40);
            for (i = 0; i < 6; i++) {
                foes_frame(&p, &level.scene, 0.05f);
            }
            d0 = foe->x - was[0];
            d1 = foe->z - was[2];
            check_expect(d0 * d0 + d1 * d1 > 0.0f && d0 >= 0.0f, "creatures: a knock-back did not move it away from the blow");
        }
    }
    foes_reset();
    dunstream_close();
    keep_clear();
    printf("creatures more: stealth, knock-back, blood, class enemies' kits, bodies with what they wore\n");
}

/* Swimming, breath, fatigue and exhaustion, by hand and in water laid over the hold's floor. */
static void check_body_more(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    Character sheet;
    Player p;
    Input in;
    Water *flood;
    float walk_d;
    float swim_d;
    int i;

    check_expect(check_near(rules_swim_speed(4.0f, 0), 1.0f, 0.001f) && check_near(rules_swim_speed(4.0f, 100), 3.0f, 0.001f),
                 "swim: a swimmer's speed is not classic's");
    check_expect(rules_max_breath(60) == 30, "swim: the most breath is not half of Endurance");

    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    player_init(&p, &sheet);

    /* A minute tires by 11, running by 88; the skill's roll is an Argonian's to skip. */
    p.sheet.fatigue = 1000;
    check_expect(player_minute(&p, 0) == PLAYER_FINE && p.sheet.fatigue == 989, "fatigue: a minute did not take 11");
    p.running = 1;
    player_minute(&p, 0);
    check_expect(p.sheet.fatigue == 901, "fatigue: a running minute did not take 88");
    p.running = 0;

    /* Out of it: with nothing about, down for an hour and some back; enemies about, or in the water, dead. */
    p.sheet.fatigue = 5;
    p.sheet.health = 1;
    i = p.sheet.skill_uses[SKILL_MEDICAL];
    check_expect(player_minute(&p, 0) == PLAYER_COLLAPSED && p.sheet.fatigue > 0 && p.sheet.health > 1 && p.sheet.skill_uses[SKILL_MEDICAL] == i + 1,
                 "fatigue: collapsing did not give an hour's rest");
    p.sheet.fatigue = 5;
    check_expect(player_minute(&p, 1) == PLAYER_DIED, "fatigue: exhaustion with enemies about did not kill");
    p.sheet.fatigue = 5;
    p.swimming = 1;
    check_expect(player_minute(&p, 0) == PLAYER_DIED, "fatigue: exhaustion in the water did not kill");
    /* tgm: nothing wears the player down, in the water with enemies about, nor does breath run out. */
    p.sheet.fatigue = 5;
    p.god = 1;
    check_expect(player_minute(&p, 1) == PLAYER_FINE && p.sheet.fatigue == 5, "fatigue: tgm let exhaustion kill");
    p.breath = 1;
    p.breath_time = 0.0f;
    check_expect(!player_breath(&p, 30.0f), "breath: tgm let the player drown");
    p.god = 0;

    /* Breath: half of Endurance in points, one each 1.04 s; none left drowns. An Argonian lasts longer. */
    p.swimming = 1;
    p.breath = 0;
    p.breath_time = 0.0f;
    p.sheet.attribute[ATTR_ENDURANCE] = 10;
    for (i = 0; i < 4; i++) {
        check_expect(!player_breath(&p, 1.05f), "breath: it ran out too soon");
    }
    check_expect(player_breath(&p, 1.05f), "breath: five points did not run out in five breaths");
    p.swimming = 0;
    check_expect(!player_breath(&p, 1.0f) && p.breath == 0, "breath: it did not reset at the surface");
    {
        Player q;
        int spent;

        q = p;
        q.sheet.race = RACE_ARGONIAN;
        q.swimming = 1;
        combat_seed(3);
        spent = 0;
        while (!player_breath(&q, 1.05f) && spent < 1000) {
            spent++;
        }
        check_expect(spent > 5, "breath: an Argonian's did not last longer");
    }

    /* In the water: laid over the hold's start, deep. It floats, and goes slower than it walks. */
    keep_clear();
    combat_reset();
    if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "swim: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    player_init(&p, &sheet);
    player_place(&p, x, y, z, 3.1416f, 0.0f);
    memset(&in, 0, sizeof(in));
    walk(&p, &level.scene, &in, 60);
    in.move_f = 1;
    walk_d = walk(&p, &level.scene, &in, 15);
    check_expect(walk_d > 10.0f, "swim: there is no room to walk at the start");
    flood = (Water *)&level.scene.water[0];
    flood->x0 = p.body.feet[0] - 4000.0f;
    flood->x1 = p.body.feet[0] + 4000.0f;
    flood->z0 = p.body.feet[2] - 4000.0f;
    flood->z1 = p.body.feet[2] + 4000.0f;
    flood->y = p.body.feet[1] + 400.0f;
    player_place(&p, x, y, z, 3.1416f, 0.0f);
    memset(&in, 0, sizeof(in));
    walk(&p, &level.scene, &in, 2);
    check_expect(p.swimming, "swim: under water it is not swimming");
    in.move_f = 1;
    swim_d = walk(&p, &level.scene, &in, 15);
    check_expect(swim_d > 0.0f && swim_d < walk_d, "swim: it does not swim slower than it walks");
    flood->x1 = flood->x0;
    dunstream_close();
    keep_clear();
    printf("body: fatigue, collapse, breath, a swimmer's speed in water over the hold's floor\n");
}

/* Two weapons, and which hand is in use. */
static void check_hands(void)
{
    Inventory inv;
    Item it;
    int i;

    inventory_clear(&inv);
    item_make(&it, ITEM_LONGSWORD, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    item_make(&it, ITEM_DAGGER, MATERIAL_STEEL);
    i = inventory_add(&inv, &it);
    check_expect(inventory_wear(&inv, i) && inv.worn[EQUIP_LEFT_HAND] == i && inv.worn[EQUIP_RIGHT_HAND] == 0,
                 "hands: a second one-handed weapon did not take the left hand");
    check_expect(inventory_weapon(&inv) == &inv.item[0], "hands: the right is not in use at first");
    check_expect(inventory_swap_hands(&inv) && inventory_weapon(&inv) == &inv.item[i] && inventory_hand_place(&inv) == EQUIP_LEFT_HAND,
                 "hands: swapping did not bring the left in use");
    check_expect(inventory_swap_hands(&inv) && inventory_weapon(&inv) == &inv.item[0], "hands: swapping back did not bring the right");
    /* A shield takes the left hand and keeps the right in use. */
    inventory_swap_hands(&inv);
    item_make(&it, ITEM_BUCKLER, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    check_expect(!inv.use_left && !inventory_swap_hands(&inv) && inventory_weapon(&inv) == &inv.item[0], "hands: a shield did not keep the right in use");
    /* A two-handed weapon takes both: the other hand in use is bare. */
    item_make(&it, 122, MATERIAL_IRON);
    inventory_wear(&inv, inventory_add(&inv, &it));
    check_expect(inv.worn[EQUIP_LEFT_HAND] == -1, "hands: a two-handed weapon left the left hand full");
    check_expect(inventory_swap_hands(&inv) && inventory_weapon(&inv) == 0, "hands: the left is not bare beside a two-handed weapon");
    printf("hands: two weapons, a shield, a two-handed weapon\n");
}

/*
 * The bug this guards: an animated record too large for a 256 sheet went still,
 * so a Giant Bat flew at the player without moving a wing while its other views
 * flapped. Every creature's walk, attack, hurt and idle records that have frames
 * must make a sheet, at the size in the world they had before.
 */
static void check_sheets_move(void)
{
    int kind;
    int shrunk;
    int records;

    shrunk = 0;
    records = 0;
    for (kind = 0; kind < ENEMY_KINDS; kind++) {
        int female;

        if (!enemy_kind(kind)) {
            continue;
        }
        for (female = 0; female < 2; female++) {
            int archive;
            int record;

            archive = enemy_archive(kind, female);
            if (archive <= 0) {
                continue;
            }
            for (record = 0; record < 20; record++) {
                Image *img;
                int was_w;
                int was_h;
                int texels;

                if (record >= texture_record_count(archive) || texture_frame_count(archive, record) < 2) {
                    continue;
                }
                img = texture_load(archive, record);
                if (!img) {
                    continue;
                }
                was_w = texture_extent(img->width, img->xscale);
                was_h = texture_extent(img->height, img->yscale);
                texels = img->width;
                records++;
                check_expect(texture_sheet(img, archive, record), "sheets: a creature's animated record made no sheet");
                check_expect(img->frames == texture_frame_count(archive, record), "sheets: a sheet lost frames");
                check_expect(img->pot_w <= 256 && img->pot_h <= 256, "sheets: a sheet is past what the card takes");
                check_expect(texture_extent(img->width, img->xscale) == was_w && texture_extent(img->height, img->yscale) == was_h,
                             "sheets: a shrunk picture changed its size in the world");
                shrunk += img->width < texels;
            }
        }
    }
    printf("sheets: %d animated creature records all make sheets, %d of them at reduced texels\n", records, shrunk);
    check_expect(shrunk > 0, "sheets: nothing needed shrinking: this test no longer tests it");
}

/* Rest: its sums by hand, and the three ways driven by clicks and keys. */
#define REST_WHILE_X 110
#define REST_HEALED_X 160
#define REST_LOITER_X 210
#define REST_BUTTON_Y 75
static void check_rest(void)
{
    Player p;
    Character sheet;
    Inventory pack;
    float was;
    int i;

    /* Endurance 60 adds 1; Medical 40 + 60 is 100, of 100 health over 1000 is 10. */
    check_expect(rules_rest_health(60, 40, 100) == 11, "rest: an hour's health is not classic's");
    check_expect(rules_rest_health(0, 0, 10) == 1, "rest: an hour's health is under 1");
    check_expect(rules_rest_fatigue(120) == 15 && rules_rest_magicka(24) == 3 && rules_rest_magicka(0) == 0,
                 "rest: an hour's fatigue or spell points are not an eighth");

    if (!create_default(&sheet, &pack)) {
        check_expect(0, "rest: no default character");
        return;
    }
    memset(&p, 0, sizeof(p));
    player_init(&p, &sheet);
    p.pack = pack;
    foes_reset();

    /* Until fully healed: from half, it ends whole, and time has passed. */
    p.sheet.health = p.sheet.health_max / 2;
    p.sheet.fatigue = p.sheet.fatigue_max / 2;
    was = clock_minute();
    check_expect(rest_open(&p), "rest: it did not open");
    rest_compose();
    write_picture(rest_pixels(), "build/rest0.ppm");
    rest_click(REST_HEALED_X, REST_BUTTON_Y);
    rest_compose();
    write_picture(rest_pixels(), "build/rest1.ppm");
    for (i = 0; i < 100000 && rest_step(0.1f) == REST_STAY; i++) {
    }
    check_expect(p.sheet.health == p.sheet.health_max && p.sheet.fatigue == p.sheet.fatigue_max && rest_said() != 0,
                 "rest: until healed did not end whole");
    check_expect(clock_minute() != was && p.sheet.skill_uses[SKILL_MEDICAL] > 0, "rest: no time passed, or Medical was not used");
    rest_close();

    /* For a while: three hours typed, and it heals for just those. */
    p.sheet.health = 1;
    p.sheet.skill_uses[SKILL_MEDICAL] = 0;
    check_expect(rest_open(&p), "rest: it did not open again");
    rest_click(REST_WHILE_X, REST_BUTTON_Y);
    rest_key('3');
    rest_compose();
    write_picture(rest_pixels(), "build/rest2.ppm");
    rest_key(0x0D);
    for (i = 0; i < 100000 && rest_step(0.1f) == REST_STAY; i++) {
    }
    check_expect(p.sheet.skill_uses[SKILL_MEDICAL] == 3, "rest: three hours were not three uses of Medical");
    check_expect(p.sheet.health == 1 + 3 * rules_rest_health(p.sheet.attribute[ATTR_ENDURANCE], p.sheet.skill[SKILL_MEDICAL], p.sheet.health_max),
                 "rest: three hours did not give back three hours' health");
    rest_close();

    /* Loitering: the hours pass, nothing heals. */
    p.sheet.health = 1;
    p.sheet.skill_uses[SKILL_MEDICAL] = 0;
    check_expect(rest_open(&p), "rest: it did not open a third time");
    rest_click(REST_LOITER_X, REST_BUTTON_Y);
    rest_key('2');
    rest_key(0x0D);
    for (i = 0; i < 100000 && rest_step(0.1f) == REST_STAY; i++) {
    }
    check_expect(p.sheet.health == 1 && p.sheet.skill_uses[SKILL_MEDICAL] == 0, "rest: loitering healed, or used Medical");
    rest_close();

    /* A town's building with no room: rest refused with a word, loitering allowed. */
    check_expect(rest_open(&p), "rest: it did not open for the rules");
    rest_set_rule(REST_RULE_NO_ROOM, 0);
    check_expect(rest_click(REST_WHILE_X, REST_BUTTON_Y) == REST_STAY && rest_note() && strstr(rest_note(), "rented"),
                 "rest: no room, and rest was not refused");
    rest_click(REST_LOITER_X, REST_BUTTON_Y);
    rest_key('1');
    rest_key(0x0D);
    for (i = 0; i < 100000 && rest_step(0.1f) == REST_STAY; i++) {
    }
    check_expect(rest_said() && strstr(rest_said(), "loitering"), "rest: loitering was refused without a room");
    rest_close();
    /* A town's streets: asking to rest is vagrancy, the screen closing for the game, said once. */
    check_expect(rest_open(&p), "rest: it did not open in the street");
    rest_set_rule(REST_RULE_VAGRANCY, 0);
    check_expect(rest_click(REST_HEALED_X, REST_BUTTON_Y) == REST_LEAVE && rest_vagrant() && !rest_vagrant(),
                 "rest: vagrancy not tried once");
    rest_close();
    /* A room held two hours: a five-hour rest ends when it runs out. */
    p.sheet.health = 1;
    p.sheet.skill_uses[SKILL_MEDICAL] = 0;
    check_expect(rest_open(&p), "rest: it did not open in a room");
    rest_set_rule(REST_RULE_ROOM, 2);
    rest_click(REST_WHILE_X, REST_BUTTON_Y);
    rest_key('5');
    rest_key(0x0D);
    for (i = 0; i < 100000 && rest_step(0.1f) == REST_STAY; i++) {
    }
    check_expect(p.sheet.skill_uses[SKILL_MEDICAL] == 2 && rest_said() && strstr(rest_said(), "expired"),
                 "rest: a two-hour room did not end a five-hour rest");
    rest_close();

    /* Escape leaves it from the panel. */
    check_expect(rest_open(&p) && rest_key(0x1B) == REST_LEAVE, "rest: Escape did not leave");
    rest_close();
    printf("rest: its sums, until healed, for a while, loitering, a town's rules; build/rest0..2.ppm\n");
}

/* What the test's saves are called, beside where a player's would be: TEST0.SAV and on, gone after. */
/* A snapshot of a known pattern for the save tests. */
static df_u16 g_test_thumb[SAVE_THUMB_W * SAVE_THUMB_H];

/*
 * A save's round trip in the streamed hold: a door, a dead creature, a pile
 * and a thing taken, written with the sheet and the pack, everything then
 * wiped, read back, and the hold opened again finding it all as it was.
 */
static void check_save(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    SaveState out;
    SaveState in;
    Inventory things;
    Inventory on_it;
    df_u8 origin[INVENTORY_MAX];
    Entity *body;
    Entity *pile;
    KeepKey body_key;
    KeepKey pile_key;
    KeepKey door_key;
    Item it;
    int found;
    int door;
    int s;
    int i;

    save_use_file("DATA\\TEST2.SAV");
    keep_clear();
    combat_reset();
    pile_new_game();
    loot_new_game(4242u);
    if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "save: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);

    /* A door left open: only stream_keep_now can have saved it, its block is still loaded. */
    door = -1;
    for (i = 0; i < level.scene.nmovers; i++) {
        if (level.scene.movers[i].inst && level.scene.movers[i].door && !level.scene.movers[i].lock
            && !level.scene.movers[i].teleport) {
            door = i;
            break;
        }
    }
    check_expect(door >= 0, "save: no door to open");
    door_key = level.scene.movers[door].key;
    mover_set_end(&level.scene.movers[door], 1);

    /* A creature dead, at level 7, with a thing taken off it. */
    body = 0;
    for (s = 0; s < stream_slots() && !body; s++) {
        Entity *ents;
        int first;
        int n;

        ents = stream_slot_entities(s, &first, &n);
        for (i = 0; ents && i < n && !body; i++) {
            const EnemyKind *kind;

            kind = ents[i].live ? enemy_kind(dunstream_kind(&ents[i], 0)) : 0;
            if (kind && loot_chance(kind->loot[0]) && ents[i].pose[ENTITY_DEAD].w[0]) {
                body = &ents[i];
            }
        }
    }
    check_expect(body != 0, "save: no creature to kill");
    if (!body) {
        dunstream_close();
        return;
    }
    body->dead = 1;
    body_key = body->key;
    keep_get(body_key, KEEP_ENTITY)->game[COMBAT_KEEP_LEVEL] = 7;
    found = loot_open(body, 7, &on_it, origin);
    if (found > 0) {
        loot_taken(body, origin[0]);
    }

    /* A pile dropped. */
    inventory_clear(&things);
    item_make(&it, ITEM_HELM, MATERIAL_DAEDRIC);
    inventory_add(&things, &it);
    {
        float feet[3];

        feet[0] = x + 30.0f;
        feet[1] = y - WALK_EYE;
        feet[2] = z;
        check_expect(pile_drop(&things, feet), "save: the pile had nowhere to go");
    }
    pile = 0;
    for (i = 0; i < level.scene.nentities; i++) {
        if (level.scene.entities[i].live && dunstream_spare(&level.scene.entities[i])) {
            pile = (Entity *)&level.scene.entities[i];
        }
    }
    check_expect(pile != 0, "save: no pile stands");
    pile_key = pile ? pile->key : body_key;

    /* What the game writes: its own state, then the loaded blocks flushed and the table with it. */
    mem_zero(&out, sizeof(out));
    check_expect(character_new(&out.sheet, RACE_KHAJIIT, CLASS_MAGE, 1, "Kept"), "save: no character");
    out.sheet.level = 3;
    out.sheet.health = 7;
    out.sheet.face = 4;
    out.sheet.skill_uses[5] = 123;
    out.sheet.attribute[ATTR_LUCK] = 99;
    inventory_clear(&out.pack);
    item_make(&it, ITEM_LONGSWORD, MATERIAL_EBONY);
    inventory_equip(&out.pack, inventory_add(&out.pack, &it));
    item_make(&it, ITEM_CUIRASS, MATERIAL_CHAIN);
    inventory_wear(&out.pack, inventory_add(&out.pack, &it));
    item_make(&it, ITEM_ARROW, MATERIAL_IRON);
    it.count = 17;
    inventory_add(&out.pack, &it);
    out.pack.gold = 4321;
    out.feet[0] = 111.5f;
    out.feet[1] = 22.5f;
    out.feet[2] = 333.5f;
    out.yaw = 1.5f;
    out.pitch = -0.25f;
    out.view_yaw = 0.6f;
    out.view_pitch = -0.2f;
    out.minute = 600.0f;
    out.day = 12345;
    out.day_zero = 12000;
    out.seed = loot_seed();
    stream_keep_now();
    check_expect(keep_find(door_key) != 0, "save: a door open in a loaded block was not flushed");
    /* The list's words and a snapshot of a known pattern, and an id for the character. */
    out.sheet.id = 0xC0FFEEu;
    df_copy(out.where_words, "Privateer's Hold", SAVE_WHERE);
    for (i = 0; i < SAVE_THUMB_W * SAVE_THUMB_H; i++) {
        g_test_thumb[i] = (df_u16)(i * 7 + 3);
    }
    out.thumb = g_test_thumb;
    check_expect(save_write(&out) && save_exists(), "save: it did not write");
    dunstream_close();

    /* Everything wiped, as another game would have left it. */
    keep_clear();
    pile_new_game();
    loot_new_game(1u);
    mem_zero(&in, sizeof(in));
    check_expect(save_read(&in), "save: it did not read back");
    check_expect(strcmp(in.sheet.name, "Kept") == 0 && in.sheet.race == RACE_KHAJIIT && in.sheet.klass == CLASS_MAGE && in.sheet.female
                     && in.sheet.level == 3 && in.sheet.health == 7 && in.sheet.face == 4 && in.sheet.skill_uses[5] == 123
                     && in.sheet.attribute[ATTR_LUCK] == 99 && in.sheet.skill[SKILL_MEDICAL] == out.sheet.skill[SKILL_MEDICAL],
                 "save: the sheet did not come back");
    check_expect(in.pack.count == 3 && in.pack.gold == 4321 && inventory_weapon(&in.pack) == &in.pack.item[0]
                     && in.pack.item[0].material == MATERIAL_EBONY && in.pack.worn[EQUIP_CHEST] == 1
                     && in.pack.item[2].count == 17 && check_near(in.pack.item[0].weight, out.pack.item[0].weight, 0.001f),
                 "save: the pack did not come back");
    check_expect(check_near(in.feet[0], 111.5f, 0.01f) && check_near(in.feet[1], 22.5f, 0.01f) && check_near(in.feet[2], 333.5f, 0.01f)
                     && check_near(in.yaw, 1.5f, 0.001f) && check_near(in.pitch, -0.25f, 0.001f)
                     && check_near(in.minute, 600.0f, 0.01f) && in.day == 12345 && in.day_zero == 12000,
                 "save: the place or the clock did not come back");
    check_expect(check_near(in.view_yaw, 0.6f, 0.0001f) && check_near(in.view_pitch, -0.2f, 0.0001f),
                 "save: classic Alt view offsets did not come back");
    check_expect(in.seed == out.seed, "save: the seed did not come back");
    loot_new_game(in.seed);

    /* The hold again: the door open, the body a body missing what was taken, the pile where it was with what it held. */
    if (!dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "save: the hold did not open again");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    for (i = 0; i < level.scene.nmovers; i++) {
        if (level.scene.movers[i].key.place == door_key.place && level.scene.movers[i].key.thing == door_key.thing) {
            break;
        }
    }
    check_expect(i < level.scene.nmovers && mover_at_end(&level.scene.movers[i]), "save: the door came back shut");
    body = entity_by_key(body_key);
    check_expect(body && body->dead, "save: the creature came back alive");
    if (body) {
        check_expect(loot_open(body, 1, &on_it, origin) == (found > 0 ? found - 1 : 0), "save: the body came back with what was taken");
    }
    pile = entity_by_key(pile_key);
    check_expect(pile && pile->live, "save: the pile did not stand again");
    if (pile) {
        check_expect(loot_open(pile, 1, &on_it, origin) == 1 && on_it.item[0].template_id == ITEM_HELM
                         && on_it.item[0].material == MATERIAL_DAEDRIC,
                     "save: the pile came back without its helm");
    }
    dunstream_close();
    keep_clear();
    pile_new_game();
    /* The list reads only the start of a file: who, where and when, and the snapshot. */
    {
        SaveMeta meta;
        static df_u16 thumb_out[SAVE_THUMB_W * SAVE_THUMB_H];
        char path[512];
        int k;
        int same;

        check_expect(save_peek("DATA\\TEST2.SAV", &meta) && meta.char_id == 0xC0FFEEu && strcmp(meta.name, out.sheet.name) == 0
                         && meta.level == out.sheet.level && strcmp(meta.where, "Privateer's Hold") == 0 && meta.has_thumb
                         && meta.year >= 2024 && meta.month >= 1 && meta.month <= 12 && meta.day >= 1 && meta.day <= 31
                         && meta.game_day == out.day,
                     "save: the list's meta did not read from the start of the file");
        memset(thumb_out, 0, sizeof(thumb_out));
        check_expect(save_peek_thumb("DATA\\TEST2.SAV", thumb_out), "save: the snapshot did not read");
        for (k = 0, same = 1; k < SAVE_THUMB_W * SAVE_THUMB_H; k++) {
            same = same && thumb_out[k] == (df_u16)(k * 7 + 3);
        }
        check_expect(same, "save: the snapshot did not come back pixel for pixel");
        check_expect(!save_peek("DATA\\NOSUCH.SAV", &meta) && !save_peek_thumb("DATA\\NOSUCH.SAV", thumb_out), "save: a missing file has a meta or a snapshot");
        /* A save of a class of one's own keeps the class. */
        {
            CareerClass mine;
            SaveState a;
            SaveState b;

            mem_zero(&mine, sizeof(mine));
            df_copy(mine.name, "Kept Class", 16);
            mine.hp_per_level = 21;
            career_set_custom(&mine);
            mem_zero(&a, sizeof(a));
            a = out;
            a.sheet.klass = CLASS_CUSTOM;
            save_use_file("DATA\\TEST4.SAV");
            check_expect(save_write(&a), "save: the custom class's save did not write");
            mine.hp_per_level = 5;
            career_set_custom(&mine);
            check_expect(save_read(&b) && career_class(CLASS_CUSTOM)->hp_per_level == 21, "save: the custom class did not come back");
            check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "save: the custom class's file did not go");
            save_use_file("DATA\\TEST2.SAV");
        }
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST2.SAV") && remove(path) == 0, "save: the test's file did not go");
    }
    check_expect(!save_exists(), "save: a save is there after its file went");
    printf("save: a door, a body, a pile, a sheet and a pack, round the file and back\n");
}

/*
 * The bug this guards: a Giant Bat 269 units over the cave's floor and 26 across
 * the ground kept landing blows, killing a player who never moved in 29 seconds.
 * Reach ignored height, and a flyer stopped sinking once near across the ground.
 * A player stands at the hold's start for a minute and a half with the creatures
 * awake: no blow comes from further than reach, feet to feet.
 */
static void check_blows_in_reach(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    Character sheet;
    Player p;
    Input in;
    int i;

    keep_clear();
    combat_reset();
    foes_reset();
    combat_seed(1234);
    if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "reach: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    character_new(&sheet, RACE_ARGONIAN, CLASS_WARRIOR, 1, "Test");
    player_init(&p, &sheet);
    player_place(&p, x, y, z, yaw, pitch);
    memset(&in, 0, sizeof(in));
    for (i = 0; i < 30 * 90; i++) {
        /* Walls keep sleepers from a player who stands still: every 3 s, on to a creature that has a clear way to them, roused. */
        if (i % (30 * 3) == 0) {
            int slot;
            int moved;

            moved = 0;
            for (slot = 0; slot < stream_slots() && !moved; slot++) {
                Entity *ents;
                int first;
                int n;
                int k;

                ents = stream_slot_entities(slot, &first, &n);
                for (k = 0; ents && k < n && !moved; k++) {
                    float from[3];
                    float to[3];

                    if (!ents[k].live || ents[k].dead || dunstream_kind(&ents[k], 0) < 0) {
                        continue;
                    }
                    from[0] = ents[k].x;
                    from[1] = ents[k].y + 40.0f;
                    from[2] = ents[k].z;
                    to[0] = ents[k].x + 120.0f;
                    to[1] = from[1];
                    to[2] = ents[k].z;
                    /* A turn of creatures: the one numbered by the time, if it has room beside it. */
                    if (((first + k) * 7) % 11 == (i / 90) % 11 && combat_sight(&level.scene, from, to)) {
                        player_place(&p, to[0], from[1] + 28.0f, to[2], p.yaw, 0.0f);
                        foes_alert(&ents[k]);
                        moved = 1;
                    }
                }
            }
        }
        player_step(&p, &in, &level.scene, 1.0f / 30.0f, 1);
        clock_set_minute(clock_minute() + 0.2f / 30.0f);
        foes_frame(&p, &level.scene, 1.0f / 30.0f);
        combat_frame(1.0f / 30.0f);
        /* Whole again each time: the test is of where blows come from, not of dying. */
        if (p.sheet.health <= 0) {
            p.sheet.health = p.sheet.health_max;
        }
    }
    {
        int gathered;
        int dropped;

        /* The last gather of the fight: nothing left out. */
        collide_stats(&gathered, &dropped);
        check_expect(dropped == 0, "reach: a sight ray left walls out of its gather");
    }
    printf("reach: the furthest blow in 90 s at the hold's start came from %.0f units, %d awake\n", foes_blow_reach(), foes_awake());
    check_expect(foes_blow_reach() > 0.0f, "reach: no creature came to strike at all");
    check_expect(foes_blow_reach() <= FOES_MELEE + 10.0f, "reach: a blow came from out of reach");
    foes_reset();
    dunstream_close();
    keep_clear();
}

/* One passage's move as the program makes it: the place left, the next opened, stood in. */
static int travel_go(Level *level, Camera *cam, TravelPose *at)
{
    travel_leave();
    if (!travel_open(level, 2)) {
        check_expect(0, travel_error());
        return 0;
    }
    travel_land(level, cam, WALK_EYE, at);
    return 1;
}

/* The player at the pose, a second to settle: 1 when they stand there, no further than most below it. */
static int stands_at(Player *p, const Level *level, const TravelPose *at, float most)
{
    Input in;
    float start;

    memset(&in, 0, sizeof(in));
    player_place(p, at->x, at->y, at->z, at->yaw, at->pitch);
    start = p->body.feet[1];
    walk(p, &level->scene, &in, 60);
    return p->body.grounded && start - p->body.feet[1] <= most && p->body.feet[1] - start <= most;
}

/* The way out of each of a sample of the map's dungeons: a body put a step from it stands. */
static void check_every_way_out(void)
{
    static DunRef rows[4300];
    Level level;
    Camera cam;
    Player p;
    Character sheet;
    int n;
    int i;
    int tried;
    int bad;

    n = dungeon_list(rows, 4300);
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    player_init(&p, &sheet);
    tried = 0;
    bad = 0;
    for (i = 0; i < n; i += 197) {
        Dungeon dun;
        TravelPose at;
        const Usable *u;

        if (!dungeon_load_at(rows[i].region, rows[i].index, &dun)
            || !dunstream_open(&dun, &level, &at.x, &at.y, &at.z, &at.yaw, &at.pitch)) {
            continue;
        }
        camera_set(&cam, at.x, at.y, at.z, at.yaw, at.pitch);
        dunstream_fill(&cam);
        u = travel_nearest_passage(&level.scene, DOOR_EXIT, at.x, at.z);
        tried++;
        if (u) {
            doors_landing(u, TRAVEL_LAND_OUT, WALK_EYE, &at.x, &at.y, &at.z, &at.yaw);
            at.pitch = 0.0f;
            camera_set(&cam, at.x, at.y, at.z, at.yaw, at.pitch);
            dunstream_update(&cam, 0.0f);
        }
        if (!u || !stands_at(&p, &level, &at, 128.0f)) {
            bad++;
            printf("travel: %s (%d, %d): %s\n", rows[i].name, rows[i].region, rows[i].index,
                   u ? "no footing by its way out" : "no way out by its start");
        }
        dunstream_close();
    }
    check_expect(tried >= 15 && bad == 0, "travel: a dungeon's way out has no footing");
    printf("travel: %d dungeons' ways out stood at, %d without\n", tried, bad);
}

/* Out of the hold, the world, down again, up by the same door; a house and out; saves in each; an origin move. */
static void check_travel(void)
{
    static DunRef towns[16000];
    Level level;
    Camera cam;
    TravelPose at;
    Player p;
    Character sheet;
    TravelWhere here;
    int origin[2];
    const Usable *u;
    double back_x;
    double back_z;
    int region;
    int index;
    int i;

    save_use_file("DATA\\TEST3.SAV");
    keep_clear();
    pile_new_game();
    combat_reset();
    foes_reset();
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    player_init(&p, &sheet);

    /* A new game: the hold, at its start, its way out named by a location it has by name. */
    travel_start_game();
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    travel_here(&here, origin);
    check_expect(here.kind == TRAVEL_DUNGEON && here.located && here.region == 17 && !here.has_back,
                 "travel: a new game is not in the hold");
    check_expect(stands_at(&p, &level, &at, 64.0f), "travel: the player does not stand at the hold's start");
    u = travel_nearest_passage(&level.scene, DOOR_EXIT, at.x, at.z);
    check_expect(u != 0, "travel: the hold has no way out");
    if (!u) {
        travel_leave();
        return;
    }
    check_expect(travel_use(u) == 0 && travel_asked(), "travel: the hold's way out leads nowhere");

    /* The world, on the step outside the hold's way down. */
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    travel_here(&here, origin);
    check_expect(here.kind == TRAVEL_WORLD && !travel_asked(), "travel: out of the hold is not the world");
    u = travel_nearest_passage(&level.scene, DOOR_DUNGEON, at.x, at.z);
    check_expect(u != 0, "travel: the hold's outside has no way down in reach");
    if (!u) {
        travel_leave();
        return;
    }
    {
        float dx;
        float dz;

        dx = u->cx - at.x;
        dz = u->cz - at.z;
        check_expect(dx * dx + dz * dz < 96.0f * 96.0f, "travel: not landed by the hold's way down");
    }
    check_expect(stands_at(&p, &level, &at, 64.0f), "travel: the player does not stand outside the hold");
    /* Nothing of a dungeon's creatures, bodies or heaps to find out here; and it can be walked. */
    {
        Input in;
        float start;

        check_expect(foes_frame(&p, &level.scene, 0.1f) == 0, "travel: a blow in the world");
        check_expect(loot_find(&level.scene, &cam) == 0, "travel: something to search outside");
        memset(&in, 0, sizeof(in));
        in.move_f = 1;
        start = p.body.feet[1];
        walk(&p, &level.scene, &in, 60);
        check_expect(p.body.grounded && p.body.feet[1] > start - 512.0f && p.body.feet[1] < start + 512.0f,
                     "travel: the ground outside the hold does not hold the player");
    }

    /* Down: beside the way out, the way back remembered. */
    u = travel_nearest_passage(&level.scene, DOOR_DUNGEON, at.x, at.z);
    {
        float bx;
        float by;
        float bz;
        float byaw;

        doors_landing(u, TRAVEL_LAND_OUT, 0.0f, &bx, &by, &bz, &byaw);
        stream_world(bx, bz, &back_x, &back_z);
    }
    check_expect(travel_use(u) == 0, "travel: the hold's way down leads nowhere");
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    travel_here(&here, origin);
    check_expect(here.kind == TRAVEL_DUNGEON && here.has_back && here.back_x == back_x && here.back_z == back_z,
                 "travel: down does not remember the way back");
    u = travel_nearest_passage(&level.scene, DOOR_EXIT, at.x, at.z);
    {
        float dx;
        float dz;

        dx = u ? u->cx - at.x : 1000.0f;
        dz = u ? u->cz - at.z : 1000.0f;
        check_expect(dx * dx + dz * dz < 96.0f * 96.0f, "travel: not landed by the hold's way out");
    }
    check_expect(stands_at(&p, &level, &at, 128.0f), "travel: the player does not stand by the hold's way out");
    /* A door opened here stays open across the world. */
    {
        int door;
        KeepKey key;

        door = -1;
        for (i = 0; i < level.scene.nmovers && door < 0; i++) {
            if (level.scene.movers[i].key.place) {
                door = i;
            }
        }
        check_expect(door >= 0, "travel: nothing keyed in the hold");
        if (door >= 0) {
            key = level.scene.movers[door].key;
            mover_start(&level.scene, door);
            for (i = 0; i < 300; i++) {
                mover_step(&level.scene, 0.05f);
            }
            check_expect(travel_use(u) == 0, "travel: the hold's way out leads nowhere the second time");
            if (!travel_go(&level, &cam, &at)) {
                return;
            }
            check_expect(keep_find(key) != 0, "travel: the door opened was not kept on leaving");
        }
    }
    /* Up by the same door. */
    {
        double wx;
        double wz;

        stream_world(at.x, at.z, &wx, &wz);
        check_expect(wx > back_x - 1.0 && wx < back_x + 1.0 && wz > back_z - 1.0 && wz < back_z + 1.0,
                     "travel: up is not at the door gone down");
    }

    /* A save in the world, read again: the same world point, with the same way back. */
    {
        SaveState out;
        SaveState in;
        double wx0;
        double wz0;
        double wx1;
        double wz1;

        check_expect(stands_at(&p, &level, &at, 64.0f), "travel: the player does not stand at the door gone down");
        memset(&out, 0, sizeof(out));
        travel_keep_now();
        travel_here(&out.where, out.origin);
        out.feet[0] = p.body.feet[0];
        out.feet[1] = p.body.feet[1];
        out.feet[2] = p.body.feet[2];
        out.yaw = p.yaw;
        out.sheet = sheet;
        inventory_clear(&out.pack);
        stream_world(p.body.feet[0], p.body.feet[2], &wx0, &wz0);
        check_expect(save_write(&out) && save_read(&in), "travel: a save in the world did not go round");
        check_expect(in.where.kind == TRAVEL_WORLD && in.where.region == out.where.region
                     && in.where.index == out.where.index && in.where.has_back == out.where.has_back
                     && in.where.back_x > out.where.back_x - 0.01 && in.where.back_x < out.where.back_x + 0.01
                     && in.origin[0] == out.origin[0] && in.origin[1] == out.origin[1],
                     "travel: a save does not hold the place");
        travel_from_save(&in.where, in.feet, in.origin, in.yaw, in.pitch);
        if (!travel_go(&level, &cam, &at)) {
            return;
        }
        stream_world(at.x, at.z, &wx1, &wz1);
        check_expect(wx1 > wx0 - 0.01 && wx1 < wx0 + 0.01 && wz1 > wz0 - 0.01 && wz1 < wz0 + 0.01,
                     "travel: a loaded save is not where it was saved");
        check_expect(at.y > in.feet[1] + WALK_EYE - 0.01f && at.y < in.feet[1] + WALK_EYE + 0.01f,
                     "travel: a loaded save is not at the height it was saved");
    }

    /* The stream's origin moves: the body stays where it stands in the world. */
    {
        double wx0;
        double wz0;
        double wx1;
        double wz1;
        float sx;
        float sz;

        /* The player walked 17 chunks east: past where the stream moves its origin. */
        player_place(&p, at.x + (float)(17 * STREAM_CHUNK), at.y, at.z, at.yaw, at.pitch);
        stream_world(p.body.feet[0], p.body.feet[2], &wx0, &wz0);
        cam.x += (float)(17 * STREAM_CHUNK);
        travel_update(&cam, 2, 0.5f, 0.0f, &sx, &sz);
        check_expect(sx == (float)(17 * STREAM_CHUNK) && sz == 0.0f, "travel: no origin move at 17 chunks");
        player_shift(&p, sx, sz);
        stream_world(p.body.feet[0], p.body.feet[2], &wx1, &wz1);
        check_expect(wx1 > wx0 - 0.01 && wx1 < wx0 + 0.01 && wz1 > wz0 - 0.01 && wz1 < wz0 + 0.01,
                     "travel: an origin move took the player with it");
        check_expect(p.body.feet[0] > -STREAM_CHUNK && p.body.feet[0] < 2 * STREAM_CHUNK,
                     "travel: an origin move left the player far from it");
    }
    travel_leave();

    /* Daggerfall: a house and out by the same door. */
    index = -1;
    region = 17;
    i = exterior_list(towns, 16000);
    while (--i >= 0) {
        if (towns[i].region == region && df_eq(towns[i].name, "Daggerfall")) {
            index = towns[i].index;
        }
    }
    check_expect(index >= 0, "travel: no Daggerfall");
    if (index >= 0) {
        TravelWhere where;
        float feet[3];
        int chunk[2];
        int px;
        int py;
        Usable door;

        worldmap_pixel_of(region, index, &px, &py);
        memset(&where, 0, sizeof(where));
        where.kind = TRAVEL_WORLD;
        where.located = 1;
        where.region = region;
        where.index = index;
        where.has_back = 1;
        where.back_x = ((double)px + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
        where.back_z = ((double)(WORLDMAP_H - 1 - py) + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
        chunk[0] = (int)(where.back_x / (double)STREAM_CHUNK);
        chunk[1] = (int)(where.back_z / (double)STREAM_CHUNK);
        feet[0] = (float)(where.back_x - (double)chunk[0] * (double)STREAM_CHUNK);
        feet[1] = 0.0f;
        feet[2] = (float)(where.back_z - (double)chunk[1] * (double)STREAM_CHUNK);
        travel_from_save(&where, feet, chunk, 0.0f, 0.0f);
        if (!travel_go(&level, &cam, &at)) {
            return;
        }
        u = travel_nearest_passage(&level.scene, DOOR_ENTER, at.x, at.z);
        check_expect(u != 0, "travel: no house by Daggerfall's middle");
        if (u) {
            door = *u;
            doors_landing(&door, TRAVEL_LAND_OUT, 0.0f, &at.x, &at.y, &at.z, &at.yaw);
            stream_world(at.x, at.z, &back_x, &back_z);
            check_expect(travel_use(&door) == 0, "travel: Daggerfall's house leads nowhere");
            if (!travel_go(&level, &cam, &at)) {
                return;
            }
            travel_here(&here, origin);
            check_expect(here.kind == TRAVEL_INSIDE && here.region == region && here.index == index
                         && origin[0] == 0 && origin[1] == 0, "travel: the door is not into a house");
            check_expect(stands_at(&p, &level, &at, 64.0f), "travel: the player does not stand in the house");
            /* No stream open: nothing to find, nothing to strike, no blow. */
            check_expect(foes_frame(&p, &level.scene, 0.1f) == 0, "travel: a blow in a house");
            check_expect(loot_find(&level.scene, &cam) == 0, "travel: something to search in a house");
            combat_frame(0.1f);
            {
                float sx;
                float sz;

                travel_update(&cam, 2, 1.0f, 0.1f, &sx, &sz);
                check_expect(sx == 0.0f && sz == 0.0f, "travel: a house has an origin");
            }
            u = 0;
            for (i = 0; i < level.scene.nusables; i++) {
                if (level.scene.usables[i].kind == USE_PASSAGE && level.scene.usables[i].what == DOOR_LEAVE) {
                    u = &level.scene.usables[i];
                }
            }
            check_expect(u != 0, "travel: the house has no way out");
            if (u) {
                double wx;
                double wz;

                check_expect(travel_use(u) == 0, "travel: the house's way out leads nowhere");
                if (!travel_go(&level, &cam, &at)) {
                    return;
                }
                stream_world(at.x, at.z, &wx, &wz);
                check_expect(wx > back_x - 1.0 && wx < back_x + 1.0 && wz > back_z - 1.0 && wz < back_z + 1.0,
                             "travel: out of a house is not at its door");
                check_expect(stands_at(&p, &level, &at, 64.0f), "travel: the player does not stand at the house's door");
            }
        }
        travel_leave();
    }
    {
        char path[260];

        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST3.SAV") && remove(path) == 0,
                     "travel: the test's file did not go");
    }
    keep_clear();
    pile_new_game();
    printf("travel: the hold, its outside, down and up by one door; Daggerfall's house and out; a save in the world; an origin move\n");
}

/* What the screens wrote, for a look at them. */
static void write_screen(const char *path, const df_u8 *pixels)
{
    static df_u8 rgb[768];
    FILE *f;
    int i;

    if (!img_art_palette(rgb)) {
        return;
    }
    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n320 200\n255\n");
    for (i = 0; i < 320 * 200; i++) {
        fwrite(&rgb[pixels[i] * 3], 1, 3, f);
    }
    fclose(f);
}

/* As write_screen, under the travel map's palette when map is 1. */
static void write_screen_with(const char *path, const df_u8 *pixels, int map)
{
    static df_u8 rgb[768];
    FILE *f;
    int i;

    if (map) {
        travelmap_palette(rgb);
    } else if (!img_art_palette(rgb)) {
        return;
    }
    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n320 200\n255\n");
    for (i = 0; i < 320 * 200; i++) {
        fwrite(&rgb[pixels[i] * 3], 1, 3, f);
    }
    fclose(f);
}

/* The factions' file, and reputation spreading through it, by hand. */
static void check_standing(void)
{
    const Faction *people;
    const Faction *f;

    check_expect(factionfile_open() && faction_count() == 366, "people: FACTION.TXT does not hold 366 factions");
    people = faction_people_of(17);
    check_expect(people && df_eq(people->name, "People of Daggerfall") && people->type == FACTION_PEOPLE
                 && people->sgroup == SOCIAL_COMMONERS && people->parent != 0,
                 "people: Daggerfall's people are not as the file has them");
    f = faction_find(1);
    check_expect(f && df_eq(f->name, "Clavicus Vile") && f->parent == 17 && f->enemy[0] == 25 && f->flat_archive[0] == 175
                 && f->flat_record[0] == 2, "people: a child's parent, enemy and flat are not read");
    f = faction_find(3);
    check_expect(f && f->ally[0] == 16 && f->enemy[0] == 25 && f->enemy[1] == 15, "people: Molag Bal's ally and two enemies");
    check_expect(faces_of_flat(175, 0) == FACE_NONE, "people: FLATS.CFG's first flat's face is 410");
    check_expect(faces_of_flat(999, 0) == FACE_NONE, "people: a flat FLATS.CFG does not name has the fallback");

    standing_new_game(77u);
    check_expect(standing_rep(1) == faction_find(1)->rep, "people: a new game's reputation is not the file's");
    /* Clavicus Vile under Oblivion: the whole to it and its root, half to a sibling, half taken from its enemy. */
    standing_change(1, 20, 1);
    check_expect(standing_rep(1) == 20 && standing_rep(17) == 20 && standing_rep(2) == 10 && standing_rep(25) == -10,
                 "people: a reputation's spread is not the whole, the root, half and half off");
    standing_change(1, 500, 0);
    check_expect(standing_rep(1) == STANDING_MOST, "people: a reputation is not clamped");
    check_expect(standing_answers_for(people->id) == people, "people: the people answer for themselves");
    standing_new_game(77u);
    standing_social_change(SOCIAL_COMMONERS, 12);
    standing_set_reaction_bonus(3);
    standing_change(people->id, 15, 0);
    check_expect(standing_reaction(people->id) == 12 + 3 + 15, "people: a reaction is not reputation, group and bonus");
    check_expect(standing_region(17)->prices >= 750 && standing_region(17)->prices <= 1250,
                 "people: a region's prices are not rolled within 750..1250");
    printf("standing: %d factions; reputation spreads whole to the root, half to the rest, half off an enemy\n", faction_count());
}

/* The sums worked by hand from DFU's formulas. */
static void check_people_sums(void)
{
    /* Quality 10, prices at par: twice the value. */
    check_expect(rules_cost(100, 10, 1000) == 200, "people: a middling shop's cost is not twice the value");
    /* Quality 20: a tenth on, then doubled. Quality 0: a tenth off, then doubled. */
    check_expect(rules_cost(100, 20, 1000) == 220 && rules_cost(100, 0, 1000) == 180, "people: quality moves a cost a tenth each way");
    check_expect(rules_cost(100, 10, 750) == 150, "people: a region's prices do not move a cost");
    check_expect(rules_cost(0, 10, 1000) == 2, "people: nothing costs under 1 before doubling");
    /*
     * Middling shop, skills 50. Each term (64 + 128) * (64 + 128) / 256 = 144.
     * Buying: 192 * 144 / 256 = 108, and (144 * 64) / 256 = 36: 144 / 256 of 200 is 112.
     * Selling: 179 * 144 / 256 = 100, 51 * 144 / 256 = 28: 128 / 256 of 200 is 100.
     */
    check_expect(rules_trade_price(200, 10, 50, 50, 0) == 112, "people: a middling buy is not 112");
    check_expect(rules_trade_price(200, 10, 50, 50, 1) == 100, "people: a middling sale is not 100");
    check_expect(rules_trade_price(200, 10, 100, 100, 0) < 112 && rules_trade_price(200, 10, 100, 100, 1) > 100,
                 "people: skill does not cheapen a buy and better a sale");
    check_expect(rules_trade_price(200, 20, 50, 50, 0) > 112 && rules_trade_price(200, 20, 50, 50, 1) < 100,
                 "people: a fine shop does not ask more and give less");
    /* A tenth of the value, costed: 10 becomes 20. */
    check_expect(rules_repair_cost(100, 10, 50, 100, 1000) == 20, "people: a repair is not a tenth of the value, costed");
    check_expect(rules_repair_cost(100, 10, 100, 100, 1000) == 0, "people: mending the unworn costs");
    check_expect(rules_shoplift_chance(30, 10, 5) == 85, "people: 100 - 30 + 10 + 5 seen");
    check_expect(rules_shoplift_chance(100, 0, 0) == 5 && rules_shoplift_chance(0, 20, 50) == 95, "people: seen is not within 5..95");
    check_expect(rules_lock_chance(50, 5) == 25, "people: a lock's chance is not the skill less five a point");
    check_expect(rules_max_loan(3) == 150000 && rules_loan_repayment(1000) == 1100 && rules_letter_cost(1000) == 1010,
                 "people: a loan's cap, a loan's repayment or a letter's fee");
    /* Personality 50, reaction 10, a place: 10 + 10 + 5. */
    check_expect(rules_talk_value(50, 1, 10, RULES_TONE_NORMAL, SOCIAL_COMMONERS, 50, 0) == 25, "people: a plain question's value");
    /* Polite to a commoner, skill passed: -10 + 5. Blunt to one, skill failed: +10 - 10. */
    check_expect(rules_talk_value(50, 0, 0, RULES_TONE_POLITE, SOCIAL_COMMONERS, 50, 10) == 10 + -10 + 5, "people: polite to a commoner");
    check_expect(rules_talk_value(50, 0, 0, RULES_TONE_BLUNT, SOCIAL_COMMONERS, 50, 90) == 10 + 10 - 10, "people: blunt to a commoner");
    check_expect(rules_talk_outcome(5, 10) == 0 && rules_talk_outcome(15, 10) == 1 && rules_talk_outcome(31, 10) == 2,
                 "people: under the roll, within 20 over it, past that");
    check_expect(rules_talk_knows(SOCIAL_MERCHANTS, 17) && !rules_talk_knows(SOCIAL_MERCHANTS, 18), "people: a merchant knows by 7 + 10");
    check_expect(rules_compass(1.0f, 0.0f) == 0 && rules_compass(0.0f, 1.0f) == 2 && rules_compass(-1.0f, 0.0f) == 4
                 && rules_compass(0.0f, -1.0f) == 6 && rules_compass(1.0f, 1.0f) == 1 && rules_compass(-1.0f, -1.0f) == 5,
                 "people: the compass's eighths");
    check_expect(rules_sentence(200, 0, 5, 1000, -40) == 200 && rules_sentence(300, 5, 50, 1000, -40) == 500
                 && rules_sentence(10000, 10, 9000, 12000, 500) == 12000, "people: a sentence's sums and its limits");
    check_expect(building_open_at(BUILDING_BANK, 8) && !building_open_at(BUILDING_BANK, 15) && building_open_at(BUILDING_TAVERN, 3)
                 && !building_open_at(BUILDING_HOUSE1, 12) && building_is_shop(BUILDING_ARMORER) && !building_is_shop(BUILDING_BANK),
                 "people: the buildings' hours or shops");
    check_expect(crime_kind(CRIME_THEFT) && crime_kind(CRIME_THEFT)->loss == 8 && df_eq(crime_kind(CRIME_THEFT)->name, "theft"),
                 "people: theft's record");
}

/* What people say: no macro left in the words, whichever variant comes. */
static void check_words(void)
{
    static const int records[] = { 7206, 7207, 7208, 7209, 7215, 7218, 7225, 7231, 7333, 7250, 7251, 7266, 7281, 7304, 7261, 7294, 8050, 8055, 8062, 8063 };
    Character sheet;
    Words w;
    char out[512];
    int r;
    int i;

    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada Lovel");
    mem_zero(&w, sizeof(w));
    w.pc = &sheet;
    w.key = "the bank";
    w.loc = "the bank";
    w.direction = "north";
    w.hint = "it is north";
    w.city = "Daggerfall";
    w.region = "Daggerfall";
    w.crime = "theft";
    w.penalty = "a fine";
    w.tone = RULES_TONE_NORMAL;
    w.reaction = 5;
    words_use(&w);
    for (r = 0; r < (int)(sizeof(records) / sizeof(records[0])); r++) {
        check_expect(textrsc_variants(records[r]) >= 1, "words: a record of talk does not read");
        for (i = 0; i < 20; i++) {
            check_expect(words_say(records[r], out, (int)sizeof(out)) > 0, "words: a record says nothing");
            check_expect(strchr(out, '%') == 0, "words: a macro is left in what is said");
        }
    }
    /* Every variant of a two-variant record comes up by its number, and a number past wraps round. */
    {
        char a[64];
        char b[64];

        check_expect(textrsc_variants(7221) == 2, "words: 7221 has two variants");
        textrsc_variant(7221, 0, a, (int)sizeof(a));
        textrsc_variant(7221, 2, b, (int)sizeof(b));
        check_expect(strcmp(a, b) == 0, "words: a variant's number does not wrap");
        textrsc_variant(7221, 1, b, (int)sizeof(b));
        check_expect(strcmp(a, b) != 0, "words: the variants are one");
    }
    words_expand("%pcf the %ra", out, (int)sizeof(out));
    check_expect(strcmp(out, "Ada the Breton") == 0, "words: the first name and the race");
    words_use(0);
}

/* A building of this type in a town, as a door names it; -1 for none. */
static int building_of_type(int region, int index, int type, int *block, int *record)
{
    const TownBuilding *rows;
    int count;
    int i;

    rows = town_buildings(region, index, &count);
    for (i = 0; rows && i < count; i++) {
        if (rows[i].type == type) {
            *block = rows[i].block;
            *record = rows[i].record;
            return i;
        }
    }
    return -1;
}

/* Daggerfall's index in its region's list. */
static int daggerfall_index(void)
{
    static DunRef towns[16000];
    int i;

    i = exterior_list(towns, 16000);
    while (--i >= 0) {
        if (towns[i].region == 17 && df_eq(towns[i].name, "Daggerfall")) {
            return towns[i].index;
        }
    }
    return -1;
}

/* The talk window, by clicks: a town's list, a question asked, one who will not answer. */
static void check_talk(void)
{
    static const Rect okay = { 4, 186, 107, 10 };
    static const Rect goodbye = { 118, 183, 67, 10 };
    Player p;
    Character sheet;
    TalkWho who;
    const Faction *people;
    char line[64];
    int index;
    int count;
    int before;
    int i;
    int found;

    index = daggerfall_index();
    check_expect(index >= 0, "talk: no Daggerfall");
    if (index < 0) {
        return;
    }
    town_buildings(17, index, &count);
    check_expect(count > 100 && df_eq(town_name(), "Daggerfall"), "talk: Daggerfall's buildings did not read");
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada Lovel");
    player_init(&p, &sheet);
    standing_new_game(5u);
    people = faction_people_of(17);
    mem_zero(&who, sizeof(who));
    who.faction = people->id;
    who.face = faces_of_flat(people->flat_archive[0], people->flat_record[0]);
    who.seed = 1234u;
    who.located = 1;
    who.region = 17;
    who.index = index;
    who.x = 16000.0f;
    who.z = 16000.0f;
    who.block = -1;
    who.record = -1;
    who.city = "Daggerfall";
    who.region_name = "Daggerfall";
    check_expect(talk_open(&p, &who), "talk: it did not open");
    /* A city has its banks and its smiths; a house is not asked after. */
    found = 0;
    for (i = 0; i < talk_topics(); i++) {
        found += df_eq(talk_topic(i), "Bank") || df_eq(talk_topic(i), "Armorer");
        check_expect(!df_eq(talk_topic(i), "House"), "talk: a house is listed");
    }
    check_expect(found == 2 && talk_lines() > 0, "talk: Daggerfall has no bank or armorer listed, or nobody greeted");
    /* The first of the list chosen, asked by Okay: more lines, none with a macro left. */
    before = talk_lines();
    check_expect(talk_click(10, 73) == TALK_STAY, "talk: choosing is leaving");
    check_expect(talk_click(okay.x + 1, okay.y + 1) == TALK_STAY, "talk: asking is leaving");
    check_expect(talk_lines() > before, "talk: asking said nothing");
    for (i = 0; i < talk_lines(); i++) {
        check_expect(strchr(talk_line(i), '%') == 0, "talk: a macro is left in the conversation");
    }
    talk_compose();
    write_screen("build/talk0.ppm", talk_pixels());
    /* Tell me about: the news, once, then none. */
    talk_click(10, 6);
    check_expect(talk_topics() == 1, "talk: tell me about has the news alone");
    talk_click(10, 73);
    before = talk_lines();
    talk_click(okay.x + 1, okay.y + 1);
    check_expect(talk_lines() > before, "talk: no news said");
    talk_compose();
    write_screen("build/talk1.ppm", talk_pixels());
    /* The wheel over the conversation: down to the end at most, back to the top at least. */
    talk_wheel(200, 100, -1000);
    talk_wheel(200, 100, 1000);
    talk_compose();
    check_expect(talk_click(goodbye.x + 1, goodbye.y + 1) == TALK_LEAVE, "talk: Goodbye did not leave");
    talk_close();
    /* Hated: no response, whatever is asked. */
    standing_change(people->id, -100, 0);
    check_expect(talk_open(&p, &who), "talk: it did not open for one who is hated");
    textrsc_read(7205, line, (int)sizeof(line));
    check_expect(strcmp(talk_line(0), line) == 0, "talk: someone who hates the player greets them");
    talk_close();
    standing_new_game(5u);
    printf("talk: Daggerfall's %d buildings, a way asked, the news; one who hates says nothing; build/talk0..1.ppm\n", count);
}

/* A shop: its stock for a day, what it deals in, buying and selling by clicks, a theft seen; the court after it. */
static void check_shop_and_court(void)
{
    static const Rect action = { 226, 134, 31, 14 };
    static const Rect steal = { 226, 112, 31, 14 };
    Player p;
    Character sheet;
    Shop shop;
    Inventory stock;
    Inventory again;
    df_u8 origin[INVENTORY_MAX];
    df_u8 origin2[INVENTORY_MAX];
    Item it;
    int price;
    int count;
    int i;

    keep_clear();
    standing_new_game(5u);
    loot_new_game(4242u);
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada Lovel");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    mem_zero(&shop, sizeof(shop));
    shop.type = BUILDING_ARMORER;
    shop.quality = 10;
    shop.prices = 1000;
    shop.key.place = (3u << 30) | 123u;
    shop.key.thing = 0xFE000000u;

    /* Today's stock: the same each look, and something on the shelf; a house has none. */
    shop_stock(&shop, 1, 100, &stock, origin);
    shop_stock(&shop, 1, 100, &again, origin2);
    check_expect(stock.count > 3 && stock.count == again.count && stock.item[0].template_id == again.item[0].template_id
                 && stock.item[0].material == again.item[0].material, "shop: an armorer's stock is not there, or not the same each look");
    /* Each trade's own goods, over many days: an armorer mostly armour, a smith mostly weapons, a general store no armour. */
    {
        static const int types[3] = { BUILDING_ARMORER, BUILDING_WEAPONS, BUILDING_GENERAL };
        int armor[3];
        int weapons[3];
        int t;
        int day;

        for (t = 0; t < 3; t++) {
            Shop one;

            one = shop;
            one.type = types[t];
            one.key.thing = 0xFE000001u + (df_u32)t;
            armor[t] = 0;
            weapons[t] = 0;
            for (day = 1000; day < 1040; day++) {
                shop_stock(&one, 1, day, &again, origin2);
                for (i = 0; i < again.count; i++) {
                    armor[t] += armor_template(again.item[i].template_id) != 0;
                    weapons[t] += armor_template(again.item[i].template_id) == 0;
                }
            }
        }
        check_expect(armor[0] > weapons[0] * 2, "shop: an armorer does not stock mostly armour");
        check_expect(weapons[1] > armor[1], "shop: a weapon smith does not stock mostly weapons");
        check_expect(armor[2] == 0 && weapons[2] > 0, "shop: a general store's goods");
    }
    /* A thing taken stays gone today; tomorrow is a new stock. */
    shop_taken(&shop, origin[0]);
    shop_stock(&shop, 1, 100, &again, origin2);
    check_expect(again.count == stock.count - 1, "shop: a thing taken is back on the shelf");
    shop_stock(&shop, 1, 101, &again, origin2);
    check_expect(again.count > 0, "shop: a new day stocks nothing");
    shop_stock(&shop, 1, 100, &stock, origin);
    {
        Shop bank;

        bank = shop;
        bank.type = BUILDING_BANK;
        shop_stock(&bank, 1, 100, &again, origin2);
        check_expect(again.count == 0, "shop: a bank has weapons on its shelves");
    }
    /* What it deals in. */
    check_expect(item_make(&it, ITEM_LONGSWORD, 0), "shop: no longsword");
    check_expect(shop_buys(&shop, &it), "shop: an armorer will not buy a sword");
    {
        Shop other;

        other = shop;
        other.type = BUILDING_BOOKSELLER;
        check_expect(!shop_buys(&other, &it), "shop: a bookseller buys a sword");
    }

    /* Buying by clicks: the shelf's first thing, paid for, at the sums'. Gold weighs: not a fortune. */
    p.pack.gold = 4000u;
    check_expect(bag_open_trade(&p, &shop, TRADE_BUY, 100), "shop: its screen did not open");
    bag_compose();
    write_screen("build/shop0.ppm", bag_pixels());
    {
        int before_gold;

        before_gold = (int)p.pack.gold;
        /* The right list's first cell. */
        bag_click(275, 60);
        bag_compose();
        write_screen("build/shop1.ppm", bag_pixels());
        check_expect(bag_click(action.x + 1, action.y + 1) == BAG_STAY, "shop: buying closed the screen");
        check_expect(p.pack.count == 1, "shop: nothing was bought");
        if (p.pack.count == 1) {
            price = shop_trade_price(&shop, shop_cost(&shop, &p.pack.item[0]), &p.sheet, 0);
            check_expect((int)p.pack.gold == before_gold - price, "shop: the price paid is not the sums'");
            check_expect(p.pack.item[0].template_id == stock.item[0].template_id, "shop: not the thing clicked");
        }
    }
    bag_close();
    shop_stock(&shop, 1, 100, &again, origin2);
    check_expect(again.count == stock.count - 1, "shop: a thing bought is back on the shelf");

    /* The wheel: a list of more than a screen rolls by notches, within its ends. The top row's item is what a click gets. */
    {
        Player many;
        int k;
        int first;

        many = p;
        inventory_clear(&many.pack);
        for (k = 0; k < 8; k++) {
            check_expect(item_make(&it, k < 4 ? ITEM_DAGGER : ITEM_LONGSWORD, 0) && inventory_add(&many.pack, &it) >= 0, "wheel: no blades");
        }
        check_expect(bag_open_trade(&many, &shop, TRADE_SELL, 100), "wheel: no screen");
        bag_wheel(177, 60, -1);
        /* One notch down: the first row is now the pack's second, so a click there lays that one on the counter. */
        bag_click(177, 60);
        first = many.pack.count;
        check_expect(first == 7 && many.pack.item[0].template_id == ITEM_DAGGER, "wheel: no scroll by a notch");
        bag_wheel(177, 60, 1000);
        bag_wheel(177, 60, -1000);
        bag_compose();
        bag_close();
        check_expect(many.pack.count == 8, "wheel: what was on the counter did not come back");
    }

    /* Selling by clicks: the thing from the pack, at the sums'. */
    {
        int before_gold;
        int got;

        before_gold = (int)p.pack.gold;
        check_expect(bag_open_trade(&p, &shop, TRADE_SELL, 100), "shop: its selling screen did not open");
        got = shop_trade_price(&shop, shop_cost(&shop, &p.pack.item[0]), &p.sheet, 1);
        /* The left list's first cell. */
        bag_click(177, 60);
        check_expect(p.pack.count == 0, "shop: the thing did not go on the counter");
        bag_click(action.x + 1, action.y + 1);
        check_expect(p.pack.count == 0 && (int)p.pack.gold == before_gold + got, "shop: the sale is not the sums'");
        bag_close();
    }
    /* A closed screen gives back what was laid on the counter and not sold. */
    {
        check_expect(item_make(&it, ITEM_LONGSWORD, 0) && inventory_add(&p.pack, &it) >= 0, "shop: no sword to carry");
        bag_open_trade(&p, &shop, TRADE_SELL, 100);
        bag_click(177, 60);
        check_expect(p.pack.count == 0, "shop: the sword is not on the counter");
        bag_close();
        check_expect(p.pack.count == 1, "shop: what was not sold did not come back");
    }
    /* Repair: its worn sword, mended and paid for. */
    {
        int before_gold;
        int fee;

        p.pack.item[0].condition = (df_u16)(p.pack.item[0].condition_max / 2);
        before_gold = (int)p.pack.gold;
        fee = shop_trade_price(&shop, shop_repair_cost(&shop, &p.pack.item[0]), &p.sheet, 0);
        check_expect(fee > 0, "shop: a repair is free");
        bag_open_trade(&p, &shop, TRADE_REPAIR, 100);
        bag_click(177, 60);
        bag_click(action.x + 1, action.y + 1);
        bag_close();
        check_expect(p.pack.count == 1 && p.pack.item[0].condition == p.pack.item[0].condition_max
                     && (int)p.pack.gold == before_gold - fee, "shop: the repair is not the sums'");
    }
    /* Theft, by the dice until seen: Pickpocket 0 at a shop that sees nearly always. */
    p.sheet.skill[SKILL_PICKPOCKET] = 0;
    shop.quality = 20;
    combat_seed(99u);
    bag_open_trade(&p, &shop, TRADE_BUY, 100);
    bag_click(275, 60);
    check_expect(bag_click(steal.x + 1, steal.y + 1) == BAG_LEAVE, "shop: Steal did not end the visit");
    check_expect(bag_stole(), "shop: a thief at 95% was not seen");
    bag_close();
    check_expect(p.pack.count == 1, "shop: a thief seen kept the goods");

    /* The court: theft's loss, half to the people; its sums; guilty pays half. A good name first: a low one can mean banishment. */
    standing_region(17)->legal = 50;
    count = standing_region(17)->legal;
    i = standing_rep(faction_people_of(17)->id);
    court_commit(CRIME_THEFT, 17);
    check_expect(court_pending() && standing_region(17)->legal == count - 8 && standing_rep(faction_people_of(17)->id) == i - 4,
                 "court: a crime's cost to the law and the people");
    {
        Sentence s;

        /* 200 gold's worth of 40 is five pieces: any split comes to five, a piece 40 gold or 3 days. */
        court_sentence(CRIME_THEFT, 0, 100000, &s);
        check_expect(s.fine / 40 + s.days / 3 == 5 && !s.banish, "court: theft is not five pieces");
        /* What cannot be paid is served. */
        court_sentence(CRIME_THEFT, 0, 0, &s);
        check_expect(s.fine == 0 && s.days >= 5, "court: no gold, no fine, and days for it");
    }
    p.pack.gold = 4000u;
    check_expect(court_open(&p), "court: it did not open");
    check_expect(court_words()[0] && strchr(court_words(), '%') == 0, "court: the plea's words have a macro left");
    check_expect(strstr(court_words(), "theft") != 0, "court: the plea does not name the crime");
    court_compose();
    write_screen("build/court0.ppm", court_pixels());
    {
        int fine;
        int days;
        int minute_day;

        fine = court_given()->fine;
        days = court_given()->days;
        minute_day = clock_day();
        /* Guilty: half of each. */
        check_expect(court_click(100, 135) == COURT_STAY, "court: Guilty ended it at once");
        check_expect(court_given()->fine == fine / 2 && court_given()->days == days / 2, "court: guilty is not half");
        check_expect((int)p.pack.gold == 4000 - fine / 2, "court: the fine was not taken");
        court_compose();
        write_screen("build/court1.ppm", court_pixels());
        /* Past the sentence's words: prison counts down its days, then the street. */
        if (days / 2 > 0) {
            check_expect(court_click(150, 135) == COURT_STAY, "court: prison was skipped");
            court_compose();
            write_screen("build/court2.ppm", court_pixels());
            check_expect(!court_pending() || court_step(0.0f) == COURT_STAY, "court: prison is over before it begins");
            for (i = 0; i < days / 2 + 1 && court_step(0.3f) == COURT_STAY; i++) {
            }
            check_expect(clock_day() == minute_day + days / 2, "court: the days were not served");
        } else {
            check_expect(court_click(150, 135) == COURT_LEAVE, "court: the fine paid does not end it");
        }
    }
    court_close();
    check_expect(!court_pending(), "court: the crime is still on the books");
    /* A sentence done gives some back: theft's 8 less, 8 / 2 - 1 = 3 back. */
    check_expect(standing_region(17)->legal == count - 8 + 3, "court: a sentence done gives nothing back");
    keep_clear();
    standing_new_game(5u);
    printf("shop and court: stock by day, buying, selling and mending at the sums, a thief seen, guilty's half; build/shop0..1.ppm, court0..2.ppm\n");
}

/* The bank: its sums, then by clicks and keys. */
static void check_bank(void)
{
    Player p;
    Character sheet;
    RegionStanding *r;

    standing_new_game(5u);
    court_forget();
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada Lovel");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    p.pack.gold = 1000u;
    r = standing_region(17);
    check_expect(bank_deposit(&p, 17, 2000) == BANK_NOT_ENOUGH_GOLD, "bank: a deposit past what is carried");
    check_expect(bank_deposit(&p, 17, 600) == BANK_DONE && p.pack.gold == 400u && r->gold == 600, "bank: a deposit's sums");
    check_expect(standing_region(18)->gold == 0, "bank: accounts are not the region's");
    check_expect(bank_withdraw(&p, 17, 700) == BANK_NOT_ENOUGH_ACCOUNT, "bank: withdrawn past the account");
    check_expect(bank_withdraw(&p, 17, 100) == BANK_DONE && p.pack.gold == 500u && r->gold == 500, "bank: a withdrawal's sums");
    check_expect(bank_withdraw(&p, 17, 0) == BANK_DONE, "bank: nothing withdrawn");
    /* A letter: 1% to write; its worth back when paid in. */
    check_expect(bank_write_letter(&p, 17, 50) == BANK_LETTER_TOO_SMALL, "bank: a letter under 100");
    check_expect(bank_write_letter(&p, 17, 499) == BANK_NOT_ENOUGH_ACCOUNT, "bank: a letter past its fee");
    check_expect(bank_write_letter(&p, 17, 300) == BANK_DONE && r->gold == 500 - 303 && p.pack.count == 1
                 && p.pack.item[0].template_id == ITEM_LETTER && p.pack.item[0].value == 300u, "bank: a letter's sums");
    check_expect(bank_deposit_letters(&p, 17) == BANK_DONE && p.pack.count == 0 && r->gold == 497, "bank: a letter paid in");
    /* A loan: limits, once, a tenth more, a year. */
    check_expect(bank_borrow(&p, 17, 50, 10) == BANK_LOAN_TOO_LOW, "bank: a loan under 100");
    check_expect(bank_borrow(&p, 17, rules_max_loan(1) + 1, 10) == BANK_LOAN_TOO_HIGH, "bank: a loan past the level's");
    check_expect(bank_borrow(&p, 17, 1000, 10) == BANK_DONE && r->gold == 1497 && r->loan == 1100 && r->due_day == 370, "bank: a loan's sums");
    check_expect(bank_borrow(&p, 17, 1000, 10) == BANK_ALREADY_LOAN, "bank: a second loan");
    /* Repaying: more than is owed takes what is owed, carried gold first. */
    check_expect(bank_repay(&p, 17, 100) == BANK_DONE && p.pack.gold == 400u && r->loan == 1000, "bank: carried gold pays first");
    check_expect(bank_repay(&p, 17, 1000000) == BANK_NOT_ENOUGH_GOLD, "bank: repaid past what there is");
    check_expect(bank_repay(&p, 17, 1000) == BANK_DONE && p.pack.gold == 0u && r->gold == 897 && r->loan == 0 && r->due_day == 0,
                 "bank: the rest of a loan from the account");
    check_expect(bank_repay(&p, 17, 1) == BANK_NO_LOAN, "bank: repaid with no loan");
    /* Unpaid past its day: a default, a crime in its region. */
    bank_borrow(&p, 17, 100, 10);
    check_expect(bank_check_loans(370) == -1 && !court_pending(), "bank: a default on the day itself");
    check_expect(bank_check_loans(371) == 17 && r->defaulted && court_pending(), "bank: no default past the day");
    check_expect(bank_borrow(&p, 17, 100, 400) == BANK_DEFAULTED, "bank: a loan after a default");
    check_expect(bank_check_loans(372) == -1, "bank: one default is two crimes");
    court_forget();
    /* The screen: Deposit by click, an amount by keys. */
    standing_new_game(5u);
    r = standing_region(17);
    p.pack.gold = 1000u;
    check_expect(bank_open(&p, 17), "bank: its screen did not open");
    /* The Deposit button, inside the panel at 48, 5. */
    check_expect(bank_click(48 + 121, 5 + 59) == BANK_STAY, "bank: Deposit left");
    bank_key('2');
    bank_key('5');
    bank_key('0');
    bank_key(0x0D);
    check_expect(r->gold == 250 && p.pack.gold == 750u, "bank: a deposit by the screen");
    bank_compose();
    write_screen("build/bank0.ppm", bank_pixels());
    check_expect(bank_click(48 + 93, 5 + 160) == BANK_LEAVE, "bank: Exit did not leave");
    bank_close();
    /* The merchant's panel over it. */
    check_expect(service_open(SERVICE_PANEL_BANK), "service: it did not open");
    check_expect(service_click(95 + 6, 56 + 6) == SERVICE_TALK && service_click(95 + 6, 56 + 15) == SERVICE_BANK
                 && service_click(95 + 45, 56 + 25) == SERVICE_LEAVE && service_click(0, 0) == SERVICE_STAY,
                 "service: a bank's panel's rows");
    service_compose();
    write_screen("build/service0.ppm", service_pixels());
    service_close();
    check_expect(service_open(SERVICE_PANEL_REPAIR), "service: a smith's did not open");
    check_expect(service_click(95 + 6, 47 + 6) == SERVICE_REPAIR && service_click(95 + 6, 47 + 15) == SERVICE_TALK
                 && service_click(95 + 6, 47 + 24) == SERVICE_SELL && service_click(95 + 45, 47 + 34) == SERVICE_LEAVE,
                 "service: a smith's panel's rows");
    service_close();
    /* A tavern's: Room asks the days (one to start), typed and Entered; Food lists the menu, a line clicked. */
    check_expect(service_open(SERVICE_PANEL_TAVERN), "service: a tavern's did not open");
    check_expect(service_click(95 + 6, 50 + 5 + 2) == SERVICE_STAY, "service: a tavern's Room row");
    service_key(0x08);
    service_key('3');
    check_expect(service_key(0x0D) == SERVICE_ROOM && service_days() == 3, "service: three days typed for a room");
    check_expect(service_click(95 + 6, 50 + 14 + 2) == SERVICE_TALK, "service: a tavern's Talk row");
    check_expect(service_click(95 + 6, 50 + 23 + 2) == SERVICE_STAY, "service: a tavern's Food row");
    service_compose();
    write_screen("build/tavern0.ppm", service_pixels());
    check_expect(service_click(95 + 20, 98 + 6 + 3 * 8 + 2) == SERVICE_FOOD && service_food() == 3, "service: the menu's fourth line is not wine");
    check_expect(service_click(95 + 6, 50 + 32 + 2) == SERVICE_LEAVE, "service: a tavern's Exit row");
    service_close();
    standing_new_game(5u);
    printf("bank: gold, letters and loans at the sums, a default a crime; the screen by clicks and keys; build/bank0.ppm, service0.ppm, tavern0.ppm\n");
}

/* An armorer in Daggerfall, entered through its own door: its shelf, its person, what the building says; a save holds it all. */
static void check_people_in_town(void)
{
    Level level;
    Camera cam;
    TravelPose at;
    TravelWhere where;
    float feet[3];
    int chunk[2];
    int index;
    int px;
    int py;
    int block;
    int record;
    int i;
    int type;
    int faction;
    int quality;
    unsigned place;
    int shelves;
    int folk;
    double wx;
    double wz;

    index = daggerfall_index();
    if (index < 0) {
        return;
    }
    if (building_of_type(17, index, BUILDING_ARMORER, &block, &record) < 0) {
        check_expect(0, "people: Daggerfall has no armorer");
        return;
    }
    worldmap_open();
    worldmap_pixel_of(17, index, &px, &py);
    wx = ((double)px + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
    wz = ((double)(WORLDMAP_H - 1 - py) + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
    mem_zero(&where, sizeof(where));
    where.kind = TRAVEL_INSIDE;
    where.located = 1;
    where.region = 17;
    where.index = index;
    where.block = block;
    where.record = record;
    where.has_back = 1;
    where.back_x = wx;
    where.back_z = wz;
    feet[0] = 0.0f;
    feet[1] = 0.0f;
    feet[2] = 0.0f;
    chunk[0] = 0;
    chunk[1] = 0;
    travel_from_save(&where, feet, chunk, 0.0f, 0.0f);
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    check_expect(travel_building(&type, &faction, &quality, &place) && type == BUILDING_ARMORER && quality >= 0 && quality <= 20,
                 "people: the armorer's inside does not say it is one");
    check_expect(travel_region() == 17, "people: the armorer's region");
    shelves = 0;
    folk = 0;
    for (i = 0; i < level.scene.nusables; i++) {
        const Usable *u;

        u = &level.scene.usables[i];
        shelves += u->kind == USE_OTHER && u->what == OTHER_SHELF;
        folk += u->kind == USE_TALK;
    }
    check_expect(shelves > 0, "people: an armorer has no shelves");
    check_expect(folk > 0, "people: an armorer has no one in it");
    for (i = 0; i < level.scene.nusables; i++) {
        const Usable *u;

        u = &level.scene.usables[i];
        if (u->kind == USE_TALK) {
            const Faction *f;

            f = faction_find(u->what);
            check_expect(f != 0, "people: someone in the shop has a faction the file does not");
            check_expect(faces_of_flat(u->tag >> 7, u->tag & 0x7f) >= 0, "people: a person's flat has no face");
            break;
        }
    }
    travel_leave();
    printf("people in town: Daggerfall's armorer, quality %d, %d shelves, %d people\n", quality, shelves, folk);
}

/*
 * A house's chests and cupboards: a Daggerfall house has some (the furniture ids DFU names), what one
 * holds is worked out from its key and is the same each look, what was taken stays gone across a
 * second visit, and what a visit took is counted and weighed for the theft it is.
 */
static void check_house_container(void)
{
    static Entity box;
    static Entity other;
    static Inventory held;
    Inventory again;
    df_u8 origin[INVENTORY_MAX];
    df_u8 origin_again[INVENTORY_MAX];
    KeepKey key;
    float weight;
    int stocked;
    int first;
    int n;
    int i;

    keep_clear();
    loot_new_game(77u);
    /* Forty containers: most hold something, and the same one the same each look. */
    stocked = 0;
    for (i = 0; i < 40; i++) {
        key.place = 0x7e570000u + (unsigned)i;
        key.thing = 0x01000040u + (unsigned)i * 16u;
        loot_house_begin(&box, key, 8);
        n = loot_open(&box, 6, &held, origin);
        loot_open(&box, 6, &again, origin_again);
        check_expect(n == again.count, "house container: it did not hold the same each look");
        stocked += n > 0;
        loot_house_end();
    }
    check_expect(stocked > 30, "house container: most should hold gold at least");

    /* One searched, a thing taken: counted, weighed, and gone for the next look and the next visit. */
    key.place = 0x7e57ffffu;
    key.thing = 0x03000123u;
    loot_house_begin(&box, key, 12);
    check_expect(loot_house_quality() == 12 && df_eq(loot_label(&box), "Container"), "house container: its quality or its label");
    first = loot_open(&box, 6, &held, origin);
    check_expect(first > 0, "house container: the one under test is empty");
    check_expect(loot_house_taken(&weight) == 0, "house container: nothing taken was counted");
    loot_taken(&box, origin[0]);
    n = loot_house_taken(&weight);
    check_expect(n == 1 && weight >= 0.0f, "house container: one thing taken was not counted once");
    check_expect(loot_open(&box, 6, &again, origin_again) == first - 1, "house container: a thing taken is still in it");
    loot_house_end();
    loot_house_begin(&box, key, 12);
    check_expect(loot_open(&box, 6, &again, origin_again) == first - 1 && loot_house_taken(&weight) == 0,
                 "house container: a second visit did not find it as left, or counted the first visit's");
    loot_house_end();
    /* Another thing is no container. */
    memset(&other, 0, sizeof(other));
    check_expect(!df_eq(loot_label(&other), "Container"), "house container: any entity is a container");
}

/* Daggerfall's houses, entered by their own doors: some of them have furniture that is a container. */
static void check_house_interiors(void)
{
    Level level;
    Camera cam;
    TravelPose at;
    TravelWhere where;
    float feet[3];
    int chunk[2];
    int index;
    int px;
    int py;
    int block;
    int record;
    int type;
    int containers;
    int visited;
    int found;
    int i;
    double wx;
    double wz;

    index = daggerfall_index();
    if (index < 0) {
        return;
    }
    containers = 0;
    visited = 0;
    for (type = BUILDING_HOUSE1; type <= BUILDING_HOUSE4; type++) {
        if (building_of_type(17, index, type, &block, &record) < 0) {
            continue;
        }
        worldmap_open();
        worldmap_pixel_of(17, index, &px, &py);
        wx = ((double)px + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
        wz = ((double)(WORLDMAP_H - 1 - py) + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
        mem_zero(&where, sizeof(where));
        where.kind = TRAVEL_INSIDE;
        where.located = 1;
        where.region = 17;
        where.index = index;
        where.block = block;
        where.record = record;
        where.has_back = 1;
        where.back_x = wx;
        where.back_z = wz;
        feet[0] = feet[1] = feet[2] = 0.0f;
        chunk[0] = chunk[1] = 0;
        travel_from_save(&where, feet, chunk, 0.0f, 0.0f);
        if (!travel_go(&level, &cam, &at)) {
            continue;
        }
        visited++;
        found = 0;
        for (i = 0; i < level.scene.nusables; i++) {
            const Usable *u;

            u = &level.scene.usables[i];
            if (u->kind == USE_OTHER && u->what == OTHER_CONTAINER) {
                found++;
                check_expect(u->tag != 0 && u->label && df_eq(u->label, "container"), "house container: a usable without its key or label");
            }
        }
        containers += found;
        printf("house: Daggerfall's type %d house has %d containers\n", type, found);
        travel_leave();
    }
    check_expect(visited > 0, "house container: Daggerfall has no house to enter");
    check_expect(containers > 0, "house container: no house of Daggerfall has a chest or a cupboard");
}

/* The console's goto: by a whole name, by numbers, a part of a name, none; on a street, on the ground, from a dungeon or a town. */
static void check_goto(void)
{
    static DunRef towns[16000];
    Level level;
    Camera cam;
    TravelPose at;
    Player p;
    Character sheet;
    float x;
    float z;
    int region;
    int index;
    int block;
    int record;
    int dagger;
    int n;
    int i;
    int stood;
    int tried;

    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    player_init(&p, &sheet);
    commands_setup();
    commands_player(&p);
    /* Nothing asked by a name that is no place's, nor by numbers past the map. */
    cmd_run("goto Nowhere At All");
    check_expect(!travel_asked(), "goto: a name that is no place's");
    cmd_run("goto 1");
    check_expect(!travel_asked(), "goto: a code that is no place's");
    cmd_run("goto 61 99999");
    check_expect(!travel_asked(), "goto: numbers past the map");
    /* A part of a name that fits many lists them, and goes nowhere. */
    cmd_run("goto dagger");
    check_expect(!travel_asked(), "goto: a part of a name that fits several went somewhere");
    /* From the hold, to Sentinel by name, any case. */
    travel_start_game();
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    cmd_run("goto SENTINEL");
    check_expect(travel_asked(), "goto: Sentinel was not found");
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    check_expect(travel_town_here(&cam, &region, &index, &x, &z, &block, &record), "goto: not in a town");
    n = 0;
    town_buildings(region, index, &n);
    check_expect(df_eq(town_name(), "Sentinel") && n > 100, "goto: not in Sentinel");
    check_expect(stands_at(&p, &level, &at, 96.0f), "goto: the player does not stand in Sentinel's street");
    {
        double wx;
        double wz;

        stream_world(at.x, at.z, &wx, &wz);
        check_expect(wild_nav(wx, wz) > 0, "goto: not landed on a walkable cell");
    }
    cmd_run("where");
    /* By its numbers: Daggerfall. */
    dagger = daggerfall_index();
    check_expect(dagger >= 0, "goto: no Daggerfall");
    {
        char line[32];

        log_format(line, (int)sizeof(line), "goto 17 %d", dagger);
        cmd_run(line);
    }
    check_expect(travel_asked(), "goto: Daggerfall by its numbers");
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    check_expect(travel_town_here(&cam, &region, &index, &x, &z, &block, &record) && region == 17 && index == dagger
                 && travel_region() == 17, "goto: not in Daggerfall");
    check_expect(stands_at(&p, &level, &at, 96.0f), "goto: the player does not stand in Daggerfall's street");
    {
        double wx;
        double wz;
        double x0;
        double z0;
        double x1;
        double z1;
        double dx;
        double dz;
        int blocks;
        int climate;
        int type;

        stream_world(at.x, at.z, &wx, &wz);
        {
            int px;
            int py;
            double pcx;
            double pcz;

            worldmap_pixel_of(region, index, &px, &py);
            pcx = ((double)px + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
            pcz = ((double)(WORLDMAP_H - 1 - py) + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
            check_expect(wild_town(pcx, pcz, &x0, &z0, &x1, &z1, &blocks, &climate, &type),
                         "goto: Daggerfall's town was not read");
        }
        dx = wx - (x0 + x1) * 0.5;
        dz = wz - (z0 + z1) * 0.5;
        check_expect(dx * dx + dz * dz > 4000.0 * 4000.0, "goto: Daggerfall was not the entrance");
    }
    /* By its code, the number LEVELFALL lists: Daggerfall's outside's own. */
    {
        Dungeon town;
        char line[32];

        check_expect(exterior_load_at(17, dagger, &town), "goto: Daggerfall's record");
        travel_start_game();
        if (!travel_go(&level, &cam, &at)) {
            return;
        }
        log_format(line, (int)sizeof(line), "goto %d", town.location_id);
        cmd_run(line);
        check_expect(travel_asked(), "goto: Daggerfall by its code");
        if (!travel_go(&level, &cam, &at)) {
            return;
        }
        check_expect(travel_town_here(&cam, &region, &index, &x, &z, &block, &record) && region == 17 && index == dagger,
                     "goto: the code did not lead to Daggerfall");
    }
    /* A sample of the whole map: footing at every one. */
    n = exterior_list(towns, 16000);
    tried = 0;
    stood = 0;
    for (i = 0; i < n; i += 1009) {
        if (!travel_go_to_town(towns[i].region, towns[i].index) || !travel_go(&level, &cam, &at)) {
            continue;
        }
        tried++;
        if (stands_at(&p, &level, &at, 96.0f)) {
            stood++;
        } else {
            printf("goto: no footing at %s (%d %d)\n", towns[i].name, towns[i].region, towns[i].index);
        }
    }
    travel_leave();
    commands_player(0);
    check_expect(tried >= 15 && stood == tried, "goto: a place of the map has no footing");
    printf("goto: Sentinel by name, Daggerfall by numbers; %d of %d places over the map stood at\n", stood, tried);
}

/*
 * Sight and walls: from the hold's start, a ray cut in pieces says the same as one
 * cut far finer, over the whole ring; and over a long fight no flyer is ever inside
 * a wall, and few wake who have no way to the player.
 */
static void check_senses(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    Character sheet;
    Player p;
    Input in;
    int flyers_in_walls;
    int rays;
    int walled;
    int i;
    int slot;

    keep_clear();
    combat_reset();
    foes_reset();
    combat_seed(4321);
    if (!dungeon_load(&hold) || !dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "senses: the hold's stream did not open");
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    character_new(&sheet, RACE_ARGONIAN, CLASS_WARRIOR, 1, "Test");
    player_init(&p, &sheet);
    player_place(&p, x, y, z, yaw, pitch);
    /* Every creature in the ring to the player: most of the hold is walls between them. */
    rays = 0;
    walled = 0;
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            float eye[3];
            float to[3];
            int gathered;
            int dropped;

            if (dunstream_kind(&ents[i], 0) < 0) {
                continue;
            }
            eye[0] = ents[i].x;
            eye[1] = ents[i].y + 40.0f;
            eye[2] = ents[i].z;
            to[0] = x;
            to[1] = y;
            to[2] = z;
            rays++;
            walled += !combat_sight(&level.scene, eye, to);
            collide_stats(&gathered, &dropped);
            check_expect(dropped == 0, "senses: a sight ray overflowed its gather");
        }
    }
    check_expect(rays >= 40 && walled >= rays - 4, "senses: creatures across the hold see the player's start through walls");
    /* A long fight: no flyer's body ever cuts a wall, tgm keeping the player up. */
    flyers_in_walls = 0;
    p.god = 1;
    memset(&in, 0, sizeof(in));
    for (i = 0; i < 30 * 60; i++) {
        int m;

        player_step(&p, &in, &level.scene, 1.0f / 30.0f, 1);
        clock_set_minute(clock_minute() + 0.2f / 30.0f);
        foes_frame(&p, &level.scene, 1.0f / 30.0f);
        combat_frame(1.0f / 30.0f);
        m = foes_flyers_in_walls(&level.scene);
        flyers_in_walls += m;
    }
    check_expect(flyers_in_walls == 0, "senses: a flyer's body went into a wall");
    printf("senses: %d of %d creatures walled from the player's start; %d flyer frames in walls, %d awake after a minute\n",
           walled, rays, flyers_in_walls, foes_awake());
    /* With the AI off, nobody hunts: a rest is allowed. */
    foes_set_on(0);
    check_expect(!foes_hunting(), "senses: the AI off, someone still hunts");
    foes_set_on(1);
    foes_reset();
    check_expect(!foes_hunting(), "senses: minds let go, someone still hunts");
    dunstream_close();
    keep_clear();
    /* The click that closes a screen is no swing: not while held, not on its release, and the next press is one. */
    weapon_gate(0, 0);
    weapon_gate(0, 0);
    check_expect(!weapon_gate(0, 0), "senses: the gate shut with no screen");
    check_expect(weapon_gate(1, 1), "senses: the gate open under a screen");
    check_expect(weapon_gate(0, 1), "senses: the closing click, still held, gets through");
    check_expect(weapon_gate(0, 0), "senses: the closing click's release gets through");
    check_expect(!weapon_gate(0, 1), "senses: the next press does not get through");
    weapon_gate(0, 0);
    /* Two weapons: the doll holds the one in use, in one hand. */
    {
        static df_u8 pixels[DOLL_W * DOLL_H];
        static df_u8 owner[DOLL_W * DOLL_H];
        Item it;
        Character c;
        Inventory pack;
        int k;
        int left;
        int right;

        character_new(&c, RACE_ARGONIAN, CLASS_WARRIOR, 1, "Test");
        inventory_clear(&pack);
        item_make(&it, ITEM_LONGSWORD, 0);
        check_expect(inventory_wear(&pack, inventory_add(&pack, &it)), "doll: first sword");
        item_make(&it, ITEM_DAGGER, 0);
        check_expect(inventory_wear(&pack, inventory_add(&pack, &it)), "doll: second blade");
        check_expect(pack.worn[EQUIP_RIGHT_HAND] >= 0 && pack.worn[EQUIP_LEFT_HAND] >= 0, "doll: two hands not filled");
        check_expect(doll_compose(pixels, owner, &c, &pack), "doll: no doll");
        left = 0;
        right = 0;
        for (k = 0; k < DOLL_W * DOLL_H; k++) {
            left += owner[k] == EQUIP_LEFT_HAND;
            right += owner[k] == EQUIP_RIGHT_HAND;
        }
        check_expect(right > 0 && left == 0, "doll: both weapons drawn in one hand");
        check_expect(inventory_swap_hands(&pack), "doll: no swap");
        doll_compose(pixels, owner, &c, &pack);
        left = 0;
        right = 0;
        for (k = 0; k < DOLL_W * DOLL_H; k++) {
            left += owner[k] == EQUIP_LEFT_HAND;
            right += owner[k] == EQUIP_RIGHT_HAND;
        }
        check_expect(left > 0 && right == 0, "doll: swapping does not show the other weapon");
        doll_forget_files();
    }
}

/* What the world thinks, through a save file and back. */
static void check_standing_saved(void)
{
    SaveState out;
    SaveState in;
    char path[260];
    const Faction *people;

    save_use_file("DATA\\TEST5.SAV");
    keep_clear();
    pile_new_game();
    standing_new_game(5u);
    people = faction_people_of(17);
    standing_change(people->id, 33, 0);
    standing_social_change(SOCIAL_NOBILITY, -7);
    standing_set_reaction_bonus(4);
    standing_region(17)->legal = -12;
    standing_region(17)->gold = 4321;
    standing_region(17)->loan = 550;
    standing_region(17)->due_day = 99;
    standing_region(17)->banished = 1;
    standing_region(17)->prices = 1111;
    mem_zero(&out, sizeof(out));
    character_new(&out.sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    inventory_clear(&out.pack);
    out.seed = 5u;
    check_expect(save_write(&out), "people: a save did not write");
    standing_new_game(6u);
    check_expect(save_read(&in), "people: a save did not read");
    check_expect(standing_rep(people->id) == people->rep + 33 && standing_social(SOCIAL_NOBILITY) == -7 && standing_reaction_bonus() == 4,
                 "people: reputations did not come back");
    check_expect(standing_region(17)->legal == -12 && standing_region(17)->gold == 4321 && standing_region(17)->loan == 550
                 && standing_region(17)->due_day == 99 && standing_region(17)->banished && standing_region(17)->prices == 1111,
                 "people: a region's law and bank did not come back");
    check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST5.SAV") && remove(path) == 0, "people: the test's file did not go");
    keep_clear();
    standing_new_game(5u);
}

/* Full screen and head bobbing: the eye's offset at a walk's phase, and the options' defaults. */
/* Rasterize the actual menu quads: layout/text checks and review pictures
 * exercise the shipping atlas path rather than an unrelated mockup. */
static df_u8 g_settings_preview[SETTINGS_W * SETTINGS_H];
static int g_settings_quads;
static void settings_preview_quad(void *ctx, int atlas, int sx, int sy, int sw, int sh,
                                  int x, int y, int w, int h)
{
    const df_u8 *pixels;
    int side, i, j;
    (void)ctx;
    pixels = ui_atlas(atlas, &side);
    check_expect(pixels && sx >= 0 && sy >= 0 && sx+sw <= side && sy+sh <= side,
                 "settings: quad outside atlas");
    check_expect(x >= 0 && y >= 0 && x+w <= SETTINGS_W && y+h <= SETTINGS_H,
                 "settings: quad outside output");
    if (!pixels || w <= 0 || h <= 0) return;
    g_settings_quads++;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
        df_u8 c;
        c = pixels[(sy+j*sh/h)*side+sx+i*sw/w];
        if ((atlas == 0 || c) && x+i >= 0 && x+i < SETTINGS_W && y+j >= 0 && y+j < SETTINGS_H)
            g_settings_preview[(y+j)*SETTINGS_W+x+i] = c;
    }
}
static const char *settings_test_translation(const char *s)
{
    if (df_eq(s, "GRAPHICAL SETTINGS")) return "OPCIONES GRAFICAS";
    if (df_eq(s, "FIELD OF VIEW")) return "CAMPO DE VISION";
    return 0;
}
static void check_settings(void)
{
    SettingsView v;
    const df_u8 *rgb;
    int tab, i;
    FILE *f;
    static const char *const paths[] = {"build/settings-files.ppm", "build/settings-keys.ppm", "build/settings-graphics.ppm", "build/settings-audio.ppm", "build/settings-gameplay.ppm", "build/settings-translated.ppm"};
    check_expect(ui_open(), "settings: assets did not open");
    rgb = ui_palette();
    mem_zero(&v, sizeof(v));
    v.classic = 1; v.waiting = -1; v.hover = SETTINGS_NOTHING;
    {
        static const char *const names[] = { "W", "S", "Left", "Right", "A", "D", "Space", "Shift", "R", "Right Button", "H", "C", "I", "E", "Tab", "Page Up", "Page Down", "Home", "Alt", "M", "V", "T" };
        for (i = 0; i < SETTINGS_BINDINGS; i++) v.bindings[i] = names[i];
    }
    v.sfx_volume = v.music_volume = 80;
    v.full_hud = 1;
    v.gfx.resolution = 0;
    v.gfx.fov = 75;
    v.gfx.far_clip = 1400;
    v.gfx.ambient = 112;
    v.gfx.lightrange = 3;
    v.gfx.floradensity = 40;
    v.gfx.florasight = 0;
    v.gfx.fog = v.gfx.spritepoint = v.gfx.vsync = v.gfx.lights = v.gfx.lightshadow = 1;
    v.gfx.flora = 1;
    v.gfx.weathersim = 0;
    for (tab = 0; tab <= SETTINGS_TABS; tab++) {
        v.tab = tab < SETTINGS_TABS ? tab : SETTINGS_GRAPHICS;
        if (tab == SETTINGS_TABS) ui_translate(settings_test_translation);
        mem_fill(g_settings_preview, 18, sizeof(g_settings_preview));
        g_settings_quads = 0;
        settings_draw(&v, settings_preview_quad, 0);
        check_expect(g_settings_quads > 100 && g_settings_quads < 2000, "settings: unexpected draw count");
        f = fopen(paths[tab], "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", SETTINGS_W, SETTINGS_H);
            for (i = 0; i < SETTINGS_W*SETTINGS_H; i++) fwrite(rgb+g_settings_preview[i]*3, 1, 3, f);
            fclose(f);
        }
    }
    ui_translate(0);
    check_expect(settings_hit(SETTINGS_FILES,0,30,120) == SETTINGS_TAB_FILES
        && settings_hit(SETTINGS_FILES,0,30,160) == SETTINGS_TAB_KEYS
        && settings_hit(SETTINGS_FILES,0,30,200) == SETTINGS_TAB_GRAPHICS, "settings: navigation order");
    /* The saved games' screen is files.c's: its own tests (check_saves, check_files) drive its controls. */
    check_expect(settings_hit(SETTINGS_KEYS,8,450,370) == SETTINGS_BIND+19
        && settings_binding_action(19,1) == INPUT_MAP, "settings: last scrolled binding inaccessible");
    check_expect(settings_binding_action(9,1) == INPUT_SWING
        && settings_binding_action(9,0) == INPUT_ATTACK, "settings: scheme-specific swing binding");
    check_expect(settings_fov_at(-100) == 45 && settings_fov_at(336) == 45
        && settings_fov_at(408) == 75 && settings_fov_at(516) == 120
        && settings_fov_at(1000) == 120, "settings: FOV track endpoints or default");
    check_expect(settings_hit(SETTINGS_GRAPHICS,0,408,120) == SETTINGS_SLIDER
        && settings_hit(SETTINGS_GRAPHICS,0,450,166) == SETTINGS_GFX_TEXDETAIL
        && settings_hit(SETTINGS_GRAPHICS,0,450,192) == SETTINGS_GFX_SPRITEDETAIL
        && settings_hit(SETTINGS_GRAPHICS,0,450,226) == SETTINGS_GFX_FOG, "settings: graphics scroll rows");
    check_expect(settings_hit(SETTINGS_GRAPHICS, 1, 450, 226) == SETTINGS_GFX_SPRITEPOINT
        && settings_hit(SETTINGS_GRAPHICS, 0, 450, 226) == SETTINGS_GFX_FOG,
        "settings: graphics hit uses list scroll offset");
    {
        int i;
        int has_default;

        /* The list is the engine's, smallest first (the menu gives it); 640 x 480 is always among them. */
        has_default = 0;
        for (i = 0; i < settings_resolutions(); i++) {
            has_default |= settings_resolution_w(i) == 640 && settings_resolution_h(i) == 480;
            check_expect(i == 0 || settings_resolution_w(i) >= settings_resolution_w(i - 1), "settings: the modes are not smallest first");
        }
        check_expect(has_default && settings_resolutions() >= 1 && settings_resolution_w(settings_resolutions()) == 0,
                     "settings: the resolution list lacks 640x480 or does not end");
    }
    check_expect(settings_gfx_console_var(SETTINGS_GFX_FOG)
        && settings_gfx_console_var(SETTINGS_GFX_FAR_SLIDER)
        && settings_gfx_console_var(SETTINGS_GFX_FLORA)
        && df_eq(settings_gfx_console_var(SETTINGS_GFX_TEXDETAIL), "texdetail")
        && df_eq(settings_gfx_console_var(SETTINGS_GFX_SPRITEDETAIL), "spritedetail")
        && !settings_gfx_console_var(SETTINGS_GFX_RESOLUTION), "settings: engine console names");
    check_expect(settings_hit(SETTINGS_AUDIO,0,408,112) == SETTINGS_SFX_SLIDER
        && settings_hit(SETTINGS_AUDIO,0,408,168) == SETTINGS_MUSIC_SLIDER
        && settings_hit(SETTINGS_GAMEPLAY,0,500,119) == SETTINGS_BOB
        && settings_hit(SETTINGS_GAMEPLAY,0,500,175) == SETTINGS_FULL_HUD,
        "settings: new controls unreachable");
    check_expect(settings_slider_at(SETTINGS_SFX_SLIDER,-10)==0
        && settings_slider_at(SETTINGS_MUSIC_SLIDER,900)==100
        && settings_slider_at(SETTINGS_SFX_SLIDER,426)==50,
        "settings: volume endpoints or midpoint");
    ui_close();
    check_expect(ui_open(), "settings: reopen failed");
    ui_close();
    printf("settings: native panels, translated labels, binding scroll and FOV endpoints; build/settings-*.ppm\n");
}

static void check_viewopt(void)
{
    Player p;
    Character sheet;
    Camera cam;
    float phase;
    float s;
    float c;
    float deg;
    float right;
    float up;
    float pitch;
    float yaw;

    viewopt_defaults();
    check_expect(!viewopt_full() && !viewopt_bob(), "view: full screen or head bobbing does not start off");
    viewopt_eye(0.0f, 1, 0, &right, &up, &pitch, &yaw);
    check_expect(right == 0.0f && up == 0.0f && pitch == 0.0f && yaw == 0.0f,
                 "view: head bobbing off still moves the eye");
    viewopt_set_bob(1);
    viewopt_eye(0.0f, 0, 0, &right, &up, &pitch, &yaw);
    check_expect(right == 0.0f && up == 0.0f && pitch == 0.0f && yaw == 0.0f, "view: standing still bobs");
    phase = 0.0f;
    s = df_sin(phase);
    c = df_cos(phase);
    viewopt_eye(phase, 1, 0, &right, &up, &pitch, &yaw);
    check_expect(check_near(right, c * 0.045f * RULES_UNITS_PER_M, 0.001f)
                     && check_near(up, df_absf(s * 0.062f) * RULES_UNITS_PER_M, 0.001f),
                 "view: a walk's phase 0 is not HeadBobber's sideways step");
    phase = DF_TAU / 4.0f;
    s = df_sin(phase);
    c = df_cos(phase);
    deg = DF_TAU / 360.0f;
    viewopt_eye(phase, 1, 0, &right, &up, &pitch, &yaw);
    check_expect(check_near(right, c * 0.045f * RULES_UNITS_PER_M, 0.001f)
                     && check_near(up, df_absf(s * 0.062f) * RULES_UNITS_PER_M, 0.001f)
                     && check_near(pitch, df_absf(s * 0.25f) * deg, 0.0001f)
                     && check_near(yaw, -s * 0.1f * deg, 0.0001f),
                 "view: the crest of a walk is not the rise and the nod");
    viewopt_eye(phase, 1, 1, &right, &up, &pitch, &yaw);
    check_expect(check_near(right, c * 0.09f * RULES_UNITS_PER_M, 0.001f)
                     && check_near(up, df_absf(s * 0.11f) * RULES_UNITS_PER_M, 0.001f),
                 "view: a run does not use HeadBobber's run amounts");
    phase = DF_TAU / 2.0f;
    viewopt_eye(phase, 1, 0, &right, &up, &pitch, &yaw);
    check_expect(check_near(right, df_cos(phase) * 0.045f * RULES_UNITS_PER_M, 0.001f) && check_near(up, 0.0f, 0.001f),
                 "view: half a walk does not sway the other way");
    viewopt_set_bob(0);
    viewopt_eye(DF_TAU / 4.0f, 1, 0, &right, &up, &pitch, &yaw);
    check_expect(right == 0.0f && up == 0.0f, "view: turning head bobbing off left the offset");

    if (character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test")) {
        player_init(&p, &sheet);
        player_place(&p, 10.0f, 80.0f, 20.0f, 0.0f, 0.0f);
        p.bob_walk = 1;
        p.bob_phase = 0.0f;
        p.running = 0;
        player_camera(&p, &cam);
        check_expect(check_near(cam.x, 10.0f, 0.001f) && check_near(cam.y, 80.0f, 0.001f) && check_near(cam.z, 20.0f, 0.001f),
                     "view: the setting off, the eye left the body");
        viewopt_set_bob(1);
        player_camera(&p, &cam);
        check_expect(check_near(cam.x, 10.0f + df_cos(0.0f) * 0.045f * RULES_UNITS_PER_M, 0.001f)
                         && check_near(cam.y, 80.0f, 0.001f) && check_near(cam.z, 20.0f, 0.001f),
                     "view: phase 0 did not shift the eye along the body's right");
        p.bob_walk = 0;
        player_camera(&p, &cam);
        check_expect(check_near(cam.x, 10.0f, 0.001f) && check_near(cam.y, 80.0f, 0.001f),
                     "view: standing still left the eye off the body");
    } else {
        check_expect(0, "view: no walker");
    }

    /* The defaults the settings file starts from: the bar shown, no bob, 640 x 480, High, no crouch toggle. */
    viewopt_set_full(1);
    viewopt_set_bob(1);
    viewopt_set_mode(400, 300);
    viewopt_set_tex_detail(0);
    viewopt_set_sprite_detail(1);
    viewopt_set_crouch_toggle(1);
    camera_set_fov(103.0f);
    viewopt_defaults();
    {
        int mw;
        int mh;

        viewopt_mode(&mw, &mh);
        check_expect(!viewopt_full() && !viewopt_bob() && !viewopt_crouch_toggle() && mw == 640 && mh == 480
                         && viewopt_tex_detail() == 2 && viewopt_sprite_detail() == 2,
                     "view: the defaults are not the bar shown, no bob, 640 x 480 and High");
        viewopt_set_mode(10, 10);
        viewopt_mode(&mw, &mh);
        check_expect(mw == 640 && mh == 480, "view: a silly mode was kept");
        viewopt_set_tex_detail(9);
        check_expect(viewopt_tex_detail() == 2, "view: a detail past High was kept");
    }
    viewopt_defaults();
    check_expect(df_iround(camera_fov()) == 75, "view: missing FOV did not keep default");
    printf("view: full screen off, head bob at a walk's phase\n");
}

/* 1 when a path ends in .OLD. */
static int ends_with_old(const char *path)
{
    int n;

    n = (int)strlen(path);
    return n > 4 && strcmp(path + n - 4, ".OLD") == 0;
}

/*
 * The saved games' folders and their screen: a character's name made safe for a file system; each character a folder,
 * each save a numbered file; two of the same name stay two, also when only the case differs; the list by name and each
 * one's saves newest first; a half-finished move still listed; deletes of a save and of a character; then the screen
 * driven by clicks and keys: selection, snapshot, LOAD, double click, SAVE only in a game, DELETE asked twice.
 */
static void make_test_save(SaveState *st, df_u32 id, const char *name, int level)
{
    mem_zero(st, sizeof(*st));
    character_new(&st->sheet, RACE_BRETON, CLASS_WARRIOR, 0, name);
    st->sheet.id = id;
    st->sheet.level = level;
    inventory_clear(&st->pack);
    df_copy(st->where_words, "Test place", SAVE_WHERE);
    st->day = 12345;
    st->minute = 600.0f;
}

static void check_saves_and_files(void)
{
    static df_u16 pix[SAVE_THUMB_W * SAVE_THUMB_H];
    SaveState st;
    char path[SAVE_PATH];
    char safe[SAVES_FOLDER];
    int i;
    int n;
    const SaveEntry *e;
    const SaveChar *c;

    /* A name made safe. */
    saves_safe_name("Anna Smith", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "Anna Smith") == 0, "saves: a plain name changed");
    saves_safe_name("a/b:c*d?e", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "a_b_c_d_e") == 0, "saves: punctuation was kept");
    saves_safe_name("  Bob   ", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "Bob") == 0, "saves: a leading or trailing space was kept");
    saves_safe_name("x.", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "x_") == 0, "saves: a dot was kept");
    saves_safe_name("CON", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "CON_") == 0, "saves: a device name was kept");
    saves_safe_name("lpt3", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "lpt3_") == 0, "saves: a device name in lower case was kept");
    saves_safe_name("Comet", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "Comet") == 0, "saves: a name that only starts like a device was changed");
    saves_safe_name("", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "Hero") == 0, "saves: an empty name has no folder name");
    saves_safe_name("\xC3\x9Cn\xC3\xAF", safe, (int)sizeof(safe));
    check_expect(strcmp(safe, "__n__") == 0, "saves: bytes past ASCII were kept");

    saves_use_root("hostcheck_saves");
    saves_scan();
    check_expect(saves_count() == 0 && !saves_any(), "saves: the test folder was not empty");
    for (i = 0; i < SAVE_THUMB_W * SAVE_THUMB_H; i++) {
        pix[i] = (df_u16)i;
    }

    /* Three saves of Anna, two of Bob, one of another Anna and one of ANNA SMITH (whose folder is the first's by case). */
    for (i = 0; i < 3; i++) {
        make_test_save(&st, 0xA001u, "Anna Smith", 2 + i);
        st.thumb = i == 1 ? 0 : pix;
        check_expect(saves_write(&st, path), "saves: Anna's save did not write");
        if (i == 0) {
            check_expect(strcmp(path, "hostcheck_saves\\Anna Smith\\S0001.SAV") == 0, "saves: the first path is not folder and S0001");
        }
    }
    check_expect(strcmp(path, "hostcheck_saves\\Anna Smith\\S0003.SAV") == 0, "saves: the third save is not S0003");
    for (i = 0; i < 2; i++) {
        make_test_save(&st, 0xB002u, "Bob", 5);
        st.thumb = pix;
        check_expect(saves_write(&st, path), "saves: Bob's save did not write");
    }
    make_test_save(&st, 0xA003u, "Anna Smith", 9);
    check_expect(saves_write(&st, path) && strcmp(path, "hostcheck_saves\\Anna Smith (2)\\S0001.SAV") == 0,
                 "saves: another Anna Smith did not get a folder of her own");
    make_test_save(&st, 0xA004u, "ANNA SMITH", 1);
    check_expect(saves_write(&st, path) && strcmp(path, "hostcheck_saves\\ANNA SMITH (3)\\S0001.SAV") == 0,
                 "saves: a name that differs only by case shared a folder, or took a taken number");

    n = saves_scan();
    check_expect(n == 7 && saves_char_count() == 4, "saves: the list did not find 7 saves of 4 characters");
    /* By name without case, then id: Anna's three ids, then Bob. */
    check_expect(saves_char(0)->id == 0xA001u && saves_char(1)->id == 0xA003u && saves_char(2)->id == 0xA004u
                     && saves_char(3)->id == 0xB002u, "saves: the characters are not by name, then id");
    c = saves_char(0);
    check_expect(c->count == 3 && saves_entry(c->first)->number == 3 && saves_entry(c->first + 1)->number == 2
                     && saves_entry(c->first + 2)->number == 1, "saves: a character's saves are not newest first");
    e = saves_entry(c->first);
    check_expect(e->meta.char_id == 0xA001u && e->meta.level == 4 && strcmp(e->meta.name, "Anna Smith") == 0
                     && strcmp(e->meta.where, "Test place") == 0 && e->meta.game_day == 12345,
                 "saves: an entry's meta is not what was saved");

    /* A move cut short: only S0002.OLD, no S0002.SAV, still the save; with both, one. */
    {
        char from[SAVE_PATH];
        char to[SAVE_PATH];

        log_format(from, (int)sizeof(from), "%s", saves_entry(c->first + 1)->path);
        log_format(to, (int)sizeof(to), "hostcheck_saves\\Anna Smith\\S0002.OLD");
        check_expect(fs_move(from, to), "saves: the test could not move a save aside");
        saves_scan();
        check_expect(saves_count() == 7 && ends_with_old(saves_entry(saves_char(0)->first + 1)->path),
                     "saves: a save left under .OLD was not listed");
        check_expect(fs_move(to, from), "saves: the test could not move it back");
        saves_scan();
    }

    /* The screen over these. */
    {
        int row_anna;
        int row_bob_save;
        int hit;
        df_u32 serial;

        files_open(1, 0xA001u, 0);
        check_expect(files_rows() == 4 + 7, "files: the list is not each character and each save");
        row_anna = files_selected_row();
        check_expect(row_anna == 1 && !files_row_is_character(row_anna), "files: the playing character's newest save was not selected");
        check_expect(files_thumb() != 0 && files_thumb()[100] == 100, "files: the snapshot of the selected save did not read");
        serial = files_thumb_serial();
        /* The regions the screen draws answer: a row, the buttons. */
        ui_begin(0, 0);
        files_draw(-1);
        check_expect(ui_hit(400, 88 + 24) == FILES_ID_ROW + 1, "files: a click on the second row is not that row");
        check_expect(ui_hit(200, 400) == FILES_ID_LOAD && ui_hit(320, 400) == FILES_ID_SAVE && ui_hit(450, 400) == FILES_ID_DELETE,
                     "files: LOAD, SAVE and DELETE are not where they are drawn");
        /* Bob's save: its row is after his header; selected by a click; the snapshot is another read. */
        for (hit = 0, row_bob_save = -1; hit < files_rows(); hit++) {
            if (files_row_is_character(hit) && hit > 1 + 3) {
                row_bob_save = hit + 1;
            }
        }
        check_expect(row_bob_save > 0, "files: Bob's rows were not found");
        check_expect(files_click(FILES_ID_ROW + row_bob_save, 1000, 1) == FILES_NONE && files_selected_row() == row_bob_save,
                     "files: a click on a save did not select it, or loaded it");
        check_expect(files_thumb_serial() != serial, "files: another save selected did not change the snapshot");
        /* A double click on a save loads it: within 450 ms, not after; a character row never loads. */
        check_expect(files_click(FILES_ID_ROW + row_bob_save, 1200, 1) == FILES_LOAD
                         && strstr(files_chosen(), "Bob") != 0 && strstr(files_chosen(), ".SAV") != 0,
                     "files: a double click did not load the save");
        files_click(FILES_ID_ROW + row_bob_save, 3000, 1);
        check_expect(files_click(FILES_ID_ROW + row_bob_save, 3600, 1) == FILES_NONE, "files: a slow second click loaded");
        check_expect(files_click(FILES_ID_ROW + row_bob_save, 3700, 0) == FILES_NONE, "files: a double click with no clock loaded");
        check_expect(files_click(FILES_ID_LOAD, 5000, 1) == FILES_LOAD, "files: LOAD did not load the selected save");
        /* A character's row chosen, LOAD takes their newest. */
        files_click(FILES_ID_ROW + 0, 6000, 1);
        check_expect(files_row_is_character(files_selected_row()) && files_click(FILES_ID_LOAD, 6100, 1) == FILES_LOAD
                         && strstr(files_chosen(), "S0003.SAV") != 0, "files: LOAD on a character did not take their newest save");
        check_expect(files_click(FILES_ID_ROW + 0, 6200, 1) == FILES_NONE && files_click(FILES_ID_ROW + 0, 6300, 1) == FILES_NONE,
                     "files: a double click on a character loaded");
        /* SAVE answers in a game and means a new save. */
        check_expect(files_click(FILES_ID_SAVE, 7000, 1) == FILES_SAVE, "files: SAVE did not ask to save");
        files_open(0, 0xA001u, 0);
        ui_begin(0, 0);
        files_draw(-1);
        check_expect(ui_hit(320, 400) == UI_NOTHING && files_click(FILES_ID_SAVE, 7100, 1) == FILES_NONE, "files: SAVE answered with no game");
        /* Keys: Down, Up, Enter loads, Delete asks. */
        files_open(0, 0xA001u, 0);
        check_expect(files_selected_row() == 1, "files: not the first save to start");
        files_key(0x28);
        check_expect(files_selected_row() == 2, "files: Down did not move the selection");
        files_key(0x26);
        files_key(0x26);
        check_expect(files_selected_row() == 0 && files_key(0x0D) == FILES_LOAD, "files: Up, or Enter on a character's row");
        /* DELETE asks twice; another click in between calls it off. */
        files_click(FILES_ID_ROW + 2, 8000, 1);
        n = saves_count();
        files_click(FILES_ID_DELETE, 8100, 1);
        check_expect(files_confirming() && files_note() != 0 && saves_count() == n, "files: the first DELETE deleted, or did not ask");
        files_click(FILES_ID_ROW + 3, 8200, 1);
        check_expect(!files_confirming() && saves_count() == n, "files: another click did not call the delete off");
        files_click(FILES_ID_DELETE, 8300, 1);
        files_click(FILES_ID_DELETE, 8400, 1);
        check_expect(saves_count() == n - 1 && !files_confirming(), "files: the second DELETE did not delete the save");
        check_expect(files_selected_row() >= 0, "files: nothing is selected after a delete");
        /* A character's row: every save of theirs, and their folder. */
        files_open(0, 0xB002u, 0);
        files_click(FILES_ID_ROW + 0, 9000, 1);
        for (i = 0; i < files_rows(); i++) {
            if (files_row_is_character(i) && saves_char(0) && i > 0) {
                break;
            }
        }
        /* Select Bob's header (the last character's). */
        for (i = files_rows() - 1; i >= 0 && !files_row_is_character(i); i--) {
        }
        files_click(FILES_ID_ROW + i, 9100, 1);
        n = saves_char_count();
        files_click(FILES_ID_DELETE, 9200, 1);
        files_click(FILES_ID_DELETE, 9300, 1);
        check_expect(saves_char_count() == n - 1 && !fs_exists("hostcheck_saves\\Bob"), "files: deleting a character left them or their folder");
    }

    /* Clean up what the test made, and the folder. */
    while (saves_char_count() > 0) {
        saves_delete_char(0);
    }
    check_expect(saves_count() == 0, "saves: the test's saves did not all go");
    fs_remove_dir("hostcheck_saves");
    check_expect(!fs_exists("hostcheck_saves"), "saves: the test's folder did not go");
    saves_use_root(0);
    printf("saves: folders by character, numbered files, same names apart, deleted a save and a character; the list driven by clicks and keys\n");
}

/*
 * gfall.cfg: every action named and defaulted, a Config through its text and back the same (keys with spaces and an
 * equals sign among them), a hand-written file read through comments, case, spaces and notes, the lines it cannot use
 * counted and the rest kept, the screen's mode peeked, a buffer too small refused.
 */
static void check_config(void)
{
    static char text[CONFIG_MAX];
    Config a;
    Config b;
    int n;
    int i;
    int j;
    int bad;
    int first;
    int w;
    int h;

    /* The actions: each but Escape has a name, none twice, and classic's key. */
    for (i = 0; i < INPUT_ACTIONS; i++) {
        const char *name;

        name = config_action_name(i);
        check_expect((i == INPUT_MENU) == (name == 0), "config: an action lacks a name, or Escape has one");
        check_expect(i == INPUT_MENU || config_default_key(i) != 0, "config: an action has no default key");
        for (j = 0; name && j < i; j++) {
            check_expect(!config_action_name(j) || strcmp(name, config_action_name(j)) != 0, "config: two actions share a name");
        }
    }
    check_expect(config_default_key(INPUT_USE) == 'E' && config_default_key(INPUT_CROUCH) == 0x11
                     && config_default_key(INPUT_ATTACK) == 0x01 && config_default_key(INPUT_MENU) == 0,
                 "config: classic's keys are not E, Ctrl and the left button");

    /* A Config with every field its own: out as text and back the same. */
    memset(&a, 0, sizeof(a));
    for (i = 0; i < INPUT_ACTIONS; i++) {
        a.key[i] = i == INPUT_MENU ? 0 : 'A' + i;
    }
    a.key[INPUT_USE] = 0xBB;
    a.key[INPUT_ATTACK] = 0x01;
    a.key[INPUT_JUMP] = 0x6B;
    a.key[INPUT_RUN] = 0x20;
    a.mouse_cursor = 0;
    a.fov = 101;
    a.far_clip = 2222;
    a.fog = 0;
    a.crisp_sprites = 1;
    a.vsync = 0;
    a.dungeon_lights = 1;
    a.ambient_light = 77;
    a.light_range = 5;
    a.light_shadow = 0;
    a.weather_view = 1;
    a.flora = 0;
    a.flora_density = 33;
    a.flora_sight = 4096;
    a.texture_detail = 1;
    a.sprite_detail = 0;
    a.mode_w = 320;
    a.mode_h = 240;
    a.sfx_volume = 12;
    a.music_volume = 99;
    a.full_hud = 0;
    a.head_bob = 1;
    a.toggle_crouch = 1;
    n = config_format(&a, text, (int)sizeof(text));
    check_expect(n > 200 && n < CONFIG_MAX, "config: the file's text is empty or too long");
    memset(&b, 0x55, sizeof(b));
    b.key[INPUT_MENU] = 0;
    bad = config_parse(text, n, &b, &first);
    check_expect(bad == 0 && first == 0, "config: the program's own text was not all used");
    check_expect(memcmp(&a, &b, sizeof(a)) == 0, "config: a Config did not come back from its text the same");
    check_expect(strstr(text, "resolution=320x240") && strstr(text, "texture_detail=medium") && strstr(text, "key.use==")
                     && strstr(text, "key.jump=Num +") && strstr(text, "key.run=Space") && strstr(text, "mouse=view"),
                 "config: the text is not written as it is documented");
    /* A buffer that cannot hold it is refused, not cut. */
    check_expect(config_format(&a, text, 100) == -1, "config: a small buffer was not refused");

    /* A file written by hand: spaces, case, notes, CRLF and LF, comments, a last line with no newline. */
    {
        static const char hand[] =
            "  FOV = 100   # note\r\n"
            "Key.Use = Space\n"
            "# comment\n"
            "resolution= 512 x 384\n"
            "fog=NO\n"
            "bogus=1\n"
            "fov=5000\n"
            "texture_detail=ultra\n"
            "key.forward=Banana\n"
            "\n"
            "key.back=left button";
        memcpy(&b, &a, sizeof(b));
        bad = config_parse(hand, (int)sizeof(hand) - 1, &b, &first);
        check_expect(bad == 4 && first == 6, "config: the lines it cannot use were not counted, or not found at the first");
        check_expect(b.fov == 100 && b.key[INPUT_USE] == 0x20 && b.mode_w == 512 && b.mode_h == 384 && b.fog == 0,
                     "config: a hand-written file was not read through its notes, case and spaces");
        check_expect(b.texture_detail == 1 && b.key[INPUT_FORWARD] == a.key[INPUT_FORWARD] && b.key[INPUT_BACK] == 0x01,
                     "config: a line it could not use changed something, or a last line without a newline was lost");
    }

    /* The mode alone. */
    n = config_format(&a, text, (int)sizeof(text));
    check_expect(config_peek_mode(text, n, &w, &h) && w == 320 && h == 240, "config: the mode was not peeked");
    check_expect(!config_peek_mode("fov=75\nfog=on\n", 14, &w, &h) && !config_peek_mode("resolution=10x10\n", 17, &w, &h)
                     && !config_peek_mode("resolution=wide\n", 16, &w, &h) && !config_peek_mode("", 0, &w, &h),
                 "config: a mode was peeked from a file with none, or a silly one");
    check_expect(config_peek_mode("# x\r\nresolution = 400 x 300 # why\r\n", 33, &w, &h) && w == 400 && h == 300,
                 "config: a mode with spaces and a note was not peeked");
}

/* Cursor or View, and which pointer play asks for. */
static void check_mouse(void)
{
    mouse_defaults();
    check_expect(mouse_cursor() && mouse_pointer(0) == 1 && mouse_pointer(1) == 1,
                 "mouse: play does not start in cursor, or a screen has no pointer");
    mouse_set_cursor(0);
    check_expect(!mouse_cursor() && mouse_pointer(0) == 0 && mouse_pointer(1) == 1,
                 "mouse: view does not look in play, or a screen lost the pointer");
    printf("mouse: cursor and view\n");
}

/* A camera a step south of e, looking north at their middle. */
static void aim_person(Camera *cam, const Entity *e)
{
    float h;

    h = entity_height(e);
    if (h < 16.0f) {
        h = 64.0f;
    }
    camera_set(cam, e->x, e->y + h * 0.5f, e->z - 24.0f, 0.0f, 0.0f);
}

/* The blood billboard standing on the body, or 0. */
static Entity *splash_near(float x, float z)
{
    int i;

    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        float dx;
        float dz;

        e = people_at(i);
        if (!e || !people_is_effect(e)) {
            continue;
        }
        dx = e->x - x;
        dz = e->z - z;
        if (dx * dx + dz * dz < 128.0f * 128.0f) {
            return e;
        }
    }
    return 0;
}

/*
 * A street citizen has 1 health. The to-hit roll is made at 100, so it cannot
 * miss, and the damage is the weapon's: the same rolls as a blow that is not
 * sure, when that blow also hits. One such blow in Daggerfall kills them.
 * Every guard turns hostile, the splash is TEXTURE.380 record 0, wanted
 * outlives the people being dropped and built again, a hostile guard is not
 * talked to, and a killed guard stays as a corpse that can be looted.
 */
static void check_citizen(void)
{
    Character pc;
    Foe citizen;
    Foe plain;
    Blow sure;
    Blow rolled;
    Item sword;
    int i;
    int misses;
    int matched;
    int index;
    Level level;
    Camera cam;
    TravelPose at;
    Player ply;
    Entity *who;
    Entity *guard;
    float feet[3];
    float eye[3];
    float yaw;
    float before;
    float after;
    int hp;
    int k;
    int guards;
    int hostile;
    int seed;
    int file;
    int record;
    int female;
    int nloot;
    int nleft;
    unsigned talk_who;
    Entity *blood;
    Entity *body;
    Inventory found;
    Inventory pack;
    df_u8 origin[INVENTORY_MAX];
    char taken[64];
    Camera look;

    if (!character_new(&pc, RACE_BRETON, CLASS_WARRIOR, 0, "Test")) {
        check_expect(0, "citizen: no character");
        return;
    }
    item_make(&sword, ITEM_LONGSWORD, MATERIAL_IRON);
    pc.skill[SKILL_CRITICAL_STRIKE] = 0;
    combat_citizen(&citizen, 0);
    plain = citizen;
    plain.citizen = 0;
    misses = 0;
    matched = 0;
    for (i = 1; i <= 200; i++) {
        combat_seed((df_u32)i);
        combat_blow(&pc, &sword, 3, &citizen, &sure);
        if (sure.chance != 100 || !sure.hit || sure.damage < 1) {
            break;
        }
        combat_seed((df_u32)i);
        combat_blow(&pc, &sword, 3, &plain, &rolled);
        if (!rolled.hit) {
            misses++;
            continue;
        }
        if (rolled.damage != sure.damage || rolled.chance > 97) {
            break;
        }
        matched++;
    }
    check_expect(i == 201 && misses > 0 && matched > 0,
                 "citizen: a blow is not a sure hit at 100, or its damage is not the weapon's");
    check_expect(PEOPLE_CITIZEN_HP == 1, "citizen: health is not 1");

    index = daggerfall_index();
    if (index < 0) {
        return;
    }
    clock_set_minute(12.0f * 60.0f);
    entity_set_population(1);
    foes_set_on(1);
    foes_reset();
    combat_reset();
    watch_pardon();
    /* The watch test stands in the crowded middle. A journey's entrance is checked with goto. */
    {
        TravelWhere where;
        float mid[3];
        int px;
        int py;
        int chunk[2];

        worldmap_pixel_of(17, index, &px, &py);
        memset(&where, 0, sizeof(where));
        where.kind = TRAVEL_WORLD;
        where.located = 1;
        where.region = 17;
        where.index = index;
        where.has_back = 1;
        where.back_x = ((double)px + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
        where.back_z = ((double)(WORLDMAP_H - 1 - py) + 0.5) * (double)(STREAM_CHUNK * WORLDMAP_BLOCKS);
        chunk[0] = (int)(where.back_x / (double)STREAM_CHUNK);
        chunk[1] = (int)(where.back_z / (double)STREAM_CHUNK);
        mid[0] = (float)(where.back_x - (double)chunk[0] * (double)STREAM_CHUNK);
        mid[1] = 0.0f;
        mid[2] = (float)(where.back_z - (double)chunk[1] * (double)STREAM_CHUNK);
        travel_from_save(&where, mid, chunk, 0.0f, 0.0f);
    }
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    who = 0;
    guards = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;

        e = people_at(i);
        if (e && people_is_guard(e)) {
            guards++;
        }
        if (e && people_is_citizen(e) && !who) {
            who = e;
        }
    }
    check_expect(people_count() > 40 && who && people_health(who) == 1, "citizen: Daggerfall has no citizen at 1 health");
    if (!who || people_health(who) != 1) {
        travel_leave();
        return;
    }
    feet[0] = who->x;
    feet[1] = who->y;
    feet[2] = who->z;
    eye[0] = feet[0];
    eye[1] = feet[1] + WALK_EYE;
    eye[2] = feet[2];
    yaw = 0.0f;
    combat_seed(3u);
    check_expect(combat_strike(&pc, &sword, 3, &level.scene, feet, eye, yaw) > 0,
                 "citizen: one blow did not kill them");
    check_expect(combat_citizen_struck(&sure, &hp) && hp == 1 && sure.chance == 100 && sure.hit && sure.damage >= 1
                     && 1 - sure.damage <= 0,
                 "citizen: the killing blow was not 1 health taken by a sure hit");
    blood = splash_near(feet[0], feet[2]);
    file = 0;
    record = -1;
    if (blood) {
        wres_picture(blood->pose[ENTITY_WALK].slot[0], &file, &record);
    }
    check_expect(blood && people_is_effect(blood) && file == 380 && record == 0,
                 "citizen: the killing blow did not splash TEXTURE.380 record 0");
    hostile = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;

        e = people_at(i);
        if (e && people_is_guard(e) && people_hostile(e) && foes_minding(e)) {
            hostile++;
        }
    }
    /* Every loaded guard comes; when none is near, 2 to 5 more are made near (the street may be full: a citizen makes room). */
    check_expect(watch_wanted() && hostile >= 1 && hostile >= guards && hostile <= guards + 5 && (guards > 0 || hostile >= 2),
                 "citizen: not every guard came, or wanted was not set");
    taken[0] = 0;
    guard = watch_guard();
    check_expect(watch_halts() == 3 && watch_halt(0) && watch_halt(1) && watch_halt(2)
                     && strcmp(watch_halt(0), "HALT") == 0 && strcmp(watch_halt(1), "HALT") == 0
                     && strcmp(watch_halt(2), "HALT") == 0,
                 "citizen: the killing did not queue three HALT lines");
    check_expect(guard && people_is_guard(guard) && foes_minding(guard)
                     && enemy_kind(ENEMY_CITY_WATCH) && enemy_kind(ENEMY_CITY_WATCH)->bark_sound == 456
                     && enemy_kind(ENEMY_CITY_WATCH)->male == 399,
                 "citizen: no city watch (kind 146, archive 399) came");
    if (guard) {
        before = df_sqrt((guard->x - feet[0]) * (guard->x - feet[0]) + (guard->z - feet[2]) * (guard->z - feet[2]));
        /* The street's guards may all be far, so some are made near: the nearest is within the near ring doubled, and not out of sight across the town. */
        printf("citizen: the nearest guard to come stood %.0f units off\n", (double)before);
        check_expect(before < 2.0f * PEOPLE_GUARD_FAR + 200.0f, "citizen: the nearest guard to come was far across the street");
        character_new(&pc, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
        player_init(&ply, &pc);
        player_place(&ply, feet[0], feet[1], feet[2], yaw, 0.0f);
        for (k = 0; k < 60; k++) {
            foes_frame(&ply, &level.scene, 0.05f);
            watch_frame(0.05f);
        }
        after = df_sqrt((guard->x - feet[0]) * (guard->x - feet[0]) + (guard->z - feet[2]) * (guard->z - feet[2]));
        check_expect(foes_minding(guard) && (after < before - 8.0f || before < FOES_MELEE),
                     "citizen: the guard did not come for the player");
        people_set_health(guard, 1);
        feet[0] = guard->x;
        feet[1] = guard->y;
        feet[2] = guard->z;
        eye[0] = feet[0];
        eye[1] = feet[1] + WALK_EYE;
        eye[2] = feet[2];
        for (seed = 1; seed < 80 && !people_is_corpse(guard); seed++) {
            combat_seed((df_u32)seed);
            people_set_health(guard, 1);
            combat_strike(&pc, &sword, 3, &level.scene, feet, eye, yaw);
        }
        check_expect(people_is_corpse(guard) && guard->live && guard->act == ENTITY_DEAD
                         && enemy_kind(ENEMY_CITY_WATCH)->corpse_archive == 380
                         && enemy_kind(ENEMY_CITY_WATCH)->corpse_record == 1,
                     "citizen: the guard did not stay as the city watch corpse");
        file = 0;
        record = -1;
        wres_picture(guard->pose[ENTITY_DEAD].slot[0], &file, &record);
        check_expect(file == 380 && record == 1, "citizen: the corpse is not TEXTURE.380 record 1");
        aim_person(&look, guard);
        check_expect(loot_find(&level.scene, &look) == guard, "citizen: the use key did not find the corpse");
        nloot = loot_open(guard, pc.level, &found, origin);
        taken[0] = 0;
        file = nloot > 0 ? found.item[0].template_id : 0;
        if (nloot > 0) {
            item_name(&found.item[0], taken, (int)sizeof(taken));
            inventory_clear(&pack);
            inventory_add(&pack, &found.item[0]);
            loot_taken(guard, origin[0]);
        }
        nleft = loot_open(guard, pc.level, &found, origin);
        check_expect(nloot >= 1 && file >= ITEM_DAGGER && file <= 127 && pack.count == 1 && nleft == nloot - 1,
                     "citizen: the corpse had no weapon to take");
        aim_person(&look, guard);
        for (k = 0; k < 30; k++) {
            people_update(&look, 0.1f);
        }
        check_expect(people_is_corpse(guard) && guard->live, "citizen: the corpse went while the player stood over it");
        keep_forget(guard->key);
    }
    /* The splash is down before the people are built again, so a later frame does not clear a new person. */
    combat_frame(2.0f);
    check_expect(!splash_near(who->x, who->z), "citizen: the blood splash did not end");
    people_end();
    people_begin(&level.scene);
    people_fill(&cam);
    watch_pursue(&cam, feet);
    check_expect(watch_wanted(), "citizen: wanted did not survive the people being dropped and built again");
    body = 0;
    hostile = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;

        e = people_at(i);
        if (e && people_is_guard(e) && people_hostile(e)) {
            hostile++;
            if (!body) {
                body = e;
            }
        }
    }
    check_expect(hostile >= 1 && body, "citizen: no guard was hostile after the people were built again");
    if (body) {
        aim_person(&look, body);
        check_expect(!people_in_view(&look, TALK_REACH, &female, &talk_who),
                     "citizen: talk opened on a hostile guard");
        people_set_hostile(body, 0);
        check_expect(people_in_view(&look, TALK_REACH, &female, &talk_who),
                     "citizen: a guard who is not hostile could not be talked to");
    }
    watch_pardon();
    travel_leave();
    foes_reset();
    printf("citizen: 1 health, sure hit, weapon damage; %d ordinary misses, %d damages matched; "
           "%d guards in Daggerfall, %d hostile; corpse gave %s\n",
           misses, matched, guards, hostile, taken[0] ? taken : "nothing");
}

void check_menu(void);

/* A world all woodland, and one all sea: the journey's sums worked by hand. */
static int all_woods(int px, int py)
{
    (void)px;
    (void)py;
    return CLIMATE_WOODLANDS;
}

static int all_sea(int px, int py)
{
    (void)px;
    (void)py;
    return CLIMATE_OCEAN;
}

/* The journey's sums by hand, then the travel map by clicks, then what the map knows through a save. */
static void check_wider_world(void)
{
    Journey j;
    int day;
    float minute;

    /*
     * Woodland is the climate past the ocean by 8: terrain row 5, ease 250. On foot
     * ((102 x 256) >> 8) x (512 - 250) >> 8 = 102 x 262 >> 8 = 104 a pixel; ten are 1040.
     */
    journey_plan(100, 100, 110, 100, 0, 1, 0, JOURNEY_FOOT, all_woods, &j);
    check_expect(j.minutes == 1040 && j.hours == 18 && j.days == 1 && j.sea_pixels == 0, "journey: ten woodland pixels on foot");
    /* 18 hours of land at inns is no whole night past the first: 5 gold. */
    check_expect(j.inn_gold == 5 && j.fare == 0 && j.cost == 5, "journey: one night's inn");
    journey_plan(100, 100, 110, 100, 1, 1, 0, JOURNEY_FOOT, all_woods, &j);
    check_expect(j.minutes == 520, "journey: reckless is half");
    /* Camping: (300 x 104) >> 8 = 121 a pixel, no inn paid. */
    journey_plan(100, 100, 110, 100, 0, 0, 0, JOURNEY_FOOT, all_woods, &j);
    check_expect(j.minutes == 1210 && j.inn_gold == 0, "journey: camping");
    /* A horse: ((102 x 128) >> 8) x 262 >> 8 = 51 x 262 >> 8 = 52 a pixel. */
    journey_plan(100, 100, 110, 100, 0, 1, 0, JOURNEY_HORSE, all_woods, &j);
    check_expect(j.minutes == 520, "journey: a horse");
    /* Thirty sea pixels by ship: 51 each, 1530 minutes, 26 hours; the fare 25 x (30 / 24 + 1) = 50; the inn's first night. */
    journey_plan(100, 100, 130, 100, 0, 1, 1, JOURNEY_FOOT, all_sea, &j);
    check_expect(j.minutes == 1530 && j.sea_pixels == 30 && j.fare == 50 && j.inn_gold == 5 && j.cost == 55, "journey: by sea");
    journey_plan(100, 100, 130, 100, 0, 1, 0, JOURNEY_FOOT, all_sea, &j);
    check_expect(j.minutes == 30 * 255 && j.fare == 0, "journey: the sea without a ship");
    /* The longer axis sets the steps: 40 across and 10 down is 40 pixels. */
    journey_plan(0, 0, 40, 10, 0, 1, 0, JOURNEY_FOOT, all_woods, &j);
    check_expect(j.minutes == 40 * 104, "journey: a diagonal's steps");
    /* Arrival: 10:00 and 1000 minutes is 02:40 the next day; cautious waits for 07:10. */
    journey_arrival(10, 600.0f, 1000, 0, &day, &minute);
    check_expect(day == 11 && minute == 160.0f, "journey: a reckless arrival");
    journey_arrival(10, 600.0f, 1000, 1, &day, &minute);
    check_expect(day == 11 && minute == 430.0f, "journey: a cautious arrival before light");
    journey_arrival(10, 900.0f, 300, 1, &day, &minute);
    check_expect(day == 11 && minute == 430.0f, "journey: a cautious arrival past 17:00");

    /* The map knows classic's places from the start; one learnt stays, through a save. */
    {
        int n;
        int k;
        int known;
        int unknown;
        int px;
        int py;
        int region;
        int index;
        int type;
        int start;

        atlas_new_game();
        n = worldmap_locations();
        known = 0;
        unknown = -1;
        for (k = 0; k < n; k++) {
            worldmap_location_info(k, &px, &py, &region, &index, &type, &start);
            known += atlas_known(k);
            if (!start && unknown < 0) {
                unknown = k;
            }
        }
        check_expect(n == 15251 && known == 11199 && unknown >= 0, "atlas: a new game knows classic's 11199 places");
        atlas_learn(unknown);
        check_expect(atlas_known(unknown) && atlas_changed_count() == 1, "atlas: a place learnt");
        {
            SaveState out;
            SaveState in;
            char path[260];

            save_use_file("DATA\\TEST4.SAV");
            keep_clear();
            pile_new_game();
            mem_zero(&out, sizeof(out));
            character_new(&out.sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
            inventory_clear(&out.pack);
            check_expect(save_write(&out), "atlas: a save did not write");
            atlas_new_game();
            check_expect(!atlas_known(unknown), "atlas: a new game knows the learnt place");
            check_expect(save_read(&in) && atlas_known(unknown) && atlas_changed_count() == 1, "atlas: the learnt place did not come back");
            check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "atlas: the test's file did not go");
        }
        atlas_new_game();
        /* Every known place of a one-screen province is on its map's panel: the maps' corners are right. */
        {
            int off;
            int checked;

            off = 0;
            checked = 0;
            for (k = 0; k < n; k++) {
                int ox;
                int oy;
                int scale;
                int x;
                int y;

                worldmap_location_info(k, &px, &py, &region, &index, &type, &start);
                if (!start || !travelmap_region_origin(region, 0, &ox, &oy, &scale) || travelmap_region_origin(region, 1, &ox, &oy, &scale)) {
                    continue;
                }
                travelmap_region_origin(region, 0, &ox, &oy, &scale);
                x = (px - ox) * scale;
                y = (py - oy) * scale;
                checked++;
                if (x < 0 || x >= 320 || y < 0 || y >= 160) {
                    if (off < 6) {
                        printf("travel map: region %d's place at %d %d is off its map, at %d %d\n", region, px, py, x, y);
                    }
                    off++;
                }
            }
            check_expect(checked > 5000 && off == 0, "travel map: a province's places are off its map");
        }
    }

    /* The map by clicks: the world, Daggerfall's province, Daggerfall's dot, yes, the popup, begin, the countdown. */
    {
        static const Rect begin = { 222, 98, 48, 10 };
        static const Rect reckless = { 50, 61, 108, 9 };
        static const Rect foot = { 163, 51, 108, 9 };
        Player p;
        Character sheet;
        int px;
        int py;
        int sx;
        int sy;
        int region;
        int index;
        int cautious;
        int k;
        int dagger;
        int ox;
        int oy;
        int scale;
        int x;
        int y;
        int wy;
        int found;
        Journey made;

        dagger = daggerfall_index();
        character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
        player_init(&p, &sheet);
        p.pack.gold = 1000u;
        worldmap_pixel_of(17, dagger, &px, &py);
        /* From Sentinel's pixel: far enough for days of road. */
        {
            static DunRef rows[4096];
            int n;
            int i;

            n = exterior_list_region(20, rows, 4096);
            n = n > 4096 ? 4096 : n;
            sx = px;
            sy = py;
            for (i = 0; i < n; i++) {
                if (df_eq(rows[i].name, "Sentinel")) {
                    worldmap_pixel_of(20, rows[i].index, &sx, &sy);
                }
            }
        }
        check_expect(travelmap_open(&p, sx, sy, 20), "travel map: it did not open");
        travelmap_compose();
        write_screen_with("build/travel0.ppm", travelmap_pixels(), 1);
        {
            const unsigned char *pix;
            int i;
            int dark;
            int lit;

            pix = travelmap_pixels();
            dark = 0;
            for (i = 0; i < 320 * 200; i++) {
                dark += pix[i] == 244;
            }
            travelmap_step(0.5f);
            check_expect(travelmap_changed(), "travel map: the province blink did not ask to redraw");
            travelmap_compose();
            pix = travelmap_pixels();
            lit = 0;
            for (i = 0; i < 320 * 200; i++) {
                lit += pix[i] == 244;
            }
            check_expect(lit > dark + 100, "travel map: the player's province did not blink");
        }
        /* The province picker: a point of the world's picture whose mask says Daggerfall. */
        travelmap_region_origin(17, 0, &ox, &oy, &scale);
        found = 0;
        for (wy = 0; wy < 170 && !found; wy++) {
            for (x = 0; x < 320 && !found; x++) {
                travelmap_point(x, wy);
                if (df_eq(travelmap_line(), "Daggerfall")) {
                    travelmap_click(x, wy);
                    found = travelmap_region() == 17;
                }
            }
        }
        check_expect(found, "travel map: Daggerfall's province not picked from the world");
        travelmap_compose();
        write_screen_with("build/travel1.ppm", travelmap_pixels(), 1);
        x = (px - ox) * scale;
        y = (py - oy) * scale + 12;
        travelmap_point(x, y);
        check_expect(df_eq(travelmap_line(), "Daggerfall"), "travel map: Daggerfall's dot is not named under the pointer");
        travelmap_click(x, y);
        k = travelmap_place();
        check_expect(k == worldmap_location_id(px, py), "travel map: Daggerfall's dot is not the place chosen");
        travelmap_compose();
        write_screen_with("build/travel2.ppm", travelmap_pixels(), 1);
        /* Yes: the popup. */
        travelmap_click(104 + 4, 112 + 4);
        travelmap_click(reckless.x + 2, reckless.y + 2);
        travelmap_click(foot.x + 2, foot.y + 2);
        travelmap_compose();
        write_screen_with("build/travel3.ppm", travelmap_pixels(), 1);
        travelmap_click(begin.x + 2, begin.y + 2);
        for (k = 0; k < 1000 && travelmap_step(0.05f) != TRAVELMAP_GO; k++) {
        }
        check_expect(k < 1000, "travel map: the countdown never ended");
        travelmap_chosen(&region, &index, &made, &cautious);
        journey_plan(sx, sy, px, py, 1, 1, 0, JOURNEY_FOOT, 0, &j);
        check_expect(region == 17 && index == dagger && !cautious && made.minutes == j.minutes && made.days > 1,
                     "travel map: the journey is not to Daggerfall, reckless, on foot, at the sums");
        /* The town map of Daggerfall: the whole city fitted, its kinds coloured, an armorer named under the pointer, panned and zoomed by clicks. */
        {
            static unsigned char bytes[TOWN_MAP_MOST];
            const TownBuilding *rows;
            int w;
            int h;
            int n;
            int i;
            int cx;
            int cy;
            int named;

            check_expect(town_automap(17, dagger, bytes, &w, &h) && w == 512 && h == 512, "town map: Daggerfall's 8 x 8 blocks");
            named = 0;
            for (i = 0; i < w * h; i++) {
                named += townmap_colour(bytes[i], 0) != 0;
            }
            check_expect(named > 10000, "town map: Daggerfall's buildings are not on its map");
            check_expect(townmap_colour(16, 0) && townmap_colour(16, 0) != townmap_colour(3, 0) && townmap_colour(12, 0) != townmap_colour(3, 0)
                         && !townmap_colour(0, 0) && !townmap_colour(251, 0) && townmap_colour(251, 1), "town map: the kinds' colours or the modes");
            check_expect(townmap_open(17, dagger, 16000.0f, 16000.0f, 0.0f), "town map: it did not open");
            check_expect(townmap_scale() == 2, "town map: Daggerfall is not fitted at half size");
            townmap_compose();
            write_screen("build/townmap0.ppm", townmap_pixels());
            /* An armorer under the pointer: its building's cell from the town's own list, on the screen at half size. */
            rows = town_buildings(17, dagger, &n);
            named = 0;
            for (i = 0; rows && i < n && !named; i++) {
                int sx;
                int sy;

                if (rows[i].type != BUILDING_ARMORER) {
                    continue;
                }
                townmap_centre(&cx, &cy);
                /* Each map pixel is 64 units; half size puts two map pixels in each screen pixel. */
                for (sy = -3; sy <= 3 && !named; sy++) {
                    for (sx = -3; sx <= 3 && !named; sx++) {
                        int mx;
                        int my;

                        mx = df_ifloor(rows[i].x / 64.0f);
                        my = h - 1 - df_ifloor(rows[i].z / 64.0f);
                        townmap_point(160 + (mx - cx) / 2 + sx, 85 + (my - cy) / 2 + sy);
                        named = df_eq(townmap_line(), "Armorer");
                    }
                }
            }
            check_expect(named, "town map: no armorer named under the pointer");
            townmap_centre(&cx, &cy);
            townmap_click(106, 172);
            townmap_click(238, 172);
            {
                int nx;
                int ny;

                townmap_centre(&nx, &ny);
                check_expect(ny == cy - 32 && townmap_scale() == 1, "town map: the up arrow at half size or the zoom in");
            }
            townmap_wheel(2);
            check_expect(townmap_scale() == -3, "town map: the wheel's zoom");
            townmap_compose();
            write_screen("build/townmap1.ppm", townmap_pixels());
            {
                float mx;
                float my;
                float angle;
                int cx2;
                int cy2;
                int nx;
                int ny;

                townmap_compose();
                townmap_click(5, 175);
                townmap_compose();
                check_expect(townmap_marker(&mx, &my, &angle) && angle == 0.0f, "town map: the player's arrow is not placed, or not north");
                townmap_centre(&cx2, &cy2);
                townmap_drag(4, 0);
                townmap_centre(&nx, &ny);
                check_expect(nx < cx2, "town map: a drag to the right did not move the map with the pointer");
                check_expect(townmap_repeats(106, 172) && !townmap_repeats(282, 172), "town map: which buttons repeat");
            }
            check_expect(townmap_key('V') == TOWNMAP_TRAVEL && townmap_click(282, 172) == TOWNMAP_LEAVE, "town map: V or Exit");
            townmap_close();
            /* Talk pointed out an armorer: opened again, the map says so and rings its cell in the talk's ink. */
            for (i = 0; rows && i < n && rows[i].type != BUILDING_ARMORER; i++) {
            }
            check_expect(rows && i < n, "town map: Daggerfall has no armorer to mark");
            townmap_mark(17, dagger, rows[i].x, rows[i].z, BUILDING_ARMORER);
            check_expect(townmap_open(17, dagger, rows[i].x, rows[i].z, 0.0f) && strstr(townmap_line(), "marked"),
                         "town map: the pointed-out armorer is not named as marked");
            townmap_compose();
            {
                int ink;
                int k;

                /* Below the map's line of words, which is in the same ink, and above the buttons. */
                ink = 0;
                for (k = 14 * CANVAS_W; k < 170 * CANVAS_W; k++) {
                    ink += townmap_pixels()[k] == CANVAS_INK;
                }
                check_expect(ink >= 16, "town map: no ring round the marked building");
            }
            townmap_close();
            check_expect(!townmap_marked(17, dagger + 1, &cx, &cy), "town map: another town shows the mark");
        }
        /* Find: typed "dagger", Daggerfall first of what has it in its name, its ring flashing, then the question. */
        travelmap_close();
        check_expect(travelmap_open(&p, sx, sy, 20), "travel map: it did not open again");
        travelmap_click(4 + 2, 175 + 2);
        {
            static const char typed[] = "DAGGER";
            int c;

            for (c = 0; typed[c]; c++) {
                travelmap_key(typed[c]);
            }
        }
        travelmap_key(0x0d);
        check_expect(travelmap_found() >= 2, "travel map: Find listed fewer than two places with dagger in their names");
        travelmap_compose();
        write_screen_with("build/travel4.ppm", travelmap_pixels(), 1);
        /* The list's first line: Daggerfall, whose name begins with it. */
        travelmap_click(70 + 12, 60 + 6 + 2);
        check_expect(travelmap_region() == 17 && travelmap_place() < 0, "travel map: the list's first line did not go to Daggerfall's province");
        for (k = 0; k < 100 && travelmap_place() < 0; k++) {
            travelmap_step(0.05f);
        }
        check_expect(travelmap_place() == worldmap_location_id(px, py) && k > 20,
                     "travel map: Daggerfall not asked about after its flash");
        printf("travel map: Sentinel to Daggerfall reckless on foot, %d days, %d gold; Find; build/travel0..4.ppm\n", made.days, made.cost);
        travelmap_close();
    }
}

/* The engine's automap polygons' edges, into a 640 x 480 picture (y from the bottom), for a look. */
static df_u8 g_poly_pic[640 * 480];
static void draw_polygon(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb)
{
    int i;

    (void)ctx;
    for (i = 0; i < n; i++) {
        const AutomapCorner *a;
        const AutomapCorner *b;
        int k;

        a = &corner[i];
        b = &corner[(i + 1) % n];
        for (k = 0; k <= 32; k++) {
            int x;
            int y;

            x = (int)(a->x + (b->x - a->x) * (float)k / 32.0f);
            y = 479 - (int)(a->y + (b->y - a->y) * (float)k / 32.0f);
            if (x >= 0 && x < 640 && y >= 0 && y < 480) {
                g_poly_pic[y * 640 + x] = (df_u8)(1 + ((rgb >> 8) & 0xff) / 2);
            }
        }
    }
}

static void write_polygons(const char *path, const Scene *scene, const AutomapView *view)
{
    FILE *f;
    int i;

    mem_zero(g_poly_pic, sizeof(g_poly_pic));
    automap_polygons(scene, view, draw_polygon, 0);
    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P5\n640 480\n255\n");
    for (i = 0; i < 640 * 480; i++) {
        fputc(g_poly_pic[i] ? 128 + g_poly_pic[i] / 2 : 0, f);
    }
    fclose(f);
}

/* The player's arrow as its own picture: an arrow, tip up at step 0, outlined, turned at a quarter. */
static void check_marker(void)
{
    static unsigned char up[MARKER_SIDE * MARKER_SIDE];
    static unsigned char right[MARKER_SIDE * MARKER_SIDE];
    int fill;
    int edge;
    int top_row;
    int right_col;
    int i;

    marker_compose(up, 0);
    marker_compose(right, MARKER_STEPS / 4);
    fill = 0;
    edge = 0;
    top_row = MARKER_SIDE;
    right_col = -1;
    for (i = 0; i < MARKER_SIDE * MARKER_SIDE; i++) {
        fill += up[i] == MARKER_FILL;
        edge += up[i] == MARKER_EDGE;
        if (up[i] && i / MARKER_SIDE < top_row) {
            top_row = i / MARKER_SIDE;
        }
        if (right[i] && i % MARKER_SIDE > right_col) {
            right_col = i % MARKER_SIDE;
        }
    }
    check_expect(fill > 60 && edge > 60, "marker: the arrow has no fill or no outline");
    check_expect(top_row <= 2 && up[top_row * MARKER_SIDE + MARKER_SIDE / 2 - 1], "marker: step 0's tip is not at the top middle");
    check_expect(right_col >= MARKER_SIDE - 3 && right[(MARKER_SIDE / 2) * MARKER_SIDE + right_col - 1], "marker: a quarter turn does not point right");
    check_expect(marker_step(0.0f) == 0 && marker_step(DF_TAU / 4.0f) == MARKER_STEPS / 4 && marker_step(-0.01f) == 0,
                 "marker: the steps of a turn");
}

/* A horse and a cart: sold by a general store, owned not carried, ridden outdoors at classic's speed, its picture, the journey by horse. */
static void check_riding(void)
{
    Player p;
    Character sheet;
    Shop shop;
    Inventory stock;
    df_u8 origin[INVENTORY_MAX];
    Item horse;
    Input in;
    CfaPicture cfa;
    Journey foot;
    Journey ridden;
    MemHeap *scratch;
    MemHeap *was;
    float x0;
    float walked;
    float rode;
    int i;
    int horses;
    int carts;

    /* The hooves: DFU's HorseClop at half speed or less, HorseClop2 faster, the cart's rattle, none on foot. */
    check_expect(ride_hoof_clip(RIDE_HORSE, 1) == SND_HORSE_CLOP && ride_hoof_clip(RIDE_HORSE, 0) == SND_HORSE_CLOP_PACE
                     && ride_hoof_clip(RIDE_CART, 0) == SND_HORSE_AND_CART && ride_hoof_clip(RIDE_FOOT, 0) == -1,
                 "riding: the hooves' clips");
    /* The cart's picture in the pack: the wagon's container icon, INVE16I0.CIF's fourth, 57 x 30. */
    {
        CifSet icons;

        check_expect(cif_open("INVE16I0.CIF", &icons) && cif_frame(&icons, 3) && cif_frame(&icons, 3)->w == 57
                         && cif_frame(&icons, 3)->h == 30,
                     "riding: the wagon's icon is not INVE16I0.CIF's fourth");
        cif_close(&icons);
    }
    /* The picture steps every 0.125 s while moving, and stands on its first frame. */
    check_expect(ride_frame(0, 0.1f) == 0 && ride_frame(1, 0.13f) == 1 && ride_frame(1, 0.25f) == 3 && ride_frame(0, 0.1f) == 0,
                 "riding: the picture's frames do not step");
    walked = 0.0f;
    rode = 0.0f;
    keep_clear();
    loot_new_game(4242u);
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    /* A general store's shelves, any day: one horse and one cart. */
    mem_zero(&shop, sizeof(shop));
    shop.type = BUILDING_GENERAL;
    shop.quality = 10;
    shop.prices = 1000;
    shop.key.place = (3u << 30) | 77u;
    shop.key.thing = 0xFE000000u;
    shop_stock(&shop, 1, 500, &stock, origin);
    horses = 0;
    carts = 0;
    for (i = 0; i < stock.count; i++) {
        horses += stock.item[i].template_id == ITEM_HORSE;
        carts += stock.item[i].template_id == ITEM_CART;
    }
    check_expect(horses == 1 && carts == 1, "riding: a general store has not one horse and one cart");
    check_expect(item_make(&horse, ITEM_HORSE, 0) && shop_buys(&shop, &horse), "riding: a general store does not deal in horses");
    /* Owned, not carried: a horse weighs nothing in the pack. */
    inventory_clear(&p.pack);
    check_expect(ride_toggle(&p, 1) != 0 && p.riding == RIDE_FOOT, "riding: mounted with no horse");
    inventory_add(&p.pack, &horse);
    check_expect(inventory_weight(&p.pack) == 0.0f && inventory_has(&p.pack, ITEM_HORSE), "riding: a horse weighs in the pack");
    check_expect(ride_toggle(&p, 0) != 0 && p.riding == RIDE_FOOT, "riding: mounted indoors");
    check_expect(ride_toggle(&p, 1) == 0 && p.riding == RIDE_HORSE, "riding: the horse not mounted outdoors");
    check_expect(ride_toggle(&p, 1) != 0 && p.riding == RIDE_FOOT, "riding: not got down");
    /* Speed 50: walking (50 + 150 - 25) / 39.5, a horse (50 + 375) / 39.5, a cart (50 + 250) / 39.5. */
    check_expect(df_absf(rules_ride_speed(50, 0) - 425.0f / 39.5f) < 0.001f && df_absf(rules_ride_speed(50, 1) - 300.0f / 39.5f) < 0.001f,
                 "riding: classic's speeds");
    /* On the hold's floor a second's walk and a second's ride: the horse goes 425 / 175 as far, and the run key does nothing on it. */
    {
        Dungeon hold;
        Level level;
        float x;
        float y;
        float z;
        float yaw;
        float pitch;

        if (dungeon_load(&hold) && dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
            Camera cam;

            camera_set(&cam, x, y, z, yaw, pitch);
            dunstream_fill(&cam);
            p.sheet.attribute[ATTR_SPEED] = 50;
            memset(&in, 0, sizeof(in));
            player_place(&p, x, y, z, yaw, pitch);
            walk(&p, &level.scene, &in, 60);
            in.move_f = 1;
            x0 = p.body.feet[0] + p.body.feet[2];
            walked = walk(&p, &level.scene, &in, 15);
            player_place(&p, x, y, z, yaw, pitch);
            in.move_f = 0;
            walk(&p, &level.scene, &in, 60);
            in.move_f = 1;
            in.held = INPUT_BIT(INPUT_RUN);
            p.riding = RIDE_HORSE;
            rode = walk(&p, &level.scene, &in, 15);
            p.riding = RIDE_FOOT;
            (void)x0;
            check_expect(rode > walked * 2.2f && rode < walked * 2.7f && !p.running, "riding: a horse's quarter second is not 425 / 175 of a walk's");
            dunstream_close();
        }
    }
    /* The travel map's journey: by horse the land takes half the minutes on foot. */
    check_expect(ride_journey(&p) == JOURNEY_HORSE, "riding: the journey is not by horse");
    journey_plan(100, 100, 110, 100, 0, 1, 0, JOURNEY_FOOT, 0, &foot);
    journey_plan(100, 100, 110, 100, 0, 1, 0, ride_journey(&p), 0, &ridden);
    check_expect(ridden.minutes * 2 <= foot.minutes + 10 && ridden.minutes > 0, "riding: a horse's journey is not half the time");
    ride_get_down(&p);
    /* The picture: MRED00I0.CFA, 106 x 105, four frames, the frame moving only while the player does. */
    scratch = mem_heap_new();
    was = mem_heap_use(scratch);
    check_expect(cfa_load("MRED00I0.CFA", &cfa) && cfa.width == 106 && cfa.height == 105 && cfa.frames == 4, "riding: the horse's picture");
    {
        int drawn;

        drawn = 0;
        for (i = 0; cfa.pixels && i < cfa.width * cfa.height; i++) {
            drawn += cfa.pixels[i] != 0;
        }
        check_expect(drawn > 1000, "riding: the horse's first frame is empty");
    }
    check_expect(cfa_load("MRED01I0.CFA", &cfa) && cfa.frames == 4, "riding: the cart's picture");
    mem_heap_use(was);
    mem_heap_delete(scratch);
    check_expect(ride_frame(0, 1.0f) == 0 && ride_frame(1, 0.13f) == 1 && ride_frame(1, 0.25f) == 3 && ride_frame(1, 0.125f) == 0
                 && ride_frame(0, 0.0f) == 0, "riding: the picture's frames");
    keep_clear();
    printf("riding: a general store's horse and cart, owned not carried, mounted outdoors, %.1f against %.1f units a quarter second, the journey by horse, MRED00I0.CFA's four frames\n",
           rode, walked);
}

/* The wagon by clicks: none without a cart, too far when out of reach, things in and out, its limit, gold, and through a save. */
static void check_wagon(void)
{
    static const Rect wagon_button = { 226, 14, 31, 14 };
    static const Rect remove_button = { 226, 80, 31, 14 };
    Player p;
    Character sheet;
    Item it;
    int sword;

    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    wagon_new_game();
    check_expect(item_make(&it, ITEM_LONGSWORD, 0) && (sword = inventory_add(&p.pack, &it)) >= 0, "wagon: no sword");
    /* No cart: no wagon. */
    wagon_set_reachable(1);
    check_expect(bag_open(&p, 0), "wagon: the inventory did not open");
    bag_click(wagon_button.x + 2, wagon_button.y + 2);
    check_expect(!bag_at_wagon() && bag_note() && strstr(bag_note(), "no wagon"), "wagon: opened with no cart");
    bag_close();
    /* A cart, but out of reach. */
    check_expect(item_make(&it, ITEM_CART, 0) && inventory_add(&p.pack, &it) >= 0, "wagon: no cart");
    wagon_set_reachable(0);
    bag_open(&p, 0);
    bag_click(wagon_button.x + 2, wagon_button.y + 2);
    check_expect(!bag_at_wagon() && bag_note() && strstr(bag_note(), "too far"), "wagon: opened out of reach");
    bag_close();
    /* In reach: the sword in, by Remove on it; back out by a click on it. */
    wagon_set_reachable(1);
    bag_open(&p, 0);
    bag_click(wagon_button.x + 2, wagon_button.y + 2);
    check_expect(bag_at_wagon(), "wagon: not opened in reach with a cart");
    bag_click(remove_button.x + 2, remove_button.y + 2);
    bag_click(177, 60);
    check_expect(wagon_contents()->count == 1 && wagon_contents()->item[0].template_id == ITEM_LONGSWORD
                 && !inventory_has(&p.pack, ITEM_LONGSWORD), "wagon: the sword did not go in");
    bag_compose();
    write_screen("build/wagon0.ppm", bag_pixels());
    /* The cart itself never goes in: the pack's one thing left is the cart. */
    bag_click(177, 60);
    check_expect(inventory_has(&p.pack, ITEM_CART) && wagon_contents()->count == 1, "wagon: the cart went into its own wagon");
    bag_click(275, 60);
    check_expect(wagon_contents()->count == 0 && inventory_has(&p.pack, ITEM_LONGSWORD), "wagon: the sword did not come back out");
    bag_close();
    /* A shop's Wagon button: the left list becomes the wagon, a sword sold out of it, the gold to the pack. */
    {
        static const Rect trade_wagon = { 226, 14, 31, 14 };
        static const Rect trade_action = { 226, 134, 31, 14 };
        Shop shop;
        df_u32 gold;

        mem_zero(&shop, sizeof(shop));
        shop.type = BUILDING_WEAPONS;
        shop.quality = 10;
        shop.prices = 1000;
        check_expect(item_make(&it, ITEM_LONGSWORD, 0) && inventory_add(wagon_contents(), &it) >= 0, "wagon: no sword for the wagon");
        gold = p.pack.gold;
        check_expect(bag_open_trade(&p, &shop, TRADE_SELL, 1), "wagon: the trade did not open");
        bag_click(trade_wagon.x + 2, trade_wagon.y + 2);
        /* The left list's first row: the wagon's sword onto the counter, then sold. */
        bag_click(177, 60);
        check_expect(wagon_contents()->count == 0, "wagon: the sword did not leave the wagon for the counter");
        bag_click(trade_action.x + 2, trade_action.y + 2);
        check_expect(p.pack.gold > gold && wagon_contents()->count == 0,
                     "wagon: selling from the wagon paid nothing");
        bag_close();
        /* Laid on the counter and taken back: home to the wagon, not the pack. */
        check_expect(item_make(&it, ITEM_DAGGER, 0) && inventory_add(wagon_contents(), &it) >= 0, "wagon: no dagger");
        check_expect(bag_open_trade(&p, &shop, TRADE_SELL, 1), "wagon: the trade did not open again");
        bag_click(trade_wagon.x + 2, trade_wagon.y + 2);
        bag_click(177, 60);
        bag_close();
        check_expect(wagon_contents()->count == 1 && wagon_contents()->item[0].template_id == ITEM_DAGGER, "wagon: the unsold dagger did not go back to the wagon");
        inventory_clear(wagon_contents());
    }
    /* Its limit: 750 kg, a thing too heavy refused. */
    it.weight = 800.0f;
    check_expect(!wagon_fits(&it), "wagon: 800 kg fits");
    it.weight = 5.0f;
    check_expect(wagon_fits(&it), "wagon: 5 kg does not fit");
    check_expect(item_make(&it, ITEM_HORSE, 0) && wagon_refuses(&it), "wagon: a horse is not refused");
    /* Through a save, with gold in it. */
    {
        SaveState out;
        SaveState in;
        char path[260];

        loot_gold_item(&it, 250);
        inventory_add(wagon_contents(), &it);
        item_make(&it, ITEM_DAGGER, 0);
        inventory_add(wagon_contents(), &it);
        save_use_file("DATA\\TEST4.SAV");
        keep_clear();
        pile_new_game();
        mem_zero(&out, sizeof(out));
        out.sheet = sheet;
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "wagon: a save did not write");
        wagon_new_game();
        check_expect(save_read(&in) && wagon_contents()->count == 2 && wagon_contents()->item[0].count == 250
                     && wagon_contents()->item[1].template_id == ITEM_DAGGER, "wagon: what was in it did not come back");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "wagon: the test's file did not go");
    }
    wagon_new_game();
    keep_clear();
    printf("wagon: none without a cart, too far out of reach, a sword in and out by clicks, the cart refused, 750 kg, gold and things through a save; build/wagon0.ppm\n");
}

/* Holidays: the cities' sixteen by date, Sun's Rest shutting shops, the festivals halving prices. */
static void check_holidays(void)
{
    const Holiday *h;
    Shop shop;
    Item sword;
    Item shield;
    int full;

    check_expect(holiday_table()->count == 16, "holidays: not the cities' sixteen");
    h = holiday_on(6, 19);
    check_expect(h && df_eq(h->name, "Sun's Rest") && (h->flags & HOLIDAY_SHOPS_SHUT), "holidays: Sun's Height 20 is not Sun's Rest, shops shut");
    h = holiday_on(0, 0);
    check_expect(h && df_eq(h->name, "New Life Festival"), "holidays: Morning Star 1 is not the New Life Festival");
    check_expect(holiday_on(6, 18) == 0, "holidays: an ordinary day has a holiday");
    h = holiday_on(2, 6);
    check_expect(h && (h->flags & HOLIDAY_TEMPLE_CURE), "holidays: First Planting has no temple cure");
    /* The Merchants' Festival halves every line; the Warriors Festival a weapon's, not a shield's. */
    mem_zero(&shop, sizeof(shop));
    shop.type = BUILDING_ARMORER;
    shop.quality = 10;
    shop.prices = 1000;
    item_make(&sword, ITEM_LONGSWORD, 0);
    item_make(&shield, ITEM_KITE_SHIELD, 0);
    full = shop_cost(&shop, &sword);
    shop.holiday = holiday_on(6, 9)->flags;
    check_expect(shop_cost(&shop, &sword) == full / 2, "holidays: the Merchants' Festival does not halve");
    shop.holiday = holiday_on(10, 19)->flags;
    check_expect(shop_cost(&shop, &sword) == full / 2, "holidays: the Warriors Festival does not halve a weapon");
    shop.holiday = 0;
    full = shop_cost(&shop, &shield);
    shop.holiday = holiday_on(10, 19)->flags;
    check_expect(shop_cost(&shop, &shield) == full, "holidays: the Warriors Festival halves a shield");
}

/*
 * Theft: the pickpocket chance by hand (a citizen's has no level term, an enemy's five a level,
 * always within 5..95), the gold roll's edges, an attempt's use of the skill, its gold and its
 * once-a-person memory, a failure's cost to the reputation with no court, and the dice's spread.
 */
static void check_theft(void)
{
    Player p;
    Character sheet;
    char line[64];
    int amount[7];
    int caught;
    int nothing;
    int golds;
    int total;
    int gold;
    int legal;
    int people;
    int uses;
    int i;

    /* The sums by hand: DFU's own example, a level 12 thief at a level 3 foe gains 45, a level 2 at a level 6 loses 20. */
    check_expect(rules_pickpocket_chance(50, 0) == 50, "theft: a citizen's chance is the skill");
    check_expect(rules_pickpocket_chance(0, 0) == 5 && rules_pickpocket_chance(100, 0) == 95, "theft: the chance is not kept within 5..95");
    check_expect(rules_pickpocket_chance(30, 12 - 3) == 75 && rules_pickpocket_chance(30, 2 - 6) == 10, "theft: five a level between thief and foe");
    check_expect(rules_pickpocket_chance(90, 9) == 95 && rules_pickpocket_chance(10, -9) == 5, "theft: a level's term breaks the clamp");
    /* The lowest 33 of 100 find nothing; the rest 1 to 6. */
    check_expect(rules_pickpocket_gold(32, 5) == 0 && rules_pickpocket_gold(33, 0) == 1 && rules_pickpocket_gold(99, 5) == 6,
                 "theft: the gold's edges");

    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    p.pack.gold = 0;
    combat_seed(31u);
    theft_forget();
    court_forget();

    /* One try a person: the second rolls nothing and tallies nothing, and forgetting lets it be tried again. */
    p.sheet.skill[SKILL_PICKPOCKET] = 95;
    uses = p.sheet.skill_uses[SKILL_PICKPOCKET];
    theft_pickpocket(&p, 12345u, 0, 17, &gold);
    check_expect(p.sheet.skill_uses[SKILL_PICKPOCKET] == uses + 1, "theft: an attempt did not count a use of Pickpocket");
    i = (int)p.pack.gold;
    check_expect(theft_pickpocket(&p, 12345u, 0, 17, &gold) == THEFT_TRIED && gold == 0 && (int)p.pack.gold == i
                 && p.sheet.skill_uses[SKILL_PICKPOCKET] == uses + 1,
                 "theft: a person was tried twice");
    check_expect(theft_tried(12345u) && !theft_tried(54321u), "theft: a person tried is not told as tried, or one not tried is");
    theft_forget();
    check_expect(!theft_tried(12345u) && theft_pickpocket(&p, 12345u, 0, 17, &gold) != THEFT_TRIED,
                 "theft: forgetting did not let them be tried again");

    /* The dice at Pickpocket 50: half seen, two in three of the rest with gold, each of 1 to 6 coming. */
    p.sheet.skill[SKILL_PICKPOCKET] = 50;
    p.pack.gold = 0;
    theft_forget();
    caught = nothing = golds = total = 0;
    for (i = 0; i < 7; i++) {
        amount[i] = 0;
    }
    for (i = 0; i < 3000; i++) {
        int r;

        /* The memory holds 64: a new person each time, and it lets the oldest go. */
        r = theft_pickpocket(&p, 1000u + (unsigned)i, 0, 17, &gold);
        if (r == THEFT_CAUGHT) {
            caught++;
        } else if (r == THEFT_NOTHING) {
            nothing++;
        } else if (r == THEFT_GOLD && gold >= 1 && gold <= 6) {
            golds++;
            total += gold;
            amount[gold]++;
        } else {
            check_expect(0, "theft: an attempt came to no result");
        }
    }
    check_expect(caught > 1350 && caught < 1650, "theft: Pickpocket 50 is not seen about half the time");
    check_expect(golds * 100 / (golds + nothing) > 60 && golds * 100 / (golds + nothing) < 74, "theft: gold is not found two times in three");
    check_expect(amount[1] > 0 && amount[2] > 0 && amount[3] > 0 && amount[4] > 0 && amount[5] > 0 && amount[6] > 0,
                 "theft: not every sum of 1 to 6 gold came");
    check_expect((int)p.pack.gold == total, "theft: the gold is not what was found");

    /* Seen: the legal reputation's loss and half with the people, and no court: the watch is the answer. */
    standing_region(17)->legal = 50;
    legal = standing_region(17)->legal;
    /* An earlier test left the people at the floor, where a loss cannot show. */
    standing_change(faction_people_of(17)->id, 80, 0);
    people = standing_rep(faction_people_of(17)->id);
    p.sheet.skill[SKILL_PICKPOCKET] = 5;
    theft_forget();
    court_forget();
    for (i = 0; i < 400 && theft_pickpocket(&p, 9000u + (unsigned)i, 0, 17, &gold) != THEFT_CAUGHT; i++) {
    }
    check_expect(i < 400, "theft: Pickpocket 5 was never seen");
    check_expect(standing_region(17)->legal == legal - 2 && standing_rep(faction_people_of(17)->id) == people - 1,
                 "theft: the crime's cost is not pickpocketing's 2 and half");
    check_expect(!court_pending(), "theft: a seen pickpocket opened the court");

    /* A class enemy's pocket: no crime on failure (region -1), and the level term moves the chance. */
    legal = standing_region(17)->legal;
    p.sheet.skill[SKILL_PICKPOCKET] = 5;
    theft_forget();
    caught = 0;
    for (i = 0; i < 200; i++) {
        caught += theft_pickpocket(&p, 0x80000000u + (unsigned)i, 0, -1, &gold) == THEFT_CAUGHT;
    }
    check_expect(caught > 150 && standing_region(17)->legal == legal, "theft: an enemy's failed pocket noted a crime, or was not mostly failed at 5");
    p.sheet.skill[SKILL_PICKPOCKET] = 50;
    theft_forget();
    caught = 0;
    for (i = 0; i < 400; i++) {
        caught += theft_pickpocket(&p, 0x80000000u + (unsigned)i, 9, -1, &gold) == THEFT_CAUGHT;
    }
    check_expect(caught < 40, "theft: nine levels above a foe at Pickpocket 50 (95%) was seen too often");

    /* Locks: a door inside is the skill and five a level the thief is above the lock, 5..95; 20 is held by magic. */
    check_expect(rules_inside_lock_chance(50, 5, 3) == 60 && rules_inside_lock_chance(50, 1, 8) == 15, "theft: the inside lock's sum");
    check_expect(rules_inside_lock_chance(10, 1, 19) == 5 && rules_inside_lock_chance(90, 10, 1) == 95, "theft: the inside lock's clamp");
    check_expect(rules_lock_alert_chance(0) == 33 && rules_lock_alert_chance(100) == 0 && rules_lock_alert_chance(50) == 16
                     && rules_lock_alert_chance(150) == 0,
                 "theft: a failed pick's alert is not a third less Stealth");
    p.sheet.level = 5;
    p.sheet.skill[SKILL_LOCKPICKING] = 50;
    uses = p.sheet.skill_uses[SKILL_LOCKPICKING];
    check_expect(theft_pick_inside(&p, RULES_LOCK_MAGICAL) == LOCK_MAGIC && p.sheet.skill_uses[SKILL_LOCKPICKING] == uses,
                 "theft: a magic lock was tried, or counted a use");
    caught = golds = 0;
    for (i = 0; i < 2000; i++) {
        golds += theft_pick_inside(&p, 3) == LOCK_PICKED;
        caught++;
    }
    check_expect(p.sheet.skill_uses[SKILL_LOCKPICKING] == uses + 2000, "theft: a pick, won or lost, did not count a use");
    check_expect(golds > 1100 && golds < 1300, "theft: lock 3 at level 5, Lockpicking 50 (60%) is not picked about 60 times in 100");
    p.sheet.skill[SKILL_STEALTH] = 0;
    caught = 0;
    for (i = 0; i < 2000; i++) {
        caught += theft_lock_heard(&p);
    }
    check_expect(caught > 560 && caught < 760, "theft: a failed pick at Stealth 0 is not heard a third of the time");
    p.sheet.skill[SKILL_STEALTH] = 100;
    caught = 0;
    for (i = 0; i < 500; i++) {
        caught += theft_lock_heard(&p);
    }
    check_expect(caught == 0, "theft: a failed pick at Stealth 100 was heard");

    theft_words(THEFT_GOLD, 1, line, (int)sizeof(line));
    check_expect(df_eq(line, "You take 1 gold piece."), "theft: one gold piece's words");
    theft_words(THEFT_GOLD, 4, line, (int)sizeof(line));
    check_expect(df_eq(line, "You take 4 gold pieces."), "theft: gold pieces' words");
    theft_words(THEFT_NOTHING, 0, line, (int)sizeof(line));
    check_expect(line[0] != 0, "theft: nothing found has no words");
}

/*
 * Crouching: held or toggled by the key, DFU's half height and half walking speed, no running and
 * no jump while down, a horse no place for it, and the OPTS row that keeps the choice. How the body
 * stays down under a low ceiling is the engine's test (check_body_crouch); here it is the player's
 * state and the speed it walks at.
 */
static void check_crouch(void)
{
    Player p;
    Character sheet;
    Input in;
    Input none;
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    mem_zero(&in, sizeof(in));
    mem_zero(&none, sizeof(none));
    check_expect(!p.crouched && p.body.height == WALK_HEIGHT, "crouch: a new player is not standing");

    /* Held: down while the key is, up when it is let go (no scene to ask: room enough). */
    player_crouch(&p, 0, 0, 1, 1);
    check_expect(p.crouched && p.body.height == PLAYER_CROUCH_HEIGHT && p.body.eye == PLAYER_CROUCH_EYE, "crouch: held did not crouch the body");
    player_crouch(&p, 0, 0, 1, 0);
    check_expect(p.crouched, "crouch: held let the body up while held");
    player_crouch(&p, 0, 0, 0, 0);
    check_expect(!p.crouched && p.body.height == WALK_HEIGHT && p.body.eye == WALK_EYE, "crouch: letting go did not stand the body");

    /* Toggled: a press flips it, its being held or let go changes nothing. */
    player_crouch(&p, 0, 1, 1, 1);
    player_crouch(&p, 0, 1, 1, 0);
    player_crouch(&p, 0, 1, 0, 0);
    check_expect(p.crouched, "crouch: a toggle was let go with the key");
    player_crouch(&p, 0, 1, 1, 1);
    player_crouch(&p, 0, 1, 0, 0);
    check_expect(!p.crouched, "crouch: a second press did not stand them");

    /* A horse is no place to crouch. */
    player_crouch(&p, 0, 0, 1, 1);
    p.riding = 1;
    player_crouch(&p, 0, 0, 1, 0);
    check_expect(!p.crouched, "crouch: crouched on a horse");
    p.riding = 0;
    player_crouch(&p, 0, 0, 0, 0);

    /* Half the walking speed, run held or not, no jump: on the hold's own floor, a quarter second. */
    {
        World *world;
        Level level;
        float x;
        float y;
        float z;
        float yaw;
        float pitch;
        float stood;
        float down;

        world = world_load();
        check_expect(world != 0, "crouch: the hold did not load");
        if (world) {
            world_as_level(world, &level);
            world_start(world, &x, &y, &z, &yaw, &pitch);
            player_place(&p, x, y, z, yaw, pitch);
            in.move_f = 1;
            in.held = INPUT_BIT(INPUT_RUN);
            walk(&p, &level.scene, &none, 60);
            /* Standing and walking, run held: the run. Crouched, the same keys: half the walk. */
            stood = walk(&p, &level.scene, &in, 15);
            check_expect(!player_sneaking(&p), "crouch: running is sneaking");
            in.move_f = -1;
            in.held = 0;
            walk(&p, &level.scene, &in, 15);
            player_crouch(&p, &level.scene, 0, 1, 1);
            walk(&p, &level.scene, &none, 30);
            in.move_f = 1;
            in.held = INPUT_BIT(INPUT_RUN);
            down = walk(&p, &level.scene, &in, 15);
            check_expect(player_sneaking(&p), "crouch: crouch-walking is not sneaking");
            walk(&p, &level.scene, &none, 5);
            check_expect(player_sneaking(&p), "crouch: standing still is not sneaking");
            printf("play: a quarter second goes %.1f running, %.1f crouched with run held\n", (double)stood, (double)down);
            check_expect(check_near(down, rules_walk_speed(p.sheet.attribute[ATTR_SPEED]) * RULES_UNITS_PER_M * 0.25f * PLAYER_CROUCH_SPEED,
                                    down * 0.1f),
                         "crouch: crouched with run held is not half the walking speed");
            check_expect(!p.running, "crouch: crouched and still running");
            in.move_f = 0;
            in.held = 0;
            in.pressed = INPUT_BIT(INPUT_JUMP);
            player_step_motion(&p, &in, &level.scene, 1.0f / 60.0f, 1, 0, 0);
            check_expect(p.body.vy <= 0.5f, "crouch: a crouched player jumped");
            in.pressed = 0;
            player_crouch(&p, &level.scene, 0, 0, 0);
            check_expect(!p.crouched, "crouch: in the open, letting go did not stand them");
        }
    }

    /* The option: off by default, and settable. */
    viewopt_defaults();
    check_expect(!viewopt_crouch_toggle(), "crouch: the toggle is not off by default");
    viewopt_set_crouch_toggle(1);
    check_expect(viewopt_crouch_toggle(), "crouch: the toggle did not set");
    viewopt_set_crouch_toggle(0);
}

/*
 * Afflictions: the diseases' records, the saving throw, a disease's days and its
 * drain given back, the werewolf's infection turned to its curse and its gifts,
 * changing shape and the moons, the urge to kill, a vampire's sun, hunger and
 * travel, a temple's price, a level-1 character kept from disease, and the
 * state through a save file.
 */
static void check_ills(void)
{
    Player p;
    Character sheet;
    const Disease *d;
    int before[ATTR_COUNT];
    int skill_before;
    int caught;
    int health;
    int said;
    int now;
    int i;

    combat_seed(7);
    ills_new_game();
    check_expect(disease_table()->count == DISEASE_COUNT, "ills: not classic's seventeen diseases");
    d = disease_kind(DISEASE_PLAGUE);
    check_expect(d && df_eq(d->name, "Plague") && d->attribute[ATTR_STRENGTH] == 1 && d->attribute[ATTR_INTELLIGENCE] == 0
                     && d->damage_min == 3 && d->damage_max == 30 && d->days_min == DISEASE_FOREVER,
                 "ills: plague's record");
    d = disease_kind(DISEASE_CALIRONS_CURSE);
    check_expect(d && d->days_min == 3 && d->days_max == 18, "ills: Caliron's Curse does not pass in 3..18 days");

    /* A Breton warrior has no tolerance either way: 50 and a tenth of the Willpower. */
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    sheet.level = 5;
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    check_expect(ills_disease_save(&p.sheet) == 50 + p.sheet.attribute[ATTR_WILLPOWER] / 10, "ills: the saving throw is not 50 + Willpower / 10");
    check_expect(ills_saved(60, 30) && !ills_saved(60, 45) && !ills_saved(60, 70) && ills_saved(100, 99),
                 "ills: a roll within 20 under the save still passes the disease on");

    /* Plague caught on day 100: nothing that day; the next, one roll off each attribute it weighs, and health. */
    for (i = 0; i < ATTR_COUNT; i++) {
        before[i] = p.sheet.attribute[i];
    }
    check_expect(ills_catch(&p.sheet, DISEASE_PLAGUE, 100) && !ills_catch(&p.sheet, DISEASE_PLAGUE, 100), "ills: plague caught twice");
    check_expect(ills_day(&p, 100) == 0 && p.sheet.attribute[ATTR_STRENGTH] == before[ATTR_STRENGTH], "ills: plague hurt the day it was caught");
    health = p.sheet.health;
    said = ills_day(&p, 101);
    {
        int lost;

        lost = before[ATTR_STRENGTH] - p.sheet.attribute[ATTR_STRENGTH];
        check_expect((said & ILLS_FELT_BAD) && lost >= 3 && lost <= 30, "ills: plague's day did not take 3..30 Strength");
        check_expect(before[ATTR_AGILITY] - p.sheet.attribute[ATTR_AGILITY] == lost
                         && p.sheet.attribute[ATTR_INTELLIGENCE] == before[ATTR_INTELLIGENCE],
                     "ills: plague's one roll is not taken from Agility alike, or touched Intelligence");
        check_expect(health - p.sheet.health == lost || p.sheet.health == 0, "ills: plague's health is not the same roll");
        check_expect(ills()->drained[DISEASE_PLAGUE * ATTR_COUNT + ATTR_STRENGTH] == -lost, "ills: the drain is not kept");
    }
    check_expect(ills_curable() == 1, "ills: plague is not one to cure");
    ills_cure(&p.sheet);
    for (i = 0; i < ATTR_COUNT; i++) {
        check_expect(p.sheet.attribute[i] == before[i], "ills: the cure did not give the attributes back");
    }
    check_expect(ills_curable() == 0, "ills: something left after the cure");

    /* Caliron's Curse passes by itself within 18 days, and gives back what it took. */
    character_rest_full(&p.sheet);
    check_expect(ills_catch(&p.sheet, DISEASE_CALIRONS_CURSE, 200), "ills: Caliron's Curse not caught");
    check_expect(ills()->days_left[DISEASE_CALIRONS_CURSE] >= 3 && ills()->days_left[DISEASE_CALIRONS_CURSE] <= 18, "ills: its days not 3..18");
    said = ills_day(&p, 220);
    check_expect((said & ILLS_PASSED) && ills()->days_left[DISEASE_CALIRONS_CURSE] == 0
                     && p.sheet.attribute[ATTR_SPEED] == before[ATTR_SPEED],
                 "ills: Caliron's Curse did not pass and give back Speed");

    /* A level-1 character catches nothing from a thousand rats; a level-5 one does, one of the rat's three. */
    {
        Character young;

        character_new(&young, RACE_BRETON, CLASS_WARRIOR, 0, "Kid");
        caught = 0;
        for (i = 0; i < 1000; i++) {
            caught += ills_struck(&young, ENEMY_RAT, 300, 17) != ILLS_STRUCK_NOTHING;
        }
        check_expect(caught == 0, "ills: a level-1 character caught a disease");
        caught = ILLS_STRUCK_NOTHING;
        for (i = 0; i < 5000 && caught == ILLS_STRUCK_NOTHING; i++) {
            caught = ills_struck(&p.sheet, ENEMY_RAT, 300, 17);
        }
        check_expect(caught == DISEASE_PLAGUE || caught == DISEASE_STOMACH_ROT || caught == DISEASE_BRAIN_FEVER,
                     "ills: five thousand rats gave no plague, stomach rot or brain fever");
        ills_cure(&p.sheet);
    }

    /* A werewolf's bite: the dream the next day, the curse past three, every disease gone with it. */
    character_rest_full(&p.sheet);
    skill_before = p.sheet.skill[SKILL_RUNNING];
    ills_catch(&p.sheet, DISEASE_DEMENTIA, 400);
    check_expect(ills_infect(ILL_WEREWOLF, 400, 17) && !ills_infect(ILL_VAMPIRE, 400, 17), "ills: two infections at once");
    check_expect(ills_day(&p, 400) == 0 || !(ills_day(&p, 400) & ILLS_DREAM), "ills: a dream the day of the bite");
    check_expect(ills_day(&p, 401) & ILLS_DREAM, "ills: no dream the next day");
    check_expect(!(ills_day(&p, 403) & ILLS_TURNED), "ills: the curse before three days");
    check_expect((ills_day(&p, 404) & ILLS_TURNED) && ills_curse() == ILL_WEREWOLF && ills()->infection == ILL_NONE,
                 "ills: no werewolf past three days");
    check_expect(ills()->days_left[DISEASE_DEMENTIA] == 0 && ills_disease_save(&p.sheet) == 100
                     && !ills_catch(&p.sheet, DISEASE_PLAGUE, 405),
                 "ills: a lycanthrope still diseased or catching");
    check_expect(p.sheet.attribute[ATTR_STRENGTH] == (before[ATTR_STRENGTH] + 40 > 100 ? 100 : before[ATTR_STRENGTH] + 40)
                     && p.sheet.attribute[ATTR_INTELLIGENCE] == before[ATTR_INTELLIGENCE]
                     && p.sheet.skill[SKILL_RUNNING] == (skill_before + 30 > 100 ? 100 : skill_before + 30),
                 "ills: a werewolf's +40 Strength and +30 Running");

    /* Into the beast at will, the hands emptied and healed whole; not again within a day; out any time. */
    {
        Item sword;
        int at;

        check_expect(item_make(&sword, ITEM_LONGSWORD, 0) && (at = inventory_add(&p.pack, &sword)) >= 0 && inventory_wear(&p.pack, at),
                     "ills: no sword in hand");
        now = 404 * 1440 + 600;
        p.sheet.health = 1;
        check_expect(ills_morph(&p, now, 0) && ills_beast() && !inventory_weapon(&p.pack) && p.sheet.health == p.sheet.health_max,
                     "ills: the change kept a sword in hand, or did not heal");
        check_expect(df_eq(ills_race_name(&p.sheet), "Werewolf"), "ills: the sheet does not say Werewolf");
        check_expect(ills_morph(&p, now + 10, 0) && !ills_beast(), "ills: could not change back");
        check_expect(!ills_morph(&p, now + 20, 0), "ills: into the beast twice in a day");
        check_expect(ills_minute(&p, now + 30, 0, 0, 1) & ILLS_FORCED, "ills: the full moon did not force the change");
        check_expect(ills_morph(&p, now + 40, 0) && !ills_beast(), "ills: could not change back after the moon");
    }
    /* The moons: Masser full on (day + 405 x 360 + 3) mod 32 == 0, day 21 of 405; Secunda on day 25. */
    check_expect(ills_full_moon(405, 21) && ills_full_moon(405, 25) && !ills_full_moon(405, 22), "ills: the moons' full days");
    /* A month without killing an innocent: health held a point an hour under its most. */
    now = ills()->last_kill + 30 * 1440 + 600;
    p.sheet.health = p.sheet.health_max;
    check_expect((ills_minute(&p, now, 0, 0, 0) & ILLS_HUNGER) && p.sheet.health == p.sheet.health_max - 10,
                 "ills: ten hours of the urge did not hold health 10 under");
    ills_killed_innocent(now);
    check_expect(!(ills_minute(&p, now + 1, 0, 0, 0) & ILLS_HUNGER), "ills: a kill did not sate the urge");

    /* Lifted, every gift taken back. */
    ills_lift_curse(&p);
    check_expect(ills_curse() == ILL_NONE && p.sheet.attribute[ATTR_STRENGTH] == before[ATTR_STRENGTH]
                     && p.sheet.skill[SKILL_RUNNING] == skill_before,
                 "ills: the lifted curse left its gifts");

    /* A vampire: +20 but Intelligence, burnt 12 each fourth minute in the sun, unfed a day no rest, no travel by day. */
    ills_infect(ILL_VAMPIRE, 500, 17);
    ills_day(&p, 501);
    check_expect((ills_day(&p, 504) & ILLS_TURNED) && ills_curse() == ILL_VAMPIRE, "ills: no vampire past three days");
    check_expect(ills()->clan > 0 && p.sheet.attribute[ATTR_LUCK] == (before[ATTR_LUCK] + 20 > 100 ? 100 : before[ATTR_LUCK] + 20)
                     && p.sheet.attribute[ATTR_INTELLIGENCE] == before[ATTR_INTELLIGENCE],
                 "ills: a vampire's clan, +20 Luck, Intelligence untouched");
    check_expect(df_eq(ills_race_name(&p.sheet), "Vampire"), "ills: the sheet does not say Vampire");
    character_rest_full(&p.sheet);
    health = p.sheet.health;
    now = 504 * 1440 + 720;
    check_expect((ills_minute(&p, now, 1, 0, 0) & ILLS_BURNT) && p.sheet.health == health - 12, "ills: the sun did not burn 12");
    check_expect(ills_minute(&p, now + 1, 1, 0, 0) == 0 && ills_minute(&p, now + 4, 0, 0, 0) == 0, "ills: burnt off the fourth minute, or in the shade");
    check_expect(!ills_rest_refused(504 * 1440 + 100) && ills_rest_refused(506 * 1440), "ills: an unfed vampire's rest");
    ills_fed(506 * 1440);
    check_expect(!ills_rest_refused(506 * 1440 + 10), "ills: fed, still no rest");
    check_expect(ills_travel_refused(1) && !ills_travel_refused(0), "ills: a vampire travels by day");

    /* The temple's price: 250 a disease by quality and prices; nothing on a cure day, half on a half day. */
    check_expect(ills_cure_cost(2, 10, 1000, 0) == rules_cost(500, 10, 1000) && ills_cure_cost(2, 10, 1000, HOLIDAY_TEMPLE_CURE) == 0
                     && ills_cure_cost(2, 10, 1000, HOLIDAY_TEMPLE_HALF) == rules_cost(500, 10, 1000) / 2
                     && ills_cure_cost(0, 10, 1000, 0) == 0,
                 "ills: the temple's price");

    /* The state goes round a save file. */
    {
        SaveState out;
        SaveState in;
        char path[260];

        ills_lift_curse(&p);
        ills_catch(&p.sheet, DISEASE_LEPROSY, 600);
        ills_infect(ILL_WEREBOAR, 600, 17);
        save_use_file("DATA\\TEST4.SAV");
        keep_clear();
        pile_new_game();
        mem_zero(&out, sizeof(out));
        out.sheet = p.sheet;
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "ills: a save did not write");
        ills_new_game();
        check_expect(save_read(&in) && ills()->days_left[DISEASE_LEPROSY] == ILLS_FOREVER && ills()->infection == ILL_WEREBOAR
                         && ills()->infected_day == 600,
                     "ills: the state did not come back from the save");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "ills: the test's file did not go");
    }
    ills_new_game();
}

/* Ships: bought from the gold carried and then the account, one at a time, sold at 85%, no fare at sea, through a save. */
static void check_ships(void)
{
    Player p;
    Character sheet;
    RegionStanding *r;
    Journey j;

    standing_new_game(1);
    bank_set_ship(BANK_SHIP_NONE);
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    r = standing_region(17);
    p.pack.gold = 50000;
    r->gold = 40000;
    check_expect(bank_buy_ship(&p, 17, BANK_SHIP_SMALL) == BANK_NOT_ENOUGH_GOLD && bank_ship() == BANK_SHIP_NONE,
                 "ships: bought with 90000 for 100000");
    r->gold = 60000;
    check_expect(bank_buy_ship(&p, 17, BANK_SHIP_SMALL) == BANK_DONE && bank_ship() == BANK_SHIP_SMALL
                     && p.pack.gold == 0 && r->gold == 10000,
                 "ships: the small ship not paid from the purse first, then the account");
    check_expect(bank_buy_ship(&p, 17, BANK_SHIP_LARGE) == BANK_ALREADY_SHIP, "ships: a second ship bought");
    /* A voyage of 30 sea pixels: 25 x (30 / 24 + 1) = 50 fare, gone on one's own ship. */
    mem_zero(&j, sizeof(j));
    j.sea_pixels = 30;
    j.fare = 50;
    j.inn_gold = 10;
    j.cost = 60;
    journey_own_ship(&j);
    check_expect(j.fare == 0 && j.cost == 10, "ships: an owned ship still pays a fare");
    {
        SaveState out;
        SaveState in;
        char path[260];

        save_use_file("DATA\\TEST4.SAV");
        keep_clear();
        pile_new_game();
        mem_zero(&out, sizeof(out));
        out.sheet = sheet;
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "ships: a save did not write");
        bank_set_ship(BANK_SHIP_NONE);
        check_expect(save_read(&in) && bank_ship() == BANK_SHIP_SMALL, "ships: the ship did not come back from the save");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "ships: the test's file did not go");
    }
    r = standing_region(17);
    check_expect(bank_sell_ship(17) == BANK_DONE && bank_ship() == BANK_SHIP_NONE && r->gold == 10000 + 85000,
                 "ships: not sold back for 85% into the account");
    check_expect(bank_sell_ship(17) == BANK_NO_SHIP, "ships: sold a ship not owned");
    standing_new_game(1);
}

/* Taverns: a room's price by the day and Heart's Day, renting and adding days, the hours left and their end; the menu's prices, hunger, holidays; through a save. */
static void check_lodging(void)
{
    Player p;
    Character sheet;
    int now;

    lodging_new_game();
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    check_expect(lodging_room_cost(3, 100) == 21 && lodging_room_cost(3, 45) == 14 && lodging_room_cost(1, 46) == 0
                     && lodging_room_cost(2, 47) == 14,
                 "lodging: 7 a day, Heart's Day free");
    now = 500 * 1440 + 600;
    p.pack.gold = 10;
    check_expect(lodging_rent(&p, 17, 5, 2, 7, 2, 30, now) == LODGING_NOT_ENOUGH_GOLD, "lodging: rented without the gold");
    p.pack.gold = 100;
    check_expect(lodging_rent(&p, 17, 5, 2, 7, 2, 14, now) == LODGING_DONE && p.pack.gold == 86
                     && lodging_hours_left(17, 5, 2, 7, now) == 48 && lodging_hours_left(17, 5, 2, 8, now) == 0,
                 "lodging: two days' room not 48 hours, or held in the wrong building");
    check_expect(lodging_rent(&p, 17, 5, 2, 7, 1, 7, now + 60) == LODGING_DONE && lodging_hours_left(17, 5, 2, 7, now + 60) == 71,
                 "lodging: a day more did not add to the room");
    check_expect(lodging_hours_left(17, 5, 2, 7, now + 72 * 60) == 0, "lodging: the room did not run out");
    check_expect(lodging_rent(&p, 17, 5, 2, 7, 351, 0, now) == LODGING_TOO_MANY_DAYS, "lodging: 351 days ahead rented");
    /* The menu: wine 3 gold and 6 health; not hungry again for four hours; free on New Life, half on Harvest's End. */
    p.sheet.health = 1;
    check_expect(lodging_eat(&p, 3, now, 0) == LODGING_DONE && p.pack.gold == 76 && p.sheet.health == 7, "lodging: wine not 3 gold and 6 health");
    check_expect(lodging_eat(&p, 0, now + 100, 0) == LODGING_NOT_HUNGRY, "lodging: hungry again within four hours");
    check_expect(lodging_food_cost(3, HOLIDAY_FREE_DRINK) == 0 && lodging_food_cost(3, HOLIDAY_FOOD_HALF) == 1
                     && lodging_food_cost(0, HOLIDAY_FOOD_HALF) == 1,
                 "lodging: the holidays' prices");
    check_expect(holiday_on(7, 26) && (holiday_on(7, 26)->flags & HOLIDAY_FOOD_HALF), "lodging: Harvest's End does not halve the food");
    {
        SaveState out;
        SaveState in;
        char path[260];

        save_use_file("DATA\\TEST4.SAV");
        keep_clear();
        pile_new_game();
        mem_zero(&out, sizeof(out));
        out.sheet = sheet;
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "lodging: a save did not write");
        lodging_new_game();
        check_expect(save_read(&in) && lodging_hours_left(17, 5, 2, 7, now + 60) == 71 && lodging_last_meal() == now,
                     "lodging: the room or the meal did not come back from the save");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "lodging: the test's file did not go");
    }
    lodging_new_game();
}

/* DFU's skill numbers the guild test names. */
#define SKILL_LONG_BLADE_ID 29
#define SKILL_AXE_ID 31

/*
 * Guilds: MAIN.DAT's twenty, a temple's order under its god, ranks earned by
 * reputation and skills worked by hand, joining and its refusals, a rank
 * looked at after 28 days, expulsion, training and its refusals, what members
 * are given, the guild's panel by clicks, and memberships through a save.
 */
static void check_guilds(void)
{
    static const char *const lines[] = { "Long Blade 31", "Axe 9" };
    const GuildKind *fighters;
    const GuildKind *akatosh;
    const GuildKind *arkay;
    const GuildKind *order;
    Player p;
    Character sheet;
    int i;
    int uses;

    guild_new_game();
    check_expect(guild_table()->count == 20, "guilds: not the Fighters, the Mages, eight temples and ten orders");
    fighters = guild_kind(41);
    check_expect(fighters && fighters->group == GUILD_GROUP_FIGHTERS && df_eq(fighters->rank[0], "Apprentice")
                     && df_eq(fighters->rank[9], "Master") && fighters->skill[4] == SKILL_LONG_BLADE_ID,
                 "guilds: the Fighters' titles or skills");
    akatosh = guild_of_faction(92);
    check_expect(akatosh && akatosh->id == 26 && akatosh->healing_rank == 1, "guilds: the Akatosh Chantry is not Akatosh's temple");
    arkay = guild_kind(21);
    order = guild_kind(368);
    check_expect(arkay && order && order->group == GUILD_GROUP_KNIGHTLY && df_eq(order->rank[5], "Knight Brother"),
                 "guilds: Arkay's temple or the Knights of the Dragon");
    /* By hand: rank 0 wants one guild skill at 22 and another at 4; rank 2, 31 and 9 and a name of 20. */
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    for (i = 0; i < SKILL_COUNT; i++) {
        sheet.skill[i] = 0;
    }
    sheet.skill[SKILL_LONG_BLADE_ID] = 22;
    check_expect(guild_rank_earned(fighters, &sheet, 0) == -1, "guilds: one skill alone earned a rank");
    sheet.skill[SKILL_AXE_ID] = 4;
    check_expect(guild_rank_earned(fighters, &sheet, 0) == 0, "guilds: 22 and 4 did not earn rank 0");
    sheet.skill[SKILL_LONG_BLADE_ID] = 31;
    sheet.skill[SKILL_AXE_ID] = 9;
    check_expect(guild_rank_earned(fighters, &sheet, 20) == 2 && guild_rank_earned(fighters, &sheet, 19) == 1
                     && guild_rank_earned(fighters, &sheet, -1) == -1,
                 "guilds: 31 and 9 with a name of 20 is not rank 2");
    /* Joining: a bad name, too little skill, in, and not twice; one temple at a time. */
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    check_expect(guild_join(fighters, &p.sheet, -5, 100) == GUILD_BAD_NAME, "guilds: joined with a bad name");
    p.sheet.skill[SKILL_AXE_ID] = 0;
    check_expect(guild_join(fighters, &p.sheet, 0, 100) == GUILD_LOW_SKILL, "guilds: joined without the skills");
    p.sheet.skill[SKILL_AXE_ID] = 9;
    check_expect(guild_join(fighters, &p.sheet, 0, 100) == GUILD_JOINED && guild_rank(fighters) == 0
                     && guild_join(fighters, &p.sheet, 0, 100) == GUILD_ALREADY,
                 "guilds: not joined at rank 0, or joined twice");
    p.sheet.skill[SKILL_MEDICAL] = 30;
    p.sheet.skill[SKILL_AXE_ID] = 9;
    check_expect(guild_join(arkay, &p.sheet, 0, 100) == GUILD_JOINED && guild_join(akatosh, &p.sheet, 0, 100) == GUILD_OTHER_OF_GROUP,
                 "guilds: two temples at once");
    /* The rank: the same before 28 days, up after, out with a bad name. */
    check_expect(guild_update(fighters, &p.sheet, 20, 120) == GUILD_SAME, "guilds: a rank changed within 28 days");
    check_expect(guild_update(fighters, &p.sheet, 20, 128) == 2 && guild_rank(fighters) == 2 && df_eq(guild_title(fighters, 2), "Swordsman"),
                 "guilds: not Swordsman after 28 days");
    /* What members get: the Fighters' hall to rest in; repairs and Arkay's cure at (10 - rank) tenths. */
    check_expect(guild_can_rest_in(41) && !guild_can_rest_in(40), "guilds: the Fighters' rest, or the Mages'");
    check_expect(guild_cure_price(21, 1000) == ((10 << 8) / 10 * 1000 >> 8) && guild_cure_price(26, 1000) == 1000,
                 "guilds: Arkay's rank 0 cure, or a temple not joined");
    check_expect(guild_heals(21) && !guild_heals(26), "guilds: Arkay heals from rank 0");
    check_expect(!guild_free_rooms(5), "guilds: free rooms without an order");
    /* Training: a Fighter's 100 a level, once in 12 hours, a skill to 50, the guild's own skills only. */
    check_expect(guild_trains(fighters) && guild_trains(akatosh) && !guild_trains(guild_kind(40)), "guilds: who trains whom");
    check_expect(guild_training_price(1, 3) == 300 && guild_training_price(0, 3) == 1200, "guilds: training's price");
    p.pack.gold = 1000;
    uses = p.sheet.skill_uses[SKILL_LONG_BLADE_ID];
    check_expect(guild_train(&p, fighters, SKILL_MEDICAL, 100, 5000) == GUILD_NOT_TAUGHT, "guilds: Medical taught by the Fighters");
    check_expect(guild_train(&p, fighters, SKILL_LONG_BLADE_ID, 100, 5000) == GUILD_TRAINED && p.pack.gold == 900
                     && p.sheet.skill_uses[SKILL_LONG_BLADE_ID] >= uses + 10,
                 "guilds: training not paid or no uses gained");
    check_expect(guild_train(&p, fighters, SKILL_AXE_ID, 100, 5100) == GUILD_TOO_SOON, "guilds: trained twice in 12 hours");
    p.sheet.skill[SKILL_AXE_ID] = 51;
    check_expect(guild_train(&p, fighters, SKILL_AXE_ID, 100, 6000) == GUILD_TOO_SKILLED, "guilds: trained past 50");
    p.sheet.skill[SKILL_AXE_ID] = 9;
    /* The panel: Join Guild, Talk, the service written in, Exit; a member's has no Join; a list's line picked. */
    check_expect(service_open(SERVICE_PANEL_GUILD), "guilds: the panel did not open");
    service_set_label("Training");
    check_expect(service_click(95 + 6, 50 + 5 + 2) == SERVICE_JOIN && service_click(95 + 6, 50 + 14 + 2) == SERVICE_TALK
                     && service_click(95 + 6, 50 + 23 + 2) == SERVICE_GUILD && service_click(95 + 46, 50 + 33 + 4) == SERVICE_LEAVE,
                 "guilds: the panel's rows");
    service_list(lines, 2);
    service_compose();
    write_screen("build/guild0.ppm", service_pixels());
    check_expect(service_click(95 + 20, 98 + 6 + 8 + 2) == SERVICE_PICKED && service_picked() == 1, "guilds: the list's second line");
    service_close();
    check_expect(service_open(SERVICE_PANEL_GUILD_MEMBER) && service_click(95 + 6, 50 + 5 + 2) == SERVICE_STAY,
                 "guilds: a member was offered Join Guild");
    service_close();
    /* Through a save. */
    {
        SaveState out;
        SaveState in;
        char path[260];

        save_use_file("DATA\\TEST4.SAV");
        keep_clear();
        pile_new_game();
        mem_zero(&out, sizeof(out));
        out.sheet = p.sheet;
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "guilds: a save did not write");
        guild_new_game();
        check_expect(save_read(&in) && guild_rank(fighters) == 2 && guild_rank(arkay) == 0 && guild_last_training() == 5000,
                     "guilds: the memberships did not come back from the save");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "guilds: the test's file did not go");
    }
    /* Out with a bad name, 28 days after the last change. */
    check_expect(guild_update(fighters, &p.sheet, -1, 200) == GUILD_EXPELLED && guild_rank(fighters) == -1,
                 "guilds: a bad name did not put a member out");
    guild_new_game();
}

/* Names: NAMEGEN.DAT's pieces cooked into MAIN.DAT, a name the same for a seed, each race's pattern, the region's bank, a flat's sex. */
static void check_names(void)
{
    char a[48];
    char b[48];
    int differ;
    unsigned seed;

    /* Breton 6 x 13, Redguard 51 + 5 + 26 + 22 + 15 and so on: hundreds of pieces. */
    check_expect(name_table()->count > 500, "names: NAMEGEN.DAT's pieces are not in MAIN.DAT");
    name_make(NAME_BANK_BRETON, 0, 1234u, a, (int)sizeof(a));
    name_make(NAME_BANK_BRETON, 0, 1234u, b, (int)sizeof(b));
    check_expect(a[0] && df_eq(a, b) && strchr(a, ' '), "names: a Breton's name is not the same twice, or has no surname");
    differ = 0;
    for (seed = 1; seed < 20; seed++) {
        name_make(NAME_BANK_BRETON, 1, seed, b, (int)sizeof(b));
        differ += !df_eq(a, b);
    }
    check_expect(differ >= 18, "names: different people share names too often");
    name_make(NAME_BANK_NORD, 0, 77u, b, (int)sizeof(b));
    check_expect(strlen(b) > 4 && df_eq(b + strlen(b) - 3, "sen"), "names: a Nord's surname does not end in sen");
    name_make(NAME_BANK_REDGUARD, 1, 77u, b, (int)sizeof(b));
    check_expect(b[0] && !strchr(b, ' '), "names: a Redguard has two names");
    check_expect(name_bank_of_race(RACE_ARGONIAN) == NAME_BANK_IMPERIAL && name_bank_of_region(17) == NAME_BANK_BRETON
                     && name_bank_of_region(0) == NAME_BANK_REDGUARD,
                 "names: the Argonians' or the regions' banks");
    /* FLATS.CFG: 175 0 a beautiful maiden (2), 175 2 a terrible child (1). */
    check_expect(faces_female(175, 0) == 1 && faces_female(175, 2) == 0 && faces_female(1, 127) == -1,
                 "names: a flat's sex from FLATS.CFG");
    name_make(NAME_BANK_NORD, 0, 77u, b, (int)sizeof(b));
    printf("names: %d pieces; a Breton man %s, a Nord %s\n", name_table()->count, a, b);
}

/* Quests' files: every QBN in ARENA2 parsed, a guild quest's records and logic, its QRC's words, MAIN.DAT's list of what is offered. */
static void check_quest_files(void)
{
    static const char *const sample = "M0B00Y00";
    MemHeap *scratch;
    MemHeap *was;
    Qbn q;
    char name[16];
    char text[512];
    const QuestKind *k;
    int files;
    int read;
    int i;
    df_u32 size;

    check_expect(quest_table()->count == 222, "quests: MAIN.DAT does not list classic's 187 offered quests and 35 of the main quest's, the tutorial's and the cures'");
    k = quest_kind(0);
    check_expect(k && df_eq(k->name, "M0C00Y11") && k->group == GUILD_GROUP_FIGHTERS && k->to == QUEST_TO_OTHERS && k->least == 0,
                 "quests: the first is not the Fighters' M0C00Y11 for others");
    /* Every quest MAIN.DAT lists has its QBN in ARENA2, and it parses. */
    scratch = mem_heap_new();
    was = mem_heap_use(scratch);
    files = 0;
    read = 0;
    for (i = 0; i < quest_table()->count; i++) {
        k = quest_kind(i);
        if (!k) {
            continue;
        }
        log_format(name, (int)sizeof(name), "%s.QBN", k->name);
        /* The Special Edition's 13 are not in every ARENA2: those there must parse. */
        if (vfs_read(name, &size)) {
            files++;
            read += qbn_load(name, &q);
        }
    }
    check_expect(files >= 209 && read == files, "quests: a listed quest's QBN that is there did not read");
    /* The Fighters' M0B00Y00: an item, a person, a place, a timer, a foe, logic and states; its first message read. */
    check_expect(qbn_load("M0B00Y00.QBN", &q), "quests: M0B00Y00.QBN did not read");
    check_expect(q.count[QBN_LOGIC] > 5 && q.count[QBN_STATES] > 0 && q.logic[0].count >= 1 && q.logic[0].count <= QBN_OPERANDS,
                 "quests: M0B00Y00's logic or states");
    /* The symbols' hash, UESP's own pairs: reward, oneday, traveltime. And one of M0B00Y00's resources is its reward. */
    check_expect(qbn_hash("reward") == 0x1B14u && qbn_hash("oneday") == 0x1AB3u && qbn_hash("traveltime") == 0x1C1E3u,
                 "quests: the symbols' hash");
    log_format(name, (int)sizeof(name), "%s.QRC", sample);
    check_expect(textrsc_read_in(name, 1000, text, (int)sizeof(text)) > 0, "quests: M0B00Y00.QRC's message 1000 did not read");
    mem_heap_use(was);
    mem_heap_delete(scratch);
    printf("quests: %d offered here, every QBN read; M0B00Y00 has %d logic records; 1000 says \"%.40s...\"\n", files, q.count[QBN_LOGIC], text);
}

/*
 * The quest machine: the Fighters' M0B00Y00 started in Daggerfall, its loup
 * chosen and put in a dungeon of the region onto the map, its timer and log;
 * the beast killed, the giver met, the gold paid and the quest over; again,
 * and left to its timer, failed.
 */
static void check_quest_machine(void)
{
    static DunRef rows[4096];
    QuestStart how;
    Player p;
    Character sheet;
    char line[512];
    int quest;
    int dagger;
    int slot;
    int kinds[8];
    int nfoes;
    int now;
    int n;
    int i;
    df_u32 gold;
    const QuestRun *run;

    quest_new_game();
    quest = -1;
    for (i = 0; i < quest_table()->count; i++) {
        if (quest_kind(i) && df_eq(quest_kind(i)->name, "M0B00Y00")) {
            quest = i;
        }
    }
    n = exterior_list_region(17, rows, 4096);
    dagger = -1;
    for (i = 0; i < n && i < 4096; i++) {
        if (df_eq(rows[i].name, "Daggerfall")) {
            dagger = rows[i].index;
        }
    }
    check_expect(quest >= 0 && dagger >= 0, "quest machine: no M0B00Y00, or no Daggerfall");
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
    player_init(&p, &sheet);
    inventory_clear(&p.pack);
    mem_zero(&how, sizeof(how));
    how.region = 17;
    how.index = dagger;
    how.giver_block = 5;
    how.giver_record = 7;
    how.giver_faction = 41;
    how.guild = 41;
    how.power = 50;
    how.prices = 1000;
    how.seed = 99;
    combat_seed(3);
    now = 400 * 1440;
    slot = quest_start(&p, quest, &how, now);
    run = quest_run(slot);
    check_expect(slot >= 0 && run && quest_running(quest) == slot && df_eq(quest_file(slot), "M0B00Y00"), "quest machine: M0B00Y00 did not start");
    /* At the start: one of the two tellings, one of the three beasts, its dungeon, the timer, the log. */
    check_expect(run && (quest_state(slot, 9) || quest_state(slot, 10)) && quest_state(slot, 6) + quest_state(slot, 7) + quest_state(slot, 8) == 1,
                 "quest machine: the start's random choices");
    check_expect(run && run->place_dungeon[0] && run->place_region[0] == 17 && run->timer_end[0] > now && run->log[0] == 1030,
                 "quest machine: its dungeon, timer or log entry");
    nfoes = quest_foes_at(17, run ? run->place_block[0] : -1, kinds, 8);
    check_expect(nfoes == 1, "quest machine: the loup is not in its dungeon");
    check_expect(quest_log_line(slot, 0, line, (int)sizeof(line)) && !strchr(line, '_') && !strchr(line, '%') && strstr(line, "Day "),
                 "quest machine: the log line has symbols left, or no date");
    printf("quest machine: log \"%.60s...\"\n", line[0] == '\n' ? line + 1 : line);
    /* The beast killed: state 1 and its message. */
    quest_killed(&p, kinds[0], 17, run->place_block[0], now + 600);
    check_expect(quest_state(slot, 1) && quest_take_message(line, (int)sizeof(line)) && !strchr(line, '_'),
                 "quest machine: the kill did not count, or said nothing");
    printf("quest machine: killed, \"%.50s...\"\n", line);
    /* Someone else met: nothing. The giver met: paid, and it is over. */
    gold = p.pack.gold;
    quest_met(&p, 17, dagger, 5, 8, 41, now + 900);
    check_expect(quest_running(quest) == slot, "quest machine: someone else ended it");
    quest_met(&p, 17, dagger, 5, 7, 41, now + 900);
    check_expect(quest_running(quest) < 0 && p.pack.gold > gold && p.pack.gold - gold >= 150 * 1 * 1000 / 1000 * 100 / 100 / 2,
                 "quest machine: the giver did not pay, or the quest went on");
    printf("quest machine: paid %u gold\n", (unsigned)(p.pack.gold - gold));
    while (quest_take_message(line, (int)sizeof(line))) {
    }
    /* Again, through a save: the same states, timer and dungeon, its logic read again. */
    slot = quest_start(&p, quest, &how, now);
    check_expect(slot >= 0, "quest machine: it did not start again");
    {
        SaveState out;
        SaveState in;
        QuestRun before;
        char path[260];

        before = *quest_run(slot);
        save_use_file("DATA\\TEST4.SAV");
        keep_clear();
        pile_new_game();
        mem_zero(&out, sizeof(out));
        out.sheet = sheet;
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "quest machine: a save did not write");
        quest_new_game();
        check_expect(save_read(&in) && quest_running(quest) == slot && quest_run(slot)->state[0] == before.state[0]
                         && quest_run(slot)->timer_end[0] == before.timer_end[0] && quest_run(slot)->place_block[0] == before.place_block[0],
                     "quest machine: the quest did not come back from the save");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "quest machine: the test's file did not go");
    }
    quest_drop(slot);
    /* Offered: bound, not running, its offer's words filled; No drops it; offered again and Yes runs it. */
    slot = quest_offer(&p, quest, &how, now);
    check_expect(slot >= 0 && !quest_run(slot) && quest_running(quest) == slot && quest_text(slot, QUEST_SAY_OFFER, line, (int)sizeof(line))
                     && strstr(line, "job"),
                 "quest machine: an offer runs, or says nothing");
    {
        int yes_x;
        int no_x;
        int by;

        check_expect(msgbox_open(line, 1), "quest machine: the message box did not open");
        msgbox_compose();
        write_screen("build/quest0.ppm", msgbox_pixels());
        msgbox_buttons(&yes_x, &no_x, &by);
        check_expect(msgbox_click(yes_x + 4, by + 4) == MSGBOX_YES && msgbox_click(no_x + 4, by + 4) == MSGBOX_NO
                         && msgbox_click(2, 2) == MSGBOX_STAY && msgbox_key('Y') == MSGBOX_YES && msgbox_key(0x1B) == MSGBOX_NO,
                     "quest machine: the box's Yes and No");
        msgbox_close();
        check_expect(msgbox_open("A word.", 0) && msgbox_click(2, 2) == MSGBOX_OK, "quest machine: a box without Yes and No did not close on a click");
        msgbox_close();
    }
    quest_drop(slot);
    check_expect(quest_running(quest) < 0, "quest machine: No did not drop the offer");
    slot = quest_offer(&p, quest, &how, now);
    check_expect(quest_accept(&p, slot, now) == slot && quest_run(slot) && quest_run(slot)->timer_end[0] > now, "quest machine: Yes did not run it");
    /* And the time runs out: over, unpaid. */
    gold = p.pack.gold;
    run = quest_run(slot);
    quest_tick(&p, run ? run->timer_end[0] + 1 : now);
    check_expect(quest_running(quest) < 0 && p.pack.gold == gold, "quest machine: the timer did not end it unpaid");
    /*
     * Many at once and the main quest: the new game's _BRISIEN starts the master S0000999 (and S0000977); a guild's
     * quests run beside them, past six; a once-only quest accepted is not offered again, through a save.
     */
    quest_new_game();
    {
        QuestStart start;
        int main_quest;
        int master;
        int count;
        int k;

        mem_zero(&start, sizeof(start));
        start.region = 17;
        start.index = -1;
        start.giver_block = -1;
        start.giver_record = -1;
        start.prices = 1000;
        start.power = 50;
        main_quest = quest_find("_BRISIEN");
        master = quest_find("S0000999");
        check_expect(main_quest >= 0 && master >= 0 && quest_kind(master)->group == QUEST_GROUP_MAIN, "quest machine: the main quest is not in MAIN.DAT");
        check_expect(quest_start(&p, main_quest, &start, now) >= 0 && quest_running(master) >= 0 && quest_running(quest_find("S0000977")) >= 0,
                     "quest machine: _BRISIEN did not start the main quest's master");
        check_expect(quest_is_done(master), "quest machine: the master is not marked started");
        /* A dozen more guild quests beside it. */
        count = 0;
        for (k = 0; k < quest_table()->count && count < 12; k++) {
            if (quest_kind(k) && quest_kind(k)->group != QUEST_GROUP_MAIN && quest_start(&p, k, &how, now) >= 0) {
                count++;
            }
        }
        check_expect(count == 12, "quest machine: twelve guild quests did not run at once");
        n = 0;
        for (k = 0; k < QUEST_SLOTS; k++) {
            n += quest_run(k) != 0;
        }
        check_expect(n >= 14, "quest machine: the quests running at once are too few");
    }
    quest_new_game();
    /* Once-only: M0B11Y18 (flag 1) accepted, finished or not, is not started again, and the done mark goes through a save. */
    {
        int once;
        int slot2;

        once = quest_find("M0B11Y18");
        if (once < 0) {
            once = quest_find("M0B21Y19");
        }
        n = -1;
        for (i = 0; i < quest_table()->count && n < 0; i++) {
            if (quest_kind(i) && (quest_kind(i)->flags & QUEST_ONCE) && quest_kind(i)->group != QUEST_GROUP_MAIN) {
                char file[16];
                df_u32 sz;

                log_format(file, (int)sizeof(file), "%s.QBN", quest_kind(i)->name);
                if (vfs_read(file, &sz)) {
                    n = i;
                }
            }
        }
        once = n;
        slot2 = quest_start(&p, once, &how, now);
        check_expect(once >= 0 && slot2 >= 0 && quest_is_done(once), "quest machine: a once-only quest, accepted, is not marked");
        quest_drop(slot2);
        check_expect(quest_offer(&p, once, &how, now) < 0, "quest machine: a once-only quest was offered again");
        {
            SaveState out;
            SaveState in;
            char path[260];

            save_use_file("DATA\\TEST4.SAV");
            keep_clear();
            pile_new_game();
            mem_zero(&out, sizeof(out));
            out.sheet = sheet;
            inventory_clear(&out.pack);
            check_expect(save_write(&out), "quest machine: a save with a done quest did not write");
            quest_new_game();
            check_expect(!quest_is_done(once), "quest machine: a new game remembers a done quest");
            check_expect(save_read(&in) && quest_is_done(once), "quest machine: the done mark did not come back from the save");
            check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "quest machine: the test's file did not go");
        }
    }
    quest_new_game();
    /* A delivery: C0C00Y13 hands the player a parcel at once and has a person stand in a building of another town, who takes it. */
    {
        int deliver;
        const QuestRun *dr;
        QuestPersonHere here[4];
        int has;
        int k;

        deliver = quest_find("C0C00Y13");
        slot = quest_start(&p, deliver, &how, now);
        dr = quest_run(slot);
        has = 0;
        for (k = 0; k < p.pack.count; k++) {
            has += p.pack.item[k].template_id == ITEM_QUEST;
        }
        check_expect(slot >= 0 && dr && has == 1, "quest machine: the delivery's parcel is not in the pack");
        check_expect(dr && dr->person_at[1] >= 0 && !dr->place_dungeon[dr->person_at[1]], "quest machine: the delivery's person is not put in a building");
        if (dr && dr->person_at[1] >= 0) {
            int at;

            at = dr->person_at[1];
            check_expect(quest_people_in(17, dr->place_index[at], dr->place_block[at], dr->place_record[at], here, 4) >= 1,
                         "quest machine: the delivery's person does not stand in their building");
            quest_met_person(&p, slot, 1, now + 120);
            has = 0;
            for (k = 0; k < p.pack.count; k++) {
                has += p.pack.item[k].template_id == ITEM_QUEST;
            }
            check_expect(has == 0 && quest_state(slot, 1), "quest machine: the parcel was not taken, or the delivery not done");
        }
    }
    quest_new_game();
    /* A dungeon of the kind the quest asks for: the first offered quest wanting a crypt (type 0) gets one, and its person's name is the same each time. */
    quest_new_game();
    {
        MemHeap *scratch;
        MemHeap *was;
        Qbn qq;
        int found;
        int k;
        int pl;
        char file[16];

        found = -1;
        pl = -1;
        scratch = mem_heap_new();
        was = mem_heap_use(scratch);
        for (k = 0; k < 187 && found < 0; k++) {
            int j;

            log_format(file, (int)sizeof(file), "%s.QBN", quest_kind(k)->name);
            if (!qbn_load(file, &qq)) {
                continue;
            }
            for (j = 0; j < qq.count[QBN_PLACES]; j++) {
                if (qq.places[j].general != 0 && qq.places[j].fine == 1 && qq.places[j].type == DUN_TYPE_CRYPT) {
                    found = k;
                    pl = j;
                }
            }
        }
        mem_heap_use(was);
        mem_heap_delete(scratch);
        check_expect(found >= 0, "quest machine: no offered quest wants a crypt");
        slot = found >= 0 ? quest_start(&p, found, &how, now) : -1;
        if (slot >= 0 && quest_run(slot)) {
            static Dungeon d;
            char a1[48];
            char a2[48];

            check_expect(dungeon_load_at(17, quest_run(slot)->place_block[pl], &d) && d.dungeon_type == DUN_TYPE_CRYPT,
                         "quest machine: the quest's dungeon is not a crypt");
            if (quest_person_name(slot, 0, a1, (int)sizeof(a1)) && quest_person_name(slot, 0, a2, (int)sizeof(a2))) {
                check_expect(df_eq(a1, a2) && a1[0], "quest machine: a quest person has two names");
            }
        }
    }
    quest_new_game();
    /* Arrival (43): M0C00Y14 sets state 4 when the player stands in its place 0; and a question (29): C0C00Y10 asks one at its start. */
    quest_new_game();
    {
        const QuestRun *ar;
        int asked;
        char prompt[512];

        slot = quest_start(&p, quest_find("M0C00Y14"), &how, now);
        ar = quest_run(slot);
        check_expect(slot >= 0 && ar && !quest_state(slot, 4), "quest machine: M0C00Y14 did not start, or has arrived already");
        if (ar) {
            if (ar->place_dungeon[0]) {
                quest_arrived(&p, 1, ar->place_region[0], ar->place_block[0], 0, 0, now + 10);
            } else {
                quest_arrived(&p, 0, ar->place_region[0], ar->place_index[0], ar->place_block[0], ar->place_record[0], now + 10);
            }
            check_expect(quest_state(slot, 4), "quest machine: arriving at the place did not set its state");
        }
        quest_new_game();
        slot = quest_start(&p, quest_find("C0C00Y10"), &how, now);
        asked = quest_take_prompt(prompt, (int)sizeof(prompt));
        check_expect(slot >= 0 && asked && prompt[0], "quest machine: C0C00Y10 asked no question");
        quest_answer(&p, 1, now + 5);
        check_expect(quest_state(slot, 1) && !quest_state(slot, 2) && !quest_take_prompt(prompt, (int)sizeof(prompt)),
                     "quest machine: Yes did not set its state, or the question stayed");
    }
    quest_new_game();
    /* Topics: none opened yet by M0B00Y00, and the call is whole. */
    quest_new_game();
    {
        static char tnames[4][QUEST_TOPIC_NAME];
        int tslots[4];
        int ta[4];
        int tb[4];

        check_expect(quest_start(&p, quest_find("M0B00Y00"), &how, now) >= 0 && quest_topics(tnames, tslots, ta, tb, 4) == 0,
                     "quest machine: a quest opened topics at its start");
    }
    quest_new_game();
    /* The logbook: two quests, a page each, the first's log and its days left; clicks turn the page and the bottom edge closes. */
    quest_new_game();
    {
        int a;
        int b;

        a = quest_start(&p, quest_find("M0B00Y00"), &how, now);
        b = quest_start(&p, quest_find("C0C00Y13"), &how, now);
        check_expect(a >= 0 && b >= 0 && logbook_open(now + 1440), "quest machine: the logbook did not open");
        check_expect(logbook_pages() == 2 && logbook_page() == 0, "quest machine: the logbook's pages");
        logbook_compose();
        write_screen("build/logbook0.ppm", logbook_pixels());
        check_expect(logbook_click(250, 100) == LOGBOOK_STAY && logbook_page() == 1 && logbook_click(10, 100) == LOGBOOK_STAY && logbook_page() == 0
                         && logbook_key(0x27) == LOGBOOK_STAY && logbook_page() == 1 && logbook_click(160, 185) == LOGBOOK_LEAVE
                         && logbook_key(0x1B) == LOGBOOK_LEAVE,
                     "quest machine: the logbook's turns and its close");
        check_expect(quest_minutes_left(a, now + 1440) > 0 && quest_minutes_left(a, now + 1440) < quest_run(a)->timer_end[0],
                     "quest machine: the days left");
        logbook_close();
    }
    quest_new_game();
    /* The nobles' N0C00Y10 puts its person in a tavern of this town at the start: they stand there, and meeting them is its trigger. */
    quest_new_game();
    for (i = 0; i < quest_table()->count; i++) {
        if (quest_kind(i) && df_eq(quest_kind(i)->name, "N0C00Y10")) {
            quest = i;
        }
    }
    slot = quest_start(&p, quest, &how, now);
    run = quest_run(slot);
    check_expect(slot >= 0 && run && run->person_at[0] == 3 && !run->place_dungeon[3] && run->place_index[3] == dagger,
                 "quest machine: N0C00Y10's person not put in a building of the town");
    if (run) {
        QuestPersonHere here[4];

        n = quest_people_in(17, dagger, run->place_block[3], run->place_record[3], here, 4);
        check_expect(n >= 1 && here[0].slot == slot && here[0].person == 0, "quest machine: the person does not stand in their building");
        check_expect(quest_people_in(17, dagger, run->place_block[3], run->place_record[3] + 1000, here, 4) == 0,
                     "quest machine: the person stands in another building");
        quest_met_person(&p, slot, 0, now + 60);
        check_expect(quest_messages_waiting() > 0 || quest_running(quest) != slot || quest_run(slot)->state[0] != run->state[0],
                     "quest machine: meeting the person did nothing");
    }
    quest_new_game();
}

/* A quest's foe in its dungeon: the stream's hook asked at open puts an ancient vampire in the hold's last draw. */
static int g_test_foes_region = -1;
static int test_foes(int region, int index, int *kinds, int cap)
{
    (void)index;
    if (region != g_test_foes_region || cap < 1) {
        return 0;
    }
    kinds[0] = ENEMY_VAMPIRE_ANCIENT;
    return 1;
}

static void check_quest_foes(void)
{
    Dungeon hold;
    Level level;
    Camera cam;
    float x, y, z, yaw, pitch;
    int found;
    int i;

    keep_clear();
    if (!dungeon_load(&hold)) {
        check_expect(0, "quest foes: the hold did not load");
        return;
    }
    g_test_foes_region = hold.region;
    dunstream_set_foes_hook(test_foes);
    mem_zero(&level, sizeof(level));
    if (!dunstream_open(&hold, &level, &x, &y, &z, &yaw, &pitch)) {
        check_expect(0, "quest foes: the hold's stream did not open");
        dunstream_set_foes_hook(0);
        return;
    }
    camera_set(&cam, x, y, z, yaw, pitch);
    dunstream_fill(&cam);
    found = 0;
    for (i = 0; i < level.scene.nentities; i++) {
        found += level.scene.entities[i].live && dunstream_kind(&level.scene.entities[i], 0) == ENEMY_VAMPIRE_ANCIENT;
    }
    check_expect(found == 1, "quest foes: the quest's vampire does not stand in the hold");
    dunstream_close();
    dunstream_set_foes_hook(0);
    keep_clear();
    printf("quest foes: the hook's ancient vampire stands in the hold\n");
}

/* Counts the engine's automap polygons. */
static void count_polygon(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb)
{
    (void)corner;
    (void)n;
    (void)rgb;
    (*(int *)ctx)++;
}

/*
 * The indoor automap: the hold's place named on arrival and none outdoors, the
 * window's view on the player from above and from behind, its clicks, its chrome's
 * hole and arrow, and what was seen through a save.
 */
static void check_dunmap(void)
{
    Level level;
    Camera cam;
    TravelPose at;
    Player p;
    Character sheet;
    float sx;
    float sy;
    int top;
    int behind;
    int holes;
    int arrow;
    int i;
    df_u32 place;

    keep_clear();
    automap_clear();
    character_new(&sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Test");
    player_init(&p, &sheet);
    travel_start_game();
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    place = automap_place();
    check_expect(place != 0 && (place >> 30) == 1u, "dunmap: the hold's automap place is not named, a dungeon's");
    player_place(&p, at.x, at.y, at.z, at.yaw, 0.0f);
    automap_reveal(&level.scene);
    check_expect(automap_count() > 300, "dunmap: the hold revealed has too few rows");
    check_expect(dunmap_open(p.body.feet, p.yaw), "dunmap: it did not open");
    /* Classic's (1, 1)..(318, 169) of 320 x 200 on 640 x 480 from the bottom: the engine's own test's numbers. */
    /* DFU's (1, 1, 318, 169) is a corner and a size: x 2..638, y from the bottom 480 - 2.4 x 170 .. 480 - 2.4. */
    check_expect(dunmap_view()->x0 == 2.0f && dunmap_view()->x1 == 638.0f && dunmap_view()->y1 > 477.5f && dunmap_view()->y1 < 477.7f
                 && dunmap_view()->y0 > 71.9f && dunmap_view()->y0 < 72.1f, "dunmap: the map's rectangle");
    check_expect(automap_to_screen(dunmap_view(), p.body.feet[0], p.body.feet[1], p.body.feet[2], &sx, &sy),
                 "dunmap: the player is not on the map from above");
    top = 0;
    automap_polygons(&level.scene, dunmap_view(), count_polygon, &top);
    check_expect(top > 20, "dunmap: the hold from above draws nothing");
    write_polygons("build/dunmap-top.pgm", &level.scene, dunmap_view());
    dunmap_compose();
    holes = 0;
    arrow = 0;
    for (i = 0; i < 320 * 200; i++) {
        int x;
        int y;

        x = i % 320;
        y = i / 320;
        if (!dunmap_pixels()[i]) {
            holes += x >= 1 && x < 319 && y >= 1 && y < 170;
            check_expect(x >= 1 && x < 319 && y >= 1 && y < 170, "dunmap: a clear pixel outside the map's hole");
            if (!(x >= 1 && x < 319 && y >= 1 && y < 170)) {
                break;
            }
        } else if (x >= 1 && x < 319 && y >= 1 && y < 170) {
            arrow++;
        }
    }
    /* The hole whole: the arrow is a picture of its own now, over it. */
    check_expect(holes > 52000 && arrow == 0, "dunmap: the hole is not whole");
    {
        float mx;
        float my;
        float angle;

        check_expect(dunmap_marker(&mx, &my, &angle) && mx > 2.0f && mx < 638.0f && my > 72.0f && my < 478.0f,
                     "dunmap: the player's arrow is not on the map");
        /* From above the facing is up the map. */
        check_expect(df_absf(df_wrap_tau(angle + 0.05f) - 0.05f) < 0.05f, "dunmap: from above, the arrow does not point up");
    }
    /* Dragging: the map follows the pointer, the view's centre the other way; a right drag turns it. */
    {
        float cx;
        float cz;
        float yaw;

        cx = dunmap_view()->cx;
        cz = dunmap_view()->cz;
        yaw = dunmap_view()->yaw;
        dunmap_drag(10, 0);
        check_expect(df_absf(dunmap_view()->cx - cx) + df_absf(dunmap_view()->cz - cz) > 1.0f, "dunmap: a drag did not move the view");
        dunmap_drag(-10, 0);
        check_expect(df_absf(dunmap_view()->cx - cx) < 0.5f && df_absf(dunmap_view()->cz - cz) < 0.5f, "dunmap: a drag back did not come back");
        dunmap_turn_by(20);
        check_expect(df_absf(dunmap_view()->yaw - yaw) > 0.1f, "dunmap: a right drag did not turn");
        dunmap_turn_by(-20);
        check_expect(dunmap_repeats(110, 175) && !dunmap_repeats(285, 175) && dunmap_on_map(160, 80) && !dunmap_on_map(160, 180),
                     "dunmap: which buttons repeat, where the map is");
    }
    write_screen("build/dunmap0.ppm", dunmap_pixels());
    /* From behind and above: the player still in view, every face toward the camera. */
    dunmap_click(80, 175);
    check_expect(dunmap_view()->mode == AUTOMAP_3D, "dunmap: the grid did not turn to the 3D view");
    check_expect(automap_to_screen(dunmap_view(), p.body.feet[0], p.body.feet[1], p.body.feet[2], &sx, &sy),
                 "dunmap: the player is not in the 3D view");
    behind = 0;
    automap_polygons(&level.scene, dunmap_view(), count_polygon, &behind);
    write_polygons("build/dunmap-3d.pgm", &level.scene, dunmap_view());
    check_expect(behind > 0, "dunmap: the 3D view draws nothing");
    /* The stairs move the slice a storey; the compass brings the view back; the wheel zooms; Exit leaves. */
    dunmap_click(80, 175);
    dunmap_click(240, 175);
    check_expect(dunmap_storey() == 1 && dunmap_view()->bottom > p.body.feet[1], "dunmap: upstairs");
    {
        float scale;

        scale = dunmap_view()->scale;
        dunmap_wheel(1);
        check_expect(dunmap_view()->scale > scale, "dunmap: the wheel did not zoom in");
    }
    dunmap_click(110, 175);
    dunmap_click(5, 175);
    check_expect(dunmap_storey() == 0 && dunmap_view()->cx == p.body.feet[0], "dunmap: the compass did not come back");
    check_expect(dunmap_key('M') == DUNMAP_LEAVE && dunmap_click(285, 175) == DUNMAP_LEAVE, "dunmap: M or Exit");
    dunmap_close();
    /* What was seen through a save, and none outdoors. */
    {
        SaveState out;
        SaveState in;
        char path[260];
        int rows;

        rows = automap_count();
        save_use_file("DATA\\TEST4.SAV");
        pile_new_game();
        mem_zero(&out, sizeof(out));
        character_new(&out.sheet, RACE_BRETON, CLASS_WARRIOR, 0, "Ada");
        inventory_clear(&out.pack);
        check_expect(save_write(&out), "dunmap: a save did not write");
        automap_clear();
        check_expect(save_read(&in) && automap_count() == rows, "dunmap: what was seen did not come back");
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "dunmap: the test's file did not go");
    }
    travel_leave();
    check_expect(automap_place() == 0, "dunmap: a place is named after leaving it");
    automap_clear();
    keep_clear();
    printf("dunmap: the hold's %d polygons from above, %d from behind; the chrome's hole and arrow; through a save; build/dunmap0.ppm\n",
           top, behind);
}

static void check_title_race_and_biography(void)
{
    Song song;
    char line[64];
    int i;

    check_expect(df_eq(sounds_title(), "05.HMI"), "title: the theme is not 05.HMI");
    memset(&song, 0, sizeof(song));
    check_expect(hmi_load(sounds_title(), 0, &song) && song.count > 0, "title: 05.HMI did not read");
    for (i = 1; i <= 8; i++) {
        const CareerRace *race;

        race = career_race(i);
        check_expect(race && sndfile_by_id(race->clip) == 365 + i, "race: the choosing clip is not that race's sound");
    }
    create_reputation_text(1, 10, line, (int)sizeof(line));
    check_expect(df_eq(line, "Merchants: higher"), "biography: a gain is not higher");
    create_reputation_text(0, -5, line, (int)sizeof(line));
    check_expect(df_eq(line, "Commoners: lower"), "biography: a loss is not lower");
    create_reputation_text(4, 0, line, (int)sizeof(line));
    check_expect(df_eq(line, "Underworld: unchanged"), "biography: no change is not unchanged");
    check_expect(create_constellation_blue(0) == 8 && create_constellation_blue(1) == 32
                 && create_constellation_blue(3) == 80,
                 "questions: the stars do not light by 24 a leaning");
}

static void check_death_drops_world(void)
{
    Level level;
    Camera cam;
    TravelPose at;
    int i;
    int opened;

    keep_clear();
    interact_say("You have died.");
    travel_start_game();
    if (!travel_go(&level, &cam, &at)) {
        return;
    }
    opened = 0;
    for (i = 0; i < level.scene.nmovers; i++) {
        mover_set_end(&level.scene.movers[i], 1);
        opened = 1;
        break;
    }
    check_expect(opened, "death: the hold opened no door to leave ajar");
    keep_clear();
    travel_drop();
    check_expect(keep_count() == 0, "death: closing the dead world wrote its doors back into the keep");
    check_expect(interact_saying(0) == 0, "death: You have died stayed after the world was dropped");
}

void check_play(void)
{
    check_menu();
    /* As game_setup does: read before any screen's heap is current. */
    standing_new_game(1u);
    check_mouse();
    check_settings();
    check_viewopt();
    check_rules();
    check_character();
    check_items();
    check_weapon();
    check_hudbar();
    check_controls();
    check_combat();
    check_foes();
    check_foes_due();
    check_armor();
    check_dye();
    check_loot();
    check_piles();
    check_bag();
    check_levelling();
    check_kit_and_words();
    check_create();
    check_title_race_and_biography();
    check_death_drops_world();
    check_sheet();
    check_class_and_wear();
    check_creatures_more();
    check_blows_in_reach();
    check_sheets_move();
    check_body_more();
    check_hands();
    check_rest();
    check_save();
    check_hold();
    check_keep();
    check_dunstream();
    check_every_way_out();
    check_travel();
    check_standing();
    check_people_sums();
    check_words();
    check_talk();
    check_shop_and_court();
    check_bank();
    check_people_in_town();
    check_house_container();
    check_house_interiors();
    check_citizen();
    check_senses();
    check_wider_world();
    check_holidays();
    check_theft();
    check_crouch();
    check_config();
    check_saves_and_files();
    check_ills();
    check_ships();
    check_lodging();
    check_guilds();
    check_names();
    check_quest_files();
    check_quest_machine();
    check_quest_foes();
    check_riding();
    check_wagon();
    check_marker();
    check_dunmap();
    check_goto();
    check_standing_saved();
}
