#include "internal.h"
#include "game/dagger/play/state.h"
#include "game/dagger/tables/datedefs.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

QuestTextHooks quest_text_hooks;
static const QuestNames *g_names;
static int (*g_macros)(const char *in, char *out, int cap);
static const char *(*g_place)(void);

void quest_set_text(int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap)) { quest_text_hooks.read = read; }
void quest_set_say(void (*say)(const char *text)) { quest_text_hooks.say = say; }
void quest_set_ask(int (*ask)(const char *words, void (*answer)(int yes))) { quest_text_hooks.ask = ask; }
void quest_set_names(const QuestNames *names) { g_names = names; }
void quest_set_macros(int (*fill)(const char *in, char *out, int cap)) { g_macros = fill; }
void quest_set_place(const char *(*current)(void)) { g_place = current; }
/* ---- Text (parse_expand, quest_symbol_text) ---- */

static int put(char *out, int cap, int at, const char *s)
{
    while (*s && at < cap - 1) { out[at++] = *s++; }
    return at;
}

/* A symbol said: items, places, persons, foes, timers, the first whose hash it is; blank for none. */
static int symbol_text(int slot, df_i32 hash, int form, df_i32 hash2, char *out, int cap)
{
    const QuestState *q;
    int i;
    df_u32 delay;

    out[0] = 0;
    q = &quest_runtime.slots[slot];
    for (i = 0; i < q->qbn.n_item; i++) { if (q->qbn.item[i].symbol == hash) { return g_names && g_names->item && g_names->item(slot, i, form, out, cap); } }
    for (i = 0; i < q->qbn.n_place; i++) {
        if (q->qbn.place[i].symbol != hash) { continue; }
        /* A place bound to a location says its name (forms 0, 16, 32) or its region's (48). */
        if (q->place[i].bound) {
            if (form >= 48 && quest_world && quest_world->region_name) { return quest_world->region_name(q->place[i].region, out, cap) > 0; }
            return log_format(out, cap, "%s", q->place[i].name) > 0;
        }
        return g_names && g_names->place && g_names->place(slot, i, form, out, cap);
    }
    for (i = 0; i < q->qbn.n_person; i++) {
        if (q->qbn.person[i].symbol != hash) { continue; }
        /* A person bound to someone says their name (0), their town (32) or the region (48); the building (16) is the town's here. */
        if (q->person[i].bound && q->qbn.person[i].kind == 21) {
            if (form >= 48 && quest_world && quest_world->region_name) { int region, here, px, py; quest_world->here(&region, &here, &px, &py); return quest_world->region_name(region, out, cap) > 0; }
            return log_format(out, cap, "%s", form >= 16 ? q->person[i].town : q->person[i].name) > 0;
        }
        return g_names && g_names->person && g_names->person(slot, i, form, out, cap);
    }
    for (i = 0; i < q->qbn.n_foe; i++) {
        if (q->qbn.foe[i].symbol == hash) {
            const FoeDef *def;

            def = foe_def(q->qbn.foe[i].type);
            return log_format(out, cap, "%s", def ? def->name : "");
        }
    }
    for (i = 0; i < q->qbn.n_timer; i++) {
        if (q->qbn.timer[i].hash == hash) {
            delay = q->timer[i].delay;
            /* _a`b_: the second timer's delay is added to the first's. */
            for (i = 0; hash2 && i < q->qbn.n_timer; i++) { if (q->qbn.timer[i].hash == hash2) { delay += q->timer[i].delay; } }
            return form == 0 ? log_format(out, cap, "%d", (int)delay) : log_format(out, cap, "%d", (int)((delay + 1439u) / 1440u));
        }
    }
    return 0;
}

