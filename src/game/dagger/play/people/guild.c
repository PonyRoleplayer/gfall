#include "game/dagger/play/people/guild.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/state.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/character/curse.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/guilddata.h"

/* See guild.h. */

void guilds_clear(void)
{
    mem_zero(guilds_state(), sizeof(GuildsState));
}

int guilds_valid(const GuildsState *g)
{
    int i;

    for (i = 0; i < MEMBERSHIPS; i++) {
        if (g->m[i].used && (g->m[i].kind < 0 || g->m[i].kind > 255 || g->m[i].faction < 0)) {
            return 0;
        }
    }
    return g->thieves_count >= 0 && g->thieves_count <= GUILD_INVITED && g->brotherhood_count >= 0 && g->brotherhood_count <= GUILD_INVITED;
}

int guild_kind_of_faction(int faction)
{
    static const short knights[KNIGHT_ORDERS] = { 368, 408, 409, 410, 411, 413, 414, 415, 416, 417 };
    static const short temples[TEMPLES][2] = { {21, 82}, {22, 84}, {24, 28}, {26, 92}, {27, 94}, {29, 98}, {33, 106}, {35, 36} };
    int i;

    switch (faction) {
    case FACTION_DARK_BROTHERHOOD: return GUILD_DARK_BROTHERHOOD;
    case FACTION_THIEVES_GUILD: return GUILD_THIEVES;
    case FACTION_MAGES_GUILD: return GUILD_MAGES;
    case FACTION_FIGHTERS_GUILD: return GUILD_FIGHTERS;
    default: break;
    }
    for (i = 0; i < TEMPLES; i++) {
        if (faction == temples[i][0] || faction == temples[i][1]) {
            return KIND_TEMPLE + GUILD_FIRST_TEMPLE + i;
        }
    }
    for (i = 0; i < KNIGHT_ORDERS; i++) {
        if (faction == knights[i]) {
            return KIND_KNIGHT + GUILD_FIRST_KNIGHT + i;
        }
    }
    return -1;
}

Membership *guild_find_faction(int faction)
{
    int i;

    for (i = 0; i < MEMBERSHIPS; i++) {
        if (guilds_state()->m[i].used && !curse_suspended(i) && guilds_state()->m[i].faction == faction) {
            return &guilds_state()->m[i];
        }
    }
    return 0;
}

Membership *guild_find_kind(int kind)
{
    int i;

    for (i = 0; i < MEMBERSHIPS; i++) {
        if (guilds_state()->m[i].used && !curse_suspended(i) && guilds_state()->m[i].kind == kind) {
            return &guilds_state()->m[i];
        }
    }
    return 0;
}

Membership *guild_find_bits(int bits)
{
    int i;

    for (i = 0; i < MEMBERSHIPS; i++) {
        if (guilds_state()->m[i].used && !curse_suspended(i) && (guilds_state()->m[i].kind & bits)) {
            return &guilds_state()->m[i];
        }
    }
    return 0;
}

/* guild_best_skill: the highest of a guild's skills, never the one to skip; its value, and which it was. */
static int best_skill(int guild, int skip, int *which)
{
    const unsigned char *list;
    int i, best, value;

    list = g_guild_skills[guild];
    best = -1;
    *which = -1;
    for (i = 0; list[i] != SKILL_LIST_END; i++) {
        value = character_state()->skill[list[i]];
        if (value > best && list[i] != skip) {
            best = value;
            *which = list[i];
        }
    }
    return best;
}

int guild_join_check(int kind, int reputation)
{
    int guild, which, primary, secondary;

    guild = kind & KIND_GUILD_MASK;
    if (guild >= GUILDS) {
        return 2;
    }
    if (reputation < 0) {
        return 1;
    }
    primary = best_skill(guild, -1, &which);
    if (primary < JOIN_PRIMARY_MIN) {
        return 2;
    }
    secondary = best_skill(guild, which, &which);
    return secondary < JOIN_SECONDARY_MIN ? 2 : 0;
}

