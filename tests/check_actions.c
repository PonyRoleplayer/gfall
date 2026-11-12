#include "check_actions.h"
#include "check_util.h"
#include "game/dagger/play/actions.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/move.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/play/save.h"
#include "engine/scene/interact.h"
#include "engine/scene/walker.h"
#include "engine/core/mem.h"

static GameState g_before;
static Scene g_scene;
static Mover g_mover[3];
static Instance g_model[3];
static Usable g_use;
static ActionInfo g_action[3];
static void fixture(void);

static void touch_source(void)
{
    Bound bounds[3];
    Camera cam;
    float x, y, z, yaw;
    int magic;

    fixture();
    mem_zero(bounds,sizeof(bounds)); bounds[0].cy = bounds[0].ey = 20;
    bounds[0].ex = bounds[0].ez = 20;
    g_scene.models = g_model; g_scene.nmodels = 3; g_scene.bounds = bounds;
    g_use.touch = 1;
    g_action[0].kind = 28; g_action[0].axis = 5; g_action[0].trigger = 3; g_action[0].next = -1;
    actions_open(&g_scene,g_action,1);
    character_state()->magicka = magic = 30;
    interact_use(&g_scene,0);
    check_expect(character_state()->magicka == magic,"hand use cannot activate a collision-only dungeon action");
    interact_set_body(0); interact_take_use(); interact_teleport(&x,&y,&z,&yaw);
    camera_set(&cam,100,WALK_EYE,0,0,0); interact_frame(&g_scene,&cam,0,0);
    cam.x = 0; interact_frame(&g_scene,&cam,0,0);
    check_expect(character_state()->magicka == magic-5 && !interact_using_touch(),
                 "a touch use carries its source into the game handler and clears it afterwards");
    interact_frame(&g_scene,&cam,0,0);
    check_expect(character_state()->magicka == magic-5,"remaining on a trigger cannot restart its chain");
    interact_set_body(move_body()); actions_close();
}

static int words(int id, char *text, int capacity)
{
    df_copy(text,id == 5401 ? "Speak, friend, and enter." : id == 5657 ? "\"friend\",\"ally\"" : "",capacity);
    return 1;
}

static void fixture(void)
{
    int i;
    keep_clear();
    mem_zero(&g_scene,sizeof(g_scene)); mem_zero(g_mover,sizeof(g_mover));
    mem_zero(g_model,sizeof(g_model)); mem_zero(g_action,sizeof(g_action)); mem_zero(&g_use,sizeof(g_use));
    g_scene.movers = g_mover; g_scene.nmovers = 3;
    g_scene.usables = &g_use; g_scene.nusables = 1;
    g_use.model = 0; g_use.sprite = -1; g_use.kind = USE_MOVER; g_use.mover = 0;
    for (i = 0; i < 3; i++) {
        g_model[i].m[0] = g_model[i].m[4] = g_model[i].m[8] = 1;
        g_mover[i].inst = &g_model[i]; g_mover[i].model = i; g_mover[i].next = -1;
        g_mover[i].key.place = 50050; g_mover[i].key.thing = 0x456700u+i;
        g_action[i].key = g_mover[i].key; g_action[i].model = g_action[i].mover = i;
        g_action[i].next = i == 2 ? -1 : i+1;
    }
    g_mover[0].tx = 40; g_mover[1].door = 1; g_mover[1].lock = 12;
    g_action[0].kind = 1; g_action[0].trigger = 8; g_action[0].duration = 36;
    g_action[1].kind = 17;
    g_action[2].kind = 21; g_action[2].sound = g_action[2].axis = 3; g_action[2].duration = 36;
    mover_init(g_mover,3);
}

