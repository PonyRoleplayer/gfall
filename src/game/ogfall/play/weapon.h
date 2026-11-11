#ifndef OGFALL_WEAPON_H
#define OGFALL_WEAPON_H

/* Original hand selection, gestures and CIF animation clock. The app supplies
 * pictures; this module owns when a strike is due. See docs/ogfall.md. */
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/state.h"

typedef struct OgWeaponView {
    int cif, material, color, hand, group, frame, visible;
    /* An enchanted weapon that is not a bow is drawn from WEAPO1##.CIF (weapons.c 132). */
    int enchanted;
} OgWeaponView;
typedef struct OgStrike {
    df_u32 item;
    int hand, bow, hit, damage;
} OgStrike;

void og_weapon_reset(void);
void og_weapon_cancel(void);
void og_weapon_switch(void);
int og_weapon_hand(void);
/* Set by the adapter from decoded CIF data. Bow groups have no idle IMG. */
void og_weapon_frame(const OgIn *input, int blocked, OgStrike *strike);
const OgWeaponView *og_weapon_view(void);
int og_weapon_sector(int dx, int dy);
int og_weapon_delay(int speed);
int og_bow_delay(int speed);
int og_arrow_count(void);
int og_arrow_take(void);
void og_weapon_set_draw(void (*draw)(const OgWeaponView *view, float flash));
void og_weapon_draw(void);

#endif
