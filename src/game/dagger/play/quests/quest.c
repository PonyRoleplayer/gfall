#include "internal.h"
#include "game/dagger/play/state.h"
#include "engine/ui/cmd.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/dagger/tables/questdata.h"

QuestRuntime quest_runtime;
int quest_foe_counter = 1;
const QuestWorld *quest_world;
static int g_offer_slot = -1;

const df_u8 *quest_globals(void) { return quest_runtime.globals; }
void quest_global_set(int state) { if (state >= 0 && state < (int)sizeof(quest_runtime.globals)) { quest_runtime.globals[state] = 1; } }
void quest_globals_clear(void) { mem_zero(quest_runtime.globals, sizeof(quest_runtime.globals)); }
void quest_reset(void)
{
    mem_zero(&quest_runtime, sizeof(quest_runtime));
    quest_foe_counter = 1;
    g_offer_slot = -1;
    quest_reset_questor();
    quest_reset_initial_start();
}

void quest_set_world(const QuestWorld *world) { quest_world = world; }
int quest_count(void) { return QUESTS; }
const QuestState *quest_at(int slot) { return slot >= 0 && slot < QUESTS && quest_runtime.slots[slot].used ? &quest_runtime.slots[slot] : 0; }

void quest_end(int slot)
{
    QuestState *q;
    int i;

    if (slot < 0 || slot >= QUESTS) { return; }
    q = &quest_runtime.slots[slot];
    if (q->used) {
        log_print(LOG_INFO, "quest: %s ends (%s), minute %u", q->qbn.name,
                  q->ended ? "its end op" : "refused or stopped", (unsigned)game_state()->minutes);
    }
    for (i = 0; i < QUEST_FACES; i++) {
        QuestFace *face = &quest_runtime.faces[i];
        if (face->used && face->quest_id == q->qbn.id) { face->used = 0; }
    }
    for (i = 0; i < RUMOURS; i++) {
        Rumour *rumour = &quest_runtime.rumours[i];
        if (rumour->used && (rumour->flags & 4) && rumour->quest_id == q->qbn.id) { rumour->used = 0; }
    }
    /* Quest-owned items go with the quest, including items in the pack. */
    for (i = 0; i < q->qbn.n_item; i++) {
        if (q->item[i].object) { object_delete(q->item[i].object); }
    }
    mem_zero(q, sizeof(*q));
}

static int start_with(const char *name, const df_u8 *bytes, df_u32 size, int faction)
{
    QuestState *q;
    int slot, id, taken, k;

    for (slot = 0; slot < QUESTS && quest_runtime.slots[slot].used; slot++) { }
    if (slot == QUESTS) { return -1; }
    for (id = 1; id < 256; id++) {
        taken = 0;
        for (k = 0; k < QUESTS; k++) { if (quest_runtime.slots[k].used && quest_runtime.slots[k].qbn.id == id) { taken = 1; } }
        if (!taken) { break; }
    }
    if (id == 256) { return -1; }
    q = &quest_runtime.slots[slot];
    mem_zero(q, sizeof(*q));
    if (bytes ? !qbn_parse(bytes, size, &q->qbn) : !qbn_load(name, &q->qbn)) {
        log_print(LOG_WARN, "quest: %s does not read", name);
        return -1;
    }
    if (bytes) { for (k = 0; k < 8 && name[k]; k++) { q->qbn.name[k] = name[k]; } q->qbn.name[k] = 0; }
    q->used = 1;
    q->qbn.id = id; q->qbn.faction = faction;
    if (!quest_bind_resources(slot)) {
        /* Any failure ends the quest and says TEXT.RSC 600 (binding logged why). */
        log_print(LOG_WARN, "quest: %s does not start, minute %u", name, (unsigned)game_state()->minutes);
        mem_zero(q, sizeof(*q));
        if (quest_text_hooks.read && quest_text_hooks.say) { char text[256]; text[0] = 0; if (quest_text_hooks.read("", 600, 1, text, sizeof(text)) > 0) { quest_text_hooks.say(text); } }
        return -1;
    }
    log_print(LOG_INFO, "quest: %s starts in slot %d, minute %u", q->qbn.name, slot, (unsigned)game_state()->minutes);
    quest_rumour_add(slot, 1005, 0, 4);
    return slot;
}

int quest_start(const char *name, int faction) { return start_with(name, 0, 0, faction); }
int quest_start_bytes(const char *name, const df_u8 *bytes, df_u32 size, int faction) { return start_with(name, bytes, size, faction); }

int quest_pick(char letter, char letter2, char membership, int rank, char *name)
{
    return quest_pick_seeded(letter, letter2, membership, rank, 0xffffffffu, name);
}

/* quest_pick_file with the pick made by a seed (func_0004CD10 seeds rand with the person's id, so a person offers the same quest each time); 0xffffffff draws at random. */
int quest_pick_seeded(char letter, char letter2, char membership, int rank, unsigned seed, char *name)
{
    int i, n, pick, count;
    const char *q;

    count = 0;
    for (i = 0; i < (int)(sizeof(g_quest_name) / sizeof(g_quest_name[0])); i++) {
        q = g_quest_name[i];
        if ((q[0] == letter || (letter2 && q[0] == letter2)) && q[1] == '0' && q[2] == membership && q[3] - '0' <= rank) { count++; }
    }
    if (!count) { return 0; }
    pick = seed == 0xffffffffu ? random_range(0, count - 1) : (int)(seed % (unsigned)count); n = 0;
    for (i = 0; i < (int)(sizeof(g_quest_name) / sizeof(g_quest_name[0])); i++) {
        q = g_quest_name[i];
        if (!((q[0] == letter || (letter2 && q[0] == letter2)) && q[1] == '0' && q[2] == membership && q[3] - '0' <= rank)) { continue; }
        if (n++ == pick) { int k; for (k = 0; k < 8; k++) { name[k] = q[k]; } name[8] = 0; return 1; }
    }
    return 0;
}

