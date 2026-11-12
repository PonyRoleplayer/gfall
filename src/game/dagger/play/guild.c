#include "game/dagger/play/guild.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/inventory.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/guilddata.h"

/* See guild.h. */

void og_guilds_clear(void)
{
    mem_zero(og_guilds(), sizeof(OgGuilds));
}

int og_guilds_valid(const OgGuilds *g)
{
    int i;

    for (i = 0; i < OG_MEMBERSHIPS; i++) {
        if (g->m[i].used && (g->m[i].kind < 0 || g->m[i].kind > 255 || g->m[i].faction < 0)) {
            return 0;
        }
    }
    return g->thieves_count >= 0 && g->thieves_count <= 100 && g->brotherhood_count >= 0 && g->brotherhood_count <= 100;
}

int og_guild_kind_of_faction(int faction)
{
    static const short knights[10] = { 368, 408, 409, 410, 411, 413, 414, 415, 416, 417 };
    static const short temples[8][2] = { {21, 82}, {22, 84}, {24, 28}, {26, 92}, {27, 94}, {29, 98}, {33, 106}, {35, 36} };
    int i;

    switch (faction) {
    case 108: return 0;
    case 42: return 3;
    case 40: return 1;
    case 41: return 2;
    default: break;
    }
    for (i = 0; i < 8; i++) {
        if (faction == temples[i][0] || faction == temples[i][1]) {
            return OG_KIND_TEMPLE + 14 + i;
        }
    }
    for (i = 0; i < 10; i++) {
        if (faction == knights[i]) {
            return OG_KIND_KNIGHT + 4 + i;
        }
    }
    return -1;
}

OgMembership *og_guild_find_faction(int faction)
{
    int i;

    for (i = 0; i < OG_MEMBERSHIPS; i++) {
        if (og_guilds()->m[i].used && og_guilds()->m[i].faction == faction) {
            return &og_guilds()->m[i];
        }
    }
    return 0;
}

OgMembership *og_guild_find_kind(int kind)
{
    int i;

    for (i = 0; i < OG_MEMBERSHIPS; i++) {
        if (og_guilds()->m[i].used && og_guilds()->m[i].kind == kind) {
            return &og_guilds()->m[i];
        }
    }
    return 0;
}

