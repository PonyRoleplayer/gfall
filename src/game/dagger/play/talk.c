#include "game/dagger/play/talk.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/enchant.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/talkdata.h"

/* See talk.h. */

#define SKILL_ETIQUETTE 1
#define SKILL_STREETWISE 2
#define ATTR_PERSONALITY 5

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
        if (d->type == 2 || d->type == 7 || d->type == 9) {
            break;
        }
        f = faction_index(faction_parent_id(d->id));
    }
    talk->faction = f >= 0 ? faction_def(f)->id : who->faction;
    c = character_state();
    talk->disposition = faction_rep(talk->faction) + player->temple_rank;
    social = faction_def(faction_index(talk->faction))->social_group;
    /* A group below five adds the player's reputation with it; the reaction enchantments of every item carried (reaction_mod_item_cb) add or take ten each. */
    if (social < 5) {
        talk->disposition += c->reputation[social];
    }
    talk->disposition += enchant_reaction(social);
    if (talk->disposition < -20) {
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
    if (!d || d->type == 15 || d->type == 14) {
        return 0;
    }
    relation = faction_relation(faction_id, player->members, player->member_count, &talk->score);
    rep = faction_rep(faction_id);
    talk->score += rep;
    social = d->social_group;
    if (social < 5) {
        talk->score += character_state()->reputation[social];
    }
    roll = rand_range(0, 15) - 10;
    if (talk->score >= roll && rep >= 30) {
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
    if (speaks->social_group == 4 && witnessed) {
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
    id = own->type == 15 || own->type == 14 ? own->id : speaks->id;
    social = (own->type == 15 || own->type == 14) ? own->social_group : speaks->social_group;
    talk->style = social < 5 ? social : 1;
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
    } else if (talk->disposition < 10) {
        *text = TALK_TEXT_COOL;
    } else if (talk->disposition < 30) {
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
        attitude += rand_range(1, 100) < c->skill[SKILL_ETIQUETTE] ? 10 : -5;
        if (talk->cache[0] == 0) {
            skill_use(SKILL_ETIQUETTE);
        }
    }
    if (talk->tone == TONE_BLUNT) {
        attitude += rand_range(1, 100) < c->skill[SKILL_STREETWISE] ? 10 : -5;
        /* The original tests the middle tone's cache here. */
        if (talk->cache[1] == 0) {
            skill_use(SKILL_STREETWISE);
        }
    }
    kind = ((1 - talk->mode) << 2) + talk->tab;
    attitude += g_attitude_mod[kind];
    attitude += c->attribute[ATTR_PERSONALITY] / 5;
    roll = rand_range(0, 20);
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
    return roll + 30 <= attitude ? 2 : 1;
}

int talk_knows(TalkState *talk)
{
    int seed, kind, knows;

    seed = random_range(0, 32767);
    if (talk->own == 806 || talk->own == 842) {
        return 1;
    }
    kind = ((1 - talk->mode) << 2) + talk->tab;
    random_seed((df_u32)((talk->npc ^ (talk->npc >> 16)) + (unsigned)kind) + (unsigned)talk->row);
    knows = rand_range(1, 20) <= g_knows[kind][talk->style] + 10;
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

    if (building_type != 1 && building_type <= 16) {
        for (i = 0; i < 13; i++) {
            if (g_category_type[i] == building_type) {
                return i;
            }
        }
        return 13;
    }
    return (building_flags & 8) ? 13 : -1;
}

const char *talk_category_name(int category)
{
    return category >= 0 && category < TALK_CATEGORIES ? g_category_name[category] : "";
}

int talk_topics(TalkTopic *out, int cap, const TalkBuilding *buildings, int count)
{
    TalkTopic swap;
    int i, n, last, swapped;

    n = 0;
    for (i = 0; i < count && n < cap - 1; i++) {
        /* talk_build_place_topics lists by type (houses, past 16 and nothing else are skipped), unlike building_category. */
        /* A house that is a quest's site (flag 8, talk_open's quest buildings) is listed too, under category 13. */
        if ((buildings[i].type >= 17 && !(buildings[i].flags & 8)) || buildings[i].type == 1) {
            continue;
        }
        out[n].category = talk_category(buildings[i].type, buildings[i].flags);
        out[n].distance = buildings[i].distance;
        out[n].building = buildings[i].id;
        n++;
    }
    out[n].category = TALK_REGIONAL;
    out[n].distance = 0;
    out[n].building = 0;
    n++;
    last = n - 2;
    swapped = 1;
    while (swapped && last >= 2) {
        swapped = 0;
        for (i = 0; i < last; i++) {
            if (out[i + 1].distance < out[i].distance) {
                swap = out[i];
                out[i] = out[i + 1];
                out[i + 1] = swap;
                swapped = 1;
            }
        }
        last--;
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
            *text = 1457;
            return ANSWER_TEXT;
        }
        knows = talk_knows(talk);
        if (talk->own != 806 && talk->own != 842) {
            talk->news_asked = 1;
        }
        if (ask->row == 0 && ask->news) {
            return ANSWER_NEWS;
        }
    } else {
        knows = talk_knows(talk);
    }
    if (ask->tab == 3) {
        if (ask->no_work) {
            *text = 8078;
        } else {
            *text = 8075 + (talk->attitude == 0 ? 0 : talk->attitude == 1 ? 1 : 2);
        }
        return ANSWER_TEXT;
    }
    if (ask->mode == 0) {
        *text = g_tell_answer[knows][talk->style][talk->attitude];
        return ANSWER_TEXT;
    }
    if (ask->tab == 0 && ask->category == TALK_REGIONAL) {
        *text = 28;
        return ANSWER_TEXT;
    }
    *text = g_where_answer[knows][talk->style][talk->attitude];
    return ANSWER_TEXT;
}

int talk_hint(const TalkAsk *ask, int regional_known, int distance, int outdoors, int *noted)
{
    *noted = 0;
    if (ask->tab == 0 && ask->category == TALK_REGIONAL) {
        return regional_known ? 10 : 11;
    }
    if (outdoors && (distance < 2048 || rand_range(1, 100) <= 25)) {
        *noted = 1;
        return 7332;
    }
    return 7333;
}
