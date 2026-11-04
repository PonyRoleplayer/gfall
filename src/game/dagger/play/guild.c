#include "game/dagger/play/guild.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/tables/careers.h"
#include "engine/core/mem.h"

/* See guild.h and docs/game.md (Guilds). */

/* DFU's Guild.rankReqReputation, rankReqSkillHigh and rankReqSkillLow. */
static const int g_rank_rep[GUILD_RANKS] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90 };
static const int g_rank_high[GUILD_RANKS] = { 22, 23, 31, 39, 47, 55, 63, 71, 79, 87 };
static const int g_rank_low[GUILD_RANKS] = { 4, 5, 9, 13, 17, 21, 25, 29, 33, 37 };

/* Training: a member's price a level and another's, at most this far, once in this many minutes. */
#define TRAIN_MEMBER 100
#define TRAIN_OTHER 400
#define TRAIN_MOST 50
#define TRAIN_EVERY 720
/* The fatigue three hours of training cost: DFU's DefaultFatigueLoss x 180, in 64ths. */
#define TRAIN_FATIGUE (11 * 180)
/* A knightly order's free rooms everywhere from this rank (KnightlyOrder.FreeTavernRooms). */
#define FREE_ROOMS_RANK 4
#define NO_TRAINING (-1000000)

static const RecField g_member_fields[] = {
    REC_FIELD("id", REC_UINT, GuildMembership, id),
    REC_FIELD("guild", REC_INT, GuildMembership, guild),
    REC_FIELD("rank", REC_INT, GuildMembership, rank),
    REC_FIELD("changed_day", REC_INT, GuildMembership, changed_day)
};
const RecSchema guild_membership_schema = {
    "MEMB", "membership", g_member_fields, (int)(sizeof(g_member_fields) / sizeof(g_member_fields[0])),
    (df_u16)sizeof(GuildMembership), 0
};

static GuildMembership g_members[GUILD_MEMBERSHIPS];
static int g_count;
static int g_last_training = NO_TRAINING;

void guild_new_game(void)
{
    mem_zero(g_members, sizeof(g_members));
    g_count = 0;
    g_last_training = NO_TRAINING;
}

int guild_count(void)
{
    return g_count;
}

const GuildMembership *guild_at(int i)
{
    return i >= 0 && i < g_count ? &g_members[i] : 0;
}

void guild_put(const GuildMembership *m)
{
    if (g_count < GUILD_MEMBERSHIPS) {
        g_members[g_count] = *m;
        g_members[g_count].id = (df_u16)g_count;
        g_count++;
    }
}

const GuildKind *guild_of_faction(int faction)
{
    const GuildKind *g;
    const Faction *f;

    g = guild_kind(faction);
    if (g) {
        return g;
    }
    /* A temple's own order (The Akatosh Chantry) stands under its god (Temple.GetDivine). */
    f = factionfile_open() ? faction_find(faction) : 0;
    return f && f->parent ? guild_kind(f->parent) : 0;
}

/* The membership in g, or -1. */
static int member_of(const GuildKind *g)
{
    int i;

    for (i = 0; g && i < g_count; i++) {
        if (g_members[i].guild == g->id) {
            return i;
        }
    }
    return -1;
}

int guild_rank(const GuildKind *g)
{
    int i;

    i = member_of(g);
    return i < 0 ? -1 : g_members[i].rank;
}

const char *guild_title(const GuildKind *g, int rank)
{
    return g && rank >= 0 && rank < GUILD_RANKS ? g->rank[rank] : "";
}

/* How many of the guild's skills reach rank's high mark, and how many more its low (CalculateNumHighLowSkills). */
static void count_skills(const GuildKind *g, const Character *c, int rank, int *high, int *low)
{
    int i;

    *high = 0;
    *low = 0;
    for (i = 0; i < GUILD_SKILLS && g->skill[i] != GUILD_NO_SKILL; i++) {
        int value;

        value = g->skill[i] < SKILL_COUNT ? c->skill[g->skill[i]] : 0;
        if (value >= g_rank_high[rank]) {
            (*high)++;
        } else if (value >= g_rank_low[rank]) {
            (*low)++;
        }
    }
}

int guild_rank_earned(const GuildKind *g, const Character *c, int rep)
{
    int high;
    int low;
    int r;

    if (rep < 0) {
        return -1;
    }
    for (r = 0; r < GUILD_RANKS; r++) {
        count_skills(g, c, r, &high, &low);
        if (rep < g_rank_rep[r] || high < 1 || low + high < 2) {
            break;
        }
    }
    return r - 1;
}

int guild_join(const GuildKind *g, const Character *c, int rep, int day)
{
    int i;

    if (member_of(g) >= 0) {
        return GUILD_ALREADY;
    }
    for (i = 0; i < g_count; i++) {
        const GuildKind *other;

        other = guild_kind(g_members[i].guild);
        if (other && other->group == g->group) {
            return GUILD_OTHER_OF_GROUP;
        }
    }
    if (rep < 0) {
        return GUILD_BAD_NAME;
    }
    if (guild_rank_earned(g, c, rep) < 0) {
        return GUILD_LOW_SKILL;
    }
    if (g_count == GUILD_MEMBERSHIPS) {
        return GUILD_NO_ROOM;
    }
    g_members[g_count].id = (df_u16)g_count;
    g_members[g_count].guild = g->id;
    g_members[g_count].rank = 0;
    g_members[g_count].changed_day = day;
    g_count++;
    return GUILD_JOINED;
}

