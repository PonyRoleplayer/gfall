#include "internal.h"
#include "engine/core/log.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/people/standing.h"

static void sheet_affiliations(void);

static void sheet_click(int x, int y)
{
    int i;
    if (progress_state()->active && possessions_hit(x,y,140,5,190,195)) {
        i = (y-5)/24;
        if (i < ATTRS) {
            possessions_view.selected = i;
            if (x >= 176) { progress_adjust(i,(y-5)%24 < 12 ? 1 : -1); }
        }
        return;
    }
    if (progress_state()->active && !possessions_hit(x,y,50,179,89,198)) { return; }
    if (possessions_hit(x,y,50,179,89,198)) { possessions_close(); }
    else if (possessions_hit(x,y,1,150,67,160)) {
        possessions_close(); possessions_open(VIEW_INVENTORY,0);
    } else if (possessions_hit(x,y,68,150,134,160)) { possessions_open(VIEW_SPELLS,0); }
    else if (possessions_hit(x,y,9,3,135,12)) { possessions_view.prompt = PROMPT_NAME; df_copy(possessions_view.input, character_state()->name, sizeof(possessions_view.input)); }
    else if (possessions_hit(x,y,2,83,134,93)) { sheet_affiliations(); }
    else if (possessions_hit(x,y,69,164,134,178)) {
        log_format(possessions_view.notice,sizeof(possessions_view.notice),"%s\n%s %s\n%s\nCommoners: %d\nMerchants: %d\nScholars: %d\nNobility: %d\nUnderworld: %d",
                   character_state()->name, character_state()->female ? "Female" : "Male", race_name(character_state()->race),
                   game_state()->career.name, character_state()->reputation[0], character_state()->reputation[1],
                   character_state()->reputation[2],character_state()->reputation[3],character_state()->reputation[4]);
    } else if (possessions_hit(x,y,1,164,67,178)) {
        /* logbook_open: the entries of the quests that are running, or an empty log. */
        if (logbook_text(possessions_view.notice, sizeof(possessions_view.notice)) <= 0) { df_copy(possessions_view.notice, "Your quest log is empty.", sizeof(possessions_view.notice)); }
        possessions_view.notice_page = 0;
    }
    else if (possessions_hit(x,y,140,5,170,195)) {
        static const char *attribute[] = {"Strength","Intelligence","Willpower","Agility","Endurance","Personality","Speed","Luck"};
        i = (y-5)/24;
        if (i < ATTRS) {
            log_format(possessions_view.notice,sizeof(possessions_view.notice),"%s: %d\nBase: %d",attribute[i],character_state()->attribute[i],game_state()->base_attribute[i]);
        }
    } else if (possessions_hit(x,y,9,62,135,71)) {
        log_format(possessions_view.notice,sizeof(possessions_view.notice),"Health: %d / %d\n%s",character_state()->health,character_state()->max_health,words_state(18));
    } else if (possessions_hit(x,y,9,52,135,61)) {
        log_format(possessions_view.notice,sizeof(possessions_view.notice),"Fatigue: %d / %d",character_state()->fatigue>>6,
                   character_state()->attribute[ATTR_STR]+character_state()->attribute[ATTR_END]);
    }
    else {
        for (i = 0; i < 4; i++) {
            if (possessions_hit(x,y,10,105+i*10,126,114+i*10)) { possessions_view.skill_group = i; possessions_view.skill_scroll = 0; }
        }
    }
}

/* sheet_show_affiliations: "Affiliation  Rank", then a line for each guild or order the player belongs to (faction name and rank title); none is TEXT.RSC 19. */
static void sheet_affiliations(void)
{
    const FactionDef *faction;
    char text[400];
    int i, n, count;

    n = 0; count = 0;
    n += log_format(text + n, (int)sizeof(text) - n, "Affiliation        Rank");
    for (i = 0; i < MEMBERSHIPS; i++) {
        const Membership *m;

        m = &guilds_state()->m[i];
        if (!m->used) { continue; }
        faction = faction_def(faction_index(m->faction));
        n += log_format(text + n, (int)sizeof(text) - n, "\n%s: %s", faction ? faction->name : "?", guild_rank_name(m->kind, m->rank));
        count++;
    }
    df_copy(possessions_view.notice, count ? text : words_state(19), sizeof(possessions_view.notice));
}

void possessions_sheet_frame(const PlayIn *in, int click)
{
    if (in->pressed[SHEET]) { possessions_close(); }
    else if (in->pressed[INVENTORY] && !progress_state()->active) { possessions_close(); possessions_open(VIEW_INVENTORY,0); }
    else if (click) { sheet_click(in->cx,in->cy); }
}

void possessions_skill_frame(const PlayIn *in, int click)
{
    if (in->ch == 27 || click) { possessions_view.skill_group = -1; }
    else if (in->key == KEY_NEXT || in->key == KEY_DOWN) { if (possessions_view.skill_group == 3 && possessions_view.skill_scroll < 7) { possessions_view.skill_scroll++; } }
    else if (in->key == KEY_PRIOR || in->key == KEY_UP) { if (possessions_view.skill_scroll) { possessions_view.skill_scroll--; } }
}
