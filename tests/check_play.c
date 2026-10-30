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

/* The composed screen to a picture: build/bag0.ppm, for a person to look at against classic's. */
static void write_screen(const char *path)
{
    df_u8 rgb[768];
    const df_u8 *pixels;
    FILE *f;
    int i;

    pixels = bag_pixels();
    if (!img_art_palette(rgb)) {
        check_expect(0, "bag: no ART_PAL.COL");
        return;
    }
    f = fopen(path, "wb");
    if (!f) {
        check_expect(0, "bag: the picture did not write");
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
    write_screen("build/bag0.ppm");

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

    /* Info mode's box: it shows, and the next click only puts it away. */
    bag_click(BAG_BUTTON_X, BAG_INFO_Y);
    i = p.pack.count;
    bag_click(BAG_LOCAL_CELL_X, BAG_ROW_Y);
    bag_compose();
    write_screen("build/bag1.ppm");
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
    check_hold();
    check_keep();
    check_dunstream();
}
