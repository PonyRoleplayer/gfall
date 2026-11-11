#include "game/ogfall/play/talk.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/enchant.h"
#include "engine/core/mem.h"
#include "game/ogfall/tables/talkdata.h"

/* See talk.h. */

#define SKILL_ETIQUETTE 1
#define SKILL_STREETWISE 2
#define ATTR_PERSONALITY 5

/* The original never clears the third tone's cache between conversations. */
static int g_stale_cache;

static int rand_range(int low, int high)
{
    return og_random_range(low, high);
}

int og_talk_start(OgTalk *talk, const OgTalkWho *who, const OgTalkPlayer *player, int *text)
{
    const OgCharacter *c;
    int f, own, social;

    *text = 0;
    mem_zero(talk, sizeof(*talk));
    talk->npc = who->npc;
    if (who->blocked) {
        *text = OG_TALK_TEXT_REFUSES;
        return 0;
    }
    if (!who->person) {
        *text = OG_TALK_TEXT_NOT_A_PERSON;
        return 0;
    }
    own = og_faction_index(who->faction);
    if (own < 0) {
        *text = OG_TALK_TEXT_NOT_A_PERSON;
        return 0;
    }
    talk->own = who->faction;
    /* The first faction of a group, a noble house or a temple up the tree speaks for them. */
    f = own;
    while (f >= 0) {
        const OgFactionDef *d;

        d = og_faction_def(f);
        if (d->type == 2 || d->type == 7 || d->type == 9) {
            break;
        }
        f = og_faction_index(og_faction_parent_id(d->id));
    }
    talk->faction = f >= 0 ? og_faction_def(f)->id : who->faction;
    c = og_character();
    talk->disposition = og_faction_rep(talk->faction) + player->temple_rank;
    social = og_faction_def(og_faction_index(talk->faction))->social_group;
    /* A group below five adds the player's reputation with it; the reaction enchantments of every item carried (reaction_mod_item_cb) add or take ten each. */
    if (social < 5) {
        talk->disposition += c->reputation[social];
    }
    talk->disposition += og_enchant_reaction(social);
    if (talk->disposition < -20) {
        *text = OG_TALK_TEXT_REFUSES;
        return 0;
    }
    return 1;
}

