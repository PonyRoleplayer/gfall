#include "game/dagger/play/people/talk.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/tables/magic.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/magic/magicitem.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/talkdata.h"

/* See talk.h. */


/* The original never clears the third tone's cache between conversations. */
static int g_stale_cache;

static int rand_range(int low, int high)
{
    return random_range(low, high);
}

int talk_start(TalkState *talk, const TalkWho *who, const TalkPlayer *player, int *text)
{
    const CharacterState *c;
    int f, own, social;

    *text = 0;
    mem_zero(talk, sizeof(*talk));
    talk->npc = who->npc;
    if (who->blocked) {
        *text = TALK_TEXT_REFUSES;
        return 0;
    }
    if (!who->person) {
        *text = TALK_TEXT_NOT_A_PERSON;
        return 0;
    }
    own = faction_index(who->faction);
    if (own < 0) {
        *text = TALK_TEXT_NOT_A_PERSON;
        return 0;
    }
    talk->own = who->faction;
    /* The first faction of a group, a noble house or a temple up the tree speaks for them. */
    f = own;
    while (f >= 0) {
        const FactionDef *d;

        d = faction_def(f);
        if (d->type == FACTION_GROUP || d->type == FACTION_PROVINCE || d->type == FACTION_TEMPLE) {
            break;
        }
        f = faction_index(faction_parent_id(d->id));
    }
    talk->faction = f >= 0 ? faction_def(f)->id : who->faction;
    c = character_state();
    talk->disposition = faction_rep(talk->faction) + player->temple_rank;
    social = faction_def(faction_index(talk->faction))->social_group;
    /* A group below five adds the player's reputation with it; the reaction enchantments of every item carried (reaction_mod_item_cb) add or take ten each. */
    if (social < SOCIAL_GROUPS) {
        talk->disposition += c->reputation[social];
    }
    talk->disposition += enchant_reaction(social);
    if (magic_artifact_worn(ARTIFACT_TALK_BONUS)) { talk->disposition += c->attribute[ATTR_PER]/5; }
    if (talk->disposition < TALK_REFUSE_BELOW) {
        *text = TALK_TEXT_REFUSES;
        return 0;
    }
    return 1;
}

static int faction_greeting(TalkState *talk, const TalkPlayer *player, int faction_id)
{
    const FactionDef *d;
    int relation, roll, rep, social;

    d = faction_def(faction_index(faction_id));
    if (!d || d->type == FACTION_PEOPLE || d->type == FACTION_COURTS) {
        return 0;
    }
    relation = faction_relation(faction_id, player->members, player->member_count, &talk->score);
    rep = faction_rep(faction_id);
    talk->score += rep;
    social = d->social_group;
    if (social < SOCIAL_GROUPS) {
        talk->score += character_state()->reputation[social];
    }
    roll = rand_range(0, GREETING_ROLL_MAX) - GREETING_ROLL_SHIFT;
    if (talk->score >= roll && rep >= GREETING_ALLIED_REP) {
        return g_greeting[relation][0];
    }
    if (talk->score >= roll) {
        return g_greeting[relation][1];
    }
    talk->hostile = 1;
    return g_greeting[relation][2];
}

int talk_open(TalkState *talk, const TalkPlayer *player, int witnessed, int *text)
{
    const FactionDef *own, *speaks;
    int greeting, id, social;

    *text = 0;
    own = faction_def(faction_index(talk->own));
    speaks = faction_def(faction_index(talk->faction));
    if (!own || !speaks) {
        return TALK_REFUSED;
    }
    if (speaks->social_group == SOCIAL_UNDERWORLD && witnessed) {
        law_note(CRIME_CONSPIRACY);
        guards_summon(0);
        return TALK_REFUSED;
    }
    talk->open = 1;
    talk->tone = TONE_NORMAL;
    talk->mode = 1;
    talk->tab = 0;
    talk->row = 0;
    talk->score = 0;
    talk->hostile = 0;
    talk->cache[0] = talk->cache[1] = 0;
    talk->cache[2] = g_stale_cache;
    /* The speech style is the social group, a commoner's and a merchant's swapped; a group past four is a merchant's. */
    id = own->type == FACTION_PEOPLE || own->type == FACTION_COURTS ? own->id : speaks->id;
    social = (own->type == FACTION_PEOPLE || own->type == FACTION_COURTS) ? own->social_group : speaks->social_group;
    talk->style = social < SOCIAL_GROUPS ? social : SOCIAL_MERCHANTS;
    if (talk->style == 1) {
        talk->style = 0;
    } else if (talk->style == 0) {
        talk->style = 1;
    }
    talk->attitude = talk_attitude(talk);
    greeting = faction_greeting(talk, player, id);
    if (greeting) {
        *text = greeting;
        return talk->hostile ? TALK_HOSTILE : TALK_OPEN;
    }
    if (talk->disposition < 0) {
        *text = TALK_TEXT_HOSTILE;
    } else if (talk->disposition < GREET_COOL_BELOW) {
        *text = TALK_TEXT_COOL;
    } else if (talk->disposition < GREET_WARM_BELOW) {
        *text = TALK_TEXT_WARM;
    } else {
        *text = TALK_TEXT_FRIENDLY;
    }
    return TALK_OPEN;
}

