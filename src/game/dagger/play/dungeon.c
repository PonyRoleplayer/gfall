#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/enchant.h"
#include "game/dagger/play/quest.h"
#include "game/dagger/play/guild.h"
#include "game/dagger/play/doors.h"
#include "game/dagger/play/ai.h"
#include "game/dagger/play/actions.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/ailments.h"
#include "game/dagger/play/anim.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/foegear.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/tables/combatdefs.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/magicdefs.h"
#include "engine/scene/collide.h"
#include "engine/scene/interact.h"
#include "engine/scene/spawn.h"
#include "engine/core/mem.h"

typedef struct FoeState {
    Entity *entity;
    const FoeDef *def;
    FighterInfo fighter;
    Body body;
    Mind mind;
    df_u32 equipped[27];
    Anim anim;
    const AnimScript *script;
    int kind, type, passive, awake, rate, loaded, restored;
    float attack_wait, hurt, knock, knock_yaw, think, voice_time;
} FoeState;

static FoeState g_foe[FOES];
static ArrowInfo g_arrow[ARROWS];
static int g_count, g_blood[2];
static const Scene *g_scene;
static int (*g_street_hit)(const Camera *cam);
static void (*g_street_died)(int kind, Entity *entity);
static SpawnId g_splash[8];
static float g_splash_time[8];

static float minimum(float a, float b) { return a < b ? a : b; }
static float maximum(float a, float b) { return a > b ? a : b; }
static void write32(df_u8 *p, df_u32 value)
{
    int k;
    for (k = 0; k < 4; k++) { p[k] = (df_u8)(value >> (k*8)); }
}

static void store_health(FoeState *f)
{
    Keep *row;
    /* A person of the street taken into the watch has no key of its own (0, 0): a row for it could never be put back by a save, and would stop the place being restored. */
    if (!f->entity->key.place && !f->entity->key.thing) { return; }
    row = keep_get(f->entity->key,KEEP_ENTITY);
    if (!row) { return; }
    row->game[0] = 0xc1; row->game[1] = (df_u8)(f->awake | (f->passive << 1));
    row->game[2] = (df_u8)f->mind.blocked;
    row->game[3] = (df_u8)(f->mind.direction | (f->mind.turns << 4));
    write32(row->game+4,(df_u32)f->fighter.health);
    write32(row->game+8,(df_u32)f->fighter.maximum);
    row->game[12] = (df_u8)f->mind.give_up;
    row->game[13] = (df_u8)f->mind.stuck;
    row->game[14] = (df_u8)f->mind.detected;
    row->game[15] = (df_u8)f->mind.encountered;
}

void dungeon_alert(int source)
{
    FoeState *f;
    Body *player;
    float dx, dy, dz;
    int i;

    player = move_body();
    for (i = 0; i < g_count; i++) {
        f = &g_foe[i];
        if (f->entity->dead || (f->passive && i != source && !f->awake)) { continue; }
        dx = f->entity->x-player->feet[0]; dy = f->entity->y-player->feet[1];
        dz = f->entity->z-player->feet[2];
        if (i == source || ai_stealth(f->mind.detected,df_iround(df_sqrt(dx*dx+dy*dy+dz*dz)),f->mind.encountered)) {
            f->awake = f->mind.detected = f->mind.encountered = 1;
            f->mind.give_up = 200; store_health(f);
        }
    }
}

int dungeon_health(int i) { return i >= 0 && i < g_count ? g_foe[i].fighter.health : 0; }
int dungeon_loot_list(int i) { return i >= 0 && i < g_count ? foe_loot(g_foe[i].kind) : -1; }
const ArrowInfo *dungeon_arrow(int i) { return i >= 0 && i < ARROWS ? &g_arrow[i] : 0; }
void dungeon_shift(float dx, float dz)
{
    int i;
    for (i = 0; i < ARROWS; i++) {
        if (g_arrow[i].live) { g_arrow[i].at[0] -= dx; g_arrow[i].at[2] -= dz; }
    }
    /* A creature of a town (the watch) keeps its own body, which moves with the stream's origin as the entity does. */
    for (i = 0; i < g_count; i++) {
        g_foe[i].body.feet[0] -= dx; g_foe[i].body.feet[2] -= dz;
    }
}
void dungeon_variant(int i, int type)
{
    if (i >= 0 && i < g_count && type >= 0 && type < 60) {
        g_foe[i].type = type; g_foe[i].script = foe_type_script(type);
    }
}

static int visible(const Scene *scene, const float *from, const float *to);

int dungeon_nearby(const Camera *cam, float reach)
{
    const Entity *e;
    float dx, dy, dz;
    int i;
    if (!cam) { return 0; }
    for (i = 0; i < g_count; i++) {
        e = g_foe[i].entity;
        if (e->dead || (g_foe[i].passive && !g_foe[i].awake)) { continue; }
        dx = e->x-cam->x; dy = e->y+40-cam->y; dz = e->z-cam->z;
        if (dx*dx+dy*dy+dz*dz >= reach*reach) { continue; }
        /* A sleeper behind a wall does not keep the player from resting; the original counts the creatures round the player, which is every one in the cells loaded. */
        if (!g_foe[i].awake && g_scene) {
            float from[3], to[3];
            from[0] = e->x; from[1] = e->y+40; from[2] = e->z;
            to[0] = cam->x; to[1] = cam->y; to[2] = cam->z;
            if (!visible(g_scene,from,to)) { continue; }
        }
        return 1;
    }
    return 0;
}
void dungeon_blood(int red, int green) { g_blood[0] = red; g_blood[1] = green; }

