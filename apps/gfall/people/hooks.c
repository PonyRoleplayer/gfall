#include "people.h"
#include "callbacks.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/play/input/activate.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/people/standing.h"

void gfall_people_install(void)
{
    activate_set_shelf(OTHER_SHELF, gfall_shelf_used);
    activate_set_container(OTHER_CONTAINER, gfall_container_used);
    activate_set_notice(OTHER_NOTICE, gfall_notice_used);
    activate_set_person(gfall_person_used);
    activate_set_talk_use(gfall_talk_used);
    possessions_set_service(gfall_service_act);
    standing_set_membership(gfall_standing_membership);
    rest_set_site(gfall_rest_site);
    rest_set_room(gfall_go_to_bed);
    possessions_set_court(gfall_court_say, gfall_court_release);
    possessions_set_offer_words(gfall_offer_say);
}
