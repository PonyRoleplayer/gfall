#include "check_magic.h"
#include "check_util.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/ailments.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/describe.h"
#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/core/mem.h"
#include "engine/scene/keep.h"
#include "engine/scene/spawn.h"
#include "engine/scene/walker.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/formats/textrsc.h"
#include <stdio.h>
#include <string.h>

void seam_present(void);
const df_u8 *seam_cast_screen(const df_u8 **palette);
static GameState g_before, g_fixture, g_bad;
static Scene g_scene;
static Entity g_enemy;
static Instance g_model[2];
static Mesh g_mesh[2];
static Tri g_tri[4];
static Camera g_camera;
static int g_version;
static char g_soul_text[1024];
void check_magic_prepare(void)
{
    check_expect(textrsc_read(1004,g_soul_text,sizeof(g_soul_text)) > 0,"read the original soul-gem description before isolating save writes");
}
static int magic_words(int id, char *text, int capacity)
{
    if (id != 1004) { return -1; }
    df_copy(text,g_soul_text,capacity); return (int)strlen(text);
}

static void fixture(int wall)
{
    static const float vertices[4][3][3] = {
        {{-2000,0,-2000},{2000,0,-2000},{2000,0,2000}},
        {{-2000,0,-2000},{2000,0,2000},{-2000,0,2000}},
        {{-1000,0,240},{1000,0,240},{1000,1000,240}},
        {{-1000,0,240},{1000,1000,240},{-1000,1000,240}}
    };
    int i, k, kind, passive, rate;
    magic_close(); dungeon_close(); spawn_clear(); keep_clear();
    check_expect(state_restore(&g_fixture),"restore isolated magic fixture");
    mem_zero(&g_scene,sizeof(g_scene)); mem_zero(g_model,sizeof(g_model));
    mem_zero(g_mesh,sizeof(g_mesh)); mem_zero(g_tri,sizeof(g_tri));
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 3; k++) { g_tri[i].x[k] = vertices[i][k][0]; g_tri[i].y[k] = vertices[i][k][1]; g_tri[i].z[k] = vertices[i][k][2]; }
    }
    for (i = 0; i < 2; i++) {
        g_mesh[i].tris = g_tri+i*2; g_mesh[i].ntris = 2;
        g_model[i].mesh = g_mesh+i; g_model[i].m[0] = g_model[i].m[4] = g_model[i].m[8] = 1;
    }
    g_mesh[0].ex = g_mesh[0].ez = 2000;
    g_mesh[1].ex = 1000; g_mesh[1].cy = g_mesh[1].ey = 500; g_mesh[1].cz = 240;
    g_scene.models = g_model; g_scene.nmodels = wall ? 2 : 1; g_scene.version = 30000+ ++g_version;
    entity_init(&g_enemy); g_enemy.live = 1; g_enemy.radius = 12; g_enemy.z = 300;
    g_enemy.key.place = 0x4000c35fu; g_enemy.key.thing = 0x7654;
    g_enemy.pose[ENTITY_WALK].w[0] = 40; g_enemy.pose[ENTITY_WALK].h[0] = 80;
    g_scene.entities = &g_enemy; g_scene.nentities = 1;
    kind = 0; passive = 1; rate = 71;
    dungeon_blood(-1,-1); dungeon_open(&g_scene,&kind,&passive,&rate); dungeon_loaded(0); magic_open(&g_scene);
    move_init(); move_place(0,WALK_EYE,0,0); move_camera(&g_camera);
}
static df_u32 owned(int id)
{
    df_u32 book;
    book = inventory_book(); if (!book) { book = item_create(game_state()->inventory_id,27,0,0); }
    return spell_create(book,id);
}
static int effects(int group)
{
    int i, n;
    n = 0;
    for (i = 0; i < EFFECTS; i++) { n += magic_state()->effect[i].active && magic_state()->effect[i].group == group; }
    return n;
}
static int ailments(void)
{
    int i, n;
    n = 0; for (i = 0; i < AILMENTS; i++) { n += magic_state()->ailment[i].active; } return n;
}
static void costs_and_durations(void)
{
    int strength, raised, rounds, i;
    df_u32 spell, uses;
    fixture(0);
    check_expect(spell_cost(spell_def(7),character_state()) == 48 &&
                 spell_cost(spell_def(82),character_state()) == 102 &&
                 spell_cost(spell_def(64),character_state()) == 36 &&
                 spell_cost(spell_def(4),character_state()) == 33,
                 "original cost formulas preserve division order, school reduction and target factors");
    strength = character_state()->attribute[ATTR_STR]; magic_apply(82,-1,-1,10,1);
    raised = character_state()->attribute[ATTR_STR];
    check_expect(raised >= strength+21 && raised <= strength+30 && effects(9) == 1,
                 "Orc Strength applies its original level magnitude and one timed modifier");
    magic_apply(82,-1,-1,10,1);
    check_expect(character_state()->attribute[ATTR_STR] == raised && effects(9) == 1,
                 "recasting extends a matching effect rather than stacking its attribute bonus");
    check_expect(state_capture_world() && save_write(6) && save_read(6),"save an active attribute spell");
    check_expect(character_state()->attribute[ATTR_STR] == raised && effects(9) == 1,"loading does not apply the same attribute modifier twice");
    rounds = 0;
    for (i = 0; i < EFFECTS; i++) { if (magic_state()->effect[i].active) { rounds = magic_state()->effect[i].rounds; } }
    state_pass_minutes(rounds);
    check_expect(character_state()->attribute[ATTR_STR] == raised,"the original last round ticks before effect removal");
    state_pass_minutes(1);
    check_expect(character_state()->attribute[ATTR_STR] == strength && !effects(9),"expiry restores exactly the applied attribute delta");
    character_state()->health = 100; spell = owned(64); uses = game_state()->skill_uses[effect_school(10)];
    check_expect(magic_ready(spell) && character_state()->health > 100 && character_state()->magicka == 964 &&
                 game_state()->skill_uses[effect_school(10)] == uses+1,"readying a self spell pays once, trains its school and applies healing");
    character_state()->conditions |= 0x100;
    check_expect(!magic_ready(spell) && character_state()->magicka == 964,"silence refuses a cast before payment");
    character_state()->conditions &= ~0x100; spell = owned(7);
    i = character_state()->magicka; uses = game_state()->skill_uses[effect_school(10)];
    check_expect(magic_ready(spell) && magic_pending() && character_state()->magicka == i,"a ranged spell is readied unpaid: spell points go at the cast");
    magic_abort();
    check_expect(!magic_pending() && character_state()->magicka == i && game_state()->skill_uses[effect_school(10)] == uses &&
                 magic_recast(),"putting a readied spell away costs and trains nothing, and recast readies the last one cast");
    magic_abort();
}
static void missiles_and_art(void)
{
    static const int ids[5] = {7,16,29,31,36};
    PlayIn input;
    df_u32 spell;
    const df_u8 *pixels, *palette;
    int i, frame, before, opaque, mirror;
    FILE *file;
    fixture(1); mem_zero(&input,sizeof(input)); input.dt = 0.1f; input.mouse_pressed = 1;
    spell = owned(7); before = dungeon_health(0);
    i = character_state()->magicka;
    check_expect(magic_ready(spell) && character_state()->magicka == i,"a missile spell costs nothing when chosen");
    magic_frame(&g_scene,&g_camera,&input,0); input.mouse_pressed = 0;
    check_expect(!magic_pending() && character_state()->magicka < i && magic_state()->last == spell,"the attack button casts it and pays then");
    magic_abort(); fixture(1); spell = owned(7); input.mouse_pressed = 1; god_set(1);
    check_expect(magic_ready(spell),"god mode readies a spell without charging magicka");
    magic_frame(&g_scene,&g_camera,&input,0); input.mouse_pressed = 0;
    check_expect(!magic_pending() && magic_state()->missile[0].live &&
                 magic_state()->missile[0].z > 149 && magic_state()->missile[0].z < 151,
                 "the original missile starts 110 units ahead and flies at 400 units per second");
    check_expect(state_capture_world() && save_write(6) && save_read(6),"save a missile in flight");
    magic_open(&g_scene);
    for (i = 0; i < 8; i++) { magic_frame(&g_scene,&g_camera,&input,0); }
    check_expect(dungeon_health(0) == before,"a reloaded missile cannot damage a creature behind the nearer wall");
    fixture(0); god_set(1); spell = owned(7); magic_ready(spell); input.mouse_pressed = 1;
    magic_frame(&g_scene,&g_camera,&input,0); input.mouse_pressed = 0;
    for (i = 0; i < 8; i++) { magic_frame(&g_scene,&g_camera,&input,0); }
    check_expect(dungeon_health(0) < before,"an unobstructed flying spell reaches normal dungeon combat");
    fixture(0); god_set(1);
    for (i = 0; i < 5; i++) {
        spell = owned(ids[i]); magic_ready(spell); input.mouse_pressed = 1;
        magic_frame(&g_scene,&g_camera,&input,0); input.mouse_pressed = 0;
        input.dt = 0.2f; magic_frame(&g_scene,&g_camera,&input,0); magic_draw(); input.dt = 0.1f;
        pixels = seam_cast_screen(&palette); opaque = 0; mirror = 1;
        for (frame = 0; pixels && frame < 64000; frame++) {
            opaque += pixels[frame] != 0;
            if (pixels[frame] != pixels[(frame/320)*320+319-frame%320]) { mirror = 0; }
        }
        check_expect(pixels && opaque > 100 && mirror,"each element draws both original casting hands as a transparent mirrored sprite");
        check_expect(sndfile_by_id(cast_sound(i)) >= 0 && sndfile_by_id(impact_sound(i)) >= 0,
                     "each element's original casting and impact sound resolves in DAGGER.SND");
        if (i == 0 && pixels) {
            file = fopen("build/gfall-casting-fire.ppm","wb");
            if (file) { fprintf(file,"P6\n320 200\n255\n"); for (frame = 0; frame < 64000; frame++) { fwrite(palette+pixels[frame]*3,1,3,file); } fclose(file); }
        }
    }
    god_set(0);
}
static void poison_variants(void)
{
    int id, changed;
    df_u32 drug;
    CharacterState before;
    for (id = 128; id < 140; id++) {
        fixture(0); character_state()->magicka = 500; character_state()->fatigue = 3000;
        before = *character_state();
        check_expect(poison_apply(-1,id,1),"each original poison has a readable onset/course definition");
        magic_state()->ailment[0].delay = 0;
        state_pass_minutes(1); changed = 0;
        switch (id) {
        case 128: case 130: case 133: changed = character_state()->health < before.health; break;
        case 129: changed = character_state()->health == before.health-2 && character_state()->attribute[ATTR_END] == 49; break;
        case 131: changed = character_state()->attribute[ATTR_STR] < 50 && character_state()->attribute[ATTR_AGI] < 50 && character_state()->attribute[ATTR_SPD] < 50; break;
        case 132: changed = character_state()->fatigue < before.fatigue; break;
        case 134: changed = character_state()->magicka < 500 && character_state()->attribute[ATTR_WIL] < 50; break;
        case 135: changed = character_state()->attribute[ATTR_WIL] < 50 && character_state()->attribute[ATTR_PER] < 50; break;
        case 136: changed = character_state()->fatigue < before.fatigue && character_state()->attribute[ATTR_LUC] > 50; break;
        case 137: changed = character_state()->attribute[ATTR_INT] < 50 && character_state()->attribute[ATTR_STR] > 50; break;
        case 138: changed = character_state()->fatigue > before.fatigue && character_state()->attribute[ATTR_WIL] < 50; break;
        case 139: changed = character_state()->magicka > 500 && character_state()->attribute[ATTR_END] < 50; break;
        }
        check_expect(changed,"each poison reaches its original damaging/draining or beneficial stat combination");
    }
    fixture(0); drug = item_create(game_state()->inventory_id,0,0,0);
    check_expect(drug && possessions_use(drug) && !object_at(drug) && ailments() == 1 &&
                 magic_state()->ailment[0].id == 136,"inventory Use consumes an Indulcet drug into its original delayed poison record");
}
static void infections(void)
{
    int health, attribute, delay, i;
    fixture(0); character_state()->level = 1;
    check_expect(!poison_apply(-1,128,1) && !disease_infect(-1,0,1),"original level-one protection blocks poison and disease even with the resistance check skipped");
    character_state()->level = 10;
    check_expect(poison_apply(-1,128,1) && ailments() == 1,"a poison creates a distinct timed ailment record");
    delay = magic_state()->ailment[0].delay; health = character_state()->health;
    check_expect(delay == 4 && state_capture_world() && save_write(6) && save_read(6),"Nux Vomica's original four-minute onset is saved");
    state_pass_minutes(delay); check_expect(character_state()->health == health,"latent poison causes no premature health damage");
    state_pass_minutes(1); check_expect(character_state()->health <= health-2 && character_state()->health >= health-12,"poison onset uses the original 2..12 health tick");
    ailments_cure(-1,1); check_expect(!ailments(),"Cure Poison removes poison records");
    check_expect(poison_apply(-1,129,1),"apply an attribute-draining poison");
    attribute = character_state()->attribute[ATTR_END]; magic_state()->ailment[0].delay = 0;
    state_pass_minutes(1); check_expect(character_state()->attribute[ATTR_END] == attribute-1,"Arsenic drains Endurance and tracks its reversible change");
    ailments_cure(-1,1); check_expect(character_state()->attribute[ATTR_END] == attribute,"curing restores the poison's drained attribute");
    state_pass_minutes(1439-game_state()->minutes%1440);
    health = character_state()->health; attribute = character_state()->attribute[ATTR_STR];
    check_expect(disease_infect(-1,0,1),"infect from the executable's actual disease table");
    check_expect(character_state()->health == health,"disease has no immediate daily damage");
    state_pass_minutes(1);
    check_expect(character_state()->health < health && character_state()->attribute[ATTR_STR] < attribute,
                 "a day boundary applies one disease damage roll to its original affected stats");
    ailments_cure(-1,0); check_expect(!ailments() && character_state()->attribute[ATTR_STR] == attribute,"Cure Disease restores its tracked attribute drain");
    poison_apply(-1,128,1); magic_state()->ailment[0].delay = 0; god_set(1); health = character_state()->health;
    for (i = 0; i < 4; i++) { state_pass_minutes(1); }
    check_expect(character_state()->health == health,"god mode prevents health loss from poison ticks");
    god_set(0);
}
static void answer(const char *text)
{
    PlayIn input;
    mem_zero(&input,sizeof(input));
    for (; *text; text++) { input.ch = *text; possessions_frame(&input); }
    input.ch = 13; possessions_frame(&input);
}
static void defenses_and_recall(void)
{
    MoveIn move;
    int health, shield, i;
    float z;
    fixture(0); character_state()->race = 0;
    random_seed(1);
    check_expect(magic_resist(-1,0,2,0) == 55,"the original resistance roll retains partial 5-percent damage steps");
    magic_apply(17,-1,-1,10,1); shield = 0;
    for (i = 0; i < EFFECTS; i++) { if (magic_state()->effect[i].active) { shield = magic_state()->effect[i].magnitude; } }
    health = character_state()->health; damage_player(shield-1,-1);
    check_expect(character_state()->health == health,"Shield absorbs damage using its computed point pool");
    damage_player(3,-1);
    check_expect(character_state()->health == health-2 && !(character_state()->conditions & 0x400000),"a broken Shield passes only excess damage and clears its flag");
    magic_apply(47,-1,-1,10,1); character_state()->magicka = 0; health = character_state()->health;
    random_seed(1); magic_apply(7,0,-1,10,0);
    check_expect(character_state()->health == health && character_state()->magicka == 48,"Spell Absorption converts an incoming spell's actual cost into magicka");
    magic_remove(-1,-1); magic_apply(46,-1,-1,20,1); health = dungeon_health(0);
    random_seed(1); magic_apply(7,0,-1,10,0);
    check_expect(dungeon_health(0) < health,"Spell Reflection redirects the bundle at its caster");
    fixture(0); magic_apply(50,0,-1,40,1);
    check_expect(character_state()->conditions & 1,"Paralysis installs its timed condition after its original chance check");
    mem_zero(&move,sizeof(move)); move.region = 1; move.dt = 0.1f; z = move_body()->feet[2];
    move_frame(&g_scene,&move);
    check_expect(move_body()->feet[2] == z,"a paralyzed player cannot walk");
    magic_apply(10,-1,-1,40,1);
    check_expect(!(character_state()->conditions & 1) && !effects(0),"Free Action removes the paralysis record and condition");
    character_state()->race = 4; magic_apply(50,0,-1,40,1);
    check_expect(!(character_state()->conditions & 1),"High Elves retain the original paralysis immunity");
    character_state()->race = 0; magic_apply(6,-1,-1,10,1);
    check_expect(!magic_sees(0) && magic_sees(18),"ordinary creatures lose an invisible player while the original undead/daedra see through it");
    magic_hostile(); check_expect(!(character_state()->conditions & 4),"a hostile action breaks ordinary Invisibility");
    magic_remove(-1,-1); magic_apply(82,-1,-1,10,1); magic_apply(59,-1,-1,10,1); answer("1");
    check_expect(!effects(9),"Dispel Magic removes a selected self-cast spell without a resistance roll");
    magic_apply(94,-1,-1,10,1); answer("anchor");
    check_expect(magic_state()->recall.valid && state_capture_world() && save_write(6) && save_read(6),"Recall's anchor is saved independently of the modal callback");
    move_place(120,WALK_EYE,90,512); magic_apply(94,-1,-1,10,1); answer("recall");
    check_expect(!magic_state()->recall.valid && move_body()->feet[0] == 0 && move_body()->feet[2] == 0 && !move_yaw(),
                 "Recall returns to the saved anchor and consumes it");
}
static void souls_and_rest(void)
{
    StrikeInfo strike;
    PlayIn input;
    df_u32 gem;
    int i, soul;
    char text[512];
    fixture(0); gem = item_create(game_state()->inventory_id,27,1,0);
    magic_apply(62,-1,0,20,1); move_place(0,WALK_EYE,270,0); move_camera(&g_camera);
    mem_zero(&strike,sizeof(strike)); strike.hand = 0; god_set(1);
    for (i = 0; i < 40 && !g_enemy.dead; i++) { dungeon_strike(&g_scene,&g_camera,&strike); }
    god_set(0); soul = 0;
    for (i = 0; i < OBJECTS; i++) {
        if (game_state()->object[i].id && game_state()->object[i].parent == gem && game_state()->object[i].type == RECORD_SOUL) { soul++; }
    }
    set_word_variants(0); set_words(magic_words); item_info(gem,text,sizeof(text));


    check_expect(g_enemy.dead && soul == 1 && strstr(text,foe_def(0)->name),"a killed Soul Trap target fills one empty gem and its item description names the soul");
    check_expect(state_capture_world() && save_write(6) && save_read(6),"a captured soul survives the ordinary owned-object save codec");
    fixture(0); character_state()->health = 1; poison_apply(-1,128,1); magic_state()->ailment[0].delay = 0;
    move_body()->grounded = 1; rest_reset();
    check_expect(rest_open(&g_scene,&g_camera),"open Rest before a fatal poison tick");
    mem_zero(&input,sizeof(input)); input.on_picture = 1; input.mouse_pressed = 1; input.cx = 160; input.cy = 70;
    rest_frame(&input,&g_camera); input.mouse_pressed = 0; input.dt = 0.6f; rest_frame(&input,&g_camera);
    check_expect(!character_state()->health && !rest_state()->open,"fatal poison interrupts Rest before hourly recovery can revive the player");
}
static void creature_casting(void)
{
    CharacterState who;
    ClassInfo career;
    int i, cast, base[8], magicka, kind, passive, rate;
    fixture(0); kind = 1; passive = 1; rate = 71; g_enemy.z = 400;
    dungeon_open(&g_scene,&kind,&passive,&rate); dungeon_loaded(0);
    magic_target(0,&who,&career,base); magicka = who.magicka;
    cast = 0;
    for (i = 0; i < 400 && !cast; i++) { cast = magic_creature(0,&g_camera); }
    magic_target(0,&who,&career,base);
    check_expect(cast && who.magicka < magicka,"an imp selects an original ranged spell and spends its own magicka");
    magicka = who.magicka;
    check_expect(state_capture_world() && save_write(6) && save_read(6),"save a creature's spent spell points");
    dungeon_open(&g_scene,&kind,&passive,&rate); magic_target(0,&who,&career,base);
    check_expect(who.magicka == magicka,"reopening a creature does not refill its saved magicka");
}