df_u32 dungeon_container(int i)
{
    Entity *e;
    if (i < 0 || i >= g_count) { return 0; }
    e = g_foe[i].entity;
    return loot_container(e->key.place,e->key.thing,e->x,e->y,e->z,dungeon_loot_list(i));
}

/* A creature with no key of its own (the watch from a street) has no container; its gear is made into one given at its death. */
void dungeon_gear_to(int index, df_u32 container)
{
    if (index < 0 || index >= g_count || !container) { return; }
    foe_equipment(g_foe[index].kind,container,g_foe[index].equipped,&g_foe[index].fighter);
}

void dungeon_close(void)
{
    int i;
    for (i = 0; i < 8; i++) { if (g_splash[i]) { spawn_remove(g_splash[i]); } }
    mem_zero(g_splash,sizeof(g_splash));
    mem_zero(g_splash_time,sizeof(g_splash_time));
    mem_zero(g_arrow,sizeof(g_arrow));
    g_count = 0;
}

int dungeon_count(void) { return g_count; }
int dungeon_has(const Entity *entity)
{
    int i;

    for (i = 0; i < g_count; i++) { if (g_foe[i].entity == entity) { return 1; } }
    return 0;
}
Entity *dungeon_entity(int index) { return index >= 0 && index < g_count ? g_foe[index].entity : 0; }

void dungeon_rebind(Entity *entities, int count)
{
    int i;
    for (i = 0; entities && i < count && i < g_count; i++) {
        g_foe[i].entity = &entities[i]; g_foe[i].body.self = &entities[i];
    }
}

int dungeon_add(Entity *entity, int kind, int passive, int rate)
{
    FoeState *f;
    const Keep *row;
    const ClassInfo *career;
    int index, j, skill, health, dice;
    if (!entity || g_count >= FOES || !((kind >= 0 && kind < 43) || (kind >= 128 && kind <= 146))) { return -1; }
    index = g_count++;
    f = &g_foe[index]; f->entity = entity;
    f->kind = kind; f->passive = passive; f->rate = rate > 0 ? rate : 100;
    f->script = foe_script(f->kind);
    f->type = foe_texture_kind(f->kind);
    f->voice_time = 1+(float)(f->entity->key.thing%4000)/1000;
    f->def = foe_def(f->kind);
    career = f->kind >= 128 ? class_at((f->kind-128)%18) : 0;
    f->fighter.race = f->kind >= 128 ? 43 : f->kind;
    f->fighter.level = f->def ? f->def->level : character_state()->level;
    f->fighter.metal = f->def ? f->def->metal : 0;
    skill = f->fighter.level*5+30; if (skill > 100) { skill = 100; }
    for (j = 0; j < SKILLS; j++) { f->fighter.skill[j] = skill; }
    for (j = 0; j < ATTRS; j++) { f->fighter.attribute[j] = f->def ? f->def->attribute[j] : career ? career->attribute[j] : 50; }
    for (j = 0; j < 7; j++) { f->fighter.armor[j] = f->def ? f->def->armor : 60; }
    f->fighter.fatigue = (f->fighter.attribute[ATTR_STR]+f->fighter.attribute[ATTR_END])*64;
    f->fighter.magicka = f->fighter.max_magicka = f->fighter.attribute[ATTR_INT]*2;
    if (creature_spells(f->kind,f->fighter.level)) {
        f->fighter.magicka = f->fighter.max_magicka = f->fighter.level*10+100;
        for (j = 22; j <= 27; j++) { f->fighter.skill[j] = 80; }
    }
    row = keep_find(f->entity->key);
    f->restored = row && row->placed;
    if (row && row->game[0] == 0xc1) {
        f->fighter.health = (int)df_ru32(row->game+4); f->fighter.maximum = (int)df_ru32(row->game+8);
        f->awake = (row->game[1] & 1) != 0;
        if (row->game[1] & 2) { f->passive = 1; }
        f->mind.blocked = row->game[2]; f->mind.direction = row->game[3] & 15;
        f->mind.turns = row->game[3] >> 4;
        f->mind.give_up = row->game[12]; f->mind.stuck = row->game[13];
        f->mind.detected = row->game[14]; f->mind.encountered = row->game[15];
    } else {
        health = foe_hp_bonus(f->kind);
        dice = f->def ? f->def->dice : f->fighter.level;
        for (j = 0; j < dice; j++) { health += random_range(1,f->def ? 8 : career ? career->hp_per_level : 10); }
        f->fighter.health = f->fighter.maximum = health > 0 ? health : 1;
        store_health(f);
    }
    if (f->entity->dead || f->fighter.health <= 0) {
        f->entity->dead = 1; f->entity->radius = 0;
        f->entity->act = ENTITY_DEAD; f->entity->hold = 1; f->entity->frame = 0;
    }
    body_init(&f->body,12,80,24,60);
    f->body.radius = f->entity->radius > 0 ? f->entity->radius : 12;
    f->body.height = 80; f->body.eye = 60; f->body.step = 24; f->body.self = f->entity;
    if (f->def && (f->def->flags & 1)) { f->body.gravity = 0; }
    body_place(&f->body,f->entity->x,f->entity->y,f->entity->z);
    magic_restore_foe(index);
    return index;
}

