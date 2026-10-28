#ifndef DF_RULES_H
#define DF_RULES_H

/*
 * Classic's rules, as sums: each a pure function of a character's
 * numbers, citing where it comes from. Read docs/game.md before
 * editing. A later mode of our own swaps these, never the code that
 * calls them; a deliberate change from classic says so here.
 *
 * Plain C: hostcheck runs it.
 */

/* Daggerfall Unity measures in metres; the engine in 40 units a metre (walker.h). */
#define RULES_UNITS_PER_M 40.0f

/*
 * Walking, in metres a second, from the Speed attribute (0..100):
 * PlayerSpeedChanger.GetWalkSpeed, (speed + 150 - drag) / 39.5, drag
 * half of how far speed is under 100, counted from no less than 30.
 */
float rules_walk_speed(int speed);
/* Running: (speed + 150) / 39.5 times 1.35 and the Running skill (0..100) over 200. */
float rules_run_speed(int speed, int running);
/*
 * A jump's height, in metres: AcrobatMotor's 4.5 m/s, raised by the
 * Jumping skill (0..100) over 200, under its gravity of 20 m/s^2.
 */
float rules_jump_height(int jumping);

#endif
