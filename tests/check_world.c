#include "check_world.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/itemdefs.h"
#include "check_util.h"
#include "game/dagger/formats/rmb.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "../apps/gfall/seam.h"
#include "../apps/gfall/buildings/buildings.h"
#include "../apps/gfall/people/callbacks.h"
#include "../apps/gfall/people/interaction.h"
#include "../apps/gfall/people/people.h"
#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/save/save.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/travel/maps.h"
#include "game/dagger/play/travel/townmap.h"
#include "game/dagger/play/travel/travelmap.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/character/rest.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/items/ground.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/ui/canvas.h"
#include "game/dagger/play/magic/spellmaker.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/input/activate.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/lodging.h"
#include "game/dagger/play/people/entry.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/play/magic/spellshop.h"
#include "game/dagger/places/people.h"
#include "game/dagger/places/wres.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/tables/enemies.h"
#include "engine/scene/entity.h"
#include "engine/scene/ent.h"
#include "game/dagger/formats/interior.h"
#include "game/dagger/places/town.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/travel.h"
#include "engine/assets/vfs.h"
#include "engine/platform/fs.h"
#include "engine/scene/interact.h"
#include "engine/scene/spawn.h"
#include "engine/scene/stream.h"
#include "engine/scene/walker.h"
#include "engine/ui/cmd.h"
#include "engine/core/mem.h"
#include "engine/sound/ambience.h"
#include "engine/sound/music.h"
#include "game/dagger/tables/sounds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static GameState g_before;
static MotionState g_motion;
static Camera g_camera;
static int g_drawn[16384];
static int g_climate;
static df_u32 g_dropped;
static void talk_shot(const char *name)
{
    const df_u8 *pixels, *palette;
    char path[96];
    FILE *file;
    int i;

    seam_present();
    pixels = seam_screen(&palette);
    snprintf(path, sizeof(path), "build/gfall-%s.ppm", name);
    file = fopen(path, "wb");
    if (!file || !pixels || !palette) {
        if (file) { fclose(file); }
        return;
    }
    fprintf(file, "P6\n320 200\n255\n");
    for (i = 0; i < 64000; i++) { fwrite(palette + pixels[i] * 3, 1, 3, file); }
    fclose(file);
}

/* A click in the middle of a window button, through the controller's frame as the game gives it. */
static void click_at(int x, int y)
{
    PlayIn in;

    memset(&in, 0, sizeof(in));
    in.mouse_pressed = 1;
    in.on_picture = 1;
    in.cx = x;
    in.cy = y;
    possessions_frame(&in);
}

static int g_asked, g_number, g_shop_id;
static void shop_bought(int id) { g_shop_id = id; }
static void ask_done(int yes) { g_asked = yes; }
static void number_done(int amount) { g_number = amount; }
static void type_char(int ch)
{
    PlayIn in;

    memset(&in, 0, sizeof(in));
    in.ch = ch;
    possessions_frame(&in);
}

static void click_talk(const short *button)
{
    click_at((button[0] + button[2]) / 2, (button[1] + button[3]) / 2);
}

static void check_guild_rank_dialogues(void)
{
    GuildsState guilds = *guilds_state();
    Membership *member;
    int reputation = faction_rep(41);
    int blade = character_state()->skill[SKILL_SHORT_BLADE];
    int archery = character_state()->skill[SKILL_ARCHERY];
    int pages;

    guilds_clear();
    faction_add_reputation(41, 30 - reputation);
    character_state()->skill[SKILL_SHORT_BLADE] = character_state()->skill[SKILL_ARCHERY] = 60;
    gfall_hall_enter(851,41);
    click_at(160,55);
    check_expect(possessions_state()->prompt == PROMPT_ASK,"Fighters Guild asks whether to join");
    pages = (notice_lines(possessions_state()->offer, 0, 0, 0) + ASK_LINES - 1) / ASK_LINES;
    while (pages-- > 0) { click_at(128,125); }
    member = guild_find_faction(41);
    check_expect(member && member->rank == 0 && strstr(possessions_state()->notice,"Apprentice"),
                 "Fighters Guild welcome names the player's new rank");
    possessions_close();
    if (member) {
        member->rank_time = game_state()->minutes - RANK_MONTH - 1;
        gfall_hall_enter(851,41);
        check_expect(member->rank == 3 && strstr(possessions_state()->notice,"Protector"),
                     "Fighters Guild promotion names the updated rank");
        possessions_close();
    }
    *guilds_state() = guilds;
    faction_add_reputation(41, reputation - faction_rep(41));
    character_state()->skill[SKILL_SHORT_BLADE] = blade;
    character_state()->skill[SKILL_ARCHERY] = archery;
}

static void check_named_directions(const Camera *cam)
{
    static const int types[] = { BUILDING_LIBRARY, BUILDING_TEMPLE, BUILDING_TAVERN, BUILDING_ARMORER };
    static const struct { int block, record, seed; } armories[] = {
        {27, 0, 16879}, {27, 1, 31237}, {36, 0, 24490}, {44, 0, 2044}
    };
    const TalkWin *window;
    const TownBuilding *buildings;
    int region, index, block, record, count, group;
    float x, z;

    if (!travel_town_here(cam, &region, &index, &x, &z, &block, &record)) {
        check_expect(0, "people: cannot find the town to check its directions");
        return;
    }
    buildings = town_buildings(region, index, &count);
    for (group = 0; group < 4; group++) {
        const TownBuilding *shop = town_building(region, index, armories[group].block, armories[group].record);

        check_expect(shop && shop->type == BUILDING_ARMORER && shop->name_seed == armories[group].seed,
                     "people: the Daggerfall armory lost its MAPPITEM name seed");
    }
    {
        /* The houses for sale: each has a price of five times its largest model's radius. */
        int h, houses, priced;

        houses = priced = 0;
        for (h = 0; h < count; h++) {
            if (buildings[h].type == BUILDING_HOUSE_FOR_SALE) {
                int price;

                houses++;
                price = town_house_price(region, index, buildings[h].block);
                priced += price > 0;
                if (houses <= 3) { printf("house for sale: block %d price %d\n", buildings[h].block, price); }
            }
        }
        check_expect(houses == 0 || priced > 0, "houses: no house for sale of the town has a price");
    }
    window = talkwin_state();
    for (group = 0; group < (int)(sizeof(types) / sizeof(types[0])); group++) {
        int category, i, expected, named, last_distance, answers;
        df_u32 seed = game_state()->random;
        TwItem item;

        category = talk_category(types[group], 0);
        expected = 0;
        for (i = 0; i < count; i++) {
            if (buildings[i].type == types[group]) { expected++; }
        }
        for (i = 0; talkwin_item(i, &item) && item.value != category; i++) { }
        if (!window->categories || i >= window->count) {
            check_expect(0, "people: Daggerfall is missing a requested category in Where is");
            return;
        }
        click_at(20, 73 + 7 * i);
        click_talk(talkwin_button(TB_OKAY));
        check_expect(!window->categories && window->category == category && expected > 0 && window->count == expected + 1,
                     "people: Where is lists all buildings of the requested type in Daggerfall");
        named = 0;
        last_distance = 0;
        for (i = 1; talkwin_item(i, &item); i++) {
            const TalkTopic *topic;

            topic = &window->topic[item.value];
            if (item.text[0] && item.text[0] != '\'' && !strchr(item.text, '%') &&
                topic->category == category && topic->distance >= last_distance) { named++; }
            last_distance = topic->distance;
        }
        check_expect(named == expected, "people: signs in the list are complete and in distance order");
        if (types[group] == BUILDING_ARMORER) {
            char names[4][65];
            int j, distinct = 1;

            for (i = 0; i < 4 && talkwin_item(i + 1, &item); i++) {
                df_copy(names[i], item.text, sizeof(names[i]));
                for (j = 0; j < i; j++) { distinct &= strcmp(names[i], names[j]) != 0; }
            }
            check_expect(i == 4 && distinct, "people: the four Daggerfall armories still share a sign");
        }
        if (talkwin_item(1, &item)) {
            check_expect(window->question_count > 0 && !strcmp(talkwin_key_text(), item.text),
                         "people: the selected destination's full name did not reach the question");
            answers = window->answer_count;
            click_talk(talkwin_button(TB_OKAY));
            check_expect(window->answer_count > answers + 3 && window->asked[answers + 1] &&
                         window->answer[window->answer_count - 1][0],
                         "people: asking for the selected named destination did not log a question and answer");
        }
        click_at(20, 73);
        /* The additional armory query should leave the shared street fixture's rolls alone. */
        if (types[group] == BUILDING_ARMORER) { random_seed(seed); }
    }
}