void dungeon_open(const Scene *scene, const int *kinds, const int *passive, const int *rates)
{
    int i;
    dungeon_close(); weapon_reset(); combat_reset();
    magic_open(scene);
    g_scene = scene;
    mem_zero(g_foe,sizeof(g_foe));
    for (i = 0; scene && i < scene->nentities && i < FOES; i++) {
        dungeon_add((Entity *)&scene->entities[i],kinds ? kinds[i] : 0,
                       passive ? passive[i] : 0,rates ? rates[i] : 100);
    }
}

void dungeon_open_street(const Scene *scene)
{
    dungeon_close();
    magic_open(scene);
    g_scene = scene;
    mem_zero(g_foe,sizeof(g_foe));
}

int dungeon_loaded(int index)
{
    FoeState *f;
    float height;
    if (index < 0 || index >= g_count) { return 0; }
    f = &g_foe[index];
    if (f->loaded || !f->entity->live) { return 0; }
    f->loaded = 1;
    if (f->restored) { f->entity->y = f->body.feet[1]; }
    else { body_place(&f->body,f->entity->x,f->entity->y,f->entity->z); }
    height = f->entity->pose[ENTITY_WALK].h[0];
    if (height < 24) { height = 24; }
    if (height > 128) { height = 128; }
    f->body.height = height; f->body.eye = f->body.eye_now = height/2;
    /* A rat's step used to consume its entire collision ellipsoid. */
    f->body.step = minimum(24,height/4);
    if (!f->entity->dead) { f->entity->radius = f->body.radius; }
    if (f->kind >= 128 || f->kind == 7 || f->kind == 8 || f->kind == 12 || f->kind == 21 || f->kind == 24) {
        foe_equipment(f->kind,dungeon_container(index),f->equipped,&f->fighter);
    }
    return 1;
}

/* A long ray is gathered in stretches: one box the length of the ray holds more triangles than the set, and the ones dropped are walls the ray would pass through, so far sleepers saw the player through the rock and woke. */
#define RAY_STRETCH 384.0f
static int wall_distance(const Scene *scene, const float *from, const float *dir, float length, float *hit)
{
    float start, stretch, a[3], b[3], origin[3], at;
    int k;
    if (!scene || !collide_use(scene)) { return 0; }
    collide_quiet(1);
    for (start = 0; start < length; start += RAY_STRETCH) {
        stretch = length-start < RAY_STRETCH ? length-start : RAY_STRETCH;
        for (k = 0; k < 3; k++) { a[k] = from[k]+dir[k]*start; b[k] = a[k]+dir[k]*stretch; origin[k] = a[k]; }
        collide_gather(minimum(a[0],b[0])-1,minimum(a[1],b[1])-1,minimum(a[2],b[2])-1,
                       maximum(a[0],b[0])+1,maximum(a[1],b[1])+1,maximum(a[2],b[2])+1);
        if (collide_ray(origin,dir,stretch,&at)) { *hit = start+at; collide_quiet(0); return 1; }
    }
    collide_quiet(0);
    return 0;
}

/* The middle of the player's body: half way from the floor to the eye, so a crouch (an eye 32 up) has it 16 up, not in the floor. */
static float chest(const Camera *cam)
{
    float feet;

    feet = move_body()->feet[1];
    return feet+(cam->y-feet)*0.5f;
}

static int visible(const Scene *scene, const float *from, const float *to)
{
    float dir[3], length, hit;
    int k;
    length = 0;
    for (k = 0; k < 3; k++) { dir[k] = to[k]-from[k]; length += dir[k]*dir[k]; }
    length = df_sqrt(length);
    if (length < 1) { return 1; }
    for (k = 0; k < 3; k++) { dir[k] /= length; }
    return !wall_distance(scene,from,dir,length,&hit) || hit >= length-1;
}

static void splash(const Scene *scene, const Entity *target, int green)
{
    Entity e;
    int i;
    if (g_blood[green] < 0) { return; }
    for (i = 0; i < 8; i++) {
        if (g_splash_time[i] > 0) { continue; }
        entity_init(&e); e.live = 1; e.x = target->x; e.y = target->y+40; e.z = target->z;
        e.pose[ENTITY_IDLE].slot[0] = (df_u16)g_blood[green];
        e.pose[ENTITY_IDLE].w[0] = e.pose[ENTITY_IDLE].h[0] = 40;
        e.pose[ENTITY_IDLE].rate = 12;
        g_splash[i] = spawn_add(scene,&e,0); g_splash_time[i] = 0.3f;
        return;
    }
}

