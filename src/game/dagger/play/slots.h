#ifndef DF_SLOTS_H
#define DF_SLOTS_H

/*
 * Classic's save and load screen (LOAD00I0.IMG, with SAVE00I0.IMG's title over
 * it to save), in memory: six slots, each with its name, at DFU's rectangles.
 * Read docs/game.md (Saves) before editing.
 *
 * To load, a click on a slot that has a save chooses it. To save, a click on
 * any slot asks for a name, its old one to start from, and Enter chooses it.
 * The game then does what was chosen. Plain C: hostcheck drives it.
 */

/* What a click or a key asks of the game. */
enum {
    SLOTS_STAY,
    SLOTS_LEAVE,
    /* slots_chosen says which. */
    SLOTS_CHOSEN
};

/* Opens to save, or to load. 0 when a picture is missing. */
int slots_open(int saving);
void slots_close(void);
void slots_compose(void);
const unsigned char *slots_pixels(void);
int slots_changed(void);
int slots_click(int x, int y);
int slots_key(int vk);
const char *slots_note(void);
/* The slot chosen, and the name typed for a save. */
int slots_chosen(void);
const char *slots_chosen_name(void);

#endif
