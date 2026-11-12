#include "check_ogtalk.h"
#include "check_util.h"
#include "game/dagger/play/talk.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/standing.h"
#include <stdio.h>
#include <string.h>

/* An individual (type 4) under a group, and a regional person (type 15) under one, in the real table. */
static int pick(int type, int *group)
{
    int i, f, id;

    for (i = 0; i < og_faction_count(); i++) {
        const OgFactionDef *d;

        d = og_faction_def(i);
        if (d->type != type || d->parent == 0) {
            continue;
        }
        f = i;
        id = d->parent;
        while (id) {
            const OgFactionDef *p;

            p = og_faction_def(og_faction_index(id));
            if (p && (p->type == 2 || p->type == 7 || p->type == 9)) {
                *group = p->id;
                return d->id;
            }
            id = p ? og_faction_parent_id(p->id) : 0;
        }
        (void)f;
    }
    return 0;
}

void check_ogtalk(void)
{
    OgTalk talk;
    OgTalkWho who;
    OgTalkPlayer player;
    int text, group, person, regional, rgroup, i, hostile, uses, att, first;
    static const int none[1] = { 0 };

    og_standing_defaults(og_standing());
    person = pick(4, &group);
    regional = pick(15, &rgroup);
    check_expect(person && regional, "talk: no individual or regional person in FACTION.TXT");
    if (!person || !regional) {
        return;
    }
    player.members = none;
    player.member_count = 0;
    player.temple_rank = 0;
    for (i = 0; i < 5; i++) {
        og_character()->reputation[i] = 0;
    }

    /* Who talks: not a person, a person who will not, and a disposition under -20 (the group's reputation, a temple's rank and the player's social reputation). */
    who.npc = 12345u;
    who.faction = person;
    who.person = 0;
    who.blocked = 0;
    check_expect(!og_talk_start(&talk, &who, &player, &text) && text == OG_TALK_TEXT_NOT_A_PERSON, "talk: a thing was spoken to");
    who.person = 1;
    who.blocked = 1;
    check_expect(!og_talk_start(&talk, &who, &player, &text) && text == OG_TALK_TEXT_REFUSES, "talk: a person who will not talk did");
    who.blocked = 0;
    og_standing()->rep[og_faction_index(group)] = -21;
    check_expect(!og_talk_start(&talk, &who, &player, &text) && text == OG_TALK_TEXT_REFUSES, "talk: a disposition of -21 was spoken to");
    og_standing()->rep[og_faction_index(group)] = -20;
    check_expect(og_talk_start(&talk, &who, &player, &text) && talk.disposition >= -20 - 5 && talk.faction == group, "talk: a disposition of -20 was refused, or the group is not the person's");
    og_standing()->rep[og_faction_index(group)] = 5;
    player.temple_rank = 3;
    og_talk_start(&talk, &who, &player, &text);
    check_expect(talk.disposition >= 8 - 5, "talk: a temple's rank did not add to the disposition");
    player.temple_rank = 0;

    /* A regional person has no faction greeting: a word by the disposition, in four steps (7206 .. 7209). */
    who.faction = regional;
    {
        static const int rep[4] = { -10, 5, 20, 60 };
        static const int want[4] = { OG_TALK_TEXT_HOSTILE, OG_TALK_TEXT_COOL, OG_TALK_TEXT_WARM, OG_TALK_TEXT_FRIENDLY };
        int k, social;

        for (k = 0; k < 4; k++) {
            og_standing()->rep[og_faction_index(rgroup)] = (short)rep[k];
            og_talk_start(&talk, &who, &player, &text);
            social = og_faction_def(og_faction_index(rgroup))->social_group;
            if (social < 5) {
                og_character()->reputation[social] = 0;
            }
            check_expect(og_talk_open(&talk, &player, 0, &text) == OG_TALK_OPEN && text == want[k], "talk: a regional person's word does not follow the disposition");
        }
    }

    /* A person of a group greets by what the player is to them: a record of the table (8550..8571); membership of the same faction is relation 0 (8550..8552). */
    who.faction = person;
    og_standing()->rep[og_faction_index(group)] = 50;
    og_talk_start(&talk, &who, &player, &text);
    check_expect(og_talk_open(&talk, &player, 0, &text) == OG_TALK_OPEN && text == 8570, "talk: a friendly group's greeting to a stranger is not 8570");
    {
        int members[1];

        members[0] = group;
        player.members = members;
        player.member_count = 1;
        og_talk_start(&talk, &who, &player, &text);
        og_talk_open(&talk, &player, 0, &text);
        check_expect(text >= 8550 && text <= 8552, "talk: a fellow member's greeting is not from relation 0");
        player.members = none;
        player.member_count = 0;
    }
    /* A hated group (-15, under the roll's floor of -10) is always hostile, and the hostile greeting closes the talk; at -5 the roll
     * (rand_range(0, 15) - 10) lets 6 in 16 through. */
    og_standing()->rep[og_faction_index(group)] = -15;
    hostile = 0;
    for (i = 0; i < 100; i++) {
        og_talk_start(&talk, &who, &player, &text);
        hostile += og_talk_open(&talk, &player, 0, &text) == OG_TALK_HOSTILE && text == 8571;
    }
    check_expect(hostile == 100, "talk: a hated group's greeting was not always hostile");
    og_standing()->rep[og_faction_index(group)] = -5;
    hostile = 0;
    for (i = 0; i < 400; i++) {
        og_talk_start(&talk, &who, &player, &text);
        hostile += og_talk_open(&talk, &player, 0, &text) == OG_TALK_HOSTILE;
    }
    check_expect(hostile > 200 && hostile < 300, "talk: at -5 the greeting was not hostile ten times in sixteen");
    og_standing()->rep[og_faction_index(group)] = 0;

    /* The first attitude: the neutral tone adds the kind's 10 and Personality / 5; a polite tone uses Etiquette once; it is cached. */
    og_standing()->rep[og_faction_index(group)] = 10;
    og_character()->attribute[5] = 50;
    og_character()->skill[1] = 100;
    og_talk_start(&talk, &who, &player, &text);
    og_talk_open(&talk, &player, 0, &text);
    check_expect(talk.cache[1] == 10 + 10, "talk: the neutral tone's first attitude is not 10 + Personality / 5");
    uses = (int)og_state()->skill_uses[1];
    og_talk_tone(&talk, OG_TONE_POLITE);
    first = talk.cache[0];
    check_expect(first == 10 + 10 + 10 && (int)og_state()->skill_uses[1] == uses + 1, "talk: a polite tone at 100 Etiquette is not 30, with one use");
    att = talk.attitude;
    og_talk_tone(&talk, OG_TONE_POLITE);
    check_expect(talk.cache[0] == first && (int)og_state()->skill_uses[1] == uses + 1 && att >= 0 && att <= 2, "talk: a second polite roll used the skill again, or changed the cached attitude");

    /* Knowing: always for factions 806 and 842; else a seeded roll the same each time, about (the table + 10) in 20 for this person's style. */
    og_talk_start(&talk, &who, &player, &text);
    og_talk_open(&talk, &player, 0, &text);
    talk.mode = 1;
    talk.tab = 0;
    talk.row = 3;
    {
        int a, b, yes;

        talk.npc = 777u;
        a = og_talk_knows(&talk);
        b = og_talk_knows(&talk);
        check_expect(a == b, "talk: the same person and row did not answer the same");
        yes = 0;
        for (i = 0; i < 400; i++) {
            talk.npc = 1000u + (unsigned)i * 97u;
            yes += og_talk_knows(&talk);
        }
        printf("talk: style %d, %d of 400 people know a place\n", talk.style, yes);
        check_expect(yes > 120 && yes < 400, "talk: the chance to know is outside what the table allows");
    }
    og_standing()->rep[og_faction_index(group)] = 0;

    /* The window's topics: categories by building type (the bank 3 is category 2, a house is none unless a quest site, an unlisted type is General). */
    {
        OgTalkBuilding b[5];
        OgTalkTopic t[16];
        OgTalkAsk ask;
        int n, noted, answer;

        check_expect(og_talk_category(3, 0) == 2 && og_talk_category(1, 0) == -1 && og_talk_category(1, 8) == 13 && og_talk_category(4, 0) == 13 &&
                     og_talk_category(0, 0) == 0 && og_talk_category(15, 0) == 12, "talk: building categories");
        check_expect(!strcmp(og_talk_category_name(2), "Banks") && !strcmp(og_talk_category_name(14), "Regional"), "talk: category names are not FALL.EXE's");
        b[0].id = 10; b[0].type = 3; b[0].flags = 0; b[0].distance = 900;
        b[1].id = 11; b[1].type = 1; b[1].flags = 0; b[1].distance = 10;
        b[2].id = 12; b[2].type = 15; b[2].flags = 0; b[2].distance = 300;
        b[3].id = 13; b[3].type = 2; b[3].flags = 0; b[3].distance = 600;
        b[4].id = 14; b[4].type = 20; b[4].flags = 0; b[4].distance = 50;
        n = og_talk_topics(t, 16, b, 5);
        check_expect(n == 4 && t[0].building == 12 && t[1].building == 13 && t[2].building == 10 && t[3].category == OG_TALK_REGIONAL && !t[3].building,
                     "talk: places are not listed nearest first, houses out, the regional entry last");
        check_expect(og_talk_has_category(b, 5, 2) && !og_talk_has_category(b, 5, 7) && og_talk_has_category(b, 5, OG_TALK_REGIONAL), "talk: town_has_category");

        /* The answers: "where is" picks its record by knowing, style and attitude; "tell me about" gives the news once. */
        og_standing_defaults(og_standing());
        who.person = 1; who.blocked = 0; who.npc = 777u; who.faction = person;
        og_character()->reputation[0] = og_character()->reputation[1] = og_character()->reputation[2] = og_character()->reputation[3] = og_character()->reputation[4] = 0;
        check_expect(og_talk_start(&talk, &who, &player, &text), "talk: a person would not talk for the answers");
        og_talk_open(&talk, &player, 0, &text);
        ask.mode = 1; ask.tab = 0; ask.category = 2; ask.row = 1; ask.news = 0; ask.no_work = 0;
        answer = og_talk_answer(&talk, &ask, &text);
        check_expect(answer == OG_ANSWER_TEXT && text >= 7250 && text <= 7290, "talk: a where-is answer is not one of the table's");
        ask.category = OG_TALK_REGIONAL;
        og_talk_answer(&talk, &ask, &text);
        check_expect(text == 28, "talk: a regional question is not record 28");
        ask.mode = 0; ask.category = 0; ask.row = 0; ask.news = 1;
        check_expect(og_talk_answer(&talk, &ask, &text) == OG_ANSWER_NEWS && talk.news_asked, "talk: the news was not given");
        og_talk_answer(&talk, &ask, &text);
        check_expect(text == 1457, "talk: the news was given twice");
        ask.tab = 3; talk.news_asked = 0; ask.news = 0; ask.row = 1; ask.no_work = 1;
        og_talk_answer(&talk, &ask, &text);
        check_expect(text == 8078, "talk: no work is not record 8078");
        ask.no_work = 0; talk.attitude = 2; talk.news_asked = 0;
        og_talk_answer(&talk, &ask, &text);
        check_expect(text == 8075 || text == 8076 || text == 8077, "talk: work is not 8075..8077");
        /* The king's and the underworld's people are asked for news again and again. */
        talk.own = 806; talk.news_asked = 0; ask.tab = 0; ask.mode = 0; ask.row = 0; ask.news = 1;
        og_talk_answer(&talk, &ask, &text);
        check_expect(!talk.news_asked, "talk: a faction 806 person's news was counted");

        /* The hint: near enough is told on the town map, far is the vague answer, regional 10 or 11. */
        ask.mode = 1; ask.tab = 0; ask.category = 2;
        check_expect(og_talk_hint(&ask, 0, 1000, 1, &noted) == 7332 && noted, "talk: a near place is not directions");
        check_expect(og_talk_hint(&ask, 0, 5000, 0, &noted) == 7333 && !noted, "talk: indoors gives directions");
        for (n = 0, i = 0; i < 400; i++) { n += og_talk_hint(&ask, 0, 5000, 1, &noted) == 7332; }
        check_expect(n > 70 && n < 130, "talk: a far place should be recalled one try in four");
        ask.category = OG_TALK_REGIONAL;
        check_expect(og_talk_hint(&ask, 1, 0, 1, &noted) == 10 && og_talk_hint(&ask, 0, 0, 1, &noted) == 11, "talk: the regional hints are not 10 and 11");
    }
}
