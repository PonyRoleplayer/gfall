#include "game/dagger/formats/qbn.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"

/* See qbn.h. */

#define HEADER_BYTES 60
#define COUNTS_AT 16
#define OFFSETS_AT 36
#define HAS_NAMES_AT 15
#define OPERAND_BYTES 15
#define CONSTANT_MARK 0x12345678u
/* The most records a section has (UESP: up to 0xFD); the names' list ends itself. */
#define RECORDS_MOST 0xFD
#define NAMES_MOST 512

/* Each section's record size; 0 for those never used. */
static const int g_record_bytes[QBN_SECTIONS] = { 19, 0, 0, 20, 24, 0, 33, 14, 87, 8, 27 };

static int s16(const df_u8 *p)
{
    return (int)(short)df_ru16(p);
}

static void read_operand(const df_u8 *p, QbnOperand *o)
{
    df_u32 ptr;

    o->negate = p[0] == 1;
    ptr = df_ru32(p + 1);
    o->constant = ptr == CONSTANT_MARK;
    o->section = (int)df_ru16(p + 5);
    o->value = df_ru32(p + 7);
}

/* Room for n records of size bytes in the heap now current, zeroed; 0 for none, *ok cleared when it was not had. */
static void *room(int n, df_u32 size, int *ok)
{
    void *p;

    if (n <= 0) {
        return 0;
    }
    p = mem_alloc((df_u32)n * size);
    if (!p) {
        *ok = 0;
        return 0;
    }
    mem_zero(p, (df_u32)n * size);
    return p;
}

