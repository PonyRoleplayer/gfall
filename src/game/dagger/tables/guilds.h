#ifndef DF_GUILDS_H
#define DF_GUILDS_H

#include "engine/assets/recfile.h"

/*
 * The guilds one can join, as records (docs/data.md), from Daggerfall Unity's
 * FightersGuild, MagesGuild, Temple and KnightlyOrder. Read docs/game.md
 * (Guilds) before editing.
 *
 *   GILD: a guild by its faction in FACTION.TXT: the Fighters (41), the Mages
 *         (40), a temple by its god (21 Arkay .. 35 Kynareth; a temple's own
 *         order is the god's child), a knightly order by its own (368..417).
 *         Its group (FACTION.TXT's ggroup), its ten ranks' titles, the skills
 *         its ranks ask for and those it trains, what it gives its members,
 *         the rank its healing comes at, and TEXT.RSC's words for joining and
 *         rising.
 *
 * The Thieves Guild and the Dark Brotherhood invite rather than take, by quest
 * (P11): they are not here yet.
 */

#define GUILD_RANKS 10
#define GUILD_SKILLS 12
#define GUILD_TITLE 16
/* The end of a skill list shorter than GUILD_SKILLS. */
#define GUILD_NO_SKILL 0xFF

/* FACTION.TXT's guild groups that take members. */
enum {
    GUILD_GROUP_KNIGHTLY = 9,
    GUILD_GROUP_MAGES = 10,
    GUILD_GROUP_FIGHTERS = 11,
    GUILD_GROUP_TEMPLE = 17
};

/* What a guild gives its members. */
#define GUILD_GIVES_REST 0x01
#define GUILD_GIVES_FREE_ROOMS 0x02
#define GUILD_GIVES_CHEAP_CURE 0x04
#define GUILD_GIVES_CHEAP_REPAIR 0x08
/* A temple trains anyone, member or not (DFU's Temple training). */
#define GUILD_TRAINS_ALL 0x10

typedef struct GuildKind {
    df_u16 id;
    df_u8 group;
    df_u8 gives;
    /* The rank from which a member is healed for nothing on coming in; GUILD_NO_SKILL for never. */
    df_u8 healing_rank;
    char rank[GUILD_RANKS][GUILD_TITLE];
    df_u8 skill[GUILD_SKILLS];
    df_u8 training[GUILD_SKILLS];
    /* TEXT.RSC: eligible to join, not for a bad name, not for skills, welcome, promotion. */
    df_u16 text_eligible;
    df_u16 text_bad_name;
    df_u16 text_low_skill;
    df_u16 text_welcome;
    df_u16 text_promotion;
} GuildKind;

extern const RecSchema guild_schema;
RecTable *guild_table(void);
/* The guild of that faction; 0 for none. */
const GuildKind *guild_kind(int faction);

#endif
