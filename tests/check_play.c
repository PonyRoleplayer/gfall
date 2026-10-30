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
#include "game/dagger/formats/dungeon.h"
#include "engine/scene/stream.h"
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

    check_expect(item_table()->count == 31 && weapon_table()->count == 18 && armor_table()->count == 11
                     && material_table()->count == MATERIAL_COUNT && loot_table()->count == 21,
                 "items: MAIN.DAT lacks classic's 31 items (11 of armour, 18 swung, arrows, gold), 12 materials or 21 loot tables");
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
#define CREATE_YES_X 100
#define CREATE_NO_X 210
#define CREATE_BOX_Y 155
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
    check_expect(create_step() == CREATE_CLASS, "create: choosing a woman did not go on");
    create_compose();
    write_picture(create_pixels(), "build/create2.ppm");
    /* The Warrior, class 16, on the list. */
    create_click(150, 22 + 16 * 8 + 8 + 2);
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
        check_expect(fs_beside(path, (int)sizeof(path), "DATA\\TEST2.SAV") && remove(path) == 0, "save: the test's file did not go");
    }
    check_expect(!save_exists() && !save_any(), "save: a save is there after its file went");
    save_use_prefix("DATA\\SAVE");
    save_use_slot(0);
    printf("save: a door, a body, a pile, a sheet and a pack, round the file and back\n");
}

void check_play(void)
{
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
    check_body_more();
    check_hands();
    check_rest();
    check_save();
    check_hold();
    check_keep();
    check_dunstream();
}
