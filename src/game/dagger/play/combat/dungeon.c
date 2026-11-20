#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/play/input/move.h"
#include "game/dagger/play/character/vitals.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/travel/doors.h"
#include "game/dagger/play/combat/ai.h"
#include "game/dagger/play/travel/actions.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/combat/anim.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/combat/foegear.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/tables/combatdefs.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/magicdefs.h"
#include "engine/scene/collide.h"
#include "engine/scene/interact.h"
#include "engine/scene/spawn.h"
#include "engine/scene/walker.h"
#include "engine/core/mem.h"

typedef struct FoeState {
    Entity *entity;
    const FoeDef *def;
    FighterInfo fighter;
    Body body;
    Mind mind;
    df_u32 equipped[EQUIP_SLOTS];
    Anim anim;
    const AnimScript *script;
    int kind, type, passive, awake, rate, loaded, restored;
    /* A creature the level's markers keep standing (action byte 99: a castle's guard or courtier): idle until the player trespasses or one of them is hurt (wait_state, equip.c 946). */
    int sleeper;
    /* A creature the player summoned with an artifact (character flag 2): it takes no target under SUMMON_GIVE_UP_SCORE and vanishes when it is the only creature left (ai_pick_target, ai_update_creatures). */
    int summoned;
    /* A creature the Wabbajack made (character flag 0x1000): another blow of it does not change it again. */
    int transformed;
    int team, target, marker_rolled;
    float retarget, attack_wait, hurt, knock, knock_yaw, think, voice_time, swing_clock;
} FoeState;

static FoeState g_foe[FOE_ACTORS];
static Entity g_unused_actor;
static ArrowInfo g_arrow[ARROWS];
static int g_count, g_allies, g_blood[2];

/* Bits in the existing keep-row AI byte. Old rows default to team 1. */
enum { FOE_AWAKE = 1, FOE_PASSIVE = 2, FOE_ALLIED = 4, FOE_MARKER_ROLLED = 8 };
static const Scene *g_scene;
static int (*g_street_hit)(const Camera *cam);
static void (*g_street_died)(int kind, Entity *entity);
/* The blood splashes (ours: the decomp makes a type-42 record of each): at most SPLASHES alive, each FOE_SPLASH_SIZE across at SPLASH_RATE frames a second, for SPLASH_LIFE seconds. */
#define SPLASHES 8
#define SPLASH_SIZE 40
#define SPLASH_RATE 12
#define SPLASH_LIFE 0.3f
static SpawnId g_splash[SPLASHES];
static float g_splash_time[SPLASHES];
/* Our keep row's layout (docs/game.md): a tag byte, the AI flags, the blocked count, the navigation (direction in the low nibble, turns above), the health and the maximum as words, then
 * the mind's counters. */
#define FOE_ROW_TAG 0xc1
#define FOE_ROW_FLAGS 1
#define FOE_ROW_BLOCKED 2
#define FOE_ROW_NAV 3
#define FOE_ROW_HEALTH 4
#define FOE_ROW_MAXIMUM 8
#define FOE_ROW_GIVE_UP 12
#define FOE_ROW_STUCK 13
#define FOE_ROW_DETECTED 14
#define FOE_ROW_ENCOUNTERED 15
#define FOE_ROW_NAV_TURN_SHIFT 4
#define FOE_ROW_NAV_DIR_MASK 15

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
    row->game[0] = FOE_ROW_TAG;
    row->game[FOE_ROW_FLAGS] = (df_u8)((f->awake ? FOE_AWAKE : 0) | (f->passive ? FOE_PASSIVE : 0) |
                          (f->team == TEAM_PLAYER ? FOE_ALLIED : 0) | (f->marker_rolled ? FOE_MARKER_ROLLED : 0));
    row->game[FOE_ROW_BLOCKED] = (df_u8)f->mind.blocked;
    row->game[FOE_ROW_NAV] = (df_u8)(f->mind.direction | (f->mind.turns << FOE_ROW_NAV_TURN_SHIFT));
    write32(row->game+FOE_ROW_HEALTH,(df_u32)f->fighter.health);
    write32(row->game+FOE_ROW_MAXIMUM,(df_u32)f->fighter.maximum);
    row->game[FOE_ROW_GIVE_UP] = (df_u8)f->mind.give_up;
    row->game[FOE_ROW_STUCK] = (df_u8)f->mind.stuck;
    row->game[FOE_ROW_DETECTED] = (df_u8)f->mind.detected;
    row->game[FOE_ROW_ENCOUNTERED] = (df_u8)f->mind.encountered;
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
        if (!f->entity->live || f->entity->dead || f->team != TEAM_HOSTILE || f->passive) { continue; }
        dx = f->entity->x-player->feet[0]; dy = f->entity->y-player->feet[1];
        dz = f->entity->z-player->feet[2];
        if (i == source || ai_stealth(f->mind.detected,df_iround(df_sqrt(dx*dx+dy*dy+dz*dz)),f->mind.encountered)) {
            f->awake = f->mind.detected = f->mind.encountered = 1;
            f->mind.give_up = FOE_GIVE_UP_HURT; store_health(f);
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
    if (i >= 0 && i < g_count && type >= 0 && type < FOE_TYPES) {
        g_foe[i].type = type; g_foe[i].script = foe_type_script(type);
    }
}

static int visible(const Scene *scene, const float *from, const float *to);

int dungeon_alive(void)
{
    int i, n = 0;

    for (i = 0; i < g_count; i++) {
        if (g_foe[i].entity->live && !g_foe[i].entity->dead) { n++; }
    }
    return n;
}

