#ifndef GFALL_QUEST_INTERNAL_H
#define GFALL_QUEST_INTERNAL_H

#include "quest.h"

/* Shared by quest modules. persist.c encodes each field explicitly. */
typedef struct QuestRuntime {
    QuestState slots[QUESTS];
    df_u8 globals[64];
    LogSlot log[LOG_SLOTS];
    Rumour rumours[RUMOURS];
    QuestFace faces[QUEST_FACES];
    QuestAccess access[QUEST_ACCESS];
} QuestRuntime;

typedef struct QuestTextHooks {
    int (*read)(const char *quest, int message, int in_text_rsc, char *out, int cap);
    void (*say)(const char *text);
    int (*ask)(const char *words, void (*answer)(int yes));
} QuestTextHooks;

extern QuestRuntime quest_runtime;
extern int quest_foe_counter;
extern const QuestWorld *quest_world;
extern QuestTextHooks quest_text_hooks;

/* Binding and world resources. */
int quest_location_is_dungeon(int type);
int quest_bind_resources(int slot);
int quest_marker_point(const QuestState *q, int place, int record, int extra, float *x, float *y, float *z);
void quest_spawn_foe(QuestState *q, int f);
void quest_set_foe_pacified(QuestFoe *foe, int pacified);
void quest_make_item(QuestState *q, int i);
int quest_item_carried(df_u32 id);
void quest_item_forget(QuestState *q, int n);
int quest_in_place(const QuestState *q, int n);
void quest_put_item_down(QuestState *q, int i);
void quest_foe_dropped(QuestState *q, int f);
/* Journal and text. */
void quest_face_set(const QuestState *q, int kind, int index, int on);
void quest_rumour_add(int slot, int message, int target, int flags);
void quest_rumour_questor(int slot, int rewarded);
void quest_show_message(const QuestState *q, int message);
/* Shared interpreter operations. */
void quest_timer_start(QuestState *q, int t);
int quest_arg_state(const QuestState *q, const QbnOp *op, int n);
void quest_arg_set(QuestState *q, const QbnOp *op, int n, int value);
void quest_op_done(QuestState *q, QbnOp *op);

void quest_reset_questor(void);
void quest_reset_initial_start(void);
void quest_item_template_set(int slot, int group, int index);
void quest_item_templates_clear(void);

#endif
