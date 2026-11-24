#include "game/dagger/play/combat/weapon.h"
#include "game/dagger/play/config.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/character/vitals.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/core/mem.h"
#include "game/dagger/play/input/move.h"

static WeaponView g_view;
static df_u32 g_item;
static int g_hand, g_step, g_sector, g_bow, g_draw_bow;
static float g_time, g_reload;
static void (*g_draw)(const WeaponView *, float);

int weapon_delay(int speed) { int n; n = (SWING_SPEED_BASE-speed)*SWING_FRAME_MS; return n < 1 ? 1 : n; }
int bow_delay(int speed) { int n; n = (BOW_RELOAD_SPEED_BASE-speed)*BOW_RELOAD_PER_SPD+BOW_RELOAD_BASE; return n < BOW_RELOAD_FLOOR ? BOW_RELOAD_FLOOR : n; }
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
    other = object_at(game_state()->equipped[g_hand ? EQUIP_RIGHT_HAND : EQUIP_LEFT_HAND]);
    /* run_00042F0F refuses a shield in the other hand. An empty hand is fists. */
    if (!hud_weapon_ready() || g_step >= 0 || (other && other->group != ITEM_GROUP_WEAPON)) { return; }
    g_hand ^= 1;
    if (object_edit(game_state()->entity_id)) { object_edit(game_state()->entity_id)->flags = g_hand; }
    weapon_cancel();
    msg_set(g_hand ? "Left hand readied." : "Right hand readied.");
}

int weapon_sector(int dx, int dy)
{
    int angle;
    /* XnGine angle 0 points down the screen; clockwise motion has positive X. */
    angle = (int)(df_atan2((float)dx,(float)dy)*ANGLE_FULL/DF_TAU);
    return (angle & (ANGLE_FULL - 1)) >> SWING_SECTOR_SHIFT;
}

int arrow_count(void)
{
    const ObjectInfo *o;
    int i, count;
    count = 0;
    for (i = 0; i < OBJECTS; i++) {
        o = &game_state()->object[i];
        if (o->id && o->parent == game_state()->inventory_id && o->type == RECORD_ITEM &&
            o->group == ITEM_GROUP_WEAPON && o->index == WEAPON_ARROW) { count += o->count; }
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
            o->group == ITEM_GROUP_WEAPON && o->index == WEAPON_ARROW && o->count > 0) {
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
    curse_attack_sound();
    s->hit = g_bow ? 0 : swing_hit(g_sector);
    s->damage = g_bow ? 0 : swing_damage(g_sector);
    fatigue_spend(FATIGUE_PER_BLOW);
}

static const ObjectInfo *hand_weapon(void)
{
    const ObjectInfo *item = object_at(game_state()->equipped[g_hand ? EQUIP_LEFT_HAND : EQUIP_RIGHT_HAND]);

    return item && item->group == ITEM_GROUP_WEAPON && item->index < WEAPON_ARROW && item->condition > 0 ? item : 0;
}

static int is_bow(const ObjectInfo *item)
{
    return item && (item->index == WEAPON_SHORT_BOW || item->index == WEAPON_LONG_BOW);
}

int weapon_hold_to_draw(void)
{
    return config_bow_draw() && is_bow(hand_weapon());
}

static void bow_frame(const PlayIn *in, int blocked, StrikeInfo *strike)
{
    int waiting;

    if (!arrow_count() && g_step <= BOW_FRAME_WAIT) {
        msg_set("You have no arrows."); hud_toggle_ready(); weapon_cancel(); return;
    }
    g_view.group = 0;
    g_view.frame = 0;
    if (g_draw_bow && blocked && g_step < BOW_FRAME_SHOOT) { g_step = -1; }
    if (g_step < 0) {
        if (g_draw_bow && (blocked || !in->pressed[SWING_BUTTON])) { return; }
        g_step = 0; g_time = 0;
    }
    waiting = g_step == BOW_FRAME_WAIT;
    if (!waiting) {
        g_time += in->dt;
        while (g_time >= BOW_FRAME_SECONDS && g_step != BOW_FRAME_WAIT) {
            g_time = g_draw_bow ? g_time - BOW_FRAME_SECONDS : 0;
            g_step++;
            if (g_step == BOW_FRAME_SHOOT && arrow_take()) { strike_at(strike); }
            if (g_step >= BOW_FRAME_END) {
                /* The original counts the reload down to BOW_RELOAD_FLOOR. */
                g_reload = (float)(bow_delay(character_state()->attribute[ATTR_SPD]) - BOW_RELOAD_FLOOR)/1000;
                g_step = -1; g_view.visible = 0; return;
            }
        }
    }
    /* An early release finishes the draw before the shot. Classic waits for a press. */
    if (g_step == BOW_FRAME_WAIT && !blocked &&
        (g_draw_bow ? !in->held[SWING_BUTTON] : waiting && (in->pressed[SWING_BUTTON] || (in->mouse_pressed & 2)))) {
        play_sound(SND_BOW); g_step++; g_time = 0;
    }
    g_view.frame = g_step;
}

void weapon_frame(const PlayIn *in, int blocked, StrikeInfo *strike)
{
    const ObjectInfo *item;
    df_u32 id;
    int held, bow, draw, dx, dy, motion, small, frame;
    float delay;
    static const int kick[] = {0,1,2,3,4,2,1,0};
    mem_zero(strike,sizeof(*strike));
    strike->hand = -1;
    item = hand_weapon();
    id = item ? item->id : 0;
    bow = is_bow(item);
    draw = bow && config_bow_draw();
    if (id != g_item || bow != g_bow || draw != g_draw_bow) { weapon_cancel(); g_item = id; g_bow = bow; g_draw_bow = draw; }
    /* A beast fights with claws (WEAPON11.CIF, the fists are 10): its hands were emptied when it changed. */
    g_view.cif = curse_beast() ? CIF_FISTS_BEAST : weapon_cif(item ? item->index : -1);
    g_view.enchanted = item && !bow && item_enchanted(id);
    g_view.material = item ? item->material : 0; g_view.hand = g_hand;
    g_view.color = item ? item->color : 0;
    g_view.visible = hud_weapon_ready() && character_state()->health > 0;
    if (!g_view.visible || (character_state()->conditions & COND_PARALYZED)) { weapon_cancel(); return; }
    held = !blocked && in->held[SWING_BUTTON];
    if (g_reload > 0) { g_reload -= in->dt; g_view.visible = 0; return; }
    if (bow) { bow_frame(in,blocked,strike); return; }
    if (g_step < 0) {
        g_view.group = 0; g_view.frame = 0;
        if (!held) { return; }
        dx = in->mouse_dx*CLASSIC_WIDTH/df_view_w(); dy = in->mouse_dy*CLASSIC_WIDTH/df_view_w();
        motion = dx < 0 ? -dx : dx; small = dy < 0 ? -dy : dy;
        if (small > motion) { frame = small; small = motion; motion = frame; }
        if (motion+small/2 < SWING_MOTION_MIN) { return; }
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
        if (g_step == ((!g_item && swing_group(g_sector) == 2) ? KICK_STRIKE_FRAME : SWING_STRIKE_FRAME)) { strike_at(strike); }
        if (g_step >= ((!g_item && swing_group(g_sector) == 2) ? KICK_END_FRAME : SWING_END_FRAME)) {
            g_step = -1;
        }
    }
}
