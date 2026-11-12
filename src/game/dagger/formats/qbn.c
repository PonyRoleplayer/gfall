#include "game/dagger/formats/qbn.h"
#include "engine/assets/vfs.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

/* See qbn.h. */

static const df_u8 *g_count;
static df_u32 g_count_size;

int qbn_arg_count(int opcode)
{
    if (!g_count) { g_count = vfs_read("COUNT.DAT", &g_count_size); }
    if (!g_count || opcode < 0 || opcode >= 89 || (df_u32)opcode >= g_count_size) { return 0; }
    return g_count[opcode] >= '0' && g_count[opcode] <= '9' ? g_count[opcode] - '0' : 0;
}

df_i32 qbn_hash(const char *name)
{
    df_u32 hash;

    hash = 0;
    for (; *name; name++) { hash = hash * 2u + (df_u8)*name; }
    return (df_i32)hash;
}

static void arg_read(const df_u8 *p, QbnArg *a)
{
    a->negate = p[0] & 1;
    a->literal = df_ru32(p + 1) == 0x12345678u;
    a->section = df_ri16(p + 5);
    a->value = df_ri32(p + 7);
}

int qbn_parse(const df_u8 *b, df_u32 size, Qbn *q)
{
    static const int record[10] = {19, 94, 34, 20, 24, 16, 33, 14, 87, 8};
    int count[10], offset[10], s, i, k;

    if (!b || size < 0x3c) { return 0; }
    mem_zero(q, sizeof(*q));
    q->id = df_ri16(b); q->faction = df_ri16(b + 2); q->text_file = df_ri16(b + 4); q->flags = b[0x0f];
    for (i = 0; i < 8 && b[6 + i]; i++) { q->name[i] = (char)b[6 + i]; }
    for (s = 0; s < 10; s++) {
        count[s] = df_ri16(b + 0x10 + s * 2); offset[s] = df_ri16(b + 0x24 + s * 2);
        if (count[s] < 0 || (count[s] && (offset[s] < 0 || (df_u32)offset[s] + (df_u32)count[s] * record[s] > size))) { return 0; }
    }
    if (count[0] > QBN_ITEMS || count[3] > QBN_PERSONS || count[4] > QBN_PLACES || count[6] > QBN_TIMERS || count[7] > QBN_FOES ||
        count[8] > QBN_OPS || count[9] > QBN_STATES || count[1] || count[2] || count[5]) { return 0; }
    q->n_item = count[0]; q->n_person = count[3]; q->n_place = count[4]; q->n_timer = count[6]; q->n_foe = count[7]; q->n_op = count[8]; q->n_state = count[9];
    for (i = 0; i < q->n_item; i++) {
        const df_u8 *p;

        p = b + offset[0] + i * 19;
        q->item[i].flags = p[2]; q->item[i].group = df_ri16(p + 3); q->item[i].index = df_ri16(p + 5); q->item[i].symbol = df_ri32(p + 7);
        q->item[i].message[0] = df_ri16(p + 15); q->item[i].message[1] = df_ri16(p + 17);
    }
    for (i = 0; i < q->n_person; i++) {
        const df_u8 *p;

        p = b + offset[3] + i * 20;
        q->person[i].flags = df_ru16(p + 2); q->person[i].kind = df_ri16(p + 4); q->person[i].faction = df_ri16(p + 6); q->person[i].symbol = df_ri32(p + 8);
        q->person[i].message[0] = df_ri16(p + 16); q->person[i].message[1] = df_ri16(p + 18);
    }
    for (i = 0; i < q->n_place; i++) {
        const df_u8 *p;

        p = b + offset[4] + i * 24;
        q->place[i].flags = p[2]; q->place[i].scope = (signed char)p[3]; q->place[i].p1 = df_ru16(p + 4); q->place[i].p2 = df_ri16(p + 6); q->place[i].p3 = df_ri16(p + 8);
        q->place[i].symbol = df_ri32(p + 12); q->place[i].message[0] = df_ri16(p + 20); q->place[i].message[1] = df_ri16(p + 22);
    }
    for (i = 0; i < q->n_timer; i++) {
        const df_u8 *p;

        p = b + offset[6] + i * 33;
        q->timer[i].flags = df_ru16(p + 2); q->timer[i].type = p[4]; q->timer[i].minimum = df_ri32(p + 5); q->timer[i].maximum = df_ri32(p + 9);
        q->timer[i].start = df_ri32(p + 13); q->timer[i].delay = df_ru32(p + 17); q->timer[i].link1 = (int)df_ru32(p + 21); q->timer[i].link2 = (int)df_ru32(p + 25);
        q->timer[i].hash = df_ri32(p + 29);
    }
    for (i = 0; i < q->n_foe; i++) {
        const df_u8 *p;

        p = b + offset[7] + i * 14;
        q->foe[i].type = p[3]; q->foe[i].count = p[4]; q->foe[i].killed = p[5]; q->foe[i].symbol = df_ri32(p + 6);
    }
    for (i = 0; i < q->n_op; i++) {
        const df_u8 *p;

        p = b + offset[8] + i * 87;
        q->op[i].opcode = df_ri16(p); q->op[i].flags = df_ru16(p + 2); q->op[i].argc = qbn_arg_count(q->op[i].opcode);
        for (k = 0; k < 5; k++) { arg_read(p + 6 + k * 15, &q->op[i].arg[k]); }
        q->op[i].message = df_ri16(p + 81); q->op[i].last_minutes = df_ru32(p + 83);
    }
    for (i = 0; i < q->n_state; i++) {
        const df_u8 *p;

        p = b + offset[9] + i * 8;
        q->state[i].index = df_ri16(p); q->state[i].is_global = p[2]; q->state[i].value = p[3]; q->state[i].hash = df_ri32(p + 4);
    }
    /* The text variables (section 10): 27 bytes each until a name that starts with 0. */
    if (size >= 0x3c) {
        df_u32 at;

        at = df_ru32(b + 0x38);
        for (i = 0; at && at + 27u <= size && i < QBN_TEXTVARS && b[at]; i++, at += 27) {
            for (k = 0; k < 19; k++) { q->textvar[i].name[k] = (char)b[at + k]; }
            q->textvar[i].section = b[at + 20]; q->textvar[i].index = df_ri16(b + at + 21);
            q->n_textvar = i + 1;
        }
    }
    return 1;
}

int qbn_load(const char *name, Qbn *q)
{
    char file[24];
    df_u32 size;
    const df_u8 *bytes;
    int i;

    log_format(file, sizeof(file), "%s.QBN", name);
    bytes = vfs_read(file, &size);
    if (!bytes || !qbn_parse(bytes, size, q)) { return 0; }
    for (i = 0; i < 8 && name[i]; i++) { q->name[i] = name[i]; }
    q->name[i] = 0;
    return 1;
}