int quest_active(const char *name)
{
    int i, k;

    for (i = 0; i < QUESTS; i++) {
        if (!quest_runtime.slots[i].used) { continue; }
        for (k = 0; k < 8 && name[k] && name[k] == quest_runtime.slots[i].qbn.name[k]; k++) { }
        if (k == 8 || (!name[k] && !quest_runtime.slots[i].qbn.name[k])) { return 1; }
    }
    return 0;
}

/* A Daedra prince's quest (quest_offer_prompt's strchr in prince_quest_codes: T..Z and 1..9, either case). */
static int prince_code(char c)
{
    return (c >= '1' && c <= '9') || (c >= 'T' && c <= 'Z') || (c >= 't' && c <= 'z');
}

/* The prince's scene the offer is in, its code and what it was answered. */
static char g_offer_code;

/* The player's answer to the offer: 1002 says yes, 1001 no, and a no ends the quest. A prince says it in his scene, anyone else in a box. */
static void offer_answer(int yes)
{
    int slot;
    char text[1024], said[1024];
    char code;

    slot = g_offer_slot;
    code = g_offer_code;
    g_offer_slot = -1;
    if (slot < 0 || !quest_runtime.slots[slot].used) { return; }
    text[0] = 0; said[0] = 0;
    if (quest_text_hooks.read && quest_text_hooks.read(quest_runtime.slots[slot].qbn.name, yes ? 1002 : 1001, 0, text, sizeof(text)) > 0 && text[0]) {
        quest_expand(slot, text, said, sizeof(said), game_state()->minutes, 0);
    }
    /* The words are made before the quest goes (they name its places); a no ends it at once. */
    if (!yes) { quest_end(slot); }
    if (!said[0]) { return; }
    if (code && quest_text_hooks.scene && quest_text_hooks.scene(code, said, 0, 0)) { return; }
    if (quest_text_hooks.say) { quest_text_hooks.say(said); }
}

/* The scene's answer: a yes is 1, a no 2 (Escape too). */
static void offer_scene_answer(int answer) { offer_answer(answer == QUEST_SCENE_YES); }

int quest_begin(const char *name, int faction)
{
    int slot, asked;
    char text[1024];

    /* quest_start refuses an empty name, N0C00Y01 and a quest already running. */
    if (!name || !name[0] || quest_active(name)) { return -1; }
    {
        static const char refused[] = "N0C00Y01";
        int k;

        for (k = 0; k < 8 && name[k] == refused[k]; k++) { }
        if (k == 8) { return -1; }
    }
    slot = quest_start(name, name[5] == 'Y' ? faction : 0);
    if (slot < 0) { return -1; }
    if (name[0] == 'S' || name[0] == 's') { return slot; }
    /* The Daedra's offers (T..Z, 1..9) and a Y quest ask; a prince's in his scene. Anything else is accepted silently. */
    text[0] = 0;
    asked = name[5] == 'Y' || prince_code(name[0]);
    if (asked && quest_text_hooks.read && quest_text_hooks.ask && quest_text_hooks.read(quest_runtime.slots[slot].qbn.name, 1000, 0, text, sizeof(text)) > 0 && text[0]) {
        char said[1024];

        quest_expand(slot, text, said, sizeof(said), game_state()->minutes, 0);
        g_offer_slot = slot;
        g_offer_code = 0;
        /* A prince asks in his scene (the question's words under his animation); if it cannot be shown, the box asks. */
        if (prince_code(name[0]) && quest_text_hooks.scene) {
            g_offer_code = name[0];
            if (quest_text_hooks.scene(name[0], said, 1, offer_scene_answer)) { return slot; }
            g_offer_code = 0;
        }
        if (!quest_text_hooks.ask(said, offer_answer)) { g_offer_slot = -1; quest_end(slot); return -1; }
    }
    return slot;
}

/* The console's quests: each running quest, the states it has set and its timers, to see why one waits. */
static void quests_command(int argc, char **argv)
{
    const QuestState *q;
    int i, k, set;

    (void)argc; (void)argv;
    log_print(LOG_PRINT, "minute %u, %d quests", (unsigned)game_state()->minutes, quest_count());
    for (i = 0; i < QUESTS; i++) {
        q = &quest_runtime.slots[i];
        if (!q->used) { continue; }
        set = 0;
        for (k = 0; k < q->qbn.n_state; k++) { if (q->qbn.state[k].value) { set++; } }
        log_print(LOG_PRINT, "%d %s%s: %d of %d states set", i, q->qbn.name, q->ended ? " (ended)" : "", set, q->qbn.n_state);
        for (k = 0; k < q->qbn.n_timer; k++) {
            if (q->timer[k].flags & 64) {
                log_print(LOG_PRINT, "  timer %d runs from %u for %u minutes (%d left)", k, (unsigned)q->timer[k].start, (unsigned)q->timer[k].delay,
                          (int)(q->timer[k].start + q->timer[k].delay) - (int)game_state()->minutes);
            }
        }
    }
}

void quest_commands(void)
{
    cmd_add("quests", quests_command, "", "List the running quests, their states set and their timers.");
}