static int faction_greeting(OgTalk *talk, const OgTalkPlayer *player, int faction_id)
{
    const OgFactionDef *d;
    int relation, roll, rep, social;

    d = og_faction_def(og_faction_index(faction_id));
    if (!d || d->type == 15 || d->type == 14) {
        return 0;
    }
    relation = og_faction_relation(faction_id, player->members, player->member_count, &talk->score);
    rep = og_faction_rep(faction_id);
    talk->score += rep;
    social = d->social_group;
    if (social < 5) {
        talk->score += og_character()->reputation[social];
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

int og_talk_open(OgTalk *talk, const OgTalkPlayer *player, int witnessed, int *text)
{
    const OgFactionDef *own, *speaks;
    int greeting, id, social;

    *text = 0;
    own = og_faction_def(og_faction_index(talk->own));
    speaks = og_faction_def(og_faction_index(talk->faction));
    if (!own || !speaks) {
        return OG_TALK_REFUSED;
    }
    if (speaks->social_group == 4 && witnessed) {
        og_law_note(OG_CRIME_CONSPIRACY);
        og_guards_summon(0);
        return OG_TALK_REFUSED;
    }
    talk->open = 1;
    talk->tone = OG_TONE_NORMAL;
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
    talk->attitude = og_talk_attitude(talk);
    greeting = faction_greeting(talk, player, id);
    if (greeting) {
        *text = greeting;
        return talk->hostile ? OG_TALK_HOSTILE : OG_TALK_OPEN;
    }
    if (talk->disposition < 0) {
        *text = OG_TALK_TEXT_HOSTILE;
    } else if (talk->disposition < 10) {
        *text = OG_TALK_TEXT_COOL;
    } else if (talk->disposition < 30) {
        *text = OG_TALK_TEXT_WARM;
    } else {
        *text = OG_TALK_TEXT_FRIENDLY;
    }
    return OG_TALK_OPEN;
}

int og_talk_attitude(OgTalk *talk)
{
    const OgCharacter *c;
    int attitude, roll, kind;

    c = og_character();
    attitude = 0;
    if (talk->tone == OG_TONE_POLITE) {
        attitude += rand_range(1, 100) < c->skill[SKILL_ETIQUETTE] ? 10 : -5;
        if (talk->cache[0] == 0) {
            og_skill_use(SKILL_ETIQUETTE);
        }
    }
    if (talk->tone == OG_TONE_BLUNT) {
        attitude += rand_range(1, 100) < c->skill[SKILL_STREETWISE] ? 10 : -5;
        /* The original tests the middle tone's cache here. */
        if (talk->cache[1] == 0) {
            og_skill_use(SKILL_STREETWISE);
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
        if (talk->tone == OG_TONE_BLUNT) {
            g_stale_cache = attitude;
        }
    }
    if (attitude < roll) {
        return 0;
    }
    return roll + 30 <= attitude ? 2 : 1;
}

int og_talk_knows(OgTalk *talk)
{
    int seed, kind, knows;

    seed = og_random_range(0, 32767);
    if (talk->own == 806 || talk->own == 842) {
        return 1;
    }
    kind = ((1 - talk->mode) << 2) + talk->tab;
    og_random_seed((df_u32)((talk->npc ^ (talk->npc >> 16)) + (unsigned)kind) + (unsigned)talk->row);
    knows = rand_range(1, 20) <= g_knows[kind][talk->style] + 10;
    og_random_seed((df_u32)seed);
    return knows;
}

void og_talk_tone(OgTalk *talk, int tone)
{
    talk->tone = tone;
    talk->attitude = og_talk_attitude(talk);
}

int og_talk_category(int building_type, int building_flags)
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

const char *og_talk_category_name(int category)
{
    return category >= 0 && category < OG_TALK_CATEGORIES ? g_category_name[category] : "";
}

int og_talk_topics(OgTalkTopic *out, int cap, const OgTalkBuilding *buildings, int count)
{
    OgTalkTopic swap;
    int i, n, last, swapped;

    n = 0;
    for (i = 0; i < count && n < cap - 1; i++) {
        /* talk_build_place_topics lists by type (houses, past 16 and nothing else are skipped), unlike building_category. */
        if (buildings[i].type >= 17 || buildings[i].type == 1) {
            continue;
        }
        out[n].category = og_talk_category(buildings[i].type, 0);
        out[n].distance = buildings[i].distance;
        out[n].building = buildings[i].id;
        n++;
    }
    out[n].category = OG_TALK_REGIONAL;
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

int og_talk_has_category(const OgTalkBuilding *buildings, int count, int category)
{
    int i;

    if (category == OG_TALK_REGIONAL) {
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (og_talk_category(buildings[i].type, buildings[i].flags) == category) {
            return 1;
        }
    }
    return 0;
}

int og_talk_answer(OgTalk *talk, const OgTalkAsk *ask, int *text)
{
    int knows;

    talk->mode = ask->mode;
    talk->tab = ask->tab;
    talk->row = ask->row;
    if (ask->mode == 0) {
        /* The news was given already: no roll is made (talk_ask_news is not reached). */
        if (talk->news_asked) {
            *text = 1457;
            return OG_ANSWER_TEXT;
        }
        knows = og_talk_knows(talk);
        if (talk->own != 806 && talk->own != 842) {
            talk->news_asked = 1;
        }
        if (ask->row == 0 && ask->news) {
            return OG_ANSWER_NEWS;
        }
    } else {
        knows = og_talk_knows(talk);
    }
    if (ask->tab == 3) {
        if (ask->no_work) {
            *text = 8078;
        } else {
            *text = 8075 + (talk->attitude == 0 ? 0 : talk->attitude == 1 ? 1 : 2);
        }
        return OG_ANSWER_TEXT;
    }
    if (ask->mode == 0) {
        *text = g_tell_answer[knows][talk->style][talk->attitude];
        return OG_ANSWER_TEXT;
    }
    if (ask->tab == 0 && ask->category == OG_TALK_REGIONAL) {
        *text = 28;
        return OG_ANSWER_TEXT;
    }
    *text = g_where_answer[knows][talk->style][talk->attitude];
    return OG_ANSWER_TEXT;
}

int og_talk_hint(const OgTalkAsk *ask, int regional_known, int distance, int outdoors, int *noted)
{
    *noted = 0;
    if (ask->tab == 0 && ask->category == OG_TALK_REGIONAL) {
        return regional_known ? 10 : 11;
    }
    if (outdoors && (distance < 2048 || rand_range(1, 100) <= 25)) {
        *noted = 1;
        return 7332;
    }
    return 7333;
}
