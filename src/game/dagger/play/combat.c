#include "game/dagger/play/combat.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/sfx.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/places/dunstream.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/items.h"
#include "engine/scene/collide.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"
#include "engine/scene/stream.h"

/* See combat.h. */

/* A swing's to-hit and damage by its direction (DFU's player swing modifiers), groups 1..6. */
static const signed char g_swing_hit[7] = { 0, -10, -5, 0, 0, 5, 10 };
static const signed char g_swing_damage[7] = { 0, 4, 2, 0, 0, -2, -4 };

/* A class enemy wears nothing yet: DFU clamps worn armour to 60, so bare is 60. */
#define CLASS_ARMOR 60
/* A body part's armour is five times the kind's ArmorValue (EnemyEntity). */
#define ARMOR_SCALE 5
/* The body part a blow comes at (CalculateStruckBodyPart): one of these twenty, alike. */
static const df_u8 g_struck_part[20] = {
    BODY_HEAD, BODY_HEAD,
    BODY_RIGHT_ARM, BODY_RIGHT_ARM, BODY_RIGHT_ARM,
    BODY_LEFT_ARM, BODY_LEFT_ARM, BODY_LEFT_ARM,
    BODY_CHEST, BODY_CHEST, BODY_CHEST, BODY_CHEST,
    BODY_HANDS, BODY_HANDS, BODY_HANDS, BODY_HANDS,
    BODY_LEGS, BODY_LEGS, BODY_LEGS,
    BODY_FEET
};
/* Flinches at once, and how long one shows: the hurt pose's five frames at its ten a second. */
#define FLINCHES 16
#define FLINCH_TIME 0.5f
/* Off a wall by this much a creature still counts as seen. */
#define SIGHT_SLACK 4.0f

static df_u32 g_dice = 0x2B0F1u;

typedef struct Flinch {
    KeepKey key;
    float left;
} Flinch;

static Flinch g_flinch[FLINCHES];

void combat_seed(df_u32 seed)
{
    g_dice = seed;
}

int combat_roll(int n)
{
    if (n <= 1) {
        return 0;
    }
    g_dice = g_dice * 1103515245u + 12345u;
    return (int)((g_dice >> 16) % (df_u32)n);
}

/* lo..hi, both in. */
static int roll_between(int lo, int hi)
{
    return hi <= lo ? lo : lo + combat_roll(hi - lo + 1);
}

int combat_strength_bonus(int strength)
{
    int d;

    /* Rounded down, below 50 too: 42 is -2. */
    d = strength - 50;
    return d >= 0 ? d / 5 : -((-d + 4) / 5);
}

void combat_foe(Foe *f, const EnemyKind *kind, int player_level, int behind)
{
    int skill;

    f->kind = kind;
    f->behind = behind;
    if (kind->id >= ENEMY_FIRST_CLASS) {
        f->level = player_level;
        f->armor = CLASS_ARMOR;
    } else {
        f->level = kind->level;
        f->armor = kind->armor * ARMOR_SCALE;
    }
    skill = f->level * 5 + 30;
    f->skill = skill > 100 ? 100 : skill;
    f->dodging = f->skill;
}

int combat_roll_health(const Foe *f)
{
    const CareerClass *c;
    int health;
    int i;

    if (f->kind->id < ENEMY_FIRST_CLASS) {
        return roll_between(f->kind->health_min, f->kind->health_max);
    }
    /* RollEnemyClassMaxHealth: 10, and a roll of 1..hit points a level for each level. */
    c = career_class(f->kind->id - ENEMY_FIRST_CLASS);
    health = 10;
    for (i = 0; i < f->level; i++) {
        health += roll_between(1, c && c->hp_per_level ? c->hp_per_level : 1);
    }
    return health;
}

