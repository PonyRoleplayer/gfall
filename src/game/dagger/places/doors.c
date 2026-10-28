#include "game/dagger/places/doors.h"
#include "game/dagger/formats/sndfile.h"
#include "engine/core/mem.h"
#include "engine/scene/interact.h"

/*
 * See doors.h and docs/interact.md. Numbers are Daggerfall Unity's
 * (RDBLayout, DaggerfallInterior, DaggerfallActionDoor, MeshReader),
 * in world units.
 */

/* A hinged door: a quarter turn back about its own up, over 1.5 s. */
#define DOOR_TURN (-DF_TAU * 0.25f)
#define DOOR_SECS 1.5f
/* DAGGER.SND positions (SoundClips): a dungeon's doors, and a house's. */
#define SND_DUNGEON_OPEN 25
#define SND_DUNGEON_CLOSE 24
#define SND_HOUSE_OPEN 94
#define SND_HOUSE_CLOSE 93
/* The red brick door has "DOR" in its letters but is a wall. */
#define RED_BRICK_DOOR 72100
/* A passage's box: this far either side of its face. */
#define DOOR_DEPTH 8.0f
/* An action's time is in twentieths of a second; its turn in 2048ths of a circle. */
#define ACTION_TICKS 20.0f
#define ACTION_TURN (DF_TAU / 2048.0f)

/* Door archives (MeshReader.LoadVertices): a climate's are these plus 100s. */
#define ARC_BUILDING 74
#define ARC_DUNGEON 56
#define ARC_RUINS 331
#define ARC_SCOURG 156
#define ARC_EXIT 95

/* Action flags (DFBlock.RdbActionFlags) and triggers (RdbTriggerFlags). */
#define ACT_TRANSLATE 1
#define ACT_ROTATE 8
#define ACT_TELEPORT 14
#define TRIG_COLLIDE1 1
#define TRIG_COLLIDE3 3
#define TRIG_DIRECT 2
#define TRIG_DIRECT6 6
#define TRIG_MULTI 8
#define TRIG_COLLIDE9 9

/* RdbActionAxes: 1 -X, 2 +X, 3 -Y, 4 +Y, 5 -Z, 6 +Z. */
static void axis_vector(int axis, float size, int turn, float *x, float *y, float *z)
{
    *x = 0.0f;
    *y = 0.0f;
    *z = 0.0f;
    /*
     * A turn goes the axis's way. A slide along X or Z goes against it,
     * as GetTranslationActionVector has it (the file's X and Z are the
     * other way about); along Y with it.
     */
    switch (axis) {
    case 1:
        *x = turn ? -size : size;
        break;
    case 2:
        *x = turn ? size : -size;
        break;
    case 3:
        *y = -size;
        break;
    case 4:
        *y = size;
        break;
    case 5:
        *z = turn ? -size : size;
        break;
    case 6:
        *z = turn ? size : -size;
        break;
    default:
        break;
    }
}

static int tag_is(const ModelAct *a, const char *t)
{
    return a->tag[0] == t[0] && a->tag[1] == t[1] && a->tag[2] == t[2];
}

/* RDBLayout.IsActionDoor. */
static int hinged_door(const ModelAct *a, int place)
{
    if (place == PLACE_INSIDE) {
        return a->hinged;
    }
    if (place != PLACE_DUNGEON || a->id == RED_BRICK_DOOR) {
        return 0;
    }
    return tag_is(a, "DOR") || tag_is(a, "DDR") || tag_is(a, "NEW") || tag_is(a, "CAV");
}

/* The starting lock, from the top nibble of the trigger word (RDBLayout.AddActionDoor). */
static int start_lock(df_u32 trigger)
{
    static const int locks[16] = {
        0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
        0x10, 0x12, 0x14, 0x19, 0x1e, 0x32, 0x80, 0xff
    };

    return locks[(trigger >> 4) & 15];
}

static const SoundClip *clip_by_id(int id)
{
    int at;

    if (id <= 0) {
        return 0;
    }
    at = sndfile_by_id((df_u32)id);
    return at >= 0 ? sndfile_clip(at) : 0;
}

static Mover *new_mover(DoorBuild *b, int model)
{
    Mover *m;

    if (b->nmover >= b->mover_cap) {
        return 0;
    }
    m = &b->mover[b->nmover++];
    mem_zero(m, sizeof(Mover));
    m->inst = &b->inst[model];
    m->model = model;
    m->next = -1;
    return m;
}

