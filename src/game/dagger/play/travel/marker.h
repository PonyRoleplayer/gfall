#ifndef DF_MARKER_H
#define DF_MARKER_H

/*
 * The player's arrow on a map, as its own small picture: MARKER_SIDE texels a
 * side, drawn into a few of classic's pixels, so the card shows it at four times
 * the map picture's detail and it reads as an arrow, not a blob. Read docs/game.md
 * (The wider world, Automaps).
 *
 * An arrowhead with a notch at its back, filled in a bright colour and outlined
 * in a dark one, turned by angle (radians, clockwise from up the screen). Index 0
 * round it, for a cutout. Composed again only when the angle's step changes.
 *
 * Plain C: hostcheck runs it.
 */

#define MARKER_SIDE 32
/* How many steps a turn has: the picture is composed again only when the step changes. */
#define MARKER_STEPS 32
/* Palette indices under ART_PAL.COL: the fill and the outline. */
#define MARKER_FILL 145
#define MARKER_EDGE 1

/* The step of an angle, 0..MARKER_STEPS-1. */
int marker_step(float angle);
/* The arrow at step into out, MARKER_SIDE x MARKER_SIDE indices. */
void marker_compose(unsigned char *out, int step);

#endif
