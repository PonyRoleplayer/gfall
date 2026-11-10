#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/actions.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/activate.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/play/travel.h"
#include "game/ogfall/tables/magicdefs.h"
#include "engine/scene/interact.h"
#include "engine/scene/spawn.h"
#include "engine/core/mem.h"

static const Scene *g_scene;
static SpawnId g_visual[OG_MISSILES];
static int g_slots[5][2];
static void (*g_draw)(int, int);
static void (*g_hud)(const OgMagicState *);
static SpawnId g_light;
static int g_light_slot = -1, g_light_w, g_light_h;
static int g_element = -1;
static float g_animation;

void og_magic_set_draw(void (*draw)(int, int)) { g_draw = draw; }
void og_magic_set_hud(void (*draw)(const OgMagicState *)) { g_hud = draw; }
void og_magic_light_sprite(int slot, int width, int height) { g_light_slot = slot; g_light_w = width; g_light_h = height; }
void og_magic_slots(const int slots[5][2]) { mem_copy(g_slots,slots,sizeof(g_slots)); }
void og_magic_close(void)
{
    int i;
    for (i = 0; i < OG_MISSILES; i++) { if (g_visual[i]) { spawn_remove(g_visual[i]); } }
    mem_zero(g_visual,sizeof(g_visual)); g_scene = 0; g_element = -1; g_animation = 0;
    if (g_light) { spawn_remove(g_light); g_light = 0; }
}
void og_magic_open(const Scene *scene) { og_magic_close(); g_scene = scene; }
void og_magic_shift(float dx, float dz)
{
    int i;
    for (i = 0; i < OG_MISSILES; i++) {
        if (og_magic_state()->missile[i].live) {
            og_magic_state()->missile[i].x -= dx; og_magic_state()->missile[i].z -= dz;
        }
    }
}
static void animate(int element)
{
    g_element = element; g_animation = 0; og_weapon_cancel();
}
void og_magic_draw(void)
{
    if (g_element >= 0 && g_draw) { g_draw(g_element,df_ifloor(g_animation*18.2065f)%6); }
    if (g_hud) { g_hud(og_magic_state()); }
}
int og_magic_pending(void) { return og_magic_state()->ready != 0; }
int og_magic_visible(void) { return g_element >= 0 || og_magic_pending(); }
void og_magic_abort(void)
{
    /* Payment and skill use occur when readied, as pickbook.c does. No refund. */
    og_magic_state()->ready = 0; og_msg_set("Spell aborted.");
}