void combat_blow(const Character *pc, const Item *weapon, int swing, const Foe *f, Blow *out)
{
    const WeaponTemplate *w;
    const Material *m;
    int skill;
    int chance;
    int bonus;
    int damage;
    int crit;

    out->chance = 0;
    out->hit = 0;
    out->damage = 0;
    out->ineffective = 0;
    out->backstab = 0;
    w = weapon ? weapon_template(weapon->template_id) : 0;
    m = w ? material(weapon->material) : 0;
    if (swing < 0 || swing > 6) {
        swing = 0;
    }
    /* A weapon of a lesser metal than the creature's least does nothing (a ghost and iron). */
    if (w && f->kind->min_metal >= 0 && (int)weapon->material < f->kind->min_metal) {
        out->ineffective = 1;
        return;
    }
    skill = pc->skill[w ? w->skill : SKILL_HAND_TO_HAND];
    chance = skill + g_swing_hit[swing];
    bonus = g_swing_damage[swing];
    /* A race's weapon expertise: a Dark Elf's with any, a Wood Elf's with a bow, a Redguard's with any but a bow. */
    if (w) {
        int race;

        race = 0;
        if (pc->race == RACE_DARK_ELF) {
            race = pc->level / 4;
        } else if (w->skill == SKILL_ARCHERY ? pc->race == RACE_WOOD_ELF : pc->race == RACE_REDGUARD) {
            race = pc->level / 3;
        }
        chance += race;
        bonus += race;
    }
    /* A class's expert proficiency (CalculateProficiencyModifiers), bare hands among them. */
    if (character_expert(pc, w ? w->skill : SKILL_HAND_TO_HAND)) {
        chance += pc->level;
        bonus += pc->level / 3 + 1;
    }
    if (f->behind) {
        chance += pc->skill[SKILL_BACKSTABBING];
    }
    if (m) {
        chance += m->modifier * 10;
    }
    /* CalculateSuccessfulHit. */
    chance += f->armor;
    chance += (pc->attribute[ATTR_LUCK] - f->kind->attribute[ATTR_LUCK]) / 10;
    chance += (pc->attribute[ATTR_AGILITY] - f->kind->attribute[ATTR_AGILITY]) / 10;
    chance -= f->dodging / 4;
    crit = pc->skill[SKILL_CRITICAL_STRIKE];
    if (combat_roll(100) < crit) {
        chance += crit / 10;
    }
    /* Against a creature, not the player: 40; and always less 50. */
    chance += 40 - 50;
    chance = chance < 3 ? 3 : (chance > 97 ? 97 : chance);
    out->chance = chance;
    if (combat_roll(100) >= chance) {
        return;
    }
    out->hit = 1;
    if (w) {
        /* CalculateWeaponAttackDamage. */
        damage = roll_between(w->damage_min, w->damage_max) + bonus;
        if (f->kind->id == ENEMY_SKELETAL_WARRIOR) {
            const ItemTemplate *t;

            t = item_template(weapon->template_id);
            if (t && t->blunt) {
                damage /= 2;
            }
            if (weapon->material == MATERIAL_SILVER) {
                damage *= 2;
            }
        }
        damage += combat_strength_bonus(pc->attribute[ATTR_STRENGTH]);
        damage += m ? m->modifier : 0;
        if (damage < 1) {
            damage = 0;
        }
    } else {
        /* CalculateHandToHandAttackDamage: skill / 10 + 1 to skill / 5 + 1. */
        damage = roll_between(skill / 10 + 1, skill / 5 + 1) + bonus + combat_strength_bonus(pc->attribute[ATTR_STRENGTH]);
    }
    /* A class's bonus or phobia against what this creature is. */
    damage += character_group_damage(pc, enemy_group(f->kind->id));
    /* A backstab triples it on a roll under the skill. */
    if (f->behind && pc->skill[SKILL_BACKSTABBING] > 1 && combat_roll(100) < pc->skill[SKILL_BACKSTABBING]) {
        damage *= 3;
        out->backstab = 1;
    }
    out->damage = damage < 0 ? 0 : damage;
}

void combat_foe_blow(const Foe *f, const Character *pc, Blow *out)
{
    int base;
    int i;

    out->chance = 0;
    out->hit = 0;
    out->damage = 0;
    out->ineffective = 0;
    out->backstab = 0;
    out->part = g_struck_part[combat_roll(20)];
    base = f->skill + pc->armor[out->part];
    base += (f->kind->attribute[ATTR_LUCK] - pc->attribute[ATTR_LUCK]) / 10;
    base += (f->kind->attribute[ATTR_AGILITY] - pc->attribute[ATTR_AGILITY]) / 10;
    base -= pc->skill[SKILL_DODGING] / 4;
    /* Against the player there is no +40; the biography's avoid-hit is not kept yet. */
    base -= 50;
    for (i = 0; i < 3; i++) {
        int chance;
        int lo;
        int hi;

        if (f->kind->id >= ENEMY_FIRST_CLASS) {
            if (i > 0) {
                break;
            }
            lo = f->skill / 10 + 1;
            hi = f->skill / 5 + 1;
        } else {
            lo = f->kind->damage_min[i];
            hi = f->kind->damage_max[i];
            if (hi <= 0) {
                continue;
            }
        }
        chance = base;
        if (combat_roll(100) < f->skill) {
            chance += f->skill / 10;
        }
        chance = chance < 3 ? 3 : (chance > 97 ? 97 : chance);
        if (!out->chance) {
            out->chance = chance;
        }
        if (combat_roll(100) < chance) {
            out->hit = 1;
            out->damage += roll_between(lo, hi);
        }
    }
}

