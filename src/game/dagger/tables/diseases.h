#ifndef DF_DISEASES_H
#define DF_DISEASES_H

#include "engine/assets/recfile.h"
#include "game/dagger/tables/careers.h"

/*
 * The diseases, as records (docs/data.md), classic's seventeen as Daggerfall
 * Unity's DiseaseEffect carries them. Read docs/game.md (Afflictions)
 * before editing.
 *
 *   DISE: a disease, by DFU's Diseases number: its name; what each day of
 *         its symptoms takes, a roll of damage_min..damage_max times each
 *         attribute's, health's, fatigue's and spell points' weight (0 none,
 *         1 the roll; more, for a mod's quicker decay); and how many days the
 *         symptoms last, days_min..days_max, DISEASE_FOREVER for until cured.
 */

#define DISEASE_COUNT 17
/* days_min of a disease that never passes by itself. */
#define DISEASE_FOREVER 0xFF

/* The diseases the code names (DFU's Diseases). */
enum {
    DISEASE_WITCHES_POX,
    DISEASE_PLAGUE,
    DISEASE_YELLOW_FEVER,
    DISEASE_STOMACH_ROT,
    DISEASE_CONSUMPTION,
    DISEASE_BRAIN_FEVER,
    DISEASE_SWAMP_ROT,
    DISEASE_CALIRONS_CURSE,
    DISEASE_CHOLERA,
    DISEASE_LEPROSY,
    DISEASE_WOUND_ROT,
    DISEASE_RED_DEATH,
    DISEASE_BLOOD_ROT,
    DISEASE_TYPHOID_FEVER,
    DISEASE_DEMENTIA,
    DISEASE_CHRONDIASIS,
    DISEASE_WIZARD_FEVER
};

typedef struct Disease {
    df_u16 id;
    char name[20];
    df_u8 attribute[ATTR_COUNT];
    df_u8 health;
    df_u8 fatigue;
    df_u8 magicka;
    df_u8 damage_min;
    df_u8 damage_max;
    df_u8 days_min;
    df_u8 days_max;
} Disease;

extern const RecSchema disease_schema;
RecTable *disease_table(void);
/* The disease of that number; 0 when MAIN.DAT has none. */
const Disease *disease_kind(int id);

#endif
