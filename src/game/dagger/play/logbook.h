#ifndef DF_LOGBOOK_H
#define DF_LOGBOOK_H

/*
 * The quest logbook, the sheet's Log button (DFU's DaggerfallQuestJournalWindow):
 * a quest a page on a scroll, its log lines as the quest wrote them and the
 * time it has left. Click the right half or press Right or Page Down for the
 * next quest, the left half or Left or Page Up for the one before, Escape or
 * a click on the bottom edge to close. Read docs/game.md (Quests).
 *
 * Plain C: hostcheck drives it by clicks.
 */

enum {
    LOGBOOK_STAY,
    LOGBOOK_LEAVE
};

/* 0 when a picture is missing. now is the game minute (ills_now's count). */
int logbook_open(int now);
void logbook_close(void);
void logbook_compose(void);
const unsigned char *logbook_pixels(void);
int logbook_changed(void);
int logbook_click(int x, int y);
int logbook_key(int vk);
/* For the tests: how many pages, the one shown. */
int logbook_pages(void);
int logbook_page(void);

#endif
