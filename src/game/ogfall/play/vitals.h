#ifndef OGFALL_VITALS_H
#define OGFALL_VITALS_H

/*
 * hud_draw's bar heights, the jump's integer magnitude, and fall
 * damage. The expressions associate left to right, as C does for
 * << and / . A non-positive numerator, or a zero max, draws nothing
 * and does not divide.
 */

/* Health and magicka: (((n << 8) / max) << 5) / 256. 0..32. */
int og_bar_ratio(int numerator, int denominator);
/* Fatigue: (((fatigue << 8) / max) << 5) >> 8. max is (STR+END)<<6. */
int og_bar_fatigue(int fatigue, int strength, int endurance);

/* ((SPD + STR) / 2) + 50, then jumping * that / 100. Before the negate. */
int og_jump_magnitude(int speed, int strength, int jumping);
/*
 * Acrobat (conditions 0x10000) doubles the magnitude. Class flag bit 2
 * then replaces the impulse with magnitude + magnitude/2, which drops
 * the double. That is the decomp's order (intrface.c).
 */
int og_jump_impulse(int magnitude, int acrobat, int class_flag_bit2);
/* The magnitude is already world units a second: the << 8 and the / 256 cancel. */
float og_jump_ups(int impulse);

/*
 * Fall damage on the classic impact speed v (1/256 world units a
 * second), only when the quotient is positive: ((v/256)/40)-7, then
 * squared over 2. og_fall_v maps body_landed's downward speed back
 * into that v.
 */
int og_fall_v(float down_units_per_sec);
int og_fall_damage(int v);

/* A jump spends 11. Clamped at 0; the collapse into rest is not slice 1. */
int og_fatigue_add(int fatigue, int amount, int strength, int endurance);

#endif