static Usable *new_use(DoorBuild *b, int kind, const char *label, float reach)
{
    Usable *u;

    if (b->nuse >= b->use_cap) {
        return 0;
    }
    u = &b->use[b->nuse++];
    mem_zero(u, sizeof(Usable));
    u->kind = kind;
    u->model = -1;
    u->sprite = -1;
    u->mover = -1;
    u->reach = reach;
    u->label = label;
    return u;
}

/*
 * An action record's motion (RDBLayout.AddAction): flag 1 slides, 8
 * turns, 2..7 slide 8 units an axis step along their own axis in 2.5 s.
 * The rest (text, spells, traps, a teleport) do not move: they pass the
 * chain on. A few models Daggerfall Unity mends by their letters.
 */
static void action_motion(Mover *m, const ModelAct *a)
{
    float size;
    int axis;
    int secs20;

    axis = (int)a->axis;
    size = (float)a->magnitude;
    secs20 = (int)a->duration;
    if (a->flags == ACT_ROTATE) {
        if (axis == 13 && tag_is(a, "TRP")) {
            axis = 2;
            size = 400.0f;
        }
        if (axis == 12 && tag_is(a, "C0E")) {
            axis = 3;
            secs20 = 0;
        }
        axis_vector(axis, size * ACTION_TURN, 1, &m->rx, &m->ry, &m->rz);
    } else if (a->flags == ACT_TRANSLATE) {
        if (axis == 11 && tag_is(a, "C0X")) {
            axis = 3;
            m->once = 1;
        }
        axis_vector(axis, size, 0, &m->tx, &m->ty, &m->tz);
    } else if (a->flags >= 2 && a->flags <= 7) {
        secs20 = 50;
        axis_vector((int)a->flags, (float)a->axis * 8.0f, 0, &m->tx, &m->ty, &m->tz);
    }
    if (tag_is(a, "LID")) {
        m->rx = 0.0f;
        m->ry = 0.0f;
        m->rz = -DF_TAU * 0.25f;
    } else if (tag_is(a, "WHE")) {
        m->rx = 0.0f;
        m->ry = -DF_TAU;
        m->rz = 0.0f;
    }
    m->secs = (float)secs20 / ACTION_TICKS;
    m->go_sound = clip_by_id((int)a->sound);
    m->back_sound = m->go_sound;
}

/* What a face painted from archive, record leads to here, DOOR_NONE for a wall. */
static int door_face(int archive, int record, int place, const ModelAct *a)
{
    int base;

    base = archive;
    if (archive > 100 && archive != ARC_RUINS && archive != ARC_SCOURG) {
        base = archive % 100;
    }
    switch (place) {
    case PLACE_TOWN:
        if (base == ARC_BUILDING) {
            /* The block's own props have doors painted on them too; only a building is entered. */
            return a->record >= 0 ? DOOR_ENTER : DOOR_NONE;
        }
        if (base == ARC_DUNGEON || (base == ARC_RUINS && record > 0)) {
            return DOOR_DUNGEON;
        }
        return DOOR_NONE;
    case PLACE_INSIDE:
        return base == ARC_BUILDING ? DOOR_LEAVE : DOOR_NONE;
    default:
        return base == ARC_EXIT ? DOOR_EXIT : DOOR_NONE;
    }
}

/*
 * Each door face of model i, a passage: its box about the face, across
 * it, up it and DOOR_DEPTH either side, facing the way the face is seen
 * from (its normal).
 */
