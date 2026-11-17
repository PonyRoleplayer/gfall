#include "internal.h"
#include "engine/core/mem.h"

/* ---- Saving ---- */

typedef struct { df_u8 *p; int n, cap, bad; } QWire;

static void qput(QWire *w, df_u32 v)
{
    if (w->n + 4 > w->cap) { w->bad = 1; return; }
    w->p[w->n++] = (df_u8)v; w->p[w->n++] = (df_u8)(v >> 8); w->p[w->n++] = (df_u8)(v >> 16); w->p[w->n++] = (df_u8)(v >> 24);
}

static df_u32 qget(QWire *w)
{
    df_u32 v;

    if (w->n + 4 > w->cap) { w->bad = 1; return 0; }
    v = (df_u32)w->p[w->n] | (df_u32)w->p[w->n + 1] << 8 | (df_u32)w->p[w->n + 2] << 16 | (df_u32)w->p[w->n + 3] << 24;
    w->n += 4;
    return v;
}

static void qbytes(QWire *w, const char *b, int len, int read)
{
    int i;

    if (w->n + len > w->cap) { w->bad = 1; return; }
    for (i = 0; i < len; i++) { if (read) { ((char *)b)[i] = (char)w->p[w->n + i]; } else { w->p[w->n + i] = (df_u8)b[i]; } }
    w->n += len;
}

/* One quest's changing parts, written or read by the same walk (a read fills what a write puts). The sparse tables (ops, places, persons) lead with a count of the rows that are not empty. */
static void qwire_quest(QWire *w, QuestState *q, int read)
{
    int i, k, count;
    df_u32 v;

#define QI(x) do { if (read) { (x) = (int)qget(w); } else { qput(w, (df_u32)(x)); } } while (0)
#define QU(x) do { if (read) { (x) = qget(w); } else { qput(w, (x)); } } while (0)
    QI(q->rewarded); QI(q->ended);
    for (i = 0; i < q->qbn.n_state; i++) { if (!q->qbn.state[i].is_global) { QI(q->qbn.state[i].value); } }
    for (i = 0; i < q->qbn.n_foe; i++) {
        QuestFoe *foe;

        foe = &q->foe[i];
        QI(q->qbn.foe[i].killed); QI(foe->number);
        /* Bit zero was the entire placement word in older saves. */
        v = (df_u32)(foe->placed | (foe->pacified << 1));
        QU(v);
        if (read) { foe->placed = v & 1u; foe->pacified = (v >> 1) & 1u; }
        QI(foe->place); QI(foe->live); QI(foe->next); QI(foe->made);
    }
    for (i = 0; i < q->qbn.n_timer; i++) { QI(q->timer[i].flags); QU(q->timer[i].start); QU(q->timer[i].delay); }
    for (i = 0; i < q->qbn.n_item; i++) { QI(q->item[i].gold); QI(q->item[i].amount); QI(q->item[i].given); QU(q->item[i].object); QI(q->item[i].place); QI(q->item[i].laid); QI(q->item[i].carrier); }
    count = 0;
    if (!read) { for (i = 0; i < q->qbn.n_op; i++) { if (q->qbn.op[i].flags || q->qbn.op[i].last_minutes || q->qbn.op[i].opcode == 25 || q->qbn.op[i].opcode == 9) { count++; } } }
    QI(count);
    if (read && (count < 0 || count > q->qbn.n_op)) { w->bad = 1; return; }
    if (read) { for (k = 0; k < count && !w->bad; k++) { i = (int)qget(w); if (i < 0 || i >= q->qbn.n_op) { w->bad = 1; return; } q->qbn.op[i].flags = (int)qget(w); q->qbn.op[i].last_minutes = qget(w); q->qbn.op[i].arg[2].value = (int)qget(w); q->qbn.op[i].arg[4].value = (int)qget(w); } }
    else { for (i = 0; i < q->qbn.n_op; i++) { if (q->qbn.op[i].flags || q->qbn.op[i].last_minutes || q->qbn.op[i].opcode == 25 || q->qbn.op[i].opcode == 9) { v = (df_u32)i; qput(w, v); qput(w, (df_u32)q->qbn.op[i].flags); qput(w, q->qbn.op[i].last_minutes); qput(w, (df_u32)q->qbn.op[i].arg[2].value); qput(w, (df_u32)q->qbn.op[i].arg[4].value); } } }
    for (i = 0; i < q->qbn.n_item; i++) { QI(q->qbn.item[i].flags); }
    for (i = 0; i < q->qbn.n_place; i++) {
        QI(q->qbn.place[i].flags);
        QI(q->place[i].bound); QI(q->place[i].marker); QI(q->place[i].block); QI(q->place[i].record);
        if (q->place[i].bound) { QI(q->place[i].ordinal); QI(q->place[i].region); QI(q->place[i].index); QI(q->place[i].type); QI(q->place[i].px); QI(q->place[i].py); qbytes(w, q->place[i].name, 33, read); }
    }
    for (i = 0; i < q->qbn.n_person; i++) {
        QI(q->qbn.person[i].flags);
        QI(q->person[i].bound);
        QI(q->person[i].at); QI(q->person[i].female); QI(q->person[i].homed); QI(q->person[i].takeover); QI(q->person[i].home_region); QI(q->person[i].home_index); QI(q->person[i].home_block); QI(q->person[i].home_record);
        if (q->person[i].bound) { QI(q->person[i].who); QI(q->person[i].faction); qbytes(w, q->person[i].name, 40, read); qbytes(w, q->person[i].town, 33, read); }
    }
#undef QI
#undef QU
}

