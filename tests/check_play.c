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
#include "game/dagger/play/weapon.h"
#include "game/dagger/formats/cif.h"
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
    for (i = 0; i < level.scene.nentities; i++) {
        ents += level.scene.entities[i].live;
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
    check_expect(sprites == world_flat_count(whole), "dunstream: the hold's ring has another count of flats");
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

        if (character_new(&sheet, CHARACTER_START_RACE, CHARACTER_START_CLASS, 0, "Test")) {
            player_init(&p, &sheet);
            player_place(&p, x, y, z, yaw, pitch);
            memset(&in, 0, sizeof(in));
            walk(&p, &level.scene, &in, 60);
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

    check_expect(item_table()->count == 19 && weapon_table()->count == 18 && material_table()->count == MATERIAL_COUNT,
                 "items: MAIN.DAT lacks classic's 19 weapon items (18 swung, and arrows) or 10 materials");
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
    check_expect(inv.weapon == 0 && inventory_weapon(&inv)->template_id == ITEM_LONGSWORD,
                 "items: the hand loses its weapon when another leaves the pack");
    inventory_remove(&inv, 0);
    check_expect(inv.count == 0 && !inventory_weapon(&inv), "items: the hand keeps a weapon that left the pack");

    inventory_clear(&p.pack);
    commands_setup();
    commands_player(&p);
    cmd_run("additem dwarven claymore; additem long sword; additem Dai-Katana Ebony; equip 1");
    check_expect(p.pack.count == 3 && p.pack.item[0].material == MATERIAL_DWARVEN && p.pack.item[1].material == MATERIAL_IRON
                     && p.pack.item[2].template_id == 123 && p.pack.item[2].material == MATERIAL_EBONY && p.pack.weapon == 1,
                 "items: additem and equip did not fill the pack and the hand");
    check_expect(check_near(inventory_weight(&p.pack), 5.5f + 4.5f + 1.75f, 0.001f), "items: the pack's weight");
    cmd_run("additem potato; equip 7; equip none");
    check_expect(p.pack.count == 3 && p.pack.weapon == -1, "items: a bad additem or equip changed the pack");
    commands_player(0);
    cmd_run("additem dagger");
    check_expect(p.pack.count == 3, "items: additem out of play added to a pack");
    printf("items: %d weapons swung, %d materials; additem, items, equip\n", weapon_table()->count, material_table()->count);
}

/* A card that takes every tile, for the swing frames' bytes: a tile is its side squared, twice for a cutout. */
static df_u32 wc_bytes(int side, int cutout)
{
    return (df_u32)side * (df_u32)side * (cutout ? 2u : 1u);
}

static int wc_upload(const df_u8 *texels, int side, int cutout, df_u32 addr, df_u32 *handle)
{
    (void)texels;
    (void)side;
    (void)cutout;
    *handle = addr + 1;
    return 1;
}

static void wc_release(df_u32 handle)
{
    (void)handle;
}

static void wc_begin(int cutout, int alpha)
{
    (void)cutout;
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
 * cropped as weapon.c sends it (*whole: as stored, uncropped): one shows
 * at a time beside the armed picture and the HUD, inside the game's
 * GAME_PICTURE_KB (docs/game.md, Combat). *exact: every frame's crop
 * loses no pixel and is tight.
 */
static df_u32 largest_swing_frame(df_u32 *whole, int *exact)
{
    static const PicOps ops = { wc_bytes, wc_upload, wc_release, wc_begin, wc_palette, wc_quad, wc_end };
    static df_u8 rgb[768];
    df_u32 most;
    int n;

    most = 0;
    *whole = 0;
    *exact = 1;
    pic_begin(0, 1024u * 1024u, &ops);
    for (n = 0; n <= 10; n++) {
        char name[16];
        CifSet set;
        int i;

        sprintf(name, "WEAPON%02d.CIF", n);
        if (!cif_open(name, &set)) {
            continue;
        }
        for (i = 0; i < set.n; i++) {
            int id;
            int pics;
            int on;
            df_u32 used;
            df_u32 room;

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
    most = largest_swing_frame(&whole, &exact);
    check_expect(exact, "weapon: a frame's crop loses a pixel or is not tight");
    check_expect(most > 0 && most <= 96u * 1024u, "weapon: a cropped swing frame takes more than 96 KB of the card");
    printf("weapon: swings of 5 frames at %.3f s, all six seen, fists, a bow; the largest frame %u bytes on the card"
           " cropped, %u whole\n",
           each, (unsigned)most, (unsigned)whole);
}

void check_play(void)
{
    check_rules();
    check_character();
    check_items();
    check_weapon();
    check_hold();
    check_keep();
    check_dunstream();
}
