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
/* The keep row's game bytes: health (two bytes, low first), and 1 once it was rolled. */
#define KEEP_HEALTH 0
#define KEEP_ROLLED 2
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
    /* A race's weapon expertise: a Dark Elf's and a Redguard's, a Wood Elf's with a bow. */
    if (w) {
        int race;

        race = 0;
        if (pc->race == RACE_DARK_ELF) {
            race = pc->level / 4;
        } else if (pc->race == RACE_REDGUARD || (pc->race == RACE_WOOD_ELF && w->skill == SKILL_ARCHERY)) {
            race = pc->level / 3;
        }
        chance += race;
        bonus += race;
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
    base = f->skill + COMBAT_PLAYER_ARMOR;
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
    if (!k->game[KEEP_ROLLED]) {
        int h;

        h = combat_roll_health(f);
        k->game[KEEP_HEALTH] = (df_u8)(h & 0xff);
        k->game[KEEP_HEALTH + 1] = (df_u8)((h >> 8) & 0xff);
        k->game[KEEP_ROLLED] = 1;
    }
    return k->game[KEEP_HEALTH] | (k->game[KEEP_HEALTH + 1] << 8);
}

static void set_health(Keep *k, int h)
{
    if (h < 0) {
        h = 0;
    }
    k->game[KEEP_HEALTH] = (df_u8)(h & 0xff);
    k->game[KEEP_HEALTH + 1] = (df_u8)((h >> 8) & 0xff);
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

int combat_strike(Character *pc, const Item *weapon, int swing, const Scene *scene, float x, float y, float z,
                  float yaw, float pitch)
{
    Entity *best;
    float best_along;
    float eye[3];
    float fwd[3];
    float cp;
    int slot;

    /* The swing tires, struck or not. */
    pc->fatigue -= COMBAT_SWING_FATIGUE;
    if (pc->fatigue < 0) {
        pc->fatigue = 0;
    }
    eye[0] = x;
    eye[1] = y;
    eye[2] = z;
    cp = df_cos(pitch);
    fwd[0] = df_sin(yaw) * cp;
    fwd[1] = df_sin(pitch);
    fwd[2] = df_cos(yaw) * cp;
    best = 0;
    best_along = 0.0f;
    /* Walls by the scene's collision grid, a ray as thin as rig_third's (the body sets its own shape again at its step). */
    if (!scene || !collide_use(scene)) {
        return 0;
    }
    collide_set_shape(1.0f, 1.0f);
    /* The nearest creature in front, within reach and its own half width of the line of the eye. */
    for (slot = 0; slot < stream_slots(); slot++) {
        Entity *ents;
        int first;
        int n;
        int i;

        ents = stream_slot_entities(slot, &first, &n);
        for (i = 0; ents && i < n; i++) {
            Entity *e;
            float c[3];
            float along;
            float off[3];
            float side;
            float half;
            int k;

            e = &ents[i];
            if (!e->live || e->dead || dunstream_kind(e, 0) < 0) {
                continue;
            }
            c[0] = e->x;
            c[1] = e->y + entity_height(e) * 0.5f;
            c[2] = e->z;
            along = 0.0f;
            for (k = 0; k < 3; k++) {
                along += (c[k] - eye[k]) * fwd[k];
            }
            half = (float)entity_pose(e)->w[0] * 0.5f;
            if (along <= 0.0f || along > COMBAT_REACH + half || (best && along >= best_along)) {
                continue;
            }
            for (k = 0; k < 3; k++) {
                off[k] = c[k] - eye[k] - fwd[k] * along;
            }
            side = df_sqrt(off[0] * off[0] + off[1] * off[1] + off[2] * off[2]);
            if (side > half + 10.0f || !combat_sight(scene, eye, c)) {
                continue;
            }
            best = e;
            best_along = along;
        }
    }
    if (!best) {
        /* Nothing in reach: the swing whistles (DFU plays it only when nothing was struck). */
        sfx_play(weapon ? SND_SWING_MEDIUM : SND_SWING_HIGH);
        return 0;
    }
    {
        const EnemyKind *kind;
        Foe f;
        Blow b;
        Keep *row;
        int health;
        int behind;

        kind = enemy_kind(dunstream_kind(best, 0));
        if (!kind) {
            return 0;
        }
        /* Facing away: its forward and the way to the player point apart. */
        behind = df_sin(best->yaw) * (x - best->x) + df_cos(best->yaw) * (z - best->z) < 0.0f;
        combat_foe(&f, kind, pc->level, behind);
        health = health_of(best, &f, &row);
        combat_blow(pc, weapon, swing, &f, &b);
        /* Struck at all, it knows: passive or asleep, it turns on the player. */
        foes_alert(best);
        if (b.ineffective) {
            interact_say("This weapon has no effect.");
            sfx_play(weapon ? SND_SWING_MEDIUM : SND_SWING_HIGH);
            return 1;
        }
        if (!b.hit || b.damage <= 0 || !row) {
            /* Nothing taken: a weapon on a creature that parries rings; anything else whistles. */
            if (weapon && kind->parries) {
                sfx_at(sfx_pick(SND_PARRY_FIRST, SND_PARRIES), best->x, best->y + entity_height(best) * 0.5f, best->z);
            } else {
                sfx_play(weapon ? SND_SWING_MEDIUM : SND_SWING_HIGH);
            }
            return 1;
        }
        sfx_at(sfx_pick(SND_HIT_FIRST, SND_HITS), best->x, best->y + entity_height(best) * 0.5f, best->z);
        health -= b.damage;
        set_health(row, health);
        if (health <= 0) {
            lay_down(best);
            sfx_at(SND_BODY_FALL, best->x, best->y, best->z);
        } else {
            flinch(best);
        }
    }
    return 1;
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