int quest_pack(df_u8 *out, int cap)
{
    QWire w;
    int s, i, k, used;

    w.p = out; w.n = 0; w.cap = cap; w.bad = 0;
    qbytes(&w, (const char *)quest_runtime.globals, sizeof(quest_runtime.globals), 0);
    qput(&w, (df_u32)quest_foe_counter);
    used = 0;
    for (i = 0; i < LOG_SLOTS; i++) { if (quest_runtime.log[i].used) { used++; } }
    qput(&w, (df_u32)used);
    for (i = 0; i < LOG_SLOTS; i++) {
        if (!quest_runtime.log[i].used) { continue; }
        qput(&w, (df_u32)i); qput(&w, (df_u32)quest_runtime.log[i].quest_id); qbytes(&w, quest_runtime.log[i].name, 9, 0);
        for (k = 0; k < 10; k++) { qput(&w, (df_u32)quest_runtime.log[i].message[k]); qput(&w, quest_runtime.log[i].minutes[k]); qbytes(&w, quest_runtime.log[i].place[k], 32, 0); }
    }
    used = 0;
    for (s = 0; s < QUESTS; s++) { if (quest_runtime.slots[s].used) { used++; } }
    qput(&w, (df_u32)used);
    for (s = 0; s < QUESTS; s++) {
        if (!quest_runtime.slots[s].used) { continue; }
        qput(&w, (df_u32)s); qbytes(&w, quest_runtime.slots[s].qbn.name, 9, 0); qput(&w, (df_u32)quest_runtime.slots[s].qbn.id); qput(&w, (df_u32)quest_runtime.slots[s].qbn.faction);
        qwire_quest(&w, &quest_runtime.slots[s], 0);
    }
    for (i = 0; i < RUMOURS; i++) {
        qput(&w, (df_u32)quest_runtime.rumours[i].used);
        if (!quest_runtime.rumours[i].used) { continue; }
        qput(&w, (df_u32)quest_runtime.rumours[i].quest_id); qput(&w, (df_u32)quest_runtime.rumours[i].message); qput(&w, (df_u32)quest_runtime.rumours[i].flags); qput(&w, (df_u32)quest_runtime.rumours[i].target); qput(&w, quest_runtime.rumours[i].expires);
        qbytes(&w, quest_runtime.rumours[i].name, 9, 0); qbytes(&w, quest_runtime.rumours[i].text, 200, 0);
    }
    for (i = 0; i < QUEST_ACCESS; i++) { qput(&w, (df_u32)quest_runtime.access[i].used); qput(&w, (df_u32)quest_runtime.access[i].quest_id); qput(&w, (df_u32)quest_runtime.access[i].region); qput(&w, (df_u32)quest_runtime.access[i].index); qput(&w, (df_u32)quest_runtime.access[i].block); qput(&w, (df_u32)quest_runtime.access[i].record); qput(&w, (df_u32)quest_runtime.access[i].access); qput(&w, quest_runtime.access[i].until); }
    for (i = 0; i < QUEST_FACES; i++) { qput(&w, (df_u32)quest_runtime.faces[i].used); qput(&w, (df_u32)quest_runtime.faces[i].quest_id); qput(&w, (df_u32)quest_runtime.faces[i].kind); qput(&w, (df_u32)quest_runtime.faces[i].index); qput(&w, (df_u32)quest_runtime.faces[i].face); }
    return w.bad ? -1 : w.n;
}

