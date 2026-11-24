#ifndef GFALL_WEAPON_H
#define GFALL_WEAPON_H

/* Original hand selection, gestures and CIF animation clock. The app supplies
 * pictures; this module owns when a strike is due. See docs/game.md. */
#include "game/dagger/play/play.h"
#include "game/dagger/play/state.h"

/* A swing frame lasts (SWING_SPEED_BASE - speed) * SWING_FRAME_MS ms (func_00072E85.c:96). */
#define SWING_SPEED_BASE 115
#define SWING_FRAME_MS 3
/* The bow's counter is set to (BOW_RELOAD_SPEED_BASE - speed) * BOW_RELOAD_PER_SPD + BOW_RELOAD_BASE ms and counts down to BOW_RELOAD_FLOOR: the wait is the counter less the floor (weapon_bow_update, weapons.c:178..200). */
#define BOW_RELOAD_SPEED_BASE 100
#define BOW_RELOAD_PER_SPD 10
#define BOW_RELOAD_BASE 1000
#define BOW_RELOAD_FLOOR 200
/* The swing's sector is the mouse angle >> SWING_SECTOR_SHIFT (16 sectors); a swing needs a mouse move of SWING_MOTION_MIN (the larger axis plus half the smaller, in CLASSIC_WIDTH pixels). */
#define SWING_SECTOR_SHIFT 7
#define SWING_MOTION_MIN 45
#define CLASSIC_WIDTH 320
/* A blow costs FATIGUE_PER_BLOW fatigue. */
#define FATIGUE_PER_BLOW 11
/* A beast fights with WEAPON11.CIF, the rest with WEAPON10.CIF's fists. */
#define CIF_FISTS_BEAST 11
/* The bow waits at BOW_FRAME_WAIT for a new press (sound SND_BOW), shoots at BOW_FRAME_SHOOT and ends at BOW_FRAME_END; our frame rate is BOW_FRAME_SECONDS. */
#define BOW_FRAME_WAIT 3
#define BOW_FRAME_SHOOT 5
#define BOW_FRAME_END 7
#define SND_BOW 6
#define BOW_FRAME_SECONDS (1.0f / 60)
/* A swing strikes at SWING_STRIKE_FRAME and ends at SWING_END_FRAME; a kick (no weapon, sector 2) at KICK_STRIKE_FRAME and KICK_END_FRAME. */
#define SWING_STRIKE_FRAME 2
#define SWING_END_FRAME 5
#define KICK_STRIKE_FRAME 4
#define KICK_END_FRAME 8
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
/* Whether the active weapon uses hold-to-draw bow controls. */
int weapon_hold_to_draw(void);
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