static void add_passages(DoorBuild *b, int i)
{
    static const char *const words[] = { "", "enter", "leave", "dungeon", "exit" };
    const Instance *in;
    const Mesh *mesh;
    int f;

    in = &b->inst[i];
    mesh = in->mesh;
    if (!mesh || !mesh->faces) {
        return;
    }
    for (f = 0; f < mesh->nfaces; f++) {
        const Face *face;
        const Tri *tri;
        Usable *u;
        float nx, ny, nz, len;
        float lo_t, hi_t, lo_y, hi_y, sum_n;
        int what;
        int t;
        int k;
        int n;

        face = &mesh->faces[f];
        if (face->ntris < 1 || face->first < 0) {
            continue;
        }
        tri = &mesh->tris[face->first];
        what = door_face((int)tri->tex_file, (int)tri->tex_image, b->place, &b->act[i]);
        if (what == DOOR_NONE) {
            continue;
        }
        nx = in->m[0] * face->nx + in->m[1] * face->ny + in->m[2] * face->nz;
        ny = in->m[3] * face->nx + in->m[4] * face->ny + in->m[5] * face->nz;
        nz = in->m[6] * face->nx + in->m[7] * face->ny + in->m[8] * face->nz;
        len = df_sqrt(nx * nx + nz * nz);
        /* A door is upright; a painted floor or lid is not walked through. */
        if (len < 0.7f * df_sqrt(nx * nx + ny * ny + nz * nz)) {
            continue;
        }
        nx /= len;
        nz /= len;
        lo_t = hi_t = lo_y = hi_y = sum_n = 0.0f;
        n = 0;
        for (t = 0; t < face->ntris; t++) {
            for (k = 0; k < 3; k++) {
                float x, y, z, across;

                instance_point(in, tri[t].x[k], tri[t].y[k], tri[t].z[k], &x, &y, &z);
                across = nz * x - nx * z;
                if (n == 0 || across < lo_t) {
                    lo_t = across;
                }
                if (n == 0 || across > hi_t) {
                    hi_t = across;
                }
                if (n == 0 || y < lo_y) {
                    lo_y = y;
                }
                if (n == 0 || y > hi_y) {
                    hi_y = y;
                }
                sum_n += nx * x + nz * z;
                n++;
            }
        }
        u = new_use(b, USE_PASSAGE, words[what], DOOR_REACH);
        if (!u) {
            return;
        }
        {
            float mid_t, along;

            mid_t = (lo_t + hi_t) * 0.5f;
            along = sum_n / (float)n;
            /* Back from (across, along) to x, z: across = (nz, -nx), along = (nx, nz). */
            u->cx = nz * mid_t + nx * along;
            u->cz = -nx * mid_t + nz * along;
        }
        u->cy = (lo_y + hi_y) * 0.5f;
        u->nx = nx;
        u->nz = nz;
        u->hw = (hi_t - lo_t) * 0.5f;
        u->hh = (hi_y - lo_y) * 0.5f;
        u->hd = DOOR_DEPTH;
        u->what = what;
        if (what == DOOR_ENTER) {
            u->tag = ((int)b->block[i] << 8) | (b->act[i].record & 0xff);
        } else if (what == DOOR_LEAVE) {
            u->tag = b->act[i].record;
        }
    }
}

/*
 * A teleport's target (DFU's Teleport: the player to its NextObject's
 * position and turn): the object at the action's next offset in the
 * same block, an editor marker as a rule (a flat: no turn, so facing
 * +z), or a model (facing its own +z). It ends its chain. With none
 * found it is no teleport: it passes nothing on, as before.
 */
/*
 * A mover's usable carries its action record's numbers, for trigview:
 * what the action's kind (RdbActionFlags: 1 slide, 8 turn, 14 teleport),
 * tag its trigger (RdbTriggerFlags: 1, 3 walked into; 2, 6, 8 by hand).
 */
static void act_numbers(Usable *u, const ModelAct *a)
{
    u->what = (int)a->flags;
    u->tag = (int)(a->trigger & 15);
}

static void teleport_target(DoorBuild *b, int i, Mover *m)
{
    const ModelAct *a;
    int j;

    a = &b->act[i];
    for (j = 0; j < b->nmark; j++) {
        if (b->mark_block[j] == b->block[i] && b->mark[j].pos == (df_u32)a->next) {
            m->to_x = b->mark[j].x;
            m->to_y = b->mark[j].y;
            m->to_z = b->mark[j].z;
            m->to_yaw = 0.0f;
            m->teleport = 1;
            return;
        }
    }
    for (j = 0; j < b->ninst; j++) {
        if (b->block[j] == b->block[i] && b->act[j].pos == (df_u32)a->next) {
            float fx, fy, fz;

            m->to_x = b->inst[j].x;
            m->to_y = b->inst[j].y;
            m->to_z = b->inst[j].z;
            instance_point(&b->inst[j], 0.0f, 0.0f, 1.0f, &fx, &fy, &fz);
            m->to_yaw = df_atan2(fx - b->inst[j].x, fz - b->inst[j].z);
            m->teleport = 1;
            return;
        }
    }
}