static int route_climate(int x, int y) { return g_climate ? g_climate : (y >= x ? 223 : 228); }
void check_journey(void)
{
    TravelHooks hooks;
    JourneyPlan trip;
    mem_zero(&hooks,sizeof(hooks)); hooks.climate = route_climate; travel_install(&hooks);
    g_climate = 228;
    journey_plan(0,0,12,0,0,0,0,0,&trip);
    check_expect(trip.minutes == 1488 && trip.hours == 25 && trip.cost == 10,
                 "original woodland walking takes 124 minutes per pixel and five gold per inn night plus five");
    journey_plan(0,0,12,0,1,0,0,0,&trip);
    check_expect(trip.minutes == 744 && trip.hours == 13 && trip.cost == 5,
                 "reckless travel halves the estimate before calculating inn charges");
    journey_plan(0,0,12,0,0,1,0,0,&trip);
    check_expect(trip.minutes == 1740 && !trip.cost,"camping applies the original 300/256 multiplier to each route pixel");
    journey_plan(0,0,1,0,0,0,0,1,&trip);
    check_expect(trip.minutes == 62,"a horse halves the transport factor before the terrain multiplication");
    journey_plan(0,0,1,0,0,0,0,2,&trip);
    check_expect(trip.minutes == 92,"a cart retains the original intermediate integer truncation");
    g_climate = 223;
    journey_plan(0,0,24,0,0,0,1,0,&trip);
    check_expect(trip.minutes == 1224 && trip.sea_pixels == 24 && trip.cost == 55,
                 "ship time is 51 minutes per sea pixel and the fare counts sea pixels, not elapsed hours");
    journey_plan(0,0,24,0,0,1,0,0,&trip);
    check_expect(trip.minutes == 7152 && !trip.cost,"without a ship, sea travel uses 255 minutes before the camping multiplier");
    g_climate = 0;
    journey_plan(0,0,2,2,0,0,0,0,&trip);
    check_expect(trip.minutes == 248 && !trip.sea_pixels,"the original strict-greater route step preserves its diagonal endpoint quirk");
    mem_zero(&hooks,sizeof(hooks)); travel_install(&hooks);
}
static void map_emit(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb)
{
    (void)ctx; (void)corner; (void)n; (void)rgb;
}
static void recall(const char *answer)
{
    PlayIn input;
    magic_apply(94,-1,-1,10,1);
    mem_zero(&input,sizeof(input));
    for (; *answer; answer++) { input.ch = *answer; possessions_frame(&input); }
    input.ch = 13; possessions_frame(&input);
}
static int change(void)
{
    float x, y, z;
    if (!seam_open_place(0,1,&x,&y,&z)) { fprintf(stderr,"world change: %s\n",seam_error()); return 0; }
    if (!seam_land(&g_camera,0,1)) { fprintf(stderr,"world landing: %s\n",seam_error()); return 0; }
    seam_creatures(&g_camera); return 1;
}
static const Usable *passage(int what)
{
    Scene *scene;
    int i;
    scene = &seam_level()->scene;
    for (i = 0; i < scene->nusables; i++) {
        if (scene->usables[i].kind == USE_PASSAGE && scene->usables[i].what == what && scene->usables[i].reach > 0) { return &scene->usables[i]; }
    }
    return 0;
}
static void map_key(int key)
{
    PlayIn input;
    mem_zero(&input,sizeof(input));
    input.ch = key;
    input.key = key >= 'a' && key <= 'z' ? key - 'a' + 'A' : key;
    maps_frame(&input);
}
static void map_click(int x, int y)
{
    PlayIn input;
    mem_zero(&input,sizeof(input)); input.on_picture = 1; input.cx = x; input.cy = y;
    input.mouse_pressed = input.mouse_held = 1; maps_frame(&input);
}
static void map_step(float dt)
{
    PlayIn input;
    mem_zero(&input,sizeof(input)); input.dt = dt; maps_frame(&input);
}
static void map_picture(const char *name)
{
    const df_u8 *pixels, *palette;
    FILE *file;
    int i;
    seam_present(); pixels = seam_screen(&palette);
    file = fopen(name,"wb");
    if (file && pixels) { fprintf(file,"P6\n320 200\n255\n"); for (i = 0; i < 64000; i++) { fwrite(palette+pixels[i]*3,1,3,file); } }
    if (file) { fclose(file); }
}
static void maps_and_trip(void)
{
    static df_u8 before_find[CANVAS_BYTES];
    const df_u8 *mask, *pixels;
    const char *name;
    df_u32 before, size;
    int i;

    check_expect(maps_open(1),"the classic travel map opens outside with GFALL's discovery and money callbacks");
    mask = vfs_read("TRAV0I01.IMG", &size);
    if (mask && size == CANVAS_BYTES) {
        for (i = 0; i < 172 * CANVAS_W && mask[i] != 128 + 17; i++) { }
        if (i < 172 * CANVAS_W) { map_click(i % CANVAS_W, i / CANVAS_W); }
    }
    check_expect(travelmap_region() == 17,"Find fixture opens the Daggerfall province map");
    map_click(10,179);
    for (name = "dagger"; *name; name++) { map_key(*name); }
    map_picture("build/gfall-travel-find.ppm");
    map_key(27);
    check_expect(travelmap_region() == 17,"Escape closes Find back to the province it covered");
    map_click(297,181);
    travelmap_compose();
    mem_copy(before_find,travelmap_pixels(),CANVAS_BYTES);
    map_click(10,179);
    travelmap_compose();
    pixels = travelmap_pixels();
    check_expect(travelmap_region() == -1 && !memcmp(before_find,pixels,78 * CANVAS_W) &&
                 !memcmp(before_find + 122 * CANVAS_W,pixels + 122 * CANVAS_W,(CANVAS_H - 122) * CANVAS_W) &&
                 memcmp(before_find + 88 * CANVAS_W + 16,pixels + 88 * CANVAS_W + 16,288),
                 "Find draws parchment in the centre and keeps the world map visible when opened after leaving a province");
    mem_copy(before_find,pixels,CANVAS_BYTES);
    map_key('d'); travelmap_compose();
    check_expect(memcmp(before_find + 93 * CANVAS_W,travelmap_pixels() + 93 * CANVAS_W,8 * CANVAS_W),
                 "typing a letter changes the parchment's input line");
    map_key(8); travelmap_compose();
    check_expect(!memcmp(before_find,travelmap_pixels(),CANVAS_BYTES),"Backspace returns Find to its empty prompt");
    for (name = "daggerfall"; *name; name++) { map_key(*name); }
    map_key(13); map_step(2); map_picture("build/gfall-travel-question.ppm");
    map_click(110,116); map_click(200,86); map_click(200,54);
    map_picture("build/gfall-travel-options.ppm");
    ailments_cure(-1,0); ailments_cure(-1,1);
    character_state()->health = character_state()->max_health; before = game_state()->minutes;
    {
        AilmentInfo *a;
        a = &magic_state()->ailment[0]; mem_zero(a,sizeof(*a));
        a->active = 1; a->id = 0; a->target = magic_key(-1); a->delay = 10;
        map_click(240,102); map_step(2);
        check_expect(maps_up() && !play_travel_asked() && game_state()->minutes == before,
                     "a latent ordinary disease asks for confirmation before journey time passes");
        map_picture("build/gfall-travel-warning.ppm"); map_click(200,116);
        check_expect(game_state()->minutes == before,"declining the illness warning preserves time and travel options");
        ailments_cure(-1,0);
    }
    map_click(240,102);
    for (i = 0; maps_up() && i < 100; i++) { map_step(1); }
    check_expect(!maps_up() && play_travel_asked() && game_state()->minutes > before,
                 "Find, confirmation, options and Begin execute an original-rules journey and advance game time");
    if (change()) {
        double wx, wz, x0, z0;
        float mx = 0, mz = 0;
        int region, index;

        stream_world(g_camera.x, g_camera.z, &wx, &wz);
        check_expect(wild_location_at(wx, wz, &region, &index, &x0, &z0) && region == 17 &&
                     town_nearest_street_marker(region, index, (float)(wx - x0), (float)(wz - z0), &mx, &mz) &&
                     check_near((float)(wx - x0), mx, 0.1f) && check_near((float)(wz - z0), mz, 0.1f),
                     "a travel-map journey to Daggerfall lands on a city gate marker");
    } else { check_expect(0, "a travel-map journey reaches the chosen streamed town"); }
}

static void check_south_gate_arrival(void)
{
    double wx, wz, x0, z0, x1, z1;
    float mx, mz;
    int blocks, climate, type, ox, oz;
    const PlaceWhere where = world_where()->where;

    stream_world(g_camera.x, g_camera.z, &wx, &wz);
    if (!wild_town(wx, wz, &x0, &z0, &x1, &z1, &blocks, &climate, &type) ||
        !town_release_marker(where.region, where.index, 0, &mx, &mz)) {
        check_expect(0, "Daggerfall has a south gate marker to approach");
        return;
    }
    stream_origin(&ox, &oz);
    move_place((float)((x0 + x1) * 0.5 - (double)ox * STREAM_CHUNK), g_camera.y,
               (float)(z0 - (double)(oz + 2) * STREAM_CHUNK), 0);
    if (!travel_go(where.region, where.index) || !change()) {
        check_expect(0, "travel to Daggerfall from the south");
        return;
    }
    stream_world(g_camera.x, g_camera.z, &wx, &wz);
    check_expect(check_near((float)(wx - x0), mx, 0.1f) && check_near((float)(wz - z0), mz, 0.1f) && !move_yaw(),
                 "a southern departure selects Daggerfall's south gate and faces into the city");
}
static void walk_across_origin(void)
{
    MissileInfo *missile;
    double x, z, after_x, after_z;
    int ox, oz, nx, nz;
    stream_world(g_camera.x,g_camera.z,&x,&z); stream_origin(&ox,&oz);
    recall("anchor");
    move_place(g_camera.x+32*STREAM_CHUNK,g_camera.y,g_camera.z,0); move_camera(&g_camera);
    missile = &magic_state()->missile[0]; mem_zero(missile,sizeof(*missile));
    missile->live = 1; missile->x = g_camera.x; missile->z = g_camera.z;
    seam_world_step(&g_camera,0.05f); stream_origin(&nx,&nz);
    stream_world(g_camera.x,g_camera.z,&after_x,&after_z);
    check_expect(nx != ox && check_near((float)(after_x-x),32*STREAM_CHUNK,0.1f) &&
                 check_near((float)(after_z-z),0,0.1f) && check_near(move_body()->feet[0],g_camera.x,0.1f) &&
                 check_near(missile->x,g_camera.x,0.1f) && check_near(game_state()->x,g_camera.x,0.1f),
                 "a real stream-origin move keeps the camera, walking body, saved pose and spell missile in one frame");
    mem_zero(missile,sizeof(*missile));
    recall("recall");
    check_expect(play_travel_asked() && change(),"Recall reopens an outdoor anchor saved under an earlier stream origin");
    stream_world(g_camera.x,g_camera.z,&after_x,&after_z);
    check_expect(check_near((float)(after_x-x),0,0.1f) && check_near((float)(after_z-z),0,0.1f),
                 "an outdoor Recall anchor names a world position, not the current origin's local coordinates");
}
static void drop_outside(void)
{
    Camera aim;
    df_u32 container, item;
    float x, y, z;
    container = ground_container(g_camera.x,move_body()->feet[1],g_camera.z+8);
    item = item_create(container,3,0,1); g_dropped = container;
    check_expect(item && ground_position(object_at(container),&x,&y,&z),"an outdoor drop records its map pixel and a small position within it");
    seam_creatures(&g_camera);
    aim = g_camera; aim.yaw = 0; aim.pitch = df_atan2(y+12-g_camera.y,8);
    check_expect(activate(&seam_level()->scene,&aim,MODE_GRAB,0,0,1,160,100) &&
                 possessions_state()->page == VIEW_INVENTORY && possessions_state()->remote == container,
                 "an outdoor pile is drawn through spawn and opens the actual item container when pointed at");
    possessions_close();
}
/* The street (the people pool as the original's pedestrians): someone in sight is a witness; a swing's hit kills the person in front, a murder seen, and the watch comes; the court sends it away. */
static void street_checks(void)
{
    static float citizen_position[PEOPLE_MAX][3];
    static int citizen[PEOPLE_MAX];
    Scene *sc;
    StrikeInfo strike;
    Camera swing, view;
    Entity *victim, *fallen;
    int pi, converted, placed;

    cmd_run("goto Daggerfall"); change();
    state_pass_minutes((df_u32)((12*60 + 1440 - (int)(game_state()->minutes % 1440u)) % 1440));
    sc = &seam_level()->scene;
    people_fill(&g_camera);
    check_expect(guards_witnesses() != 0, "street: nobody in a busy street saw the player");
    move_camera(&view);
    victim = 0;
    fallen = 0;
    placed = 0;
    for (pi = 0; pi < PEOPLE_MAX; pi++) {
        Entity *cand;

        cand = people_at(pi);
        citizen[pi] = people_is_citizen(cand);
        if (!citizen[pi]) { continue; }
        if (!victim) {
            victim = cand;
        } else if (placed < 8) {
            /* Make the turn eligible regardless of the streamed population's initial layout. */
            float distance = 600.0f + 64.0f * placed++;
            cand->x = view.x - distance * df_sin(view.yaw);
            cand->z = view.z - distance * df_cos(view.yaw);
        }
        citizen_position[pi][0] = cand->x;
        citizen_position[pi][1] = cand->y;
        citizen_position[pi][2] = cand->z;
    }
    check_expect(victim != 0, "street: no citizen to strike");
    if (!victim) { return; }
    camera_set(&swing, victim->x, victim->y + entity_height(victim) * 0.5f, victim->z - 40, 0, 0);
    strike.item = 0; strike.hand = 0; strike.bow = 0; strike.hit = 1; strike.damage = 5;
    law_state()->crime = 0;
    seed_stream_begin(0);
    dungeon_strike(sc, &swing, &strike);
    seed_stream_end();
    check_expect(!people_is_citizen(victim), "street: a swing did not kill the person in front");
    check_expect(law_state()->crime == CRIME_MURDER, "street: a killing seen was not noted as murder");
    check_expect(dungeon_guards() > 0 && guards_present(), "street: the watch did not come to a murder");
    converted = 0;
    for (pi = 0; pi < PEOPLE_MAX; pi++) {
        Entity *guard = people_at(pi);
        int file, record, foe = dungeon_index(guard);

        if (!citizen[pi] || !people_hostile(guard) || dungeon_kind(foe) != ENEMY_CITY_WATCH) { continue; }
        converted++;
        check_expect(people_is_guard(guard) && guard->x == citizen_position[pi][0] &&
                     guard->y == citizen_position[pi][1] && guard->z == citizen_position[pi][2],
                     "street: a pedestrian should become a guard in place");
        check_expect(wres_picture(guard->pose[ENTITY_WALK].slot[0], &file, &record) &&
                     file == ENEMY_ARCHIVE_WATCH && record == 0 &&
                     wres_picture(guard->pose[ENTITY_ATTACK].slot[0], &file, &record) &&
                     file == ENEMY_ARCHIVE_WATCH && record == 5,
                     "street: a converted guard should walk and attack in City Watch sprites");
    }
    check_expect(converted > 0, "street: the summons did not convert any civilians");
    /* A guard struck down leaves a pile of its gear to loot. */
    {
        int g;

        for (g = 0; g < dungeon_count(); g++) {
            if (dungeon_kind(g) == ENEMY_CITY_WATCH) { fallen = dungeon_entity(g); break; }
        }
        check_expect(fallen != 0, "street: no guard of the watch to strike");
        if (fallen) {
            int file, record;
            const EntityPose *pose;

            dungeon_enchant_damage(g, 9999, TARGET_PLAYER);
            pose = entity_pose(fallen);
            check_expect(people_is_corpse(fallen) && pose == &fallen->pose[ENTITY_DEAD] &&
                         wres_picture(pose->slot[0], &file, &record) && file == 380 && record == 1,
                         "street: a killed guard should show Classic's human corpse");
            check_expect(!fallen->key.place && !fallen->key.thing, "street: the transient corpse should stay unkeyed");
            check_expect(inventory_rows(ground_container(fallen->x,fallen->y,fallen->z),-1,0,0) > 0,
                         "street: a dead guard left no pile of gear");
        }
    }
    if (fallen) {
        Camera far = g_camera;

        far.x += PEOPLE_RECYCLE * 3.0f;
        people_update(&far, 0.0f);
        people_fill(&g_camera);
        check_expect(!fallen->live && dungeon_index(fallen) >= 0,
                     "street: pedestrians must wait for combat to release a guard's slot");
        gfall_street_tick(0.0f);
        check_expect(dungeon_index(fallen) < 0, "street: combat should release a recycled guard");
        people_fill(&g_camera);
        guards_summon(1);
    }
    /* Pursued by the watch, into a building, out, and into another: the creatures of the street must not outlive the street. */
    {
        Scene *entry_scene;
        int k, entries, entered;

        entries = 0;
        for (entries = 0; entries < 2; entries++) {
            entry_scene = &seam_level()->scene;
            entered = 0;
            for (k = 0; k < entry_scene->nusables && !entered; k++) {
                const Usable *candidate;

                candidate = &entry_scene->usables[k];
                if (candidate->kind == USE_PASSAGE && candidate->what == 1 && candidate->reach > 0 && travel_passage(candidate) && play_travel_asked()) { entered = 1; }
            }
            check_expect(entered && change(), "street: the watch's quarry could not enter a building");
            dungeon_step(&seam_level()->scene, &g_camera, 0.05f);
            {
                const Usable *leave;

                leave = passage(2);
                check_expect(leave && travel_passage(leave) && change(), "street: the watch's quarry could not leave a building");
            }
            dungeon_step(&seam_level()->scene, &g_camera, 0.05f);
        }
    }
    guards_dismiss();
    check_expect(!dungeon_guards(), "street: the court did not send the watch away");
    law_state()->crime = 0;
}