int guild_rank_for_skills(int kind, int reputation)
{
    int guild, which, rank, primary, secondary, step_primary, step_secondary;

    guild = kind & KIND_GUILD_MASK;
    if (guild >= GUILDS) {
        return 0;
    }
    rank = reputation / REPUTATION_PER_RANK;
    primary = best_skill(guild, -1, &which);
    secondary = best_skill(guild, which, &which);
    /* The index of the first step the skill reaches (the original's scan, which would run past the table for a skill over 94: held at RANK_STEPS). */
    step_primary = 0;
    while (step_primary < RANK_STEPS && g_rank_primary[step_primary] < primary) {
        step_primary++;
    }
    step_secondary = 0;
    while (step_secondary < RANK_STEPS && g_rank_secondary[step_secondary] < secondary) {
        step_secondary++;
    }
    if (step_primary < rank) { rank = step_primary; }
    if (step_secondary < rank) { rank = step_secondary; }
    return rank > MAX_RANK ? MAX_RANK : rank;
}

int guild_offer(int kind, int faction, int reputation, GuildStep *step)
{
    int guild, refusal;

    (void)faction;
    mem_zero(step, sizeof(*step));
    step->map = -1;
    guild = kind & KIND_GUILD_MASK;
    if (guild >= GUILDS || guild_find_faction(faction)) {
        return 0;
    }
    if ((kind & KIND_KNIGHT) && guild_find_bits(KIND_KNIGHT)) {
        return 0;
    }
    if ((kind & KIND_TEMPLE) && guild_find_bits(KIND_TEMPLE)) {
        return 0;
    }
    refusal = guild_join_check(kind, reputation);
    if (refusal == 2) {
        step->text = g_guild_message[guild][GUILD_MSG_REFUSED];
    } else if (refusal == 1) {
        step->text = g_guild_message[guild][GUILD_MSG_REPUTATION];
    } else {
        step->text = g_guild_message[guild][GUILD_MSG_ASK];
        step->ask = 1;
    }
    return step->text != 0;
}

int guild_join(int kind, int faction, df_u32 now, GuildStep *step)
{
    int i;
    Membership *m;

    mem_zero(step, sizeof(*step));
    step->map = -1;
    m = 0;
    for (i = 0; i < MEMBERSHIPS && !m; i++) {
        if (!guilds_state()->m[i].used) {
            m = &guilds_state()->m[i];
        }
    }
    if (!m) {
        return 0;
    }
    m->used = 1;
    m->kind = kind;
    m->rank = 0;
    m->faction = faction;
    m->rank_time = now;
    m->armor = 0;
    step->joined = 1;
    step->text = g_guild_message[kind & KIND_GUILD_MASK][3];
    return 1;
}

int guild_promote(int kind, int faction, int reputation, df_u32 now, GuildStep *step)
{
    Membership *m;
    int guild, delta, expelled, rank;

    mem_zero(step, sizeof(*step));
    step->map = -1;
    guild = kind & KIND_GUILD_MASK;
    m = guild_find_faction(faction);
    if (!m || guild >= GUILDS || now - m->rank_time <= RANK_MONTH) {
        return 0;
    }
    expelled = 0;
    delta = guild_rank_for_skills(kind, reputation) - m->rank;
    if (reputation < 0) {
        delta = -(m->rank + 1);
        expelled = 1;
    }
    if (delta <= 0 && !expelled) {
        return 0;
    }
    /* The rank is a byte: an expulsion takes it past 100. */
    rank = (m->rank + delta) & 255;
    m->rank = rank;
    m->rank_time = now;
    if (guild == GUILD_THIEVES && (rank == THIEVES_MAP_RANK_1 || rank == THIEVES_MAP_RANK_2)) {
        step->map = 0;
    }
    if (guild == GUILD_DARK_BROTHERHOOD && rank < RANK_EXPELLED_OVER) {
        step->map = 1;
    }
    if (rank > RANK_EXPELLED_OVER) {
        step->text = TEXT_GUILD_EXPELLED;
        step->expelled = 1;
        m->used = 0;
        if (guild == GUILD_THIEVES) { guilds_state()->thieves_count = 0; }
        if (guild == GUILD_DARK_BROTHERHOOD) { guilds_state()->brotherhood_count = 0; }
        return 1;
    }
    if (delta < 0) {
        step->text = TEXT_GUILD_DEMOTED;
        step->demoted = 1;
        return 1;
    }
    step->text = g_guild_message[guild][GUILD_MSG_RANK0 + rank];
    step->promoted = 1;
    return 1;
}