int dungeon_nearby(const Camera *cam, float reach)
{
    const Entity *e;
    float dx, dy, dz;
    int i;
    if (!cam) { return 0; }
    for (i = 0; i < g_count; i++) {
        e = g_foe[i].entity;
        if (!e->live || e->dead || g_foe[i].passive || g_foe[i].team == TEAM_PLAYER) { continue; }
        dx = e->x-cam->x; dy = e->y+BODY_MIDDLE-cam->y; dz = e->z-cam->z;
        if (dx*dx+dy*dy+dz*dz >= reach*reach) { continue; }
        /* A sleeper behind a wall does not keep the player from resting; the original counts the creatures round the player, which is every one in the cells loaded. */
        if (!g_foe[i].awake && g_scene) {
            float from[3], to[3];
            from[0] = e->x; from[1] = e->y+BODY_MIDDLE; from[2] = e->z;
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
    for (i = 0; i < SPLASHES; i++) { if (g_splash[i]) { spawn_remove(g_splash[i]); } }
    mem_zero(g_splash,sizeof(g_splash));
    mem_zero(g_splash_time,sizeof(g_splash_time));
    mem_zero(g_arrow,sizeof(g_arrow));
    magic_forget_transients();
    g_count = g_allies = 0;
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

static int g_next_summoned, g_next_transformed, g_last_added = -1;
int dungeon_last_added(void) { return g_last_added; }
void dungeon_next_transformed(int on) { g_next_transformed = on != 0; }
int dungeon_transformed(int index) { return index >= 0 && index < g_count && g_foe[index].transformed; }
int dungeon_max_health(int index) { return index >= 0 && index < g_count ? g_foe[index].fighter.maximum : 0; }
void dungeon_next_summoned(int on) { g_next_summoned = on != 0; }

int dungeon_add(Entity *entity, int kind, int passive, int rate)
{
    FoeState *f;
    const Keep *row;
    const ClassInfo *career;
    int index, j, skill, health, dice;
    if (!entity || !ENEMY_IS_KNOWN(kind)) { return -1; }
    for (index = 0; index < g_count && g_foe[index].entity != &g_unused_actor; index++) { }
    if (index == FOE_ACTORS) { return -1; }
    if (index == g_count) { g_count++; }
    f = &g_foe[index]; mem_zero(f,sizeof(*f)); f->entity = entity;
    g_last_added = index;
    f->kind = kind; f->passive = passive; f->sleeper = passive != 0; f->summoned = g_next_summoned; f->transformed = g_next_transformed; f->rate = rate > 0 ? rate : FOE_RATE_NORMAL;
    f->target = TARGET_NONE; f->team = TEAM_HOSTILE;
    f->script = foe_script(f->kind);
    f->type = foe_texture_kind(f->kind);
    f->voice_time = 1+(float)(f->entity->key.thing%FOE_VOICE_STAGGER_MS)/1000;
    f->def = foe_def(f->kind);
    career = ENEMY_IS_CLASS(f->kind) ? class_at((f->kind-ENEMY_FIRST_CLASS)%ENEMY_CLASS_COUNT) : 0;
    f->fighter.race = ENEMY_IS_CLASS(f->kind) ? ENEMY_RACE_CLASS : f->kind;
    f->fighter.level = f->def ? f->def->level : character_state()->level;
    if (f->kind == ENEMY_CITY_WATCH) { f->fighter.level += random_range(WATCH_LEVEL_MIN, WATCH_LEVEL_MAX); }
    f->fighter.metal = f->def ? f->def->metal : 0;
    skill = f->fighter.level*FOE_SKILL_PER_LEVEL+FOE_SKILL_BASE; if (skill > FOE_SKILL_MAX) { skill = FOE_SKILL_MAX; }
    for (j = 0; j < SKILLS; j++) { f->fighter.skill[j] = skill; }
    for (j = 0; j < ATTRS; j++) { f->fighter.attribute[j] = f->def ? f->def->attribute[j] : career ? career->attribute[j] : 50; }
    for (j = 0; j < BODY_PARTS; j++) { f->fighter.armor[j] = FOE_ARMOR_SCALE * (f->def ? f->def->armor : enemy_class_armor(f->kind == ENEMY_CITY_WATCH ? ENEMY_CLASS_COUNT : (f->kind-ENEMY_FIRST_CLASS)%ENEMY_CLASS_COUNT)); }
    f->fighter.fatigue = fatigue_maximum(f->fighter.attribute[ATTR_STR],f->fighter.attribute[ATTR_END]);
    f->fighter.magicka = f->fighter.max_magicka = f->fighter.attribute[ATTR_INT]*2;
    if (creature_spells(f->kind,f->fighter.level)) {
        f->fighter.magicka = f->fighter.max_magicka = f->fighter.level*FOE_CASTER_MAGICKA_PER_LEVEL+FOE_CASTER_MAGICKA_BASE;
        for (j = SKILL_DESTRUCTION; j <= SKILL_MYSTICISM; j++) { f->fighter.skill[j] = FOE_CASTER_MAGIC_SKILL; }
    }
    row = keep_find(f->entity->key);
    f->restored = row && row->placed;
    if (row && row->game[0] == FOE_ROW_TAG) {
        f->fighter.health = (int)df_ru32(row->game+FOE_ROW_HEALTH); f->fighter.maximum = (int)df_ru32(row->game+FOE_ROW_MAXIMUM);
        f->awake = (row->game[FOE_ROW_FLAGS] & FOE_AWAKE) != 0;
        f->passive = (row->game[FOE_ROW_FLAGS] & FOE_PASSIVE) != 0; f->sleeper = f->passive;
        f->marker_rolled = (row->game[FOE_ROW_FLAGS] & FOE_MARKER_ROLLED) != 0;
        if (row->game[FOE_ROW_FLAGS] & FOE_ALLIED) { f->team = TEAM_PLAYER; g_allies++; }
        f->mind.blocked = row->game[FOE_ROW_BLOCKED]; f->mind.direction = row->game[FOE_ROW_NAV] & FOE_ROW_NAV_DIR_MASK;
        f->mind.turns = row->game[FOE_ROW_NAV] >> FOE_ROW_NAV_TURN_SHIFT;
        f->mind.give_up = row->game[FOE_ROW_GIVE_UP]; f->mind.stuck = row->game[FOE_ROW_STUCK];
        f->mind.detected = row->game[FOE_ROW_DETECTED]; f->mind.encountered = row->game[FOE_ROW_ENCOUNTERED];
    } else {
        health = foe_hp_bonus(f->kind);
        dice = f->def ? f->def->dice : f->fighter.level;
        for (j = 0; j < dice; j++) { health += random_range(1,f->def ? FOE_HIT_DIE : career ? career->hp_per_level : 10); }
        f->fighter.health = f->fighter.maximum = health > 0 ? health : 1;
        store_health(f);
    }
    if (f->entity->dead || f->fighter.health <= 0) {
        f->entity->dead = 1; f->entity->radius = 0;
        f->entity->act = ENTITY_DEAD; f->entity->hold = 1; f->entity->frame = 0;
    }
    body_init(&f->body,FOE_BODY_RADIUS,FOE_BODY_HEIGHT,FOE_BODY_STEP,FOE_BODY_EYE);
    f->body.radius = f->entity->radius > 0 ? f->entity->radius : FOE_BODY_RADIUS;
    f->body.height = FOE_BODY_HEIGHT; f->body.eye = FOE_BODY_EYE; f->body.step = FOE_BODY_STEP; f->body.self = f->entity;
    if (f->def && (f->def->flags & FOE_FLAG_FLYING)) { f->body.gravity = 0; }
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
                       passive ? passive[i] : 0,rates ? rates[i] : FOE_RATE_NORMAL);
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
    if (height < FOE_BODY_HEIGHT_MIN) { height = FOE_BODY_HEIGHT_MIN; }
    if (height > FOE_BODY_HEIGHT_MAX) { height = FOE_BODY_HEIGHT_MAX; }
    f->body.height = height; f->body.eye = f->body.eye_now = height/2;
    /* A rat's step used to consume its entire collision ellipsoid. */
    f->body.step = minimum(FOE_BODY_STEP,height/FOE_BODY_STEP_DIV);
    if (!f->entity->dead) { f->entity->radius = f->body.radius; }
    if (ENEMY_IS_CLASS(f->kind) || f->kind == ENEMY_ORC || f->kind == ENEMY_CENTAUR || f->kind == ENEMY_ORC_SERGEANT || f->kind == ENEMY_ORC_SHAMAN || f->kind == ENEMY_ORC_WARLORD) {
        foe_equipment(f->kind,dungeon_container(index),f->equipped,&f->fighter);
    }
    return 1;
}

int dungeon_loaded_marker(int index)
{
    FoeState *f;

    if (!dungeon_loaded(index)) { return 0; }
    f = &g_foe[index];
    if (!f->marker_rolled && !f->entity->dead) {
        f->marker_rolled = 1;
        /* A legacy row can already carry a live creature's pacified state. */
        if (!f->restored && ai_marker_pacify(f->kind,f->entity->key)) { dungeon_set_pacified(index,1); }
        store_health(f);
    }
    return 1;
}

int dungeon_team(int index)
{
    if (index == TARGET_PLAYER) { return TEAM_PLAYER; }
    return index >= 0 && index < g_count ? g_foe[index].team : -1;
}

void dungeon_set_team(int index, int team)
{
    FoeState *f;

    if (index < 0 || index >= g_count || (team != TEAM_PLAYER && team != TEAM_HOSTILE)) { return; }
    f = &g_foe[index];
    if (f->team == team) { return; }
    g_allies += team == TEAM_PLAYER ? 1 : -1;
    f->team = team; f->target = TARGET_NONE; f->retarget = 0;
    store_health(f);
}

int dungeon_target(int index)
{
    return index >= 0 && index < g_count ? g_foe[index].target : TARGET_NONE;
}

int dungeon_is_pacified(int index)
{
    return index >= 0 && index < g_count && g_foe[index].passive;
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
    for (i = 0; i < SPLASHES; i++) {
        if (g_splash_time[i] > 0) { continue; }
        entity_init(&e); e.live = 1; e.x = target->x; e.y = target->y+FOE_CHEST_HEIGHT; e.z = target->z;
        e.pose[ENTITY_IDLE].slot[0] = (df_u16)g_blood[green];
        e.pose[ENTITY_IDLE].w[0] = e.pose[ENTITY_IDLE].h[0] = SPLASH_SIZE;
        e.pose[ENTITY_IDLE].rate = SPLASH_RATE;
        g_splash[i] = spawn_add(scene,&e,0); g_splash_time[i] = SPLASH_LIFE;
        return;
    }
}

/* damage_knockback (damage.c 205): the throw of force and amount (a blow's are damage * KNOCK_FORCE_PER_DAMAGE and * KNOCK_AMOUNT_PER_DAMAGE) against the creature's weight, thrown the way yaw points; a throw already going is not
 * changed, and one under KNOCK_SPEED_MIN is that. */
static void foe_knock(FoeState *f, int force, int amount, float yaw)
{
    int weight, absorbed;

    weight = foe_weight(f->kind);
    if (!weight || f->knock) { return; }
    absorbed = (amount*((force-weight)*256/(force+weight)))/256;
    f->knock = (float)((force/weight)*(amount-absorbed));
    if (f->knock < KNOCK_SPEED_MIN) { f->knock = KNOCK_SPEED_MIN; }
    f->knock_yaw = yaw;
}

void dungeon_knockback(int index, int force, int amount, float yaw)
{
    FoeState *f;

    if (index < 0 || index >= g_count || g_foe[index].entity->dead || !g_foe[index].entity->live) { return; }
    f = &g_foe[index];
    foe_knock(f,force,amount,yaw);
    f->hurt = FOE_HURT_TIME; f->entity->act = ENTITY_HURT; f->entity->hold = 1; f->entity->frame = 0;
}

void dungeon_wake_sleepers(void);
static void wake_sleepers(void)
{
    int k;

    for (k = 0; k < g_count; k++) {
        if (g_foe[k].sleeper && !g_foe[k].entity->dead) { g_foe[k].passive = 0; g_foe[k].awake = 1; g_foe[k].sleeper = 0; }
    }
}

static int hit_foe_from(const Scene *scene, int i, int damage, float yaw, int source)
{
    FoeState *f;
    Entity *e;
    df_u32 attacker;

    if (damage <= 0) { return 0; }
    f = &g_foe[i]; e = f->entity;
    damage = magic_absorb(i,damage); if (!damage) { return 0; }
    f->fighter.health -= damage; f->awake = 1; f->passive = 0;
    /* monster_wake_all: a sleeper hurt wakes every one in the place (the original; ours woke only the one). */
    if (f->sleeper) { wake_sleepers(); }
    f->mind.detected = f->mind.encountered = 1; f->mind.give_up = FOE_GIVE_UP_HURT;
    attacker = source == TARGET_PLAYER ? game_state()->entity_id : 0;
    /* damage_spawn_splash: the second blood picture is the undead's, the others bleed the first. */
    splash(scene,e,foe_category(f->kind) == FOE_UNDEAD && !ENEMY_IS_CLASS(f->kind));
    if (f->fighter.health <= 0) {
        f->fighter.health = 0; e->dead = 1; e->radius = 0;
        e->act = ENTITY_DEAD; e->hold = 1; e->frame = 0; e->moving = 0;
        if (source == TARGET_PLAYER) { magic_soul_capture(i); }
        sound_at(SOUND_DEATH,e->x,e->y,e->z);
        state_event(EVENT_KILL,e->key.thing,attacker,damage);
        /* Event 2 of the quests: a creature a quest placed died. */
        if (QUEST_FOE_IS(e->key.thing)) { quest_foe_killed(QUEST_FOE_NUMBER(e->key.thing)); }
        if (g_street_died) { g_street_died(f->kind,e); }
        /* damage_creature_death: a watchman killed is counted by the Brotherhood (guild_count_crime(6, 1)). */
        if (source == TARGET_PLAYER && f->kind == ENEMY_CITY_WATCH) { guild_count_crime(GUILD_COUNT_KILL, 1, game_state()->minutes); }
    } else {
        foe_knock(f,damage*KNOCK_FORCE_PER_DAMAGE,damage*KNOCK_AMOUNT_PER_DAMAGE,yaw);
        f->hurt = FOE_HURT_TIME; e->act = ENTITY_HURT; e->hold = 1; e->frame = 0;
        state_event(EVENT_HURT,e->key.thing,attacker,damage);
    }
    store_health(f);
    return damage;
}

static int hit_foe(const Scene *scene, int target, int damage, float yaw)
{
    return hit_foe_from(scene,target,damage,yaw,TARGET_PLAYER);
}

void dungeon_enchant_damage(int index, int damage, int source)
{
    if (index >= 0 && index < g_count && !g_foe[index].entity->dead) { hit_foe_from(g_scene,index,damage,0,source); }
}

int dungeon_enchant_heal(int index, int amount)
{
    FoeState *f;

    if (index < 0 || index >= g_count || g_foe[index].entity->dead) { return 0; }
    f = &g_foe[index];
    if (amount > f->fighter.maximum-f->fighter.health) { amount = f->fighter.maximum-f->fighter.health; }
    f->fighter.health += amount;
    store_health(f);
    return amount;
}

int dungeon_kind(int i) { return i >= 0 && i < g_count ? g_foe[i].kind : -1; }

int dungeon_spell_target(int index, CharacterState *who, ClassInfo *career, int *base)
{
    FoeState *f;
    int i;
    if (index < 0 || index >= g_count) { return 0; }
    f = &g_foe[index]; mem_zero(who,sizeof(*who)); mem_zero(career,sizeof(*career));
    if (ENEMY_IS_CLASS_ONLY(f->kind) && class_at(f->kind-ENEMY_FIRST_CLASS)) { *career = *class_at(f->kind-ENEMY_FIRST_CLASS); }
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
        hit_foe_from(g_scene,index,f->fighter.health-who->health,0,caster);
        conditions = (conditions & ~COND_SHIELD) | (f->fighter.conditions & COND_SHIELD);
    } else { f->fighter.health = who->health; store_health(f); }
    f->fighter.fatigue = who->fatigue; f->fighter.magicka = who->magicka;
    f->fighter.max_magicka = who->max_magicka; f->fighter.conditions = conditions;
    for (i = 0; i < ATTRS; i++) { f->fighter.attribute[i] = who->attribute[i]; }
}

void dungeon_set_pacified(int index, int passive)
{
    FoeState *f;

    if (index < 0 || index >= g_count) { return; }
    f = &g_foe[index];
    f->passive = passive != 0; f->sleeper = 0;
    if (f->passive) {
        f->awake = 0; f->target = TARGET_NONE; f->entity->moving = 0;
        f->entity->act = 0; f->entity->hold = 0;
        mem_zero(&f->anim,sizeof(f->anim));
    }
    store_health(f);
}

void dungeon_pacify(int index) { dungeon_set_pacified(index,1); }

static int miss_sound(int weapon, int kind)
{
    if (weapon >= 0 && foe_parries(kind)) {
        /* Watcom rand() < 32768 always succeeds; retain that original check. */
        random_range(0,RAND_MAX_WATCOM); return random_range(SOUND_PARRY_FIRST,SOUND_PARRY_LAST);
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
    part = struck_part(random_range(0,BODY_PART_ROLLS-1));
    skill = weapon_skill(index);
    expert = index >= 0 && (weapon_proficiency(index) & game_state()->career.expert);
    chance = a.skill[skill] + (expert ? a.level : 0);
    toward = df_atan2(cam->x-f->entity->x,cam->z-f->entity->z)-f->entity->yaw;
    backstab = !s->bow && df_cos(toward) < df_cos(BACKSTAB_ARC*DF_TAU/ANGLE_FULL) ? a.skill[SKILL_BACKSTABBING] : 0;
    chance += backstab;
    skill_use(skill); skill_use(SKILL_CRITICAL_STRIKE); if (backstab) { skill_use(SKILL_BACKSTABBING); }
    if (!god_mode() && index >= 0 && material < f->fighter.metal) {
        f->awake = 1; store_health(f);
        msg_set("Your weapon is ineffective against this creature.");
        sound_at(miss_sound(index,f->kind),f->entity->x,f->entity->y+FOE_CHEST_HEIGHT,f->entity->z); return;
    }
    chance = attack_chance(&a,&f->fighter,chance,index,index >= 0 ? material_hit(material) : 0,
                              part,s->hit,random_range(1,100));
    f->awake = 1;
    if (!god_mode() && random_range(0,100) > chance) {
        sound_at(miss_sound(index,f->kind),f->entity->x,f->entity->y+FOE_CHEST_HEIGHT,f->entity->z);
        store_health(f); return;
    }
    base = index >= 0 ? random_range(weapon_damage(index,0),weapon_damage(index,1)) :
        random_range(a.skill[SKILL_HAND_TO_HAND]/HAND_DAMAGE_MIN_DIV+1,a.skill[SKILL_HAND_TO_HAND]/HAND_DAMAGE_MAX_DIV+1);
    damage = attack_damage(&a,&f->fighter,index,material,s->damage,expert,base);
    /* damage_bonus_vs_target: a creature of a class is a humanoid. */
    if (index >= 0 && s->item) { damage += enchant_hit_bonus(s->item, foe_enchant_category(f->kind)); if (damage < 0) { damage = 0; } }
    if (god_mode() && damage < 1) { damage = 1; }
    if (backstab && random_range(1,100) <= backstab) { damage *= BACKSTAB_MULTIPLIER; msg_set("Backstab!"); }
    /* Event 21: the player hit a creature of a quest. */
    if (damage && QUEST_FOE_IS(f->entity->key.thing)) { quest_foe_hit(QUEST_FOE_NUMBER(f->entity->key.thing)); }
    damage = hit_foe(scene,target,damage,cam->yaw);
    /* A weapon the player poisoned: the first blow that wounds puts the poison on the creature (which may resist) and is spent (qkey.c 403..408). */
    if (damage && s->item && weapon_poison(s->item)) { poison_apply(target,weapon_poison(s->item),0); weapon_poison_spend(s->item); }
    if (damage && index >= 0 && s->item) { enchant_strike(s->item, target, damage); }
    if (damage) {
        sound_at(random_range(SOUND_HIT_FIRST,SOUND_HIT_FIRST+SOUND_HIT_COUNT-1),f->entity->x,f->entity->y+FOE_CHEST_HEIGHT,f->entity->z); item_wear(s->item,damage);
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
    law_grace_end();
    magic_hostile();
    item = object_at(s->item); index = item ? item->index : -1; material = item ? item->material : 0;
    if (s->bow) { launch_arrow(cam,-1,s->item,index,material); return; }
    hits = 0;
    /* The ray of a blow starts at the middle of the body, not a fixed 35 under the eye: crouched, the eye is 32 above the floor and 35 under it is in the floor, where no blow would ever be seen to connect. */
    from[0] = cam->x; from[1] = chest(cam); from[2] = cam->z;
    for (i = 0; i < g_count; i++) {
        e = g_foe[i].entity;
        if (e->dead || !e->live || dungeon_team(i) == TEAM_PLAYER) { continue; }
        dx = e->x-cam->x; dz = e->z-cam->z;
        distance = df_sqrt(dx*dx+dz*dz);
        if (distance > MELEE_REACH || df_absf(e->y-move_body()->feet[1]) > MELEE_REACH) { continue; }
        facing = df_cos(df_atan2(dx,dz)-cam->yaw);
        if (distance > MELEE_ARC_SKIP && facing <= df_cos(MELEE_ARC*DF_TAU/ANGLE_FULL)) { continue; }
        point[0] = e->x; point[1] = e->y+FOE_CHEST_HEIGHT; point[2] = e->z;
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
        if (o->id && o->parent == container && o->type == RECORD_ITEM && o->group == ITEM_GROUP_WEAPON && o->index == WEAPON_ARROW) {
            if (o->count < 199) { object_edit(o->id)->count++; }
            return;
        }
    }
    arrow = item_create(container,ITEM_GROUP_WEAPON,WEAPON_ARROW,0);
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
        for (j = a->source >= 0 && !g_allies ? g_count : 0; j <= g_count; j++) {
            if (j == g_count) {
                if (dungeon_team(a->source) == TEAM_PLAYER) { continue; }
                dx = cam->x-a->at[0]; dy = chest(cam)-a->at[1]; dz = cam->z-a->at[2]; r = 30;
            } else {
                e = g_foe[j].entity; if (e->dead || !e->live || dungeon_team(j) == dungeon_team(a->source)) { continue; }
                dx = e->x-a->at[0]; dy = e->y+BODY_MIDDLE-a->at[1]; dz = e->z-a->at[2];
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
            if (a->source < 0) {
                mem_zero(&s,sizeof(s)); s.hand = 0; s.bow = 1; s.item = a->weapon;
                aim = *cam; aim.yaw = df_atan2(a->direction[0],a->direction[2]);
                resolve_player(scene,&aim,target,&s,a->index,a->material); recover_to(dungeon_container(target));
            } else {
                FighterInfo player;
                const FighterInfo *victim;
                FoeState *archer;
                int chance, part, damage, before, hit_player;

                archer = &g_foe[a->source];
                hit_player = target == g_count;
                if (hit_player) { fighter_player(&player); victim = &player; }
                else { victim = &g_foe[target].fighter; }
                part = struck_part(random_range(0,BODY_PART_ROLLS-1));
                chance = attack_chance(&archer->fighter,victim,archer->fighter.skill[SKILL_ARCHERY],16,0,part,0,random_range(1,100));
                if (hit_player) { skill_use(SKILL_DODGING); }
                if (random_range(0,100) <= chance) {
                    damage = random_range(weapon_damage(16,0),weapon_damage(16,1));
                    if (hit_player) {
                        before = character_state()->health;
                        damage_player(damage,part);
                        enchant_reflect(a->source,foe_enchant_category(archer->kind),before-character_state()->health);
                    } else { hit_foe_from(scene,target,damage,df_atan2(a->direction[0],a->direction[2]),a->source); }
                }
                recover_to(hit_player ? game_state()->inventory_id : dungeon_container(target));
            }
            a->live = 0;
        } else if (blocked || a->distance >= 2048) {
            if (blocked) {
                UseHit hit;
                interact_cast(scene,a->at[0]-a->direction[0]*(nearest+1),
                    a->at[1]-a->direction[1]*(nearest+1),a->at[2]-a->direction[2]*(nearest+1),
                    a->direction[0],a->direction[1],a->direction[2],nearest+2,&hit);
                if (hit.model >= 0) { actions_trigger(scene,hit.model,LINK_EVENT_STRIKE); }
            }
            a->live = 0;
        }
    }
}

static int target_live(int target)
{
    if (target == TARGET_PLAYER) { return character_state()->health > 0; }
    return target >= 0 && target < g_count && g_foe[target].entity->live && !g_foe[target].entity->dead;
}

static void target_point(const Camera *cam, int target, float point[3])
{
    const Entity *entity;

    entity = dungeon_entity(target);
    point[0] = entity ? entity->x : cam->x;
    point[1] = entity ? entity->y+BODY_MIDDLE : chest(cam);
    point[2] = entity ? entity->z : cam->z;
}

static int target_opposes(const FoeState *foe, int target)
{
    return target_live(target) && foe->team != dungeon_team(target);
}

/* Opposing actors in the active area are scored by proximity, sight, and
 * whether they already have a target. Skip rays that cannot win. */
static int pick_target(const Scene *scene, const Camera *cam, FoeState *foe)
{
    float from[3], to[3], dx, dy, dz, distance;
    int target, best, best_score, score;

    if (!g_allies && foe->team == TEAM_HOSTILE) { return TARGET_PLAYER; }
    from[0] = foe->entity->x; from[1] = foe->entity->y+FOE_CHEST_HEIGHT; from[2] = foe->entity->z;
    best = TARGET_NONE; best_score = -1;
    /* The original considers the creatures in list order, then the player. */
    for (target = 0; target <= g_count; target++) {
        int candidate;

        candidate = target == g_count ? TARGET_PLAYER : target;
        if (!target_opposes(foe,candidate)) { continue; }
        target_point(cam,candidate,to);
        dx = to[0]-from[0]; dy = to[1]-from[1]; dz = to[2]-from[2];
        distance = df_sqrt(dx*dx+dy*dy+dz*dz);
        if (distance > FOE_ACTIVE_RANGE) { continue; }
        score = TARGET_SCORE_BASE-(int)distance/TARGET_SCORE_DISTANCE_DIV;
        if (dungeon_target(candidate) == TARGET_NONE) { score += TARGET_SCORE_IDLE; }
        if (score+TARGET_SCORE_SEEN <= best_score) { continue; }
        if (visible(scene,from,to)) { score += TARGET_SCORE_SEEN; }
        if (score > best_score) { best = candidate; best_score = score; }
    }
    if (foe->summoned && best_score < SUMMON_GIVE_UP_SCORE) { return TARGET_NONE; }
    return best;
}

static void monster_strike(const Scene *scene, const Camera *cam, FoeState *foe, int target)
{
    FighterInfo player;
    const FighterInfo *victim;
    const ObjectInfo *weapon;
    float from[3], to[3], dx, dy, dz, distance, yaw;
    int i, part, chance, damage, dealt, total, skill, index, material, category, source;
    df_u32 weapon_id;

    target_point(cam,target,to);
    from[0] = foe->entity->x; from[1] = foe->entity->y+FOE_CHEST_HEIGHT; from[2] = foe->entity->z;
    dx = to[0]-from[0]; dy = to[1]-from[1]; dz = to[2]-from[2];
    distance = dx*dx+dy*dy+dz*dz;
    if (distance > MELEE_REACH*MELEE_REACH) { return; }
    yaw = df_atan2(dx,dz);
    if (distance > MELEE_ARC_SKIP*MELEE_ARC_SKIP && df_cos(yaw-foe->entity->yaw) <= df_cos(MELEE_ARC*DF_TAU/ANGLE_FULL)) { return; }
    if (!visible(scene,from,to)) { return; }
    if (target == TARGET_PLAYER) {
        fighter_player(&player); player.metal = curse_min_metal(); victim = &player;
        category = curse_beast() ? foe_category(curse_form() == CURSE_FORM_WEREWOLF ? ENEMY_WEREWOLF : ENEMY_WEREBOAR) : FOE_HUMANOID;
    } else {
        victim = &g_foe[target].fighter;
        category = foe_enchant_category(g_foe[target].kind);
    }
    source = (int)(foe-g_foe);
    part = struck_part(random_range(0,BODY_PART_ROLLS-1)); total = 0;
    weapon_id = foe->equipped[EQUIP_RIGHT_HAND]; weapon = object_at(weapon_id);
    index = weapon && weapon->condition > 0 ? weapon->index : -1;
    material = index >= 0 ? weapon->material : 0; skill = weapon_skill(index);
    for (i = 0; i < (index < 0 && foe->def ? FOE_ATTACKS : 1); i++) {
        if (index < 0 && foe->def && (!foe->def->minimum[i] || random_range(0,PERCENT-1) >= FOE_ATTACK_TRY_BASE-(character_state()->reflexes-REFLEXES_NORMAL)*FOE_ATTACK_TRY_PER_REFLEX)) { continue; }
        chance = attack_chance(&foe->fighter,victim,foe->fighter.skill[skill],index,
            index >= 0 ? material_hit(material) : 0,part,0,random_range(1,100));
        if (target == TARGET_PLAYER) { skill_use(SKILL_DODGING); }
        if (random_range(0,100) > chance || (index >= 0 && material < victim->metal)) { continue; }
        damage = index >= 0 ? random_range(weapon_damage(index,0),weapon_damage(index,1)) :
            foe->def ? random_range(foe->def->minimum[i],foe->def->maximum[i]) :
            random_range(foe->fighter.skill[SKILL_HAND_TO_HAND]/HAND_DAMAGE_MIN_DIV+1,foe->fighter.skill[SKILL_HAND_TO_HAND]/HAND_DAMAGE_MAX_DIV+1);
        damage = attack_damage(&foe->fighter,victim,index,material,0,0,damage);
        if (index >= 0) {
            damage += enchant_hit_bonus_for(weapon_id,category,source,foe->fighter.level);
            if (foe->entity->dead) { break; }
        }
        if (target == TARGET_PLAYER) {
            int before;

            before = character_state()->health;
            damage_player(damage,part); dealt = before-character_state()->health;
            if (damage > 0) { ailments_strike(source,foe->kind); }
        } else {
            dealt = hit_foe_from(scene,target,damage,yaw,source);
            if (dealt > 0) { item_wear(g_foe[target].equipped[armor_slot(part)],dealt); }
        }
        if (dealt > 0) {
            total += dealt;
            /* A poisoned weapon's first wound: the poison on the victim (it may resist), and spent. */
            if (index >= 0 && weapon_poison(weapon_id)) { poison_apply(target,weapon_poison(weapon_id),0); weapon_poison_spend(weapon_id); }
            if (index >= 0) { enchant_strike_for(weapon_id,source,target,foe->fighter.level,dealt); }
            if (target == TARGET_PLAYER) { enchant_reflect(source,foe_enchant_category(foe->kind),dealt); }
        }
        if (index >= 0 && !foe->entity->dead) { item_wear(weapon_id,dealt); }
        if (foe->entity->dead || !target_live(target)) { break; }
    }
    sound_at(total ? random_range(SOUND_CLAW_HIT_FIRST,SOUND_CLAW_HIT_FIRST+1) : SOUND_CLAW_MISS,from[0],from[1],from[2]);
}

static void monster_swing(const Scene *scene, const Camera *cam, FoeState *foe)
{
    int target, count;

    count = g_count;

    if (g_allies) {
        for (target = 0; target < count && !foe->entity->dead; target++) {
            if (target_opposes(foe,target)) { monster_strike(scene,cam,foe,target); }
        }
    }
    if (!foe->entity->dead && target_opposes(foe,TARGET_PLAYER)) { monster_strike(scene,cam,foe,TARGET_PLAYER); }
}

static void foe_voice(FoeState *f, int variant)
{
    int id;
    if (f->kind == ENEMY_CITY_WATCH) { id = ENEMY_VOICE_BASE+ENEMY_CITY_WATCH*ENEMY_VOICE_STRIDE+1; }
    else if (ENEMY_IS_CLASSIC(f->kind)) { id = ENEMY_VOICE_BASE+f->kind*ENEMY_VOICE_STRIDE+variant; }
    else { return; }
    sound_at(id,f->entity->x,f->entity->y+f->body.height/2,f->entity->z);
}

static void foe_move(FoeState *f, const Scene *scene, float dx, float dz, float dt)
{
    if (f->def && (f->def->flags & FOE_FLAG_FLYING)) { body_fly_step(&f->body,scene,dx,f->body.vy*dt,dz); }
    else { body_step(&f->body,scene,dx,dz,dt); }
}

/* A flyer killed in the air drops under gravity until its body lands, then lies there. The body flew with no gravity; a corpse is not flying. */
static void corpse_fall(FoeState *f, const Scene *scene, float dt)
{
    Entity *e;

    if (!f->def || !(f->def->flags & FOE_FLAG_FLYING) || f->body.grounded) { return; }
    e = f->entity;
    f->body.gravity = WALK_GRAVITY;
    body_step(&f->body,scene,0,0,dt);
    e->x = f->body.feet[0]; e->y = f->body.feet[1]; e->z = f->body.feet[2];
}

/* The Speed roll of a melee swing (see FOE_SWING_SPEED_DIV): the steps of the original that fell in this frame, each rolled; 1 when one passed. */
static int swing_roll(FoeState *f, float dt)
{
    int speed;

    speed = f->fighter.attribute[ATTR_SPD];
    f->swing_clock += dt*FOE_THINK_HZ;
    while (f->swing_clock >= 1) {
        f->swing_clock -= 1;
        if (speed <= 0 || random_range(0,RAND_MAX_WATCOM) % speed < speed/FOE_SWING_SPEED_DIV + FOE_SWING_SPEED_ADD) { f->swing_clock = 0; return 1; }
    }
    return 0;
}

static void foe_step(const Scene *scene, const Camera *cam, int index, float dt)
{
    FoeState *f;
    Entity *e;
    float dx, dz, distance, from[3], to[3], speed, reach;
    int events, pose, stepped, sight, angle, facing;
    f = &g_foe[index]; e = f->entity;
    if (!e->live) { return; }
    if (e->dead) { corpse_fall(f,scene,dt); return; }
    if (f->fighter.conditions & COND_PARALYZED) {
        foe_move(f,scene,0,0,dt);
        e->x = f->body.feet[0]; e->y = f->body.feet[1]; e->z = f->body.feet[2];
        return;
    }
    dx = cam->x-e->x; dz = cam->z-e->z; distance = df_sqrt(dx*dx+dz*dz);
    if (distance > FOE_ACTIVE_RANGE) { return; }
    stepped = 0;
    if (f->def && (f->def->flags & FOE_FLAG_FLYING)) { f->body.vy = 0; }
    f->voice_time -= dt;
    if (distance < FOE_VOICE_RANGE && f->voice_time <= 0) {
        foe_voice(f,f->awake ? (distance < FOE_VOICE_NEAR && random_range(0,3) ? 2 : 1) : 0);
        f->voice_time = (float)random_range(FOE_VOICE_MIN_MS,FOE_VOICE_MAX_MS)/1000;
    }
    if (f->passive && !(f->sleeper && actions_trespassing())) { return; }
    /* A sleeper looks only when its think timer is due: the sight ray gathers every triangle between it and the player, too much to do for each sleeper each frame. */
    if (!f->awake) {
        f->think -= dt;
        if (f->think > 0) { return; }
    }
    from[0] = e->x; from[1] = e->y+FOE_CHEST_HEIGHT; from[2] = e->z;
    f->retarget -= dt;
    if (f->retarget <= 0 || (f->target != TARGET_NONE && !target_opposes(f,f->target))) {
        f->target = pick_target(scene,cam,f);
        f->retarget = FOE_RETARGET_TIME+(float)(index & (FOE_RETARGET_SLOTS-1))*FOE_RETARGET_STAGGER;
    }
    if (f->target == TARGET_NONE) {
        e->moving = 0; e->act = 0; e->hold = 0;
        mem_zero(&f->anim,sizeof(f->anim));
        return;
    }
    target_point(cam,f->target,to);
    dx = to[0]-from[0]; dz = to[2]-from[2];
    distance = df_sqrt(dx*dx+dz*dz);
    reach = f->target == TARGET_PLAYER ? FOE_REACH_PLAYER : FOE_REACH_FOE;
    sight = visible(scene,from,to);
    angle = df_iround(df_atan2(dx,dz)*(ANGLE_FULL/DF_TAU)) & ANGLE_MASK;
    facing = df_iround(e->yaw*(ANGLE_FULL/DF_TAU)) & ANGLE_MASK;
    if (!f->awake) {
        {
            f->think = FOE_SLEEP_LOOK_TIME;
            if (sight && (f->target != TARGET_PLAYER || (magic_sees(f->kind) && (ai_angle(facing,angle) <= FOE_SIGHT_ARC ||
                ai_stealth(f->mind.detected,df_iround(distance),f->mind.encountered))))) {
                f->awake = f->mind.detected = f->mind.encountered = 1;
                f->mind.give_up = FOE_GIVE_UP; store_health(f);
            }
        }
        if (!f->awake) { return; }
    }
    if (f->mind.give_up > 0) {
        f->mind.give_up -= dt*TICKS_PER_SECOND;
        if (f->mind.give_up < 0) { f->mind.give_up = 0; }
    } else if (f->target == TARGET_PLAYER && (!magic_sees(f->kind) || ((!sight || ai_angle(facing,angle) > FOE_SIGHT_ARC) &&
               !ai_stealth(0,df_iround(distance),f->mind.encountered)))) {
        f->awake = f->mind.detected = 0; e->moving = 0; store_health(f); return;
    } else { f->mind.detected = 1; f->mind.give_up = FOE_GIVE_UP; }
    f->attack_wait -= dt;
    e->moving = 0;
    if (f->hurt > 0) {
        f->hurt -= dt;
        if (f->knock > 0) {
            if (f->knock > KNOCK_SPEED_MAX) { f->knock = KNOCK_SPEED_MAX; }
            speed = f->knock > KNOCK_SPEED_MAX ? KNOCK_SPEED_MAX : f->knock;
            speed = (speed > KNOCK_STEP_MAX ? KNOCK_STEP_MAX : speed)*dt*TICKS_PER_SECOND;
            {
                float want_x = df_sin(f->knock_yaw)*speed, want_z = df_cos(f->knock_yaw)*speed, was_x = f->body.feet[0], was_z = f->body.feet[2], got_x, got_z;

                foe_move(f,scene,want_x,want_z,dt);
                got_x = f->body.feet[0]-was_x; got_z = f->body.feet[2]-was_z;
                stepped = 1;
                /* Against a wall: half the speed is the damage, and the throw is over. */
                if (got_x*got_x+got_z*got_z < KNOCK_WALL_RATIO*KNOCK_WALL_RATIO*(want_x*want_x+want_z*want_z)) {
                    int damage = (int)f->knock/2;

                    f->knock = 0;
                    e->x = f->body.feet[0]; e->y = f->body.feet[1]; e->z = f->body.feet[2];
                    hit_foe_from(scene,index,damage,0,TARGET_NONE);
                    store_health(f);
                    return;
                }
            }
            f->knock -= dt*KNOCK_DECAY_PER_TICK*TICKS_PER_SECOND;
            if (f->knock <= KNOCK_STOP_SPEED) { f->knock = 0; }
        }
        if (f->hurt <= 0) { f->knock = 0; mem_zero(&f->anim,sizeof(f->anim)); }
    } else if (f->anim.script && !f->anim.ended) {
        f->anim.time += dt*MS_PER_SECOND;
        while (f->anim.time >= f->rate && !f->anim.ended) {
            f->anim.time -= f->rate; events = anim_tick(&f->anim);
            if (events & 1) { monster_swing(scene,cam,f); if (e->dead) { return; } }
            if ((events & 2) && target_opposes(f,f->target)) {
                Camera shot;
                shot.x = e->x; shot.y = e->y+FOE_BOW_HEIGHT; shot.z = e->z;
                shot.yaw = df_atan2(dx,dz); shot.pitch = df_atan2(to[1]-shot.y,distance);
                sound_at(6,shot.x,shot.y,shot.z); launch_arrow(&shot,index,0,16,0);
            }
        }
        pose = f->anim.record == RECORD_SHOOT_2 ? ENTITY_SHOOT : f->anim.record == RECORD_CAST ? ENTITY_CAST : ENTITY_ATTACK;
        e->act = (df_u8)pose; e->hold = 1; e->frame = (df_u8)f->anim.frame;
    } else {
        e->hold = 0; e->act = 0;
        if (f->attack_wait <= 0 && sight && ai_angle(facing,angle) < FOE_ATTACK_ARC &&
            magic_creature_at(index,f->target,cam)) {
            anim_start(&f->anim,f->script,ANIM_ACTION_CAST); f->attack_wait = FOE_CAST_PAUSE;
            e->act = ENTITY_CAST; e->hold = 1; e->frame = 0;
        } else if (f->attack_wait <= 0 && (distance <= reach ||
            ((foe_type_flags(f->type) & 32) && distance > FOE_RANGED_REACH_MUL*reach))) {
            if (sight && ai_angle(facing,angle) < FOE_ATTACK_ARC && distance <= FOE_RANGED_REACH_MUL*reach && !swing_roll(f,dt)) {
                /* The roll failed: it stands this step (monster_set_action 48). */
            } else if (sight && ai_angle(facing,angle) < FOE_ATTACK_ARC) {
                anim_start(&f->anim,f->script,distance > FOE_RANGED_REACH_MUL*reach ? ANIM_ACTION_SHOOT : ANIM_ACTION_ATTACK);
                if (f->voice_time < 2) { foe_voice(f,2); f->voice_time = 2; }
                /* Newly initialized monsters have reflexes zero in the original. */
                f->attack_wait = (float)(random_range(FOE_ATTACK_WAIT_MIN,FOE_ATTACK_WAIT_MAX)-(character_state()->level-FOE_ATTACK_WAIT_LEVEL_PIVOT)*FOE_ATTACK_WAIT_PER_LEVEL-REFLEXES_NORMAL*FOE_ATTACK_WAIT_PER_REFLEX)/1000;
                if (f->attack_wait < FOE_ATTACK_WAIT_FLOOR) { f->attack_wait = (float)FOE_ATTACK_WAIT_FALLBACK/1000; }
            } else if (sight) {
                e->yaw = ai_turn(facing,angle,df_iround(dt*TICKS_PER_SECOND*FOE_TURN_PER_TICK))*(DF_TAU/ANGLE_FULL);
            }
        } else if (distance > reach-FOE_CLOSE_MARGIN) {
            /* monster_move_step: SPD - 50 + D_00187CA9 (200). */
            speed = (float)(f->fighter.attribute[ATTR_SPD]+WALK_SPEED_ADD);
            ai_walk(&f->mind,&f->body,e,scene,to,sight,
                         f->def && (f->def->flags & FOE_FLAG_FLYING),speed,dt);
            stepped = 1;
            e->moving = 1;
        } else {
            e->yaw = ai_turn(facing,angle,df_iround(dt*TICKS_PER_SECOND*FOE_TURN_PER_TICK))*(DF_TAU/ANGLE_FULL);
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
    /* A summoned creature left alone is gone (ai_update_creatures: one creature, flag 2). */
    {
        int alive = 0, only = -1;

        for (i = 0; i < g_count; i++) { if (g_foe[i].entity->live && !g_foe[i].entity->dead) { alive++; only = i; } }
        if (alive == 1 && g_foe[only].summoned) { dungeon_remove(only); }
    }
    arrows_step(scene,cam,dt);
    for (i = 0; i < SPLASHES; i++) {
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
    f->mind.give_up = WATCH_GIVE_UP;
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

/* The pool owner releases its SpawnId after the combat references are gone.
 * Other actor indices stay stable, including arrows still in flight. */
void dungeon_release_actor(int index)
{
    int i;
    FoeState *foe;

    if (index < 0 || index >= g_count || g_foe[index].entity == &g_unused_actor) { return; }
    foe = &g_foe[index];
    magic_forget_actor(index);
    if (foe->team == TEAM_PLAYER) { g_allies--; }
    for (i = 0; i < g_count; i++) {
        if (g_foe[i].target == index) { g_foe[i].target = TARGET_NONE; g_foe[i].retarget = 0; }
    }
    for (i = 0; i < ARROWS; i++) {
        if (g_arrow[i].live && g_arrow[i].source == index) { g_arrow[i].live = 0; }
    }
    foe->entity->live = 0;
    mem_zero(foe,sizeof(*foe));
    foe->entity = &g_unused_actor;
    while (g_count && g_foe[g_count-1].entity == &g_unused_actor) { g_count--; }
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
        if (g_foe[i].kind == ENEMY_CITY_WATCH && g_foe[i].fighter.health > 0) { n++; }
    }
    return n;
}

void dungeon_wake_sleepers(void) { wake_sleepers(); }

int dungeon_level(int index) { return index >= 0 && index < g_count ? g_foe[index].fighter.level : 0; }

int dungeon_aim(float x, float y, float z, float dx, float dy, float dz, float reach)
{
    int i, best = -1;
    float best_along = 0;

    for (i = 0; i < g_count; i++) {
        const Entity *e = g_foe[i].entity;
        float px, py, pz, along, ex, ey, ez, radius;

        if (!e->live || e->dead) { continue; }
        px = e->x-x; py = e->y+BODY_MIDDLE-y; pz = e->z-z;
        along = px*dx+py*dy+pz*dz;
        if (along < 0 || along > reach) { continue; }
        ex = px-dx*along; ey = py-dy*along; ez = pz-dz*along;
        radius = e->radius > AIM_RADIUS_MIN ? e->radius : AIM_RADIUS_MIN;
        if (ex*ex+ey*ey+ez*ez > radius*radius) { continue; }
        if (best < 0 || along < best_along) { best = i; best_along = along; }
    }
    return best;
}