/* Low legal standing and banishment both call the watch through the witness timer. */
static void street_reputation_check(void)
{
    Camera view;
    Court court = {0};
    CourtStep sentence;
    Entity *witness = 0, *recruit = 0;
    int i, region, previous, punishment, foe, file, record;
    df_u32 before;

    people_fill(&g_camera);
    move_camera(&view);
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e = people_at(i);

        if (people_is_citizen(e)) {
            if (!witness) { witness = e; continue; }
            if (!recruit) { recruit = e; continue; }
        }
        if (e->live) { e->x += PEOPLE_RECYCLE * 3.0f; }
    }
    check_expect(witness && recruit, "street reputation: two civilians are needed for the summons");
    if (!witness || !recruit) { return; }
    /* One civilian can see the player from in front. Another is eligible for conversion behind. */
    witness->x = view.x + 0.5f * df_sin(view.yaw);
    witness->y = view.y - 40.0f;
    witness->z = view.z + 0.5f * df_cos(view.yaw);
    recruit->x = view.x - 700.0f * df_sin(view.yaw);
    recruit->z = view.z - 700.0f * df_cos(view.yaw);
    check_expect(guards_witnesses() == 1, "street reputation: only a civilian should witness the player");
    region = world_where()->where.region;
    previous = region_legal(region);
    punishment = law_state()->punishment[region];
    law_state()->punishment[region] = 0;
    region_add_legal(region, LAW_BAD_STANDING - 1 - previous);
    guards_reset();
    seed_stream_begin(0); /* First Watcom roll is 0, so the summons and then the conversion both hit. */
    law_time_pass(region);
    seed_stream_end();
    check_expect(law_state()->crime == CRIME_CONSPIRACY && guards_timer() > 0.0f && !dungeon_guards(),
                 "street reputation: the witness should set the timer without starting combat");
    region_add_legal(region, -region_legal(region));
    guards_dismiss();
    law_state()->crime = 0;
    /* Begin at the guilty plea for a banishment, with good standing to
     * isolate the punishment flag from the separate reputation roll. */
    court.open = 1;
    court.region = region;
    court_accept(&court, &sentence);
    check_expect(sentence.release == RELEASE_STREET && (law_state()->punishment[region] & PUNISH_BANISHED) &&
                 guards_timer() < 0.0f && !dungeon_guards(),
                 "street banishment: the sentence should release the player and keep the region flag");
    before = game_state()->minutes;
    seed_stream_begin(1); /* First two Watcom rolls miss the banishment check. */
    state_advance_ms(5000);
    seed_stream_end();
    check_expect(game_state()->minutes > before && guards_timer() < 0.0f && !dungeon_guards(),
                 "street banishment: a missed roll should leave the player free on the street");
    seed_stream_begin(0);
    state_advance_ms(5000);
    seed_stream_end();
    check_expect(law_state()->crime == CRIME_CONSPIRACY && guards_timer() > 0.0f && !dungeon_guards(),
                 "street banishment: the live clock should schedule the watch when a civilian recognizes the player");
    seed_stream_begin(0);
    guards_tick(GUARDS_DELAY_MAX + 1.0f);
    seed_stream_end();
    foe = dungeon_index(recruit);
    check_expect(people_is_guard(recruit) && people_hostile(recruit) && dungeon_kind(foe) == ENEMY_CITY_WATCH &&
                 wres_picture(entity_pose(recruit)->slot[0], &file, &record) && file == ENEMY_ARCHIVE_WATCH,
                 "street reputation: a recruited civilian should already wear Watch art when combat starts");
    check_expect(people_is_citizen(witness) && !people_hostile(witness) && dungeon_index(witness) < 0,
                 "street reputation: a civilian witness should stay out of combat");
    guards_dismiss();
    guards_reset();
    law_state()->crime = 0;
    law_state()->punishment[region] = punishment;
    region_add_legal(region, previous - region_legal(region));
}

static void street_release_check(void)
{
    const PlaceWhere place = world_where()->where;
    int flags = law_state()->punishment[place.region];
    const Scene *scene;
    Entity *guard;
    SpawnSpot spot = {0};
    double wx, wz, x0, z0, x1, z1;
    float mx = 0, mz = 0, gx, gy, gz, dx = 0, dy, dz = 0, distance;
    int blocks, climate, type, i, slot, foe;

    check_expect(town_release_marker(place.region, place.index, 0, &mx, &mz) &&
                 mx == 2 * STREAM_CHUNK + 1653 && mz == STREAM_CHUNK - 1190,
                 "court release: Daggerfall should have an editor marker at its south gate");
    seed_stream_begin(0);
    gfall_court_release(RELEASE_STREET);
    seed_stream_end();
    if (!play_travel_asked() || !change()) {
        check_expect(0, "court release: an outdoor banishment should move the player");
        return;
    }
    stream_world(g_camera.x, g_camera.z, &wx, &wz);
    check_expect(world_where()->where.region == place.region && world_where()->where.index == place.index &&
                 wild_town(wx, wz, &x0, &z0, &x1, &z1, &blocks, &climate, &type) &&
                 df_absf((float)(wx - x0) - mx) < 0.1f && df_absf((float)(wz - z0) - mz) < 0.1f,
                 "court release: the player should land at the selected gate in the same city");
    scene = &seam_level()->scene;
    spot.x = g_camera.x; spot.y = move_body()->feet[1]; spot.z = g_camera.z;
    spot.inner = 140; spot.outer = 220;
    spot.radius = WALK_RADIUS; spot.height = WALK_HEIGHT; spot.climb = WALK_STEP; spot.flags = SPAWN_DRY;
    for (i = 0; i < 16; i++) {
        UseHit hit;

        spot.seed = (unsigned)i;
        if (!spawn_find(scene, 0, &spot, &gx, &gy, &gz)) { continue; }
        dx = g_camera.x - gx; dy = g_camera.y - (gy + 40); dz = g_camera.z - gz;
        distance = df_sqrt(dx * dx + dy * dy + dz * dz);
        interact_cast(scene, gx, gy + 40, gz, dx / distance, dy / distance, dz / distance, distance, &hit);
        if (hit.wall >= distance) { break; }
    }
    slot = people_spawn_guard(&g_camera);
    check_expect(slot >= 0 && i < 16, "court release: a patrol needs a clear street nearby");
    if (slot < 0 || i == 16) { return; }
    guard = people_at(slot);
    guard->x = gx; guard->y = gy; guard->z = gz; guard->yaw = df_atan2(dx, dz);
    law_state()->punishment[place.region] = 0;
    gfall_street_tick(0.5f);
    check_expect(!people_hostile(guard) && dungeon_index(guard) < 0,
                 "patrol: seeing an ordinary passerby should not start a pursuit");
    law_state()->punishment[place.region] = PUNISH_BANISHED;
    gfall_street_tick(0.5f);
    foe = dungeon_index(guard);
    check_expect(people_hostile(guard) && dungeon_kind(foe) == ENEMY_CITY_WATCH &&
                 law_state()->crime == CRIME_CONSPIRACY,
                 "patrol: seeing a banished player should start an arrest");
    for (i = 0; i < 40 && guard->act != ENTITY_ATTACK; i++) { dungeon_step(scene, &g_camera, 0.05f); }
    check_expect(dungeon_target(foe) == TARGET_PLAYER && guard->act == ENTITY_ATTACK,
                 "patrol: the guard should pursue and swing at the banished player");
    law_state()->punishment[place.region] = 0;
    law_note(CRIME_ASSAULT);
    slot = people_spawn_guard(&g_camera);
    check_expect(slot >= 0, "patrol: spawn a second patrol during an arrest");
    if (slot >= 0) {
        guard = people_at(slot);
        guard->x = gx; guard->y = gy; guard->z = gz;
        gfall_street_tick(0.5f);
        check_expect(people_hostile(guard) && law_state()->crime == CRIME_ASSAULT,
                     "patrol: a new guard should join the pursuit under the existing charge");
    }
    guards_dismiss();
    law_state()->crime = 0;
    law_state()->punishment[place.region] = flags;
}

static void status_check(void)
{
    static char said[300];

    check_expect(status_intro(said,sizeof(said)) > 0 && !strchr(said,'%') && strstr(said,"It is ") && strstr(said," on ") && strstr(said,"you are "),
                 "status: the Status key's first box has no date, time or law's opinion, or a macro left unsaid");
}