static void hit_foe(const Scene *scene, int i, int damage, float yaw)
{
    FoeState *f;
    int force, weight, absorbed;
    Entity *e;
    if (damage <= 0) { return; }
    f = &g_foe[i]; e = f->entity;
    damage = magic_absorb(i,damage); if (!damage) { return; }
    f->fighter.health -= damage; f->awake = 1;
    f->mind.detected = f->mind.encountered = 1; f->mind.give_up = 200;
    splash(scene,e,f->kind == 7 || f->kind == 8);
    if (f->fighter.health <= 0) {
        f->fighter.health = 0; e->dead = 1; e->radius = 0;
        e->act = ENTITY_DEAD; e->hold = 1; e->frame = 0; e->moving = 0;
        magic_soul_capture(i);
        sound_at(17,e->x,e->y,e->z);
        state_event(EVENT_KILL,e->key.thing,game_state()->entity_id,damage);
        /* Event 2 of the quests: a creature a quest placed died. */
        if ((e->key.thing & 0xe0000000u) == QUEST_FOE_KEY) { quest_foe_killed((int)((e->key.thing >> 4) & 0xffffffu)); }
        if (g_street_died) { g_street_died(f->kind,e); }
        /* damage_creature_death: a watchman killed is counted by the Brotherhood (guild_count_crime(6, 1)). */
        if (f->kind == 146) { guild_count_crime(6, 1, game_state()->minutes); }
    } else {
        weight = foe_weight(f->kind);
        if (weight && !f->knock) {
            force = damage*10;
            absorbed = (damage*2*((force-weight)*256/(force+weight)))/256;
            f->knock = (float)((force/weight)*(damage*2-absorbed));
            if (f->knock < 15) { f->knock = 15; }
            f->knock_yaw = yaw;
        }
        f->hurt = 0.25f; e->act = ENTITY_HURT; e->hold = 1; e->frame = 0;
        state_event(EVENT_HURT,e->key.thing,game_state()->entity_id,damage);
    }
    store_health(f);
}

int dungeon_kind(int i) { return i >= 0 && i < g_count ? g_foe[i].kind : -1; }

int dungeon_spell_target(int index, CharacterState *who, ClassInfo *career, int *base)
{
    FoeState *f;
    int i;
    if (index < 0 || index >= g_count) { return 0; }
    f = &g_foe[index]; mem_zero(who,sizeof(*who)); mem_zero(career,sizeof(*career));
    if (f->kind >= 128 && f->kind < 146 && class_at(f->kind-128)) { *career = *class_at(f->kind-128); }
    who->race = f->fighter.race; who->level = f->fighter.level;
    who->health = f->fighter.health; who->max_health = f->fighter.maximum;
    who->fatigue = f->fighter.fatigue; who->magicka = f->fighter.magicka;
    who->max_magicka = f->fighter.max_magicka; who->conditions = f->fighter.conditions;
    for (i = 0; i < ATTRS; i++) {
        who->attribute[i] = f->fighter.attribute[i];
        base[i] = f->def ? f->def->attribute[i] : career->attribute[i];
    }
    for (i = 0; i < SKILLS; i++) { who->skill[i] = f->fighter.skill[i]; }
    return 1;
}

void dungeon_spell_commit(int index, const CharacterState *who, int caster)
{
    FoeState *f;
    int i, conditions;
    if (index < 0 || index >= g_count) { return; }
    f = &g_foe[index]; conditions = who->conditions;
    if (who->health < f->fighter.health) {
        if (caster >= 0) { f->awake = 1; }
        hit_foe(g_scene,index,f->fighter.health-who->health,0);
        conditions = (conditions & ~0x400000) | (f->fighter.conditions & 0x400000);
    } else { f->fighter.health = who->health; store_health(f); }
    f->fighter.fatigue = who->fatigue; f->fighter.magicka = who->magicka;
    f->fighter.max_magicka = who->max_magicka; f->fighter.conditions = conditions;
    for (i = 0; i < ATTRS; i++) { f->fighter.attribute[i] = who->attribute[i]; }
}

void dungeon_pacify(int index)
{
    if (index >= 0 && index < g_count) {
        g_foe[index].passive = 1; g_foe[index].awake = 0;
        g_foe[index].entity->moving = 0; store_health(&g_foe[index]);
    }
}

static int miss_sound(int weapon, int kind)
{
    if (weapon >= 0 && foe_parries(kind)) {
        /* Watcom rand() < 32768 always succeeds; retain that original check. */
        random_range(0,32767); return random_range(291,299);
    }
    return swing_sound(weapon);
}