static int line_clear(float x, float y, float z, float tx, float ty, float tz)
{
    UseHit hit;
    float dx, dy, dz, distance;
    if (!g_scene) { return 1; }
    dx = tx-x; dy = ty-y; dz = tz-z; distance = df_sqrt(dx*dx+dy*dy+dz*dz);
    if (distance < 1) { return 1; }
    interact_cast(g_scene,x,y,z,dx/distance,dy/distance,dz/distance,distance,&hit);
    return hit.wall >= distance-1;
}
static void apply_area(const OgSpellDef *s, int caster, int level, float x, float y, float z)
{
    Entity *e;
    Body *body;
    int i;
    float dx, dy, dz, radius;
    radius = (s->icon >= 250 ? 168 : level*4+64)*3;
    for (i = -1; i < og_dungeon_count(); i++) {
        e = og_dungeon_entity(i);
        if (i == -1) { body = og_move_body(); dx = body->feet[0]-x; dy = body->feet[1]+40-y; dz = body->feet[2]-z; }
        else {
            if (e->dead || !e->live) { continue; }
            dx = e->x-x; dy = e->y+40-y; dz = e->z-z;
        }
        if (dx*dx+dy*dy+dz*dz >= radius*radius || !line_clear(x,y,z,x+dx,y+dy,z+dz)) { continue; }
        if (caster == -1 && i == -1 && s->target == 3) { continue; }
        og_magic_apply(s->id,caster,i,level,0);
    }
}
static void visual(int i)
{
    OgMissile *m;
    Entity entity, *live;
    int slot;
    m = &og_magic_state()->missile[i];
    slot = g_slots[m->element][m->burst != 0];
    live = spawn_get(g_visual[i]);
    if (!live) {
        entity_init(&entity); entity.live = 1; entity.glow = 1;
        entity.pose[ENTITY_IDLE].slot[0] = (df_u16)slot;
        entity.pose[ENTITY_IDLE].w[0] = entity.pose[ENTITY_IDLE].h[0] = m->burst ? 128 : 48;
        entity.pose[ENTITY_IDLE].rate = 12;
        g_visual[i] = spawn_add(g_scene,&entity,0); live = spawn_get(g_visual[i]);
    }
    if (live) {
        live->x = m->x; live->y = m->y-24; live->z = m->z;
        live->pose[ENTITY_IDLE].slot[0] = (df_u16)slot;
        live->pose[ENTITY_IDLE].w[0] = live->pose[ENTITY_IDLE].h[0] = m->burst ? 128 : 48;
    }
}
static void step_missile(int index, const Camera *camera, float dt);
static int launch(const OgSpellDef *s, int caster, int level, const Camera *aim)
{
    OgMissile *m;
    Basis basis;
    int i;
    for (i = 0; i < OG_MISSILES; i++) { if (!og_magic_state()->missile[i].live) { break; } }
    if (i == OG_MISSILES || !aim) { return 0; }
    m = &og_magic_state()->missile[i]; mem_zero(m,sizeof(*m));
    m->live = 1; m->caster = og_magic_key(caster); m->spell = s->id; m->level = level;
    if (caster == -1) { og_magic_hostile(); }
    m->element = s->element; m->target = s->target;
    camera_basis(aim,&basis); m->dx = basis.fx*basis.cp; m->dy = basis.sp; m->dz = basis.fz*basis.cp;
    m->x = aim->x; m->y = aim->y; m->z = aim->z;
    /* The original launch advances 110 units. Sweep this too, so a nearby
     * wall or creature cannot be skipped between the hand and that point. */
    og_sound_at(og_cast_sound(s->element),m->x,m->y,m->z);
    step_missile(i,aim,110.0f/400.0f);
    return 1;
}

static int ready(df_u32 id, int recast)
{
    const OgObject *item;
    const OgSpellDef *s;
    OgCharacter *who;
    int cost, bonus, k;
    item = og_object(id); who = og_character();
    if (who->health <= 0 || !item || item->type != OG_RECORD_SPELL || item->parent != og_inventory_book()) { return 0; }
    s = og_spell_def(item->index); if (!s) { return 0; }
    if (who->conditions & 0x100) { og_msg_set("You are silenced."); return 0; }
    cost = og_magicka_cost(recast ? og_magic_state()->paid : og_spell_cost(s,who));
    if (cost > who->magicka+who->spell_points_bonus) { og_msg_set("You do not have enough spell points."); return 0; }
    bonus = cost < who->spell_points_bonus ? cost : who->spell_points_bonus;
    who->spell_points_bonus -= bonus; og_magicka_spend(cost-bonus);
    og_magic_state()->paid = bonus == cost ? 0 : cost; og_magic_state()->last = id;
    og_magic_state()->ready = s->target == 0 ? 0 : id;
    for (k = 0; k < 3; k++) { if (s->group[k] != 255) { og_skill_use(og_effect_school(s->group[k])); } }
    og_state_event(OG_EVENT_SPELL,id,og_state()->entity_id,cost);
    if (s->target == 0) {
        og_sound(og_cast_sound(s->element)); animate(s->element);
        og_magic_apply(s->id,-1,-1,who->level,0);
    } else { og_msg_set("Press the mouse button to cast the spell."); og_weapon_cancel(); }
    return 1;
}
int og_magic_ready(df_u32 id) { return ready(id,0); }
int og_magic_recast(void)
{
    if (og_magic_pending()) { og_msg_set("You already have a spell ready."); return 0; }
    return ready(og_magic_state()->last,1);
}