static int note_on_townmap(const TownBuilding *building, int region, int index, const char *name)
{
    static df_u8 expected[CANVAS_BYTES];
    const df_u8 *pixels;
    Composer canvas;
    int i, ink = 0;

    if (!townmap_open(region, index, building->x, building->z, 0)) { return 0; }
    townmap_compose();
    pixels = townmap_pixels();
    /* Daggerfall at half zoom, with the marked cell at the centre. Compare
     * the actual glyphs at the mark, without hovering over the building. */
    if (townmap_scale() != 2 || pixels[81 * CANVAS_W + 160] != CANVAS_INK || !composer_open(&canvas)) {
        townmap_close();
        return 0;
    }
    mem_zero(expected, sizeof(expected));
    smallfont_draw(&canvas.font, expected, CANVAS_W, CANVAS_H, 166, 85 - canvas.font.height / 2, CANVAS_INK, -1, name);
    for (i = 0; i < CANVAS_BYTES; i++) {
        if (expected[i] == CANVAS_INK) {
            if (pixels[i] != CANVAS_INK) { ink = 0; break; }
            ink++;
        }
    }
    composer_close(&canvas);
    townmap_close();
    return ink > 0;
}

static void ask_for_quest_building(const TownBuilding *building, int region, int index, const char *name)
{
    const TalkWin *window = talkwin_state();
    const Scene *scene = &seam_level()->scene;
    int category, i, asked = 0;
    unsigned id = gfall_building_id(building);
    char noted[65];
    TwItem item;

    people_fill(&g_camera);
    for (i = 0; i < scene->nentities && !window->open; i++) {
        const Entity *person = &scene->entities[i];
        Camera aim;

        if (!people_is_citizen(person)) { continue; }
        camera_set(&aim, person->x, person->y + entity_height(person) * 0.5f, person->z - 60, 0, 0);
        activate(scene, &aim, MODE_TALK, 0, 0, 1, 160, 100);
    }
    category = TALK_CATEGORY_QUEST;
    for (i = 0; window->open && talkwin_item(i, &item) && item.value != category; i++) { }
    if (window->open && window->categories && i < window->count) {
        click_at(20, 73 + 7 * i);
        click_talk(talkwin_button(TB_OKAY));
        for (i = 1; talkwin_item(i, &item); i++) {
            if (window->topic[item.value].building != id) { continue; }
            if (i >= window->top + TW_LIST_ROWS) {
                int steps = i - window->top - TW_LIST_ROWS + 1;

                while (steps-- > 0) { talkwin_key(TW_KEY_DOWN); }
            }
            click_at(20, 73 + 7 * (i - window->top));
            seed_stream_begin(0);
            for (asked = 0; asked < 20; asked++) {
                click_talk(talkwin_button(TB_OKAY));
                if (gfall_building_map_name(building, region, index, noted, sizeof(noted)) == TOWNMAP_MARKED) { break; }
            }
            seed_stream_end();
            break;
        }
    }
    possessions_close();
    check_expect(gfall_building_map_name(building, region, index, noted, sizeof(noted)) == TOWNMAP_MARKED &&
                 !strcmp(noted, name) && note_on_townmap(building, region, index, name),
                 "spider quest: NPC directions should draw the correct name at the address before visiting");
}

static void check_spider_quest(void)
{
    const QuestState *q;
    const TownBuilding *building;
    const Usable *door = 0;
    const Scene *scene;
    char name[80];
    float x, y, z;
    int slot, region, index, block, record, here_block, here_record, i, kind, found = 0;
    unsigned conditions;
    df_u32 thing;

    quest_set_questor(771, 851, "Theodore Goering", "Daggerfall");
    slot = quest_begin("M0C00Y12", 41);
    check_expect(slot >= 0 && possessions_state()->prompt == PROMPT_ASK &&
                 strstr(possessions_state()->offer, "Are you interested?"),
                 "spider quest: the entire original offer should reach the parchment");
    if (slot < 0) { possessions_close(); return; }
    q = quest_at(slot);
    region = q->place[0].region; index = q->place[0].index;
    block = q->place[0].block; record = q->place[0].record;
    building = town_building(region, index, block, record);
    check_expect(building && building->sites[SITE_QUEST],
                 "spider quest: bind a specific building with a Classic quest marker");
    if (!building) { possessions_close(); quest_end(slot); return; }
    gfall_building_name(building, region, index, name, sizeof(name));
    i = (notice_lines(possessions_state()->offer, 0, 0, 0) + ASK_LINES - 1) / ASK_LINES;
    while (i-- > 0) { click_at(128,125); }
    check_expect(name[0] && strcmp(name, "Daggerfall") && strstr(possessions_state()->notice, name),
                 "spider quest: the acceptance should name the building");
    possessions_close();
    quest_run();
    check_expect(q->foe[0].placed && !q->foe[0].made,
                 "spider quest: the spider should wait for its building");
    if (!travel_town_here(&g_camera, &region, &index, &x, &z, &here_block, &here_record)) { quest_end(slot); return; }
    move_place(g_camera.x + building->x - x, g_camera.y, g_camera.z + building->z - z, 0);
    move_camera(&g_camera);
    for (i = 0; i < 24 && !door; i++) {
        int j;

        seam_world_step(&g_camera,0.05f);
        scene = &seam_level()->scene;
        for (j = 0; j < scene->nusables; j++) {
            const Usable *u = &scene->usables[j];

            if (u->kind == USE_PASSAGE && u->what == 1 && u->reach > 0 && u->tag == ((block << 8) | record)) {
                door = u;
                break;
            }
        }
    }
    check_expect(door != 0, "spider quest: the chosen building should have an entrance");
    if (door) {
        ask_for_quest_building(building, region, index, name);
        conditions = character_state()->conditions;
        character_state()->conditions |= 0x40; /* Open spell, in case this door is locked. */
        travel_passage(door);
        character_state()->conditions = conditions;
        if (play_travel_asked() && change()) {
            kind = q->qbn.foe[0].type;
            thing = QUEST_FOE_THING(q->foe[0].number, 0);
            quest_run();
            for (i = 0; i < dungeon_count(); i++) {
                if (dungeon_kind(i) == kind && dungeon_entity(i)->key.thing == thing) { found++; }
            }
            check_expect(found == 1 && q->foe[0].made == 1 && q->foe[0].live == 1,
                         "spider quest: entering the named building should spawn one live spider");
            state_capture_motion();
            if (!seam_capture_world() || !save_write(7) || !save_read(7)) {
                fprintf(stderr,"spider quest save: %s %s\n",save_error(),state_error());
                check_expect(0, "spider quest: save and read the destination");
            } else if (!seam_open_place(1,0,&x,&y,&z) || !seam_land(&g_camera,1,0)) {
                fprintf(stderr,"spider quest reopen: %s\n",seam_error());
                check_expect(0, "spider quest: reopen the saved destination");
            }
            fs_delete("gfall\\saves\\S0007.SAV");
            found = 0;
            for (i = 0; i < dungeon_count(); i++) {
                if (dungeon_kind(i) == kind && dungeon_entity(i)->key.thing == thing) { found++; }
            }
            q = quest_at(slot);
            check_expect(q && q->place[0].block == block && q->place[0].record == record && found == 1,
                         "spider quest: the save should restore the same building and spider");
            building = town_building(region, index, block, record);
            check_expect(building && note_on_townmap(building, region, index, name),
                         "spider quest: the directions note should survive saving and loading");
            if (q && found == 1) {
                KeepKey key;
                const Keep *row;

                key.place = object_at(WORLD_ROOT)->place_key;
                key.thing = thing;
                encounter_capture();
                row = keep_find(key);
                check_expect(row && row->made, "spider quest: the live foe has a world row to retire");
                if (row) {
                    Keep orphan = *row;
                    df_u32 loot = loot_container(key.place, key.thing, row->x, row->y, row->z, -1);
                    df_u32 item = item_create(loot, ITEM_GROUP_WEAPON, WEAPON_ARROW, 0);
                    int count = spawn_count();

                    orphan.key.place ^= 0x01000000u;
                    check_expect(item && keep_put(&orphan), "spider quest: stage loot and an unloaded copy");
                    quest_end(slot);
                    check_expect(spawn_count() == count - 1 && !keep_find(key) && !keep_find(orphan.key) &&
                                 !object_at(loot) && !object_at(item),
                                 "quest end retires its live foe, loot and world rows in both buildings");

                    /* Recreate the old save format's leftover corpse in the loaded building. */
                    orphan.key = key;
                    orphan.dead = 1;
                    check_expect(keep_put(&orphan) && save_write(7) && save_read(7) &&
                                 seam_open_place(1,0,&x,&y,&z) && seam_land(&g_camera,1,0) && !keep_find(key),
                                 "older saves discard an ended quest's corpse before reopening its building");
                    fs_delete("gfall\\saves\\S0007.SAV");
                }
            }
            door = passage(2);
            check_expect(door && travel_passage(door) && change(), "spider quest: leave the building");
        } else { check_expect(0, "spider quest: enter the named building"); }
    }
    quest_end(slot);
}

static void check_offer_pages(void)
{
    char offer[512] = "";
    char last[1][96];
    PlayIn in = {0};
    int i;

    for (i = 0; i < ASK_LINES; i++) { strcat(offer, "A line of the offer.\n"); }
    strcat(offer, "Are you interested?");
    g_asked = -1;
    check_expect(possessions_ask(offer, ask_done), "quest prompt: open a two-page offer");
    click_at(128,125);
    check_expect(g_asked == -1 && possessions_state()->offer_page == 1 &&
                 notice_lines(possessions_state()->offer, ASK_LINES, last, 1) == ASK_LINES + 1 &&
                 !strcmp(last[0], "Are you interested?"),
                 "quest prompt: Next should show the full second page without answering");
    in.key = KEY_PRIOR;
    possessions_frame(&in);
    check_expect(g_asked == -1 && possessions_state()->offer_page == 0,
                 "quest prompt: Page Up should return to the first page");
    type_char(13);
    click_at(192,125);
    check_expect(g_asked == 0 && !possessions_state()->page,
                 "quest prompt: No on the last page should answer once");
}

static void check_tavern_patron(const Scene *scene)
{
    EntList lists[ENT_LISTS];
    int n, j, i, side;

    ent_place(scene);
    n = ent_lists(scene, lists, ENT_LISTS);
    for (j = 0; j < n; j++) {
        for (i = 0; i < lists[j].n; i++) {
            const Entity *e = &lists[j].first[i];

            if (!e->live || e->use.kind != USE_TALK || faction_index(e->use.what) >= 0) { continue; }
            for (side = 0; side < 4; side++) {
                float yaw = side * DF_TAU / 4.0f;
                float dx = df_sin(yaw), dz = df_cos(yaw);
                float reach = e->radius + 16.0f;
                Camera aim;
                UseHit hit;

                camera_set(&aim, e->x - dx * reach, e->y + entity_height(e) * 0.5f,
                           e->z - dz * reach, yaw, 0.0f);
                if (!interact_aim_from(scene, aim.x, aim.y, aim.z, dx, 0.0f, dz, &hit) || hit.entity != ent_id(e)) { continue; }
                activate(scene, &aim, MODE_TALK, 0, 0, 1, 160, 100);
                check_expect(possessions_state()->page == VIEW_TALK,
                             "tavern: an unaffiliated static patron opens dialogue");
                possessions_close();
                return;
            }
        }
    }
    check_expect(0, "tavern: find an unaffiliated static patron to talk to");
}