static void resolve_player(const Scene *scene, const Camera *cam, int target, const StrikeInfo *s, int index, int material)
{
    FighterInfo a;
    FoeState *f;
    int skill, part, chance, base, damage, expert, backstab;
    float toward;
    f = &g_foe[target];
    fighter_player(&a);
    part = struck_part(random_range(0,19));
    skill = weapon_skill(index);
    expert = index >= 0 && (weapon_proficiency(index) & game_state()->career.expert);
    chance = a.skill[skill] + (expert ? a.level : 0);
    toward = df_atan2(cam->x-f->entity->x,cam->z-f->entity->z)-f->entity->yaw;
    backstab = !s->bow && df_cos(toward) < df_cos(640*DF_TAU/2048) ? a.skill[19] : 0;
    chance += backstab;
    skill_use(skill); skill_use(34); if (backstab) { skill_use(19); }
    if (!god_mode() && index >= 0 && material < f->fighter.metal) {
        f->awake = 1; store_health(f);
        msg_set("Your weapon is ineffective against this creature.");
        sound_at(miss_sound(index,f->kind),f->entity->x,f->entity->y+40,f->entity->z); return;
    }
    chance = attack_chance(&a,&f->fighter,chance,index,index >= 0 ? material_hit(material) : 0,
                              part,s->hit,random_range(1,100));
    f->awake = 1;
    if (!god_mode() && random_range(0,100) > chance) {
        sound_at(miss_sound(index,f->kind),f->entity->x,f->entity->y+40,f->entity->z);
        store_health(f); return;
    }
    base = index >= 0 ? random_range(weapon_damage(index,0),weapon_damage(index,1)) :
        random_range(a.skill[30]/10+1,a.skill[30]/5+1);
    damage = attack_damage(&a,&f->fighter,index,material,s->damage,expert,base);
    /* damage_bonus_vs_target: a creature of a class is a humanoid. */
    if (index >= 0 && s->item) { damage += enchant_hit_bonus(s->item, f->kind >= 128 ? 2 : foe_category(f->kind)); if (damage < 0) { damage = 0; } }
    if (god_mode() && damage < 1) { damage = 1; }
    if (backstab && random_range(1,100) <= backstab) { damage *= 3; msg_set("Backstab!"); }
    /* Event 21: the player hit a creature of a quest. */
    if (damage && (f->entity->key.thing & 0xe0000000u) == QUEST_FOE_KEY) { quest_foe_hit((int)((f->entity->key.thing >> 4) & 0xffffffu)); }
    hit_foe(scene,target,damage,cam->yaw);
    if (damage && index >= 0 && s->item) { enchant_strike(s->item, target, damage); }
    if (damage) {
        sound_at(random_range(378,382),f->entity->x,f->entity->y+40,f->entity->z); item_wear(s->item,damage);
        item_wear(f->equipped[armor_slot(part)],damage);
    }
}

static void launch_arrow(const Camera *cam, int source, df_u32 weapon, int index, int material)
{
    ArrowInfo *arrow;
    Basis b;
    int i;
    for (i = 0; i < ARROWS; i++) {
        arrow = &g_arrow[i];
        if (arrow->live) { continue; }
        mem_zero(arrow,sizeof(*arrow)); arrow->live = 1; arrow->source = source;
        arrow->weapon = weapon; arrow->index = index; arrow->material = material;
        camera_basis(cam,&b);
        arrow->direction[0] = b.fx*b.cp; arrow->direction[1] = b.sp; arrow->direction[2] = b.fz*b.cp;
        arrow->at[0] = cam->x; arrow->at[1] = cam->y; arrow->at[2] = cam->z;
        return;
    }
}

void dungeon_strike(const Scene *scene, const Camera *cam, const StrikeInfo *s)
{
    const ObjectInfo *item;
    Entity *e;
    float dx, dz, distance, point[3], from[3], facing;
    int i, hits, index, material;
    if (!s || s->hand < 0 || !cam) { return; }
    magic_hostile();
    item = object_at(s->item); index = item ? item->index : -1; material = item ? item->material : 0;
    if (s->bow) { launch_arrow(cam,-1,s->item,index,material); return; }
    hits = 0;
    /* The ray of a blow starts at the middle of the body, not a fixed 35 under the eye: crouched, the eye is 32 above the floor and 35 under it is in the floor, where no blow would ever be seen to connect. */
    from[0] = cam->x; from[1] = chest(cam); from[2] = cam->z;
    for (i = 0; i < g_count; i++) {
        e = g_foe[i].entity;
        if (e->dead || !e->live) { continue; }
        dx = e->x-cam->x; dz = e->z-cam->z;
        distance = df_sqrt(dx*dx+dz*dz);
        if (distance > 90 || df_absf(e->y-move_body()->feet[1]) > 90) { continue; }
        facing = df_cos(df_atan2(dx,dz)-cam->yaw);
        if (distance > 10 && facing <= df_cos(200*DF_TAU/2048)) { continue; }
        point[0] = e->x; point[1] = e->y+40; point[2] = e->z;
        if (!visible(scene,from,point)) { continue; }
        dungeon_alert(i);
        resolve_player(scene,cam,i,s,index,material); hits++;
    }
    if (g_street_hit) { hits += g_street_hit(cam); }
    if (!hits) {
        play_sound(swing_sound(index));
        actions_strike(scene,cam,5);
        door_strike(scene,cam);
    }
}

static void recover_to(df_u32 container)
{
    df_u32 arrow;
    int i;
    if (!container) { return; }
    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;
        o = &game_state()->object[i];
        if (o->id && o->parent == container && o->type == RECORD_ITEM && o->group == 3 && o->index == 18) {
            if (o->count < 199) { object_edit(o->id)->count++; }
            return;
        }
    }
    arrow = item_create(container,3,18,0);
    if (arrow) { object_edit(arrow)->count = 1; }
}

