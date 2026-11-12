#ifndef GFALL_WEAPON_H
#define GFALL_WEAPON_H

/* Original hand selection, gestures and CIF animation clock. The app supplies
 * pictures; this module owns when a strike is due. See docs/gfall.md. */
#include "game/dagger/play/play.h"
#include "game/dagger/play/state.h"

typedef struct WeaponView {
    int cif, material, color, hand, group, frame, visible;
    /* An enchanted weapon that is not a bow is drawn from WEAPO1##.CIF (weapons.c 132). */
    int enchanted;
} WeaponView;
typedef struct StrikeInfo {
    df_u32 item;
    int hand, bow, hit, damage;
} StrikeInfo;

void weapon_reset(void);
void weapon_cancel(void);
void weapon_switch(void);
int weapon_hand(void);
/* Set by the adapter from decoded CIF data. Bow groups have no idle IMG. */
void weapon_frame(const PlayIn *input, int blocked, StrikeInfo *strike);
const WeaponView *weapon_view(void);
int weapon_sector(int dx, int dy);
int weapon_delay(int speed);
int bow_delay(int speed);
int arrow_count(void);
int arrow_take(void);
void weapon_set_draw(void (*draw)(const WeaponView *view, float flash));
void weapon_draw(void);

#endif