static void check_tavern_music(void)
{
    const Level *level = seam_level();
    const World *world = travel_whole_world();
    const char *song = level->sound->song(level->sound, -1, 0);
    Weather weather = {0};
    MotionState arrival;
    float bed[3];
    int i, found;

    ambience_frame(level, &g_camera, &weather, 0, 0, 0);
    check_expect(law_state()->crime == 0 && !strcmp(music_name(), song),
                 "tavern music: entering as a guest should play the tavern song");
    for (i = 0; (found = world_marker_nth(world, 4, i, &bed[0], &bed[1], &bed[2])); i++) {
        if (world_restriction_at(world, bed) > ACCESS_VISITOR) { break; }
    }
    check_expect(found, "tavern music: need a bed area above the shop visitor allowance");
    if (!found) { return; }
    move_capture(&arrival);
    move_place(bed[0], bed[1] + WALK_EYE, bed[2], 0);
    move_camera(&g_camera);
    seam_world_step(&g_camera, 0.0f);
    ambience_frame(level, &g_camera, &weather, 0, 0, 0);
    check_expect(law_state()->crime == 0 && !strcmp(music_name(), song),
                 "tavern music: walking upstairs should not trigger trespass");
    move_restore(&arrival);
    move_camera(&g_camera);
    law_state()->crime = 0;
}

static void check_tavern_bed(void)
{
    const Scene *scene = &seam_level()->scene;
    ShopState shop = {0};
    RoomOffer offer;
    MotionState arrival;
    PlayIn input = {0};
    MoveIn move = {0};
    df_u8 held[ACTS] = {0};
    LodgingState lodging = *lodging_state();
    float bx, by, bz;
    int type, faction, quality, text, i, health;
    unsigned building;
    df_u32 now = game_state()->minutes;

    if (!travel_building(&type, &faction, &quality, &building)) {
        check_expect(0, "tavern bed: no tavern to rent from");
        return;
    }
    room_offer(building, 1, &shop, 50, 50, 1, now, &offer);
    check_expect(room_take(building, &offer, 0, now, &text), "tavern bed: rent the first bed");
    move_capture(&arrival);
    body_step(move_body(), scene, 0, 0, 0.05f);
    move_camera(&g_camera);
    check_expect(rest_open(scene, &g_camera), "tavern bed: open the real rest menu");
    input.mouse_pressed = input.on_picture = 1;
    input.cx = 110; input.cy = 72;
    rest_frame(&input, &g_camera);
    check_expect(rest_state()->stage == REST_HOURS && move_body()->grounded &&
                 !move_airborne() && move_body()->vy == 0.0f,
                 "tavern bed: the rest menu should put the player on a supported floor");
    check_expect(world_marker_nth(travel_whole_world(), 4, 0, &bx, &by, &bz) &&
                 df_absf(move_body()->feet[0] - bx) <= 4 * WALK_RADIUS &&
                 df_absf(move_body()->feet[2] - bz) <= 4 * WALK_RADIUS &&
                 (move_body()->feet[0] != arrival.feet[0] || move_body()->feet[2] != arrival.feet[2]),
                 "tavern bed: the sleeper should be moved near the rented bed");
    rest_close();
    health = character_state()->health;
    move.view_mode = 1; move.dt = 0.05f; move.held = held;
    for (i = 0; i < 100; i++) { move_frame(scene, &move); }
    held[FORWARD] = 1;
    move_frame(scene, &move);
    check_expect(move_body()->grounded && !move_airborne() && character_state()->health == health,
                 "tavern bed: waiting and taking a step after rest should cause no fall or damage");
    move_restore(&arrival);
    move_camera(&g_camera);
    *lodging_state() = lodging;
}

static int check_shop_music(void)
{
    static Dungeon town;
    static RmbRestriction points[RMB_RESTRICTIONS_MAX];
    const PlaceWhere *where = &world_where()->where;
    const df_u8 *bytes;
    unsigned size;
    const Level *level = seam_level();
    Weather weather = {0};
    int i, n, low = -1, high = -1, sneak = 0;
    float dx, dz;

    if (!exterior_load_at(where->region, where->index, &town)) {
        check_expect(0, "shop music: could not find the interior's town");
        return 0;
    }
    dx = (float)town.block[where->block].x * STREAM_CHUNK;
    dz = (float)town.block[where->block].z * STREAM_CHUNK;
    bytes = town_block_bytes(where->region, where->index, where->block, &size);
    n = bytes ? rmb_restrictions(bytes, size, where->record, points, RMB_RESTRICTIONS_MAX) : 0;
    for (i = 0; i < n; i++) {
        if (points[i].level == ACCESS_VISITOR) { low = i; }
        if (points[i].level > ACCESS_VISITOR) { high = i; }
    }
    check_expect((dx != 0 || dz != 0) && low >= 0 && high >= 0, "shop music: need public and restricted points in a shifted town block");
    if (low < 0 || high < 0) { return 0; }
    law_state()->crime = 0;
    move_place(points[low].x + dx, points[low].y + WALK_EYE, points[low].z + dz, 0);
    move_camera(&g_camera);
    seam_world_step(&g_camera, 0.0f);
    ambience_frame(level, &g_camera, &weather, 0, 0, 0);
    check_expect(law_state()->crime == 0 && !strcmp(music_name(), level->sound->song(level->sound, -1, 0)),
                 "shop music: a public point triggered trespass");
    move_place(points[high].x + dx, points[high].y + WALK_EYE, points[high].z + dz, 0);
    move_camera(&g_camera);
    seam_world_step(&g_camera, 0.0f);
    ambience_frame(level, &g_camera, &weather, 0, 0, 0);
    for (i = 0; i < 7; i++) { sneak |= !strcmp(music_name(), sounds_sneak(i)); }
    check_expect(law_state()->crime == CRIME_TRESPASS && sneak, "shop music: a restricted point did not trigger trespass music");
    move_place(points[low].x + dx, points[low].y + WALK_EYE, points[low].z + dz, 0);
    move_camera(&g_camera);
    seam_world_step(&g_camera, 0.0f);
    ambience_frame(level, &g_camera, &weather, 0, 0, 0);
    check_expect(!strcmp(music_name(), level->sound->song(level->sound, -1, 0)), "shop music: returning to public space did not restore the shop song");
    /* Walk out while the override is active, before another update runs outdoors. */
    move_place(points[high].x + dx, points[high].y + WALK_EYE, points[high].z + dz, 0);
    move_camera(&g_camera);
    seam_world_step(&g_camera, 0.0f);
    law_state()->crime = 0;
    return 1;
}

static void check_daggerfall_shop(void)
{
    static df_u32 items[OBJECTS];
    const TownBuilding *rows, *nearest = 0;
    const Scene *scene;
    const Usable *door = 0;
    df_u32 inn = 0;
    int region, index, block, record, i, j, count, arrows = 0, music_tested;
    float x, z, best = 0.0f;

    if (!travel_town_here(&g_camera, &region, &index, &x, &z, &block, &record)) {
        check_expect(0, "shop: could not locate Daggerfall's town blocks");
        return;
    }
    rows = town_buildings(region, index, &count);
    for (i = 0; rows && i < count; i++) {
        float dx = rows[i].x - x, dz = rows[i].z - z;
        float distance = dx*dx + dz*dz;

        if (!inn && rows[i].type == BUILDING_TAVERN) { inn = gfall_building_id(&rows[i]); }
        if (rows[i].type == BUILDING_WEAPONS && (!nearest || distance < best)) {
            nearest = &rows[i]; best = distance;
        }
    }
    check_expect(nearest != 0, "shop: no weaponsmith in Daggerfall's building list");
    if (!nearest) { return; }
    block = nearest->block; record = nearest->record;
    move_place(g_camera.x + nearest->x - x, g_camera.y, g_camera.z + nearest->z - z, 0);
    move_camera(&g_camera);
    seam_world_step(&g_camera, 0.05f);
    scene = &seam_level()->scene;
    for (i = 0; i < scene->nusables; i++) {
        const Usable *u = &scene->usables[i];

        if (u->kind == USE_PASSAGE && u->what == 1 && u->reach > 0 && u->tag == ((block << 8) | record)) {
            door = u; break;
        }
    }
    check_expect(door != 0, "shop: no weaponsmith door in Daggerfall's loaded blocks");
    if (!door) { return; }
    state_pass_minutes((720u + 1440u - game_state()->minutes % 1440u) % 1440u);
    check_expect(travel_passage(door) && change(), "shop: the Daggerfall weaponsmith door did not open at noon");
    if (world_where()->where.kind != WHERE_INSIDE) { return; }
    scene = &seam_level()->scene;
    for (i = 0; i < scene->nusables && !arrows; i++) {
        const Usable *u = &scene->usables[i];

        if (u->kind != USE_OTHER || u->what != OTHER_SHELF || !gfall_shelf_used(i, u->tag)) { continue; }
        if (possessions_state()->trade == MARKET_BUY) {
            count = inventory_list(possessions_state()->remote, 0, items, OBJECTS);
            for (j = 0; j < count && j < OBJECTS; j++) {
                const ObjectInfo *item = object_at(items[j]);

                if (item && item->group == ITEM_GROUP_WEAPON && item->index == WEAPON_ARROW) { arrows += item->count; }
            }
        }
        possessions_close();
    }
    check_expect(arrows > 0, "shop: the Daggerfall weaponsmith had no arrows for sale on its shelves");
    music_tested = check_shop_music();
    {
        GameState *state = (GameState *)game_state();
        df_u32 minutes = state->minutes;
        RoomInfo *room = &lodging_state()->room[0];
        RoomInfo previous = *room;
        const Possessions *view = possessions_state();

        state->minutes = minutes - minutes % (360u * 1440u) + 14u * 1440u + 720u;
        room->building = inn;
        room->expires = state->minutes + 15u * 60u;
        msg_clear();
        door = passage(2);
        check_expect(door && travel_passage(door) && change(), "shop: could not leave the Daggerfall weaponsmith");
        check_expect(view->page == VIEW_MESSAGE && strstr(view->notice, "South Wind's Prayer") &&
                     strstr(view->notice, "in Daggerfall") && strstr(view->notice, "Town's temples") &&
                     strstr(view->notice, "usual price") && !strchr(view->notice, '%'),
                     "holiday arrival shows the complete South Wind's Prayer parchment with its town macros expanded");
        check_expect(inn && strstr(message_line(), "You have a room at") && strstr(message_line(), "for 16 hours."),
                     "holiday arrival also retains the rented-room reminder in the HUD feed");
        possessions_close(); msg_clear();
        *room = previous;
        state->minutes = minutes;
    }
    if (music_tested && world_where()->where.kind == WHERE_WORLD) {
        Weather weather = {0};
        const Level *level = seam_level();

        ambience_frame(level, &g_camera, &weather, 0, 0, 0);
        check_expect(level->sound && !strcmp(music_name(), level->sound->song(level->sound, weather.kind, 0)),
                     "shop music: the first street frame kept the indoor override");
    }
}