static void arrows_step(const Scene *scene, const Camera *cam, float dt)
{
    ArrowInfo *a;
    StrikeInfo s;
    Camera aim;
    float wall, length, nearest, along, dx, dy, dz, r, closest, centre[3];
    Entity *e;
    int i, j, target, blocked, k;
    for (i = 0; i < ARROWS; i++) {
        a = &g_arrow[i]; if (!a->live) { continue; }
        length = 2400*dt; if (length > 2048-a->distance) { length = 2048-a->distance; }
        wall = length; blocked = wall_distance(scene,a->at,a->direction,length,&wall);
        nearest = blocked ? wall : length; target = -2;
        for (j = a->source < 0 ? 0 : g_count; j <= g_count; j++) {
            if (j == g_count) {
                if (a->source < 0) { continue; }
                dx = cam->x-a->at[0]; dy = chest(cam)-a->at[1]; dz = cam->z-a->at[2]; r = 30;
            } else {
                e = g_foe[j].entity; if (e->dead || !e->live || j == a->source) { continue; }
                dx = e->x-a->at[0]; dy = e->y+40-a->at[1]; dz = e->z-a->at[2];
                r = e->radius > 20 ? e->radius : 20; r += 20;
            }
            along = dx*a->direction[0]+dy*a->direction[1]+dz*a->direction[2];
            closest = dx*dx+dy*dy+dz*dz-along*along;
            if (closest > r*r) { continue; }
            along -= df_sqrt(r*r-closest);
            if (along < 0) { along = 0; }
            if (along <= nearest) {
                centre[0] = a->at[0]+dx; centre[1] = a->at[1]+dy; centre[2] = a->at[2]+dz;
                if (visible(scene,a->at,centre)) { nearest = along; target = j; }
            }
        }
        for (k = 0; k < 3; k++) { a->at[k] += a->direction[k]*nearest; }
        a->distance += nearest;
        if (target >= 0) {
            sound_at(7,a->at[0],a->at[1],a->at[2]);
            if (target < g_count) {
                mem_zero(&s,sizeof(s)); s.hand = 0; s.bow = 1; s.item = a->weapon;
                aim = *cam; aim.yaw = df_atan2(a->direction[0],a->direction[2]);
                resolve_player(scene,&aim,target,&s,a->index,a->material); recover_to(dungeon_container(target));
            } else {
                FighterInfo p;
                int chance, part;
                fighter_player(&p); part = struck_part(random_range(0,19));
                chance = attack_chance(&g_foe[a->source].fighter,&p,
                    g_foe[a->source].fighter.skill[33],16,0,part,0,random_range(1,100));
                skill_use(20);
                if (random_range(0,100) <= chance) { damage_player(random_range(weapon_damage(16,0),weapon_damage(16,1)),part); }
                recover_to(game_state()->inventory_id);
            }
            a->live = 0;
        } else if (blocked || a->distance >= 2048) {
            if (blocked) {
                UseHit hit;
                interact_cast(scene,a->at[0]-a->direction[0]*(nearest+1),
                    a->at[1]-a->direction[1]*(nearest+1),a->at[2]-a->direction[2]*(nearest+1),
                    a->direction[0],a->direction[1],a->direction[2],nearest+2,&hit);
                if (hit.model >= 0) { actions_trigger(scene,hit.model,6); }
            }
            a->live = 0;
        }
    }
}

static void monster_strike(const Scene *scene, const Camera *cam, FoeState *f)
{
    FighterInfo player;
    float from[3], to[3], dx, dz;
    const ObjectInfo *weapon;
    int i, part, chance, damage, total, skill, index, material;
    dx = cam->x-f->entity->x; dz = cam->z-f->entity->z;
    if (dx*dx+dz*dz > 90*90 || df_absf(f->entity->y-move_body()->feet[1]) > 90) { return; }
    if (dx*dx+dz*dz > 100 && df_cos(df_atan2(dx,dz)-f->entity->yaw) <= df_cos(200*DF_TAU/2048)) { return; }
    from[0] = f->entity->x; from[1] = f->entity->y+40; from[2] = f->entity->z;
    to[0] = cam->x; to[1] = chest(cam); to[2] = cam->z;
    if (!visible(scene,from,to)) { return; }
    fighter_player(&player);
    part = struck_part(random_range(0,19)); total = 0;
    weapon = object_at(f->equipped[19]);
    index = weapon && weapon->condition > 0 ? weapon->index : -1;
    material = index >= 0 ? weapon->material : 0; skill = weapon_skill(index);
    for (i = 0; i < (index < 0 && f->def ? 5 : 1); i++) {
        if (index < 0 && f->def && (!f->def->minimum[i] || random_range(0,99) >= 50-(character_state()->reflexes-2)*10)) { continue; }
        chance = attack_chance(&f->fighter,&player,f->fighter.skill[skill],index,
            index >= 0 ? material_hit(material) : 0,part,0,random_range(1,100));
        skill_use(20);
        if (random_range(0,100) > chance) { continue; }
        damage = index >= 0 ? random_range(weapon_damage(index,0),weapon_damage(index,1)) :
            f->def ? random_range(f->def->minimum[i],f->def->maximum[i]) :
            random_range(f->fighter.skill[30]/10+1,f->fighter.skill[30]/5+1);
        damage = attack_damage(&f->fighter,&player,index,material,0,0,damage);
        damage_player(damage,part); total += damage;
        if (damage > 0) { ailments_strike((int)(f-g_foe),f->kind); }
        if (index >= 0) { item_wear(f->equipped[19],damage); }
    }
    sound_at(total ? random_range(380,381) : 374,f->entity->x,f->entity->y+40,f->entity->z);
}