/* Its health from its keep row, rolled and written the first time. -1 with no row (no key, no memory). */
static int health_of(const Entity *e, const Foe *f, Keep **row)
{
    Keep *k;

    *row = 0;
    if (!e->key.place && !e->key.thing) {
        return -1;
    }
    k = keep_get(e->key, KEEP_ENTITY);
    if (!k) {
        return -1;
    }
    *row = k;
    if (!k->game[COMBAT_KEEP_ROLLED]) {
        int h;

        h = combat_roll_health(f);
        k->game[COMBAT_KEEP_HEALTH] = (df_u8)(h & 0xff);
        k->game[COMBAT_KEEP_HEALTH + 1] = (df_u8)((h >> 8) & 0xff);
        k->game[COMBAT_KEEP_ROLLED] = 1;
    }
    return k->game[COMBAT_KEEP_HEALTH] | (k->game[COMBAT_KEEP_HEALTH + 1] << 8);
}

static void set_health(Keep *k, int h)
{
    if (h < 0) {
        h = 0;
    }
    k->game[COMBAT_KEEP_HEALTH] = (df_u8)(h & 0xff);
    k->game[COMBAT_KEEP_HEALTH + 1] = (df_u8)((h >> 8) & 0xff);
}

int combat_sight(const Scene *scene, const float *a, const float *b)
{
    float d[3];
    float len;
    float t;
    int i;

    if (!scene || !collide_use(scene)) {
        return 0;
    }
    collide_set_shape(1.0f, 1.0f);
    for (i = 0; i < 3; i++) {
        d[i] = b[i] - a[i];
    }
    len = df_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1.0f) {
        return 1;
    }
    for (i = 0; i < 3; i++) {
        d[i] /= len;
    }
    collide_gather((a[0] < b[0] ? a[0] : b[0]) - 2.0f, (a[1] < b[1] ? a[1] : b[1]) - 2.0f,
                   (a[2] < b[2] ? a[2] : b[2]) - 2.0f, (a[0] > b[0] ? a[0] : b[0]) + 2.0f,
                   (a[1] > b[1] ? a[1] : b[1]) + 2.0f, (a[2] > b[2] ? a[2] : b[2]) + 2.0f);
    return !collide_ray(a, d, len, &t) || t >= len - SIGHT_SLACK;
}

/* A creature killed lies as its body, on the floor under it; one with no body's picture goes. */
static void lay_down(Entity *e)
{
    float fy;

    e->dead = 1;
    e->radius = 0.0f;
    if (!e->pose[ENTITY_DEAD].w[0]) {
        e->live = 0;
        return;
    }
    e->act = ENTITY_DEAD;
    e->hold = 1;
    e->frame = 0;
    collide_gather(e->x - 4.0f, e->y - 4096.0f, e->z - 4.0f, e->x + 4.0f, e->y + 8.0f, e->z + 4.0f);
    if (collide_floor(e->x, e->z, e->y + 4.0f, e->y - 4096.0f, &fy)) {
        e->y = fy;
    }
}

static void flinch(Entity *e)
{
    int i;
    int free_at;

    if (!e->pose[ENTITY_HURT].w[0]) {
        return;
    }
    e->act = ENTITY_HURT;
    e->hold = 0;
    e->phase = 0;
    free_at = -1;
    for (i = 0; i < FLINCHES; i++) {
        if (g_flinch[i].left > 0.0f && g_flinch[i].key.place == e->key.place && g_flinch[i].key.thing == e->key.thing) {
            g_flinch[i].left = FLINCH_TIME;
            return;
        }
        if (free_at < 0 && g_flinch[i].left <= 0.0f) {
            free_at = i;
        }
    }
    if (free_at >= 0) {
        g_flinch[free_at].key = e->key;
        g_flinch[free_at].left = FLINCH_TIME;
    }
}

