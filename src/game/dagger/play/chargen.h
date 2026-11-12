#ifndef OGFALL_CHARGEN_H
#define OGFALL_CHARGEN_H

/*
 * title_menu from the title through the summary. See docs/ogfall.md.
 * Class list/questions and the custom editor feed the twelve BIOG
 * answers, then name, portrait, rolled attributes (6..14 points),
 * three skill groups (6 points each), reflexes and editable summary.
 * Pure state: app/seam supplies file data and paints the indexed UI.
 *
 * quests_start_initial runs after the landing in title_menu. It does
 * not itself draw: it finds quest files and calls quest_start, which
 * can show an offer or text 600. That box belongs to the quest
 * machine, a later slice. Slice 1 does not start quests and does not
 * put that box up before the player walks.
 *
 * Clicks are classic 320 x 200 pixels. The province is the map
 * pixel under the cursor, not a rectangle. Pixel 0 is the sea.
 * Pixels 1..8 are the eight races (1 Breton, 8 Argonian). That click
 * opens the race confirm. Yes is the left button, or Y. No, N, or
 * Escape returns to the map. Gender is Male on the left (or M) and
 * Female on the right (or F). The button X edges are msgbox_button_at's
 * (112 and 176). The Y is msgbox_render's, from the line count.
 */

/* Two buttons under the parchment. X is msgbox_draw_buttons. Y is og_box_button_y. */
#define OG_CHOICE_X0 112
#define OG_CHOICE_X1 176
#define OG_CHOICE_W 32
#define OG_CHOICE_H 16
/* list_popup_open's pick00i0.img (header 60,36) and picklist_init's list. */
#define OG_LIST_X 85
#define OG_LIST_Y 62
#define OG_LIST_W 140
#define OG_LIST_H 73
#define OG_LIST_ROW 8
#define OG_LIST_VIS 9
#define OG_LIST_N 19

enum {
    OG_UI_TITLE,
    OG_UI_LOAD,
    OG_UI_MAP,
    OG_UI_RACE,
    OG_UI_GENDER,
    OG_UI_CLASS_HOW,
    OG_UI_CLASS_LIST,
    OG_UI_CLASS_YES,
    OG_UI_QUESTIONS,
    OG_UI_MAKER,
    OG_UI_BG_HOW,
    OG_UI_BIOG,
    OG_UI_REPUTE,
    OG_UI_NAME,
    OG_UI_FACE,
    OG_UI_ATTR,
    OG_UI_SKILL,
    OG_UI_REFLEX,
    OG_UI_SUMMARY,
    OG_UI_WORLD
};

/* Seed once at boot; host scenarios can repeat a roll exactly. */
void og_chargen_seed(unsigned seed);
/* Boundary to BIOG file data. answer -1 returns answer count; otherwise applies it. */
void og_chargen_background(int (*fn)(int klass, int question, int answer));
void og_chargen_begin(void);
/* A validated loaded character resumes without rerunning creation. */
void og_chargen_resume(void);
unsigned og_chargen_revision(void);
/* Custom editor: 0 main, 1 skill picker, 2/3 special lists, 4 special picker,
 * 5 subchoice picker, 6 reputations, 7 class name. */
int og_chargen_maker_page(void);
int og_chargen_maker_reputation(int group);
int og_chargen_maker_top(void);
int og_chargen_maker_kind(void);
int og_chargen_maker_list(void);
int og_chargen_maker_skill(int row);
int og_chargen_special_count(int list);
int og_chargen_special_kind(int list, int slot);
int og_chargen_special_param(int list, int slot);
int og_chargen_attr_selected(void);
int og_chargen_skill_selected(int group);
int og_chargen_attr_y(int attr);
int og_chargen_skill_y(int slot);
const char *og_chargen_notice(void);
int og_chargen_mode(void);
/* A caption for the screen, when the picture is not enough. */
const char *og_chargen_caption(void);
/* TEXT.RSC id of the parchment on screen, or 0. */
int og_chargen_text_id(void);
/*
 * Top of the Yes/No or Male/Female buttons for the parchment on screen,
 * or -1 when this screen has none. msgbox_button_at's Y is this value.
 */
int og_chargen_choice_y(void);
/* Non-empty lines in a TEXT.RSC string (newlines are the 0xFD breaks). */
int og_chargen_count_lines(const char *text);
void og_chargen_set_lines(int lines);
int og_chargen_lines(void);
/* The class list's scroll and the highlighted row. Row 18 is Custom. */
int og_chargen_list_top(void);
int og_chargen_list_sel(void);
/* Shared pick00i0 bar: original track y=58,h=83; thumb height/count arithmetic. */
void og_chargen_scroll_thumb(int top, int count, int *y, int *height);
void og_chargen_scroll_frame(int x, int y, int held, int pressed, float dt);
/* Which of the twelve biography questions is up, 0..11. */
int og_chargen_question(void);
int og_chargen_question_number(void);
int og_chargen_question_scroll(void);
/* Points still to spend, and how many landed on each attribute and skill group. */
int og_chargen_attr_pool(void);
int og_chargen_attr_bonus(int attr);
int og_chargen_skill_bonus(int group);

/* pixel is the map image's index. Other screens ignore it. */
void og_chargen_click(int x, int y, int pixel);
/* 27 escape, 13 enter, 8 backspace, otherwise a character. */
void og_chargen_key(int ch);
/* 1 when the title's Exit was chosen. */
int og_chargen_quit(void);

#endif
