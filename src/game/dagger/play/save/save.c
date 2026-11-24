#include "game/dagger/play/save/save.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/magic/spellbook.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/platform/fs.h"
#include "engine/scene/keep.h"
#include "engine/scene/automap.h"

static GameState g_candidate;
static int g_legacy;
static const char *g_error = "";

const char *save_error(void)
{
    return g_error;
}

static void put_u32(df_u8 *dst, df_u32 value)
{
    int i;

    for (i = 0; i < 4; i++) {
        dst[i] = (df_u8)(value >> (i * 8));
    }
}

static void append_text(char *dst, const char *src, int capacity)
{
    int length;

    length = 0;
    while (length < capacity && dst[length]) {
        length++;
    }
    df_copy(dst + length, src, capacity - length);
}

static int magic_matches(const df_u8 *data)
{
    static const char magic[8] = "OGSAVE\r\n";
    int i;

    for (i = 0; i < 8; i++) {
        if (data[i] != (df_u8)magic[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * Explicit little-endian wire fields. Never write struct layout or pointers.
 *
 * A save is streamed (docs/game.md, Saves): a window of WIRE_WINDOW bytes goes to the file as it fills, or is refilled from it, so the size of a save is not held in memory and
 * has no limit but the disk. Writing is two passes over the same state, the first only counting (the header holds the size and the checksum, which come before the bytes); the rows of the keep and
 * the automap are written from the engine's own arrays and read into a staged heap (state_stage_*), taken by the restore once the whole save has been judged.
 */
#define WIRE_WINDOW 65536u
/* The least bytes a keep row and an automap row take on the wire: a count that could not fit in what is left of the file is refused before anything is allocated. */
#define WIRE_KEEP_LEAST 52u
#define WIRE_MAP_LEAST 20u
typedef struct Wire {
    df_u32 pos, limit;
    int read, bad, version;
    /* Writing: count only (nothing leaves), the checksum of the bytes after the header, and the file failing. */
    int count_only, io_failed;
    df_u32 sum;
    /* Reading: the file the window refills from, and where in it the window starts. */
    FsFile *file;
    df_u32 base;
    df_u32 fill;
    /* Reading: where the rows go (0 for a read that only wants the header's facts), and what the quests' section holds. */
    Keep *keeps;
    AutomapRow *maprows;
    df_u8 *quest;
    int discard;
    df_u8 win[WIRE_WINDOW];
} Wire;
static Wire g_w;
/* A place to read the quests' bytes into when a save is only being looked at. */
static df_u8 g_scratch[4096];

#define FNV_BASIS 2166136261u
#define FNV_PRIME 16777619u

static void wire_flush(Wire *w)
{
    if (w->fill && !w->count_only && !fs_append(w->win, w->fill)) { w->bad = 1; w->io_failed = 1; }
    w->fill = 0;
}

static void wire_bytes(Wire *w, void *data, df_u32 size)
{
    df_u8 *bytes;
    df_u32 done, n, i;

    if (w->bad || size > w->limit - w->pos) {
        w->bad = 1;
        return;
    }
    bytes = (df_u8 *)data;
    if (w->read) {
        for (done = 0; done < size; done += n) {
            if (w->pos < w->base || w->pos >= w->base + w->fill) {
                w->base = w->pos;
                w->fill = w->limit - w->pos < WIRE_WINDOW ? w->limit - w->pos : WIRE_WINDOW;
                if (!fs_read_at(w->file, w->base, w->win, w->fill)) { w->bad = 1; w->fill = 0; return; }
            }
            n = w->fill - (w->pos - w->base);
            if (n > size - done) { n = size - done; }
            mem_copy(bytes + done, w->win + (w->pos - w->base), n);
            w->pos += n;
        }
        return;
    }
    for (i = 0; i < size; i++) { w->sum = (w->sum ^ bytes[i]) * FNV_PRIME; }
    if (w->count_only) {
        w->pos += size;
        return;
    }
    for (done = 0; done < size; done += n) {
        n = WIRE_WINDOW - w->fill;
        if (n > size - done) { n = size - done; }
        mem_copy(w->win + w->fill, bytes + done, n);
        w->fill += n;
        w->pos += n;
        if (w->fill == WIRE_WINDOW) {
            wire_flush(w);
            if (w->bad) { return; }
        }
    }
}

static void wire_u32(Wire *w, df_u32 *v)
{
    df_u8 b[4];

    if (!w->read) {
        put_u32(b, *v);
    }
    wire_bytes(w, b, 4);
    if (w->read && !w->bad) {
        *v = df_ru32(b);
    }
}

static void wire_int(Wire *w, int *v)
{
    df_u32 u;

    u = (df_u32)*v;
    wire_u32(w, &u);
    if (w->read) {
        *v = (int)u;
    }
}

static void wire_float(Wire *w, float *v)
{
    df_u32 u;

    mem_copy(&u, v, 4);
    wire_u32(w, &u);
    if (w->read) {
        mem_copy(v, &u, 4);
    }
}

static void wire_character(Wire *w, CharacterState *c)
{
    int i;

    wire_bytes(w, c->name, NAME_LEN);
    wire_int(w, &c->race);
    wire_int(w, &c->female);
    wire_int(w, &c->klass);
    for (i = 0; i < ATTRS; i++) {
        wire_int(w, &c->attribute[i]);
    }
    for (i = 0; i < SKILLS; i++) {
        wire_int(w, &c->skill[i]);
    }
    wire_int(w, &c->health);
    wire_int(w, &c->max_health);
    wire_int(w, &c->fatigue);
    wire_int(w, &c->magicka);
    wire_int(w, &c->max_magicka);
    wire_int(w, &c->spell_points_bonus);
    wire_int(w, &c->level);
    wire_int(w, &c->gold);
    wire_int(w, &c->reflexes);
    wire_int(w, &c->class_flags);
    wire_int(w, &c->conditions);
    wire_int(w, &c->face);
    wire_int(w, &c->weapon);
    wire_int(w, &c->weapon_sound);
    for (i = 0; i < 10; i++) {
        wire_int(w, &c->reputation[i]);
    }
    wire_int(w, &c->reaction);
    wire_int(w, &c->arrows);
    wire_int(w, &c->gifts);
    for (i = 0; i < 32; i++) {
        wire_int(w, &c->gift[i].group);
        wire_int(w, &c->gift[i].index);
        wire_int(w, &c->gift[i].material);
    }
}

static void wire_df_u32_politics(Wire *w, PoliticsState *po)
{
    int k;

    wire_u32(w, &po->now_last);
    wire_bytes(w, po->flags, sizeof(po->flags));
    wire_bytes(w, po->groups, sizeof(po->groups));
    for (k = 0; k < REGIONS; k++) { int v = po->persecuted[k]; wire_int(w, &v); po->persecuted[k] = (short)v; }
    wire_bytes(w, po->factor, sizeof(po->factor));
    for (k = 0; k < FACTIONS_MAX; k++) { wire_u32(w, &po->seed[k]); }
    for (k = 0; k < RUMOURS_MOST; k++) {
        NewsRumour *r = &po->rumour[k];
        int v;

        v = r->faction1; wire_int(w, &v); r->faction1 = (short)v;
        v = r->faction2; wire_int(w, &v); r->faction2 = (short)v;
        v = r->kind; wire_int(w, &v); r->kind = (short)v;
        wire_bytes(w, &r->region, 1); wire_bytes(w, &r->flags, 1); wire_bytes(w, &r->used, 1);
        wire_u32(w, &r->expires);
        wire_bytes(w, r->text, RUMOUR_TEXT);
        r->text[RUMOUR_TEXT - 1] = 0;
    }
}

static void wire_career(Wire *w, ClassInfo *k)
{
    int i;

    wire_bytes(w, k->name, 16);
    for (i = 0; i < ATTRS; i++) {
        wire_int(w, &k->attribute[i]);
    }
    wire_int(w, &k->hp_per_level);
    wire_int(w, &k->flags);
    for (i = 0; i < 3; i++) {
        wire_int(w, &k->primary[i]);
        wire_int(w, &k->major[i]);
    }
    for (i = 0; i < 6; i++) {
        wire_int(w, &k->minor[i]);
    }
    wire_int(w, &k->resistance);
    wire_int(w, &k->immunity);
    wire_int(w, &k->tolerance);
    wire_int(w, &k->weakness);
    wire_int(w, &k->absorption);
    wire_int(w, &k->rapid);
    wire_int(w, &k->regeneration);
    wire_int(w, &k->attack);
    wire_int(w, &k->expert);
    wire_int(w, &k->special_flags);
    wire_int(w, &k->forbidden_materials);
    wire_int(w, &k->forbidden_equipment);
    wire_int(w, &k->advancement);
}

/* The curse (schema 18): CURSE_WIRE_BYTES in all. */
static void wire_curse(Wire *w, CurseState *c)
{
    int i;

    wire_u32(w, &c->infection_time); wire_int(w, &c->infection_kind); wire_int(w, &c->dream);
    wire_int(w, &c->disease); wire_int(w, &c->form); wire_int(w, &c->clan); wire_int(w, &c->action);
    for (i = 0; i < ATTRS; i++) { wire_int(w, &c->drained[i]); }
    wire_career(w, &c->saved_class);
    wire_u32(w, &c->kill_time); wire_u32(w, &c->shape_time);
    wire_int(w, &c->health_base); wire_int(w, &c->suspended);
}

static void wire_motion(Wire *w, MotionState *motion)
{
    int i;
    for (i = 0; i < 3; i++) { wire_float(w, &motion->feet[i]); wire_float(w, &motion->safe[i]); }
    wire_float(w, &motion->eye); wire_float(w, &motion->vy); wire_float(w, &motion->top);
    wire_float(w, &motion->vx); wire_float(w, &motion->vz); wire_float(w, &motion->bob_ms);
    wire_int(w, &motion->yaw); wire_int(w, &motion->pitch); wire_int(w, &motion->airborne);
    wire_int(w, &motion->crouched); wire_int(w, &motion->water); wire_int(w, &motion->weight);
    wire_int(w, &motion->grounded); wire_int(w, &motion->have_safe);
}

/* Where row i of the world is on the wire: reading, in the staged array (or a scratch row when only the header's facts are wanted); writing, a copy of the keep's own. */
static Keep *keep_slot(Wire *w, int i, Keep *tmp)
{
    const Keep *live;

    if (!w->read) {
        live = keep_at(i);
        if (live) { *tmp = *live; } else { mem_zero(tmp, (df_u32)sizeof(*tmp)); w->bad = 1; }
        return tmp;
    }
    if (w->keeps) { return &w->keeps[i]; }
    mem_zero(tmp, (df_u32)sizeof(*tmp));
    return tmp;
}

static AutomapRow *map_slot(Wire *w, int i, AutomapRow *tmp)
{
    const AutomapRow *live;

    if (!w->read) {
        live = automap_at(i);
        if (live) { *tmp = *live; } else { mem_zero(tmp, (df_u32)sizeof(*tmp)); w->bad = 1; }
        return tmp;
    }
    if (w->maprows) { return &w->maprows[i]; }
    mem_zero(tmp, (df_u32)sizeof(*tmp));
    return tmp;
}

static void wire_keep(Wire *w, Keep *row)
{
    wire_u32(w, &row->key.place); wire_u32(w, &row->key.thing);
    wire_bytes(w, &row->kind, 1); wire_bytes(w, &row->at_end, 1);
    wire_int(w, &row->lock);
    wire_float(w, &row->x); wire_float(w, &row->y); wire_float(w, &row->z); wire_float(w, &row->yaw);
    wire_bytes(w, &row->dead, 1); wire_bytes(w, &row->moving, 1); wire_bytes(w, &row->act, 1);
    wire_bytes(w, &row->hold, 1); wire_bytes(w, &row->frame, 1); wire_bytes(w, &row->placed, 1);
    wire_bytes(w, row->game, KEEP_GAME_BYTES);
}

static void wire_pixel(Wire *w, df_u16 *pixel)
{
    df_u8 bytes[2] = {0, 0};
    if (!w->read) { bytes[0] = (df_u8)*pixel; bytes[1] = (df_u8)(*pixel >> 8); }
    wire_bytes(w, bytes, 2);
    if (w->read && !w->bad) { *pixel = df_ru16(bytes); }
}

static void wire_where(Wire *w, PlaceWhere *p)
{
    wire_int(w,&p->kind); wire_int(w,&p->located); wire_int(w,&p->region); wire_int(w,&p->index);
    wire_int(w,&p->block); wire_int(w,&p->record); wire_int(w,&p->has_back);
    wire_int(w,&p->back_x); wire_int(w,&p->back_z); wire_int(w,&p->back_yaw);
    wire_int(w,&p->origin_x); wire_int(w,&p->origin_z);
}
static void transfer(Wire *w, GameState *s)
{
    int i, j;

    wire_int(w, &s->version);
    wire_int(w, &s->ready);
    wire_character(w, &s->who);
    wire_career(w, &s->career);
    for (i = 0; i < ATTRS; i++) {
        wire_int(w, &s->base_attribute[i]);
    }
    for (i = 0; i < SKILLS; i++) {
        wire_u32(w, &s->skill_uses[i]);
    }
    for (i = 0; i < EQUIP_SLOTS; i++) {
        wire_u32(w, &s->equipped[i]);
    }
    wire_u32(w, &s->random);
    wire_u32(w, &s->next_low);
    wire_u32(w, &s->minutes);
    wire_u32(w, &s->tick_fraction);
    wire_u32(w, &s->ticks);
    wire_float(w, &s->frame_fraction);
    wire_int(w, &s->settings_flags);
    wire_int(w, &s->mouse_mode);
    wire_int(w, &s->mode);
    wire_int(w, &s->depth);
    for (i = 0; i < MODES; i++) {
        wire_int(w, &s->mode_stack[i]);
    }
    wire_int(w, &s->region);
    wire_int(w, &s->location);
    wire_int(w, &s->environment);
    wire_float(w, &s->x);
    wire_float(w, &s->eye_y);
    wire_float(w, &s->z);
    wire_int(w, &s->yaw);
    wire_int(w, &s->pitch);
    wire_u32(w, &s->move_id);
    wire_u32(w, &s->eye_id);
    wire_u32(w, &s->entity_id);
    wire_u32(w, &s->inventory_id);
    for (i = 0; i < OBJECTS; i++) {
        ObjectInfo *o;
        o = &s->object[i];
        wire_u32(w, &o->id);
        wire_u32(w, &o->parent);
        wire_int(w, &o->type);
        wire_int(w, &o->index);
        wire_int(w, &o->flags);
        wire_int(w, &o->group);
        wire_int(w, &o->material);
        wire_int(w, &o->count);
        wire_int(w, &o->condition);
        if (w->version >= 3) {
            wire_int(w, &o->max_condition); wire_int(w, &o->weight); wire_int(w, &o->value);
            wire_bytes(w, o->name, sizeof(o->name));
            wire_int(w, &o->inventory_image); wire_int(w, &o->dropped_image);
            wire_int(w, &o->armor_type); wire_int(w, &o->color); wire_int(w, &o->message);
            wire_u32(w, &o->place_key); wire_u32(w, &o->thing_key);
            wire_float(w, &o->x); wire_float(w, &o->y); wire_float(w, &o->z);
        }
        if (w->version >= 4) { wire_int(w,&o->spell_icon); }
    }
    wire_int(w, &s->motion_valid);
    wire_motion(w, &s->motion);
    wire_int(w, &s->world_count);
    if (w->bad) { return; }
    if (s->world_count < 0 || (w->read && (df_u32)s->world_count > (w->limit - w->pos) / WIRE_KEEP_LEAST)) { w->bad = 1; return; }
    if (w->read && !w->discard) {
        w->keeps = state_stage_keep(s->world_count);
        if (!w->keeps) { g_error = "Not enough memory to read this save"; w->bad = 1; return; }
        mem_zero(w->keeps, (df_u32)s->world_count * (df_u32)sizeof(Keep));
    }
    for (i = 0; i < s->world_count; i++) {
        Keep tmp, *row;

        row = keep_slot(w, i, &tmp);
        wire_keep(w, row);
    }
    wire_int(w, &s->hud_mode); wire_int(w, &s->weapon_ready); wire_int(w, &s->cursor_active);
    wire_bytes(w, s->save_label, sizeof(s->save_label));
    wire_int(w, &s->preview_valid);
    for (i = 0; i < THUMB_PIXELS; i++) { wire_pixel(w, &s->preview[i]); }
    wire_u32(w, &s->event_serial);
    wire_int(w, &s->last_event.kind);
    wire_u32(w, &s->last_event.subject);
    wire_u32(w, &s->last_event.other);
    wire_int(w, &s->last_event.value);
    if (w->version >= 5) {
        wire_u32(w,&s->stealth_minute);
        wire_u32(w,&s->progress.checked); wire_u32(w,&s->progress.raised_lo); wire_u32(w,&s->progress.raised_hi);
        wire_int(w,&s->progress.base_sum); wire_int(w,&s->progress.points); wire_int(w,&s->progress.active);
        for (i = 0; i < ATTRS; i++) { wire_int(w,&s->progress.floor[i]); }
        wire_int(w,&s->survival.breath); wire_u32(w,&s->survival.ticks);
        wire_u32(w,&s->survival.last_tick); wire_float(w,&s->survival.fraction);
        wire_float(w,&s->survival.climb_time); wire_u32(w,&s->survival.climb_minute);
        wire_int(w,&s->survival.climbing); wire_int(w,&s->survival.climb_failed);
        wire_u32(w,&s->encounters.place); wire_u32(w,&s->encounters.serial);
        for (i = 0; i < 6; i++) { wire_int(w,&s->encounters.choices[i]); }
        for (i = 0; i < ENCOUNTERS; i++) {
            EncounterRoll *actor;
            actor = &s->encounters.actor[i];
            wire_u32(w,&actor->key.place); wire_u32(w,&actor->key.thing);
            wire_int(w,&actor->kind); wire_int(w,&actor->archive);
            wire_float(w,&actor->x); wire_float(w,&actor->y); wire_float(w,&actor->z);
        }
    }
    if (w->version >= 6) {
        wire_u32(w,&s->magic.ready); wire_u32(w,&s->magic.last); wire_int(w,&s->magic.paid);
        for (i = 0; i < EFFECTS; i++) {
            Effect *e;
            e = &s->magic.effect[i];
            wire_u32(w,&e->target.place); wire_u32(w,&e->target.thing);
            wire_u32(w,&e->caster.place); wire_u32(w,&e->caster.thing);
            wire_int(w,&e->active); wire_int(w,&e->group); wire_int(w,&e->sub); wire_int(w,&e->rounds);
            wire_int(w,&e->magnitude); wire_int(w,&e->chance); wire_int(w,&e->change); wire_int(w,&e->spell);
            wire_int(w,&e->level); wire_int(w,&e->icon); wire_int(w,&e->element); wire_int(w,&e->permanent);
        }
        for (i = 0; i < AILMENTS; i++) {
            AilmentInfo *a;
            a = &s->magic.ailment[i];
            wire_u32(w,&a->target.place); wire_u32(w,&a->target.thing);
            wire_int(w,&a->active); wire_int(w,&a->id);
            for (j = 0; j < 11; j++) { wire_int(w,&a->stat[j]); }
            wire_int(w,&a->delay); wire_int(w,&a->maximum); wire_int(w,&a->left); wire_int(w,&a->stage);
            for (j = 0; j < 8; j++) { wire_int(w,&a->drained[j]); }
        }
        for (i = 0; i < MISSILES; i++) {
            MissileInfo *m;
            m = &s->magic.missile[i];
            wire_u32(w,&m->caster.place); wire_u32(w,&m->caster.thing);
            wire_int(w,&m->live); wire_int(w,&m->spell); wire_int(w,&m->level);
            wire_int(w,&m->element); wire_int(w,&m->target); wire_int(w,&m->burst);
            wire_float(w,&m->x); wire_float(w,&m->y); wire_float(w,&m->z);
            wire_float(w,&m->dx); wire_float(w,&m->dy); wire_float(w,&m->dz);
        }
        for (i = 0; i < MAGIC_ACTORS; i++) {
            MagicActor *a;
            a = &s->magic.actor[i]; wire_u32(w,&a->key.place); wire_u32(w,&a->key.thing);
            wire_int(w,&a->active); wire_int(w,&a->magicka); wire_int(w,&a->fatigue);
        }
        wire_int(w,&s->magic.recall.valid); wire_u32(w,&s->magic.recall.place);
        wire_float(w,&s->magic.recall.x); wire_float(w,&s->magic.recall.y); wire_float(w,&s->magic.recall.z);
        wire_int(w,&s->magic.recall.yaw); wire_int(w,&s->magic.recall.pitch);

    }

    /* Standing (people and law, slice 1): reputations and powers by the table's order, then each region's legal standing and prices. */
    if (w->version >= 8) {
        StandingState *st;
        int n, k;

        st = &s->standing;
        wire_int(w, &st->ready);
        n = faction_total();
        wire_int(w, &n);
        if (st->ready && n != faction_total()) { w->bad = 1; return; }
        if (st->ready) {
            for (k = 0; k < n; k++) {
                int v;
                v = st->rep[k]; wire_int(w, &v); st->rep[k] = (short)v;
                v = st->power[k]; wire_int(w, &v); st->power[k] = (short)v;
            }
            for (k = 0; k < REGIONS; k++) {
                int v;
                v = st->legal[k]; wire_int(w, &v); st->legal[k] = (short)v;
                v = st->price[k]; wire_int(w, &v); st->price[k] = (unsigned short)v;
            }
        }
    }

    /* Banks (slice 3): each region's balance, loan, its due minute and flags, then the house and the ship. */
    if (w->version >= 9) {
        BankState *b;
        int k;

        b = &s->bank;
        for (k = 0; k < REGIONS; k++) {
            wire_int(w, &b->account[k].balance); wire_int(w, &b->account[k].loan_owed);
            wire_u32(w, &b->account[k].loan_due); wire_int(w, &b->account[k].flags);
        }
        wire_u32(w, &b->house); wire_int(w, &b->house_price);
        wire_int(w, &b->ship); wire_int(w, &b->ship_price);
    }

    /* Lodging (slice 4): the rooms held (building, the minute they run out, the room) and the minute of the last meal. */
    if (w->version >= 10) {
        int k;

        for (k = 0; k < ROOMS_MOST; k++) {
            wire_u32(w, &s->lodging.room[k].building); wire_u32(w, &s->lodging.room[k].expires);
            wire_int(w, &s->lodging.room[k].room);
        }
        wire_u32(w, &s->lodging.last_meal);
    }

    /* Law (slice 5): the crime noted and each region's punishment flags. */
    if (w->version >= 11) {
        int k;

        wire_int(w, &s->law.crime);
        for (k = 0; k < REGIONS; k++) {
            int v;
            v = s->law.punishment[k]; wire_int(w, &v); s->law.punishment[k] = (unsigned char)v;
        }
    }

    /* Guilds (slice 9): the memberships (kind, rank, faction, the minute of the rank, the armour received), the crime counts and the invitations. */
    if (w->version >= 12) {
        int k;

        for (k = 0; k < MEMBERSHIPS; k++) {
            Membership *m;

            m = &s->guilds.m[k];
            wire_int(w, &m->used); wire_int(w, &m->kind); wire_int(w, &m->rank); wire_int(w, &m->faction);
            wire_u32(w, &m->rank_time); wire_u32(w, &m->armor);
        }
        wire_int(w, &s->guilds.thieves_count); wire_int(w, &s->guilds.brotherhood_count);
        wire_u32(w, &s->guilds.thieves_invite); wire_u32(w, &s->guilds.brotherhood_invite);
        wire_u32(w, &s->guilds.last_training); wire_u32(w, &s->guilds.daedra_met);
        wire_int(w, &s->guilds.blessing.active); wire_int(w, &s->guilds.blessing.target); wire_int(w, &s->guilds.blessing.amount);
        wire_int(w, &s->guilds.blessing.region); wire_u32(w, &s->guilds.blessing.end);
    }

    /* The spells the player made (schema 13): the 89-byte records, each used or not. */
    if (w->version >= 13) {
        int k;

        for (k = 0; k < CUSTOM_SPELLS; k++) {
            wire_int(w, &s->custom_spell[k].used);
            wire_bytes(w, s->custom_spell[k].record, 89);
        }
    }

    /* The items made at the item maker (schema 14). */
    if (w->version >= 14) {
        int k, j;

        for (k = 0; k < ENCHANTED_ITEMS; k++) {
            ItemEnchant *e;

            e = &s->enchant[k];
            wire_u32(w, &e->item); wire_int(w, &e->identified);
            for (j = 0; j < 10; j++) { wire_int(w, &e->type[j]); wire_int(w, &e->param[j]); }
        }
    }

    /* The last kill (schema 15). */
    if (w->version >= 15) { wire_u32(w, &s->last_kill); }

    /* The quests (schema 16): a length and the bytes quest.c packed. */
    if (w->version >= 16) {
        wire_int(w, &s->quest_len);
        if (w->bad) { return; }
        if (s->quest_len < 0 || (w->read && (df_u32)s->quest_len > w->limit - w->pos)) { w->bad = 1; return; }
        if (w->read) {
            df_u8 *blob;

            /* A save only asked about (the slot list) skips the bytes in a scratch of its own, a piece at a time. */
            if (w->discard) {
                int left;

                for (left = s->quest_len; left > 0 && !w->bad; left -= (int)sizeof(g_scratch)) { wire_bytes(w, g_scratch, (df_u32)(left < (int)sizeof(g_scratch) ? left : (int)sizeof(g_scratch))); }
            } else {
                blob = state_stage_quest(s->quest_len);
                if (!blob) { g_error = "Not enough memory to read this save"; w->bad = 1; return; }
                wire_bytes(w, blob, (df_u32)s->quest_len);
            }
        } else {
            wire_bytes(w, w->quest, (df_u32)s->quest_len);
        }
    }

    if (w->version >= 7) {
        WorldState *world;
        world = &s->world_state;
        wire_where(w,&world->where); wire_where(w,&world->recall_where);
        wire_int(w,&world->px); wire_int(w,&world->py);
        wire_bytes(w,world->known,KNOWN_BYTES); wire_int(w,&world->map_count);
        if (w->bad) { return; }
        if (world->map_count < 0 || (w->read && (df_u32)world->map_count > (w->limit - w->pos) / WIRE_MAP_LEAST)) { w->bad = 1; return; }
        if (w->read && !w->discard) {
            w->maprows = state_stage_map(world->map_count);
            if (!w->maprows) { g_error = "Not enough memory to read this save"; w->bad = 1; return; }
        }
        for (i = 0; i < world->map_count; i++) {
            AutomapRow tmp, *row;

            row = map_slot(w, i, &tmp);
            wire_u32(w,&row->place); wire_u32(w,&row->mesh);
            wire_int(w,&row->x); wire_int(w,&row->y); wire_int(w,&row->z);
        }
    }

    if (w->version >= 18) { wire_curse(w, &s->curse); }

    /* The politics (schema 20): the factions' alliances and enmities, then the region flags, the factors and the rumours. */
    if (w->version >= 20) {
        StandingState *st = &s->standing;
        PoliticsState *po = &s->politics;
        int k, j;

        wire_int(w, &po->ready);
        if (st->ready) {
            for (k = 0; k < faction_total(); k++) {
                for (j = 0; j < FACTION_LINKS; j++) {
                    int v;
                    v = st->ally[k][j]; wire_int(w, &v); st->ally[k][j] = (short)v;
                    v = st->enemy[k][j]; wire_int(w, &v); st->enemy[k][j] = (short)v;
                }
            }
        }
        wire_df_u32_politics(w, po);
    }

    if (w->version >= 24) { wire_u32(w, &s->motion.mount); }

    /* The world's seed (schema 23). */
    if (w->version >= 23) {
        wire_u32(w, &s->world_seed);
        wire_u32(w, &s->sweep_epoch);
        wire_int(w, &s->places);
        if (s->places < 0 || s->places > PLACE_ROWS) { w->bad = 1; return; }
        for (i = 0; i < s->places; i++) {
            PlaceRow *row = &s->place[i];

            wire_u32(w, &row->place); wire_u32(w, &row->seed_epoch); wire_u32(w, &row->last_epoch); wire_int(w, &row->roll_level);
        }
    }

    /* The ship's way back (schema 22). */
    if (w->version >= 22) {
        ShipBack *sh = &s->world_state.ship;

        wire_int(w, &sh->valid); wire_where(w, &sh->where);
        wire_float(w, &sh->x); wire_float(w, &sh->y); wire_float(w, &sh->z);
        wire_int(w, &sh->yaw); wire_int(w, &sh->pitch);
    }

    /* The house's region and town names (schema 21). */
    if (w->version >= 21) {
        wire_bytes(w, s->bank.house_region, BANK_PLACE_NAME); wire_bytes(w, s->bank.house_town, BANK_PLACE_NAME);
        s->bank.house_region[BANK_PLACE_NAME - 1] = 0; s->bank.house_town[BANK_PLACE_NAME - 1] = 0;
    }

    /* The biography's lasting modifiers (schema 19). */
    if (w->version >= 19) {
        CharacterState *c = &s->who;

        wire_int(w, &c->bio_resist_disease); wire_int(w, &c->bio_resist_magic); wire_int(w, &c->bio_to_hit_against);
        wire_int(w, &c->bio_resist_poison); wire_int(w, &c->bio_fatigue);
    }

    /* What an entity the game made keeps in its keep row (schema 17: ENT_KEPT, docs/entities.md, Kept; a row is the whole entity): after everything else, a record a row. */
    if (w->version >= 17) {
        int k;

        for (k = 0; k < s->world_count; k++) {
            Keep tmp, *row;

            row = keep_slot(w, k, &tmp);
            wire_bytes(w, &row->made, 1); wire_bytes(w, &row->residence, 1);
            wire_u32(w, &row->flags); wire_u32(w, &row->owner);
            wire_int(w, &row->cx); wire_int(w, &row->cz);
        }
    }
}

/* What a failed write says, by what the engine found (fs_last_error, read right after the call that failed): a full disk and a folder that will not take the file are told as such. */
static char g_failure[96];
static const char *save_failure(const char *general)
{
    switch (fs_last_error()) {
    case FS_ERR_DISK_FULL: return "The disk is full: the game was not saved. Free some space and save again.";
    case FS_ERR_ACCESS: return "The save folder cannot be written (read-only or in use): the game was not saved.";
    default: break;
    }
    df_copy(g_failure, general, (int)sizeof(g_failure));
    return g_failure;
}

/* The temporary file kept: 1 when its data reached the disk (the flush and the close both worked; a full disk can show only there). */
static int save_finish(void)
{
    return fs_finish();
}

/* Room for the temporary file before a byte of it is written: the drive's free bytes against the save's size and a cluster's slack (the old slot stays until the new one replaces it, so both are on the disk at once). 1 when there is room or the drive cannot say. */
#define SAVE_SPACE_SLACK 4096u
static int save_room(const char *temp, df_u32 size)
{
    df_u32 free_bytes;

    if (!fs_free_bytes(temp, &free_bytes) || free_bytes >= size + SAVE_SPACE_SLACK) { return 1; }
    log_format(g_failure, (int)sizeof(g_failure), "Not enough disk space to save: %u KB needed, %u KB free.", (unsigned)((size + 1023u) / 1024u), (unsigned)(free_bytes / 1024u));
    return 0;
}

static int path_for(char *path, int slot, const char *suffix)
{
    char digits[5];
    int i;
    if (slot < 1 || slot > 9999) {
        g_error = "Save slot must be 1..9999";
        return 0;
    }
    for (i = 3; i >= 0; i--) {
        digits[i] = (char)('0' + slot % 10);
        slot /= 10;
    }
    digits[4] = 0;
    df_copy(path, SAVE_DIR "\\S", 64);
    append_text(path, digits, 64);
    append_text(path, suffix, 64);
    return 1;
}

/* The header (24 bytes) and the checksum of the rest, read a window at a time: 1 when the file is a save of ours that was written whole. */
static int file_sound(FsFile *f, df_u32 size, df_u32 *version)
{
    df_u8 head[24];
    df_u32 at, n, sum, i;

    if (size < 24 || !fs_read_at(f, 0, head, 24)) { return 0; }
    *version = df_ru32(head + 8);
    if (!magic_matches(head) || *version < 2 || *version > STATE_VERSION || df_ru32(head + 12) != size - 24 || df_ru32(head + 20) != 0) { return 0; }
    sum = FNV_BASIS;
    for (at = 24; at < size; at += n) {
        n = size - at < WIRE_WINDOW ? size - at : WIRE_WINDOW;
        if (!fs_read_at(f, at, g_w.win, n)) { return 0; }
        for (i = 0; i < n; i++) { sum = (sum ^ g_w.win[i]) * FNV_PRIME; }
    }
    return sum == df_ru32(head + 16);
}

/* Reads a save into g_candidate. rows: 1 stages the keep's and the automap's rows and the quests' bytes for the restore; 0 only looks (the slot list, the check after a write). */
static int read_save(const char *path, int rows)
{
    FsFile *f;
    df_u32 size, version;
    Wire *w;

    g_legacy = 0;
    g_error = "";
    if (rows) { state_stage_drop(); }
    f = fs_open_beside(path);
    if (!f) {
        g_error = "Cannot open GFALL save";
        return 0;
    }
    size = fs_size(f);
    if (!file_sound(f, size, &version)) {
        fs_close(f);
        g_error = size < 24 ? "Incomplete GFALL save" : "Unsupported or damaged GFALL save";
        return 0;
    }
    mem_zero(&g_candidate, sizeof(g_candidate));
    w = &g_w;
    w->pos = 24;
    w->limit = size;
    w->version = (int)version;
    w->read = 1;
    w->bad = 0;
    w->io_failed = 0;
    w->file = f;
    w->base = 0;
    w->fill = 0;
    w->keeps = 0;
    w->maprows = 0;
    w->discard = !rows;
    transfer(w, &g_candidate);
    fs_close(f);
    w->file = 0;
    if (w->version == 2 && !w->bad) {
        if (!inventory_upgrade(&g_candidate)) { w->bad = 1; }
        else { g_legacy = 1; }
    }
    if (w->version < 4 && !w->bad) { spell_upgrade(&g_candidate); }
    if (w->version < 5 && !w->bad) {
        g_candidate.version = STATE_VERSION;
        g_candidate.progress.checked = g_candidate.minutes;
        g_candidate.progress.base_sum = progress_sum(&g_candidate.who,&g_candidate.career)-(g_candidate.who.level-1)*15;
    }
    if (w->version < 8) { standing_defaults(&g_candidate.standing); }
    if (w->version < 20 && g_candidate.standing.ready) { standing_relations_defaults(&g_candidate.standing); }
    /* A save from before the world seed is given one from what it holds: the same save always gets the same world. */
    if (w->version < 23) {
        g_candidate.world_seed = seed_mix(g_candidate.random, g_candidate.minutes);
        g_candidate.sweep_epoch = g_candidate.minutes / (MINUTES_PER_DAY * PLACE_EPOCH_DAYS);
    }
    if (w->version < STATE_VERSION) { g_candidate.version = STATE_VERSION; }
    spells_sync_custom(&g_candidate);
    if (!w->bad && rows) {
        int i;

        /* A row that makes no sense is a save that does not: judged now, before anything of it is taken. */
        for (i = 0; w->keeps && i < g_candidate.world_count; i++) { if (!state_keep_row_valid(&w->keeps[i])) { w->bad = 1; break; } }
        for (i = 0; w->maprows && i < g_candidate.world_state.map_count; i++) { if (!world_map_row_valid(&w->maprows[i])) { w->bad = 1; break; } }
    }
    if (w->bad || w->pos != size || !state_valid(&g_candidate)) {
        spells_sync_custom(game_state());
        if (rows) { state_stage_drop(); }
        if (!g_error[0]) { g_error = "Invalid GFALL save state"; }
        return 0;
    }
    return 1;
}

/* The quests' bytes, packed into a buffer that is doubled until they fit (quest.c refuses to overrun it); the heap they live in is the caller's to delete. Not enough memory is an error, not a short save. */
#define QUEST_PACK_FIRST 65536
static df_u8 *pack_quests(MemHeap *heap, int *length)
{
    MemHeap *before;
    df_u8 *buffer;
    int cap, len;

    for (cap = QUEST_PACK_FIRST; cap > 0; cap *= 2) {
        before = mem_heap_use(heap);
        buffer = (df_u8 *)mem_alloc((df_u32)cap);
        mem_heap_use(before);
        if (!buffer) { return 0; }
        len = quest_pack(buffer, cap);
        if (len >= 0) { *length = len; return buffer; }
    }
    return 0;
}

/* One pass of the state through the wire: counting (the first) or emitting (the second). */
static int write_pass(int counting, df_u8 *quests)
{
    Wire *w = &g_w;

    w->pos = 24;
    w->limit = 0xFFFFFFFFu;
    w->version = STATE_VERSION;
    w->read = 0;
    w->bad = 0;
    w->io_failed = 0;
    w->count_only = counting;
    w->sum = FNV_BASIS;
    w->fill = 0;
    w->quest = quests;
    transfer(w, &g_candidate);
    if (!w->bad && !counting) { wire_flush(w); }
    return !w->bad;
}

int save_write(int slot)
{
    char path[64], temp[64];
    df_u8 head[24], *quests;
    df_u32 size, sum;
    MemHeap *heap;
    int len, wrote;
    if (!path_for(path, slot, ".SAV") || !path_for(temp, slot, ".TMP")) {
        return 0;
    }
    if (!state_valid(game_state())) {
        g_error = "No valid GFALL session to save";
        return 0;
    }
    if (character_state()->health <= 0) {
        g_error = "A dead character cannot be saved.";
        return 0;
    }
    g_candidate = *game_state();
    /* The rows are written from the keep and the automap themselves: the counts are what they hold now. */
    g_candidate.world_count = keep_count();
    g_candidate.world_state.map_count = automap_count();
    heap = mem_heap_new();
    quests = heap ? pack_quests(heap, &len) : 0;
    if (!quests) {
        if (heap) { mem_heap_delete(heap); }
        g_error = "Not enough memory to pack the quests for a save";
        return 0;
    }
    g_candidate.quest_len = len;
    /* Menus are transient; a loaded save always resumes the world. */
    g_candidate.mode = GAME_WORLD;
    g_candidate.depth = 0;
    mem_zero(g_candidate.mode_stack, sizeof(g_candidate.mode_stack));
    g_error = "";
    if (!write_pass(1, quests)) {
        mem_heap_delete(heap);
        g_error = "The state does not fit a save";
        return 0;
    }
    size = g_w.pos;
    sum = g_w.sum;
    mem_copy(head, "OGSAVE\r\n", 8);
    put_u32(head + 8, STATE_VERSION);
    put_u32(head + 12, size - 24);
    put_u32(head + 16, sum);
    put_u32(head + 20, 0);
    if (!save_room(temp, size)) {
        mem_heap_delete(heap);
        g_error = g_failure;
        return 0;
    }
    if (!fs_make_dirs(SAVE_DIR) || !fs_create(temp)) {
        mem_heap_delete(heap);
        g_error = save_failure("Cannot create GFALL save");
        return 0;
    }
    wrote = fs_append(head, 24) && write_pass(0, quests);
    if (!wrote || g_w.pos != size || g_w.sum != sum) {
        mem_heap_delete(heap);
        g_error = !wrote ? save_failure("Cannot write GFALL save") : "The save did not come out the size it was counted";
        fs_abort();
        return 0;
    }
    mem_heap_delete(heap);
    if (!save_finish()) {
        g_error = save_failure("Cannot finish writing GFALL save");
        fs_delete(temp);
        return 0;
    }
    if (!read_save(temp, 0)) {
        fs_delete(temp);
        g_error = "Cannot verify or replace GFALL save";
        return 0;
    }
    if (!fs_move(temp, path)) {
        fs_delete(temp);
        g_error = save_failure("Cannot verify or replace GFALL save");
        return 0;
    }
    g_error = "";
    return 1;
}

int save_read(int slot)
{
    char path[64];
    int i;
    if (!path_for(path, slot, ".SAV")) {
        return 0;
    }
    /* Win98 replacing rename can leave the previous save under .OLD. */
    if (!fs_exists(path)) {
        append_text(path, ".OLD", 64);
    }
    if (!read_save(path, 1)) { return 0; }
    if (g_candidate.who.health <= 0) {
        g_error = "This save contains a dead character. Choose another save or start a new game.";
        return 0;
    }
    if (!state_restore(&g_candidate)) {
        state_stage_drop();
        return 0;
    }
    if (g_legacy) {
        /* Version 2 retained gifts but had no live equipment or spell contents.
         * Materialise them once, after validating and restoring the snapshot. */
        for (i = 0; i < OBJECTS; i++) {
            const ObjectInfo *item;
            item = &game_state()->object[i];
            if (!item->id || item->type != RECORD_ITEM || item->parent != game_state()->inventory_id) { continue; }
            if (item->group == ITEM_GROUP_MISC && item->index == MISC_SPELLBOOK) { inventory_start_spells(item->id); }
            else if (item->group == 6 || item->group == 12) { inventory_equip(item->id); }
        }
    }
    g_error = "";
    return 1;
}

int save_info(int slot, SaveInfo *info)
{
    char path[64];
    mem_zero(info, sizeof(*info));
    if (!path_for(path, slot, ".SAV")) { return -1; }
    if (!fs_exists(path)) {
        append_text(path, ".OLD", sizeof(path));
        if (!fs_exists(path)) { return 0; }
    }
    if (!read_save(path, 0)) { return -1; }
    df_copy(info->label, g_candidate.save_label[0] ? g_candidate.save_label : g_candidate.who.name, sizeof(info->label));
    df_copy(info->character, g_candidate.who.name, sizeof(info->character));
    info->level = g_candidate.who.level;
    info->minutes = g_candidate.minutes;
    info->preview_valid = g_candidate.preview_valid;
    mem_copy(info->preview, g_candidate.preview, sizeof(info->preview));
    info->status = 1;
    return 1;
}
