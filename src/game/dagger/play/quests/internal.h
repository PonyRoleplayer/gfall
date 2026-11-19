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
    /* A prince's scene (his animation with the words): the quest's first letter, the words, whether it asks (the answer: 1 yes, 2 no, 0 left); 0 when it cannot be shown. */
    int (*scene)(char code, const char *words, int ask, void (*answer)(int answer));
} QuestTextHooks;

extern QuestRuntime quest_runtime;
extern int quest_foe_counter;
extern const QuestWorld *quest_world;
/* The quest ops (quest-machine.md 3.x), by what each does; the numbers are the files'. Ops 58, 59, 82 and 84 are the curse ops (docs/curses.md). A face op below QOP_FACE_FIRST_FOE is a person's. */
/* Op 29 (ask yes or no): its operands are the state set with no beside the state set with yes, and the question's QRC id; the yes state is 1, the no state 2 (prompt_answer). */
#define QOP_ASK_MESSAGE_ARG 3
enum {
    QOP_PLACE_ITEM = 0,
    QOP_REWARD = 4,
    QOP_END = 6,
    QOP_CLEAR_STATES = 7,
    QOP_START_QUEST = 8,
    QOP_SPAWN_REPEAT = 9,
    QOP_TOPICS_SHOW = 10,
    QOP_TOPICS_HIDE = 11,
    QOP_TIMER_START = 12,
    QOP_TIMER_STOP = 13,
    QOP_DAY_RANGE = 16,
    QOP_GRANT_ACCESS = 17,
    QOP_REVEAL_PLACE = 19,
    QOP_REVEAL_PLACE_2 = 20,
    QOP_PLACE_FOE = 22,
    QOP_LOG_SET = 23,
    QOP_LOG_REMOVE = 24,
    QOP_COUNTDOWN = 25,
    QOP_GIVE_ITEM = 26,
    QOP_REVEAL_LOCATION = 27,
    QOP_ASK_YES_NO = 29,
    QOP_MOVE_PERSON = 30,
    QOP_MINUTE_RANGE = 31,
    QOP_PLACE_FOE_2 = 33,
    QOP_SET_ONE_OF = 34,
    QOP_CLEAR_AND_NEXT = 35,
    QOP_DELETE_ITEM = 36,
    QOP_REPUTATION_AT_LEAST = 37,
    QOP_FOE_CARRIES = 39,
    QOP_IN_PLACE = 43,
    QOP_DELETE_PERSON = 44,
    QOP_HIDE_PERSON = 46,
    QOP_SHOW_PERSON = 48,
    QOP_CURE_DISEASES = 49,
    QOP_PLAY_MOVIE = 50,
    QOP_SAY = 51,
    QOP_ALL_OF = 52,
    QOP_ANY_OF = 53,
    QOP_FORGET_ITEM = 54,
    QOP_FACE_PERSON_ON = 55,
    QOP_FACE_PERSON_OFF = 56,
    QOP_ITEM_USED = 57,
    QOP_OP_58 = 58,
    QOP_OP_59 = 59,
    QOP_CHANGE_REPUTATION = 61,
    QOP_FACE_FOE_ON = 63,
    QOP_FACE_FOE_OFF = 64,
    QOP_LEGAL_REPUTATION = 65,
    QOP_PACIFY_FOE = 68,
    QOP_CARRIES_ALL = 70,
    QOP_INFECT = 72,
    QOP_TEMPLATE_ITEMS = 75,
    QOP_DELIVER_LATER = 76,
    QOP_LEVEL_AT_LEAST = 77,
    QOP_FACTION_REPUTATION = 79,
    QOP_SHOW_MESSAGE = 80,
    QOP_OPEN_BUILDING = 81,
    QOP_OP_82 = 82,
    QOP_TELEPORT_PLAYER = 83,
    QOP_OP_84 = 84,
    QOP_SOUND = 60,
    QOP_CAST_ON_FOE = 69,
    QOP_FACTION_FLAG_SET = 85,
    QOP_FACTION_FLAG_CLEAR = 86,
    QOP_RESPAWN_FOE = 87,
    QOP_FACE_FIRST_FOE = 60
};

extern QuestTextHooks quest_text_hooks;

/* Binding and world resources. */
int quest_location_is_dungeon(int type);
int quest_bind_resources(int slot);
int quest_marker_point(const QuestState *q, int place, int record, int extra, float *x, float *y, float *z);
void quest_spawn_foe(QuestState *q, int f);
void quest_set_foe_pacified(QuestFoe *foe, int pacified);
void quest_cast_on_foe(const QuestFoe *foe, int spell);
void quest_remove_foe(const QuestFoe *foe);
/* The caster magic_apply takes for the player (the spells the player casts). */
#define QUEST_CASTER_PLAYER (-1)
/* The flag ops 85 and 86 set and clear on a person's faction (a faction's flags are the FACTION.TXT's; nothing in FALL.EXE reads this bit, so it is not kept). */
#define FACTION_FLAG_QUEST 0x200
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
