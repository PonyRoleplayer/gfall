#include "game/dagger/play/state.h"
#include "game/dagger/play/ui/screen.h"
#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/travel/maps.h"
#include "../seam.h"
#include "../buildings/buildings.h"
#include "../sound.h"
#include "screen.h"
#include "callbacks.h"

#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"

#include "game/dagger/places/travel.h"
#include "game/dagger/play/travel/travelmap.h"
#include "game/dagger/play/travel/dunmap.h"
#include "game/dagger/play/travel/townmap.h"
#include "game/dagger/play/travel/marker.h"

#include "game/dagger/play/play.h"

#include "internal.h"
#include "game/dagger/play/ui/canvas.h"

static int g_maps_mode;
static df_u8 g_maps_rgb[768];
/* Classic map presenters accept GFALL data callbacks. They never receive an
 * GFALL character disguised as a GFALL player. */
static int g_map_pic, g_map_marker, g_map_marker_step = -1;
static int g_map_x, g_map_y, g_map_drag;
static float g_map_repeat;
static unsigned map_gold(void) { return (unsigned)character_state()->gold; }
static const char *map_warning(void) { return journey_warning() ? words_state(1010) : 0; }
static void map_question(const char *place, char *out, int capacity)
{
    const char *text;
    int n, i;
    text = words_state(31); n = 0;
    while (*text && n+1 < capacity) {
        if (text[0] == '%' && text[1] == 't' && text[2] == 'c' && text[3] == 'n') {
            for (i = 0; place[i] && n+1 < capacity; i++) { out[n++] = place[i]; }
            text += 4;
        } else { out[n++] = *text++; }
    }
    out[n] = 0;
}
static void map_journey(int sx, int sy, int ex, int ey, int reckless, int inns, int ship, Journey *out)
{
    JourneyPlan trip;
    journey_plan(sx,sy,ex,ey,reckless,!inns,ship,journey_transport(),&trip);
    mem_zero(out,sizeof(*out)); out->minutes = trip.minutes; out->hours = trip.hours;
    out->days = (trip.minutes+1439)/1440; out->sea_pixels = trip.sea_pixels; out->cost = trip.cost;
}
static void close_maps(void)
{
    dunmap_close(); townmap_close(); travelmap_close();
    if (g_map_pic) { pic_free(g_map_pic); g_map_pic = 0; }
    if (g_map_marker) { pic_free(g_map_marker); g_map_marker = 0; }
    g_map_marker_step = -1; g_maps_mode = g_map_drag = 0;
    pic_set(PIC_SCREEN,0);
}
/* The map opened to teleport (the guild's mode 100): a place chosen is arrived at, not travelled to. */
static int g_teleport;
static int open_maps(int trip)
{
    TravelMapRules rules;
    Camera camera;
    int region, index, block, record;
    float x, z;
    travel_capture(); move_camera(&camera);
    g_map_x = g_map_y = -1; g_map_drag = 0; g_map_repeat = 0;
    g_teleport = trip == 100;
    if (trip && !g_teleport && world_where()->where.kind != WHERE_WORLD) { return 0; }
    if (!trip && world_where()->where.kind != WHERE_WORLD) {
        if (!dunmap_open(move_body()->feet,camera.yaw)) { return 0; }
        g_maps_mode = 1;
    } else if (!trip && travel_town_here(&camera,&region,&index,&x,&z,&block,&record) && townmap_open(region,index,x,z,camera.yaw)) {
        g_maps_mode = 2;
    } else {
        rules.gold = map_gold; rules.known = travel_known; rules.plan = map_journey;
        rules.question = map_question; rules.warning = map_warning;
        if (!travelmap_open_rules(&rules,world_where()->px,world_where()->py,game_state()->region)) { return 0; }
        g_maps_mode = 3;
    }
    gfall_screen_clear_composition();
    pic_set(PIC_SCREEN,0); return 1;
}
static int frame_maps(const PlayIn *input)
{
    int result, dx, dy, pressed, repeat;
    result = 0; pressed = input->mouse_pressed & 3;
    if (input->reset) { g_map_drag = 0; g_map_x = g_map_y = -1; return 0; }
    if (g_maps_mode != 3 && input->pressed[AUTOMAP]) { return 1; }
    if (g_maps_mode == 2 && input->pressed[TRAVEL]) { close_maps(); return !open_maps(1); }
    if (input->key) {
        result = g_maps_mode == 1 ? dunmap_key(input->key) : g_maps_mode == 2 ? townmap_key(input->key) : travelmap_key(input->key,input->ch);
    }
    if (g_maps_mode == 3) {
        if (input->on_picture) { travelmap_point(input->cx,input->cy); }
        if (pressed == 1 && input->on_picture) { result = travelmap_click(input->cx,input->cy); gfall_play_sound(203); }
        if (travelmap_step(input->dt) == TRAVELMAP_GO) {
            int region, index, cautious, reckless, camp, ship;
            Journey journey;
            travelmap_chosen(&region,&index,&journey,&cautious); travelmap_options(&reckless,&camp,&ship);
            if (g_teleport ? teleport(region,index) : journey_begin(region,index,reckless,camp,ship)) { return 1; }
        }
        return result == TRAVELMAP_LEAVE;
    }
    if (result == TOWNMAP_TRAVEL && g_maps_mode == 2) { close_maps(); return !open_maps(1); }
    if (result) { return 1; }
    if (input->wheel) { if (g_maps_mode == 1) { dunmap_wheel(input->wheel); } else { townmap_wheel(input->wheel); } }
    if (g_maps_mode == 2 && input->on_picture) { townmap_point(input->cx,input->cy); }
    repeat = g_maps_mode == 1 ? dunmap_repeats(input->cx,input->cy) : townmap_repeats(input->cx,input->cy);
    if (pressed && input->on_picture) {
        if (pressed == 1) {
            result = g_maps_mode == 1 ? dunmap_click(input->cx,input->cy) : townmap_click(input->cx,input->cy);
            g_map_drag = (g_maps_mode == 1 ? dunmap_on_map(input->cx,input->cy) : townmap_on_map(input->cx,input->cy)) ? 1 : 0;
        } else if (g_maps_mode == 1 && dunmap_on_map(input->cx,input->cy)) { g_map_drag = 2; }
        gfall_play_sound(203); g_map_repeat = 0;
    }
    if (!(input->mouse_held & 3) || !input->on_picture) { g_map_drag = 0; }
    if (g_map_drag && g_map_x >= 0 && !pressed) {
        dx = input->cx-g_map_x; dy = input->cy-g_map_y;
        if (g_maps_mode == 1) { if (g_map_drag == 2) { dunmap_turn_by(dx); } else { dunmap_drag(dx,dy); } }
        else { townmap_drag(dx,dy); }
    }
    if (repeat && (input->mouse_held & 1) && input->on_picture) {
        g_map_repeat += input->dt;
        while (g_map_repeat >= 1.0f/18.2065f) {
            g_map_repeat -= 1.0f/18.2065f;
            result = g_maps_mode == 1 ? dunmap_click(input->cx,input->cy) : townmap_click(input->cx,input->cy);
        }
    }
    g_map_x = input->cx; g_map_y = input->cy;
    return result != 0;
}
const AutomapView *seam_automap_view(void)
{
    static AutomapView view;
    int x, top, w, h;
    if (!maps_up() || g_maps_mode != 1) { return 0; }
    view = *dunmap_view(); screen_rect(df_view_w(),df_view_h(),&x,&top,&w,&h);
    view.x0 = x+view.x0*w/df_view_w(); view.x1 = x+view.x1*w/df_view_w();
    view.y0 = top-h+view.y0*h/df_view_h(); view.y1 = top-h+view.y1*h/df_view_h();
    view.scale *= (float)w/df_view_w(); return &view;
}
static void map_overlay(void)
{
    const AutomapView *view;
    float sx, sy, angle, ax, ay;
    int x, top, w, h, step, visible;
    df_u8 pixels[MARKER_SIDE*MARKER_SIDE];
    visible = 0; angle = 0;
    if (g_maps_mode == 1) {
        view = seam_automap_view();
        visible = automap_to_screen(view,move_body()->feet[0],move_body()->feet[1],move_body()->feet[2],&sx,&sy);
        if (visible && automap_to_screen(view,move_body()->feet[0]+df_sin(move_yaw()*DF_TAU/2048)*64,
             move_body()->feet[1],move_body()->feet[2]+df_cos(move_yaw()*DF_TAU/2048)*64,&ax,&ay)) { angle = df_atan2(ax-sx,ay-sy); }
    } else if (g_maps_mode == 2) {
        visible = townmap_marker(&sx,&sy,&angle);
        screen_rect(df_view_w(),df_view_h(),&x,&top,&w,&h);
        sx = x+sx*w/df_view_w(); sy = top-h+sy*h/df_view_h();
    }
    if (!visible) { return; }
    step = marker_step(angle);
    if (!g_map_marker || step != g_map_marker_step) {
        marker_compose(pixels,step);
        if (!g_map_marker) { g_map_marker = pic_load(pixels,MARKER_SIDE,MARKER_SIDE,gfall_screen_art_palette(),1); }
        else { pic_update(g_map_marker,pixels); }
        g_map_marker_step = step;
    }
    w = df_view_w()*8/320; pic_draw(g_map_marker,df_iround(sx)-w/2,df_iround(sy)+w/2,w,w);
}
void gfall_screen_install_maps(void)
{
    MapsHooks hooks;
    hooks.open = open_maps; hooks.close = close_maps; hooks.frame = frame_maps; hooks.overlay = map_overlay;
    maps_install(&hooks);
    townmap_set_building_name(gfall_building_map_name);
}
void gfall_screen_present_maps(void)
{
    const df_u8 *pixels;
    int changed;
    if (g_maps_mode == 1) { changed = dunmap_changed(); if (changed || !g_map_pic) { dunmap_compose(); } pixels = dunmap_pixels(); }
    else if (g_maps_mode == 2) { changed = townmap_changed(); if (changed || !g_map_pic) { townmap_compose(); } pixels = townmap_pixels(); }
    else { changed = travelmap_changed(); if (changed || !g_map_pic) { travelmap_compose(); } pixels = travelmap_pixels(); }
    if (!changed && g_map_pic) { return; }
    if (g_maps_mode == 3) { travelmap_palette(g_maps_rgb); } else { mem_copy(g_maps_rgb,gfall_screen_art_palette(),768); }
    if (!g_map_pic) { g_map_pic = pic_load(pixels,CANVAS_W,CANVAS_H,g_maps_rgb,g_maps_mode == 1); }
    else { pic_update(g_map_pic,pixels); }
    pic_set(PIC_SCREEN,g_map_pic);
    gfall_screen_copy_map(pixels);
}

const df_u8 *gfall_screen_map_palette(void) { return g_maps_rgb; }
