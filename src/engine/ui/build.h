#ifndef ENG_BUILD_H
#define ENG_BUILD_H

/*
 * Compile stamp. The Makefile bakes the local time into the exe, so a
 * new link shows a new string. Read docs/console.md before editing.
 */

const char *build_text(void);
void build_set_draw(int on);
int build_draw(void);
/* Bottom left. No plate when drawing is off. */
void build_legend(void);

#endif
