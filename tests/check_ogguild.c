#include <string.h>
#include "check_ogguild.h"
#include "check_util.h"
#include "game/ogfall/play/guild.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/save.h"

static void skills(int a, int b, int c)
{
    int i;

    for (i = 0; i < OG_SKILLS; i++) { og_character()->skill[i] = 0; }
    og_character()->skill[22] = a; og_character()->skill[23] = b; og_character()->skill[24] = c;
}

void check_ogguild(void)
{
    OgGuildStep step;
    OgMembership *m;
    df_u32 now;
    int i, ok, mages;

    og_guilds_clear();
    /* The halls' factions: Mages 40, Fighters 41, Thieves 42, Brotherhood 108, a temple (21 and its own 82), a knightly order 368. */
    check_expect(og_guild_kind_of_faction(40) == 1 && og_guild_kind_of_faction(41) == 2 && og_guild_kind_of_faction(42) == 3 && og_guild_kind_of_faction(108) == 0 &&
                 og_guild_kind_of_faction(21) == 142 && og_guild_kind_of_faction(82) == 142 && og_guild_kind_of_faction(368) == 68 && og_guild_kind_of_faction(999) == -1,
                 "guild: the kinds of the halls' factions");
    mages = og_guild_kind_of_faction(40);

    /* Joining wants the best of the guild's skills at 22 and the next at 4, and a reputation that is not negative. */
    skills(21, 10, 0);
    check_expect(og_guild_join_check(mages, 10) == 2, "guild: a best skill of 21 was good enough");
    skills(30, 3, 0);
    check_expect(og_guild_join_check(mages, 10) == 2, "guild: a second skill of 3 was good enough");
    skills(30, 4, 0);
    check_expect(og_guild_join_check(mages, 10) == 0 && og_guild_join_check(mages, -1) == 1, "guild: 30 and 4 should join, a bad name should not");
    check_expect(og_guild_offer(mages, 40, 10, &step) && step.ask && step.text == 606, "guild: the Mages' offer is not record 606");
    check_expect(og_guild_offer(mages, 40, -5, &step) && !step.ask && step.text == 612, "guild: a bad name is not refused with 612");
    skills(10, 3, 0);
    check_expect(og_guild_offer(mages, 40, 10, &step) && !step.ask && step.text == 611, "guild: too little skill is not refused with 611");

    /* The yes: a rank 0 membership begun now; nobody asks twice. */
    skills(30, 4, 0);
    now = 100000u;
    check_expect(og_guild_join(mages, 40, now, &step) && step.joined && step.text == 5293 && og_guild_find_faction(40) && og_guild_find_kind(mages)->rank == 0, "guild: joining did not make a rank 0 membership");
    check_expect(!og_guild_offer(mages, 40, 10, &step), "guild: a member was offered the guild again");

    /* The rank is the lesser of a tenth of the reputation and the steps the skills reach, at most 9: 54 and 28 are steps 4 and 6; reputation 50 makes 5. */
    skills(54, 28, 0);
    check_expect(og_guild_rank_for_skills(mages, 50) == 4 && og_guild_rank_for_skills(mages, 30) == 3 && og_guild_rank_for_skills(mages, 1000) == 4, "guild: the rank by steps");
    skills(100, 100, 100);
    check_expect(og_guild_rank_for_skills(mages, 1000) == 9, "guild: the rank is not held to 9");

    /* A month between ranks: nothing within 40320 minutes; after it a promotion with its line (message 3 + rank). */
    skills(54, 28, 0);
    check_expect(!og_guild_promote(mages, 40, 50, now + 40320u, &step), "guild: a promotion came within the month");
    check_expect(og_guild_promote(mages, 40, 50, now + 40321u, &step) && step.promoted && og_guild_find_faction(40)->rank == 4 && step.text == 5236, "guild: no promotion after the month");
    check_expect(og_guild_find_faction(40)->rank_time == now + 40321u, "guild: the rank's minute was not kept");
    check_expect(!og_guild_promote(mages, 40, 50, now + 40321u + 100u, &step), "guild: promoted twice");

    /* A negative reputation expels: the membership goes (and the Thieves' and Brotherhood's counts with it). */
    check_expect(og_guild_promote(mages, 40, -1, now + 100000u, &step) && step.expelled && step.text == 668 && !og_guild_find_faction(40), "guild: a bad name did not expel");

    /* Knights and temples: one of each, and the local ones by region (FACTION.TXT's own regions). */
    og_guild_join(og_guild_kind_of_faction(21), 21, now, &step);
    check_expect(!og_guild_offer(og_guild_kind_of_faction(82), 82, 10, &step), "guild: a second temple was offered");
    check_expect(og_guild_find_bits(OG_KIND_TEMPLE) != 0 && og_guild_find_bits(OG_KIND_KNIGHT) == 0, "guild: the temple was found as a knight");
    og_guild_find_faction(21)->rank = 5;
    {
        const OgFactionDef *f;
        int region, other;

        f = og_faction_def(og_faction_index(21));
        region = f ? f->region : -1;
        other = region == 0 ? 1 : 0;
        check_expect(f && og_guild_local_temple_rank(region) == 5 && og_guild_local_temple_rank(other) == 0, "guild: a temple's rank counts only in its region");
    }
    og_guild_join(og_guild_kind_of_faction(368), 368, now, &step);
    og_guild_find_faction(368)->rank = 3;
    {
        const OgFactionDef *f;

        f = og_faction_def(og_faction_index(368));
        check_expect(f && og_guild_is_local_knight(f->region) && !og_guild_is_local_knight(f->region == 0 ? 1 : 0), "guild: a knight is local in the order's region, and at rank 4");
        og_guild_find_faction(368)->rank = 4;
        check_expect(og_guild_is_local_knight(f->region == 0 ? 1 : 0), "guild: a knight of rank 4 was not local everywhere");
    }

    /* Invitations: a pickpocket counted six times, a kill (passing 5) fifteen; three days after, out of doors, the counter jumps to 100. */
    og_guilds()->thieves_count = og_guilds()->brotherhood_count = 0;
    for (i = 0; i < 5; i++) { og_guild_count_crime(5, 1, now); }
    check_expect(og_guilds()->thieves_invite == 0, "guild: five pickpockets already invited");
    og_guild_count_crime(5, 1, now);
    check_expect(og_guilds()->thieves_invite == now + 4320u && og_guilds()->brotherhood_count == 0, "guild: six pickpockets did not set the invitation");
    ok = og_guild_invitation(now + 4000u, 1, &i);
    check_expect(!ok && !og_guild_invitation(now + 5000u, 0, &i), "guild: an invitation came early or indoors");
    check_expect(og_guild_invitation(now + 5000u, 1, &i) && i == 3 && og_guilds()->thieves_count == 100 && !og_guilds()->thieves_invite, "guild: the Thieves' invitation");
    og_guild_count_crime(5, 1, now);
    check_expect(og_guilds()->thieves_count == 100, "guild: a count after the invitation");
    for (i = 0; i < 15; i++) { og_guild_count_crime(1, 1, now); }
    check_expect(og_guilds()->brotherhood_invite == now + 4320u, "guild: fifteen kills did not invite the Brotherhood");

    /* Through a save and back. */
    check_expect(og_guilds_valid(og_guilds()), "guild: the state is not valid");
    m = og_guild_find_faction(368);
    check_expect(m && m->rank == 4, "guild: the knight went missing");

    /* The hall's people: the gating of guild_service_dispatch, member or stranger, by rank. */
    {
        int heal;

        check_expect(og_guild_service(0, 841, 0, &heal) == OG_SVC_RANK_REFUSED && og_guild_service(0, 841, 1, &heal) == OG_SVC_POTIONS &&
                     og_guild_service(0, 842, 7, &heal) == OG_SVC_SPYMASTER && og_guild_service(0, 839, -1, &heal) == OG_SVC_NONE, "guild: the Brotherhood's gating");
        check_expect(og_guild_service(1, 60, -1, &heal) == OG_SVC_SPELLS && og_guild_service(1, 65, 2, &heal) == OG_SVC_RANK_REFUSED &&
                     og_guild_service(1, 65, 3, &heal) == OG_SVC_MAGIC_ITEMS && heal == 1 && og_guild_service(1, 62, 8, &heal) == OG_SVC_TELEPORT &&
                     og_guild_service(1, 64, -1, &heal) == OG_SVC_MEMBERS_ONLY && heal == 0, "guild: the Mages' gating (a member is healed, a stranger buys spells)");
        check_expect(og_guild_service(2, 850, 0, &heal) == OG_SVC_REPAIRS && og_guild_service(2, 850, -1, &heal) == OG_SVC_TALK &&
                     og_guild_service(2, 851, -1, &heal) == OG_SVC_QUEST, "guild: the Fighters' gating");
        check_expect(og_guild_service(3, 805, 1, &heal) == OG_SVC_RANK_REFUSED && og_guild_service(3, 805, 2, &heal) == OG_SVC_FENCE &&
                     og_guild_service(3, 803, -1, &heal) == OG_SVC_NONE, "guild: the Thieves' gating");
        check_expect(og_guild_service(71, 845, 2, &heal) == OG_SVC_ARMOR && og_guild_service(71, 846, -1, &heal) == OG_SVC_QUEST &&
                     og_guild_service(71, 845, -1, &heal) == OG_SVC_MEMBERS_ONLY, "guild: a knightly order's gating");
        check_expect(og_guild_service(143, 462, 1, &heal) == OG_SVC_POTIONS && heal == 0 && og_guild_service(143, 462, 2, &heal) == OG_SVC_POTIONS && heal == 1 &&
                     og_guild_service(143, 810, -1, &heal) == OG_SVC_DONATE && og_guild_service(143, 460, -1, &heal) == OG_SVC_BLESSING &&
                     og_guild_service(142, 460, -1, &heal) == OG_SVC_MEMBERS_ONLY && og_guild_service(142, 453, 0, &heal) == OG_SVC_RANK_REFUSED && heal == 1,
                     "guild: a temple's gating (Mara heals from rank 2, Akatosh always and has no blessing)");
        check_expect(!strcmp(og_guild_service_label(60), "Buy Spells") && !strcmp(og_guild_service_label(810), "Make Donation") && og_guild_service_label(1) == 0, "guild: the service labels are not FALL.EXE's");
        check_expect(!strcmp(og_guild_picture(0), "GILD00I0.IMG") && !strcmp(og_guild_picture(1), "GILD01I0.IMG") && og_guild_price_scale(0) == 256 && og_guild_price_scale(5) == 128, "guild: the menu picture and the price scale");
    }

    /* Training: 12 hours between lessons, the price a level of a hundred (four times for a stranger), over 50 refused, uses by the skill's advance. */
    {
        int price, text, minutes;
        const unsigned char *list;

        og_guilds()->last_training = 0;
        og_character()->gold = 5000;
        og_character()->level = 3;
        list = og_training_skills(61);
        check_expect(list && list[0] == 22 && og_training_skills(1) == 0 && og_training_skills(839)[0] == 28, "guild: the trainers' skills");
        check_expect(og_training_offer(1000u, 3, 1, &price, &text) && price == 300 && og_training_offer(1000u, 3, 0, &price, &text) && price == 1200, "guild: a lesson's price");
        og_character()->skill[22] = 51;
        check_expect(!og_training_learn(22, 300, 1000u, &text, &minutes) && text == 4022 && og_character()->gold == 5000, "guild: a skill over 50 was taught");
        og_character()->skill[22] = 40;
        og_state_clear_skill_uses(22);
        check_expect(og_training_learn(22, 300, 1000u, &text, &minutes) && text == 5221 && minutes == 180 && og_character()->gold == 4700 &&
                     og_state()->skill_uses[22] >= 10u && og_state()->skill_uses[22] <= 20u && og_guilds()->last_training == 1000u,
                     "guild: a lesson did not cost, teach 10..20 uses (skill 22 advances by 1), and note the minute");
        check_expect(!og_training_offer(1000u + 719u, 3, 1, &price, &text) && text == 4023 && og_training_offer(1000u + 720u, 3, 1, &price, &text), "guild: lessons closer than 12 hours");
    }
    /* Blessings: one at a time, a skill, an attribute or the legal standing by the temple, rank + 10 for rank + 4 days; ended, what was added is taken off. */
    {
        int text, was_skill, was_attr, was_legal;

        og_guilds()->blessing.active = 0;
        was_skill = og_character()->skill[14];
        check_expect(!og_blessing_buy(142, 3, 17, 5000u, &text), "guild: Akatosh's gave a blessing");
        check_expect(og_blessing_buy(143, 3, 17, 5000u, &text) && text == 705 && og_character()->skill[14] == was_skill + 13 && og_guilds()->blessing.end == 5000u + 7u * 1440u,
                     "guild: Dibella's blessing is not rank + 10 on skill 14 for rank + 4 days");
        check_expect(!og_blessing_buy(144, 3, 17, 5000u, &text) && text == 454, "guild: a second blessing was given");
        check_expect(!og_blessing_expire(5000u + 7u * 1440u) && og_blessing_expire(5000u + 7u * 1440u + 1u) && og_character()->skill[14] == was_skill && !og_guilds()->blessing.active,
                     "guild: a blessing did not end at its time and take its points back");
        was_attr = og_character()->attribute[5];
        og_blessing_buy(144, 0, 17, 1u, &text);
        check_expect(og_character()->attribute[5] == was_attr + 10 && text == 707, "guild: a temple's attribute blessing");
        og_blessing_expire(1000000u);
        was_legal = og_region_legal(17);
        og_blessing_buy(148, 2, 17, 1u, &text);
        check_expect(og_region_legal(17) == was_legal + 12 && text == 716, "guild: Zenithar's blessing raises the legal standing");
        /* The gift is taken from region 0 at the end, as the original's record never names the region. */
        og_blessing_expire(1000000u);
        check_expect(og_region_legal(17) == was_legal + 12, "guild: the legal gift came off the region it was given in");
        og_region_add_legal(17, -12);
        og_region_add_legal(0, 12);
    }
}