int quest_expand(int slot, const char *in, char *out, int cap, df_u32 minutes, const char *place)
{
    static char stage[2048];
    char name[24], name2[24], said[256];
    int at, n, k, form, second;
    char lead;

    if (cap < 1) { return 0; }
    at = 0;
    while (*in && at < (int)sizeof(stage) - 1) {
        if ((*in == '_' || *in == '=') && slot >= 0 && slot < QUESTS && quest_runtime.slots[slot].used) {
            lead = *in; n = 0;
            while (in[n] == lead) { n++; }
            k = 0; second = 0;
            while (in[n + k] && in[n + k] != '_' && in[n + k] != ' ' && in[n + k] != '\n' && k < 20) {
                if (in[n + k] == '`') { second = k + 1; }
                k++;
            }
            if (in[n + k] == '_' && k > 0) {
                int j;

                for (j = 0; j < k && (!second || j < second - 1); j++) { name[j] = in[n + j]; }
                name[j] = 0; name2[0] = 0;
                if (second) { int m; for (m = 0; second + m < k; m++) { name2[m] = in[n + second + m]; } name2[m] = 0; }
                form = lead == '_' ? (n - 1) * 16 : n;
                said[0] = 0;
                symbol_text(slot, qbn_hash(name), form, name2[0] ? qbn_hash(name2) : 0, said, sizeof(said));
                at = put(stage, sizeof(stage), at, said);
                in += n + k + 1;
                continue;
            }
        }
        stage[at++] = *in++;
    }
    stage[at] = 0;
    /* %qdt: the date a log entry was written; %cn: its place, when it has one. */
    {
        static char second[2048];
        int s, d;
        char date[64];

        s = 0;
        for (d = 0; stage[d] && s < (int)sizeof(second) - 1; ) {
            if (stage[d] == '%' && stage[d + 1] == 'q' && stage[d + 2] == 'd' && stage[d + 3] == 't') {
                date_text(minutes, date, sizeof(date)); s = put(second, sizeof(second), s, date); d += 4;
            } else if (place && stage[d] == '%' && stage[d + 1] == 'c' && stage[d + 2] == 'n') {
                s = put(second, sizeof(second), s, place); d += 3;
            } else { second[s++] = stage[d++]; }
        }
        second[s] = 0;
        if (g_macros) { return g_macros(second, out, cap); }
        out[0] = 0; at = put(out, cap, 0, second); out[at] = 0;
        return at;
    }
}

/* ---- The logbook (logbook.c) ---- */

const LogSlot *logbook_slot(int i) { return i >= 0 && i < LOG_SLOTS ? &quest_runtime.log[i] : 0; }
void logbook_clear(void) { mem_zero(quest_runtime.log, sizeof(quest_runtime.log)); }

/* logbook_prune_quests: the slot of a quest that is not running is free. */
static void prune(void)
{
    int i, k, alive;

    for (i = 0; i < LOG_SLOTS; i++) {
        if (!quest_runtime.log[i].used) { continue; }
        alive = 0;
        for (k = 0; k < QUESTS; k++) { if (quest_runtime.slots[k].used && quest_runtime.slots[k].qbn.id == quest_runtime.log[i].quest_id) { alive = 1; } }
        if (!alive) { mem_zero(&quest_runtime.log[i], sizeof(quest_runtime.log[i])); }
    }
}

void logbook_add(int quest_slot, int index, int message)
{
    int i, found, k;
    const char *where;

    if (!quest_at(quest_slot)) { return; }
    prune();
    found = -1;
    for (i = 0; i < LOG_SLOTS; i++) { if (quest_runtime.log[i].used && quest_runtime.log[i].quest_id == quest_runtime.slots[quest_slot].qbn.id) { found = i; } }
    if (found < 0) {
        for (i = 0; i < LOG_SLOTS && quest_runtime.log[i].used; i++) { }
        /* A full log writes nothing and the quest goes on (7.7: the original wrote slot -1). */
        if (i == LOG_SLOTS) { return; }
        found = i;
        mem_zero(&quest_runtime.log[found], sizeof(quest_runtime.log[found]));
        quest_runtime.log[found].used = 1; quest_runtime.log[found].quest_id = quest_runtime.slots[quest_slot].qbn.id;
        for (k = 0; k < 8; k++) { quest_runtime.log[found].name[k] = quest_runtime.slots[quest_slot].qbn.name[k]; }
    }
    index = ((index % 10) + 10) % 10;
    quest_runtime.log[found].message[index] = message;
    quest_runtime.log[found].minutes[index] = game_state()->minutes;
    where = g_place ? g_place() : 0;
    for (k = 0; where && where[k] && k < 31; k++) { quest_runtime.log[found].place[index][k] = where[k]; }
    quest_runtime.log[found].place[index][k] = 0;
}

