#include "game/ogfall/play/weapon.h"
#include "game/ogfall/play/itemmaker.h"
#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/combat.h"
#include "game/ogfall/play/vitals.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/tables/combatdefs.h"
#include "engine/core/mem.h"

static OgWeaponView g_view;
static df_u32 g_item;
static int g_hand, g_step, g_sector, g_bow;
static float g_time, g_reload;
static void (*g_draw)(const OgWeaponView *, float);

int og_weapon_delay(int speed) { int n; n = (115-speed)*3; return n < 1 ? 1 : n; }
int og_bow_delay(int speed) { int n; n = (100-speed)*10+1000; return n < 200 ? 200 : n; }
int og_weapon_hand(void) { return g_hand; }
const OgWeaponView *og_weapon_view(void) { return &g_view; }
void og_weapon_set_draw(void (*draw)(const OgWeaponView *, float)) { g_draw = draw; }
void og_weapon_draw(void) { if (g_draw) { g_draw(&g_view,og_combat_flash()); } }

void og_weapon_cancel(void)
{
    g_step = -1; g_time = 0; g_reload = 0;
    g_view.visible = 0;
}

void og_weapon_reset(void)
{
    mem_zero(&g_view,sizeof(g_view));
    g_hand = og_object(og_state()->entity_id) ? og_object(og_state()->entity_id)->flags & 1 : 0;
    g_item = 0; og_weapon_cancel();
}

void og_weapon_switch(void)
{
    const OgObject *other;
    other = og_object(og_state()->equipped[g_hand ? 19 : 21]);
    /* run_00042F0F refuses a shield in the other hand. An empty hand is fists. */
    if (!og_hud_weapon_ready() || g_step >= 0 || (other && other->group != 3)) { return; }
    g_hand ^= 1;
    if (og_object_edit(og_state()->entity_id)) { og_object_edit(og_state()->entity_id)->flags = g_hand; }
    og_weapon_cancel();
    og_msg_set(g_hand ? "Left hand readied." : "Right hand readied.");
}

int og_weapon_sector(int dx, int dy)
{
    int angle;
    /* XnGine angle 0 points down the screen; clockwise motion has positive X. */
    angle = (int)(df_atan2((float)dx,(float)dy)*2048/DF_TAU);
    return (angle & 2047) >> 7;
}

int og_arrow_count(void)
{
    const OgObject *o;
    int i, count;
    count = 0;
    for (i = 0; i < OG_OBJECTS; i++) {
        o = &og_state()->object[i];
        if (o->id && o->parent == og_state()->inventory_id && o->type == OG_RECORD_ITEM &&
            o->group == 3 && o->index == 18) { count += o->count; }
    }
    return count;
}

int og_arrow_take(void)
{
    const OgObject *o;
    int i;
    for (i = 0; i < OG_OBJECTS; i++) {
        o = &og_state()->object[i];
        if (o->id && o->parent == og_state()->inventory_id && o->type == OG_RECORD_ITEM &&
            o->group == 3 && o->index == 18 && o->count > 0) {
            if (o->count == 1) { og_object_delete(o->id); }
            else { og_object_edit(o->id)->count--; }
            return 1;
        }
    }
    return 0;
}

static void strike_at(OgStrike *s)
{
    s->item = g_item; s->hand = g_hand; s->bow = g_bow;
    s->hit = g_bow ? 0 : og_swing_hit(g_sector);
    s->damage = g_bow ? 0 : og_swing_damage(g_sector);
    og_fatigue_spend(11);
}

void og_weapon_frame(const OgIn *in, int blocked, OgStrike *strike)
{
    const OgObject *item;
    df_u32 id;
    int held, bow, dx, dy, motion, small, frame;
    float delay;
    static const int kick[] = {0,1,2,3,4,2,1,0};
    mem_zero(strike,sizeof(*strike));
    strike->hand = -1;
    id = og_state()->equipped[g_hand ? 21 : 19];
    item = og_object(id);
    if (!item || item->group != 3 || item->index >= 18 || item->condition <= 0) { item = 0; id = 0; }
    bow = item && (item->index == 16 || item->index == 17);
    if (id != g_item || bow != g_bow) { og_weapon_cancel(); g_item = id; g_bow = bow; }
    g_view.cif = og_weapon_cif(item ? item->index : -1);
    g_view.enchanted = item && !bow && og_item_enchanted(id);
    g_view.material = item ? item->material : 0; g_view.hand = g_hand;
    g_view.color = item ? item->color : 0;
    g_view.visible = og_hud_weapon_ready() && og_character()->health > 0;
    if (!g_view.visible || (og_character()->conditions & 1)) { og_weapon_cancel(); return; }
    held = !blocked && in->held[OG_SWING_BUTTON];
    if (g_reload > 0) { g_reload -= in->dt; g_view.visible = 0; return; }
    if (bow) {
        /* 0..3 draw, wait for a new right-button press, 4..6 release. */
        if (!og_arrow_count() && g_step < 4) {
            og_msg_set("You have no arrows."); og_hud_toggle_ready(); og_weapon_cancel(); return;
        }
        if (g_step < 0) { g_step = 0; g_time = 0; }
        g_time += in->dt;
        if (g_step == 3) {
            if (!blocked && (in->pressed[OG_SWING_BUTTON] || (in->mouse_pressed & 2))) {
                og_sound(6); g_step = 4; g_time = 0;
            }
        } else if (g_time >= 1.0f/60) {
            g_time = 0; g_step++;
            if (g_step == 5 && og_arrow_take()) { strike_at(strike); }
            if (g_step >= 7) {
                g_reload = (float)og_bow_delay(og_character()->attribute[OG_SPD])/1000;
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
        g_sector = og_weapon_sector(dx,dy);
        g_step = 0; g_time = 0;
    }
    g_view.group = og_swing_group(g_sector)+1;
    frame = g_step;
    if (!g_item && og_swing_group(g_sector) == 2) { frame = kick[g_step]; }
    g_view.frame = frame;
    delay = (float)og_weapon_delay(og_character()->attribute[OG_SPD])/1000;
    g_time += in->dt;
    while (g_time >= delay && g_step >= 0) {
        g_time -= delay; g_step++;
        if (g_step == ((!g_item && og_swing_group(g_sector) == 2) ? 4 : 2)) { strike_at(strike); }
        if (g_step >= ((!g_item && og_swing_group(g_sector) == 2) ? 8 : 5)) {
            g_step = -1;
        }
    }
}