static void check_transport_key(void)
{
    df_u32 horse = item_create(game_state()->inventory_id, ITEM_GROUP_TRANSPORT, 1, 0);
    df_u32 cart = item_create(game_state()->inventory_id, ITEM_GROUP_TRANSPORT, 0, 0);
    const Scene *scene = &seam_level()->scene;
    const Possessions *menu = possessions_state();
    PlayIn input = {0};
    MoveIn idle = {0};
    CfaPicture riding;
    float x, y, z;
    int i;

    idle.region = STEER_CENTRE; idle.dt = 0.05f;
    for (i = 0; i < 16 && !move_body()->grounded; i++) { move_frame(scene, &idle); }
    move_camera(&g_camera);
    input.pressed[TRANSPORT] = 1;
    play_frame(&input, &g_camera, scene);
    check_expect(horse && cart && menu->page == VIEW_TRANSPORT && menu->transport_enabled == 23u,
                 "transport: the key offers the carried horse and cart without a ship");
    {
        ImgPicture base, inactive;
        const df_u8 *pixels;
        int row, good = 1;

        seam_present();
        pixels = seam_screen(0);
        if (pixels && img_load("MOVE00I0.IMG", &base) && img_load("MOVE01I0.IMG", &inactive)) {
            for (row = 0; row < base.height; row++) {
                int col, y = base.y + row;

                for (col = 0; col < base.width; col++) {
                    int x = base.x + col;
                    int ship = y >= 78 && y < 87 && x >= 99 && x < 221;
                    int color = ship ? inactive.pixels[(27 + y - 78) * inactive.width + x - 99] : base.pixels[row * base.width + col];

                    good &= pixels[y * 320 + x] == color;
                }
            }
            check_expect(good && !pixels[0] && !seam_screen_covers(), "transport: the panel and disabled ship art must be drawn over the world");
        } else { check_expect(0, "transport: MOVE panel assets did not load"); }
    }
    click_at(160, 82);
    check_expect(menu->page == VIEW_TRANSPORT && !play_travel_asked(), "transport: an unavailable ship leaves the chooser open");
    click_at(160, 64);
    check_expect(move_mounted() == MOUNT_HORSE && menu->page == VIEW_NONE, "transport: choosing Horse did not mount");
    if (passage(DOOR_ENTER)) {
        check_expect(travel_passage(passage(DOOR_ENTER)) && !play_travel_asked(), "transport: a building let the player ride inside");
    }
    check_expect(cfa_load("MRED00I0.CFA", &riding) && riding.frames == 4 && riding.y == 49, "transport: horse riding frames did not decode");
    check_expect(cfa_load("MRED01I0.CFA", &riding) && riding.frames == 4 && riding.y == 49, "transport: cart riding frames did not decode");
    state_capture_motion();
    check_expect(seam_capture_world() && save_write(8) && save_read(8) && seam_open_place(1, 0, &x, &y, &z), "transport: the mounted save could not be reopened");
    check_expect(seam_land(&g_camera, 1, 0), "transport: land in the reopened world");
    scene = &seam_level()->scene;
    check_expect(move_mounted() == MOUNT_HORSE, "transport: reloading did not restore the horse");
    fs_delete("gfall\\saves\\S0008.SAV");
    play_frame(&input, &g_camera, scene);
    click_at(160, 55);
    check_expect(!move_mounted() && menu->page == VIEW_NONE, "transport: choosing On foot did not dismount");
    object_delete(horse); object_delete(cart);
}

static int tavern_map_name(int region, int index, int tag, char *name, int cap, int hover)
{
    const TownBuilding *t = town_building(region, index, tag >> 8, tag & 255);
    const df_u8 *pixels;
    int x, y, found;

    if (!t || !gfall_building_map_name(t, region, index, name, cap)) { return 0; }
    if (!hover) { return 1; }
    if (!townmap_open(region, index, t->x, t->z, 0)) { return 0; }
    townmap_compose();
    pixels = townmap_pixels();
    found = 0;
    for (y = 1; y < 170 && !found; y++) {
        for (x = 1; x < 319; x++) {
            if (pixels[y * 320 + x] != townmap_colour(BUILDING_TAVERN + 1, 0)) { continue; }
            townmap_point(x, y);
            if (!strcmp(townmap_line(), name)) { found = 1; break; }
        }
    }
    townmap_close();
    return found;
}