static void foe_voice(FoeState *f, int variant)
{
    int id;
    if (f->kind == 146) { id = 11461; }
    else if (f->kind >= 0 && f->kind < 43) { id = 10000+f->kind*10+variant; }
    else { return; }
    sound_at(id,f->entity->x,f->entity->y+f->body.height/2,f->entity->z);
}

static void foe_move(FoeState *f, const Scene *scene, float dx, float dz, float dt)
{
    if (f->def && (f->def->flags & 1)) { body_fly_step(&f->body,scene,dx,f->body.vy*dt,dz); }
    else { body_step(&f->body,scene,dx,dz,dt); }
}

static void foe_step(const Scene *scene, const Camera *cam, int index, float dt)
{
    FoeState *f;
    Entity *e;
    float dx, dz, distance, from[3], to[3], speed, amount;
    int events, pose, stepped, sight, angle, facing;
    f = &g_foe[index]; e = f->entity;
    if (e->dead || !e->live) { return; }
    if (f->fighter.conditions & 1) {
        foe_move(f,scene,0,0,dt);
        e->x = f->body.feet[0]; e->y = f->body.feet[1]; e->z = f->body.feet[2];
        return;
    }
    dx = cam->x-e->x; dz = cam->z-e->z; distance = df_sqrt(dx*dx+dz*dz);
    if (distance > 2048) { return; }
    stepped = 0;
    if (f->def && (f->def->flags & 1)) { f->body.vy = 0; }
    f->voice_time -= dt;
    if (distance < 1024 && f->voice_time <= 0) {
        foe_voice(f,f->awake ? (distance < 128 && random_range(0,3) ? 2 : 1) : 0);
        f->voice_time = (float)random_range(2000,6000)/1000;
    }
    /* A sleeper looks only when its think timer is due: the sight ray gathers every triangle between it and the player, too much to do for each sleeper each frame. */
    if (!f->awake) {
        f->think -= dt;
        if (f->passive || f->think > 0) { return; }
    }
    from[0] = e->x; from[1] = e->y+40; from[2] = e->z;
    to[0] = cam->x; to[1] = chest(cam); to[2] = cam->z;
    sight = visible(scene,from,to);
    angle = df_iround(df_atan2(dx,dz)*(2048/DF_TAU)) & 2047;
    facing = df_iround(e->yaw*(2048/DF_TAU)) & 2047;
    if (!f->awake) {
        {
            f->think = 0.2f;
            if (sight && magic_sees(f->kind) && (ai_angle(facing,angle) <= 512 ||
                ai_stealth(f->mind.detected,df_iround(distance),f->mind.encountered))) {
                f->awake = f->mind.detected = f->mind.encountered = 1;
                f->mind.give_up = 200; store_health(f);
            }
        }
        if (!f->awake) { return; }
    }
    if (f->mind.give_up > 0) {
        f->mind.give_up -= dt*60;
        if (f->mind.give_up < 0) { f->mind.give_up = 0; }
    } else if (!magic_sees(f->kind) || ((!sight || ai_angle(facing,angle) > 512) &&
               !ai_stealth(0,df_iround(distance),f->mind.encountered))) {
        f->awake = f->mind.detected = 0; e->moving = 0; store_health(f); return;
    } else { f->mind.detected = 1; f->mind.give_up = 200; }
    f->attack_wait -= dt;
    e->moving = 0;
    if (f->hurt > 0) {
        f->hurt -= dt;
        if (f->knock > 0) {
            if (f->knock > 40) { f->knock = 40; }
            speed = f->knock > 40 ? 40 : f->knock;
            amount = (speed > 25 ? 25 : speed)*dt*60;
            foe_move(f,scene,df_sin(f->knock_yaw)*amount,df_cos(f->knock_yaw)*amount,dt);
            stepped = 1;
            f->knock -= dt*300;
        }
        if (f->hurt <= 0) { f->knock = 0; mem_zero(&f->anim,sizeof(f->anim)); }
    } else if (f->anim.script && !f->anim.ended) {
        f->anim.time += dt*1000;
        while (f->anim.time >= f->rate && !f->anim.ended) {
            f->anim.time -= f->rate; events = anim_tick(&f->anim);
            if (events & 1) { monster_strike(scene,cam,f); }
            if (events & 2) {
                Camera shot;
                shot.x = e->x; shot.y = e->y+50; shot.z = e->z;
                shot.yaw = df_atan2(dx,dz); shot.pitch = df_atan2(chest(cam)-shot.y,distance);
                sound_at(6,shot.x,shot.y,shot.z); launch_arrow(&shot,index,0,16,0);
            }
        }
        pose = f->anim.record == 25 ? ENTITY_SHOOT : f->anim.record == 20 ? ENTITY_CAST : ENTITY_ATTACK;
        e->act = (df_u8)pose; e->hold = 1; e->frame = (df_u8)f->anim.frame;
    } else {
        e->hold = 0; e->act = 0;
        if (f->attack_wait <= 0 && sight && ai_angle(facing,angle) < 128 &&
            magic_creature(index,cam)) {
            anim_start(&f->anim,f->script,32); f->attack_wait = 0.5f;
            e->act = ENTITY_CAST; e->hold = 1; e->frame = 0;
        } else if (f->attack_wait <= 0 && (distance <= 90 ||
            ((foe_type_flags(f->type) & 32) && distance > 360))) {
            if (sight && ai_angle(facing,angle) < 128) {
                anim_start(&f->anim,f->script,distance > 360 ? 24 : 8);
                if (f->voice_time < 2) { foe_voice(f,2); f->voice_time = 2; }
                /* Newly initialized monsters have reflexes zero in the original. */
                f->attack_wait = (float)(random_range(1500,3000)-(character_state()->level-10)*50-900)/1000;
                if (f->attack_wait < 0.1f) { f->attack_wait = 1.5f; }
            } else if (sight) {
                e->yaw = ai_turn(facing,angle,df_iround(dt*60*64))*(DF_TAU/2048);
            }
        } else if (distance > 70) {
            /* monster_move_step: SPD - 50 + D_00187CA9 (200). */
            speed = (float)(f->fighter.attribute[ATTR_SPD]+150);
            amount = speed*dt;
            if (amount > distance-60) { amount = distance-60; }
            ai_walk(&f->mind,&f->body,e,scene,to,sight,
                         f->def && (f->def->flags & 1),speed,dt);
            stepped = 1;
            e->moving = 1;
        } else {
            e->yaw = ai_turn(facing,angle,df_iround(dt*60*64))*(DF_TAU/2048);
        }
    }
    /* Gravity and floor contact also run during attack and hurt pictures. */
    if (!stepped) { foe_move(f,scene,0,0,dt); }
    e->x = f->body.feet[0]; e->y = f->body.feet[1]; e->z = f->body.feet[2];
    store_health(f);
}

