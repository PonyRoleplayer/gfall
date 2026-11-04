#include "game/dagger/tables/quests.h"

/* See quests.h and docs/data.md. */

static const RecField g_quest_fields[] = {
    REC_FIELD("id", REC_UINT, QuestKind, id),
    REC_FIELD("name", REC_TEXT, QuestKind, name),
    REC_FIELD("group", REC_UINT, QuestKind, group),
    REC_FIELD("to", REC_UINT, QuestKind, to),
    REC_FIELD("least", REC_UINT, QuestKind, least),
    REC_FIELD("flags", REC_UINT, QuestKind, flags)
};
const RecSchema quest_schema = {
    "QUES", "quest", g_quest_fields, (int)(sizeof(g_quest_fields) / sizeof(g_quest_fields[0])), (df_u16)sizeof(QuestKind), 0
};

static RecTable g_quests;

RecTable *quest_table(void)
{
    if (!g_quests.schema) {
        rec_table_init(&g_quests, &quest_schema, 256);
    }
    return &g_quests;
}

const QuestKind *quest_kind(int id)
{
    return (const QuestKind *)rec_row(quest_table(), id);
}