static void impact(int i, int victim, int model)
{
    OgMissile *m;
    const OgSpellDef *s;
    int caster;
    m = &og_magic_state()->missile[i]; s = og_spell_def(m->spell);
    caster = og_magic_index(m->caster);
    if (!s) { m->live = 0; return; }
    og_sound_at(og_impact_sound(m->element),m->x,m->y,m->z);
    if (model >= 0) { og_actions_trigger(g_scene,model,6); }
    if (m->target == 4) { apply_area(s,caster,m->level,m->x,m->y,m->z); }
    else if (victim >= -1) { og_magic_apply(s->id,caster,victim,m->level,0); }
    m->burst = 1; visual(i);
}
static void step_missile(int index, const Camera *camera, float dt)
{
    OgMissile *m;
    Entity *e;
    Body *body;
    UseHit wall;
    int i, victim, caster;
    float distance, nearest, dx, dy, dz, along, squared, radius;
    m = &og_magic_state()->missile[index]; if (!m->live) { return; }
    if (m->burst) {
        m->burst += df_iround(dt*1000);
        if (m->burst > 600) { m->live = 0; spawn_remove(g_visual[index]); g_visual[index] = 0; }
        else { visual(index); }
        return;
    }
    dx = camera->x-m->x; dy = camera->y-m->y; dz = camera->z-m->z;
    if (dx*dx+dy*dy+dz*dz > 2048*2048) { m->burst = 1; visual(index); return; }
    distance = 400*dt;
    interact_cast(g_scene,m->x,m->y,m->z,m->dx,m->dy,m->dz,distance,&wall);
    nearest = wall.wall; victim = -2; caster = og_magic_index(m->caster);
    for (i = -1; i < og_dungeon_count(); i++) {
        if (i == caster) { continue; }
        e = og_dungeon_entity(i); radius = 60;
        if (i == -1) { body = og_move_body(); dx = body->feet[0]-m->x; dy = body->feet[1]+40-m->y; dz = body->feet[2]-m->z; }
        else { if (e->dead || !e->live) { continue; } dx = e->x-m->x; dy = e->y+40-m->y; dz = e->z-m->z; }
        along = dx*m->dx+dy*m->dy+dz*m->dz;
        squared = dx*dx+dy*dy+dz*dz-along*along;
        if (squared > radius*radius || along < 0) { continue; }
        along -= df_sqrt(radius*radius-squared); if (along < 0) { along = 0; }
        if (along <= nearest && line_clear(m->x,m->y,m->z,m->x+dx,m->y+dy,m->z+dz)) { nearest = along; victim = i; }
    }
    m->x += m->dx*nearest; m->y += m->dy*nearest; m->z += m->dz*nearest;
    if (victim >= -1 || wall.wall < distance) { impact(index,victim,wall.model); }
    else { visual(index); }
}

