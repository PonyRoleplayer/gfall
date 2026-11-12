#include "game/dagger/play/weapon.h"
#include "game/dagger/play/itemmaker.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/vitals.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/core/mem.h"

static WeaponView g_view;
static df_u32 g_item;
static int g_hand, g_step, g_sector, g_bow;
static float g_time, g_reload;
static void (*g_draw)(const WeaponView *, float);

int weapon_delay(int speed) { int n; n = (115-speed)*3; return n < 1 ? 1 : n; }
int bow_delay(int speed) { int n; n = (100-speed)*10+1000; return n < 200 ? 200 : n; }
int weapon_hand(void) { return g_hand; }
const WeaponView *weapon_view(void) { return &g_view; }
void weapon_set_draw(void (*draw)(const WeaponView *, float)) { g_draw = draw; }
void weapon_draw(void) { if (g_draw) { g_draw(&g_view,combat_flash()); } }

void weapon_cancel(void)
{
    g_step = -1; g_time = 0; g_reload = 0;
    g_view.visible = 0;
}

void weapon_reset(void)
{
    mem_zero(&g_view,sizeof(g_view));
    g_hand = object_at(game_state()->entity_id) ? object_at(game_state()->entity_id)->flags & 1 : 0;
    g_item = 0; weapon_cancel();
}

void weapon_switch(void)
{
    const ObjectInfo *other;
    other = object_at(game_state()->equipped[g_hand ? 19 : 21]);
    /* run_00042F0F refuses a shield in the other hand. An empty hand is fists. */
    if (!hud_weapon_ready() || g_step >= 0 || (other && other->group != 3)) { return; }
    g_hand ^= 1;
    if (object_edit(game_state()->entity_id)) { object_edit(game_state()->entity_id)->flags = g_hand; }
    weapon_cancel();
    msg_set(g_hand ? "Left hand readied." : "Right hand readied.");
}

int weapon_sector(int dx, int dy)
{
    int angle;
    /* XnGine angle 0 points down the screen; clockwise motion has positive X. */
    angle = (int)(df_atan2((float)dx,(float)dy)*2048/DF_TAU);
    return (angle & 2047) >> 7;
}

int arrow_count(void)
{
    const ObjectInfo *o;
    int i, count;
    count = 0;
    for (i = 0; i < OBJECTS; i++) {
        o = &game_state()->object[i];
        if (o->id && o->parent == game_state()->inventory_id && o->type == RECORD_ITEM &&
            o->group == 3 && o->index == 18) { count += o->count; }
    }
    return count;
}

int arrow_take(void)
{
    const ObjectInfo *o;
    int i;
    for (i = 0; i < OBJECTS; i++) {
        o = &game_state()->object[i];
        if (o->id && o->parent == game_state()->inventory_id && o->type == RECORD_ITEM &&
            o->group == 3 && o->index == 18 && o->count > 0) {
            if (o->count == 1) { object_delete(o->id); }
            else { object_edit(o->id)->count--; }
            return 1;
        }
    }
    return 0;
}

static void strike_at(StrikeInfo *s)
{
    s->item = g_item; s->hand = g_hand; s->bow = g_bow;
    s->hit = g_bow ? 0 : swing_hit(g_sector);
    s->damage = g_bow ? 0 : swing_damage(g_sector);
    fatigue_spend(11);
}

void weapon_frame(const PlayIn *in, int blocked, StrikeInfo *strike)
{
    const ObjectInfo *item;
    df_u32 id;
    int held, bow, dx, dy, motion, small, frame;
    float delay;
    static const int kick[] = {0,1,2,3,4,2,1,0};
    mem_zero(strike,sizeof(*strike));
    strike->hand = -1;
    id = game_state()->equipped[g_hand ? 21 : 19];
    item = object_at(id);
    if (!item || item->group != 3 || item->index >= 18 || item->condition <= 0) { item = 0; id = 0; }
    bow = item && (item->index == 16 || item->index == 17);
    if (id != g_item || bow != g_bow) { weapon_cancel(); g_item = id; g_bow = bow; }
    g_view.cif = weapon_cif(item ? item->index : -1);
    g_view.enchanted = item && !bow && item_enchanted(id);
    g_view.material = item ? item->material : 0; g_view.hand = g_hand;
    g_view.color = item ? item->color : 0;
    g_view.visible = hud_weapon_ready() && character_state()->health > 0;
    if (!g_view.visible || (character_state()->conditions & 1)) { weapon_cancel(); return; }
    held = !blocked && in->held[SWING_BUTTON];
    if (g_reload > 0) { g_reload -= in->dt; g_view.visible = 0; return; }
    if (bow) {
        /* 0..3 draw, wait for a new right-button press, 4..6 release. */
        if (!arrow_count() && g_step < 4) {
            msg_set("You have no arrows."); hud_toggle_ready(); weapon_cancel(); return;
        }
        if (g_step < 0) { g_step = 0; g_time = 0; }
        g_time += in->dt;
        if (g_step == 3) {
            if (!blocked && (in->pressed[SWING_BUTTON] || (in->mouse_pressed & 2))) {
                play_sound(6); g_step = 4; g_time = 0;
            }
        } else if (g_time >= 1.0f/60) {
            g_time = 0; g_step++;
            if (g_step == 5 && arrow_take()) { strike_at(strike); }
            if (g_step >= 7) {
                g_reload = (float)bow_delay(character_state()->attribute[ATTR_SPD])/1000;
                g_step = -1; g_view.visible = 0; return;
            }
        }
        g_view.group = 0; g_view.frame = g_step;
        return;
    }
    if (g_step < 0) {
        g_view.group = 0; g_view.frame = 0;
        if (!held) { return; }
        dx = in->mouse_dx*320/df_view_w(); dy = in->mouse_dy*320/df_view_w();
        motion = dx < 0 ? -dx : dx; small = dy < 0 ? -dy : dy;
        if (small > motion) { frame = small; small = motion; motion = frame; }
        if (motion+small/2 < 45) { return; }
        g_sector = weapon_sector(dx,dy);
        g_step = 0; g_time = 0;
    }
    g_view.group = swing_group(g_sector)+1;
    frame = g_step;
    if (!g_item && swing_group(g_sector) == 2) { frame = kick[g_step]; }
    g_view.frame = frame;
    delay = (float)weapon_delay(character_state()->attribute[ATTR_SPD])/1000;
    g_time += in->dt;
    while (g_time >= delay && g_step >= 0) {
        g_time -= delay; g_step++;
        if (g_step == ((!g_item && swing_group(g_sector) == 2) ? 4 : 2)) { strike_at(strike); }
        if (g_step >= ((!g_item && swing_group(g_sector) == 2) ? 8 : 5)) {
            g_step = -1;
        }
    }
}
