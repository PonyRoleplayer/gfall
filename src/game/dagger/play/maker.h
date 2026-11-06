#ifndef DF_MAKER_H
#define DF_MAKER_H

#include "game/dagger/play/player.h"
#include "game/dagger/play/spells.h"

/*
 * The spell maker (a Mages Guild's Make Spells; DFU's DaggerfallSpellMakerWindow), on
 * classic's INFO01I0.IMG and, for an effect's parameters, MASK05I0.IMG. Read
 * docs/game.md (Magic) before editing.
 *
 * A spell of up to three effects. Add Effect picks the next free slot's group and, when it
 * has several, its subgroup, from lists on a scroll (the effects built or recorded,
 * effects.h; the rest wait). Clicking a slot edits its parameters on the second picture: a
 * spinner each for what the effect supports (duration, chance, magnitude; the range of each is
 * DFU's), the effect's gold at the corner; Delete or Backspace on the main screen
 * takes the slot last touched out. The target buttons and the elements' are lit when an
 * effect allows them (the targets all effects allow together, the elements any does, magic
 * always), one is chosen; the arrows choose the icon; the name is typed after a click
 * on its line (letters, digits and space, Enter ends it). The costs are live: the spell's gold
 * and the spell points it will take of this character, their money and their most. Buy
 * charges the gold and inscribes the spell in the book, if it has an effect, a name, and
 * one of that name is not in the book; New clears; Exit leaves.
 *
 * An effect that would leave the slots with no target in common is not added
 * ("That cannot be joined with the other effects."). Plain C over canvas.h: hostcheck
 * drives it by clicks and keys and writes pictures.
 */

enum {
    MAKER_STAY,
    MAKER_LEAVE
};

/* Parameter limits, DFU's spinners'. */
#define MAKER_DURATION_MAX 60
#define MAKER_CHANCE_MAX 100
#define MAKER_MAGNITUDE_MAX 100
#define MAKER_PER_MAX 20

/* 0 when a picture is missing. The player pays and has the spell inscribed in book, both kept by pointer until it closes. */
int maker_open(Spellbook *book, Player *player);
void maker_close(void);
void maker_compose(void);
const unsigned char *maker_pixels(void);
int maker_changed(void);
int maker_click(int x, int y);
/* A key (a virtual key). */
int maker_key(int vk);
/* A line for the screen's top: what was refused or done; 0 when none. */
const char *maker_note(void);

/* For the tests: the spell as it stands, which part of the screen shows, the cost of it. */
const Spell *maker_spell(void);
int maker_mode(void);
int maker_cost(int *gold);
/* The picker's rows now: how many, and the name of one. */
int maker_list_count(void);
const char *maker_list_name(int i);
enum {
    MAKER_MODE_MAIN,
    MAKER_MODE_GROUP,
    MAKER_MODE_SUB,
    MAKER_MODE_EDIT,
    MAKER_MODE_NAME
};

#endif