int guild_local_temple_rank(int region)
{
    Membership *m;
    const FactionDef *f;

    m = guild_find_bits(KIND_TEMPLE);
    if (!m) {
        return 0;
    }
    f = faction_def(faction_index(m->faction));
    return f && f->region == region ? m->rank : 0;
}

int guild_is_local_knight(int region)
{
    Membership *m;
    const FactionDef *f;

    m = guild_find_bits(KIND_KNIGHT);
    if (!m) {
        return 0;
    }
    f = faction_def(faction_index(m->faction));
    if (f && f->region == region) {
        return 1;
    }
    return m->rank >= KNIGHT_ANYWHERE_RANK;
}

void guild_count_crime(int crime, int count, df_u32 now)
{
    GuildsState *g;

    g = guilds_state();
    /* A townsperson or a city guard killed (the kill kinds) feeds a lycanthrope, before the guilds look at it (docs/curses.md 7.4). */
    if (crime == GUILD_COUNT_KILL) { curse_innocent_killed(); }
    if (crime == GUILD_COUNT_THIEF) {
        if (g->thieves_invite || g->thieves_count == GUILD_INVITED) {
            return;
        }
        g->thieves_count += (signed char)count;
        if (g->thieves_count >= THIEVES_INVITE_AT) {
            g->thieves_invite = now + GUILD_INVITE_DELAY;
        }
        return;
    }
    if (g->brotherhood_invite || g->brotherhood_count == GUILD_INVITED) {
        return;
    }
    g->brotherhood_count += (signed char)count;
    if (g->brotherhood_count >= BROTHERHOOD_INVITE_AT) {
        g->brotherhood_invite = now + GUILD_INVITE_DELAY;
    }
}

int guild_invitation(df_u32 now, int outdoors, int *kind)
{
    GuildsState *g;

    g = guilds_state();
    if (g->thieves_count != GUILD_INVITED && g->thieves_invite && g->thieves_invite < now && outdoors) {
        g->thieves_count = GUILD_INVITED;
        g->thieves_invite = 0;
        *kind = GUILD_THIEVES;
        return 1;
    }
    if (g->brotherhood_count == GUILD_INVITED || !g->brotherhood_invite || g->brotherhood_invite >= now || !outdoors) {
        return 0;
    }
    g->brotherhood_count = GUILD_INVITED;
    g->brotherhood_invite = 0;
    *kind = GUILD_DARK_BROTHERHOOD;
    return 1;
}

const char *guild_service_label(int person_faction)
{
    int i;

    for (i = 0; i < SERVICE_LABELS; i++) {
        if (g_service_faction[i] == person_faction) {
            return g_service_label[i];
        }
    }
    return 0;
}

const char *guild_picture(int member)
{
    /* "gild0%ci0.img": the digit is 0 for a stranger and 1 for a member (the VFS wants it as the others are named). */
    static char name[] = "GILD00I0.IMG";

    name[5] = (char)('0' + (member ? 1 : 0));
    return name;
}

int guild_price_scale(int rank)
{
    return (((MAX_RANK + 1) - rank) * PRICE_SCALE_ONE) / (MAX_RANK + 1);
}

/* One temple's rule (kinds 142..149): the rank at which a member is healed (-1 never, 0 always), then three members' services (faction, rank, which) and the strangers' (training faction, blessing faction, 0 for none). */
typedef struct TempleRule {
    short heal;
    short member[4][2];
    unsigned char which[4];
    short training, blessing;
} TempleRule;

