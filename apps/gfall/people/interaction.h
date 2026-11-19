#ifndef GFALL_PEOPLE_INTERACTION_H
#define GFALL_PEOPLE_INTERACTION_H

/* Shared by NPC selection and the service menus that return to dialogue. */
void gfall_start_talk(void);
void gfall_person_say_text(int text, char *out, int cap);
unsigned gfall_people_id(void);
const char *gfall_people_name(void);
void gfall_hall_enter(int person_faction, int hall_faction);
/* A person of a coven (faction type 8) clicked: the coven's menu (Talk, Summon, Quest, Exit); the person's own faction is the coven. */
void gfall_coven_enter(int coven_faction);

#endif