void check_actions(void)
{
    int event, health, damage, magicka;
    df_u32 random;
    PlayIn input;

    g_before = *game_state(); actions_close(); keep_clear();
    fixture();
    for (event = 1; event <= 9; event++) {
        check_expect(actions_accepts(8,event) == (event == 2 || event == 3 || event == 5 || event == 6),
                     "original MultiTrigger accepts only direct, collision, melee and arrow events");
        check_expect(actions_accepts(9,event) == (event == 2 || event == 3),
                     "original combined trigger accepts direct and collision events");
    }
    check_expect(actions_open(&g_scene,g_action,3),"open original-style dungeon action chain");
    health = character_state()->health; damage = 3*character_state()->level; random = game_state()->random;
    check_expect(!actions_trigger(&g_scene,0,4) && health == character_state()->health && random == game_state()->random,
                 "a failed-pick event cannot fire a strike/direct-only chain");
    check_expect(interact_use(&g_scene,0) && !g_mover[1].lock && character_state()->health == health-damage,
                 "hand activation moves a lever, unlocks its linked door and fires the damage trap");
    health = character_state()->health;
    check_expect(!actions_trigger(&g_scene,0,5) && character_state()->health == health,
                 "an active chain cannot be started again by another input");
    actions_step(0.25f); mover_step(&g_scene,0.25f);
    check_expect(character_state()->health == health && mover_progress(&g_mover[0]) > 0,
                 "a damage trap respects its one-second pulse while the lever moves");
    actions_capture(); keep_save(&g_scene,0,3,0,0,0,0,0);
    check_expect(state_capture_world() && save_write(6),"save active dungeon actions with mover progress and trap phase");
    actions_close(); fixture(); keep_clear();
    check_expect(save_read(6) && state_restore_world(),"reload active actions through the game save codec");
    keep_restore(&g_scene,0,3,0,0,0,0,0); actions_open(&g_scene,g_action,3);
    check_expect(mover_busy(&g_scene,0) && g_model[0].x > 4 && g_model[0].x < 6 && !g_mover[1].lock,
                 "loading restores the lever partway through its sweep and its linked unlocked door");
    health = character_state()->health;
    actions_step(0.5f);
    check_expect(character_state()->health == health,"loading does not reset a trap into an immediate extra hit");
    actions_step(0.3f);
    check_expect(character_state()->health == health-damage,"the saved trap resumes its next pulse at the right time");
    actions_close(); fixture(); g_action[0].kind = 28; g_action[0].axis = 5; g_action[0].next = -1;
    actions_open(&g_scene,g_action,1);
    character_state()->magicka = 50; god_set(1); magicka = character_state()->magicka;
    check_expect(actions_trigger(&g_scene,0,2) && character_state()->magicka == magicka,
                 "god mode protects magicka from a dungeon drain");
    god_set(0); actions_close();
    fixture(); g_action[0].kind = 12; g_action[0].sound = 1; g_action[0].next = 1;
    g_action[1].next = -1;
    set_word_variants(0); set_words(words);
    actions_open(&g_scene,g_action,2); actions_trigger(&g_scene,0,2);
    check_expect(possessions_state()->prompt == PROMPT_ANSWER && g_mover[1].lock == 12,
                 "a riddle pauses its chain before the linked door unlocks");
    mem_zero(&input,sizeof(input)); input.ch = 'x'; possessions_frame(&input);
    input.ch = 13; possessions_frame(&input);
    check_expect(!possessions_state()->page && g_mover[1].lock == 12,"a wrong answer cancels the riddle's remaining chain");
    actions_trigger(&g_scene,0,2);
    {
        const char *text;
        for (text = "FrIeNd"; *text; text++) { input.ch = *text; possessions_frame(&input); }
    }
    input.ch = 13; possessions_frame(&input);
    check_expect(!possessions_state()->page && !g_mover[1].lock,
                 "a case-insensitive accepted riddle answer resumes and unlocks the linked door");
    actions_close(); set_words(textrsc_read); set_word_variants(textrsc_variant);
    touch_source();
    /* World messages use the same parchment/page geometry as item notices,
     * but return directly to the world and own no item or inventory. */
    check_expect(possessions_message("A message from a dungeon action.",0) &&
                 possessions_state()->page == VIEW_MESSAGE,"a dungeon message opens a world parchment");
    mem_zero(&input,sizeof(input)); input.ch = 13; possessions_frame(&input);
    check_expect(!possessions_state()->page,"closing a world parchment returns directly to play");
    state_restore(&g_before); state_restore_world();
}
