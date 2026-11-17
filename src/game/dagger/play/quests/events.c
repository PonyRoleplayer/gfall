#include "internal.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"

/* Whether the one talked to is the quest's person (type 65, a faction, is any person of that faction). */
static int is_person(const QuestState *q, int i, int who, int faction)
{
    if (i < 0 || i >= q->qbn.n_person || !q->person[i].bound) { return 0; }
    if (q->qbn.person[i].kind == -8 || (q->qbn.person[i].kind == -1 && !(q->qbn.person[i].flags & 0x100) && q->person[i].faction)) { return q->person[i].faction == faction; }
    return q->person[i].who == who;
}

int quest_talk(int who, int faction)
{
    int s, i, consumed;

    consumed = 0;
    for (s = 0; s < QUESTS; s++) {
        QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op; i++) {
            QbnOp *op;

            op = &q->qbn.op[i];
            if (op->opcode == 28 && !(op->arg[0].value == -1) && is_person(q, op->arg[1].value, who, faction)) {
                quest_arg_set(q, op, 0, 1); quest_op_done(q, op);
                /* The click is consumed unless the person's talk passes through (flag 0x2000). */
                if (!(q->qbn.person[op->arg[1].value].flags & 0x2000)) { consumed = 1; }
            } else if (op->opcode == 71 && is_person(q, op->arg[3].value, who, faction)) {
                if (gold_can_afford(op->arg[2].value)) {
                    gold_spend(op->arg[2].value);
                    quest_arg_set(q, op, 0, 1); quest_arg_set(q, op, 1, 0); quest_op_done(q, op);
                } else { quest_arg_set(q, op, 0, 0); quest_arg_set(q, op, 1, 1); }
                consumed = 1;
            } else if (op->opcode == 1 && !(op->flags & 1) && is_person(q, op->arg[2].value, who, faction)) {
                /* The item given to the person: carried, it is taken from the player and forgotten, and the state set. */
                int n;

                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_item && q->item[n].object && quest_item_carried(q->item[n].object)) {
                    inventory_unequip(q->item[n].object);
                    object_delete(q->item[n].object); quest_item_forget(q, n);
                    quest_arg_set(q, op, 0, 1); quest_op_done(q, op);
                    if (!(q->qbn.person[op->arg[2].value].flags & 0x2000)) { consumed = 1; }
                }
            } else if (op->opcode == 78 && op->arg[1].literal && op->arg[1].value == faction) {
                quest_arg_set(q, op, 0, 1); consumed = 1;
            }
        }
    }
    return consumed;
}


/* The state's events that quests watch (the original's quest_event_object): an item the pack took (3) or put down (5), a spell cast (73). */
void quest_event(int kind, df_u32 subject, df_u32 other)
{
    int s, i, n;

    if (kind != EVENT_GIVE && kind != EVENT_DROP && kind != EVENT_SPELL) { return; }
    for (s = 0; s < QUESTS; s++) {
        QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (i = 0; i < q->qbn.n_op; i++) {
            QbnOp *op;

            op = &q->qbn.op[i];
            if (kind == EVENT_GIVE && op->opcode == 3 && !op->arg[1].literal) {
                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_item && q->item[n].object == subject) { quest_arg_set(q, op, 0, 1); quest_op_done(q, op); }
            } else if (kind == EVENT_DROP && op->opcode == 5 && !op->arg[1].literal) {
                n = op->arg[1].value;
                if (n >= 0 && n < q->qbn.n_item && q->item[n].object == subject && other != game_state()->inventory_id && quest_in_place(q, op->arg[2].value)) { quest_arg_set(q, op, 0, 1); quest_op_done(q, op); }
            } else if (kind == EVENT_SPELL && op->opcode == 73 && !(op->flags & 1) && quest_arg_state(q, op, 0)) {
                const ObjectInfo *spell;

                spell = object_at(subject);
                if (spell && spell->index == op->arg[2].value) { quest_arg_set(q, op, 1, 1); op->flags |= 1; }
            }
        }
    }
}

/* Event 2 and 21 for every quest that has the foe: the ops of that opcode on it. */
static void foe_event(int number, int opcode)
{
    int s, i, f;

    for (s = 0; s < QUESTS; s++) {
        QuestState *q;

        q = &quest_runtime.slots[s];
        if (!q->used) { continue; }
        for (f = 0; f < q->qbn.n_foe && q->foe[f].number != number; f++) { }
        if (f == q->qbn.n_foe) { continue; }
        for (i = 0; i < q->qbn.n_op; i++) {
            QbnOp *op;

            op = &q->qbn.op[i];
            if (op->opcode != opcode || op->arg[1].literal || op->arg[1].value != f) { continue; }
            if (opcode == 2) {
                if (q->qbn.foe[f].killed < op->arg[2].value) { q->qbn.foe[f].killed++; }
                if (q->qbn.foe[f].killed >= op->arg[2].value) { quest_arg_set(q, op, 0, 1); quest_op_done(q, op); }
            } else { quest_arg_set(q, op, 0, 1); }
        }
        if (opcode == 2 && q->foe[f].live > 0) { q->foe[f].live--; }
        if (opcode == 2) { quest_foe_dropped(q, f); }
    }
}

void quest_foe_killed(int number) { foe_event(number, 2); }
void quest_foe_hit(int number) { foe_event(number, 21); }