int guild_update(const GuildKind *g, const Character *c, int rep, int day)
{
    int i;
    int earned;
    int k;

    i = member_of(g);
    if (i < 0 || day < g_members[i].changed_day + GUILD_RANK_DAYS) {
        return GUILD_SAME;
    }
    earned = guild_rank_earned(g, c, rep);
    if (earned == g_members[i].rank) {
        return GUILD_SAME;
    }
    /* No rank earned, by a bad name or skills below rank 0's, puts the member out (Guild.UpdateRank). */
    if (earned < 0) {
        for (k = i; k + 1 < g_count; k++) {
            g_members[k] = g_members[k + 1];
            g_members[k].id = (df_u16)k;
        }
        g_count--;
        return GUILD_EXPELLED;
    }
    g_members[i].rank = earned;
    g_members[i].changed_day = day;
    return g_members[i].rank;
}

int guild_training_price(int member, int level)
{
    return (member ? TRAIN_MEMBER : TRAIN_OTHER) * level;
}

int guild_trains(const GuildKind *g)
{
    return g && (member_of(g) >= 0 || (g->gives & GUILD_TRAINS_ALL));
}

int guild_train(Player *p, const GuildKind *g, int skill, int price, int now)
{
    const CareerSkill *s;
    int i;
    int taught;

    taught = 0;
    for (i = 0; g && i < GUILD_SKILLS && g->training[i] != GUILD_NO_SKILL; i++) {
        taught |= g->training[i] == skill;
    }
    if (!taught || skill < 0 || skill >= SKILL_COUNT) {
        return GUILD_NOT_TAUGHT;
    }
    if (now - g_last_training < TRAIN_EVERY) {
        return GUILD_TOO_SOON;
    }
    if (p->sheet.skill[skill] > TRAIN_MOST) {
        return GUILD_TOO_SKILLED;
    }
    if (price > (int)p->pack.gold) {
        return GUILD_NOT_ENOUGH_GOLD;
    }
    p->pack.gold -= (df_u32)price;
    g_last_training = now;
    p->sheet.fatigue -= TRAIN_FATIGUE;
    p->sheet.fatigue = p->sheet.fatigue < 0 ? 0 : p->sheet.fatigue;
    /* 10 to 20 uses times the skill's advancement (DaggerfallGuildServiceTraining.TrainSkill). */
    s = career_skill(skill);
    character_tally(&p->sheet, skill, (10 + combat_roll(11)) * (s && s->advance ? s->advance : 1));
    return GUILD_TRAINED;
}

int guild_last_training(void)
{
    return g_last_training;
}

void guild_set_last_training(int minute)
{
    g_last_training = minute;
}

/* The membership of the guild a building's faction is, or 0. */
static const GuildMembership *membership_of_faction(int faction, const GuildKind **out)
{
    const GuildKind *g;
    int i;

    g = guild_of_faction(faction);
    i = member_of(g);
    *out = g;
    return i < 0 ? 0 : &g_members[i];
}

int guild_can_rest_in(int faction)
{
    const GuildKind *g;

    return membership_of_faction(faction, &g) && (g->gives & GUILD_GIVES_REST);
}

int guild_free_rooms(int region)
{
    int i;

    for (i = 0; i < g_count; i++) {
        const GuildKind *g;
        const Faction *f;

        g = guild_kind(g_members[i].guild);
        if (!g || !(g->gives & GUILD_GIVES_FREE_ROOMS)) {
            continue;
        }
        if (g_members[i].rank >= FREE_ROOMS_RANK) {
            return 1;
        }
        f = factionfile_open() ? faction_find(g->id) : 0;
        if (f && f->region == region) {
            return 1;
        }
    }
    return 0;
}

/* (10 - rank) tenths, as DFU's ((10 - rank) << 8) / 10 x price >> 8. */
static int by_rank(int rank, int price)
{
    return (((10 - rank) << 8) / 10 * price) >> 8;
}

int guild_cure_price(int faction, int price)
{
    const GuildMembership *m;
    const GuildKind *g;

    m = membership_of_faction(faction, &g);
    return m && (g->gives & GUILD_GIVES_CHEAP_CURE) ? by_rank(m->rank, price) : price;
}

int guild_repair_price(int faction, int price)
{
    const GuildMembership *m;
    const GuildKind *g;

    m = membership_of_faction(faction, &g);
    return m && (g->gives & GUILD_GIVES_CHEAP_REPAIR) ? by_rank(m->rank, price) : price;
}

int guild_heals(int faction)
{
    const GuildMembership *m;
    const GuildKind *g;

    m = membership_of_faction(faction, &g);
    return m && g->healing_rank != GUILD_NO_SKILL && m->rank >= g->healing_rank;
}