int doors_build(DoorBuild *b)
{
    int *mover_of;
    int i;

    b->nuse = 0;
    b->nmover = 0;
    if (b->ninst < 1) {
        return 1;
    }
    mover_of = (int *)mem_alloc((df_u32)b->ninst * (df_u32)sizeof(int));
    if (!mover_of) {
        return 0;
    }
    for (i = 0; i < b->ninst; i++) {
        const ModelAct *a;
        Mover *m;
        Usable *u;

        a = &b->act[i];
        mover_of[i] = -1;
        /*
         * A door whose action is a teleport (DFU gives a door its action
         * record): a portal. Used, it does not swing onto the wall behind
         * it; it sends the player to its next object (below).
         */
        if (hinged_door(a, b->place) && a->flags == ACT_TELEPORT) {
            m = new_mover(b, i);
            if (!m) {
                continue;
            }
            /* 2 until its target is found (teleport_target); then 1. */
            m->teleport = 2;
            m->door = 1;
            m->lock = start_lock(a->trigger);
            m->go_sound = sndfile_clip(SND_DUNGEON_OPEN);
            mover_of[i] = b->nmover - 1;
            u = new_use(b, USE_MOVER, "door", DOOR_REACH);
            if (u) {
                u->model = i;
                u->mover = mover_of[i];
                act_numbers(u, a);
            }
            continue;
        }
        if (hinged_door(a, b->place)) {
            m = new_mover(b, i);
            if (!m) {
                continue;
            }
            m->ry = DOOR_TURN;
            m->secs = DOOR_SECS;
            m->door = 1;
            if (b->place == PLACE_INSIDE) {
                m->go_sound = sndfile_clip(SND_HOUSE_OPEN);
                m->back_sound = sndfile_clip(SND_HOUSE_CLOSE);
            } else {
                m->lock = start_lock(a->trigger);
                m->go_sound = sndfile_clip(SND_DUNGEON_OPEN);
                m->back_sound = sndfile_clip(SND_DUNGEON_CLOSE);
            }
            mover_of[i] = b->nmover - 1;
            u = new_use(b, USE_MOVER, "door", DOOR_REACH);
            if (u) {
                u->model = i;
                u->mover = mover_of[i];
                act_numbers(u, a);
            }
            continue;
        }
        if (b->place == PLACE_DUNGEON && a->flags) {
            int trig;

            m = new_mover(b, i);
            if (!m) {
                continue;
            }
            action_motion(m, a);
            m->teleport = (df_u8)(a->flags == ACT_TELEPORT ? 2 : 0);
            mover_of[i] = b->nmover - 1;
            /*
             * Used by hand (2, 6, 8, 9), or by walking into it (1, 3, 9: a
             * teleporter, a plate); the rest start only from the one before them.
             */
            trig = (int)(a->trigger & 15);
            {
                int hand;
                int touch;

                hand = trig == TRIG_DIRECT || trig == TRIG_DIRECT6 || trig == TRIG_MULTI
                    || trig == TRIG_COLLIDE9;
                touch = trig == TRIG_COLLIDE1 || trig == TRIG_COLLIDE3 || trig == TRIG_COLLIDE9;
                if (hand || touch) {
                    u = new_use(b, USE_MOVER, "action", hand ? DOOR_REACH : 0.0f);
                    if (u) {
                        u->model = i;
                        u->mover = mover_of[i];
                        u->touch = (df_u8)touch;
                        act_numbers(u, a);
                    }
                }
            }
            continue;
        }
        add_passages(b, i);
    }
    /* Chains: the next object, by its offset in the same block's file. */
    for (i = 0; i < b->ninst; i++) {
        const ModelAct *a;
        int j;

        a = &b->act[i];
        if (mover_of[i] < 0 || a->next < 0 || !a->flags) {
            continue;
        }
        if (b->mover[mover_of[i]].teleport) {
            teleport_target(b, i, &b->mover[mover_of[i]]);
            continue;
        }
        for (j = 0; j < b->ninst; j++) {
            if (b->block[j] == b->block[i] && b->act[j].pos == (df_u32)a->next && mover_of[j] >= 0) {
                b->mover[mover_of[i]].next = mover_of[j];
                break;
            }
        }
    }
    /*
     * A teleport whose target was not found is none: a door swings as
     * any other, an action passes nothing on, as before.
     */
    for (i = 0; i < b->nmover; i++) {
        Mover *m;

        m = &b->mover[i];
        if (m->teleport != 2) {
            continue;
        }
        m->teleport = 0;
        if (m->door) {
            m->ry = DOOR_TURN;
            m->secs = DOOR_SECS;
            m->back_sound = sndfile_clip(SND_DUNGEON_CLOSE);
        }
    }
    /* People, by their flats. */
    for (i = 0; i < b->nflat; i++) {
        Usable *u;

        if (!(b->flat[i].tag & FLAT_PERSON)) {
            continue;
        }
        u = new_use(b, USE_TALK, "person", TALK_REACH);
        if (!u) {
            break;
        }
        u->sprite = i;
        u->what = (int)(b->flat[i].tag & 0x7fff);
    }
    mover_init(b->mover, b->nmover);
    return 1;
}

void doors_landing(const Usable *u, float d, float eye,
                   float *x, float *y, float *z, float *yaw)
{
    *x = u->cx + u->nx * d;
    *y = u->cy - u->hh + eye;
    *z = u->cz + u->nz * d;
    /* Camera yaw 0 looks along +Z, and a quarter turn along +X. */
    *yaw = df_wrap_tau(df_atan2(u->nx, u->nz));
}
