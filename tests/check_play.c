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
#include "game/dagger/places/town.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/words.h"
#include "game/dagger/play/talk.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/service.h"
#include "game/dagger/play/shop.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/court.h"
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
#include "game/dagger/play/slots.h"
#include "engine/scene/clock.h"
#include "engine/platform/fs.h"
#include "game/dagger/formats/biog.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/play/doll.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/foes.h"
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

    check_expect(item_table()->count == 32 && weapon_table()->count == 18 && armor_table()->count == 11
                     && material_table()->count == MATERIAL_COUNT && loot_table()->count == 21,
                 "items: MAIN.DAT lacks classic's 32 items (11 of armour, 18 swung, arrows, a letter of credit, gold), 12 materials or 21 loot tables");
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
    for (i = 0; i < 20; i++) {
        create_click(50, 24);
    }
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_SKILLS, "create: every point placed did not go on");
    create_compose();
    write_picture(create_pixels(), "build/create7.ppm");
    /* Six points a group: the spinner's right on line 0, then on a major and a minor line. */
    for (i = 0; i < 6; i++) {
        create_click(235, 33);
    }
    create_click(CREATE_OK_X, CREATE_OK_Y);
    check_expect(create_step() == CREATE_SKILLS && create_note() != 0, "create: OK went on with the major and minor points left");
    create_click(100, 83);
    for (i = 0; i < 6; i++) {
        create_click(235, 82);
    }
    create_click(100, 132);
    for (i = 0; i < 6; i++) {
        create_click(235, 131);
    }
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

    /* Escape on the map starts at once as the default. */
    check_expect(create_open() && create_key(0x1B) == CREATE_SKIPPED, "create: Escape on the map did not skip");
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

    /* Escape leaves it from the panel. */
    check_expect(rest_open(&p) && rest_key(0x1B) == REST_LEAVE, "rest: Escape did not leave");
    rest_close();
    printf("rest: its sums, until healed, for a while, loitering; build/rest0..2.ppm\n");
}

/* What the test's saves are called, beside where a player's would be: TEST0.SAV and on, gone after. */
#define SAVE_TEST_PREFIX "DATA\\TEST"

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

    save_use_prefix(SAVE_TEST_PREFIX);
    save_use_slot(2);
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
    out.minute = 600.0f;
    out.day = 12345;
    out.day_zero = 12000;
    out.seed = loot_seed();
    df_copy(out.name, "Kept Game", SAVE_NAME);
    stream_keep_now();
    check_expect(keep_find(door_key) != 0, "save: a door open in a loaded block was not flushed");
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
    /* The slots' screen: to load, a slot with a save is chosen, an empty one says so; its name is shown. */
    {
        char name[SAVE_NAME];
        char path[512];

        check_expect(save_any() && save_peek_name(2, name) && strcmp(name, "Kept Game") == 0, "save: the slot's name did not read");
        check_expect(!save_peek_name(0, name), "save: an empty slot has a name");
        check_expect(slots_open(0), "save: the load screen did not open");
        slots_compose();
        write_picture(slots_pixels(), "build/slots0.ppm");
        check_expect(slots_click(10, 58) == SLOTS_STAY && slots_note() != 0, "save: an empty slot was chosen to load");
        /* Slot 2 is the left column's third. */
        check_expect(slots_click(10, 188) == SLOTS_CHOSEN && slots_chosen() == 2, "save: a slot with a save was not chosen");
        slots_close();
        /* To save, a slot asks for a name; Enter chooses it. */
        check_expect(slots_open(1), "save: the save screen did not open");
        slots_click(170, 58);
        slots_key('M');
        slots_key('Y');
        check_expect(slots_key(0x0D) == SLOTS_CHOSEN && slots_chosen() == 3 && strcmp(slots_chosen_name(), "My") == 0,
                     "save: the slot's name was not as typed");
        slots_compose();
        write_picture(slots_pixels(), "build/slots1.ppm");
        check_expect(slots_click(150, 155) == SLOTS_LEAVE, "save: Cancel did not leave");
        slots_close();
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
            save_use_slot(4);
            check_expect(save_write(&a), "save: the custom class's save did not write");
            mine.hp_per_level = 5;
            career_set_custom(&mine);
            check_expect(save_read(&b) && career_class(CLASS_CUSTOM)->hp_per_level == 21, "save: the custom class did not come back");
            check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST4.SAV") && remove(path) == 0, "save: the custom class's file did not go");
            save_use_slot(2);
        }
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST2.SAV") && remove(path) == 0, "save: the test's file did not go");
    }
    check_expect(!save_exists() && !save_any(), "save: a save is there after its file went");
    save_use_prefix("DATA\\SAVE");
    save_use_slot(0);
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

    save_use_prefix(SAVE_TEST_PREFIX);
    save_use_slot(3);
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
        df_copy(out.name, "Travel", SAVE_NAME);
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
    standing_new_game(5u);
    printf("bank: gold, letters and loans at the sums, a default a crime; the screen by clicks and keys; build/bank0.ppm, service0.ppm\n");
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

    save_use_prefix(SAVE_TEST_PREFIX);
    save_use_slot(5);
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
    df_copy(out.name, "People", SAVE_NAME);
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

void check_play(void)
{
    /* As game_setup does: read before any screen's heap is current. */
    standing_new_game(1u);
    check_rules();
    check_character();
    check_items();
    check_weapon();
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
    check_senses();
    check_goto();
    check_standing_saved();
}