void logbook_remove(int quest_slot, int index)
{
    int i;

    if (!quest_at(quest_slot)) { return; }
    index = ((index % 10) + 10) % 10;
    for (i = 0; i < LOG_SLOTS; i++) {
        if (quest_runtime.log[i].used && quest_runtime.log[i].quest_id == quest_runtime.slots[quest_slot].qbn.id) { quest_runtime.log[i].message[index] = 0; quest_runtime.log[i].minutes[index] = 0; quest_runtime.log[i].place[index][0] = 0; }
    }
}

int logbook_text(char *out, int cap)
{
    int i, k, n, quest;
    char raw[1024], said[1024];

    prune();
    n = 0; out[0] = 0;
    for (i = 0; i < LOG_SLOTS; i++) {
        if (!quest_runtime.log[i].used) { continue; }
        for (quest = 0; quest < QUESTS && !(quest_runtime.slots[quest].used && quest_runtime.slots[quest].qbn.id == quest_runtime.log[i].quest_id); quest++) { }
        if (quest == QUESTS) { continue; }
        for (k = 0; k < 10; k++) {
            if (!quest_runtime.log[i].message[k] || !quest_text_hooks.read) { continue; }
            raw[0] = 0;
            if (quest_text_hooks.read(quest_runtime.log[i].name, quest_runtime.log[i].message[k], 0, raw, sizeof(raw)) <= 0 || !raw[0]) { continue; }
            quest_expand(quest, raw, said, sizeof(said), quest_runtime.log[i].minutes[k], quest_runtime.log[i].place[k]);
            n += log_format(out + n, cap - n, "%s\n", said);
        }
    }
    return n;
}


/* rumor_add_quest: the message's text, filled in, kept with the quest's id; 30 days, or 180 for the questor's. A full list loses its first. */
void quest_rumour_add(int slot, int message, int target, int flags)
{
    Rumour *r;
    char raw[1024], said[1024];
    int i;

    if (!quest_text_hooks.read) { return; }
    raw[0] = 0;
    if (quest_text_hooks.read(quest_runtime.slots[slot].qbn.name, message, 0, raw, sizeof(raw)) <= 0 || !raw[0]) { return; }
    quest_expand(slot, raw, said, sizeof(said), game_state()->minutes, 0);
    for (i = 0; i < RUMOURS && quest_runtime.rumours[i].used; i++) { }
    if (i == RUMOURS) { for (i = 1; i < RUMOURS; i++) { quest_runtime.rumours[i - 1] = quest_runtime.rumours[i]; } i = RUMOURS - 1; }
    r = &quest_runtime.rumours[i];
    mem_zero(r, sizeof(*r));
    r->used = 1; r->quest_id = quest_runtime.slots[slot].qbn.id; r->message = message; r->flags = flags; r->target = target;
    r->expires = game_state()->minutes + (flags & 2 ? 180u : 30u) * 1440u;
    log_format(r->name, sizeof(r->name), "%s", quest_runtime.slots[slot].qbn.name);
    log_format(r->text, sizeof(r->text), "%s", said);
}

/* The questor's own rumour at the quest's end: the person of kind 21. */
void quest_rumour_questor(int slot, int rewarded)
{
    int i;

    for (i = 0; i < quest_runtime.slots[slot].qbn.n_person; i++) {
        if (quest_runtime.slots[slot].qbn.person[i].kind == 21 && quest_runtime.slots[slot].person[i].bound) { quest_rumour_add(slot, rewarded ? 1008 : 1009, quest_runtime.slots[slot].person[i].who, 2); return; }
    }
}

const Rumour *rumour_at(int i) { return i >= 0 && i < RUMOURS && quest_runtime.rumours[i].used ? &quest_runtime.rumours[i] : 0; }

int quest_news(int faction, char *out, int cap)
{
    int i, n, pick;

    (void)faction;
    n = 0;
    for (i = 0; i < RUMOURS; i++) { if (quest_runtime.rumours[i].used && (quest_runtime.rumours[i].flags & 12) && quest_runtime.rumours[i].expires > game_state()->minutes) { n++; } }
    if (!n) { return 0; }
    pick = random_range(0, n - 1);
    for (i = 0; i < RUMOURS; i++) {
        if (quest_runtime.rumours[i].used && (quest_runtime.rumours[i].flags & 12) && quest_runtime.rumours[i].expires > game_state()->minutes && pick-- == 0) { log_format(out, cap, "%s", quest_runtime.rumours[i].text); return 1; }
    }
    return 0;
}