void dungeon_step(const Scene *scene, const Camera *cam, float dt)
{
    int i;
    if (!scene || !cam || dt <= 0 || character_state()->health <= 0) { return; }
    if (dt > 0.1f) { dt = 0.1f; }
    combat_step(dt);
    for (i = 0; i < g_count && character_state()->health > 0; i++) { foe_step(scene,cam,i,dt); }
    arrows_step(scene,cam,dt);
    for (i = 0; i < 8; i++) {
        if (g_splash_time[i] > 0) {
            g_splash_time[i] -= dt;
            if (g_splash_time[i] <= 0 && g_splash[i]) { spawn_remove(g_splash[i]); g_splash[i] = 0; }
        }
    }
}

/* The nearest living creature's distance on the floor, -1 for none (nearest_creature_distance). */
int dungeon_nearest_distance(const Camera *cam)
{
    int i, best;
    float dx, dz, d;

    best = -1;
    for (i = 0; cam && i < g_count; i++) {
        if (g_foe[i].entity->dead || !g_foe[i].entity->live) { continue; }
        dx = g_foe[i].entity->x-cam->x; dz = g_foe[i].entity->z-cam->z; d = df_sqrt(dx*dx+dz*dz);
        if (best < 0 || d < (float)best) { best = df_iround(d); }
    }
    return best;
}

/* Whether a living creature of that category (0 undead, 1 Daedra, 2 humanoid, 3 animal) is within reach on the floor. */
int dungeon_category_within(const Camera *cam, int category, int reach)
{
    int i;
    float dx, dz;

    for (i = 0; cam && i < g_count; i++) {
        if (g_foe[i].entity->dead || !g_foe[i].entity->live || foe_category(g_foe[i].kind) != category) { continue; }
        dx = g_foe[i].entity->x-cam->x; dz = g_foe[i].entity->z-cam->z;
        if (dx*dx+dz*dz < (float)reach*(float)reach) { return 1; }
    }
    return 0;
}

/* Damage to the nearest living creature within reach (Vampiric effect at range); 1 when there was one. */
int dungeon_hurt_nearest(const Camera *cam, float reach, int damage)
{
    int i, best;
    float dx, dz, d, best_d;

    best = -1; best_d = reach;
    for (i = 0; cam && i < g_count; i++) {
        if (g_foe[i].entity->dead || !g_foe[i].entity->live) { continue; }
        dx = g_foe[i].entity->x-cam->x; dz = g_foe[i].entity->z-cam->z; d = df_sqrt(dx*dx+dz*dz);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) { return 0; }
    hit_foe(g_scene,best,damage,0);
    return 1;
}

void dungeon_wake(int index)
{
    FoeState *f;

    if (index < 0 || index >= g_count) { return; }
    f = &g_foe[index];
    f->awake = 1;
    f->mind.detected = f->mind.encountered = 1;
    /* A city watch gives up after three times a creature's count. */
    f->mind.give_up = 600;
}

int dungeon_index(const Entity *e)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_foe[i].entity == e) { return i; }
    }
    return -1;
}

void dungeon_remove(int index)
{
    FoeState *f;

    if (index < 0 || index >= g_count) { return; }
    f = &g_foe[index];
    f->fighter.health = 0;
    f->awake = 0;
    f->entity->live = 0;
}

void dungeon_set_street(int (*hit)(const Camera *cam), void (*died)(int kind, Entity *entity))
{
    g_street_hit = hit;
    g_street_died = died;
}

int dungeon_guards(void)
{
    int i, n;

    n = 0;
    for (i = 0; i < g_count; i++) {
        if (g_foe[i].kind == 146 && g_foe[i].fighter.health > 0) { n++; }
    }
    return n;
}