int quest_unpack(const df_u8 *in, int len)
{
    QWire w;
    int i, k, n, s;

    quest_reset();
    if (len <= 0) { return len == 0; }
    w.p = (df_u8 *)in; w.n = 0; w.cap = len; w.bad = 0;
    qbytes(&w, (const char *)quest_runtime.globals, sizeof(quest_runtime.globals), 1);
    quest_foe_counter = (int)qget(&w);
    n = (int)qget(&w);
    if (n < 0 || n > LOG_SLOTS) { w.bad = 1; }
    for (k = 0; k < n && !w.bad; k++) {
        i = (int)qget(&w);
        if (i < 0 || i >= LOG_SLOTS) { w.bad = 1; break; }
        quest_runtime.log[i].used = 1; quest_runtime.log[i].quest_id = (int)qget(&w); qbytes(&w, quest_runtime.log[i].name, 9, 1);
        for (s = 0; s < 10; s++) { quest_runtime.log[i].message[s] = (int)qget(&w); quest_runtime.log[i].minutes[s] = qget(&w); qbytes(&w, quest_runtime.log[i].place[s], 32, 1); quest_runtime.log[i].place[s][31] = 0; }
    }
    n = w.bad ? 0 : (int)qget(&w);
    if (n < 0 || n > QUESTS) { w.bad = 1; }
    for (k = 0; k < n && !w.bad; k++) {
        QuestState *q;
        char name[9];

        s = (int)qget(&w);
        if (s < 0 || s >= QUESTS || quest_runtime.slots[s].used) { w.bad = 1; break; }
        qbytes(&w, name, 9, 1); name[8] = 0;
        q = &quest_runtime.slots[s];
        mem_zero(q, sizeof(*q));
        if (w.bad || !qbn_load(name, &q->qbn)) { w.bad = 1; break; }
        q->used = 1; q->qbn.id = (int)qget(&w); q->qbn.faction = (int)qget(&w);
        qwire_quest(&w, q, 1);
    }
    for (i = 0; i < RUMOURS && !w.bad; i++) {
        quest_runtime.rumours[i].used = (int)qget(&w);
        if (!quest_runtime.rumours[i].used) { continue; }
        quest_runtime.rumours[i].quest_id = (int)qget(&w); quest_runtime.rumours[i].message = (int)qget(&w); quest_runtime.rumours[i].flags = (int)qget(&w); quest_runtime.rumours[i].target = (int)qget(&w); quest_runtime.rumours[i].expires = qget(&w);
        qbytes(&w, quest_runtime.rumours[i].name, 9, 1); qbytes(&w, quest_runtime.rumours[i].text, 200, 1);
        quest_runtime.rumours[i].name[8] = 0; quest_runtime.rumours[i].text[199] = 0;
    }
    for (i = 0; i < QUEST_ACCESS && !w.bad; i++) { quest_runtime.access[i].used = (int)qget(&w); quest_runtime.access[i].quest_id = (int)qget(&w); quest_runtime.access[i].region = (int)qget(&w); quest_runtime.access[i].index = (int)qget(&w); quest_runtime.access[i].block = (int)qget(&w); quest_runtime.access[i].record = (int)qget(&w); quest_runtime.access[i].access = (int)qget(&w); quest_runtime.access[i].until = qget(&w); }
    for (i = 0; i < QUEST_FACES && !w.bad; i++) { quest_runtime.faces[i].used = (int)qget(&w); quest_runtime.faces[i].quest_id = (int)qget(&w); quest_runtime.faces[i].kind = (int)qget(&w); quest_runtime.faces[i].index = (int)qget(&w); quest_runtime.faces[i].face = (int)qget(&w); }
    if (w.bad || w.n != len) { quest_reset(); return 0; }
    return 1;
}