int quest_rumour_for(int who, char *out, int cap)
{
    int i;

    for (i = 0; i < RUMOURS; i++) {
        if (quest_runtime.rumours[i].used && (quest_runtime.rumours[i].flags & 2) && quest_runtime.rumours[i].target == who && quest_runtime.rumours[i].expires > game_state()->minutes) { log_format(out, cap, "%s", quest_runtime.rumours[i].text); return 1; }
    }
    return 0;
}

/* ---- Talk topics ---- */

/* The nth topic: its quest, the section (0 item, 3 person, 4 place) and the record. A resource is a topic when it is not hidden (an item and a place 128, a person 0x8000) and has a message. */
static int topic_find(int n, int *quest, int *section, int *index)
{
    int s, i;

    for (s = 0; s < QUESTS; s++) {
        const QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_item; i++) {
            if (!(q->qbn.item[i].flags & 128) && (q->qbn.item[i].message[0] > 0 || q->qbn.item[i].message[1] > 0) && n-- == 0) { *quest = s; *section = 0; *index = i; return 1; }
        }
        for (i = 0; i < q->qbn.n_person; i++) {
            if (!(q->qbn.person[i].flags & 0x8000) && (q->qbn.person[i].message[0] > 0 || q->qbn.person[i].message[1] > 0) && n-- == 0) { *quest = s; *section = 3; *index = i; return 1; }
        }
        for (i = 0; i < q->qbn.n_place; i++) {
            if (!(q->qbn.place[i].flags & 128) && (q->qbn.place[i].message[0] > 0 || q->qbn.place[i].message[1] > 0) && n-- == 0) { *quest = s; *section = 4; *index = i; return 1; }
        }
    }
    return 0;
}

int quest_topic_count(void)
{
    int n, s, i;

    n = 0;
    for (s = 0; s < QUESTS; s++) {
        const QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_item; i++) { if (!(q->qbn.item[i].flags & 128) && (q->qbn.item[i].message[0] > 0 || q->qbn.item[i].message[1] > 0)) { n++; } }
        for (i = 0; i < q->qbn.n_person; i++) { if (!(q->qbn.person[i].flags & 0x8000) && (q->qbn.person[i].message[0] > 0 || q->qbn.person[i].message[1] > 0)) { n++; } }
        for (i = 0; i < q->qbn.n_place; i++) { if (!(q->qbn.place[i].flags & 128) && (q->qbn.place[i].message[0] > 0 || q->qbn.place[i].message[1] > 0)) { n++; } }
    }
    return n;
}

int quest_topic_name(int i, char *out, int cap)
{
    int quest, section, index, ok;

    out[0] = 0;
    if (!topic_find(i, &quest, &section, &index) || !g_names) { return 0; }
    ok = 0;
    if (section == 0 && g_names->item) { ok = g_names->item(quest, index, 0, out, cap); }
    else if (section == 3 && g_names->person) { ok = g_names->person(quest, index, 0, out, cap); }
    else if (section == 4 && g_names->place) { ok = g_names->place(quest, index, 0, out, cap); }
    return ok;
}

int quest_topic_answer(int i, char *out, int cap)
{
    int quest, section, index, message;
    char raw[1024];

    out[0] = 0;
    if (!quest_text_hooks.read || !topic_find(i, &quest, &section, &index)) { return 0; }
    if (section == 0) { message = quest_runtime.slots[quest].qbn.item[index].message[1] > 0 ? quest_runtime.slots[quest].qbn.item[index].message[1] : quest_runtime.slots[quest].qbn.item[index].message[0]; }
    else if (section == 3) { message = quest_runtime.slots[quest].qbn.person[index].message[1] > 0 ? quest_runtime.slots[quest].qbn.person[index].message[1] : quest_runtime.slots[quest].qbn.person[index].message[0]; }
    else { message = quest_runtime.slots[quest].qbn.place[index].message[1] > 0 ? quest_runtime.slots[quest].qbn.place[index].message[1] : quest_runtime.slots[quest].qbn.place[index].message[0]; }
    raw[0] = 0;
    if (quest_text_hooks.read(quest_runtime.slots[quest].qbn.name, message, 0, raw, sizeof(raw)) <= 0 || !raw[0]) { return 0; }
    quest_expand(quest, raw, out, cap, game_state()->minutes, 0);
    return 1;
}