/* One creature struck: the blow rolled, its sound, its health; a flinch or its body. */
static void strike_one(Character *pc, Item *weapon, int swing, Entity *e, const float *feet)
{
    const EnemyKind *kind;
    Foe f;
    Blow b;
    Keep *row;
    int health;
    int behind;
    float mid;

    kind = enemy_kind(dunstream_kind(e, 0));
    if (!kind) {
        return;
    }
    mid = e->y + entity_height(e) * 0.5f;
    /* Facing away: its forward and the way to the player point apart. */
    behind = df_sin(e->yaw) * (feet[0] - e->x) + df_cos(e->yaw) * (feet[2] - e->z) < 0.0f;
    combat_foe(&f, kind, pc->level, behind);
    health = health_of(e, &f, &row);
    /* A blow at its back trains the skill, landed or not (FormulaHelper's backstab chance). */
    if (behind) {
        character_tally(pc, SKILL_BACKSTABBING, 1);
    }
    combat_blow(pc, weapon, swing, &f, &b);
    /* Struck at all, it knows: passive or asleep, it turns on the player. */
    foes_alert(e);
    if (b.ineffective) {
        /* Classic's own line (FALL.EXE); DFU says "This weapon has no effect." */
        interact_say("The material of the weapon you are using is ineffective.");
        sfx_play(weapon ? SND_SWING_MEDIUM : SND_SWING_HIGH);
        return;
    }
    if (!b.hit || b.damage <= 0 || !row) {
        /* Nothing taken: a weapon on a creature that parries rings; anything else whistles. */
        if (weapon && kind->parries) {
            sfx_at(sfx_pick(SND_PARRY_FIRST, SND_PARRIES), e->x, mid, e->z);
        } else {
            sfx_play(weapon ? SND_SWING_MEDIUM : SND_SWING_HIGH);
        }
        return;
    }
    sfx_at(sfx_pick(SND_HIT_FIRST, SND_HITS), e->x, mid, e->z);
    if (weapon) {
        item_wear_down(weapon, b.damage, combat_roll(100));
    }
    if (b.backstab) {
        interact_say("Successful backstab!");
    }
    health -= b.damage;
    set_health(row, health);
    if (health <= 0) {
        /* What is on the body goes by the level the player was when it fell (loot.c). */
        row->game[COMBAT_KEEP_LEVEL] = (df_u8)(pc->level > 255 ? 255 : pc->level);
        lay_down(e);
        sfx_at(SND_BODY_FALL, e->x, e->y, e->z);
    } else {
        flinch(e);
    }
}

int combat_reaches(const Scene *scene, const float *feet, const float *eye, float yaw, const Entity *e)
{
    float d[3];
    float dist;
    float mid[3];

    if (!e->live || e->dead || dunstream_kind(e, 0) < 0) {
        return 0;
    }
    d[0] = e->x - feet[0];
    d[1] = e->y - feet[1];
    d[2] = e->z - feet[2];
    dist = df_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (dist > COMBAT_REACH) {
        return 0;
    }
    if (dist > COMBAT_POINT_BLANK) {
        float off;

        off = df_wrap_tau(df_atan2(d[0], d[2]) - yaw);
        if (off > DF_TAU * 0.5f) {
            off = DF_TAU - off;
        }
        if (off > COMBAT_ARC) {
            return 0;
        }
    }
    mid[0] = e->x;
    mid[1] = e->y + entity_height(e) * 0.5f;
    mid[2] = e->z;
    return combat_sight(scene, eye, mid);
}

/* The fatigue a swing or a shot costs. */
static void tire(Character *pc)
{
    pc->fatigue -= COMBAT_SWING_FATIGUE;
    if (pc->fatigue < 0) {
        pc->fatigue = 0;
    }
}

int combat_ray_box(const float *eye, const float *dir, const float *low, const float *high, float reach, float *t_out)
{
    float t_near;
    float t_far;
    int axis;

    t_near = 0.0f;
    t_far = reach;
    for (axis = 0; axis < 3; axis++) {
        float t0;
        float t1;

        if (dir[axis] > -0.0001f && dir[axis] < 0.0001f) {
            /* Along this axis the ray does not move: it is inside the slab or never will be. */
            if (eye[axis] < low[axis] || eye[axis] > high[axis]) {
                return 0;
            }
            continue;
        }
        t0 = (low[axis] - eye[axis]) / dir[axis];
        t1 = (high[axis] - eye[axis]) / dir[axis];
        if (t0 > t1) {
            float swap;

            swap = t0;
            t0 = t1;
            t1 = swap;
        }
        t_near = t0 > t_near ? t0 : t_near;
        t_far = t1 < t_far ? t1 : t_far;
        if (t_near > t_far) {
            return 0;
        }
    }
    *t_out = t_near;
    return 1;
}