void og_magic_frame(const Scene *scene, const Camera *camera, const OgIn *input, int blocked)
{
    const OgObject *item;
    const OgSpellDef *s;
    Entity *e;
    Camera aim;
    Basis basis;
    float dx, dy, dz, distance, best, across;
    int i, victim, cast;
    if (!scene || !camera || !input) { return; }
    g_scene = scene;
    og_activate_aim(camera,og_options_mouse() == 0,og_cursor_free(),
                    !input->on_picture || !og_wants_pointer(),input->cx,input->cy,&aim);
    if ((og_character()->conditions & 16) && g_light_slot >= 0) {
        Entity light, *live;
        live = spawn_get(g_light);
        if (!live) {
            entity_init(&light); light.live = light.glow = 1;
            light.pose[ENTITY_IDLE].slot[0] = (df_u16)g_light_slot;
            light.pose[ENTITY_IDLE].w[0] = (df_u16)g_light_w; light.pose[ENTITY_IDLE].h[0] = (df_u16)g_light_h;
            light.pose[ENTITY_IDLE].rate = 12; g_light = spawn_add(scene,&light,0); live = spawn_get(g_light);
        }
        if (live) {
            live->x = camera->x+df_sin(camera->yaw)*192;
            live->y = camera->y-18; live->z = camera->z+df_cos(camera->yaw)*192;
        }
    } else if (g_light) { spawn_remove(g_light); g_light = 0; }
    if (g_element >= 0) { g_animation += input->dt; if (g_animation*18.2065f >= 7) { g_element = -1; } }
    if (og_magic_pending()) {
        item = og_object(og_magic_state()->ready); s = item ? og_spell_def(item->index) : 0;
        if (!s) { og_magic_state()->ready = 0; }
        else if (!blocked) {
            cast = input->mouse_pressed & 3;
            if (s->target == 1) {
                victim = -2; best = 160; camera_basis(&aim,&basis);
                for (i = 0; i < og_dungeon_count(); i++) {
                    e = og_dungeon_entity(i); if (!e->live || e->dead) { continue; }
                    dx = e->x-camera->x; dy = e->y+40-camera->y; dz = e->z-camera->z;
                    distance = dx*basis.fx*basis.cp+dy*basis.sp+dz*basis.fz*basis.cp;
                    across = dx*dx+dy*dy+dz*dz-distance*distance;
                    if (distance < 0 || distance >= best || across > 60*60 ||
                        !line_clear(camera->x,camera->y,camera->z,e->x,e->y+40,e->z)) { continue; }
                    victim = i; best = distance;
                }
                if (cast && victim >= 0) {
                    og_magic_hostile();
                    og_sound(og_cast_sound(s->element)); og_magic_apply(s->id,-1,victim,og_character()->level,0);
                    animate(s->element); og_magic_state()->ready = 0;
                }
            } else if (cast) {
                if (s->target == 3) {
                    og_sound(og_cast_sound(s->element)); apply_area(s,-1,og_character()->level,camera->x,camera->y,camera->z);
                    animate(s->element); og_magic_state()->ready = 0;
                } else if (launch(s,-1,og_character()->level,&aim)) { animate(s->element); og_magic_state()->ready = 0; }
            }
        }
    }
    for (i = 0; i < OG_MISSILES; i++) { step_missile(i,camera,input->dt); }
}