OgMembership *og_guild_find_bits(int bits)
{
    int i;

    for (i = 0; i < OG_MEMBERSHIPS; i++) {
        if (og_guilds()->m[i].used && (og_guilds()->m[i].kind & bits)) {
            return &og_guilds()->m[i];
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
    for (i = 0; list[i] != 255; i++) {
        value = og_character()->skill[list[i]];
        if (value > best && list[i] != skip) {
            best = value;
            *which = list[i];
        }
    }
    return best;
}

int og_guild_join_check(int kind, int reputation)
{
    int guild, which, primary, secondary;

    guild = kind & 63;
    if (guild >= OG_GUILDS) {
        return 2;
    }
    if (reputation < 0) {
        return 1;
    }
    primary = best_skill(guild, -1, &which);
    if (primary < 22) {
        return 2;
    }
    secondary = best_skill(guild, which, &which);
    return secondary < 4 ? 2 : 0;
}

int og_guild_rank_for_skills(int kind, int reputation)
{
    int guild, which, rank, primary, secondary, step_primary, step_secondary;

    guild = kind & 63;
    if (guild >= OG_GUILDS) {
        return 0;
    }
    rank = reputation / 10;
    primary = best_skill(guild, -1, &which);
    secondary = best_skill(guild, which, &which);
    /* The index of the first step the skill reaches (the original's scan, which would run past the table for a skill over 94: held at 10). */
    step_primary = 0;
    while (step_primary < 10 && g_rank_primary[step_primary] < primary) {
        step_primary++;
    }
    step_secondary = 0;
    while (step_secondary < 10 && g_rank_secondary[step_secondary] < secondary) {
        step_secondary++;
    }
    if (step_primary < rank) { rank = step_primary; }
    if (step_secondary < rank) { rank = step_secondary; }
    return rank > 9 ? 9 : rank;
}

int og_guild_offer(int kind, int faction, int reputation, OgGuildStep *step)
{
    int guild, refusal;

    (void)faction;
    mem_zero(step, sizeof(*step));
    step->map = -1;
    guild = kind & 63;
    if (guild >= OG_GUILDS || og_guild_find_faction(faction)) {
        return 0;
    }
    if ((kind & OG_KIND_KNIGHT) && og_guild_find_bits(OG_KIND_KNIGHT)) {
        return 0;
    }
    if ((kind & OG_KIND_TEMPLE) && og_guild_find_bits(OG_KIND_TEMPLE)) {
        return 0;
    }
    refusal = og_guild_join_check(kind, reputation);
    if (refusal == 2) {
        step->text = g_guild_message[guild][0];
    } else if (refusal == 1) {
        step->text = g_guild_message[guild][1];
    } else {
        step->text = g_guild_message[guild][2];
        step->ask = 1;
    }
    return step->text != 0;
}

int og_guild_join(int kind, int faction, df_u32 now, OgGuildStep *step)
{
    int i;
    OgMembership *m;

    mem_zero(step, sizeof(*step));
    step->map = -1;
    m = 0;
    for (i = 0; i < OG_MEMBERSHIPS && !m; i++) {
        if (!og_guilds()->m[i].used) {
            m = &og_guilds()->m[i];
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
    step->text = g_guild_message[kind & 63][3];
    return 1;
}

int og_guild_promote(int kind, int faction, int reputation, df_u32 now, OgGuildStep *step)
{
    OgMembership *m;
    int guild, delta, expelled, rank;

    mem_zero(step, sizeof(*step));
    step->map = -1;
    guild = kind & 63;
    m = og_guild_find_faction(faction);
    if (!m || guild >= OG_GUILDS || now - m->rank_time <= OG_RANK_MONTH) {
        return 0;
    }
    expelled = 0;
    delta = og_guild_rank_for_skills(kind, reputation) - m->rank;
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
    if (guild == 3 && (rank == 6 || rank == 8)) {
        step->map = 0;
    }
    if (guild == 0 && rank < 100) {
        step->map = 1;
    }
    if (rank > 100) {
        step->text = 668;
        step->expelled = 1;
        m->used = 0;
        if (guild == 3) { og_guilds()->thieves_count = 0; }
        if (guild == 0) { og_guilds()->brotherhood_count = 0; }
        return 1;
    }
    if (delta < 0) {
        step->text = 667;
        step->demoted = 1;
        return 1;
    }
    step->text = g_guild_message[guild][3 + rank];
    step->promoted = 1;
    return 1;
}

int og_guild_local_temple_rank(int region)
{
    OgMembership *m;
    const OgFactionDef *f;

    m = og_guild_find_bits(OG_KIND_TEMPLE);
    if (!m) {
        return 0;
    }
    f = og_faction_def(og_faction_index(m->faction));
    return f && f->region == region ? m->rank : 0;
}

int og_guild_is_local_knight(int region)
{
    OgMembership *m;
    const OgFactionDef *f;

    m = og_guild_find_bits(OG_KIND_KNIGHT);
    if (!m) {
        return 0;
    }
    f = og_faction_def(og_faction_index(m->faction));
    if (f && f->region == region) {
        return 1;
    }
    return m->rank >= 4;
}

void og_guild_count_crime(int crime, int count, df_u32 now)
{
    OgGuilds *g;

    g = og_guilds();
    if (crime == 5) {
        if (g->thieves_invite || g->thieves_count == 100) {
            return;
        }
        g->thieves_count += (signed char)count;
        if (g->thieves_count >= 6) {
            g->thieves_invite = now + 4320u;
        }
        return;
    }
    if (g->brotherhood_invite || g->brotherhood_count == 100) {
        return;
    }
    g->brotherhood_count += (signed char)count;
    if (g->brotherhood_count >= 15) {
        g->brotherhood_invite = now + 4320u;
    }
}

int og_guild_invitation(df_u32 now, int outdoors, int *kind)
{
    OgGuilds *g;

    g = og_guilds();
    if (g->thieves_count != 100 && g->thieves_invite && g->thieves_invite < now && outdoors) {
        g->thieves_count = 100;
        g->thieves_invite = 0;
        *kind = 3;
        return 1;
    }
    if (g->brotherhood_count == 100 || !g->brotherhood_invite || g->brotherhood_invite >= now || !outdoors) {
        return 0;
    }
    g->brotherhood_count = 100;
    g->brotherhood_invite = 0;
    *kind = 0;
    return 1;
}

const char *og_guild_service_label(int person_faction)
{
    int i;

    for (i = 0; i < 89; i++) {
        if (g_service_faction[i] == person_faction) {
            return g_service_label[i];
        }
    }
    return 0;
}

const char *og_guild_picture(int member)
{
    /* "gild0%ci0.img": the digit is 0 for a stranger and 1 for a member (the VFS wants it as the others are named). */
    static char name[] = "GILD00I0.IMG";

    name[5] = (char)('0' + (member ? 1 : 0));
    return name;
}

int og_guild_price_scale(int rank)
{
    return ((10 - rank) << 8) / 10;
}

/* One temple's rule (kinds 142..149): the rank at which a member is healed (-1 never, 0 always), then three members' services (faction, rank, which) and the strangers' (training faction, blessing faction, 0 for none). */
typedef struct TempleRule {
    short heal;
    short member[3][2];
    unsigned char which[3];
    short training, blessing;
} TempleRule;

static const TempleRule g_temple[8] = {
    /* 142 */ { 0, {{453, 1}, {454, 4}, {455, 4}}, {OG_SVC_POTIONS, OG_SVC_POTION_MAKER, OG_SVC_SOULGEMS}, 241, 0 },
    /* 143 */ { 2, {{462, 1}, {463, 6}, {464, 8}}, {OG_SVC_POTIONS, OG_SVC_POTION_MAKER, OG_SVC_DAEDRA}, 243, 460 },
    /* 144 */ { 1, {{468, 2}, {469, 5}, {470, 7}}, {OG_SVC_POTIONS, OG_SVC_POTION_MAKER, OG_SVC_DAEDRA}, 245, 466 },
    /* 145 */ { 1, {{473, 4}, {474, 5}, {475, 7}}, {OG_SVC_POTIONS, OG_SVC_POTION_MAKER, OG_SVC_DAEDRA}, 247, 471 },
    /* 146 */ { 2, {{480, 3}, {481, 5}, {482, 6}}, {OG_SVC_MAGIC_ITEMS, OG_SVC_ITEM_MAKER, OG_SVC_DAEDRA}, 249, 477 },
    /* 147 */ { 2, {{485, 1}, {487, 5}, {488, 7}}, {OG_SVC_POTIONS, OG_SVC_POTION_MAKER, OG_SVC_DAEDRA}, 250, 484 },
    /* 148 */ { 0, {{490, 2}, {491, 5}, {492, 7}}, {OG_SVC_POTIONS, OG_SVC_POTION_MAKER, OG_SVC_DAEDRA}, 252, 493 },
    /* 149 */ { 1, {{496, 3}, {497, 6}, {498, 7}}, {OG_SVC_SPELLS, OG_SVC_SPELL_MAKER, OG_SVC_DAEDRA}, 254, 494 }
};

/* A strange face at a temple: training, the temple's quest, a donation, a cure, a blessing; the rest are reserved. */
static int temple_stranger(const TempleRule *t, int person)
{
    if (person == t->training) { return OG_SVC_TRAINING; }
    if (person == 240) { return OG_SVC_TEMPLE_QUEST; }
    if (person == 810) { return OG_SVC_DONATE; }
    if (person == 813) { return OG_SVC_CURE; }
    if (t->blessing && person == t->blessing) { return OG_SVC_BLESSING; }
    return OG_SVC_MEMBERS_ONLY;
}

int og_guild_service(int kind, int person, int rank, int *heal)
{
    int guild, i, member;

    *heal = 0;
    member = rank >= 0;
    guild = kind & 63;
    if (kind >= 142 && kind <= 149) {
        const TempleRule *t;

        t = &g_temple[kind - 142];
        if (member) {
            *heal = t->heal >= 0 && rank >= t->heal;
            for (i = 0; i < 3; i++) {
                if (person == t->member[i][0]) {
                    return rank >= t->member[i][1] ? t->which[i] : OG_SVC_RANK_REFUSED;
                }
            }
        }
        return temple_stranger(t, person);
    }
    switch (kind) {
    case 0:
        if (!member) { return OG_SVC_NONE; }
        switch (person) {
        case 839: return OG_SVC_TRAINING;
        case 841: return rank >= 1 ? OG_SVC_POTIONS : OG_SVC_RANK_REFUSED;
        case 840: return rank >= 3 ? OG_SVC_POTION_MAKER : OG_SVC_RANK_REFUSED;
        case 843: return rank >= 5 ? OG_SVC_SOULGEMS : OG_SVC_RANK_REFUSED;
        case 842: return rank >= 7 ? OG_SVC_SPYMASTER : OG_SVC_RANK_REFUSED;
        case 807: return OG_SVC_QUEST;
        default: return OG_SVC_TALK;
        }
    case 1:
        *heal = member;
        if (member) {
            switch (person) {
            case 61: return OG_SVC_TRAINING;
            case 64: return OG_SVC_SPELL_MAKER;
            case 65: return rank >= 3 ? OG_SVC_MAGIC_ITEMS : OG_SVC_RANK_REFUSED;
            case 802: return rank >= 5 ? OG_SVC_ITEM_MAKER : OG_SVC_RANK_REFUSED;
            case 66: return rank >= 6 ? OG_SVC_DAEDRA : OG_SVC_RANK_REFUSED;
            case 62: return rank >= 8 ? OG_SVC_TELEPORT : OG_SVC_RANK_REFUSED;
            default: break;
            }
        }
        *heal = 0;
        switch (person) {
        case 60: return OG_SVC_SPELLS;
        case 63: return OG_SVC_QUEST;
        case 801: return member ? OG_SVC_IDENTIFY : OG_SVC_MEMBERS_ONLY;
        default: return OG_SVC_MEMBERS_ONLY;
        }
    case 2:
        if (member) {
            switch (person) {
            case 849: return OG_SVC_TRAINING;
            case 850: return OG_SVC_REPAIRS;
            case 851: return OG_SVC_QUEST;
            default: break;
            }
        }
        return person == 851 ? OG_SVC_QUEST : OG_SVC_TALK;
    case 3:
        if (!member) { return OG_SVC_NONE; }
        switch (person) {
        case 803: return OG_SVC_TRAINING;
        case 804: return OG_SVC_QUEST;
        case 805: return rank >= 2 ? OG_SVC_FENCE : OG_SVC_RANK_REFUSED;
        case 806: return rank >= 4 ? OG_SVC_SPYMASTER : OG_SVC_RANK_REFUSED;
        default: return OG_SVC_TALK;
        }
    default: break;
    }
    if (guild >= 4 && guild <= 13) {
        if (member) {
            if (person == 845) { return OG_SVC_ARMOR; }
            if (person == 848) { return OG_SVC_HOUSE; }
        }
        return person == 846 ? OG_SVC_QUEST : OG_SVC_MEMBERS_ONLY;
    }
    return OG_SVC_TALK;
}

const unsigned char *og_training_skills(int person_faction)
{
    int i;

    for (i = 0; i < 12; i++) {
        if (g_trainer_faction[i] == person_faction) {
            return g_trainer_skills[i];
        }
    }
    return 0;
}

int og_training_offer(df_u32 now, int level, int member, int *price, int *text)
{
    *text = 0;
    if (now - og_guilds()->last_training < 720u) {
        *text = 4023;
        return 0;
    }
    *price = level * 100;
    if (!member) {
        *price <<= 2;
    }
    return 1;
}

int og_training_learn(int skill, int price, df_u32 now, int *text, int *minutes)
{
    *minutes = 0;
    if (skill < 0 || skill >= OG_SKILLS) {
        return 0;
    }
    if (og_character()->skill[skill] > 50) {
        *text = 4022;
        return 0;
    }
    og_gold_spend(price);
    og_guilds()->last_training = now;
    og_skill_add_uses(skill, og_random_range(10, 20) * g_skill_advance[skill]);
    *text = 5221;
    *minutes = 180;
    return 1;
}

/* blessing_apply: the points added to the skill, attribute or region's legal standing, held at 100; the amount kept is the whole. */
static int blessing_apply(int target, int amount, int region)
{
    if (target == 255) {
        og_region_add_legal(region, amount);
        if (og_region_legal(region) > 100) {
            og_region_add_legal(region, 100 - og_region_legal(region));
        }
    } else if (target & 128) {
        og_character()->attribute[target & 127] += amount;
        if (og_character()->attribute[target & 127] > 100) { og_character()->attribute[target & 127] = 100; }
    } else {
        og_character()->skill[target] += amount;
        if (og_character()->skill[target] > 100) { og_character()->skill[target] = 100; }
    }
    return amount;
}

static void blessing_remove(const struct OgBlessing *b)
{
    if (b->target == 255) {
        og_region_add_legal(b->region, -b->amount);
    } else if (b->target & 128) {
        og_character()->attribute[b->target & 127] -= b->amount;
    } else {
        og_character()->skill[b->target] -= b->amount;
    }
}

int og_blessing_buy(int kind, int rank, int region, df_u32 now, int *text)
{
    static const short target[8] = { 0, 14, 133, 134, 129, 135, 255, 132 };
    static const short words[8] = { 0, 705, 707, 709, 710, 712, 716, 717 };
    struct OgBlessing *b;

    *text = 0;
    if (kind == 142 || kind < 142 || kind > 149) {
        return 0;
    }
    b = &og_guilds()->blessing;
    if (b->active) {
        *text = 454;
        return 0;
    }
    b->active = 1;
    b->target = target[kind - 142];
    /* The gift to the legal standing goes to the region the player is in, but the record never names it: the end takes it from region 0 (blessing_remove). */
    b->region = 0;
    b->end = (df_u32)(rank + 4) * 1440u + now;
    b->amount = blessing_apply(b->target, rank + 10, region);
    *text = words[kind - 142];
    return 1;
}

int og_blessing_expire(df_u32 now)
{
    struct OgBlessing *b;

    b = &og_guilds()->blessing;
    if (!b->active || b->end >= now) {
        return 0;
    }
    blessing_remove(b);
    mem_zero(b, sizeof(*b));
    return 1;
}

int og_knight_armor(OgMembership *m, int *text)
{
    int metal;
    df_u32 id;

    *text = 0;
    if ((1u << m->rank) & m->armor) {
        *text = 461;
        return 0;
    }
    /* forced_material = rank - 1 as a byte: over 100 (rank 0) is 2, over 10 is 10; the metals here end at 9. */
    metal = (m->rank - 1) & 255;
    if (metal > 100) { metal = 2; } else if (metal > 10) { metal = 10; }
    if (metal > 9) { metal = 9; }
    id = og_item_create(og_state()->inventory_id, 2, og_random_range(0, 6), metal);
    if (!id) {
        return 0;
    }
    m->armor |= 1u << m->rank;
    *text = 463;
    return 1;
}

int og_knight_house_allowed(const OgMembership *m, int has_house, int *text)
{
    *text = 0;
    if (has_house) {
        return 0;
    }
    if (m->rank != 9) {
        *text = 460;
        return 0;
    }
    return 1;
}

int og_daedra_offer(int hall_faction, int hall_reputation, df_u32 now, OgDaedraStep *step)
{
    int day, i, prince, price;

    step->text = 0; step->ask = 0; step->price = 0; step->prince = 0; step->line = 0;
    day = (int)((now % 518400u) / 1440u);
    prince = 0;
    for (i = 0; i < 16; i++) {
        /* The fourth entry is the original's "no summoning" sentinel (D_00179E66). */
        if (g_daedra_day[i] == day && i != 3) {
            prince = i + 1;
            break;
        }
    }
    if (!prince) {
        step->text = 480;
        return 0;
    }
    if (hall_faction != 40 && og_faction_has_enemy(hall_faction, prince)) {
        step->line = "Today is a day for summoning enemies. We won't.";
        return 0;
    }
    price = (100 - hall_reputation) * 1000 + 100000;
    if (price < 0) { price = -price; }
    if (price > 200000) { price = 200000; }
    step->prince = prince;
    step->price = price;
    step->text = 481;
    step->ask = 1;
    return 1;
}

int og_daedra_summon(int prince, int weather, int *kind)
{
    int chance, n;

    n = weather == 6 ? 15 : 5;
    if (og_random_range(0, 32767) % 101 <= n) {
        prince = 9;
    }
    chance = 30 + og_faction_rep(prince);
    if (g_daedra_weather[prince] == 100 || weather == g_daedra_weather[prince]) {
        chance += 30;
    }
    if (og_random_range(1, 100) > chance) {
        return OG_DAEDRA_FAILED;
    }
    if (og_guilds()->daedra_met & (1u << prince)) {
        *kind = g_daedra_kind[og_random_range(0, 4)];
        return OG_DAEDRA_ALLY;
    }
    og_guilds()->daedra_met |= 1u << prince;
    return OG_DAEDRA_QUEST;
}

const char *og_guild_rank_name(int kind, int rank)
{
    if (kind < 0 || kind > 21 || rank < 0 || rank > 9) { return ""; }
    return g_rank_name[kind][rank];
}
