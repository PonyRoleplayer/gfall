#include "game/dagger/tables/guilds.h"

/* See guilds.h and docs/data.md. */

#define F(name, type, member) REC_FIELD(name, type, GuildKind, member)
#define FA(name, type, member) REC_ARRAY(name, type, GuildKind, member)
static const RecField g_guild_fields[] = {
    F("id", REC_UINT, id),
    F("group", REC_UINT, group),
    F("gives", REC_UINT, gives),
    F("healing_rank", REC_UINT, healing_rank),
    /* The ten titles as ten text fields: a record file's text is one string a field. */
    { "rank0", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[0]) },
    { "rank1", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[1]) },
    { "rank2", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[2]) },
    { "rank3", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[3]) },
    { "rank4", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[4]) },
    { "rank5", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[5]) },
    { "rank6", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[6]) },
    { "rank7", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[7]) },
    { "rank8", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[8]) },
    { "rank9", REC_TEXT, GUILD_TITLE, 1, (df_u16)DF_OFFSETOF(GuildKind, rank[9]) },
    FA("skill", REC_UINT, skill),
    FA("training", REC_UINT, training),
    F("text_eligible", REC_UINT, text_eligible),
    F("text_bad_name", REC_UINT, text_bad_name),
    F("text_low_skill", REC_UINT, text_low_skill),
    F("text_welcome", REC_UINT, text_welcome),
    F("text_promotion", REC_UINT, text_promotion)
};
#undef F
#undef FA
const RecSchema guild_schema = {
    "GILD", "guild", g_guild_fields, (int)(sizeof(g_guild_fields) / sizeof(g_guild_fields[0])), (df_u16)sizeof(GuildKind), 0
};

static RecTable g_guilds;

RecTable *guild_table(void)
{
    if (!g_guilds.schema) {
        rec_table_init(&g_guilds, &guild_schema, 32);
    }
    return &g_guilds;
}

const GuildKind *guild_kind(int faction)
{
    return faction > 0 ? (const GuildKind *)rec_row(guild_table(), faction) : 0;
}