void check_world(const char *root)
{
    char resolved[4096];
    const Usable *door;
    LocationInfo place;
    double bx, bz, ax, az;
    float x, y, z;
    int i, held, region, index, opened, status;
    int tavern_tag = -1;
    char tavern_name[65];
    pid_t child;
    const char *test_root = "build/gfall-world-data/ARENA2";
    check_expect(seam_land(&g_camera,0,0), "world: land in the initial place");
    status_check();
    state_capture_motion(); seam_capture_world(); g_before = *game_state(); move_capture(&g_motion); move_camera(&g_camera);
    fs_set_root("build/ARENA2"); fs_make_dirs("gfall-world-data");
    check_expect(realpath(root,resolved) != 0,"resolve world test assets"); unlink(test_root);
    check_expect(symlink(resolved,test_root) == 0,"isolate world save writes from game data"); vfs_mount(test_root);
    for (i = 0; i < seam_level()->scene.nmodels && i < 16384; i++) { g_drawn[i] = i; }
    automap_frame(&seam_level()->scene,&g_camera,g_drawn,i);
    opened = maps_open(0);
    check_expect(opened && seam_automap_view() != 0 &&
                 automap_polygons(&seam_level()->scene,seam_automap_view(),map_emit,0) > 0,
                 "the indoor map presents actual seen engine geometry");
    map_picture("build/gfall-dungeon-map.ppm"); map_key(27);
    check_expect(!maps_up(),"Escape closes the map without advancing the world");
    recall("anchor");
    door = passage(4); check_expect(door != 0,"the Hold has an actual exit passage");
    if (!door) { goto restore; }
    held = dungeon_count();
    check_expect(travel_passage(door) && play_travel_asked() && change(),"the actual Hold exit opens the streamed outside");
    check_expect(seam_level()->streamed && seam_level()->sky && stream_active() && seam_level()->scene.nmodels > 0 && !dungeon_count() && held > 0,
                 "outside uses engine chunks, terrain and sky, without turning pedestrians into dungeon foes");
    recall("recall");
    check_expect(play_travel_asked() && !magic_state()->recall.valid && change() &&
                 world_where()->where.kind == WHERE_DUNGEON &&
                 check_near(g_camera.x,g_before.x,0.1f) && check_near(g_camera.z,g_before.z,0.1f),
                 "Recall crosses from the exterior back to the exact saved dungeon anchor");
    door = passage(4);
    check_expect(door && travel_passage(door) && change(),"the recalled dungeon can be left again");
    move_capture(&g_motion);
    check_expect(travel_count() > 15000 && cmd_run("goto Daggerfall") && play_travel_asked() && change(),
                 "goto resolves an actual town name and opens its streamed street");
    check_expect(world_where()->where.kind == WHERE_WORLD && game_state()->region == 17,"Daggerfall arrival updates the persistent region");
    {
        const Entity *actor;
        Camera away;
        df_u32 saved_random;

        saved_random = game_state()->random;
        random_seed(0);
        check_expect(encounter_minute(&seam_level()->scene,&g_camera,0,ENCOUNTER_WALK),
                     "a nighttime encounter can spawn in a real Daggerfall street");
        actor = dungeon_entity(0);
        check_expect(actor && actor->pose[ENTITY_WALK].w[0] && encounter_outdoor_actor(actor),
                     "the encounter has real streamed creature art and a combat actor");
        away = g_camera; away.x += 5000;
        encounter_update(&seam_level()->scene,&away);
        check_expect(!dungeon_count(),"the real streamed encounter leaves no combat actor past its leash");
        random_seed(saved_random);
    }
    /* The people inside Daggerfall's buildings, read by GFALL's own reader: shops have a shopkeeper, houses none. */
    {
        int region_t, index_t, block_t, record_t, count, i, k, n, shops, shopkeepers, house_keepers, people_total;
        float tx, tz;
        const TownBuilding *rows;

        check_expect(travel_town_here(&g_camera, &region_t, &index_t, &tx, &tz, &block_t, &record_t), "people: not in a town for the interior test");
        rows = town_buildings(region_t, index_t, &count);
        shops = shopkeepers = house_keepers = people_total = 0;
        for (i = 0; rows && i < count; i++) {
            InteriorPerson people[INTERIOR_PEOPLE];
            const unsigned char *bytes;
            unsigned size;
            int has;

            bytes = town_block_bytes(region_t, index_t, rows[i].block, &size);
            n = interior_people(bytes, size, rows[i].record, people, INTERIOR_PEOPLE);
            people_total += n;
            has = 0;
            for (k = 0; k < n; k++) { has |= interior_shopkeeper(&people[k]); }
            if (rows[i].type == 1) { house_keepers += has; }
            else if (rows[i].type == 0 || rows[i].type == 2 || rows[i].type == 3 || rows[i].type == 13) { shops++; shopkeepers += has; }
        }
        printf("interior: %d buildings, %d people, %d of %d shops have a shopkeeper, %d houses\n", count, people_total, shopkeepers, shops, house_keepers);
        check_expect(people_total > 20 && shops > 5 && shopkeepers * 2 > shops && house_keepers == 0, "people: the block's shopkeepers are not in the shops");
        /* A building's key is its own: no two of the town's buildings share one (the record alone was the key, and every block has record 0), whatever the block. */
        {
            int a, c2, same = 0;

            for (a = 0; rows && a < count; a++) {
                for (c2 = a + 1; c2 < count; c2++) {
                    same += town_building_key(region_t,index_t,rows[a].block,rows[a].record) == town_building_key(region_t,index_t,rows[c2].block,rows[c2].record);
                }
            }
            check_expect(count > 1 && same == 0 && town_building_key(region_t,index_t,0,0) != town_building_key(region_t,index_t,1,0), "buildings: two buildings of a town share a key");
        }
        /* Section 3: every interior's points read, with the restriction bytes the BLOCKS.BSA scan found (0, 5..45) and most of them 5 or 10. */
        {
            static RmbRestriction points[192];
            int total = 0, low = 0, odd = 0, k2, m;
            const unsigned char *bytes;
            unsigned size;

            for (i = 0; rows && i < count; i++) {
                bytes = town_block_bytes(region_t, index_t, rows[i].block, &size);
                m = rmb_restrictions(bytes, size, rows[i].record, points, 192);
                for (k2 = 0; k2 < m; k2++) {
                    total++; low += points[k2].level == 5 || points[k2].level == 10;
                    odd += points[k2].level < 0 || points[k2].level > 45;
                }
            }
            check_expect(total > 100 && odd == 0 && low * 10 > total * 7, "access: the interiors' restriction points are not mostly 5 and 10, or are out of 0..45");
        }
    }
    /* Slice 8: a person in the view is named in info mode, greets in talk mode, and has a pocket in steal mode. */
    {
        Scene *sc;
        Camera look;
        int k, found, female_check;

        sc = &seam_level()->scene;
        people_fill(&g_camera);
        found = -1;
        for (k = 0; k < sc->nentities && found < 0; k++) {
            if (sc->entities[k].live && entity_height(&sc->entities[k]) > 0) { found = k; }
        }
        check_expect(found >= 0, "people: no person to talk to in Daggerfall");
        if (found >= 0) {
            const Entity *person;
            char line[400];

            person = &sc->entities[found];
            camera_set(&look, person->x, person->y + entity_height(person) * 0.5f, person->z - 60, 0, 0);
            female_check = activate(sc, &look, MODE_INFO, 0, 0, 1, 160, 100);
            check_expect(female_check, "people: info mode did not take a person");
            character_state()->skill[15] = 100;
            law_state()->crime = 0;
            activate(sc, &look, MODE_TALK, 0, 0, 1, 160, 100);
            check_expect(possessions_state()->page == VIEW_TALK && talkwin_state()->open && talkwin_state()->answer_count > 0,
                         "people: talk mode did not open the window with a greeting");
            talk_shot("talk0");
            /* The window's own buttons: the categories of this town's buildings are listed, Escape leaves. */
            check_expect(talkwin_state()->categories && talkwin_state()->count > 0, "people: the window lists no places");
            check_named_directions(&look);
            click_talk(talkwin_button(TB_TELL));
            talk_shot("talk1");
            click_talk(talkwin_button(TB_WHERE));
            click_talk(talkwin_button(TB_OKAY));
            talk_shot("talk2");
            click_talk(talkwin_button(TB_GOODBYE));
            check_expect(possessions_state()->page == VIEW_NONE, "people: Goodbye did not end the talk");
            /* A shopkeeper's panels: drawn over the world, Exit leaves; a click outside every button leaves it be. */
            check_expect(possessions_service(SERVICE_REPAIR) && possessions_state()->page == VIEW_SERVICE, "people: the repair panel did not open");
            talk_shot("service0");
            click_at(120, 78);
            check_expect(possessions_state()->page == VIEW_SERVICE, "people: a click between the buttons closed the panel");
            click_at(160, 87);
            check_expect(possessions_state()->page == VIEW_NONE, "people: Exit did not close the repair panel");
            check_expect(possessions_service(SERVICE_BANK), "people: the bank's panel did not open");
            talk_shot("service1");
            click_at(160, 87);
            check_expect(possessions_state()->page == VIEW_NONE, "people: Exit did not close the bank's panel");
            /* The innkeeper's panel: its picture, and Goodbye (the fourth row) leaves. */
            check_expect(possessions_service(SERVICE_TAVERN), "people: the tavern's panel did not open");
            talk_shot("tavern0");
            click_at(160, 82);
            check_expect(possessions_state()->page == VIEW_NONE, "people: Goodbye did not close the tavern's panel");
            /* A coven's menu (DAED00I0): Talk, Summon, Quest and Exit; a click between the buttons leaves it be, Exit leaves. */
            check_expect(possessions_service(SERVICE_COVEN) && possessions_state()->service == SERVICE_COVEN, "people: the coven's menu did not open");
            talk_shot("coven0");
            click_at(160, 79);
            check_expect(possessions_state()->page == VIEW_SERVICE, "people: a click between the coven's buttons closed its menu");
            click_at(160, 87);
            check_expect(possessions_state()->page == VIEW_NONE, "people: Exit did not close the coven's menu");
            /* A guild hall's menu: Join is shut to a member; Exit leaves. A yes or no and a number go to their callbacks. */
            check_expect(possessions_guild(1, "Training") && possessions_state()->service == SERVICE_GUILD, "people: the hall's menu did not open");
            talk_shot("guild0");
            click_at(160, 55);
            check_expect(possessions_state()->page == VIEW_SERVICE, "people: a member could press Join");
            click_at(160, 87);
            check_expect(possessions_state()->page == VIEW_NONE, "people: Exit did not close the hall's menu");
            check_guild_rank_dialogues();
            g_asked = -1;
            check_expect(possessions_ask("Join the guild?", ask_done) && possessions_state()->prompt == PROMPT_ASK, "people: the question did not open");
            talk_shot("guild1");
            click_at(117, 125);
            check_expect(g_asked == 1 && possessions_state()->page == VIEW_NONE, "people: Yes did not reach its callback and close");
            check_expect(possessions_ask("Join the guild?", ask_done), "people: the question did not open again");
            click_at(203, 125);
            check_expect(g_asked == 0 && possessions_state()->page == VIEW_NONE, "people: No did not reach its callback and close");
            g_number = -1;
            check_expect(possessions_number("How much?", number_done) && possessions_state()->prompt == PROMPT_NUMBER, "people: the number did not open");
            type_char('1'); type_char('2'); type_char('x'); type_char('5');
            type_char(13);
            check_expect(g_number == 125 && possessions_state()->page == VIEW_NONE, "people: the number typed was not 125 with digits only");
            /* The potion maker: Healing's four ingredients clicked into the cauldron, Mix, and a bottle of Healing in the pack. */
            {
                int group[8], index[8], n2, k2, bottles;

                n2 = potion_ingredients(2, group, index, 8);
                for (k2 = 0; k2 < n2; k2++) { item_create(game_state()->inventory_id, group[k2], index[k2], -1); }
                check_expect(possessions_potion() && possessions_state()->page == VIEW_POTION, "people: the potion maker did not open");
                talk_shot("potion0");
                for (k2 = 0; k2 < n2; k2++) { click_at(28, 42); }
                check_expect(cauldron_count() == n2, "people: the ingredients did not go into the cauldron by clicks");
                talk_shot("potion1");
                click_at(190, 48);
                bottles = 0;
                for (k2 = 0; k2 < OBJECTS; k2++) { if (game_state()->object[k2].id && potion_of(game_state()->object[k2].id) == 2) { bottles++; } }
                check_expect(bottles >= 1 && possessions_state()->notice[0] != 0, "people: Mix did not make a bottle of Healing and say so");
                click_at(160, 100);
                click_at(298, 190);
                check_expect(possessions_state()->page == VIEW_NONE, "people: Exit did not close the potion maker");
            }
            /* The spell shop: the spells a person offers listed, one chosen by a click, Buy given its id, Exit leaves. */
            {
                int offered[SPELLSHOP_MOST], listed;

                listed = spellshop_list(4321u, 20, offered, SPELLSHOP_MOST);
                check_expect(listed > 1 && possessions_spellshop(offered, listed, shop_bought) && possessions_state()->page == VIEW_SPELLSHOP, "people: the spell shop did not open");
                talk_shot("spellshop0");
                click_at(60, 30 + 9 + 2);
                check_expect(possessions_state()->selected == 1, "people: a click did not choose the second spell");
                g_shop_id = -1;
                click_at(40, 174);
                check_expect(g_shop_id == offered[1] && possessions_state()->page == VIEW_SPELLSHOP, "people: Buy did not hand over the chosen spell and keep the window");
                click_at(260, 174);
                check_expect(possessions_state()->page == VIEW_NONE, "people: Exit did not close the spell shop");
            }
            /* The spell maker: an effect added from the list, its subtype and numbers by clicks, a name typed, bought into the book, Exit. */
            {
                static const int left[11] = {64, 104, 160, 64, 104, 160, 64, 104, 144, 184, 240};
                static const int top[11] = {94, 94, 94, 114, 114, 114, 134, 134, 134, 134, 134};
                df_u32 book2, rows2[OBJECTS];
                int row, k2, before2, up, value;

                book2 = inventory_book();
                before2 = book2 ? spell_rows(book2, rows2, OBJECTS) : 0;
                check_expect(possessions_spellmaker() && possessions_state()->page == VIEW_MAKER, "maker: the spell maker did not open");
                talk_shot("maker0");
                click_at(255, 125);
                check_expect(possessions_state()->prompt == PROMPT_LIST && possessions_state()->list_count > 20, "maker: the add button did not list the effects");
                row = -1;
                for (k2 = 0; k2 < possessions_state()->list_count && k2 < LIST_SHOWN; k2++) { if (!strcmp(possessions_state()->list[k2], "Damage")) { row = k2; } }
                check_expect(row >= 0, "maker: Damage is not in the list of effects");
                if (row >= 0) {
                    click_at(150, 34 + row * 8 + 3);
                    check_expect(possessions_state()->prompt == PROMPT_LIST && possessions_state()->list_count == 3 && !strcmp(possessions_state()->list[0], "Health"), "maker: Damage did not ask for its subtype");
                    click_at(150, 37);
                    check_expect(maker_state()->kind != 0 && maker_state()->spell[1] == 0, "maker: the subtype did not open the numbers");
                    talk_shot("maker1");
                    up = -1;
                    for (k2 = 0; k2 < 11 && up < 0; k2++) { if (maker_spin_allowed(k2)) { up = k2; } }
                    check_expect(up >= 0, "maker: Damage has no number to set");
                    if (up >= 0) {
                        value = maker_value(up);
                        click_at(left[up] + 12, top[up] + 2);
                        check_expect(maker_value(up) == value + 1 || maker_value(up) == value, "maker: the up arrow moved the number the wrong way");
                        click_at(left[up] + 12, top[up] + 12);
                        check_expect(maker_value(up) >= 1, "maker: the down arrow took the number under 1");
                    }
                    click_at(292, 100);
                    check_expect(maker_state()->kind == 0 && maker_price() > 0, "maker: the close button did not leave the numbers with a price");
                    click_at(100, 187);
                    check_expect(possessions_state()->prompt == PROMPT_SPELL_NAME, "maker: the name box did not ask for a name");
                    type_char('Z'); type_char('a'); type_char('p'); type_char(13);
                    check_expect(!strcmp((const char *)maker_state()->spell + MAKER_NAME_AT, "Zap"), "maker: the name typed was not kept");
                    character_state()->gold = 100000;
                    talk_shot("maker2");
                    click_at(256, 155);
                    check_expect(possessions_state()->notice[0] != 0 && spell_rows(book2, rows2, OBJECTS) == before2 + 1, "maker: Buy did not add the spell to the book and say so");
                    click_at(160, 100);
                }
                click_at(256, 187);
                check_expect(possessions_state()->page == VIEW_NONE, "maker: Exit did not close the spell maker");
            }
            /* The item maker: an item taken from the list, a power with its parameter added by clicks, a side effect, ENCHANT, and the item gone from the list. */
            {
                df_u32 gem2, pack2, ids2[OBJECTS];
                int idx, listed, before_gold, row2;

                pack2 = game_state()->inventory_id;
                idx = 0;
                gem2 = item_create(pack2, 3, 1, 9);
                (void)idx;
                character_state()->gold = 1000000;
                check_expect(possessions_itemmaker() && possessions_state()->page == VIEW_ITEMMAKER, "itemmaker: the item maker did not open");
                talk_shot("itemmaker0");
                listed = possessions_im_list(ids2, OBJECTS);
                check_expect(listed > 0, "itemmaker: the list shows nothing to enchant");
                row2 = -1;
                {
                    int k3;

                    for (k3 = 0; k3 < listed && k3 < 4; k3++) { if (ids2[k3] == gem2) { row2 = k3; } }
                }
                if (row2 >= 0) {
                    click_at(280, 52 + row2 * 37);
                    check_expect(itemmaker_state()->item == gem2, "itemmaker: a click on the list did not take the item");
                    click_at(40, 187);
                    check_expect(possessions_state()->prompt == PROMPT_LIST && possessions_state()->list_count > 3, "itemmaker: ADD POWERS did not list the powers");
                    click_at(150, 34 + 4 * 8 + 3);
                    check_expect(possessions_state()->prompt == PROMPT_LIST || itemmaker_state()->pending < 0, "itemmaker: a power chosen did not ask for its parameter");
                    if (possessions_state()->prompt == PROMPT_LIST) { click_at(150, 37); }
                    check_expect(itemmaker_state()->kind[0] == 0 && itemmaker_state()->pending < 0, "itemmaker: the first slot is not a finished power");
                    talk_shot("itemmaker1");
                    click_at(25, 61);
                    check_expect(itemmaker_state()->kind[0] == -1, "itemmaker: a click on a power's row did not take it out");
                    click_at(40, 187);
                    click_at(150, 34 + 11 * 8 + 3);
                    before_gold = character_state()->gold;
                    click_at(225, 122);
                    check_expect(possessions_state()->notice[0] != 0, "itemmaker: ENCHANT said nothing");
                    click_at(160, 100);
                    check_expect(itemmaker_state()->item == 0 && itemmaker_row(gem2) != 0 && character_state()->gold < before_gold, "itemmaker: ENCHANT did not enchant the item and charge for it");
                    talk_shot("itemmaker2");
                }
                click_at(225, 187);
                check_expect(possessions_state()->page == VIEW_NONE, "itemmaker: Exit did not close the item maker");
            }
            /* A Fighters' trainer: a member sees the menu, the service asks the price, lists the skills and teaches one, three hours pass. */
            {
                GuildStep joined;
                df_u32 before;
                int gold;

                guilds_clear();
                guild_join(2, 41, game_state()->minutes, &joined);
                faction_add_reputation(41, 40);
                character_state()->level = 2;
                character_state()->gold = 1000;
                character_state()->skill[32] = 30;
                guilds_state()->last_training = 0;
                state_pass_minutes(2000);
                before = game_state()->minutes;
                gold = character_state()->gold;
                seam_hall_enter(849, 41);
                check_expect(possessions_state()->page == VIEW_SERVICE && possessions_state()->service == SERVICE_GUILD && possessions_state()->service_member &&
                             !strcmp(possessions_state()->service_label, "Training"), "guild: the Fighters' trainer did not open the member's menu");
                click_at(160, 73);
                check_expect(possessions_state()->prompt == PROMPT_ASK, "guild: the lesson's price was not asked");
                click_at(117, 125);
                check_expect(possessions_state()->prompt == PROMPT_LIST && possessions_state()->list_count == 11, "guild: the trainer's skills were not listed");
                click_at(150, 38);
                check_expect(character_state()->gold == gold - 200 && game_state()->minutes == before + 180 && guilds_state()->last_training == before &&
                             possessions_state()->notice[0] != 0, "guild: the lesson did not cost 200 and take three hours");
                possessions_close();
                /* Get Quest at a Fighters Guild: a member of rank 0 is offered a quest of the guild (M0B0..), asked yes or no, and accepted. */
                {
                    int q3, started, bats;

                    quest_reset();
                    guilds_clear(); guild_join(2, 41, game_state()->minutes, &joined);
                    seam_hall_enter(851, 41);
                    check_expect(possessions_state()->page == VIEW_SERVICE, "quest: the Fighters' quest giver did not open the member's menu");
                    click_at(160, 73);
                    if (possessions_state()->prompt == PROMPT_ASK) { click_at(117, 125); }
                    started = 0;
                    for (q3 = 0; q3 < quest_count(); q3++) { if (quest_at(q3) && quest_at(q3)->qbn.name[0] == 'M' && quest_at(q3)->qbn.name[2] == 'B') { started++; } }
                    check_expect(started == 1, "quest: Get Quest did not start one quest of the Fighters Guild");
                    possessions_close();
                    quest_reset();

                    quest_set_questor((int)gfall_people_id(), 851, "Theodore Gaering", "Daggerfall");
                    bats = quest_start("M0B00Y15", 851);
                    check_expect(bats >= 0, "quest: the bat job should bind to the Fighters Guild quest giver");
                    if (bats >= 0) {
                        quest_run();
                        for (q3 = 0; q3 < 4; q3++) { quest_foe_killed(quest_at(bats)->foe[0].number); }
                        seam_hall_enter(851, 41);
                        click_at(160, 73);
                        quest_run();
                        check_expect(!quest_at(bats) && possessions_state()->page == VIEW_MESSAGE &&
                                     strstr(possessions_state()->notice, "Killing bats, even giant ones"),
                                     "quest: Get Quest turns in the completed bat job and shows its reward parchment");
                        possessions_close();
                    }
                    quest_reset();
                }
            }
            (void)line;
        }
    }
    check_daggerfall_shop();
    check_transport_key();
    /* The street and the watch first: what they leave behind must not stop the place being left and restored (a person of the street has no keep key). */
    street_checks();
    street_reputation_check();
    street_release_check();
    check_offer_pages();
    check_spider_quest();
    /* Teleporting (guild_teleport): to a place on the map, at once, with no gold and no time; one that is not on the map is refused. */
    {
        LocationInfo target;
        int k, here_region, minutes_before, gold_before;

        here_region = game_state()->region;
        target.region = -1;
        for (k = 0; k < travel_count(); k++) {
            LocationInfo candidate;

            if (travel_location(k, &candidate) && travel_known(k) && candidate.region != here_region && candidate.type == 6) { target = candidate; break; }
        }
        check_expect(target.region >= 0, "teleport: no known town elsewhere on the map");
        if (target.region >= 0) {
            minutes_before = (int)game_state()->minutes;
            gold_before = character_state()->gold;
            check_expect(teleport(target.region, target.index) && play_travel_asked(), "teleport: a known place was not arrived at");
            check_expect((int)game_state()->minutes == minutes_before && character_state()->gold == gold_before, "teleport: it took time or gold");
            check_expect(change() && game_state()->region == target.region, "teleport: the player is not in the town's region");
            check_expect(!teleport(-5, 0), "teleport: a place that is none was reached");
            cmd_run("goto Daggerfall"); change();
        }
    }
    walk_across_origin();
    drop_outside();
    region = index = -1;
    for (i = 0; i < travel_count(); i++) {
        travel_location(i,&place);
        if (place.type == 6) { region = place.region; index = place.index; break; }
    }
    check_expect(region >= 0 && travel_go(region,index) && change(),"goto accepts a real tavern location as well as towns");
    /* The doors keep hours now (building_try_enter): the entrance is tried at noon. */
    state_pass_minutes((df_u32)((12*60 + 1440 - (int)(game_state()->minutes % 1440u)) % 1440));
    door = passage(1);
    check_expect(door != 0,"a streamed tavern has a building entrance");
    if (door) {
        Scene *entry_scene;
        int k, entered;

        /* Use a real tavern door so its arrival can be distinguished from a house's. */
        entry_scene = &seam_level()->scene;
        msg_clear();
        entered = 0;
        for (k = 0; k < entry_scene->nusables && !entered; k++) {
            const Usable *candidate;
            const TownBuilding *building;

            candidate = &entry_scene->usables[k];
            if (candidate->kind != USE_PASSAGE || candidate->what != 1 || candidate->reach <= 0) { continue; }
            building = town_building(region, index, candidate->tag >> 8, candidate->tag & 0xff);
            if (building && building->type == BUILDING_TAVERN && travel_passage(candidate) && play_travel_asked()) {
                door = candidate; entered = 1;
            }
        }
        stream_world(door->cx,door->cz,&bx,&bz);
        if (entered) {
            tavern_tag = door->tag;
            check_expect(!tavern_map_name(region, index, tavern_tag, tavern_name, sizeof(tavern_name), 0),
                         "tavern: a queued entry has not revealed the map name yet");
        }
        check_expect(!strstr(message_line(), "You've entered"), "tavern: the announcement appeared before arrival");
        law_state()->crime = 0;
        check_expect(entered && change() && !seam_level()->streamed && !seam_level()->sky && world_where()->where.kind == WHERE_INSIDE,
                     "the tavern entrance opens its interior on a separate place heap");
        check_expect(!strncmp(message_line(), "You've entered The ", sizeof("You've entered The ") - 1),
                     "tavern: entering did not announce its named sign on the HUD");
        {
            float mx, my, mz;

            /* The editor markers of the inside are there to be picked: the enter marker (record 8), and beds (record 4) if the building has them. */
            check_expect(entered && world_marker_nth(travel_whole_world(), 8, 0, &mx, &my, &mz), "tavern: the inside's enter marker is not read from the place");
        }
        if (entered && world_where()->where.kind == WHERE_INSIDE) {
            check_tavern_music();
            check_tavern_patron(&seam_level()->scene);
            check_tavern_bed();
        }
        msg_clear();
        door = passage(2); check_expect(door && travel_passage(door) && change(),"the interior exit returns to the streamed world");
        check_expect(!strstr(message_line(), "You've entered"), "tavern: leaving incorrectly announced a building arrival");
        stream_world(g_camera.x,g_camera.z,&ax,&az);
        check_expect(ax-bx < 200 && ax-bx > -200 && az-bz < 200 && az-bz > -200,"leaving a building lands beside the same world door");
        check_expect(tavern_tag >= 0 && tavern_map_name(region, index, tavern_tag, tavern_name, sizeof(tavern_name), 1),
                     "tavern: the outdoor automap shows the visited tavern's sign on hover");
    }
    maps_and_trip();
    check_south_gate_arrival();
    /* Save a little past a rebase: reopening centres the stream on a
     * different chunk, so every live logical position must be translated. */
    move_place(g_camera.x+32*STREAM_CHUNK,g_camera.y,g_camera.z,0); move_camera(&g_camera);
    seam_world_step(&g_camera,0.05f);
    move_place(g_camera.x+2*STREAM_CHUNK,g_camera.y,g_camera.z,0); move_camera(&g_camera);
    seam_world_step(&g_camera,0.05f);
    {
        MissileInfo *missile;
        missile = &magic_state()->missile[0]; mem_zero(missile,sizeof(*missile));
        missile->live = 1; missile->spell = 7; missile->level = 10; missile->target = 3;
        missile->x = g_camera.x+20; missile->y = g_camera.y+10; missile->z = g_camera.z+30; missile->dz = 1;
    }
    state_pose(g_camera.x,g_camera.y,g_camera.z,move_yaw(),0); state_capture_motion();
    state_save_label("World round trip");
    /* The street checks above sent the player to prison for months: a pile left outside for that long is gone (the town's sweep, docs/gfall-place-state.md), so this test's own is dropped again. */
    if (!object_at(g_dropped)) { drop_outside(); }
    check_expect(seam_capture_world() && save_write(6),"save an outside place with its stream origin");
    child = fork();
    if (!child) { execl("/proc/self/exe","gfall-test","--gfall-resume-world",test_root,(char *)0); _exit(127); }
    status = -1;
    if (child > 0) { waitpid(child,&status,0); }
    check_expect(child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,"a fresh process restores the outdoor save and explored map");
    check_expect(save_read(6) && seam_open_place(1,0,&x,&y,&z),"load an outside save instead of always reopening the Hold");
    check_expect(seam_land(&g_camera,1,0), "land in the restored outdoor world");
    check_expect(tavern_tag >= 0 && tavern_map_name(region, index, tavern_tag, tavern_name, sizeof(tavern_name), 1),
                 "tavern: the building name stays revealed through save and load");
    check_expect(seam_level()->streamed && world_where()->where.kind == WHERE_WORLD,"outside restore retains the streamed place kind");
    check_expect(check_near(magic_state()->missile[0].x-g_camera.x,20,0.1f) &&
                 check_near(magic_state()->missile[0].z-g_camera.z,30,0.1f),
                 "outside load translates a saved in-flight missile into the reopened stream's frame");
    {
        const ObjectInfo *drop;
        PlaceWhere anchor;
        float px, py, pz, dx, dy, dz, length;
        drop = object_at(g_dropped);
        check_expect(drop && inventory_rows(drop->id,-1,0,0) > 0,"an outdoor pile and its items survive travel and a save/load");
        anchor = world_where()->where;
        check_expect(ground_position(drop,&px,&py,&pz) &&
                     travel_return(&anchor,px,py+WALK_EYE,pz-8,0,0) && change(),
                     "the saved outdoor pile can be revisited from a later stream origin");
        seam_creatures(&g_camera);
        ground_position(object_at(g_dropped),&px,&py,&pz);
        dx = px-g_camera.x; dy = py+12-g_camera.y; dz = pz-g_camera.z; length = df_sqrt(dx*dx+dy*dy+dz*dz);
        check_expect(ground_aim(g_camera.x,g_camera.y,g_camera.z,dx/length,dy/length,dz/length,128) == g_dropped,
                     "a revisited outdoor pile is visible and pickable at its saved world position");
    }
restore:
    seam_close_hold(); state_restore(&g_before); state_restore_world(); world_restore();
    fs_delete("gfall\\saves\\S0006.SAV"); fs_remove_dir("gfall\\saves"); fs_remove_dir("gfall");
    fs_set_root("build/ARENA2"); unlink(test_root); fs_remove_dir("gfall-world-data"); vfs_mount(root);
    check_expect(seam_open_place(1,0,&x,&y,&z),"restore the original game after world travel tests");
    check_expect(seam_land(&g_camera,1,0), "land in the original game after world travel tests");
}
