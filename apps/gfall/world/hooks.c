#include "game/dagger/play/state.h"
#include "game/dagger/play/travel/travel.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/scene/stream.h"

#include "game/dagger/formats/dungeon.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/formats/worldmap.h"

#include "world.h"
#include "../buildings/buildings.h"
/* Map records and placement remain behind this adapter. GFALL supplies its
 * own journey rules, object state and UI; the shared layer builds geometry. */
static int location_catalogue(void)
{
    return worldmap_open() ? worldmap_locations() : 0;
}
static int location_record(int ordinal, LocationInfo *out)
{
    if (!worldmap_location_info(ordinal,&out->px,&out->py,&out->region,&out->index,&out->type,&out->known)) { return 0; }
    df_copy(out->name,worldmap_location_name(ordinal),(int)sizeof(out->name)); return 1;
}
static int go_to_location(int region, int index)
{
    int px, py, type;
    if (!worldmap_open() || !worldmap_pixel_of(region,index,&px,&py)) { return 0; }
    type = worldmap_location_type(px,py);
    return travel_go_to_place(region,index,type == 4 || type == 7 || type == 10 || type == 12 || type == 13);
}
static int classic_angle(float radians)
{
    int angle;
    angle = df_iround(radians*2048/DF_TAU); return angle & 2047;
}
static void current_place(PlaceWhere *out, int *px, int *py)
{
    TravelWhere here;
    Camera camera;
    const Dungeon *data;
    int origin[2], exterior, region, index;
    if (!seam_level()) { return; }
    travel_here(&here,origin); move_camera(&camera);
    out->kind = here.kind; out->located = here.located; out->region = here.region; out->index = here.index;
    out->block = here.block; out->record = here.record; out->has_back = here.has_back;
    out->back_x = (int)here.back_x; out->back_z = (int)here.back_z; out->back_yaw = classic_angle(here.back_yaw);
    out->origin_x = origin[0]; out->origin_z = origin[1];
    if (here.kind == TRAVEL_WORLD) {
        if (!travel_pixel(&camera,px,py)) { return; }
        if (worldmap_location(*px,*py,&region,&index)) {
            out->region = region; out->index = index; out->located = 1;
            travel_discover(worldmap_location_id(*px,*py));
        }
    } else {
        data = travel_place_data(); exterior = here.index;
        if (here.kind == TRAVEL_DUNGEON) { exterior = dungeon_outside(here.region,data->exterior_id); }
        if (exterior >= 0) { worldmap_pixel_of(here.region,exterior,px,py); }
    }
    if (game_state()->region != out->region || game_state()->location != out->index || game_state()->environment != here.kind+1) {
        state_set_place(out->region,out->index,here.kind+1);
    }
}
static int return_to_anchor(const PlaceWhere *saved, float x, float eye_y, float z, int yaw, int pitch)
{
    TravelWhere where;
    int origin[2];
    float feet[3];
    mem_zero(&where,sizeof(where));
    where.kind = saved->kind; where.located = saved->located; where.region = saved->region; where.index = saved->index;
    where.block = saved->block; where.record = saved->record; where.has_back = saved->has_back;
    where.back_x = saved->back_x; where.back_z = saved->back_z; where.back_yaw = saved->back_yaw*(DF_TAU/2048);
    origin[0] = saved->origin_x; origin[1] = saved->origin_z;
    feet[0] = x; feet[1] = eye_y-WALK_EYE; feet[2] = z;
    if (where.kind == TRAVEL_WORLD) {
        /* Open the stream at the anchor, not at the old building's doorstep. */
        where.has_back = 1;
        where.back_x = (double)x+(double)origin[0]*STREAM_CHUNK;
        where.back_z = (double)z+(double)origin[1]*STREAM_CHUNK;
    }
    travel_to_pose(&where,feet,origin,yaw*(DF_TAU/2048),pitch*(DF_TAU/2048)); return 1;
}
void gfall_world_install(void)
{
    TravelHooks hooks;
    mem_zero(&hooks,sizeof(hooks));
    hooks.count = location_catalogue; hooks.location = location_record; hooks.go = go_to_location;
    hooks.asked = travel_asked; hooks.passage = gfall_building_passage; hooks.here = current_place;
    hooks.climate = worldmap_climate; hooks.return_to = return_to_anchor; travel_install(&hooks);
}