int talk_attitude(TalkState *talk)
{
    const CharacterState *c;
    int attitude, roll, kind;

    c = character_state();
    attitude = 0;
    if (talk->tone == TONE_POLITE) {
        attitude += rand_range(1, PERCENT) < c->skill[SKILL_ETIQUETTE] ? TONE_PASS : TONE_FAIL;
        if (talk->cache[0] == 0) {
            skill_use(SKILL_ETIQUETTE);
        }
    }
    if (talk->tone == TONE_BLUNT) {
        attitude += rand_range(1, PERCENT) < c->skill[SKILL_STREETWISE] ? TONE_PASS : TONE_FAIL;
        /* The original tests the middle tone's cache here. */
        if (talk->cache[1] == 0) {
            skill_use(SKILL_STREETWISE);
        }
    }
    kind = ((1 - talk->mode) << 2) + talk->tab;
    attitude += g_attitude_mod[kind];
    attitude += c->attribute[ATTR_PER] / 5;
    roll = rand_range(0, ATTITUDE_ROLL_MAX);
    if (talk->cache[talk->tone] != 0) {
        attitude = talk->cache[talk->tone];
    } else {
        talk->cache[talk->tone] = attitude;
        if (talk->tone == TONE_BLUNT) {
            g_stale_cache = attitude;
        }
    }
    if (attitude < roll) {
        return 0;
    }
    return roll + ATTITUDE_BEST_OVER <= attitude ? 2 : 1;
}

int talk_knows(TalkState *talk)
{
    int seed, kind, knows;

    seed = random_range(0, RAND_MAX_WATCOM);
    if (talk->own == FACTION_SHADOW_SPIES || talk->own == FACTION_DARK_PLOTTERS) {
        return 1;
    }
    kind = ((1 - talk->mode) << 2) + talk->tab;
    random_seed((df_u32)((talk->npc ^ (talk->npc >> 16)) + (unsigned)kind) + (unsigned)talk->row);
    knows = rand_range(1, KNOWS_ROLL_MAX) <= g_knows[kind][talk->style] + KNOWS_BASE;
    random_seed((df_u32)seed);
    return knows;
}

void talk_tone(TalkState *talk, int tone)
{
    talk->tone = tone;
    talk->attitude = talk_attitude(talk);
}

int talk_category(int building_type, int building_flags)
{
    int i;

    if (building_type != BUILDING_HOUSE_FOR_SALE && building_type <= BUILDING_PALACE) {
        for (i = 0; i < TALK_BUILDING_CATEGORIES; i++) {
            if (g_category_type[i] == building_type) {
                return i;
            }
        }
        return TALK_CATEGORY_OTHER;
    }
    return (building_flags & TALK_BUILDING_FLAG) ? TALK_CATEGORY_OTHER : -1;
}

const char *talk_category_name(int category)
{
    return category >= 0 && category < TALK_CATEGORIES ? g_category_name[category] : "";
}

int talk_topics(TalkTopic *out, int cap, const TalkBuilding *buildings, int count)
{
    int i, n;

    n = 0;
    for (i = 0; i < count && n < cap - 1; i++) {
        int category, j;

        category = talk_category(buildings[i].type, buildings[i].flags);
        if (category < 0) {
            continue;
        }
        /* Stable nearest-first order, so equal distances keep the town's order. */
        for (j = n; j > 0 && buildings[i].distance < out[j - 1].distance; j--) {
            out[j] = out[j - 1];
        }
        out[j].category = category;
        out[j].distance = buildings[i].distance;
        out[j].building = buildings[i].id;
        n++;
    }
    if (n < cap) {
        out[n].category = TALK_REGIONAL;
        out[n].distance = 0;
        out[n++].building = 0;
    }
    return n;
}

int talk_has_category(const TalkBuilding *buildings, int count, int category)
{
    int i;

    if (category == TALK_REGIONAL) {
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (talk_category(buildings[i].type, buildings[i].flags) == category) {
            return 1;
        }
    }
    return 0;
}

int talk_answer(TalkState *talk, const TalkAsk *ask, int *text)
{
    int knows;

    talk->mode = ask->mode;
    talk->tab = ask->tab;
    talk->row = ask->row;
    if (ask->mode == 0) {
        /* The news was given already: no roll is made (talk_ask_news is not reached). */
        if (talk->news_asked) {
            *text = TALK_TEXT_NEWS_ASKED;
            return ANSWER_TEXT;
        }
        knows = talk_knows(talk);
        if (talk->own != FACTION_SHADOW_SPIES && talk->own != FACTION_DARK_PLOTTERS) {
            talk->news_asked = 1;
        }
        if (ask->row == 0 && ask->news) {
            return ANSWER_NEWS;
        }
    } else {
        knows = talk_knows(talk);
    }
    if (ask->tab == TALK_TAB_WORK) {
        if (ask->no_work) {
            *text = TALK_TEXT_NO_WORK;
        } else {
            *text = TALK_TEXT_WORK + (talk->attitude == 0 ? 0 : talk->attitude == 1 ? 1 : 2);
        }
        return ANSWER_TEXT;
    }
    if (ask->mode == 0) {
        *text = g_tell_answer[knows][talk->style][talk->attitude];
        return ANSWER_TEXT;
    }
    if (ask->tab == 0 && ask->category == TALK_REGIONAL) {
        *text = TALK_TEXT_REGIONAL;
        return ANSWER_TEXT;
    }
    *text = g_where_answer[knows][talk->style][talk->attitude];
    return ANSWER_TEXT;
}

int talk_hint(const TalkAsk *ask, int regional_known, int distance, int outdoors, int *noted)
{
    *noted = 0;
    if (ask->tab == 0 && ask->category == TALK_REGIONAL) {
        return regional_known ? TALK_TEXT_REGION_KNOWN : TALK_TEXT_REGION_UNKNOWN;
    }
    if (outdoors && (distance < TALK_MARK_NEAR || rand_range(1, PERCENT) <= TALK_MARK_CHANCE)) {
        *noted = 1;
        return TALK_TEXT_MARKED;
    }
    return TALK_TEXT_DIRECTIONS;
}
