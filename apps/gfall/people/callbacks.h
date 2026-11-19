#ifndef GFALL_APP_PEOPLE_CALLBACKS_H
#define GFALL_APP_PEOPLE_CALLBACKS_H

#include "engine/scene/interact.h"
#include "game/dagger/play/character/rest.h"
#include "game/dagger/play/people/law.h"

int gfall_shelf_used(int index, int variant);
int gfall_container_used(int index);
/* A notice board used (click_show_building_info, click.c 1088): the town name, then the region's regional news. */
void gfall_notice_used(void);
/* A notice board used (click_show_building_info, click.c 1088): the town's name, then the region's regional news. */
void gfall_notice_used(void);
int gfall_person_used(const Camera *aim, int mode);
int gfall_talk_used(const Usable *u, int mode, float dist, const Camera *cam);
void gfall_service_act(int what);
int gfall_standing_membership(int bits);
int gfall_rest_site(RestSite *site);
void gfall_go_to_bed(df_u32 building, int tavern);
int gfall_court_say(int text, const Court *court, char *out, int cap);
void gfall_court_release(int where);
int gfall_offer_say(int text, int price, char *out, int cap);
/* A summoning's text (481, 482, 484) with the prince's name (%dae) and today's date (%dat). */
int gfall_daedra_say(int text, int price, int prince, char *out, int cap);

#endif