int og_magic_creature(int index, const Camera *camera)
{
    const df_u8 *list;
    const OgSpellDef *s;
    OgCharacter who;
    OgClass career;
    Camera aim;
    Entity *e;
    int base[8], ids[32], n, i, ranged;
    float dx, dz, distance;
    e = og_dungeon_entity(index); list = 0;
    if (!e || !camera || !og_magic_target(index,&who,&career,base) || !who.magicka || (who.conditions & 0x100)) { return 0; }
    list = og_creature_spells(og_dungeon_kind(index),who.level); if (!list) { return 0; }
    dx = camera->x-e->x; dz = camera->z-e->z; distance = df_sqrt(dx*dx+dz*dz); ranged = distance > 360;
    /* ai_creature_think: a ranged spell only past the band's start (4 reaches, 360 for the player); a touch spell only within a blow's reach. Between, the creature closes in. */
    if (!ranged && distance > 90) { return 0; }
    n = 0;
    for (i = 0; list[i] != 255 && i < 32; i++) {
        int held, k;
        s = og_spell_def(list[i]); if (!s) { continue; }
        /* ai_pick_*_spell: nothing is cast that the player already has on them. */
        held = 0;
        for (k = 0; k < OG_EFFECTS; k++) {
            const OgEffect *fx = &og_magic_state()->effect[k];
            if (fx->active && fx->spell == s->id && fx->target.place == og_magic_key(-1).place && fx->target.thing == og_magic_key(-1).thing) { held = 1; }
        }
        if (held) { continue; }
        if (ranged ? (s->target == 2 || s->target == 4) : (s->target == 0 || s->target == 1)) { ids[n++] = s->id; }
    }
    if (!n) { return 0; }
    s = og_spell_def(ids[og_random_range(0,n-1)]);
    if (ranged && og_random_range(0,39)) { return 0; }
    if (!line_clear(e->x,e->y+40,e->z,camera->x,camera->y-20,camera->z)) { return 0; }
    aim.x = e->x; aim.y = e->y+50; aim.z = e->z;
    aim.yaw = df_atan2(dx,dz); aim.pitch = df_atan2(camera->y-20-aim.y,distance);
    if (s->target == 2 || s->target == 4) { if (!launch(s,index,who.level,&aim)) { return 0; } }
    else { og_sound_at(og_cast_sound(s->element),e->x,e->y+40,e->z); og_magic_apply(s->id,index,s->target == 0 ? index : -1,who.level,0); }
    if (og_magic_target(index,&who,&career,base)) {
        who.magicka -= og_spell_cost(s,&who); og_magic_commit(index,&who,index);
    }
    return 1;
}
void og_magic_trap(int spell, float x, float y, float z)
{
    const OgSpellDef *s;
    Body *body;
    Camera aim;
    float dx, dz, distance;
    s = og_spell_def(spell); if (!s) { return; }
    if (s->target == 2 || s->target == 4) {
        body = og_move_body(); dx = body->feet[0]-x; dz = body->feet[2]-z;
        distance = df_sqrt(dx*dx+dz*dz); aim.x = x; aim.y = y; aim.z = z;
        aim.yaw = df_atan2(dx,dz); aim.pitch = df_atan2(body->feet[1]+40-y,distance);
        launch(s,-3,og_character()->level,&aim);
    } else {
        og_sound_at(og_cast_sound(s->element),x,y,z);
        og_magic_apply(spell,s->target == 0 ? -1 : -3,-1,og_character()->level,0);
    }
}

static void recall_answer(const char *answer)
{
    OgRecall *anchor;
    Body *body;
    OgMotion motion;
    anchor = &og_magic_state()->recall;
    if (!answer || !*answer) { return; }
    if (*answer == 'a' || *answer == 'A') {
        og_travel_capture(); og_world_state()->recall_where = og_world_state()->where;
        body = og_move_body(); anchor->valid = 1;
        anchor->place = og_object(OG_WORLD_ROOT)->place_key;
        anchor->x = body->feet[0]; anchor->y = body->feet[1]+body->eye_now; anchor->z = body->feet[2];
        og_move_capture(&motion); anchor->yaw = motion.yaw; anchor->pitch = motion.pitch;
        og_msg_set("Your anchor has been set.");
    } else if (*answer == 'r' || *answer == 'R' || *answer == 't' || *answer == 'T') {
        if (!anchor->valid) { og_msg_set("You have not set an anchor."); return; }
        if (anchor->place == og_object(OG_WORLD_ROOT)->place_key && og_travel_at(&og_world_state()->recall_where)) {
            og_move_place(anchor->x,anchor->y,anchor->z,anchor->yaw);
            og_move_capture(&motion); motion.pitch = anchor->pitch; og_move_restore(&motion);
        } else if (!og_travel_return(&og_world_state()->recall_where,anchor->x,anchor->y,anchor->z,anchor->yaw,anchor->pitch)) {
            og_msg_set("Your anchor's place is unavailable."); return;
        }
        anchor->valid = 0;
        og_msg_set("You return to your anchor.");
    }
}
void og_magic_recall_prompt(void)
{
    og_possessions_message("Recall\n\nType ANCHOR to set an anchor here, or RECALL to return to it.\nThe anchor is consumed when you return.",recall_answer);
}
