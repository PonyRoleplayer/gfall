#include "check_ogworld.h"
#include "check_util.h"
#include "../apps/ogfall/seam.h"
#include "game/ogfall/play/travel.h"
#include "game/ogfall/play/save.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/maps.h"
#include "game/ogfall/play/ailments.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/ground.h"
#include "game/ogfall/play/activate.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/law.h"
#include "game/dagger/places/people.h"
#include "engine/scene/entity.h"
#include "engine/assets/vfs.h"
#include "engine/platform/fs.h"
#include "engine/scene/interact.h"
#include "engine/scene/stream.h"
#include "engine/scene/walker.h"
#include "engine/ui/cmd.h"
#include "engine/core/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static OgState g_before;
static OgMotion g_motion;
static Camera g_camera;
static int g_drawn[16384];
static int g_climate;
static df_u32 g_dropped;
static int route_climate(int x, int y) { return g_climate ? g_climate : (y >= x ? 223 : 228); }
void check_ogjourney(void)
{
    OgTravelHooks hooks;
    OgJourney trip;
    mem_zero(&hooks,sizeof(hooks)); hooks.climate = route_climate; og_travel_install(&hooks);
    g_climate = 228;
    og_journey_plan(0,0,12,0,0,0,0,0,&trip);
    check_expect(trip.minutes == 1488 && trip.hours == 25 && trip.cost == 10,
                 "original woodland walking takes 124 minutes per pixel and five gold per inn night plus five");
    og_journey_plan(0,0,12,0,1,0,0,0,&trip);
    check_expect(trip.minutes == 744 && trip.hours == 13 && trip.cost == 5,
                 "reckless travel halves the estimate before calculating inn charges");
    og_journey_plan(0,0,12,0,0,1,0,0,&trip);
    check_expect(trip.minutes == 1740 && !trip.cost,"camping applies the original 300/256 multiplier to each route pixel");
    og_journey_plan(0,0,1,0,0,0,0,1,&trip);
    check_expect(trip.minutes == 62,"a horse halves the transport factor before the terrain multiplication");
    og_journey_plan(0,0,1,0,0,0,0,2,&trip);
    check_expect(trip.minutes == 92,"a cart retains the original intermediate integer truncation");
    g_climate = 223;
    og_journey_plan(0,0,24,0,0,0,1,0,&trip);
    check_expect(trip.minutes == 1224 && trip.sea_pixels == 24 && trip.cost == 55,
                 "ship time is 51 minutes per sea pixel and the fare counts sea pixels, not elapsed hours");
    og_journey_plan(0,0,24,0,0,1,0,0,&trip);
    check_expect(trip.minutes == 7152 && !trip.cost,"without a ship, sea travel uses 255 minutes before the camping multiplier");
    g_climate = 0;
    og_journey_plan(0,0,2,2,0,0,0,0,&trip);
    check_expect(trip.minutes == 248 && !trip.sea_pixels,"the original strict-greater route step preserves its diagonal endpoint quirk");
    mem_zero(&hooks,sizeof(hooks)); og_travel_install(&hooks);
}
static void map_emit(void *ctx, const AutomapCorner *corner, int n, df_u32 rgb)
{
    (void)ctx; (void)corner; (void)n; (void)rgb;
}
static void recall(const char *answer)
{
    OgIn input;
    og_magic_apply(94,-1,-1,10,1);
    mem_zero(&input,sizeof(input));
    for (; *answer; answer++) { input.ch = *answer; og_possessions_frame(&input); }
    input.ch = 13; og_possessions_frame(&input);
}
static int change(void)
{
    float x, y, z;
    if (!seam_open_place(0,1,&x,&y,&z)) { fprintf(stderr,"world change: %s\n",seam_error()); return 0; }
    seam_land(&g_camera,0,1); seam_creatures(&g_camera); return 1;
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
    OgIn input;
    mem_zero(&input,sizeof(input)); input.key = key; og_maps_frame(&input);
}
static void map_click(int x, int y)
{
    OgIn input;
    mem_zero(&input,sizeof(input)); input.on_picture = 1; input.cx = x; input.cy = y;
    input.mouse_pressed = input.mouse_held = 1; og_maps_frame(&input);
}
static void map_step(float dt)
{
    OgIn input;
    mem_zero(&input,sizeof(input)); input.dt = dt; og_maps_frame(&input);
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
    const char *name;
    df_u32 before;
    int i;
    check_expect(og_maps_open(1),"the classic travel map opens outside with OGFALL's discovery and money callbacks");
    map_click(10,179);
    for (name = "DAGGERFALL"; *name; name++) { map_key(*name); }
    map_key(13); map_step(2); map_picture("build/ogfall-travel-question.ppm");
    map_click(110,116); map_click(200,86); map_click(200,54);
    map_picture("build/ogfall-travel-options.ppm");
    og_ailments_cure(-1,0); og_ailments_cure(-1,1);
    og_character()->health = og_character()->max_health; before = og_state()->minutes;
    {
        OgAilment *a;
        a = &og_magic_state()->ailment[0]; mem_zero(a,sizeof(*a));
        a->active = 1; a->id = 0; a->target = og_magic_key(-1); a->delay = 10;
        map_click(240,102); map_step(2);
        check_expect(og_maps_up() && !og_travel_asked() && og_state()->minutes == before,
                     "a latent ordinary disease asks for confirmation before journey time passes");
        map_picture("build/ogfall-travel-warning.ppm"); map_click(200,116);
        check_expect(og_state()->minutes == before,"declining the illness warning preserves time and travel options");
        og_ailments_cure(-1,0);
    }
    map_click(240,102);
    for (i = 0; og_maps_up() && i < 100; i++) { map_step(1); }
    check_expect(!og_maps_up() && og_travel_asked() && og_state()->minutes > before,
                 "Find, confirmation, options and Begin execute an original-rules journey and advance game time");
    check_expect(change(),"a travel-map journey reaches the chosen streamed town");
}
static void walk_across_origin(void)
{
    OgMissile *missile;
    double x, z, after_x, after_z;
    int ox, oz, nx, nz;
    stream_world(g_camera.x,g_camera.z,&x,&z); stream_origin(&ox,&oz);
    recall("anchor");
    og_move_place(g_camera.x+32*STREAM_CHUNK,g_camera.y,g_camera.z,0); og_move_camera(&g_camera);
    missile = &og_magic_state()->missile[0]; mem_zero(missile,sizeof(*missile));
    missile->live = 1; missile->x = g_camera.x; missile->z = g_camera.z;
    seam_world_step(&g_camera,0.05f); stream_origin(&nx,&nz);
    stream_world(g_camera.x,g_camera.z,&after_x,&after_z);
    check_expect(nx != ox && check_near((float)(after_x-x),32*STREAM_CHUNK,0.1f) &&
                 check_near((float)(after_z-z),0,0.1f) && check_near(og_move_body()->feet[0],g_camera.x,0.1f) &&
                 check_near(missile->x,g_camera.x,0.1f) && check_near(og_state()->x,g_camera.x,0.1f),
                 "a real stream-origin move keeps the camera, walking body, saved pose and spell missile in one frame");
    mem_zero(missile,sizeof(*missile));
    recall("recall");
    check_expect(og_travel_asked() && change(),"Recall reopens an outdoor anchor saved under an earlier stream origin");
    stream_world(g_camera.x,g_camera.z,&after_x,&after_z);
    check_expect(check_near((float)(after_x-x),0,0.1f) && check_near((float)(after_z-z),0,0.1f),
                 "an outdoor Recall anchor names a world position, not the current origin's local coordinates");
}
static void drop_outside(void)
{
    Camera aim;
    df_u32 container, item;
    float x, y, z;
    container = og_ground_container(g_camera.x,og_move_body()->feet[1],g_camera.z+8);
    item = og_item_create(container,3,0,1); g_dropped = container;
    check_expect(item && og_ground_position(og_object(container),&x,&y,&z),"an outdoor drop records its map pixel and a small position within it");
    seam_creatures(&g_camera);
    aim = g_camera; aim.yaw = 0; aim.pitch = df_atan2(y+12-g_camera.y,8);
    check_expect(og_activate(&seam_level()->scene,&aim,OG_MODE_GRAB,0,0,1,160,100) &&
                 og_possessions()->page == OG_VIEW_INVENTORY && og_possessions()->remote == container,
                 "an outdoor pile is drawn through spawn and opens the actual item container when pointed at");
    og_possessions_close();
}
void check_ogworld(const char *root)
{
    char resolved[4096];
    const Usable *door;
    OgLocation place;
    double bx, bz, ax, az;
    float x, y, z;
    int i, held, region, index, opened, status;
    pid_t child;
    const char *test_root = "build/ogfall-world-data/ARENA2";
    seam_land(&g_camera,0,0);
    og_state_capture_motion(); seam_capture_world(); g_before = *og_state(); og_move_capture(&g_motion); og_move_camera(&g_camera);
    fs_set_root("build/ARENA2"); fs_make_dirs("ogfall-world-data");
    check_expect(realpath(root,resolved) != 0,"resolve world test assets"); unlink(test_root);
    check_expect(symlink(resolved,test_root) == 0,"isolate world save writes from game data"); vfs_mount(test_root);
    for (i = 0; i < seam_level()->scene.nmodels && i < 16384; i++) { g_drawn[i] = i; }
    automap_frame(&seam_level()->scene,&g_camera,g_drawn,i);
    opened = og_maps_open(0);
    check_expect(opened && seam_automap_view() != 0 &&
                 automap_polygons(&seam_level()->scene,seam_automap_view(),map_emit,0) > 0,
                 "the indoor map presents actual seen engine geometry");
    map_picture("build/ogfall-dungeon-map.ppm"); map_key(27);
    check_expect(!og_maps_up(),"Escape closes the map without advancing the world");
    recall("anchor");
    door = passage(4); check_expect(door != 0,"the Hold has an actual exit passage");
    if (!door) { goto restore; }
    held = og_dungeon_count();
    check_expect(og_travel_passage(door) && og_travel_asked() && change(),"the actual Hold exit opens the streamed outside");
    check_expect(seam_level()->streamed && seam_level()->sky && stream_active() && seam_level()->scene.nmodels > 0 && !og_dungeon_count() && held > 0,
                 "outside uses engine chunks, terrain and sky, without turning pedestrians into dungeon foes");
    recall("recall");
    check_expect(og_travel_asked() && !og_magic_state()->recall.valid && change() &&
                 og_world_state()->where.kind == OG_PLACE_DUNGEON &&
                 check_near(g_camera.x,g_before.x,0.1f) && check_near(g_camera.z,g_before.z,0.1f),
                 "Recall crosses from the exterior back to the exact saved dungeon anchor");
    door = passage(4);
    check_expect(door && og_travel_passage(door) && change(),"the recalled dungeon can be left again");
    og_move_capture(&g_motion);
    check_expect(og_travel_count() > 15000 && cmd_run("goto Daggerfall") && og_travel_asked() && change(),
                 "goto resolves an actual town name and opens its streamed street");
    check_expect(og_world_state()->where.kind == OG_PLACE_WORLD && og_state()->region == 17,"Daggerfall arrival updates the persistent region");
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
            female_check = og_activate(sc, &look, OG_MODE_INFO, 0, 0, 1, 160, 100);
            check_expect(female_check, "people: info mode did not take a person");
            og_character()->skill[15] = 100;
            og_law()->crime = 0;
            og_activate(sc, &look, OG_MODE_TALK, 0, 0, 1, 160, 100);
            check_expect(og_possessions()->page == OG_VIEW_MESSAGE, "people: talk mode did not greet");
            (void)line;
            og_possessions_close();
        }
    }
    walk_across_origin();
    drop_outside();
    region = index = -1;
    for (i = 0; i < og_travel_count(); i++) {
        og_travel_location(i,&place);
        if (place.type == 6) { region = place.region; index = place.index; break; }
    }
    check_expect(region >= 0 && og_travel_go(region,index) && change(),"goto accepts a real tavern location as well as towns");
    door = passage(1);
    check_expect(door != 0,"a streamed tavern has a building entrance");
    if (door) {
        stream_world(door->cx,door->cz,&bx,&bz);
        check_expect(og_travel_passage(door) && change() && !seam_level()->streamed && !seam_level()->sky && og_world_state()->where.kind == OG_PLACE_INSIDE,
                     "the tavern entrance opens its interior on a separate place heap");
        door = passage(2); check_expect(door && og_travel_passage(door) && change(),"the interior exit returns to the streamed world");
        stream_world(g_camera.x,g_camera.z,&ax,&az);
        check_expect(ax-bx < 200 && ax-bx > -200 && az-bz < 200 && az-bz > -200,"leaving a building lands beside the same world door");
    }
    maps_and_trip();
    /* Save a little past a rebase: reopening centres the stream on a
     * different chunk, so every live logical position must be translated. */
    og_move_place(g_camera.x+32*STREAM_CHUNK,g_camera.y,g_camera.z,0); og_move_camera(&g_camera);
    seam_world_step(&g_camera,0.05f);
    og_move_place(g_camera.x+2*STREAM_CHUNK,g_camera.y,g_camera.z,0); og_move_camera(&g_camera);
    seam_world_step(&g_camera,0.05f);
    {
        OgMissile *missile;
        missile = &og_magic_state()->missile[0]; mem_zero(missile,sizeof(*missile));
        missile->live = 1; missile->spell = 7; missile->level = 10; missile->target = 3;
        missile->x = g_camera.x+20; missile->y = g_camera.y+10; missile->z = g_camera.z+30; missile->dz = 1;
    }
    og_state_pose(g_camera.x,g_camera.y,g_camera.z,og_move_yaw(),0); og_state_capture_motion();
    og_state_save_label("World round trip");
    check_expect(seam_capture_world() && og_save_write(6),"save an outside place with its stream origin");
    child = fork();
    if (!child) { execl("/proc/self/exe","ogfall-test","--ogfall-resume-world",test_root,(char *)0); _exit(127); }
    status = -1;
    if (child > 0) { waitpid(child,&status,0); }
    check_expect(child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,"a fresh process restores the outdoor save and explored map");
    check_expect(og_save_read(6) && seam_open_place(1,0,&x,&y,&z),"load an outside save instead of always reopening the Hold");
    seam_land(&g_camera,1,0);
    check_expect(seam_level()->streamed && og_world_state()->where.kind == OG_PLACE_WORLD,"outside restore retains the streamed place kind");
    check_expect(check_near(og_magic_state()->missile[0].x-g_camera.x,20,0.1f) &&
                 check_near(og_magic_state()->missile[0].z-g_camera.z,30,0.1f),
                 "outside load translates a saved in-flight missile into the reopened stream's frame");
    {
        const OgObject *drop;
        OgWhere anchor;
        float px, py, pz, dx, dy, dz, length;
        drop = og_object(g_dropped);
        check_expect(drop && og_inventory_rows(drop->id,-1,0,0) > 0,"an outdoor pile and its items survive travel and a save/load");
        anchor = og_world_state()->where;
        check_expect(og_ground_position(drop,&px,&py,&pz) &&
                     og_travel_return(&anchor,px,py+WALK_EYE,pz-8,0,0) && change(),
                     "the saved outdoor pile can be revisited from a later stream origin");
        seam_creatures(&g_camera);
        og_ground_position(og_object(g_dropped),&px,&py,&pz);
        dx = px-g_camera.x; dy = py+12-g_camera.y; dz = pz-g_camera.z; length = df_sqrt(dx*dx+dy*dy+dz*dz);
        check_expect(og_ground_aim(g_camera.x,g_camera.y,g_camera.z,dx/length,dy/length,dz/length,128) == g_dropped,
                     "a revisited outdoor pile is visible and pickable at its saved world position");
    }
restore:
    seam_close_hold(); og_state_restore(&g_before); og_state_restore_world(); og_world_restore();
    fs_delete("ogfall\\saves\\S0006.SAV"); fs_remove_dir("ogfall\\saves"); fs_remove_dir("ogfall");
    fs_set_root("build/ARENA2"); unlink(test_root); fs_remove_dir("ogfall-world-data"); vfs_mount(root);
    check_expect(seam_open_place(1,0,&x,&y,&z),"restore the original game after world travel tests");
    seam_land(&g_camera,1,0);
}