static const TempleRule g_temple[TEMPLES] = {
    /* 142 */ { 0, {{453, 1}, {454, 4}, {455, 4}, {456, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_SOULGEMS, SVC_DAEDRA}, 241, 0 },
    /* 143 */ { 2, {{462, 1}, {463, 6}, {464, 8}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 243, 460 },
    /* 144 */ { 1, {{468, 2}, {469, 5}, {470, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 245, 466 },
    /* 145 */ { 1, {{473, 4}, {474, 5}, {475, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 247, 471 },
    /* 146 */ { 2, {{480, 3}, {481, 5}, {482, 6}}, {SVC_MAGIC_ITEMS, SVC_ITEM_MAKER, SVC_DAEDRA}, 249, 477 },
    /* 147 */ { 2, {{485, 1}, {487, 5}, {488, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 250, 484 },
    /* 148 */ { 0, {{490, 2}, {491, 5}, {492, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 252, 493 },
    /* 149 */ { 1, {{496, 3}, {497, 6}, {498, 7}}, {SVC_SPELLS, SVC_SPELL_MAKER, SVC_DAEDRA}, 254, 494 }
};

/* A strange face at a temple: training, the temple's quest, a donation, a cure, a blessing; the rest are reserved. */
static int temple_stranger(const TempleRule *t, int person)
{
    if (person == t->training) { return SVC_TRAINING; }
    if (person == PERSON_TEMPLE_QUEST) { return SVC_TEMPLE_QUEST; }
    if (person == PERSON_TEMPLE_DONATE) { return SVC_DONATE; }
    if (person == PERSON_TEMPLE_CURE) { return SVC_CURE; }
    if (t->blessing && person == t->blessing) { return SVC_BLESSING; }
    return SVC_MEMBERS_ONLY;
}

int guild_service(int kind, int person, int rank, int *heal)
{
    int guild, i, member;

    *heal = 0;
    member = rank >= 0;
    guild = kind & KIND_GUILD_MASK;
    if (kind >= TEMPLE_AKATOSH && kind < TEMPLE_AKATOSH + TEMPLES) {
        const TempleRule *t;

        t = &g_temple[kind - TEMPLE_AKATOSH];
        if (member) {
            *heal = t->heal >= 0 && rank >= t->heal;
            for (i = 0; i < 4; i++) {
                if (t->member[i][0] && person == t->member[i][0]) {
                    return rank >= t->member[i][1] ? t->which[i] : SVC_RANK_REFUSED;
                }
            }
        }
        return temple_stranger(t, person);
    }
    switch (kind) {
    case 0:
        if (!member) { return SVC_NONE; }
        switch (person) {
        case 839: return SVC_TRAINING;
        case 841: return rank >= 1 ? SVC_POTIONS : SVC_RANK_REFUSED;
        case 840: return rank >= 3 ? SVC_POTION_MAKER : SVC_RANK_REFUSED;
        case 843: return rank >= 5 ? SVC_SOULGEMS : SVC_RANK_REFUSED;
        case 842: return rank >= 7 ? SVC_SPYMASTER : SVC_RANK_REFUSED;
        case 807: return SVC_QUEST;
        default: return SVC_TALK;
        }
    case 1:
        *heal = member;
        if (member) {
            switch (person) {
            case 61: return SVC_TRAINING;
            case 64: return SVC_SPELL_MAKER;
            case 65: return rank >= 3 ? SVC_MAGIC_ITEMS : SVC_RANK_REFUSED;
            case 802: return rank >= 5 ? SVC_ITEM_MAKER : SVC_RANK_REFUSED;
            case 66: return rank >= 6 ? SVC_DAEDRA : SVC_RANK_REFUSED;
            case 62: return rank >= 8 ? SVC_TELEPORT : SVC_RANK_REFUSED;
            default: break;
            }
        }
        *heal = 0;
        switch (person) {
        case 60: return SVC_SPELLS;
        case 63: return SVC_QUEST;
        case 801: return member ? SVC_IDENTIFY : SVC_MEMBERS_ONLY;
        default: return SVC_MEMBERS_ONLY;
        }
    case 2:
        if (member) {
            switch (person) {
            case 849: return SVC_TRAINING;
            case 850: return SVC_REPAIRS;
            case 851: return SVC_QUEST;
            default: break;
            }
        }
        return person == 851 ? SVC_QUEST : SVC_TALK;
    case 3:
        if (!member) { return SVC_NONE; }
        switch (person) {
        case 803: return SVC_TRAINING;
        case 804: return SVC_QUEST;
        case 805: return rank >= 2 ? SVC_FENCE : SVC_RANK_REFUSED;
        case 806: return rank >= 4 ? SVC_SPYMASTER : SVC_RANK_REFUSED;
        default: return SVC_TALK;
        }
    default: break;
    }
    if (guild >= 4 && guild <= 13) {
        if (member) {
            if (person == 845) { return SVC_ARMOR; }
            if (person == 848) { return SVC_HOUSE; }
        }
        return person == 846 ? SVC_QUEST : SVC_MEMBERS_ONLY;
    }
    return SVC_TALK;
}

const unsigned char *training_skills(int person_faction)
{
    int i;

    for (i = 0; i < 12; i++) {
        if (g_trainer_faction[i] == person_faction) {
            return g_trainer_skills[i];
        }
    }
    return 0;
}

int training_offer(df_u32 now, int level, int member, int *price, int *text)
{
    *text = 0;
    if (now - guilds_state()->last_training < TRAINING_GAP) {
        *text = TEXT_TRAINING_TOO_SOON;
        return 0;
    }
    *price = level * TRAINING_PRICE_PER_LEVEL;
    if (!member) {
        *price <<= TRAINING_STRANGER_SHIFT;
    }
    return 1;
}

int training_learn(int skill, int price, df_u32 now, int *text, int *minutes)
{
    *minutes = 0;
    if (skill < 0 || skill >= SKILLS) {
        return 0;
    }
    if (character_state()->skill[skill] > TRAINING_SKILL_MOST) {
        *text = TEXT_TRAINING_TOO_GOOD;
        return 0;
    }
    gold_spend(price);
    guilds_state()->last_training = now;
    skill_add_uses(skill, random_range(TRAINING_USES_MIN, TRAINING_USES_MAX) * g_skill_advance[skill]);
    *text = TEXT_TRAINED;
    *minutes = TRAINING_MINUTES;
    return 1;
}

/* blessing_apply: the points added to the skill, attribute or region's legal standing, held at 100; the amount kept is the whole. */
static int blessing_apply(int target, int amount, int region)
{
    if (target == BLESS_LEGAL) {
        region_add_legal(region, amount);
        if (region_legal(region) > STAT_MOST) {
            region_add_legal(region, STAT_MOST - region_legal(region));
        }
    } else if (target & BLESS_ATTRIBUTE) {
        character_state()->attribute[target & (BLESS_ATTRIBUTE - 1)] += amount;
        if (character_state()->attribute[target & (BLESS_ATTRIBUTE - 1)] > STAT_MOST) { character_state()->attribute[target & (BLESS_ATTRIBUTE - 1)] = STAT_MOST; }
    } else {
        character_state()->skill[target] += amount;
        if (character_state()->skill[target] > STAT_MOST) { character_state()->skill[target] = STAT_MOST; }
    }
    return amount;
}

static void blessing_remove(const struct BlessingInfo *b)
{
    if (b->target == BLESS_LEGAL) {
        region_add_legal(b->region, -b->amount);
    } else if (b->target & BLESS_ATTRIBUTE) {
        character_state()->attribute[b->target & (BLESS_ATTRIBUTE - 1)] -= b->amount;
    } else {
        character_state()->skill[b->target] -= b->amount;
    }
}

int blessing_buy(int kind, int rank, int region, df_u32 now, int *text)
{
    static const short target[8] = { 0, 14, 133, 134, 129, 135, BLESS_LEGAL, 132 };
    static const short words[8] = { 0, 705, 707, 709, 710, 712, 716, 717 };
    struct BlessingInfo *b;

    *text = 0;
    if (kind <= TEMPLE_AKATOSH || kind > TEMPLE_AKATOSH + TEMPLES - 1) {
        return 0;
    }
    b = &guilds_state()->blessing;
    if (b->active) {
        *text = TEXT_BLESSED_ALREADY;
        return 0;
    }
    b->active = 1;
    b->target = target[kind - TEMPLE_AKATOSH];
    /* The gift to the legal standing goes to the region the player is in, but the record never names it: the end takes it from region 0 (blessing_remove). */
    b->region = 0;
    /* A stranger (rank -1) is given BLESS_DAYS_BASE days and BLESS_AMOUNT_STRANGER (guild_buy_blessing). */
    b->end = (df_u32)((rank < 0 ? 0 : rank) + BLESS_DAYS_BASE) * MINUTES_PER_DAY + now;
    b->amount = blessing_apply(b->target, rank < 0 ? BLESS_AMOUNT_STRANGER : rank + BLESS_AMOUNT_MEMBER_BASE, region);
    *text = words[kind - TEMPLE_AKATOSH];
    return 1;
}

int blessing_expire(df_u32 now)
{
    struct BlessingInfo *b;

    b = &guilds_state()->blessing;
    if (!b->active || b->end >= now) {
        return 0;
    }
    blessing_remove(b);
    mem_zero(b, sizeof(*b));
    return 1;
}

int knight_armor(Membership *m, int *text)
{
    int metal;
    df_u32 id;

    *text = 0;
    if ((1u << m->rank) & m->armor) {
        *text = TEXT_ARMOR_GIVEN;
        return 0;
    }
    /* run_0006DC35.c:459..471: forced_material = rank - 1 as a byte (over 100, that is rank 0, becomes 2; over 10 becomes 10), and the original's forced_material is
     * the material + 1 with 0 for none: so rank 0 gets material 1, rank 1 none (-1, the usual roll) and rank r >= 2 material r - 2. */
    metal = (m->rank - 1) & 255;
    if (metal > 100) { metal = 2; } else if (metal > 10) { metal = 10; }
    metal -= 1;
    if (metal > MATERIALS - 1) { metal = MATERIALS - 1; }
    id = item_create(game_state()->inventory_id, ITEM_GROUP_ARMOR, random_range(0, KNIGHT_ARMOR_PIECES), metal);
    if (!id) {
        return 0;
    }
    m->armor |= 1u << m->rank;
    *text = TEXT_ARMOR_GIFT;
    return 1;
}

int knight_house_allowed(const Membership *m, int has_house, int *text)
{
    *text = 0;
    if (has_house) {
        return 0;
    }
    if (m->rank != KNIGHT_HOUSE_RANK) {
        *text = TEXT_HOUSE_RANK;
        return 0;
    }
    return 1;
}

int daedra_offer(int hall_faction, int hall_reputation, df_u32 now, DaedraStep *step)
{
    int day, i, prince, price;

    step->text = 0; step->ask = 0; step->price = 0; step->prince = 0; step->line = 0;
    day = (int)((now % MINUTES_PER_YEAR) / MINUTES_PER_DAY);
    prince = 0;
    for (i = 0; i < DAEDRA_DAYS; i++) {
        /* The fourth entry is the original's "no summoning" sentinel (D_00179E66). */
        if (g_daedra_day[i] == day && i != DAEDRA_DAY_NONE) {
            prince = i + 1;
            break;
        }
    }
    if (!prince) {
        step->text = TEXT_NO_SUMMONING;
        return 0;
    }
    if (hall_faction != FACTION_MAGES_GUILD && faction_has_enemy(hall_faction, prince)) {
        step->line = "Today is a day for summoning enemies. We won't.";
        return 0;
    }
    price = (100 - hall_reputation) * SUMMON_PRICE_PER_REP + SUMMON_PRICE_BASE;
    if (price < 0) { price = -price; }
    if (price > SUMMON_PRICE_MOST) { price = SUMMON_PRICE_MOST; }
    step->prince = prince;
    step->price = price;
    step->text = TEXT_SUMMON_PRICE;
    step->ask = 1;
    return 1;
}

int daedra_summon(int prince, int weather, int *kind)
{
    int chance, n;

    n = weather == WEATHER_STORM ? SUMMON_FORCED_STORM : SUMMON_FORCED;
    if (random_range(0, RAND_MAX_WATCOM) % (PERCENT + 1) <= n) {
        prince = PRINCE_FORCED;
    }
    chance = SUMMON_CHANCE_BASE + faction_rep(prince);
    if (g_daedra_weather[prince] == DAEDRA_ANY_WEATHER || weather == g_daedra_weather[prince]) {
        chance += SUMMON_WEATHER_BONUS;
    }
    if (random_range(1, 100) > chance) {
        return DAEDRA_FAILED;
    }
    if (guilds_state()->daedra_met & (1u << prince)) {
        *kind = g_daedra_kind[random_range(0, DAEDRA_SERVANTS - 1)];
        return DAEDRA_ALLY;
    }
    guilds_state()->daedra_met |= 1u << prince;
    return DAEDRA_QUEST;
}

const char *guild_rank_name(int kind, int rank)
{
    if (kind < 0 || kind >= GUILDS || rank < 0 || rank > MAX_RANK) { return ""; }
    return g_rank_name[kind][rank];
}