int qbn_parse(const df_u8 *b, df_u32 n, Qbn *out)
{
    int offset[QBN_SECTIONS];
    int ok;
    int s;
    int i;

    mem_zero(out, sizeof(*out));
    if (!b || n < HEADER_BYTES) {
        return 0;
    }
    for (s = 0; s < QBN_SECTIONS; s++) {
        offset[s] = (int)df_ru16(b + OFFSETS_AT + s * 2);
        out->count[s] = s < 10 ? (int)df_ru16(b + COUNTS_AT + s * 2) : 0;
        if (s < 10 && out->count[s] > RECORDS_MOST) {
            return 0;
        }
        if (s < 10 && out->count[s] && (!g_record_bytes[s] || (df_u32)offset[s] + (df_u32)out->count[s] * (df_u32)g_record_bytes[s] > n)) {
            return 0;
        }
    }
    /* The symbols' names run from their offset to a record with an empty name, when the header says they are there. */
    if (b[HAS_NAMES_AT] && offset[QBN_NAMES]) {
        for (i = 0; i < NAMES_MOST && (df_u32)offset[QBN_NAMES] + (df_u32)(i + 1) * 27 <= n
                    && b[offset[QBN_NAMES] + i * 27]; i++) {
        }
        out->count[QBN_NAMES] = i;
    }
    ok = 1;
    out->items = (QbnItem *)room(out->count[QBN_ITEMS], sizeof(QbnItem), &ok);
    out->people = (QbnPerson *)room(out->count[QBN_PEOPLE], sizeof(QbnPerson), &ok);
    out->places = (QbnPlace *)room(out->count[QBN_PLACES], sizeof(QbnPlace), &ok);
    out->timers = (QbnTimer *)room(out->count[QBN_TIMERS], sizeof(QbnTimer), &ok);
    out->foes = (QbnFoe *)room(out->count[QBN_FOES], sizeof(QbnFoe), &ok);
    out->logic = (QbnLogic *)room(out->count[QBN_LOGIC], sizeof(QbnLogic), &ok);
    out->states = (QbnState *)room(out->count[QBN_STATES], sizeof(QbnState), &ok);
    out->names = (QbnName *)room(out->count[QBN_NAMES], sizeof(QbnName), &ok);
    if (!ok) {
        return 0;
    }
    for (i = 0; i < out->count[QBN_ITEMS]; i++) {
        const df_u8 *p;
        QbnItem *it;

        p = b + offset[QBN_ITEMS] + i * 19;
        it = &out->items[i];
        it->index = s16(p);
        it->reward = p[2];
        it->category = (int)df_ru16(p + 3);
        it->category_index = (int)df_ru16(p + 5);
        it->symbol = df_ru32(p + 7);
        it->text_given = (int)df_ru16(p + 15);
        it->text_used = (int)df_ru16(p + 17);
    }
    for (i = 0; i < out->count[QBN_PEOPLE]; i++) {
        const df_u8 *p;
        QbnPerson *who;

        p = b + offset[QBN_PEOPLE] + i * 20;
        who = &out->people[i];
        who->index = s16(p);
        who->gender = p[2];
        who->face = p[3];
        who->kind = (int)df_ru16(p + 4);
        who->faction = (int)df_ru16(p + 6);
        who->symbol = df_ru32(p + 8);
        who->text_tell[0] = (int)df_ru16(p + 16);
        who->text_tell[1] = (int)df_ru16(p + 18);
    }
    for (i = 0; i < out->count[QBN_PLACES]; i++) {
        const df_u8 *p;
        QbnPlace *at;

        p = b + offset[QBN_PLACES] + i * 24;
        at = &out->places[i];
        at->index = (int)df_ru16(p);
        at->general = p[3];
        at->fine = (int)df_ru16(p + 4);
        at->type = s16(p + 6);
        at->door = s16(p + 8);
        at->symbol = df_ru32(p + 12);
        at->text_tell[0] = (int)df_ru16(p + 20);
        at->text_tell[1] = (int)df_ru16(p + 22);
    }
    for (i = 0; i < out->count[QBN_TIMERS]; i++) {
        const df_u8 *p;
        QbnTimer *t;

        p = b + offset[QBN_TIMERS] + i * 33;
        t = &out->timers[i];
        t->index = s16(p);
        t->flags = (int)df_ru16(p + 2);
        t->type = p[4];
        t->least = (long)(df_i32)df_ru32(p + 5);
        t->most = (long)(df_i32)df_ru32(p + 9);
        t->link[0] = (long)(df_i32)df_ru32(p + 21);
        t->link[1] = (long)(df_i32)df_ru32(p + 25);
        t->symbol = df_ru32(p + 29);
    }
    for (i = 0; i < out->count[QBN_FOES]; i++) {
        const df_u8 *p;
        QbnFoe *f;

        p = b + offset[QBN_FOES] + i * 14;
        f = &out->foes[i];
        f->index = p[0];
        f->kind = p[3];
        f->count = (int)df_ru16(p + 4);
        f->symbol = df_ru32(p + 6);
    }
    for (i = 0; i < out->count[QBN_LOGIC]; i++) {
        const df_u8 *p;
        QbnLogic *l;
        int k;

        p = b + offset[QBN_LOGIC] + i * 87;
        l = &out->logic[i];
        l->opcode = (int)df_ru16(p);
        l->flags = (int)df_ru16(p + 2);
        l->count = (int)df_ru16(p + 4);
        /* Eight of classic's files carry records with a bad count: held to the five there are. */
        l->count = l->count < 0 ? 0 : (l->count > QBN_OPERANDS ? QBN_OPERANDS : l->count);
        for (k = 0; k < QBN_OPERANDS; k++) {
            read_operand(p + 6 + k * OPERAND_BYTES, &l->arg[k]);
        }
        l->message = (int)df_ru16(p + 81);
    }
    for (i = 0; i < out->count[QBN_STATES]; i++) {
        const df_u8 *p;
        QbnState *st;

        p = b + offset[QBN_STATES] + i * 8;
        st = &out->states[i];
        st->index = s16(p);
        st->global = p[2] != 0;
        st->global_index = p[3];
        st->symbol = df_ru32(p + 4);
    }
    for (i = 0; i < out->count[QBN_NAMES]; i++) {
        const df_u8 *p;
        int k;

        p = b + offset[QBN_NAMES] + i * 27;
        for (k = 0; k < 20 && p[k]; k++) {
            out->names[i].name[k] = (char)p[k];
        }
        out->names[i].name[k] = 0;
        out->names[i].section = p[20];
        out->names[i].record = (int)df_ru16(p + 21);
    }
    return 1;
}

df_u32 qbn_hash(const char *name)
{
    df_u32 h;

    h = 0;
    while (*name) {
        h = (h << 1) + (df_u8)*name++;
    }
    return h;
}

int qbn_load(const char *name, Qbn *out)
{
    df_u8 *bytes;
    df_u32 n;

    bytes = vfs_read(name, &n);
    return bytes && qbn_parse(bytes, n, out);
}
