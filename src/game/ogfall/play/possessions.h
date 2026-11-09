#ifndef OGFALL_POSSESSIONS_H
#define OGFALL_POSSESSIONS_H

/* Input controllers for classic inventory, sheet, spells and readable books.
 * Painting is the adapter's responsibility. No UI pointers enter the save. */
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/spellbook.h"

enum { OG_VIEW_NONE, OG_VIEW_INVENTORY, OG_VIEW_SHEET, OG_VIEW_SPELLS, OG_VIEW_BOOK };
enum { OG_ITEM_INFO, OG_ITEM_EQUIP, OG_ITEM_REMOVE, OG_ITEM_USE };
enum { OG_PROMPT_NONE, OG_PROMPT_GOLD, OG_PROMPT_NAME, OG_PROMPT_PAGE, OG_PROMPT_SPELL_NAME };
#define OG_NOTICE_LINES 16
typedef struct OgPossessions {
    int page, underlay, tab, action, scroll[2], selected, skill_group, skill_scroll;
    int prompt, book_page, notice_page, painting;
    int drag, drag_offset, repeat_step;
    float repeat_time;
    df_u32 remote, book;
    char notice[2048], input[32];
    unsigned revision;
} OgPossessions;
void og_possessions_reset(void);
const OgPossessions *og_possessions(void);
const OgBook *og_reading(void);
int og_possessions_open(int page, df_u32 remote);
void og_possessions_close(void);
void og_possessions_frame(const OgIn *input);
int og_possessions_use(df_u32 id);
/* Wrapped message lines; returns the total, filling at most capacity from first.
 * With out=0, only counts. Drawing and paging share this geometry. */
int og_notice_lines(const char *text, int first, char out[][96], int capacity);
void og_spell_scrollbar(int *top, int *height);
/* Adapter fills one slot number per 110x184 doll pixel, 255 for no item. */
df_u8 *og_doll_owners(void);
#endif