int quest_item_text(df_u32 item, char *out, int cap)
{
    int s, i;
    char raw[1024];

    if (!item || !quest_text_hooks.read) { return 0; }
    for (s = 0; s < QUESTS; s++) {
        if (!quest_runtime.slots[s].used) { continue; }
        for (i = 0; i < quest_runtime.slots[s].qbn.n_item; i++) {
            if (quest_runtime.slots[s].item[i].object != item || quest_runtime.slots[s].qbn.item[i].message[1] <= 0) { continue; }
            raw[0] = 0;
            if (quest_text_hooks.read(quest_runtime.slots[s].qbn.name, quest_runtime.slots[s].qbn.item[i].message[1], 0, raw, sizeof(raw)) <= 0 || !raw[0]) { return 0; }
            quest_expand(s, raw, out, cap, game_state()->minutes, 0);
            return 1;
        }
    }
    return 0;
}

const QuestFace *quest_face(int i) { return i >= 0 && i < QUEST_FACES && quest_runtime.faces[i].used ? &quest_runtime.faces[i] : 0; }

/* quest_face_add and its removal: one slot a person or foe of a quest; a full list takes none. */
void quest_face_set(const QuestState *q, int kind, int index, int on)
{
    int i, free_slot;

    free_slot = -1;
    for (i = 0; i < QUEST_FACES; i++) {
        if (quest_runtime.faces[i].used && quest_runtime.faces[i].quest_id == q->qbn.id && quest_runtime.faces[i].kind == kind && quest_runtime.faces[i].index == index) {
            if (!on) { quest_runtime.faces[i].used = 0; }
            return;
        }
        if (!quest_runtime.faces[i].used && free_slot < 0) { free_slot = i; }
    }
    if (on && free_slot >= 0) {
        int woman, bank, region, here, px, py, child;

        /* quest_face_add: the sex (a person's by their flag, a foe's male here), the region's name bank (D_001841E3), a child's face for faction 514, else a picture of the bank 0..9 at random. */
        woman = kind == 1 && q->person[index].female;
        child = kind == 1 && q->person[index].faction == 514;
        region = 0;
        if (quest_world && quest_world->here) { quest_world->here(&region, &here, &px, &py); }
        /* The regions of the second bank (D_001841E3): 0, 1, 7, 8, 10, 11, 14, 20, 22, 25, 28, 29, 43..56 and 61. */
        bank = region == 0 || region == 1 || region == 7 || region == 8 || region == 10 || region == 11 || region == 14 || region == 20 || region == 22 || region == 25 || region == 28 || region == 29 ||
               (region >= 43 && region <= 56) || region == 61;
        quest_runtime.faces[free_slot].used = 1; quest_runtime.faces[free_slot].quest_id = q->qbn.id; quest_runtime.faces[free_slot].kind = kind; quest_runtime.faces[free_slot].index = index;
        quest_runtime.faces[free_slot].face = (woman ? 0x80 : 0) | (bank ? 0x40 : 0) | (child ? 0x10 : 0) | (child ? (woman ? 1 : 0) + bank * 2 : random_range(0, 9));
    }
}

/* ---- The messages ---- */

/* The records come broken into lines of about 45 letters, each continuation starting with a space. The parchment fits its own lines, so a break before a space is joined; the rest stay. */
static void join_lines(char *text)
{
    int from, to;

    for (from = to = 0; text[from]; from++) {
        if (text[from] == '\n' && text[from + 1] == ' ') { continue; }
        text[to++] = text[from];
    }
    text[to] = 0;
}

void quest_show_message(const QuestState *q, int message)
{
    char text[1024], said[1024];

    if (message == -1 || !quest_text_hooks.read || !quest_text_hooks.say) { return; }
    text[0] = 0;
    if (quest_text_hooks.read(q->qbn.name, message & 0x7fff, (message & 0x8000) != 0, text, sizeof(text)) > 0 && text[0]) {
        log_print(LOG_INFO, "quest: %s says %s %d", q->qbn.name, (message & 0x8000) ? "TEXT.RSC" : "QRC", message & 0x7fff);
        quest_expand((int)(q - quest_runtime.slots), text, said, sizeof(said), game_state()->minutes, 0);
        join_lines(said);
        quest_text_hooks.say(said);
    }
}
