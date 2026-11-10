#include "game/ogfall/play/save.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/spellbook.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"

static OgState g_candidate;
static int g_legacy;
static const char *g_error = "";

const char *og_save_error(void)
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

/* Explicit little-endian wire fields. Never write struct layout or pointers. */
#define SAVE_CAP 524288u
static df_u8 g_wire[SAVE_CAP];
typedef struct Wire {
    df_u32 pos, limit;
    int read, bad, version;
} Wire;
static void wire_bytes(Wire *w, void *data, df_u32 size)
{
    if (w->bad || size > w->limit - w->pos) {
        w->bad = 1;
        return;
    }
    if (w->read) {
        mem_copy(data, g_wire + w->pos, size);
    } else {
        mem_copy(g_wire + w->pos, data, size);
    }
    w->pos += size;
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

static void wire_character(Wire *w, OgCharacter *c)
{
    int i;

    wire_bytes(w, c->name, OG_NAME);
    wire_int(w, &c->race);
    wire_int(w, &c->female);
    wire_int(w, &c->klass);
    for (i = 0; i < OG_ATTRS; i++) {
        wire_int(w, &c->attribute[i]);
    }
    for (i = 0; i < OG_SKILLS; i++) {
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

static void wire_career(Wire *w, OgClass *k)
{
    int i;

    wire_bytes(w, k->name, 16);
    for (i = 0; i < OG_ATTRS; i++) {
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

static void wire_motion(Wire *w, OgMotion *motion)
{
    int i;
    for (i = 0; i < 3; i++) { wire_float(w, &motion->feet[i]); wire_float(w, &motion->safe[i]); }
    wire_float(w, &motion->eye); wire_float(w, &motion->vy); wire_float(w, &motion->top);
    wire_float(w, &motion->vx); wire_float(w, &motion->vz); wire_float(w, &motion->bob_ms);
    wire_int(w, &motion->yaw); wire_int(w, &motion->pitch); wire_int(w, &motion->airborne);
    wire_int(w, &motion->crouched); wire_int(w, &motion->water); wire_int(w, &motion->weight);
    wire_int(w, &motion->grounded); wire_int(w, &motion->have_safe);
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

static void wire_where(Wire *w, OgWhere *p)
{
    wire_int(w,&p->kind); wire_int(w,&p->located); wire_int(w,&p->region); wire_int(w,&p->index);
    wire_int(w,&p->block); wire_int(w,&p->record); wire_int(w,&p->has_back);
    wire_int(w,&p->back_x); wire_int(w,&p->back_z); wire_int(w,&p->back_yaw);
    wire_int(w,&p->origin_x); wire_int(w,&p->origin_z);
}
static void transfer(Wire *w, OgState *s)
{
    int i, j;

    wire_int(w, &s->version);
    wire_int(w, &s->ready);
    wire_character(w, &s->who);
    wire_career(w, &s->career);
    for (i = 0; i < OG_ATTRS; i++) {
        wire_int(w, &s->base_attribute[i]);
    }
    for (i = 0; i < OG_SKILLS; i++) {
        wire_u32(w, &s->skill_uses[i]);
    }
    for (i = 0; i < 27; i++) {
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
    for (i = 0; i < OG_MODES; i++) {
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
    for (i = 0; i < OG_OBJECTS; i++) {
        OgObject *o;
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
    if (s->world_count < 0 || s->world_count > OG_WORLD_ROWS) { w->bad = 1; return; }
    for (i = 0; i < s->world_count; i++) { wire_keep(w, &s->world[i]); }
    wire_int(w, &s->hud_mode); wire_int(w, &s->weapon_ready); wire_int(w, &s->cursor_active);
    wire_bytes(w, s->save_label, sizeof(s->save_label));
    wire_int(w, &s->preview_valid);
    for (i = 0; i < OG_THUMB_PIXELS; i++) { wire_pixel(w, &s->preview[i]); }
    wire_u32(w, &s->event_serial);
    wire_int(w, &s->last_event.kind);
    wire_u32(w, &s->last_event.subject);
    wire_u32(w, &s->last_event.other);
    wire_int(w, &s->last_event.value);
    if (w->version >= 5) {
        wire_u32(w,&s->stealth_minute);
        wire_u32(w,&s->progress.checked); wire_u32(w,&s->progress.raised_lo); wire_u32(w,&s->progress.raised_hi);
        wire_int(w,&s->progress.base_sum); wire_int(w,&s->progress.points); wire_int(w,&s->progress.active);
        for (i = 0; i < OG_ATTRS; i++) { wire_int(w,&s->progress.floor[i]); }
        wire_int(w,&s->survival.breath); wire_u32(w,&s->survival.ticks);
        wire_u32(w,&s->survival.last_tick); wire_float(w,&s->survival.fraction);
        wire_float(w,&s->survival.climb_time); wire_u32(w,&s->survival.climb_minute);
        wire_int(w,&s->survival.climbing); wire_int(w,&s->survival.climb_failed);
        wire_u32(w,&s->encounters.place); wire_u32(w,&s->encounters.serial);
        for (i = 0; i < 6; i++) { wire_int(w,&s->encounters.choices[i]); }
        for (i = 0; i < OG_ENCOUNTERS; i++) {
            OgEncounter *actor;
            actor = &s->encounters.actor[i];
            wire_u32(w,&actor->key.place); wire_u32(w,&actor->key.thing);
            wire_int(w,&actor->kind); wire_int(w,&actor->archive);
            wire_float(w,&actor->x); wire_float(w,&actor->y); wire_float(w,&actor->z);
        }
    }
    if (w->version >= 6) {
        wire_u32(w,&s->magic.ready); wire_u32(w,&s->magic.last); wire_int(w,&s->magic.paid);
        for (i = 0; i < OG_EFFECTS; i++) {
            OgEffect *e;
            e = &s->magic.effect[i];
            wire_u32(w,&e->target.place); wire_u32(w,&e->target.thing);
            wire_u32(w,&e->caster.place); wire_u32(w,&e->caster.thing);
            wire_int(w,&e->active); wire_int(w,&e->group); wire_int(w,&e->sub); wire_int(w,&e->rounds);
            wire_int(w,&e->magnitude); wire_int(w,&e->chance); wire_int(w,&e->change); wire_int(w,&e->spell);
            wire_int(w,&e->level); wire_int(w,&e->icon); wire_int(w,&e->element); wire_int(w,&e->permanent);
        }
        for (i = 0; i < OG_AILMENTS; i++) {
            OgAilment *a;
            a = &s->magic.ailment[i];
            wire_u32(w,&a->target.place); wire_u32(w,&a->target.thing);
            wire_int(w,&a->active); wire_int(w,&a->id);
            for (j = 0; j < 11; j++) { wire_int(w,&a->stat[j]); }
            wire_int(w,&a->delay); wire_int(w,&a->maximum); wire_int(w,&a->left); wire_int(w,&a->stage);
            for (j = 0; j < 8; j++) { wire_int(w,&a->drained[j]); }
        }
        for (i = 0; i < OG_MISSILES; i++) {
            OgMissile *m;
            m = &s->magic.missile[i];
            wire_u32(w,&m->caster.place); wire_u32(w,&m->caster.thing);
            wire_int(w,&m->live); wire_int(w,&m->spell); wire_int(w,&m->level);
            wire_int(w,&m->element); wire_int(w,&m->target); wire_int(w,&m->burst);
            wire_float(w,&m->x); wire_float(w,&m->y); wire_float(w,&m->z);
            wire_float(w,&m->dx); wire_float(w,&m->dy); wire_float(w,&m->dz);
        }
        for (i = 0; i < OG_MAGIC_ACTORS; i++) {
            OgMagicActor *a;
            a = &s->magic.actor[i]; wire_u32(w,&a->key.place); wire_u32(w,&a->key.thing);
            wire_int(w,&a->active); wire_int(w,&a->magicka); wire_int(w,&a->fatigue);
        }
        wire_int(w,&s->magic.recall.valid); wire_u32(w,&s->magic.recall.place);
        wire_float(w,&s->magic.recall.x); wire_float(w,&s->magic.recall.y); wire_float(w,&s->magic.recall.z);
        wire_int(w,&s->magic.recall.yaw); wire_int(w,&s->magic.recall.pitch);

    }

    /* Standing (people and law, slice 1): reputations and powers by the table's order, then each region's legal standing and prices. */
    if (w->version >= 8) {
        OgStanding *st;
        int n, k;

        st = &s->standing;
        wire_int(w, &st->ready);
        n = og_faction_count();
        wire_int(w, &n);
        if (st->ready && n != og_faction_count()) { w->bad = 1; return; }
        if (st->ready) {
            for (k = 0; k < n; k++) {
                int v;
                v = st->rep[k]; wire_int(w, &v); st->rep[k] = (short)v;
                v = st->power[k]; wire_int(w, &v); st->power[k] = (short)v;
            }
            for (k = 0; k < OG_REGIONS; k++) {
                int v;
                v = st->legal[k]; wire_int(w, &v); st->legal[k] = (short)v;
                v = st->price[k]; wire_int(w, &v); st->price[k] = (unsigned short)v;
            }
        }
    }

    if (w->version >= 7) {
        OgWorldState *world;
        world = &s->world_state;
        wire_where(w,&world->where); wire_where(w,&world->recall_where);
        wire_int(w,&world->px); wire_int(w,&world->py);
        wire_bytes(w,world->known,OG_KNOWN_BYTES); wire_int(w,&world->map_count);
        if (world->map_count < 0 || world->map_count > OG_MAP_ROWS) { w->bad = 1; return; }
        for (i = 0; i < world->map_count; i++) {
            AutomapRow *row;
            row = &world->map[i]; wire_u32(w,&row->place); wire_u32(w,&row->mesh);
            wire_int(w,&row->x); wire_int(w,&row->y); wire_int(w,&row->z);
        }
    }

}

static df_u32 checksum(const df_u8 *p, df_u32 n)
{
    df_u32 h, i;
    h = 2166136261u;
    for (i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
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
    df_copy(path, OG_SAVE_DIR "\\S", 64);
    append_text(path, digits, 64);
    append_text(path, suffix, 64);
    return 1;
}

static int read_save(const char *path)
{
    FsFile *f;
    df_u32 size;
    Wire w;
    g_legacy = 0;
    f = fs_open_beside(path);
    if (!f) {
        g_error = "Cannot open OGFALL save";
        return 0;
    }
    size = fs_size(f);
    if (size < 24 || size > SAVE_CAP || !fs_read_at(f, 0, g_wire, size)) {
        fs_close(f);
        g_error = "Incomplete OGFALL save";
        return 0;
    }
    fs_close(f);
    if (!magic_matches(g_wire) ||
        (df_ru32(g_wire + 8) < 2 || df_ru32(g_wire + 8) > OG_STATE_VERSION) || df_ru32(g_wire + 12) != size - 24 ||
        df_ru32(g_wire + 20) != 0 || df_ru32(g_wire + 16) != checksum(g_wire + 24, size - 24)) {
        g_error = "Unsupported or damaged OGFALL save";
        return 0;
    }
    mem_zero(&g_candidate, sizeof(g_candidate));
    w.pos = 24;
    w.limit = size;
    w.version = (int)df_ru32(g_wire + 8);
    w.read = 1;
    w.bad = 0;
    transfer(&w, &g_candidate);
    if (w.version == 2 && !w.bad) {
        if (!og_inventory_upgrade(&g_candidate)) { w.bad = 1; }
        else { g_legacy = 1; }
    }
    if (w.version < 4 && !w.bad) { og_spell_upgrade(&g_candidate); }
    if (w.version < 5 && !w.bad) {
        g_candidate.version = OG_STATE_VERSION;
        g_candidate.progress.checked = g_candidate.minutes;
        g_candidate.progress.base_sum = og_progress_sum(&g_candidate.who,&g_candidate.career)-(g_candidate.who.level-1)*15;
    }
    if (w.version < 8) { og_standing_defaults(&g_candidate.standing); }
    if (w.version < OG_STATE_VERSION) { g_candidate.version = OG_STATE_VERSION; }
    if (w.bad || w.pos != size || !og_state_valid(&g_candidate)) {
        g_error = "Invalid OGFALL save state";
        return 0;
    }
    return 1;
}

int og_save_write(int slot)
{
    Wire w;
    char path[64], temp[64];
    df_u32 size;
    if (!path_for(path, slot, ".SAV") || !path_for(temp, slot, ".TMP")) {
        return 0;
    }
    if (!og_state_valid(og_state())) {
        g_error = "No valid OGFALL session to save";
        return 0;
    }
    if (og_character()->health <= 0) {
        g_error = "A dead character cannot be saved.";
        return 0;
    }
    w.pos = 24;
    w.limit = SAVE_CAP;
    w.version = OG_STATE_VERSION;
    w.read = 0;
    w.bad = 0;
    g_candidate = *og_state();
    /* Menus are transient; a loaded save always resumes the world. */
    g_candidate.mode = OG_GAME_WORLD;
    g_candidate.depth = 0;
    mem_zero(g_candidate.mode_stack, sizeof(g_candidate.mode_stack));
    transfer(&w, &g_candidate);
    if (w.bad) {
        g_error = "OGFALL save is too large";
        return 0;
    }
    size = w.pos;
    mem_copy(g_wire, "OGSAVE\r\n", 8);
    put_u32(g_wire + 8, OG_STATE_VERSION);
    put_u32(g_wire + 12, size - 24);
    put_u32(g_wire + 16, checksum(g_wire + 24, size - 24));
    put_u32(g_wire + 20, 0);
    if (!fs_make_dirs(OG_SAVE_DIR) || !fs_create(temp)) {
        g_error = "Cannot create OGFALL save";
        return 0;
    }
    if (!fs_append(g_wire, size)) {
        fs_abort();
        g_error = "Cannot write OGFALL save";
        return 0;
    }
    fs_finish();
    if (!read_save(temp) || !fs_move(temp, path)) {
        fs_delete(temp);
        g_error = "Cannot verify or replace OGFALL save";
        return 0;
    }
    g_error = "";
    return 1;
}

int og_save_read(int slot)
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
    if (!read_save(path)) { return 0; }
    if (g_candidate.who.health <= 0) {
        g_error = "This save contains a dead character. Choose another save or start a new game.";
        return 0;
    }
    if (!og_state_restore(&g_candidate)) {
        return 0;
    }
    if (g_legacy) {
        /* Version 2 retained gifts but had no live equipment or spell contents.
         * Materialise them once, after validating and restoring the snapshot. */
        for (i = 0; i < OG_OBJECTS; i++) {
            const OgObject *item;
            item = &og_state()->object[i];
            if (!item->id || item->type != OG_RECORD_ITEM || item->parent != og_state()->inventory_id) { continue; }
            if (item->group == 27 && item->index == 0) { og_inventory_start_spells(item->id); }
            else if (item->group == 6 || item->group == 12) { og_inventory_equip(item->id); }
        }
    }
    g_error = "";
    return 1;
}

int og_save_info(int slot, OgSaveInfo *info)
{
    char path[64];
    mem_zero(info, sizeof(*info));
    if (!path_for(path, slot, ".SAV")) { return -1; }
    if (!fs_exists(path)) {
        append_text(path, ".OLD", sizeof(path));
        if (!fs_exists(path)) { return 0; }
    }
    if (!read_save(path)) { return -1; }
    df_copy(info->label, g_candidate.save_label[0] ? g_candidate.save_label : g_candidate.who.name, sizeof(info->label));
    df_copy(info->character, g_candidate.who.name, sizeof(info->character));
    info->level = g_candidate.who.level;
    info->minutes = g_candidate.minutes;
    info->preview_valid = g_candidate.preview_valid;
    mem_copy(info->preview, g_candidate.preview, sizeof(info->preview));
    info->status = 1;
    return 1;
}