void check_magic(void)
{
    MotionState motion;
    int i;
    g_before = *game_state(); move_capture(&motion); g_fixture = g_before;
    mem_zero(&g_fixture.magic,sizeof(g_fixture.magic));
    g_fixture.who.level = 10; g_fixture.who.health = g_fixture.who.max_health = 500;
    g_fixture.who.magicka = g_fixture.who.max_magicka = 1000; g_fixture.who.spell_points_bonus = 0;
    g_fixture.who.conditions = 0; g_fixture.who.fatigue = 6400;
    g_fixture.career.immunity = g_fixture.career.resistance = g_fixture.career.tolerance = g_fixture.career.weakness = 0;
    for (i = 0; i < ATTRS; i++) { g_fixture.who.attribute[i] = g_fixture.base_attribute[i] = 50; }
    for (i = 0; i < SKILLS; i++) { g_fixture.who.skill[i] = 50; }
    costs_and_durations(); missiles_and_art(); defenses_and_recall(); souls_and_rest(); creature_casting(); poison_variants(); infections();
    g_bad = *game_state(); g_bad.magic.ready = 0x12345678u;
    check_expect(!state_valid(&g_bad),"save validation rejects a ready spell with no owned record");
    g_bad = *game_state(); g_bad.magic.ailment[0].active = 1; g_bad.magic.ailment[0].id = 127;
    check_expect(!state_valid(&g_bad),"save validation rejects an unknown ailment identity");
    magic_close(); dungeon_close(); spawn_clear(); god_set(0);
    set_words(textrsc_read);
    state_restore(&g_before); state_restore_world(); move_restore(&motion);
}

void check_magic_hold(void)
{
    character_state()->level = 2;
    magic_apply(82,-1,-1,10,1); poison_apply(-1,129,1);
    magic_state()->ailment[0].delay = 3;
}
void check_magic_loaded(void)
{
    check_expect(effects(9) && ailments() && magic_state()->ailment[0].delay == 3,
                 "a fresh process restores active magic and a latent poison without an early tick");
}
