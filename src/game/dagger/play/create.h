#ifndef DF_CREATE_H
#define DF_CREATE_H

#include "game/dagger/play/character.h"
#include "game/dagger/play/inventory.h"

/*
 * Classic's character creation, in memory: what each screen shows and what
 * a click or a key does. Read docs/game.md (Creation) before editing.
 *
 * In order, as Daggerfall Unity's creation windows have them:
 *   the map of Tamriel (TMAP00I0.IMG, a click read through TAMRIEL2.IMG,
 *     whose every pixel is a race), and that race's words to confirm;
 *   a woman or a man;
 *   how to choose a class (BUTN01I0.IMG): from the list (PICK00I0.IMG), whose
 *     last entry makes one's own, on CUST00I0.IMG (name, twelve skills, hit
 *     points, attributes moved about, a harder class advancing faster), or by
 *     classic's ten questions (CHGN00I0.IMG, TEXT.RSC's 9000 and CLASSES.DAT);
 *     the class's words to confirm;
 *   the twelve background questions of that class (BIOG00I0.IMG);
 *   a name (CHAR00I0.IMG), a face of the race's ten (CHAR01I0.IMG);
 *   the attributes' rolls and points to place (CHAR02I0.IMG);
 *   the skills' rolls and points to place (CHAR03I0.IMG);
 *   reflexes (CHAR05I0.IMG);
 *   all of it to look over, or start again (CHAR04I0.IMG).
 *
 * Escape on the map starts at once as the default character. Escape during
 * the questions answers the rest at random, as classic's skip does.
 *
 * Plain C, drawing nothing: hostcheck drives it by clicks and keys, and
 * screens.c puts it on the screen.
 */

/* The steps, in order: which screen is up. */
enum {
    CREATE_RACE,
    CREATE_RACE_CONFIRM,
    CREATE_SEX,
    CREATE_METHOD,
    CREATE_GENERATE,
    CREATE_CLASS,
    CREATE_CLASS_CONFIRM,
    CREATE_CUSTOM,
    CREATE_CUSTOM_PICK,
    CREATE_QUESTIONS,
    CREATE_NAME,
    CREATE_FACE,
    CREATE_ATTRIBUTES,
    CREATE_SKILLS,
    CREATE_REFLEXES,
    CREATE_SUMMARY,
    CREATE_STEPS
};

/* What a click or a key asks of the game. */
enum {
    CREATE_STAY,
    /* The character is made: create_result. */
    CREATE_DONE,
    /* Skipped: create_default. */
    CREATE_SKIPPED
};

/* 0 when its heaps or font did not come. */
int create_open(void);
void create_close(void);
/* The screen now, whatever create_changed says: 320 x 200 under ART_PAL.COL. */
void create_compose(void);
const df_u8 *create_pixels(void);
/* The palette the screen now is under: ART_PAL.COL's, or the class questions' picture's own. 768 RGB bytes. */
void create_palette(df_u8 *rgb);
/* The class ten answers come to, from CLASSES.DAT's table, by how many went each way. -1 when none. */
int create_class_of_answers(int warrior, int rogue, int mage);
/* 1 once since the last compose if something on it changed. */
int create_changed(void);
/* A click at classic's x, y: CREATE_STAY, DONE or SKIPPED. */
int create_click(int x, int y);
/* A key pressed, by its virtual key code: the name, Enter, Escape. As a click. */
int create_key(int vk);
const char *create_note(void);
int create_step(void);
/* The character made, level 1 and whole, and what they start with. 0 when none was. */
int create_result(Character *sheet, Inventory *pack);
/* The game's default character, the middle of every roll, and their kit. */
int create_default(Character *sheet, Inventory *pack);

#endif