int combat_shoot(Character *pc, Item *weapon, const Scene *scene, const float *feet, const float *eye, const float *dir)
{
    Entity *best;
    float best_t;
    int slot;

    tire(pc);
    /* A shot trains its skill and Critical Strike, struck or not (WeaponManager). */
    character_tally(pc, SKILL_ARCHERY, 1);
    character_tally(pc, SKILL_CRITICAL_STRIKE, 1);
    if (!scene || !collide_use(scene)) {
        return 0;
    }
    best = 0;
    best_t = FOES_SIGHT;
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            Entity *e;
            float low[3];
            float high[3];
            float half;
            float t;

            e = &ents[i];
            if (!e->live || e->dead || dunstream_kind(e, 0) < 0) {
                continue;
            }
            half = (float)entity_pose(e)->w[0] * 0.25f;
            low[0] = e->x - half;
            low[1] = e->y;
            low[2] = e->z - half;
            high[0] = e->x + half;
            high[1] = e->y + entity_height(e);
            high[2] = e->z + half;
            if (combat_ray_box(eye, dir, low, high, best_t, &t)) {
                float at[3];

                at[0] = eye[0] + dir[0] * t;
                at[1] = eye[1] + dir[1] * t;
                at[2] = eye[2] + dir[2] * t;
                if (combat_sight(scene, eye, at)) {
                    best = e;
                    best_t = t;
                }
            }
        }
    }
    if (!best) {
        return 0;
    }
    strike_one(pc, weapon, 0, best, feet);
    return 1;
}

int combat_strike(Character *pc, Item *weapon, int swing, const Scene *scene, const float *feet,
                  const float *eye, float yaw)
{
    int struck;
    int slot;

    /* The swing tires, struck or not. */
    tire(pc);
    struck = 0;
    /* Walls by the scene's collision grid, a ray as thin as rig_third's (the body sets its own shape again at its step). */
    if (!scene || !collide_use(scene)) {
        return 0;
    }
    collide_set_shape(1.0f, 1.0f);
    /*
     * Classic's (FALL.EXE): every creature whose feet are within reach of
     * the player's, and within the arc of its facing on the ground (any
     * way at point blank), that no wall hides from the eye.
     */
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            if (combat_reaches(scene, feet, eye, yaw, &ents[i])) {
                strike_one(pc, weapon, swing, &ents[i], feet);
                struck++;
            }
        }
    }
    if (!struck) {
        /* Nothing in reach: the swing whistles (classic plays it only when nothing was struck). */
        sfx_play(weapon ? SND_SWING_MEDIUM : SND_SWING_HIGH);
    } else {
        /* A swing that reached something trains its skill and Critical Strike, once a swing (WeaponManager). */
        const WeaponTemplate *w;

        w = weapon ? weapon_template(weapon->template_id) : 0;
        character_tally(pc, w ? w->skill : SKILL_HAND_TO_HAND, 1);
        character_tally(pc, SKILL_CRITICAL_STRIKE, 1);
    }
    return struck;
}

/* The entity with key, among the stream's, or 0. */
static Entity *by_key(KeepKey key)
{
    int slot;

    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            if (ents[i].key.place == key.place && ents[i].key.thing == key.thing) {
                return &ents[i];
            }
        }
    }
    return 0;
}

void combat_frame(float dt)
{
    int slot;
    int i;

    for (i = 0; i < FLINCHES; i++) {
        Entity *e;

        if (g_flinch[i].left <= 0.0f) {
            continue;
        }
        g_flinch[i].left -= dt;
        if (g_flinch[i].left <= 0.0f) {
            e = by_key(g_flinch[i].key);
            if (e && !e->dead && e->act == ENTITY_HURT) {
                e->act = 0;
            }
        }
    }
    /* A body come back with its block (keep_restore puts dead back, not the radius or a missing picture). */
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            if (ents[i].live && ents[i].dead) {
                ents[i].radius = 0.0f;
                if (!ents[i].pose[ENTITY_DEAD].w[0]) {
                    ents[i].live = 0;
                }
            }
        }
    }
}

void combat_reset(void)
{
    int i;

    for (i = 0; i < FLINCHES; i++) {
        g_flinch[i].left = 0.0f;
    }
}
